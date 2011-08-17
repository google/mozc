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

#include <Windows.h>

#include "base/const.h"
#include "base/util.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "win32/base/uninstall_helper.h"

namespace mozc {
namespace win32 {
namespace {
// Windows NT 6.0, 6.1
const CLSID CLSID_IMJPTIP = {
    0x03b5835f, 0xf03c, 0x411b, {0x9c, 0xe2, 0xaa, 0x23, 0xe1, 0x17, 0x1e, 0x36}
};
const GUID GUID_IMJPTIP = {
    0xa76c93d9, 0x5523, 0x4e90, {0xaa, 0xfa, 0x4d, 0xb1, 0x12, 0xf9, 0xac, 0x76}
};
const LANGID kLANGJaJP = MAKELANGID(LANG_JAPANESE, SUBLANG_JAPANESE_JAPAN);

wstring ToWideString(const string &str) {
  wstring wide;
  if (mozc::Util::UTF8ToWide(str.c_str(), &wide) <= 0) {
    return L"";
  }
  return wide;
}
}  // anonymous namespace

// Test case for b/2950946
// 1. Install Google Japanese Input into Windows XP.
// 2. Set Google Japanese Input as the default IME.
// 3. Uninstall Google Japanese Input.
//    -> MS-IME should be the default IME.
TEST(UninstallHelperTest, Issue_2950946) {
  // Full IMM32 version of Google Japanese Input.
  KeyboardLayoutInfo gimeja;
  {
    gimeja.klid = 0xE0200411;
    gimeja.ime_filename = ToWideString(kIMEFile);
  }
  // Built-in MS-IME.
  KeyboardLayoutInfo msime;
  {
    msime.klid = 0xE0010411;
    msime.ime_filename = L"imjp81.ime";
  }

  // First entry of |current_preloads| is the default IME.
  vector<KeyboardLayoutInfo> current_preloads;
  current_preloads.push_back(gimeja);
  current_preloads.push_back(msime);

  // |installed_preloads| is sorted by |klid|.
  vector<KeyboardLayoutInfo> installed_preloads;
  installed_preloads.push_back(msime);
  installed_preloads.push_back(gimeja);

  vector<KeyboardLayoutInfo> new_preloads;

  EXPECT_TRUE(UninstallHelper::GetNewPreloadLayoutsForXP(
      current_preloads,
      installed_preloads,
      &new_preloads));

  EXPECT_EQ(1, new_preloads.size());
  EXPECT_EQ(msime.klid, new_preloads.at(0).klid);
  EXPECT_EQ(msime.ime_filename, new_preloads.at(0).ime_filename);
}

TEST(UninstallHelperTest, BasicCaseForVista) {
  // 1. Install Google Japanese Input into Windows Vista.
  // 2. Set Google Japanese Input as the default IME.
  // 3. Uninstall Google Japanese Input.
  //    -> MS-IME should be the default IME.
  vector<LayoutProfileInfo> current_profiles;
  {
    // Full IMM32 version of Google Japanese Input.
    LayoutProfileInfo info;
    info.langid = MAKELANGID(LANG_JAPANESE, SUBLANG_JAPANESE_JAPAN);
    info.clsid = CLSID_NULL;
    info.profile_guid = GUID_NULL;
    info.klid = 0xE0200411;
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

  vector<LayoutProfileInfo> installed_profiles;
  installed_profiles = current_profiles;

  LayoutProfileInfo current_default;
  LayoutProfileInfo new_default;
  vector<LayoutProfileInfo> removed_profiles;

  EXPECT_TRUE(UninstallHelper::GetNewEnabledProfileForVista(
      current_profiles,
      installed_profiles,
      &current_default,
      &new_default,
      &removed_profiles));

  EXPECT_EQ(1, removed_profiles.size());
  EXPECT_EQ(0xE0200411, removed_profiles.at(0).klid);

  EXPECT_EQ(0xE0200411, current_default.klid);
  EXPECT_EQ(GUID_IMJPTIP, new_default.profile_guid);
}

// The results of following functions are not predictable.  So
// only their availability is checked.
// TODO(yukawa): Use API hook to inject mock result.
TEST(UninstallHelperTest, LoadKeyboardProfilesTest) {
  vector<LayoutProfileInfo> installed_profiles;
  EXPECT_TRUE(UninstallHelper::GetInstalledProfilesByLanguage(
      kLANGJaJP, &installed_profiles));

  if (Util::IsVistaOrLater()) {
    vector<LayoutProfileInfo> current_profiles;
    EXPECT_TRUE(UninstallHelper::GetCurrentProfilesForVista(
        &current_profiles));
  }

  vector<KeyboardLayoutInfo> preload_layouts;
  vector<KeyboardLayoutInfo> installed_layouts;
  EXPECT_TRUE(UninstallHelper::GetKeyboardLayoutsForXP(
      &preload_layouts, &installed_layouts));
}

TEST(UninstallHelperTest, ComposeProfileStringForVistaTest) {
  vector<LayoutProfileInfo> profiles;
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
  const wstring &profile_string =
      UninstallHelper::ComposeProfileStringForVista(profiles);
  EXPECT_EQ(L"0411:E0220411;0411:{03B5835F-F03C-411B-9CE2-AA23E1171E36}"
            L"{A76C93D9-5523-4E90-AAFA-4DB112F9AC76}",
            profile_string);
}
}  // namespace win32
}  // namespace mozc
