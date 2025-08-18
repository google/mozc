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

#include "base/update_util.h"

#ifdef _WIN32
#include "base/win32/win_api_test_helper.h"
#endif  // _WIN32
#include "testing/gunit.h"

namespace mozc {
namespace {

#ifdef _WIN32

class UpdateUtilTestWin : public testing::Test {
 protected:
  struct CallResult {
    CallResult()
        : created_key(nullptr),
          reg_create_key_ex_called(false),
          reg_set_value_ex_called(false),
          written_type(0),
          reg_close_key_called(false) {}

    HKEY created_key;
    bool reg_create_key_ex_called;
    std::wstring created_key_path;
    bool reg_set_value_ex_called;
    std::wstring written_value_name;
    std::wstring written_value;
    DWORD written_type;
    bool reg_close_key_called;
  };

  static void SetUpTestCase() {
    std::vector<WinAPITestHelper::HookRequest> requests;
    requests.push_back(
        DEFINE_HOOK("advapi32.dll", RegCreateKeyExW, HookRegCreateKeyExW));
    requests.push_back(
        DEFINE_HOOK("advapi32.dll", RegSetValueExW, HookRegSetValueExW));
    requests.push_back(
        DEFINE_HOOK("advapi32.dll", RegCloseKey, HookRegCloseKey));

    hook_restore_info_ =
        WinAPITestHelper::DoHook(::GetModuleHandle(nullptr), requests);
  }

  static void TearDownTestCase() {
    WinAPITestHelper::RestoreHook(hook_restore_info_);
    hook_restore_info_ = nullptr;
    delete call_result_;
    call_result_ = nullptr;
  }

  virtual void SetUp() { call_result_ = new CallResult(); }
  virtual void TearDown() {
    delete call_result_;
    call_result_ = nullptr;
  }

  static const CallResult& call_result() { return *call_result_; }

 private:
  static LSTATUS WINAPI
  HookRegCreateKeyExW(HKEY hkey, LPCWSTR sub_key, DWORD reserved,
                      LPWSTR class_name, DWORD options, REGSAM sam,
                      const LPSECURITY_ATTRIBUTES security_attributes,
                      PHKEY result, LPDWORD disposition) {
    call_result_->reg_create_key_ex_called = true;
    call_result_->created_key = hkey;
    call_result_->created_key_path = sub_key;
    return ERROR_SUCCESS;
  }

  static LSTATUS WINAPI HookRegSetValueExW(HKEY key, LPCTSTR value_name,
                                           DWORD reserved, DWORD type,
                                           const BYTE* data, DWORD num_data) {
    call_result_->reg_set_value_ex_called = true;
    call_result_->written_value_name = value_name;
    call_result_->written_value =
        std::wstring(reinterpret_cast<const wchar_t*>(data));
    call_result_->written_type = type;
    return ERROR_SUCCESS;
  }

  static LSTATUS WINAPI HookRegCloseKey(HKEY key) {
    call_result_->reg_close_key_called = true;
    return ERROR_SUCCESS;
  }

  static CallResult* call_result_;
  static WinAPITestHelper::RestoreInfo* hook_restore_info_;
};

UpdateUtilTestWin::CallResult* UpdateUtilTestWin::call_result_ = nullptr;
WinAPITestHelper::RestoreInfo* UpdateUtilTestWin::hook_restore_info_ = nullptr;

// For official branding build on Windows
TEST_F(UpdateUtilTestWin, WriteActiveUsageInfo) {
#if defined(GOOGLE_JAPANESE_INPUT_BUILD)
  EXPECT_TRUE(UpdateUtil::WriteActiveUsageInfo());
  ASSERT_TRUE(call_result().reg_create_key_ex_called);
  EXPECT_EQ(call_result().created_key, HKEY_CURRENT_USER);
  EXPECT_EQ(call_result().created_key_path,
            L"Software\\Google\\Update\\ClientState"
            L"\\{DDCCD2A9-025E-4142-BCEB-F467B88CF830}");
  ASSERT_TRUE(call_result().reg_set_value_ex_called);
  EXPECT_EQ(call_result().written_value_name, L"dr");
  EXPECT_EQ(call_result().written_value, L"1");
  ASSERT_TRUE(call_result().reg_close_key_called);
#else   // GOOGLE_JAPANESE_INPUT_BUILD
  EXPECT_FALSE(UpdateUtil::WriteActiveUsageInfo());
  ASSERT_FALSE(call_result().reg_create_key_ex_called);
  ASSERT_FALSE(call_result().reg_set_value_ex_called);
  ASSERT_FALSE(call_result().reg_close_key_called);
#endif  // GOOGLE_JAPANESE_INPUT_BUILD
}

#else  // _WIN32

// UpdateUtil::WriteActiveUsageInfo is not implemented except for Windows.
TEST(UpdateUtilTest, WriteActiveUsageInfo) {
  EXPECT_FALSE(UpdateUtil::WriteActiveUsageInfo());
}

#endif  // _WIN32

}  // namespace
}  // namespace mozc
