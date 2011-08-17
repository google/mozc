// Copyright 2010-2011, Google Inc.
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

#include "win32/base/input_dll.h"

#include "base/base.h"
#include "base/util.h"
#include "base/win_util.h"

namespace mozc {
namespace win32 {
const wchar_t kInputDllName[] = L"input.dll";

const char kEnumEnabledLayoutOrTipName[]    = "EnumEnabledLayoutOrTip";
const char kEnumLayoutOrTipForSetup[]       = "EnumLayoutOrTipForSetup";
const char kInstallLayoutOrTipUserRegName[] = "InstallLayoutOrTipUserReg";
const char kSetDefaultLayoutOrTipName[]     = "SetDefaultLayoutOrTip";

bool InputDll::EnsureInitialized() {
  if (not_found_) {
    // Previous trial was not successful.  give up.
    return false;
  }

  if (module_ != NULL) {
    // Already initialized.
    return true;
  }

  bool lock_held = false;
  if (!WinUtil::IsDLLSynchronizationHeld(&lock_held)) {
    LOG(ERROR) << "IsDLLSynchronizationHeld failed.";
    return false;
  }
  if (lock_held) {
    LOG(INFO) << "This thread has loader lock. "
              << "LoadLibrary should not be called.";
    return false;
  }

  const HMODULE input_dll = Util::LoadSystemLibrary(kInputDllName);
  if (input_dll == NULL) {
    const int last_error = ::GetLastError();
    DLOG(INFO) << "LoadSystemLibrary(\"" << kInputDllName << "\") failed. "
               << "error = " << last_error;
    if (last_error == ERROR_MOD_NOT_FOUND) {
      not_found_ = true;
    }
    return false;
  }

  enum_enabled_layout_or_tip_ =
      reinterpret_cast<FPEnumEnabledLayoutOrTip>(
        ::GetProcAddress(input_dll, kEnumEnabledLayoutOrTipName));

  enum_layout_or_tip_for_setup_ =
      reinterpret_cast<FPEnumLayoutOrTipForSetup>(
        ::GetProcAddress(input_dll, kEnumLayoutOrTipForSetup));

  install_layout_or_tip_user_reg_ =
      reinterpret_cast<FPInstallLayoutOrTipUserReg>(
        ::GetProcAddress(input_dll, kInstallLayoutOrTipUserRegName));

  set_default_layout_or_tip_ =
      reinterpret_cast<FPSetDefaultLayoutOrTip>(
        ::GetProcAddress(input_dll, kSetDefaultLayoutOrTipName));

  // Other threads may load the same DLL concurrently.
  // Check if this thread is the first thread which updated the |module_|.
  const HMODULE original = static_cast<HMODULE>(
    ::InterlockedCompareExchangePointer(
          reinterpret_cast<volatile PVOID *>(&module_), input_dll, NULL));
  if (original == NULL) {
    // This is the first thread which updated the |module_| with valid handle.
    // Do not call FreeLibrary to keep the reference count positive.
  } else {
    // |module_| has already been updated by another thread.  Call FreeLibrary
    // to decrement the reference count which this thread owns.
    ::FreeLibrary(input_dll);
  }

  return true;
}

InputDll::FPEnumEnabledLayoutOrTip InputDll::enum_enabled_layout_or_tip() {
  return enum_enabled_layout_or_tip_;
}

InputDll::FPEnumLayoutOrTipForSetup InputDll::enum_layout_or_tip_for_setup() {
  return enum_layout_or_tip_for_setup_;
}

InputDll::FPInstallLayoutOrTipUserReg
    InputDll::install_layout_or_tip_user_reg() {
  return install_layout_or_tip_user_reg_;
}

InputDll::FPSetDefaultLayoutOrTip InputDll::set_default_layout_or_tip() {
  return set_default_layout_or_tip_;
}

bool InputDll::not_found_;

volatile HMODULE InputDll::module_;
InputDll::FPEnumEnabledLayoutOrTip InputDll::enum_enabled_layout_or_tip_;
InputDll::FPEnumLayoutOrTipForSetup
    InputDll::enum_layout_or_tip_for_setup_;
InputDll::FPInstallLayoutOrTipUserReg
    InputDll::install_layout_or_tip_user_reg_;
InputDll::FPSetDefaultLayoutOrTip InputDll::set_default_layout_or_tip_;
}  // namespace win32
}  // namespace mozc
