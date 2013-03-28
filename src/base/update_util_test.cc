// Copyright 2010-2013, Google Inc.
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

#include <string>

#include "base/update_util.h"
#include "base/version.h"
#include "testing/base/public/gunit.h"
#if defined(OS_WIN) && defined(GOOGLE_JAPANESE_INPUT_BUILD)
#include "shared/opensource/patching/sidestep/cross/auto_testing_hook.h"
#endif  // OS_WIN && GOOGLE_JAPANESE_INPUT_BUILD

#ifdef OS_WIN
#ifdef GOOGLE_JAPANESE_INPUT_BUILD

namespace {
HKEY g_created_key;
wstring g_created_key_path;
wstring g_written_value_name;
wstring g_written_value;
DWORD g_written_type;
wstring g_queried_value_name;
wstring g_query_value_returned;
HKEY g_opened_key;
wstring g_opened_key_path;

LSTATUS WINAPI TestRegCreateKeyExW(
    HKEY hkey, LPCWSTR sub_key, DWORD reserved, LPWSTR class_name,
    DWORD options, REGSAM sam, LPSECURITY_ATTRIBUTES security_attributes,
    PHKEY result, LPDWORD disposition) {
  g_created_key = hkey;
  g_created_key_path = sub_key;
  return ERROR_SUCCESS;
}

LSTATUS WINAPI TestRegSetValueEx(
    HKEY key, LPCTSTR value_name, DWORD reserved, DWORD type,
    const BYTE *data, DWORD num_data) {
  g_written_value_name = value_name;
  g_written_value = wstring(reinterpret_cast<const wchar_t*>(data));
  g_written_type = type;
  return ERROR_SUCCESS;
}

LSTATUS WINAPI TestRegCloseKey(HKEY key) {
  return ERROR_SUCCESS;
}

LSTATUS WINAPI TestRegOpenKeyEx(
    HKEY key, LPCWSTR sub_key, DWORD options, REGSAM sam, PHKEY result) {
  g_opened_key = key;
  g_opened_key_path = sub_key;
  return ERROR_SUCCESS;
}

LSTATUS WINAPI TestRegQueryValueEx(
    HKEY key, LPCTSTR value_name, LPDWORD reserved, LPDWORD type, LPBYTE data,
    LPDWORD num_data) {
  g_queried_value_name = value_name;
  copy(g_query_value_returned.begin(), g_query_value_returned.end(),
       reinterpret_cast<wchar_t*>(data));
  *num_data = g_query_value_returned.size() * sizeof(wchar_t);
  *type = REG_SZ;
  return ERROR_SUCCESS;
}

class UpdateUtilTestWin : public testing::Test {
 protected:
  static void SetUpTestCase() {
    hook_reg_create_ =
      sidestep::MakeTestingHook(RegCreateKeyExW, TestRegCreateKeyExW);
    hook_reg_set_ =
      sidestep::MakeTestingHook(RegSetValueEx, TestRegSetValueEx);
    hook_reg_close_ =
      sidestep::MakeTestingHook(RegCloseKey, TestRegCloseKey);
    hook_reg_open_ =
      sidestep::MakeTestingHook(RegOpenKeyEx, TestRegOpenKeyEx);
    hook_reg_query_ =
      sidestep::MakeTestingHook(RegQueryValueEx, TestRegQueryValueEx);
  }

  static void TearDownTestCase() {
    delete hook_reg_create_;
    hook_reg_create_ = NULL;
    delete hook_reg_set_;
    hook_reg_set_ = NULL;
    delete hook_reg_close_;
    hook_reg_close_ = NULL;
    delete hook_reg_open_;
    hook_reg_open_ = NULL;
    delete hook_reg_query_;
    hook_reg_query_ = NULL;
  }

  virtual void SetUp() {
    g_created_key = NULL;
    g_created_key_path.clear();
    g_written_value_name.clear();
    g_written_value.clear();
    g_written_type = 0;
    g_queried_value_name.clear();
    g_opened_key = NULL;
    g_opened_key_path.clear();
    g_query_value_returned = L"1.2.3.4";
  }
  virtual void TearDown() {
  }
 private:
  static sidestep::AutoTestingHookBase *hook_reg_create_;
  static sidestep::AutoTestingHookBase *hook_reg_set_;
  static sidestep::AutoTestingHookBase *hook_reg_close_;
  static sidestep::AutoTestingHookBase *hook_reg_open_;
  static sidestep::AutoTestingHookBase *hook_reg_query_;
};

sidestep::AutoTestingHookBase *UpdateUtilTestWin::hook_reg_create_;
sidestep::AutoTestingHookBase *UpdateUtilTestWin::hook_reg_set_;
sidestep::AutoTestingHookBase *UpdateUtilTestWin::hook_reg_close_;
sidestep::AutoTestingHookBase *UpdateUtilTestWin::hook_reg_open_;
sidestep::AutoTestingHookBase *UpdateUtilTestWin::hook_reg_query_;
}  // namespace

namespace mozc {

TEST_F(UpdateUtilTestWin, WriteActiveUsageInfo) {
  EXPECT_TRUE(UpdateUtil::WriteActiveUsageInfo());
  EXPECT_EQ(HKEY_CURRENT_USER, g_created_key);
  EXPECT_EQ(L"Software\\Google\\Update\\ClientState"
            L"\\{DDCCD2A9-025E-4142-BCEB-F467B88CF830}", g_created_key_path);
  EXPECT_EQ(L"dr", g_written_value_name);
  EXPECT_EQ(L"1", g_written_value);
}

TEST_F(UpdateUtilTestWin, GetAvailableVersion) {
  string available_version =  UpdateUtil::GetAvailableVersion();
  EXPECT_EQ(HKEY_LOCAL_MACHINE, g_opened_key);
  EXPECT_EQ(L"Software\\Google\\Update\\Clients"
            L"\\{DDCCD2A9-025E-4142-BCEB-F467B88CF830}", g_opened_key_path);
  EXPECT_EQ(L"pv", g_queried_value_name);
  EXPECT_EQ("1.2.3.4", available_version);
}

TEST_F(UpdateUtilTestWin, IsNewVersionAvailable) {
  g_query_value_returned = L"0.0.0.1";
  EXPECT_FALSE(UpdateUtil::IsNewVersionAvailable());
  g_query_value_returned = L"1000.0.0.0";
  EXPECT_TRUE(UpdateUtil::IsNewVersionAvailable());
}

}  // namespace mozc

#else

// On Non-GoogleJapaneseInput build, all the methods related to Omaha
// must never be functional.
namespace mozc {

TEST(UpdateUtilTestWin, WriteActiveUsageInfo) {
  EXPECT_FALSE(UpdateUtil::WriteActiveUsageInfo());
}

TEST(UpdateUtilTestWin, GetAvailableVersion) {
  EXPECT_EQ("", UpdateUtil::GetAvailableVersion());
}

TEST(UpdateUtilTestWin, IsNewVersionAvailable) {
  EXPECT_FALSE(UpdateUtil::IsNewVersionAvailable());
}

}  // namespace mozc

#endif  // branding (GOOGLE_JAPANESE_INPUT_BUILD or not)
#endif  // OS_WIN

namespace mozc {

TEST(UpdateUtilTest, GetCurrentVersion) {
  EXPECT_EQ(UpdateUtil::GetCurrentVersion(), Version::GetMozcVersion());
}

}  // namespace mozc
