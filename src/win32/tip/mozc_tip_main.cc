// Copyright 2010-2021, Google Inc.
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

// Mozc TIP (Text Input Processor) DLL entry point for Text Services Framework.

#include <msctf.h>
#include <windows.h>

#include "google/protobuf/stubs/common.h"
#include "absl/base/call_once.h"
#include "base/protobuf/message.h"
#include "base/protobuf/protobuf.h"
#include "base/win32/com_implements.h"
#include "win32/base/tsf_profile.h"
#include "win32/tip/tip_class_factory.h"
#include "win32/tip/tip_dll_module.h"
#include "win32/tip/tip_text_service.h"
#include "win32/tip/tip_ui_handler.h"

namespace {

using mozc::win32::TsfProfile;
using mozc::win32::tsf::TipDllModule;
using mozc::win32::tsf::TipTextServiceFactory;
using mozc::win32::tsf::TipUiHandler;

// Marker objects for one-time initialization.
absl::once_flag initialize_once;

// True if the boot mode is safe mode.
bool in_safe_mode = true;

// Creates the global resources shared among all the ImeTextService objects.
void TipBuildGlobalObjects() {
  // Cache the boot mode here so that we need not call user32.dll functions
  // from DllMain.  If it is safe mode, we omit some initializations/
  // uninitializations to reduce potential crashes around them. (b/2728123)
  in_safe_mode = (::GetSystemMetrics(SM_CLEANBOOT) > 0);
  if (in_safe_mode) {
    return;
  }
}

BOOL OnDllProcessAttach(HINSTANCE instance, bool static_loading) {
  TipDllModule::set_module_handle(instance);
  TipTextServiceFactory::OnDllProcessAttach(instance, static_loading);
  TipUiHandler::OnDllProcessAttach(instance, static_loading);
  return TRUE;
}

BOOL OnDllProcessDetach(HINSTANCE instance, bool process_shutdown) {
  TipUiHandler::OnDllProcessDetach(instance, process_shutdown);
  TipTextServiceFactory::OnDllProcessDetach(instance, process_shutdown);
  if (!in_safe_mode && !process_shutdown) {
    // It is our responsibility to make sure that our code never touch
    // protobuf library after mozc::protobuf::ShutdownProtobufLibrary is
    // called. Unfortunately, DllMain is the only place that satisfies this
    // condition. So we carefully call it here, even though there is a risk
    // of deadlocks. See b/2126375 for details.
    ::mozc::protobuf::ShutdownProtobufLibrary();
  }

  TipDllModule::set_module_handle(nullptr);
  TipDllModule::Unload();
  return TRUE;
}

}  // namespace

// Retrieves interfaces exported by this module.
// This module exports only the IClassFactory object, which is a COM interface
// that creates an instance of the COM objects implemented by this module.
STDAPI DllGetClassObject(REFCLSID class_id, REFIID interface_id,
                         void **object) {
  absl::call_once(initialize_once, &TipBuildGlobalObjects);
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
STDAPI DllCanUnloadNow() { return mozc::win32::CanComModuleUnloadNow(); }

// Represents the entry point of this module.
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
  switch (reason) {
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
      return OnDllProcessAttach(instance, reserved != nullptr);
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
      return OnDllProcessDetach(instance, reserved != nullptr);
  }
  return TRUE;
}
