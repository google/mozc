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

// Omaha is the code name of Google Update, which is not used in
// OSS version of Mozc.
#if !defined(GOOGLE_JAPANESE_INPUT_BUILD)
#error OmahaUtil must be used with Google Japanese Input, not OSS Mozc
#endif  // !GOOGLE_JAPANESE_INPUT_BUILD

// sidestep seems not to have supported x64 environment yet.
// TODO(yukawa): Implement DLL hook library which supports x64 code.
#if defined(_M_IX86)

#include <windows.h>
#include <strsafe.h>

#include <clocale>

#include "base/base.h"
#include "base/singleton.h"
#include "base/util.h"
#include "base/win_util.h"
#include "testing/base/public/gunit.h"
#include "win32/base/omaha_util.h"
#include "shared/opensource/patching/sidestep/cross/auto_testing_hook.h"

namespace mozc {
namespace win32 {
namespace {
// Most of the following codes are very similar to those in
// config/stats_config_util.test.cc.
// TODO(yukawa): Remove code duplication.

const wchar_t kOmahaUsageKey[] =
    L"Software\\Google\\Update\\ClientState\\"
    L"{DDCCD2A9-025E-4142-BCEB-F467B88CF830}";
const wchar_t kRegEntryNameForChannel[] = L"ap";
const wchar_t kRegEntryNameForInstallerResult[] = L"InstallerResult";
const wchar_t kRegEntryNameForInstallerResultUIString[] =
    L"InstallerResultUIString";

#define INT2HKEY(value) ((HKEY)(ULONG_PTR)((LONG)(value)))
const HKEY kHKLM32_ClientState_Read = INT2HKEY(1);
const HKEY kHKLM32_ClientState_ReadWrite = INT2HKEY(2);
const HKEY kHKLM64_ClientState_Read = INT2HKEY(3);
const HKEY kHKLM64_ClientState_ReadWrite = INT2HKEY(4);
const HKEY KRegKey_NotFound = INT2HKEY(100);
#undef INT2HKEY

bool IsEqualInLowercase(const wstring &lhs, const wstring &rhs) {
  return WinUtil::SystemEqualString(lhs, rhs, true);
}

// Win32 registry emulator for unit testing.  To separate internal state,
// set unique id at the template parameter.
template<int Id>
class RegistryEmulator {
 public:
  template<int Id>
  class PropertySelector {
   public:
    PropertySelector()
        : omaha_client_state_key_exists_(false),
          has_ap_value_(false),
          has_installer_result_(false),
          installer_result_(0),
          has_installer_result_ui_string_(false) {}

    void Clear() {
      omaha_client_state_key_exists_ = false;
      has_ap_value_ = false;
      ap_value_.clear();
      has_installer_result_ = false;
      installer_result_ = 0;
      has_installer_result_ui_string_ = false;
      installer_result_ui_string_.clear();
    }

    bool omaha_key_exists() const {
      return omaha_client_state_key_exists_;
    }
    void set_omaha_key_exists(bool exist) {
      omaha_client_state_key_exists_ = exist;
    }

    // Field definitions.
#define DEFINE_FIELD(type, field_name)   \
    type field_name() const {            \
      DCHECK(has_##field_name##_);       \
      return field_name##_;              \
    }                                    \
    void clear_##field_name##() {        \
      has_##field_name##_ = false;       \
      field_name##_ = type();            \
    }                                    \
    bool has_##field_name() const {      \
      return has_##field_name##_;        \
    }                                    \
    type * mutable_##field_name() {      \
      has_##field_name##_ = true;        \
      return &##field_name##_;           \
    }
    DEFINE_FIELD(wstring, ap_value)
    DEFINE_FIELD(DWORD, installer_result)
    DEFINE_FIELD(wstring, installer_result_ui_string)
#undef DEFINE_FIELD

   private:
    bool omaha_client_state_key_exists_;
    bool has_ap_value_;
    wstring ap_value_;
    bool has_installer_result_;
    DWORD installer_result_;
    bool has_installer_result_ui_string_;
    wstring installer_result_ui_string_;
  };

  typedef PropertySelector<Id> Property;
  RegistryEmulator()
    : hook_reg_create_(
          sidestep::MakeTestingHook(RegCreateKeyExW, TestRegCreateKeyExW)),
      hook_reg_set_(
          sidestep::MakeTestingHook(RegSetValueExW, TestRegSetValueExW)),
      hook_reg_close_(
          sidestep::MakeTestingHook(RegCloseKey, TestRegCloseKey)),
      hook_reg_open_(
          sidestep::MakeTestingHook(RegOpenKeyExW, TestRegOpenKeyExW)),
      hook_reg_query_(
          sidestep::MakeTestingHook(RegQueryValueExW, TestRegQueryValueExW)),
      hook_reg_delete_value_(
          sidestep::MakeTestingHook(RegDeleteValueW, TestRegDeleteValueW)) {
  }

  static Property *property() {
    return Singleton<Property>::get();
  }

  static HKEY GetClientStateKey(REGSAM regsam) {
    const REGSAM kReadWrite = (KEY_WRITE | KEY_READ);
    const REGSAM kRead = KEY_READ;
#if defined(_M_X64)
    // for 64-bit code
    const bool contain_wow64_32_key =
        ((regsam & KEY_WOW64_32KEY) == KEY_WOW64_32KEY);
    if ((regsam & kReadWrite) == kReadWrite) {
      return contain_wow64_32_key ? kHKLM32_ClientState_ReadWrite
                                  : kHKLM64_ClientState_ReadWrite;
    }
    if ((regsam & KEY_WRITE) == KEY_WRITE) {
      return contain_wow64_32_key ? kHKLM32_ClientState_Read
                                  : kHKLM64_ClientState_Read;
    }
#else
    // for 32-bit code
    if (Util::IsWindowsX64()) {
      // 64-bit OS
      const bool contain_wow64_64_key =
          ((regsam & KEY_WOW64_64KEY) == KEY_WOW64_64KEY);
      const bool contain_wow64_32_key =
          ((regsam & KEY_WOW64_32KEY) == KEY_WOW64_32KEY);

      EXPECT_TRUE(contain_wow64_32_key) <<
          "KEY_WOW64_32KEY should be specified just in case.";

      if ((regsam & kReadWrite) == kReadWrite) {
        return contain_wow64_64_key ? kHKLM64_ClientState_ReadWrite
                                    : kHKLM32_ClientState_ReadWrite;
      }
      if ((regsam & kRead) == kRead) {
        return contain_wow64_64_key ? kHKLM64_ClientState_Read
                                    : kHKLM32_ClientState_Read;
      }
    } else {
      // 32-bit OS
      if ((regsam & kReadWrite) == kReadWrite) {
        return kHKLM32_ClientState_ReadWrite;
      }
      if ((regsam & kRead) == kRead) {
        return kHKLM32_ClientState_Read;
      }
    }
#endif
    EXPECT_TRUE(false) << "Unexpected combination found.  regsam = " << regsam;
    return KRegKey_NotFound;
  }

  static LSTATUS WINAPI TestRegCreateKeyExW(
      HKEY key, LPCWSTR sub_key, DWORD reserved, LPWSTR class_name,
      DWORD options, REGSAM sam, LPSECURITY_ATTRIBUTES security_attributes,
      PHKEY result, LPDWORD disposition) {
    if (key != HKEY_LOCAL_MACHINE) {
      return ERROR_FILE_NOT_FOUND;
    }
    if (!IsEqualInLowercase(sub_key, kOmahaUsageKey)) {
      return ERROR_FILE_NOT_FOUND;
    }
    const HKEY returned_key = GetClientStateKey(sam);
    if (returned_key == KRegKey_NotFound) {
      return ERROR_FILE_NOT_FOUND;
    }
    if (result != NULL) {
      *result = returned_key;
    }
    property()->set_omaha_key_exists(true);
    return ERROR_SUCCESS;
  }

  static LSTATUS UpdateString(const wchar_t *value_name,
                              const wchar_t *src, DWORD num_data) {
    wstring *target = NULL;
    if (IsEqualInLowercase(value_name, kRegEntryNameForChannel)) {
      target = property()->mutable_ap_value();
    } else if (IsEqualInLowercase(
                   value_name, kRegEntryNameForInstallerResultUIString)) {
      target = property()->mutable_installer_result_ui_string();
    }

    if (target == NULL) {
      return ERROR_FILE_NOT_FOUND;
    }

    // This length includes null-termination.
    const size_t total_size_in_tchar = (num_data / sizeof(wchar_t));
    if (total_size_in_tchar > 0) {
      const size_t null_char_index = total_size_in_tchar - 1;
      EXPECT_EQ(L'\0', src[null_char_index]);
      const size_t total_length_without_null = total_size_in_tchar - 1;
      const wstring value(src, src + total_length_without_null);
      target->assign(value);
    } else {
      target->assign(L"");
    }
    return ERROR_SUCCESS;
  }

  static LSTATUS UpdateDWORD(const wchar_t *value_name,
                             const DWORD *src, DWORD num_data) {
    DWORD *target = NULL;
    if (IsEqualInLowercase(value_name, kRegEntryNameForInstallerResult)) {
      target = property()->mutable_installer_result();
    }
    if (target == NULL) {
      return ERROR_FILE_NOT_FOUND;
    }
    DCHECK_EQ(sizeof(DWORD), num_data);
    *target = *src;
    return ERROR_SUCCESS;
  }

  static LSTATUS WINAPI TestRegSetValueExW(
      HKEY key, LPCWSTR value_name, DWORD reserved, DWORD type,
      const BYTE *data, DWORD num_data) {
    if (key != kHKLM32_ClientState_ReadWrite) {
      EXPECT_TRUE(false) << "Unexpected key is specified.";
      return ERROR_ACCESS_DENIED;
    }
    const wchar_t *src = reinterpret_cast<const wchar_t *>(data);
    switch (type) {
      case REG_SZ:
        return UpdateString(
            value_name, reinterpret_cast<const wchar_t *>(data), num_data);
      case REG_DWORD:
        return UpdateDWORD(
            value_name, reinterpret_cast<const DWORD *>(data), num_data);
      default:
        return ERROR_FILE_NOT_FOUND;
    }
  }

  static LSTATUS WINAPI TestRegCloseKey(HKEY key) {
    if ((kHKLM32_ClientState_Read != key) &&
        (kHKLM32_ClientState_ReadWrite != key)) {
      EXPECT_TRUE(false) << "Unexpected key is specified.";
      return ERROR_ACCESS_DENIED;
    }
    return ERROR_SUCCESS;
  }

  static LSTATUS WINAPI TestRegOpenKeyExW(
      HKEY key, LPCWSTR sub_key, DWORD options, REGSAM sam, PHKEY result) {
    if (key != HKEY_LOCAL_MACHINE) {
      return ERROR_FILE_NOT_FOUND;
    }
    if (!IsEqualInLowercase(sub_key, kOmahaUsageKey)) {
      return ERROR_FILE_NOT_FOUND;
    }
    if (!property()->omaha_key_exists()) {
      return ERROR_FILE_NOT_FOUND;
    }
    const HKEY returned_key = GetClientStateKey(sam);
    if (returned_key == KRegKey_NotFound) {
      return ERROR_FILE_NOT_FOUND;
    }
    if (result != NULL) {
      *result = returned_key;
    }
    return ERROR_SUCCESS;
  }

  static LSTATUS QueryString(const wchar_t *value_name, DWORD *type,
                             wchar_t *dest, DWORD *num_data) {
    wstring value;
    if (IsEqualInLowercase(value_name, kRegEntryNameForChannel)) {
      if (!property()->has_ap_value()) {
        return ERROR_FILE_NOT_FOUND;
      }
      value = property()->ap_value();
    } else if (IsEqualInLowercase(
                   value_name, kRegEntryNameForInstallerResultUIString)) {
      if (!property()->has_installer_result_ui_string()) {
        return ERROR_FILE_NOT_FOUND;
      }
      value = property()->installer_result_ui_string();
    } else {
      return ERROR_FILE_NOT_FOUND;
    }

    // Add 1 for NULL.
    const size_t total_length_in_tchar = value.size() + 1;
    const DWORD value_length_in_byte =
        (total_length_in_tchar * sizeof(wchar_t));

    if (dest == NULL) {
      if (num_data != NULL) {
        *num_data = value_length_in_byte;
      }
      return ERROR_SUCCESS;
    }

    DCHECK_NE(NULL, num_data);
    const DWORD dest_buffer_size = *num_data;

    if (dest_buffer_size <= value_length_in_byte) {
      return ERROR_INSUFFICIENT_BUFFER;
    }

    const HRESULT result = ::StringCbCopyN(
        dest, dest_buffer_size, value.c_str(),
        value_length_in_byte);
    EXPECT_HRESULT_SUCCEEDED(result);
    *num_data = value_length_in_byte;

    if (type != NULL) {
      *type = REG_SZ;
    }
    return ERROR_SUCCESS;
  }

  static LSTATUS QueryDWORD(const wchar_t *value_name, DWORD *type,
                            DWORD *dest, DWORD *num_data) {
    DWORD value = 0;
    if (IsEqualInLowercase(value_name, kRegEntryNameForInstallerResult)) {
      if (!property()->has_installer_result()) {
        return ERROR_FILE_NOT_FOUND;
      }
      value = property()->installer_result();
    } else {
      return ERROR_FILE_NOT_FOUND;
    }

    const DWORD value_length_in_byte = sizeof(DWORD);

    if (dest == NULL) {
      if (num_data != NULL) {
        *num_data = value_length_in_byte;
      }
      return ERROR_SUCCESS;
    }

    DCHECK_NE(NULL, num_data);
    const DWORD dest_buffer_size = *num_data;

    if (dest_buffer_size <= value_length_in_byte) {
      return ERROR_INSUFFICIENT_BUFFER;
    }

    *dest = value;
    *num_data = value_length_in_byte;

    if (type != NULL) {
      *type = REG_DWORD;
    }
    return ERROR_SUCCESS;
  }

  static LSTATUS WINAPI TestRegQueryValueExW(
      HKEY key, LPCWSTR value_name, LPDWORD reserved, LPDWORD type,
      LPBYTE data, LPDWORD num_data) {
    if ((kHKLM32_ClientState_Read != key) &&
        (kHKLM32_ClientState_ReadWrite != key)) {
      EXPECT_TRUE(false) << "Unexpected key is specified.";
      return ERROR_ACCESS_DENIED;
    }

    if (IsEqualInLowercase(value_name, kRegEntryNameForChannel)) {
      return QueryString(value_name, type,
                         reinterpret_cast<wchar_t *>(data), num_data);
    } else if (IsEqualInLowercase(
                   value_name, kRegEntryNameForInstallerResultUIString)) {
      return QueryString(value_name, type,
                         reinterpret_cast<wchar_t *>(data), num_data);
    } else if (IsEqualInLowercase(
                   value_name, kRegEntryNameForInstallerResult)) {
      return QueryDWORD(value_name, type,
                        reinterpret_cast<DWORD *>(data), num_data);
    }
    return ERROR_FILE_NOT_FOUND;
  }

  static LSTATUS WINAPI TestRegDeleteValueW(HKEY key, LPCWSTR value_name) {
    if (kHKLM32_ClientState_ReadWrite != key) {
      EXPECT_TRUE(false) << "Unexpected key is specified.";
      return ERROR_ACCESS_DENIED;
    }
    if (IsEqualInLowercase(value_name, kRegEntryNameForChannel)) {
      if (!property()->has_ap_value()) {
        return ERROR_FILE_NOT_FOUND;
      }
      property()->clear_ap_value();
      return ERROR_SUCCESS;
    } else if (IsEqualInLowercase(
                   value_name, kRegEntryNameForInstallerResultUIString)) {
      if (!property()->has_installer_result_ui_string()) {
        return ERROR_FILE_NOT_FOUND;
      }
      property()->clear_installer_result_ui_string();
      return ERROR_SUCCESS;
    } else if (IsEqualInLowercase(
                   value_name, kRegEntryNameForInstallerResult)) {
      if (!property()->has_installer_result()) {
        return ERROR_FILE_NOT_FOUND;
      }
      property()->clear_installer_result();
      return ERROR_SUCCESS;
    }

    // Unknown entry.  Returns success either way.
    return ERROR_SUCCESS;
  }

  sidestep::AutoTestingHook hook_reg_create_;
  sidestep::AutoTestingHook hook_reg_set_;
  sidestep::AutoTestingHook hook_reg_close_;
  sidestep::AutoTestingHook hook_reg_open_;
  sidestep::AutoTestingHook hook_reg_query_;
  sidestep::AutoTestingHook hook_reg_delete_value_;
};
}  // anonymous namespace

class OmahaUtilTestOn32bitMachine : public testing::Test {
 protected:
  static void SetUpTestCase() {
    // A quick fix of b/2669319.  If mozc::Util::GetSystemDir is first called
    // when registry APIs are hooked by sidestep, GetSystemDir fails
    // unexpectedly because GetSystemDir also depends on registry API
    // internally.  The second call of mozc::Util::GetSystemDir works well
    // because it caches the result of the first call.  So any registry API
    // access occurs in the second call.  We call mozc::Util::GetSystemDir here
    // so that it works even when registry APIs are hooked.
    // TODO(yukawa): remove this quick fix as a part of b/2769852.
    Util::GetSystemDir();

    // Call IsWindowsX64 in case it internally uses registry.
    Util::IsWindowsX64();
  }

  virtual void SetUp() {
    Util::SetIsWindowsX64ModeForTest(
        Util::IS_WINDOWS_X64_EMULATE_32BIT_MACHINE);
  }

  virtual void TearDown() {
    Util::SetIsWindowsX64ModeForTest(Util::IS_WINDOWS_X64_DEFAULT_MODE);
  }
};

class OmahaUtilTestOn64bitMachine : public testing::Test {
 protected:
  static void SetUpTestCase() {
    // A quick fix of b/2669319.  If mozc::Util::GetSystemDir is first called
    // when registry APIs are hooked by sidestep, GetSystemDir fails
    // unexpectedly because GetSystemDir also depends on registry API
    // internally.  The second call of mozc::Util::GetSystemDir works well
    // because it caches the result of the first call.  So any registry API
    // access occurs in the second call.  We call mozc::Util::GetSystemDir here
    // so that it works even when registry APIs are hooked.
    // TODO(yukawa): remove this quick fix as a part of b/2769852.
    Util::GetSystemDir();

    // Call IsWindowsX64 in case it internally uses registry.
    Util::IsWindowsX64();
  }

  virtual void SetUp() {
    Util::SetIsWindowsX64ModeForTest(
        Util::IS_WINDOWS_X64_EMULATE_64BIT_MACHINE);
  }

  virtual void TearDown() {
    Util::SetIsWindowsX64ModeForTest(Util::IS_WINDOWS_X64_DEFAULT_MODE);
  }
};

TEST_F(OmahaUtilTestOn32bitMachine, ReadWriteClearChannel) {
  RegistryEmulator<__COUNTER__> test;

  // ClientStateKey does not exist.
  test.property()->Clear();
  test.property()->set_omaha_key_exists(false);
  EXPECT_TRUE(OmahaUtil::WriteChannel(L"internal-stable"));
  // The ClientState key should be created.
  EXPECT_TRUE(test.property()->omaha_key_exists());
  EXPECT_EQ(L"internal-stable", test.property()->ap_value());
  EXPECT_EQ(L"internal-stable", OmahaUtil::ReadChannel());
  EXPECT_TRUE(OmahaUtil::ClearChannel());
  EXPECT_TRUE(test.property()->omaha_key_exists());
  EXPECT_FALSE(test.property()->has_ap_value());
  EXPECT_EQ(L"", OmahaUtil::ReadChannel());

  // ClientStateKey exists. "ap" value does not exist.
  test.property()->Clear();
  test.property()->set_omaha_key_exists(true);
  EXPECT_TRUE(OmahaUtil::WriteChannel(L"internal-stable"));
  EXPECT_TRUE(test.property()->omaha_key_exists());
  EXPECT_EQ(L"internal-stable", test.property()->ap_value());
  EXPECT_EQ(L"internal-stable", OmahaUtil::ReadChannel());
  EXPECT_TRUE(OmahaUtil::ClearChannel());
  EXPECT_TRUE(test.property()->omaha_key_exists());
  EXPECT_FALSE(test.property()->has_ap_value());
  EXPECT_EQ(L"", OmahaUtil::ReadChannel());

  // ClientStateKey exists. "ap" value exists.
  test.property()->Clear();
  test.property()->set_omaha_key_exists(true);
  test.property()->mutable_ap_value()->assign(L"internal-dev");
  EXPECT_TRUE(OmahaUtil::WriteChannel(L"internal-stable"));
  EXPECT_TRUE(test.property()->omaha_key_exists());
  EXPECT_EQ(L"internal-stable", test.property()->ap_value());
  EXPECT_EQ(L"internal-stable", OmahaUtil::ReadChannel());
  EXPECT_TRUE(OmahaUtil::ClearChannel());
  EXPECT_TRUE(test.property()->omaha_key_exists());
  EXPECT_FALSE(test.property()->has_ap_value());
  EXPECT_EQ(L"", OmahaUtil::ReadChannel());
}

TEST_F(OmahaUtilTestOn64bitMachine, ReadWriteClearChannel) {
  RegistryEmulator<__COUNTER__> test;

  // ClientStateKey does not exist.
  test.property()->Clear();
  test.property()->set_omaha_key_exists(false);
  EXPECT_TRUE(OmahaUtil::WriteChannel(L"internal-stable"));
  // The ClientState key should be created.
  EXPECT_TRUE(test.property()->omaha_key_exists());
  EXPECT_EQ(L"internal-stable", test.property()->ap_value());
  EXPECT_EQ(L"internal-stable", OmahaUtil::ReadChannel());
  EXPECT_TRUE(OmahaUtil::ClearChannel());
  EXPECT_TRUE(test.property()->omaha_key_exists());
  EXPECT_FALSE(test.property()->has_ap_value());
  EXPECT_EQ(L"", OmahaUtil::ReadChannel());

  // ClientStateKey exists. "ap" value does not exist.
  test.property()->Clear();
  test.property()->set_omaha_key_exists(true);
  EXPECT_TRUE(OmahaUtil::WriteChannel(L"internal-stable"));
  EXPECT_TRUE(test.property()->omaha_key_exists());
  EXPECT_EQ(L"internal-stable", test.property()->ap_value());
  EXPECT_EQ(L"internal-stable", OmahaUtil::ReadChannel());
  EXPECT_TRUE(OmahaUtil::ClearChannel());
  EXPECT_TRUE(test.property()->omaha_key_exists());
  EXPECT_FALSE(test.property()->has_ap_value());
  EXPECT_EQ(L"", OmahaUtil::ReadChannel());

  // ClientStateKey exists. "ap" value exists.
  test.property()->Clear();
  test.property()->set_omaha_key_exists(true);
  test.property()->mutable_ap_value()->assign(L"internal-dev");
  EXPECT_TRUE(OmahaUtil::WriteChannel(L"internal-stable"));
  EXPECT_TRUE(test.property()->omaha_key_exists());
  EXPECT_EQ(L"internal-stable", test.property()->ap_value());
  EXPECT_EQ(L"internal-stable", OmahaUtil::ReadChannel());
  EXPECT_TRUE(OmahaUtil::ClearChannel());
  EXPECT_TRUE(test.property()->omaha_key_exists());
  EXPECT_FALSE(test.property()->has_ap_value());
  EXPECT_EQ(L"", OmahaUtil::ReadChannel());
}

TEST_F(OmahaUtilTestOn32bitMachine, WriteClearOmahaError) {
  RegistryEmulator<__COUNTER__> test;

  // ClientStateKey does not exist.
  test.property()->Clear();
  test.property()->set_omaha_key_exists(false);
  EXPECT_TRUE(OmahaUtil::WriteOmahaError(L"xx", L"yy"));
  // The ClientState key should be created.
  EXPECT_TRUE(test.property()->omaha_key_exists());
  EXPECT_EQ(1, test.property()->installer_result());
  EXPECT_EQ(L"yy\r\nxx", test.property()->installer_result_ui_string());

  // If the header does not exist, CRLF disappears.
  EXPECT_TRUE(OmahaUtil::WriteOmahaError(L"xx", L""));
  EXPECT_EQ(L"xx", test.property()->installer_result_ui_string());

  // Check if we can clear the error code.
  EXPECT_TRUE(OmahaUtil::ClearOmahaError());
  EXPECT_TRUE(test.property()->omaha_key_exists());
  EXPECT_EQ(0, test.property()->installer_result());
  EXPECT_EQ(L"", test.property()->installer_result_ui_string());
}

TEST_F(OmahaUtilTestOn64bitMachine, WriteClearOmahaError) {
  RegistryEmulator<__COUNTER__> test;

  // ClientStateKey does not exist.
  test.property()->Clear();
  test.property()->set_omaha_key_exists(false);
  EXPECT_TRUE(OmahaUtil::WriteOmahaError(L"xx", L"yy"));
  // The ClientState key should be created.
  EXPECT_TRUE(test.property()->omaha_key_exists());
  EXPECT_EQ(1, test.property()->installer_result());
  EXPECT_EQ(L"yy\r\nxx", test.property()->installer_result_ui_string());

  // If the header does not exist, CRLF disappears.
  EXPECT_TRUE(OmahaUtil::WriteOmahaError(L"xx", L""));
  EXPECT_EQ(L"xx", test.property()->installer_result_ui_string());

  // Check if we can clear the error code.
  EXPECT_TRUE(OmahaUtil::ClearOmahaError());
  EXPECT_TRUE(test.property()->omaha_key_exists());
  EXPECT_EQ(0, test.property()->installer_result());
  EXPECT_EQ(L"", test.property()->installer_result_ui_string());
}
}  // namespace win32
}  // namespace mozc

#endif  // _M_IX86
