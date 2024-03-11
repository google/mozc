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

#include "win32/base/uninstall_helper.h"

#include <combaseapi.h>
#include <msctf.h>
#include <windows.h>

#include <cstddef>
#include <memory>
#include <string>

#include "base/logging.h"
#include "base/win32/wide_char.h"
#include "win32/base/input_dll.h"
#include "win32/base/tsf_profile.h"

// clang-format off
#include <strsafe.h>  // NOLINT: strsafe.h needs to be the last include.
// clang-format on

namespace mozc {
namespace win32 {
namespace {

// Windows NT 6.0, 6.1 and 6.2
constexpr CLSID CLSID_IMJPTIP = {
    0x03b5835f,
    0xf03c,
    0x411b,
    {0x9c, 0xe2, 0xaa, 0x23, 0xe1, 0x17, 0x1e, 0x36}};
constexpr GUID GUID_IMJPTIP = {
    0xa76c93d9,
    0x5523,
    0x4e90,
    {0xaa, 0xfa, 0x4d, 0xb1, 0x12, 0xf9, 0xac, 0x76}};

std::wstring GUIDToString(const GUID &guid) {
  wchar_t buffer[256];
  const int character_length_with_null =
      ::StringFromGUID2(guid, buffer, std::size(buffer));
  if (character_length_with_null <= 0) {
    return L"";
  }

  const size_t character_length_without_null = character_length_with_null - 1;
  return std::wstring(buffer, buffer + character_length_without_null);
}

std::wstring LANGIDToString(LANGID langid) {
  wchar_t buffer[5];
  HRESULT hr = ::StringCchPrintf(buffer, std::size(buffer), L"%04x", langid);
  if (FAILED(hr)) {
    return L"";
  }
  return buffer;
}

}  // namespace

bool UninstallHelper::RestoreUserIMEEnvironmentMain() {
  const UINT num_element =
      ::EnumEnabledLayoutOrTip(nullptr, nullptr, nullptr, nullptr, 0);
  std::unique_ptr<LAYOUTORTIPPROFILE[]> buffer(
      new LAYOUTORTIPPROFILE[num_element]);
  const UINT num_copied = ::EnumEnabledLayoutOrTip(nullptr, nullptr, nullptr,
                                                   buffer.get(), num_element);

  bool mozc_found = false;
  bool mozc_was_default = false;
  size_t mozc_index = 0;
  bool msime_found = false;
  size_t msime_index = 0;
  bool other_keyboard_found = false;
  size_t other_keyboard_index = 0;

  for (size_t i = 0; i < num_copied; ++i) {
    const LAYOUTORTIPPROFILE &src = buffer[i];
    if (::IsEqualCLSID(src.clsid, TsfProfile::GetTextServiceGuid()) &&
        ::IsEqualGUID(src.guidProfile, TsfProfile::GetProfileGuid())) {
      mozc_found = true;
      mozc_was_default = (src.dwFlags & LOT_DEFAULT) == LOT_DEFAULT;
      mozc_index = i;
      continue;
    }
    if (::IsEqualCLSID(src.clsid, CLSID_IMJPTIP) &&
        ::IsEqualGUID(src.guidProfile, GUID_IMJPTIP)) {
      msime_found = true;
      msime_index = i;
      continue;
    }
    if (!other_keyboard_found && src.catid == GUID_TFCAT_TIP_KEYBOARD) {
      other_keyboard_found = true;
      other_keyboard_index = i;
      continue;
    }
  }

  // If Mozc is already default, we need to find a new default IME.
  if (mozc_was_default) {
    if (msime_found || other_keyboard_found) {
      // Prefer MS-IME over other enabled items.
      const size_t index = msime_found ? msime_index : other_keyboard_index;
      const LAYOUTORTIPPROFILE &src = buffer[index];
      const auto desc = ::mozc::win32::StrCatW(
          L"0x", LANGIDToString(src.langid), L":", GUIDToString(src.clsid),
          GUIDToString(src.guidProfile));
      ::SetDefaultLayoutOrTip(desc.c_str(), 0);
    }
  }

  // Then remove Mozc from the enabled list.
  if (mozc_found) {
    const LAYOUTORTIPPROFILE &src = buffer[mozc_index];
    const auto desc = ::mozc::win32::StrCatW(L"0x", LANGIDToString(src.langid),
                                             L":", GUIDToString(src.clsid),
                                             GUIDToString(src.guidProfile));
    ::InstallLayoutOrTipUserReg(nullptr, nullptr, nullptr, desc.c_str(),
                                ILOT_UNINSTALL);
  }
  return true;
}

}  // namespace win32
}  // namespace mozc
