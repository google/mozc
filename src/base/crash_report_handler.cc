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

#include "base/crash_report_handler.h"

#if defined(OS_WIN) && defined(GOOGLE_JAPANESE_INPUT_BUILD)

#include <Windows.h>
#include <ShellAPI.h>  // for CommandLineToArgvW

#include <cstdlib>
#include <string>

#include "base/const.h"
#include "base/crash_report_util.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/process.h"
#include "base/util.h"
#include "base/version.h"
#include "base/win_util.h"
#include "third_party/breakpad/src/client/windows/handler/exception_handler.h"

namespace {

// The prefix of the pipe name which GoogleCrashHandler.exe opens for clients
// to register them.
const wchar_t kGoogleCrashHandlerPipePrefix[] =
    L"\\\\.\\pipe\\GoogleCrashServices\\";

// This is the well known SID for the system principal.
const wchar_t kSystemPrincipalSid[] =L"S-1-5-18";

// The postfix of the pipe name which GoogleCrashHandler.exe opens for clients
// to register them.
// Covert: On x86 environment, both _M_X64 and _M_IX86 are defined. So we need
//     to check _M_X64 first.
#if defined(_M_X64)
// x64 crash handler expects the postfix "-64".
// See b/5166654 or http://crbug.com/89730 for the background info.
const wchar_t kGoogleCrashHandlerPipePostfix[] =L"-x64";
#elif defined(_M_IX86)
// No postfix for the x86 crash handler.
const wchar_t kGoogleCrashHandlerPipePostfix[] =L"";
#else
#error "unsupported platform"
#endif

// The product name registered in the crash server.
const wchar_t kProductNameInCrash[] = L"Google_Japanese_IME";

// The reference count for ExceptionHandler.
int g_reference_count = 0;

// The CRITICAL_SECTION struct used for creating or deleting ExceptionHandler in
// a mutually exclusive manner.
CRITICAL_SECTION *g_critical_section = NULL;

google_breakpad::ExceptionHandler *g_handler = NULL;

struct CrashStateInformation {
  bool invalid_process_heap_detected;
  bool loader_lock_detected;
};

CrashStateInformation g_crash_state_info = { false, false };

// Returns the name of the build mode.
std::wstring GetBuildMode() {
#if defined(NO_LOGGING)
  return L"rel";
#elif defined(DEBUG)
  return L"dbg";
#else
  return L"opt";
#endif
}

// Reduces the size of the string |str| to a max of 64 chars (Extra 1 char is
// trimmed for NULL-terminator so effective characters are 63 characters).
// Required because breakpad's CustomInfoEntry raises an invalid_parameter error
// if the string we want to set is longer than 64 characters, including
// NULL-terminator.
std::wstring TrimToBreakpadMax(const std::wstring &str) {
  std::wstring shorter(str);
  return shorter.substr(0,
      google_breakpad::CustomInfoEntry::kValueMaxLength - 1);
}

// Returns the custom info structure based on the dll in parameter and the
// process type.
google_breakpad::CustomClientInfo* GetCustomInfo() {
  // Common entries.
  google_breakpad::CustomInfoEntry ver_entry(L"ver",
      mozc::Version::GetMozcVersionW().c_str());
  google_breakpad::CustomInfoEntry prod_entry(L"prod", kProductNameInCrash);
  google_breakpad::CustomInfoEntry buildmode_entry(L"Build Mode",
                                                   GetBuildMode().c_str());

  // Get the first two command line switches if they exist.
  google_breakpad::CustomInfoEntry switch1(L"switch-1", L"");
  google_breakpad::CustomInfoEntry switch2(L"switch-2", L"");
  {
    int num_args = 0;
    wchar_t** args = ::CommandLineToArgvW(::GetCommandLineW(), &num_args);
    if (args != NULL) {
      if (num_args > 1) {
        switch1.set_value(TrimToBreakpadMax(args[1]).c_str());
      }
      if (num_args > 2) {
        switch2.set_value(TrimToBreakpadMax(args[2]).c_str());
      }
      // Caller is responsible to free the returned value of CommandLineToArgv.
      ::LocalFree(args);
    }
  }

  static google_breakpad::CustomInfoEntry entries[] =
      {ver_entry, prod_entry, buildmode_entry, switch1, switch2};
  static google_breakpad::CustomClientInfo custom_info =
      {entries, arraysize(entries)};

  return &custom_info;
}

// Returns the pipe name of the GoogleCrashHandler.exe or
// GoogleCrashHandler64.exe running as a system user.
wstring GetCrashHandlerPipeName() {
  wstring pipe_name = kGoogleCrashHandlerPipePrefix;
  pipe_name.append(kSystemPrincipalSid);
  pipe_name.append(kGoogleCrashHandlerPipePostfix);
  return pipe_name;
}

class ScopedCriticalSection {
 public:
  explicit ScopedCriticalSection(CRITICAL_SECTION *critical_section)
    : critical_section_(critical_section) {
      if (critical_section_ != NULL) {
        EnterCriticalSection(critical_section_);
      }
  }
  ~ScopedCriticalSection() {
    if (critical_section_ != NULL) {
      LeaveCriticalSection(critical_section_);
    }
  }

 private:
  CRITICAL_SECTION *critical_section_;
};

// get the handle to the module containing the given executing address
HMODULE GetModuleHandleFromAddress(void *address) {
  // address may be NULL
  MEMORY_BASIC_INFORMATION mbi;
  SIZE_T result = VirtualQuery(address, &mbi, sizeof(mbi));
  if (0 == result) {
    return NULL;
  }
  return static_cast<HMODULE>(mbi.AllocationBase);
}

// get the handle to the currently executing module
HMODULE GetCurrentModuleHandle() {
  return GetModuleHandleFromAddress(GetCurrentModuleHandle);
}

// Check to see if an address is in the current module.
bool IsAddressInCurrentModule(void *address) {
  // address may be NULL
  return GetCurrentModuleHandle() == GetModuleHandleFromAddress(address);
}

bool IsCurrentModuleInStack(PCONTEXT context) {
  STACKFRAME64 stack;
  ZeroMemory(&stack, sizeof(stack));
// This macro is defined for x86 processors.
// See http://msdn.microsoft.com/ja-jp/library/b0084kay(VS.80).aspx for details.
#if defined(_M_IX86)
  stack.AddrPC.Offset = context->Eip;
  stack.AddrPC.Mode = AddrModeFlat;
  stack.AddrStack.Offset = context->Esp;
  stack.AddrStack.Mode = AddrModeFlat;
  stack.AddrFrame.Offset = context->Ebp;
  stack.AddrFrame.Mode = AddrModeFlat;
// This macro is defined for x64 processors.
#elif defined(_M_X64)
  stack.AddrPC.Offset = context->Rip;
  stack.AddrPC.Mode = AddrModeFlat;
  stack.AddrStack.Offset = context->Rsp;
  stack.AddrStack.Mode = AddrModeFlat;
  stack.AddrFrame.Offset = context->Rbp;
  stack.AddrFrame.Mode = AddrModeFlat;
#else
#error "unsupported platform"
#endif

  while (StackWalk64(IMAGE_FILE_MACHINE_I386,
                     GetCurrentProcess(),
                     GetCurrentThread(),
                     &stack,
                     context,
                     0,
                     SymFunctionTableAccess64,
                     SymGetModuleBase64,
                     0)) {
    if (IsAddressInCurrentModule(
        reinterpret_cast<void*>(stack.AddrPC.Offset))) {
      return true;
    }
  }
  return false;
}

void UpdateCrashStateInformation() {
  __try {                                              // NOLINT
    g_crash_state_info.invalid_process_heap_detected =
        (::HeapValidate(::GetProcessHeap(), 0, NULL) == FALSE);
  } __except (EXCEPTION_EXECUTE_HANDLER) {             // NOLINT
                                                       // ignore exception
  }
  __try {                                              // NOLINT
    mozc::WinUtil::IsDLLSynchronizationHeld(
        &g_crash_state_info.loader_lock_detected);
  } __except (EXCEPTION_EXECUTE_HANDLER) {             // NOLINT
                                                       // ignore exception
  }
}

bool FilterHandler(void *context, EXCEPTION_POINTERS *exinfo,
                   MDRawAssertionInfo *assertion) {
  if (exinfo == NULL) {
    // We do not catch CRT error in release build.
#ifdef NO_LOGGING
    return false;
#else
    return true;
#endif
  }

  // Make sure it's our module which cause the crash.
  if (IsAddressInCurrentModule(exinfo->ExceptionRecord->ExceptionAddress)) {
    UpdateCrashStateInformation();
    return true;
  }

  if (IsCurrentModuleInStack(exinfo->ContextRecord)) {
    UpdateCrashStateInformation();
    return true;
  }

  return false;
}

}  // namespace


namespace mozc {

bool CrashReportHandler::Initialize(bool check_address) {
  ScopedCriticalSection critical_section(g_critical_section);
  DCHECK_GE(g_reference_count, 0);
  ++g_reference_count;
  if (g_reference_count == 1 && g_handler == NULL) {
    const string acrashdump_directory =
        CrashReportUtil::GetCrashReportDirectory();
    // create a crash dump directory if not exist.
    if (!FileUtil::FileExists(acrashdump_directory)) {
      FileUtil::CreateDirectory(acrashdump_directory);
    }

    wstring crashdump_directory;
    Util::UTF8ToWide(acrashdump_directory.c_str(), &crashdump_directory);

    google_breakpad::ExceptionHandler::FilterCallback filter_callback =
        check_address ? FilterHandler : NULL;
    const auto kCrashDumpType = static_cast<MINIDUMP_TYPE>(
        MiniDumpWithUnloadedModules | MiniDumpWithProcessThreadData);
    g_handler = new google_breakpad::ExceptionHandler(
        crashdump_directory.c_str(),
        filter_callback,
        NULL,  // MinidumpCallback
        NULL,  // callback_context
        google_breakpad::ExceptionHandler::HANDLER_ALL,
        kCrashDumpType,
        GetCrashHandlerPipeName().c_str(),
        GetCustomInfo());
    g_handler->RegisterAppMemory(&g_crash_state_info,
                                 sizeof(g_crash_state_info));
#ifdef DEBUG
    g_handler->set_handle_debug_exceptions(true);
#endif  // DEBUG
    return true;
  }
  return false;
}

bool CrashReportHandler::IsInitialized() {
  ScopedCriticalSection critical_section(g_critical_section);
  return g_reference_count > 0;
}

bool CrashReportHandler::Uninitialize() {
  ScopedCriticalSection critical_section(g_critical_section);
  --g_reference_count;
  DCHECK_GE(g_reference_count, 0);
  if (g_reference_count == 0 && g_handler != NULL) {
    delete g_handler;
    g_handler = NULL;
    return true;
  }
  return false;
}

void CrashReportHandler::SetCriticalSection(
    CRITICAL_SECTION *critical_section) {
  g_critical_section = critical_section;
}

}  // namespace mozc

#else

namespace mozc {

// Null implementation for platforms where we do not want to enable breakpad.

bool CrashReportHandler::Initialize(bool check_address) {
  return false;
}

bool CrashReportHandler::IsInitialized() {
  return false;
}

bool CrashReportHandler::Uninitialize() {
  return false;
}

#ifdef OS_WIN
void CrashReportHandler::SetCriticalSection(
    CRITICAL_SECTION *critical_section) {
}
#endif  // OS_WIN

}  // namespace mozc

#endif
