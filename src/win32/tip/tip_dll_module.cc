// Copyright 2010-2014, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "win32/tip/tip_dll_module.h"

#include <msctf.h>

#include "google/protobuf/stubs/common.h"
#include "base/mutex.h"
#include "base/singleton.h"
#include "base/crash_report_handler.h"
#include "config/stats_config_util.h"
#include "win32/base/tsf_profile.h"
#include "win32/base/tsf_registrar.h"
#include "win32/tip/tip_class_factory.h"
#include "win32/tip/tip_ui_handler.h"
#include "win32/tip/tip_text_service.h"

namespace {

using mozc::CrashReportHandler;
using mozc::SingletonFinalizer;
using mozc::config::StatsConfigUtil;
using mozc::once_t;
using mozc::win32::TsfProfile;
using mozc::win32::TsfRegistrar;
using mozc::win32::tsf::TipTextServiceFactory;
using mozc::win32::tsf::TipUiHandler;

// True if the boot mode is safe mode.
bool g_in_safe_mode = true;

// Marker objects for one-time initialization.
once_t g_initialize_once = MOZC_ONCE_INIT;
once_t g_uninitialize_once = MOZC_ONCE_INIT;

// Creates the global resources shared among all the ImeTextService objects.
void TipBuildGlobalObjects() {
  // Cache the boot mode here so that we need not call user32.dll functions
  // from DllMain.  If it is safe mode, we omit some initializations/
  // uninitializations to reduce potential crashes around them. (b/2728123)
  g_in_safe_mode = (::GetSystemMetrics(SM_CLEANBOOT) > 0);
  if (g_in_safe_mode) {
    return;
  }

  if (StatsConfigUtil::IsEnabled()) {
    CrashReportHandler::Initialize(true);
  }
}

void TipShutdownCrashReportHandler() {
  if (CrashReportHandler::IsInitialized()) {
    // Uninitialize the breakpad.
    CrashReportHandler::Uninitialize();
  }
}

class ModuleImpl {
 public:
  // Increases and decreases the reference count to this module.
  // This reference count is used for preventing Windows from unloading
  // this module.
  static LONG AddRef() {
    ::InterlockedIncrement(&ref_count_);
    return ref_count_;
  }
  static LONG Release() {
    if (::InterlockedDecrement(&ref_count_) == 0) {
      if (!in_unit_test_) {
        // |ref_count_| is now decremented to be 0. So our DLL is likely to be
        // unloaded soon. Here is the good point to release global resources
        // that should not be unloaded in DllMain due to the loader lock.
        // However, it should also be noted that there is a chance that
        // AddRef() is called again and the application continues to use Mozc
        // client DLL. Actually we can observe this situation inside
        // "Visual Studio 2012 Remote Debugging Monitor" running on Windows 8.
        // Thus we must not shut down libraries that cannot be designed to be
        // re-initializable. For instance, we must not call following
        // functions here.
        // - SingletonFinalizer::Finalize()               // see b/10233768
        // - google::protobuf::ShutdownProtobufLibrary()  // see b/2126375
        CallOnce(&g_uninitialize_once, TipShutdownCrashReportHandler);
      }
    }
    return ref_count_;
  }
  static bool IsUnloaded() {
    return unloaded_;
  }
  static bool CanUnload() {
    return ref_count_ <= 0;
  }

  static BOOL OnDllProcessAttach(HINSTANCE instance, bool static_loading) {
    module_handle_ = instance;
    if (!::InitializeCriticalSectionAndSpinCount(
            &critical_section_for_breakpad_, 0)) {
        return FALSE;
    }
    CrashReportHandler::SetCriticalSection(&critical_section_for_breakpad_);
    TipTextServiceFactory::OnDllProcessAttach(instance, static_loading);
    TipUiHandler::OnDllProcessAttach(instance, static_loading);
    return TRUE;
  }

  static BOOL OnDllProcessDetach(HINSTANCE instance, bool process_shutdown) {
    TipUiHandler::OnDllProcessDetach(instance, process_shutdown);
    TipTextServiceFactory::OnDllProcessDetach(instance, process_shutdown);
    if (!g_in_safe_mode && !process_shutdown) {
      // It is our responsibility to make sure that our code never touch
      // protobuf library after google::protobuf::ShutdownProtobufLibrary is
      // called. Unfortunately, DllMain is the only place that satisfies this
      // condition. So we carefully call it here, even though there is a risk
      // of deadlocks. See b/2126375 for details.
      google::protobuf::ShutdownProtobufLibrary();
    }

    ::DeleteCriticalSection(&critical_section_for_breakpad_);
    module_handle_ = nullptr;
    unloaded_ = true;
    return TRUE;
  }

  static HMODULE module_handle() {
    return module_handle_;
  }

  static void InitForUnitTest() {
    in_unit_test_ = true;
  }

 private:
  static HMODULE module_handle_;
  static volatile LONG ref_count_;
  static bool unloaded_;
  static bool in_unit_test_;
  static CRITICAL_SECTION critical_section_for_breakpad_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ModuleImpl);
};

HMODULE ModuleImpl::module_handle_ = nullptr;
volatile LONG ModuleImpl::ref_count_ = 0;
bool ModuleImpl::unloaded_ = false;
bool ModuleImpl::in_unit_test_ = false;
CRITICAL_SECTION ModuleImpl::critical_section_for_breakpad_;

}  // namespace

// Retrieves interfaces exported by this module.
// This module exports only the IClassFactory object, which is a COM interface
// that creates an instance of the COM objects implemented by this module.
STDAPI DllGetClassObject(REFCLSID class_id,
                         REFIID interface_id,
                         void **object) {
  mozc::CallOnce(&g_initialize_once, TipBuildGlobalObjects);
  if (object == nullptr) {
    return E_INVALIDARG;
  }
  if ((::IsEqualIID(interface_id, IID_IClassFactory) ||
       ::IsEqualIID(interface_id, IID_IUnknown)) &&
       ::IsEqualGUID(class_id, TsfProfile::GetTextServiceGuid())) {
    IClassFactory *factory = new mozc::win32::tsf::TipClassFactory();
    factory->AddRef();
    *object = factory;
    return NOERROR;
  }

  *object = nullptr;
  return CLASS_E_CLASSNOTAVAILABLE;
}

// Returns whether or not Windows can unload this module.
STDAPI DllCanUnloadNow() {
  return ModuleImpl::CanUnload() ? S_OK : S_FALSE;
}

// Unregisters this module from Windows.
// This function is called when executing a command
// "regsvr32.exe /u $(MODULE_NAME)".
STDAPI DllUnregisterServer() {
  TsfRegistrar::UnregisterCategories();
  TsfRegistrar::UnregisterProfiles();
  TsfRegistrar::UnregisterCOMServer();

  return S_OK;
}

// Registers this module as a text-input processor.
// This function is called when executing a command
// "regsvr32.exe $(MODULE_NAME)".
STDAPI DllRegisterServer() {
  // To register this DLL as a TSF test service, we need the following three
  // registrations:
  // 1. Register this DLL as a COM server;
  // 2. Register this COM server as a TSF text service, and;
  // 3. Register this text service as a TSF text-input processor.
  wchar_t path[MAX_PATH] = {};
  const DWORD path_length = ::GetModuleFileName(
      ModuleImpl::module_handle(), &path[0], arraysize(path));
  HRESULT result = TsfRegistrar::RegisterCOMServer(path, path_length);
  if (FAILED(result)) {
    DllUnregisterServer();
    return result;
  }
  result = TsfRegistrar::RegisterProfiles(path, path_length);
  if (FAILED(result)) {
    DllUnregisterServer();
    return result;
  }
  result = TsfRegistrar::RegisterCategories();
  if (FAILED(result)) {
    DllUnregisterServer();
    return result;
  }
  return result;
}

// Represents the entry point of this module.
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
  switch (reason)  {
    case DLL_PROCESS_ATTACH:
    // We disable thread library calls only when the dynamic CRT is specified.
    // This can be determined by checking the _DLL macro.
    // http://msdn.microsoft.com/en-us/library/b0084kay.aspx
#if defined(_DLL)
      if (::DisableThreadLibraryCalls(instance) == 0) {
        // DisableThreadLibraryCalls failed.
#if defined(_DEBUG)
        ::DebugBreak();
        return FALSE;
#endif  // _DEBUG
      }
#endif  // _DLL
      return ModuleImpl::OnDllProcessAttach(instance, reserved != nullptr);
#if defined(_DLL) && defined(_DEBUG)
    // If dynamic CRT is specified, neither DLL_THREAD_ATTACH
    // nor DLL_THREAD_DETACH should be passed to the DllMain.
    // To assert this, call DebugBreak here.
    case DLL_THREAD_ATTACH:
      ::DebugBreak();
      return FALSE;
    case DLL_THREAD_DETACH:
      ::DebugBreak();
      return FALSE;
#endif  // _DLL && _DEBUG
    case DLL_PROCESS_DETACH:
      return ModuleImpl::OnDllProcessDetach(instance, reserved != nullptr);
  }
  return TRUE;
}

namespace mozc {
namespace win32 {
namespace tsf {

// Increases the reference count to this module.
LONG TipDllModule::AddRef() {
  return ModuleImpl::AddRef();
}

// Decreases the reference count to this module.
LONG TipDllModule::Release() {
  return ModuleImpl::Release();
}

bool TipDllModule::IsUnloaded() {
  return ModuleImpl::IsUnloaded();
}

bool TipDllModule::CanUnload() {
  return ModuleImpl::CanUnload();
}

HMODULE TipDllModule::module_handle() {
  return ModuleImpl::module_handle();
}

void TipDllModule::InitForUnitTest() {
  ModuleImpl::InitForUnitTest();
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc
