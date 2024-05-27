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

#include <msctf.h>
#include <objbase.h>
#include <wil/com.h>
#include <wil/resource.h>
#include <windows.h>

#include <iterator>
#include <string>
#include <vector>

#include "absl/log/log.h"
#include "base/win32/wide_char.h"
#include "win32/base/input_dll.h"
#include "win32/base/tsf_profile.h"

// clang-format off
#include <strsafe.h>  // NOLINT: strsafe.h needs to be the last include.
// clang-format on

namespace mozc {
namespace win32 {

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

  wil::com_ptr_nothrow<ITfInputProcessorProfileMgr> profile_mgr;
  {
    wil::com_ptr_nothrow<ITfInputProcessorProfiles> profiles;
    HRESULT result = TF_CreateInputProcessorProfiles(profiles.put());
    if (!profiles) {
      DLOG(ERROR) << "TF_CreateInputProcessorProfiles failed:" << result;
      return false;
    }
    result = profiles.query_to(profile_mgr.put());
    if (!profile_mgr) {
      DLOG(ERROR) << "QueryInterface to ITfInputProcessorProfileMgr failed:"
                  << result;
      return false;
    }
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

}  // namespace win32
}  // namespace mozc
