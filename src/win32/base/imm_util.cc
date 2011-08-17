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

#include "win32/base/imm_util.h"

#include <windows.h>
#include <atlbase.h>
#include <atlstr.h>
#include <imm.h>
#include <strsafe.h>
#include <msctf.h>

#include <string>
#include <vector>

#include "base/base.h"
#include "base/const.h"
#include "base/scoped_ptr.h"
#include "base/util.h"
#include "win32/base/imm_registrar.h"
#include "win32/base/input_dll.h"
#include "win32/base/keyboard_layout_id.h"

namespace mozc {
namespace win32 {
namespace {
// The registry key for the CUAS setting.
const wchar_t kCUASKey[] = L"Software\\Microsoft\\CTF\\SystemShared";
const wchar_t kCUASValueName[] = L"CUAS";

bool GetDefaultLayout(LAYOUTORTIPPROFILE *profile) {
  vector<LAYOUTORTIPPROFILE> profiles;

  if (!InputDll::EnsureInitialized()) {
    return false;
  }

  if (InputDll::enum_enabled_layout_or_tip() == NULL) {
    return false;
  }

  const UINT num_element =
      InputDll::enum_enabled_layout_or_tip()(NULL, NULL, NULL, NULL, 0);

  scoped_array<LAYOUTORTIPPROFILE> buffer(new LAYOUTORTIPPROFILE[num_element]);

  const UINT num_copied = InputDll::enum_enabled_layout_or_tip()(
      NULL, NULL, NULL, buffer.get(), num_element);

  for (size_t i = 0; i < num_copied; ++i) {
    if ((buffer[i].dwFlags & LOT_DEFAULT) == LOT_DEFAULT) {
      *profile = buffer[i];
      return true;
    }
  }

  return false;
}

// Reads CUAS value in the registry keys and returns true if the value is set
// to 1.
// The CUAS value is read from 64 bit registry keys if KEY_WOW64_64KEY is
// specified as |additional_regsam| and read from 32 bit registry keys if
// KEY_WOW64_32KEY is specified.
bool IsCuasEnabledInternal(REGSAM additional_regsam) {
  REGSAM sam_desired = KEY_QUERY_VALUE | additional_regsam;
  CRegKey key;
  LONG result = key.Open(HKEY_LOCAL_MACHINE, kCUASKey, sam_desired);
  if (ERROR_SUCCESS != result) {
    LOG(ERROR) << "Cannot open HKEY_LOCAL_MACHINE\\Software\\Microsoft\\CTF\\"
                  "SystemShared: "
               << result;
    return false;
  }
  DWORD cuas;
  result = key.QueryDWORDValue(kCUASValueName, cuas);
  if (ERROR_SUCCESS != result) {
    LOG(ERROR) << "Failed to query CUAS value:" << result;
  }
  return (cuas == 1);
}

// The CUAS value is set to 64 bit registry keys if KEY_WOW64_64KEY is specified
// as |additional_regsam| and set to 32 bit registry keys if KEY_WOW64_32KEY is
// specified.
bool SetCuasEnabledInternal(bool enable, REGSAM additional_regsam) {
  REGSAM sam_desired = KEY_WRITE | additional_regsam;
  CRegKey key;
  LONG result = key.Open(HKEY_LOCAL_MACHINE, kCUASKey, sam_desired);
  if (ERROR_SUCCESS != result) {
    LOG(ERROR) << "Cannot open HKEY_LOCAL_MACHINE\\Software\\Microsoft\\CTF\\"
                  "SystemShared: "
               << result;
    return false;
  }
  const DWORD cuas = enable ? 1 : 0;
  result = key.SetDWORDValue(kCUASValueName, cuas);
  if (ERROR_SUCCESS != result) {
    LOG(ERROR) << "Failed to set CUAS value:" << result;
  }
  return true;
}

const wchar_t kTIPKeyboardKey[] = L"Software\\Microsoft\\CTF\\Assemblies\\"
                                  L"0x00000411\\"
                                  L"{34745C63-B2F0-4784-8B67-5E12C8701A31}";
}  // anonymous namespace

bool ImeUtil::IsDefault() {
  LAYOUTORTIPPROFILE profile;
  if (GetDefaultLayout(&profile)) {
    // Check if this profile looks like IMM32 version of Mozc;
    if (profile.dwProfileType != LOTP_KEYBOARDLAYOUT) {
      return false;
    }
    if (profile.clsid != GUID_NULL) {
      return false;
    }
    if (profile.guidProfile != GUID_NULL) {
      return false;
    }

    const wstring id(profile.szId);
    // A valid |profile.szId| should consists of language ID (LANGID) and
    // keyboard layout ID (KILD) as follows.
    //  <LangID 1>:<KLID 1>
    //       "0411:E0200411"
    // Check if |id.size()| is expected.
    if (id.size() != 13) {
      return false;
    }
    // Extract KLID.  It should be 8-letter hexadecimal code begins at 6th
    // character in the |id|, that is, |id.substr(5, 8)|.
    const KeyboardLayoutID default_klid(id.substr(5, 8));
    if (!default_klid.has_id()) {
      return false;
    }
    const KeyboardLayoutID mozc_klid(ImmRegistrar::GetKLIDForIME());
    if (!mozc_klid.has_id()) {
      // This means Mozc is not installed.
      return false;
    }
    return (default_klid.id() == mozc_klid.id());
  }

  HKL hkl = NULL;
  if (0 == ::SystemParametersInfo(SPI_GETDEFAULTINPUTLANG,
                                  0,
                                  reinterpret_cast<PVOID>(&hkl),
                                  0)) {
    LOG(ERROR) << "SystemParameterInfo failed: " << GetLastError();
    return false;
  }
  const LANGID langage_id = reinterpret_cast<LANGID>(hkl);
  const LANGID kJapaneseLangID =
      MAKELANGID(LANG_JAPANESE, SUBLANG_JAPANESE_JAPAN);

  if (langage_id != kJapaneseLangID) {
    return false;
  }

  return ImmRegistrar::IsIME(hkl, ImmRegistrar::GetFileNameForIME());
}

bool ImeUtil::SetDefault() {
  const KeyboardLayoutID &mozc_klid = win32::ImmRegistrar::GetKLIDForIME();
  if (!mozc_klid.has_id()) {
    LOG(ERROR) << "GetKLIDForIME failed: ";
    return false;
  }

  if (InputDll::EnsureInitialized() &&
      InputDll::set_default_layout_or_tip() != NULL) {
    // In most cases, we can use this method on Vista or later.
    const wstring &profile_list = L"0x0411:0x" + mozc_klid.ToString();
    if (!InputDll::set_default_layout_or_tip()(profile_list.c_str(), 0)) {
      DLOG(ERROR) << "SetDefaultLayoutOrTip failed";
      return false;
    }
  } else {
    // We cannot use const HKL because |&mozc_hkl| will be cast into PVOID.
    HKL hkl = ::LoadKeyboardLayout(mozc_klid.ToString().c_str(), KLF_ACTIVATE);
    if (0 == ::SystemParametersInfo(SPI_SETDEFAULTINPUTLANG,
                                    0,
                                    &hkl,
                                    SPIF_SENDCHANGE)) {
      LOG(ERROR) << "SystemParameterInfo failed: " << GetLastError();
      return false;
    }

    if (S_OK != ImmRegistrar::MovePreloadValueToTop(mozc_klid)) {
      LOG(ERROR) << "MovePreloadValueToTop failed";
      return false;
    }
  }

  if (!ActivateForCurrentSession()) {
    LOG(ERROR) << "ActivateIMEForCurrentSession failed";
    return false;
  }
  return true;
}

bool ImeUtil::IsCuasEnabled() {
  if (mozc::Util::IsVistaOrLater()) {
    // CUAS is always enabled on Vista or later.
    return true;
  }

  if (mozc::Util::IsWindowsX64()) {
    // see both 64 bit and 32 bit registry keys
    return IsCuasEnabledInternal(KEY_WOW64_64KEY) &&
           IsCuasEnabledInternal(KEY_WOW64_32KEY);
  } else {
    return IsCuasEnabledInternal(0);
  }
}

bool ImeUtil::SetCuasEnabled(bool enable) {
  if (mozc::Util::IsVistaOrLater()) {
    // No need to enable CUAS since it is always enabled on Vista or later.
    return true;
  }

  if (mozc::Util::IsWindowsX64()) {
    // see both 64 bit and 32 bit registry keys
    return SetCuasEnabledInternal(enable, KEY_WOW64_64KEY) &&
           SetCuasEnabledInternal(enable, KEY_WOW64_32KEY);
  } else {
    return SetCuasEnabledInternal(enable, 0);
  }
}

// TF_IsCtfmonRunning looks for ctfmon.exe's mutex.
// Ctfmon.exe is running if TSF is enabled.
// Most of the implementation and comments are based on a code by
// thatanaka
bool ImeUtil::IsCtfmonRunning() {
#ifdef _M_IX86
  typedef BOOL (__stdcall *PFN_TF_ISCTFMONRUNNING)();

  // If TSF is enabled and this process has created a window, msctf.dll should
  // be loaded.
  HMODULE hMsctfDll = Util::GetSystemModuleHandle(L"msctf.dll");
  if (!hMsctfDll) {
    LOG(ERROR) << "Util::GetSystemModuleHandle failed";
    return false;
  }

  PFN_TF_ISCTFMONRUNNING pfnTF_IsCtfmonRunning =
      GetProcAddress(hMsctfDll, "TF_IsCtfmonRunning");
  if (!pfnTF_IsCtfmonRunning) {
    LOG(ERROR) << "GetProcAddress for TF_IsCtfmonRunning failed";
    return false;
  }

  return((*pfnTF_IsCtfmonRunning)() == TRUE);
#else
  // TODO(mazda): Check if the function declaration is correct.
  LOG(ERROR) << "Unsupported platform";
  return false;
#endif
}

bool ImeUtil::ActivateForCurrentProcess() {
  const KeyboardLayoutID &mozc_hkld = ImmRegistrar::GetKLIDForIME();
  if (!mozc_hkld.has_id()) {
    return false;
  }
  return (0 != ::LoadKeyboardLayout(mozc_hkld.ToString().c_str(),
                                    KLF_ACTIVATE | KLF_SETFORPROCESS));
}

bool ImeUtil::ActivateForCurrentSession() {
  const KeyboardLayoutID &mozc_hkld = ImmRegistrar::GetKLIDForIME();
  if (!mozc_hkld.has_id()) {
    return false;
  }
  const HKL mozc_hkl = ::LoadKeyboardLayout(
      mozc_hkld.ToString().c_str(), KLF_ACTIVATE);
  DWORD recipients = BSM_APPLICATIONS;
  return (0 < ::BroadcastSystemMessage(
      BSF_POSTMESSAGE,
      &recipients,
      WM_INPUTLANGCHANGEREQUEST,
      INPUTLANGCHANGE_SYSCHARSET,
      reinterpret_cast<LPARAM>(mozc_hkl)));
}
}  // namespace win32
}  // namespace mozc
