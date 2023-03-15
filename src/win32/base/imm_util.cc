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

#include "win32/base/imm_util.h"

#include <atlbase.h>
#include <atlstr.h>
#include <imm.h>
#include <msctf.h>
#include <strsafe.h>
#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/system_util.h"
#include "base/util.h"
#include "base/win32/scoped_com.h"
#include "base/win32/scoped_handle.h"
#include "win32/base/imm_registrar.h"
#include "win32/base/input_dll.h"
#include "win32/base/keyboard_layout_id.h"
#include "win32/base/tsf_profile.h"

namespace mozc {
namespace win32 {
namespace {

// Timeout value used by a work around against b/5765783. As b/6165722
// this value is determined to be:
// - smaller than the default time-out used in IsHungAppWindow API.
// - similar to the same timeout used by TSF.
constexpr uint32_t kWaitForAsmCacheReadyEventTimeout = 4500;  // 4.5 sec.

bool SetDefaultWin8() {
  wchar_t clsid[64] = {};
  if (!::StringFromGUID2(TsfProfile::GetTextServiceGuid(), clsid,
                         std::size(clsid))) {
    return E_OUTOFMEMORY;
  }
  wchar_t profile_id[64] = {};
  if (!::StringFromGUID2(TsfProfile::GetProfileGuid(), profile_id,
                         std::size(profile_id))) {
    return E_OUTOFMEMORY;
  }

  const std::wstring &profile = std::wstring(L"0x0411:") + clsid + profile_id;
  if (!::InstallLayoutOrTip(profile.c_str(), 0)) {
    DLOG(ERROR) << "InstallLayoutOrTip failed";
    return false;
  }
  if (!::SetDefaultLayoutOrTip(profile.c_str(), 0)) {
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
          TF_PROFILETYPE_INPUTPROCESSOR, kLANGJaJP,
          TsfProfile::GetTextServiceGuid(), TsfProfile::GetProfileGuid(),
          nullptr, TF_IPPMF_FORPROCESS | TF_IPPMF_FORSESSION))) {
    DLOG(ERROR) << "ActivateProfile failed";
    return false;
  }

  return true;
}

}  // namespace

bool ImeUtil::SetDefault() {
  if (SystemUtil::IsWindows8OrLater()) {
    return SetDefaultWin8();
  }

  const KeyboardLayoutID &mozc_klid = win32::ImmRegistrar::GetKLIDForIME();
  if (!mozc_klid.has_id()) {
    LOG(ERROR) << "GetKLIDForIME failed: ";
    return false;
  }

  const std::wstring &profile_list = L"0x0411:0x" + mozc_klid.ToString();
  if (!::SetDefaultLayoutOrTip(profile_list.c_str(), 0)) {
    DLOG(ERROR) << "SetDefaultLayoutOrTip failed";
    return false;
  }

  if (!ActivateForCurrentSession()) {
    LOG(ERROR) << "ActivateIMEForCurrentSession failed";
    return false;
  }
  return true;
}

bool ImeUtil::ActivateForCurrentSession() {
  const KeyboardLayoutID &mozc_hkld = ImmRegistrar::GetKLIDForIME();
  if (!mozc_hkld.has_id()) {
    return false;
  }
  const HKL mozc_hkl =
      ::LoadKeyboardLayout(mozc_hkld.ToString().c_str(), KLF_ACTIVATE);

  if (!WaitForAsmCacheReady(kWaitForAsmCacheReadyEventTimeout)) {
    LOG(ERROR) << "ImeUtil::WaitForAsmCacheReady failed.";
  }

  // Broadcasting WM_INPUTLANGCHANGEREQUEST so that existing process in the
  // current session will change their input method to |hkl|. This mechanism
  // also works against a HKL which is substituted by a TIP on Windows XP.
  // Note: we have virtually the same code in uninstall_helper.cc too.
  // TODO(yukawa): Make a common function around WM_INPUTLANGCHANGEREQUEST.
  DWORD recipients = BSM_APPLICATIONS;
  return (0 < ::BroadcastSystemMessage(BSF_POSTMESSAGE, &recipients,
                                       WM_INPUTLANGCHANGEREQUEST,
                                       INPUTLANGCHANGE_SYSCHARSET,
                                       reinterpret_cast<LPARAM>(mozc_hkl)));
}

// Wait for "MSCTF.AsmCacheReady.<desktop name><session #>" event signal to
// work around b/5765783.
bool ImeUtil::WaitForAsmCacheReady(uint32_t timeout_msec) {
  std::wstring event_name;
  if (Util::Utf8ToWide(SystemUtil::GetMSCTFAsmCacheReadyEventName(),
                       &event_name) == 0) {
    LOG(ERROR) << "Failed to compose event name.";
    return false;
  }
  ScopedHandle handle(::OpenEventW(SYNCHRONIZE, FALSE, event_name.c_str()));
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
