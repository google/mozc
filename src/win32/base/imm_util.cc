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

// clang-format off
#include <windows.h>  // windows.h needs to come before imm.h
// clang-format on
#include <imm.h>
#include <msctf.h>
#include <objbase.h>
#include <wil/com.h>
#include <wil/resource.h>

#include <cstdint>
#include <iterator>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/system_util.h"
#include "base/win32/com.h"
#include "base/win32/scoped_com.h"
#include "base/win32/wide_char.h"
#include "win32/base/input_dll.h"
#include "win32/base/tsf_profile.h"

// clang-format off
#include <strsafe.h>  // NOLINT: strsafe.h needs to be the last include.
// clang-format on

namespace mozc {
namespace win32 {
namespace {

// Timeout value used by a work around against b/5765783. As b/6165722
// this value is determined to be:
// - smaller than the default time-out used in IsHungAppWindow API.
// - similar to the same timeout used by TSF.
constexpr uint32_t kWaitForAsmCacheReadyEventTimeout = 4500;  // 4.5 sec.

}  // namespace

bool ImeUtil::SetDefault() {
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

  const std::wstring profile = StrCatW(L"0x0411:", clsid, profile_id);
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
  auto profile_mgr = ComCreateInstance<ITfInputProcessorProfileMgr>(
      CLSID_TF_InputProcessorProfiles);
  if (!profile_mgr) {
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

// Wait for "MSCTF.AsmCacheReady.<desktop name><session #>" event signal to
// work around b/5765783.
bool ImeUtil::WaitForAsmCacheReady(uint32_t timeout_msec) {
  const std::wstring event_name =
      Utf8ToWide(SystemUtil::GetMSCTFAsmCacheReadyEventName());
  if (event_name.empty()) {
    LOG(ERROR) << "Failed to compose event name.";
    return false;
  }
  wil::unique_event_nothrow event;
  if (!event.try_open(event_name.c_str(), SYNCHRONIZE, false)) {
    // Event not found.
    // Returns true assuming that we need not to wait anything.
    return true;
  }
  const DWORD result = ::WaitForSingleObject(event.get(), timeout_msec);
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
