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

#include "win32/base/imm_util.h"

#include <windows.h>
// Workaround against KB813540
#include <atlbase_mozc.h>
#include <atlstr.h>
#include <imm.h>
#include <msctf.h>
#include <strsafe.h>

#include <memory>
#include <string>
#include <vector>

#include "base/const.h"
#include "base/logging.h"
#include "base/scoped_handle.h"
#include "base/system_util.h"
#include "base/util.h"
#include "base/win_util.h"
#include "win32/base/imm_registrar.h"
#include "win32/base/input_dll.h"
#include "win32/base/keyboard_layout_id.h"
#include "win32/base/tsf_profile.h"

namespace mozc {
namespace win32 {
namespace {

using std::unique_ptr;

// The registry key for the CUAS setting.
// Note: We have the same values in base/win_util.cc
// TODO(yukawa): Define these constants at the same place.
const wchar_t kCUASKey[] = L"Software\\Microsoft\\CTF\\SystemShared";
const wchar_t kCUASValueName[] = L"CUAS";

// Timeout value used by a work around against b/5765783. As b/6165722
// this value is determined to be:
// - smaller than the default time-out used in IsHungAppWindow API.
// - similar to the same timeout used by TSF.
const uint32 kWaitForAsmCacheReadyEventTimeout = 4500;  // 4.5 sec.

bool GetDefaultLayout(LAYOUTORTIPPROFILE *profile) {
  if (!InputDll::EnsureInitialized()) {
    return false;
  }

  if (InputDll::enum_enabled_layout_or_tip() == nullptr) {
    return false;
  }

  const UINT num_element = InputDll::enum_enabled_layout_or_tip()(
      nullptr, nullptr, nullptr, nullptr, 0);

  unique_ptr<LAYOUTORTIPPROFILE[]> buffer(new LAYOUTORTIPPROFILE[num_element]);

  const UINT num_copied = InputDll::enum_enabled_layout_or_tip()(
      nullptr, nullptr, nullptr, buffer.get(), num_element);

  for (size_t i = 0; i < num_copied; ++i) {
    if ((buffer[i].dwFlags & LOT_DEFAULT) == LOT_DEFAULT) {
      *profile = buffer[i];
      return true;
    }
  }

  return false;
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

bool IsDefaultWin8() {
  LAYOUTORTIPPROFILE profile = {};
  if (!GetDefaultLayout(&profile)) {
    return false;
  }
  // Check if this profile looks like TSF version of Mozc;
  if (profile.dwProfileType != LOTP_INPUTPROCESSOR) {
    return false;
  }
  if (!IsEqualCLSID(profile.clsid, TsfProfile::GetTextServiceGuid())) {
    return false;
  }
  if (!IsEqualGUID(profile.guidProfile, TsfProfile::GetProfileGuid())) {
    return false;
  }
  return true;
}

bool SetDefaultWin8() {
  if (!InputDll::EnsureInitialized()) {
    return false;
  }
  if (InputDll::set_default_layout_or_tip() == nullptr) {
    return false;
  }
  wchar_t clsid[64] = {};
  if (!::StringFromGUID2(TsfProfile::GetTextServiceGuid(), clsid,
                         arraysize(clsid))) {
    return E_OUTOFMEMORY;
  }
  wchar_t profile_id[64] = {};
  if (!::StringFromGUID2(TsfProfile::GetProfileGuid(), profile_id,
                         arraysize(profile_id))) {
    return E_OUTOFMEMORY;
  }

  const wstring &profile = wstring(L"0x0411:") + clsid + profile_id;
  if (!InputDll::install_layout_or_tip()(profile.c_str(), 0)) {
    DLOG(ERROR) << "InstallLayoutOrTip failed";
    return false;
  }
  if (!InputDll::set_default_layout_or_tip()(profile.c_str(), 0)) {
    DLOG(ERROR) << "SetDefaultLayoutOrTip failed";
    return false;
  }

  // Activate the TSF Mozc.
  ScopedCOMInitializer com_initializer;
  CComPtr<ITfInputProcessorProfileMgr> profile_mgr;
  if (FAILED(profile_mgr.CoCreateInstance(CLSID_TF_InputProcessorProfiles))) {
    DLOG(ERROR) << "CoCreateInstance CLSID_TF_InputProcessorProfiles failed";
    return false;
  }
  const LANGID kLANGJaJP = MAKELANGID(LANG_JAPANESE, SUBLANG_JAPANESE_JAPAN);
  if (FAILED(profile_mgr->ActivateProfile(
          TF_PROFILETYPE_INPUTPROCESSOR,
          kLANGJaJP,
          TsfProfile::GetTextServiceGuid(),
          TsfProfile::GetProfileGuid(),
          nullptr,
          TF_IPPMF_FORPROCESS | TF_IPPMF_FORSESSION))) {
    DLOG(ERROR) << "ActivateProfile failed";
    return false;
  }

  return true;
}

}  // namespace

bool ImeUtil::IsDefault() {
  if (SystemUtil::IsWindows8OrLater()) {
    return IsDefaultWin8();
  }

  LAYOUTORTIPPROFILE profile = {};
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

  HKL hkl = nullptr;
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
  if (SystemUtil::IsWindows8OrLater()) {
    return SetDefaultWin8();
  }

  const KeyboardLayoutID &mozc_klid = win32::ImmRegistrar::GetKLIDForIME();
  if (!mozc_klid.has_id()) {
    LOG(ERROR) << "GetKLIDForIME failed: ";
    return false;
  }

  if (InputDll::EnsureInitialized() &&
      InputDll::set_default_layout_or_tip() != nullptr) {
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

bool ImeUtil::SetCuasEnabled(bool enable) {
  if (SystemUtil::IsVistaOrLater()) {
    // No need to enable CUAS since it is always enabled on Vista or later.
    return true;
  }

  if (SystemUtil::IsWindowsX64()) {
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
  HMODULE hMsctfDll = WinUtil::GetSystemModuleHandle(L"msctf.dll");
  if (!hMsctfDll) {
    LOG(ERROR) << "WinUtil::GetSystemModuleHandle failed";
    return false;
  }

  PFN_TF_ISCTFMONRUNNING pfnTF_IsCtfmonRunning =
      ::GetProcAddress(hMsctfDll, "TF_IsCtfmonRunning");
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

  if (!WaitForAsmCacheReady(kWaitForAsmCacheReadyEventTimeout)) {
    LOG(ERROR) << "ImeUtil::WaitForAsmCacheReady failed.";
  }

  // Broadcasting WM_INPUTLANGCHANGEREQUEST so that existing process in the
  // current session will change their input method to |hkl|. This mechanism
  // also works against a HKL which is substituted by a TIP on Windows XP.
  // Note: we have virtually the same code in uninstall_helper.cc too.
  // TODO(yukawa): Make a common function around WM_INPUTLANGCHANGEREQUEST.
  DWORD recipients = BSM_APPLICATIONS;
  return (0 < ::BroadcastSystemMessage(
      BSF_POSTMESSAGE,
      &recipients,
      WM_INPUTLANGCHANGEREQUEST,
      INPUTLANGCHANGE_SYSCHARSET,
      reinterpret_cast<LPARAM>(mozc_hkl)));
}

// Wait for "MSCTF.AsmCacheReady.<desktop name><session #>" event signal to
// work around b/5765783.
bool ImeUtil::WaitForAsmCacheReady(uint32 timeout_msec) {
  wstring event_name;
  if (Util::UTF8ToWide(SystemUtil::GetMSCTFAsmCacheReadyEventName(),
                       &event_name) == 0) {
    LOG(ERROR) << "Failed to compose event name.";
    return false;
  }
  ScopedHandle handle(
      ::OpenEventW(SYNCHRONIZE, FALSE, event_name.c_str()));
  if (handle.get() == nullptr) {
    // Event not found.
    // Returns true assuming that we need not to wait anything.
    return true;
  }
  const DWORD result = ::WaitForSingleObject(handle.get(), timeout_msec);
  switch (result) {
    case WAIT_OBJECT_0:
      return true;
    case WAIT_TIMEOUT:
      LOG(ERROR) << "WaitForSingleObject timeout. timeout_msec: "
                 << timeout_msec;
      return false;
    case WAIT_ABANDONED:
      LOG(ERROR) << "Event was abandoned";
      return false;
    default:
      LOG(ERROR) << "WaitForSingleObject with unknown error: " << result;
      return false;
  }
  LOG(FATAL) << "Should never reach here.";
  return false;
}

}  // namespace win32
}  // namespace mozc
