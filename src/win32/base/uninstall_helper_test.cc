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

#include <CGuid.h>
#include <Windows.h>

#include "base/const.h"
#include "base/util.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "win32/base/tsf_profile.h"
#include "win32/base/uninstall_helper.h"

namespace mozc {
namespace win32 {
namespace {

// Windows NT 6.0, 6.1
const CLSID CLSID_IMJPTIP = {0x03b5835f,
                             0xf03c,
                             0x411b,
                             {0x9c, 0xe2, 0xaa, 0x23, 0xe1, 0x17, 0x1e, 0x36}};
const GUID GUID_IMJPTIP = {0xa76c93d9,
                           0x5523,
                           0x4e90,
                           {0xaa, 0xfa, 0x4d, 0xb1, 0x12, 0xf9, 0xac, 0x76}};
const LANGID kLANGJaJP = MAKELANGID(LANG_JAPANESE, SUBLANG_JAPANESE_JAPAN);

constexpr DWORD kJapaneseKLID = 0xE0200411;

std::wstring ToWideString(const std::string &str) {
  std::wstring wide;
  if (mozc::Util::UTF8ToWide(str, &wide) <= 0) {
    return L"";
  }
  return wide;
}

}  // namespace

TEST(UninstallHelperTest, BasicCaseForVista) {
  // 1. Install Google Japanese Input into Windows Vista.
  // 2. Set Google Japanese Input as the default IME.
  // 3. Uninstall Google Japanese Input.
  //    -> MS-IME should be the default IME.
  std::vector<LayoutProfileInfo> current_profiles;
  {
    // Full IMM32 version of Google Japanese Input.
    LayoutProfileInfo info;
    info.langid = MAKELANGID(LANG_JAPANESE, SUBLANG_JAPANESE_JAPAN);
    info.clsid = CLSID_NULL;
    info.profile_guid = GUID_NULL;
    info.klid = kJapaneseKLID;
    info.ime_filename = ToWideString(kIMEFile);
    info.is_default = true;
    info.is_tip = false;
    info.is_enabled = true;
    current_profiles.push_back(info);
  }
  {
    // Built-in MS-IME.
    LayoutProfileInfo info;
    info.langid = MAKELANGID(LANG_JAPANESE, SUBLANG_JAPANESE_JAPAN);
    info.clsid = CLSID_IMJPTIP;
    info.profile_guid = GUID_IMJPTIP;
    info.klid = 0;
    info.ime_filename.clear();
    info.is_default = false;
    info.is_tip = true;
    info.is_enabled = true;
    current_profiles.push_back(info);
  }

  std::vector<LayoutProfileInfo> installed_profiles;
  installed_profiles = current_profiles;

  LayoutProfileInfo current_default;
  LayoutProfileInfo new_default;
  std::vector<LayoutProfileInfo> removed_profiles;

  EXPECT_TRUE(UninstallHelper::GetNewEnabledProfileForVista(
      current_profiles, installed_profiles, &current_default, &new_default,
      &removed_profiles));

  EXPECT_EQ(1, removed_profiles.size());
  EXPECT_EQ(kJapaneseKLID, removed_profiles.at(0).klid);

  EXPECT_EQ(kJapaneseKLID, current_default.klid);
  EXPECT_EQ(GUID_IMJPTIP, new_default.profile_guid);
}

TEST(UninstallHelperTest, BasicCaseForWin8) {
  // 1. Install Google Japanese Input into Windows 8.
  // 2. Set Google Japanese Input (IMM32) as the default IME.
  // 3. Uninstall Google Japanese Input.
  //    -> MS-IME should be the default IME.
  std::vector<LayoutProfileInfo> current_profiles;
  {
    // Full IMM32 version of Google Japanese Input.
    LayoutProfileInfo info;
    info.langid = MAKELANGID(LANG_JAPANESE, SUBLANG_JAPANESE_JAPAN);
    info.clsid = CLSID_NULL;
    info.profile_guid = GUID_NULL;
    info.klid = kJapaneseKLID;
    info.ime_filename = ToWideString(kIMEFile);
    info.is_default = true;
    info.is_tip = false;
    info.is_enabled = true;
    current_profiles.push_back(info);
  }
  {
    // Full TSF version of Google Japanese Input.
    LayoutProfileInfo info;
    info.langid = MAKELANGID(LANG_JAPANESE, SUBLANG_JAPANESE_JAPAN);
    info.clsid = TsfProfile::GetTextServiceGuid();
    info.profile_guid = TsfProfile::GetProfileGuid();
    info.klid = 0;
    info.ime_filename.clear();
    info.is_default = false;
    info.is_tip = true;
    info.is_enabled = true;
    current_profiles.push_back(info);
  }
  {
    // Built-in MS-IME.
    LayoutProfileInfo info;
    info.langid = MAKELANGID(LANG_JAPANESE, SUBLANG_JAPANESE_JAPAN);
    info.clsid = CLSID_IMJPTIP;
    info.profile_guid = GUID_IMJPTIP;
    info.klid = 0;
    info.ime_filename.clear();
    info.is_default = false;
    info.is_tip = true;
    info.is_enabled = true;
    current_profiles.push_back(info);
  }

  std::vector<LayoutProfileInfo> installed_profiles;
  installed_profiles = current_profiles;

  LayoutProfileInfo current_default;
  LayoutProfileInfo new_default;
  std::vector<LayoutProfileInfo> removed_profiles;

  EXPECT_TRUE(UninstallHelper::GetNewEnabledProfileForVista(
      current_profiles, installed_profiles, &current_default, &new_default,
      &removed_profiles));

  EXPECT_EQ(2, removed_profiles.size());
  EXPECT_EQ(kJapaneseKLID, removed_profiles.at(0).klid);
  EXPECT_EQ(TsfProfile::GetProfileGuid(), removed_profiles.at(1).profile_guid);

  EXPECT_EQ(kJapaneseKLID, current_default.klid);
  EXPECT_EQ(GUID_IMJPTIP, new_default.profile_guid);
}

// The results of following functions are not predictable.  So
// only their availability is checked.
// TODO(yukawa): Use API hook to inject mock result.
TEST(UninstallHelperTest, LoadKeyboardProfilesTest) {
  std::vector<LayoutProfileInfo> installed_profiles;
  EXPECT_TRUE(UninstallHelper::GetInstalledProfilesByLanguage(
      kLANGJaJP, &installed_profiles));

  std::vector<LayoutProfileInfo> current_profiles;
  EXPECT_TRUE(UninstallHelper::GetCurrentProfilesForVista(&current_profiles));
}

TEST(UninstallHelperTest, ComposeProfileStringForVistaTest) {
  std::vector<LayoutProfileInfo> profiles;
  {
    LayoutProfileInfo info;
    info.langid = kLANGJaJP;
    info.klid = 0xE0220411;
    info.ime_filename = ToWideString(kIMEFile);
    profiles.push_back(info);
  }
  {
    LayoutProfileInfo info;
    info.langid = kLANGJaJP;
    info.clsid = CLSID_IMJPTIP;
    info.profile_guid = GUID_IMJPTIP;
    info.is_tip = true;
    profiles.push_back(info);
  }
  const std::wstring &profile_string =
      UninstallHelper::ComposeProfileStringForVista(profiles);
  EXPECT_EQ(
      L"0411:E0220411;0411:{03B5835F-F03C-411B-9CE2-AA23E1171E36}"
      L"{A76C93D9-5523-4E90-AAFA-4DB112F9AC76}",
      profile_string);
}

}  // namespace win32
}  // namespace mozc
