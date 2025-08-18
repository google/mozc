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

#include "base/win32/win_util.h"

#include <wil/resource.h>
#include <windows.h>

#include <cstdint>
#include <string>
#include <string_view>

#include "base/system_util.h"
#include "testing/gmock.h"
#include "testing/gunit.h"

namespace mozc {
namespace {

constexpr std::wstring_view kNtPathPrefix = L"\\Device\\";

MATCHER(LooksLikeNtPath, "") {
  return arg.find(kNtPathPrefix) != std::wstring::npos;
}

class WinUtilLoaderLockTest : public testing::Test {
 protected:
  // Dynamically load the DLL to test the loader lock detection.
  // This dll checks the loader lock in the DllMain and
  // returns the result via IsLockHeld exported function.
  WinUtilLoaderLockTest()
      : module_(::LoadLibraryExW(L"win_util_test_dll.dll", nullptr,
                                 LOAD_LIBRARY_SEARCH_APPLICATION_DIR)) {}

  wil::unique_hmodule module_;
};

TEST_F(WinUtilLoaderLockTest, IsDLLSynchronizationHeldTest) {
  ASSERT_NE(module_, nullptr);

  using CheckProc = int(__stdcall*)();

  CheckProc is_lock_check_succeeded = reinterpret_cast<CheckProc>(
      ::GetProcAddress(module_.get(), "IsLockCheckSucceeded"));
  EXPECT_NE(is_lock_check_succeeded, nullptr);
  EXPECT_NE(is_lock_check_succeeded(), FALSE);

  CheckProc is_lock_held = reinterpret_cast<CheckProc>(
      ::GetProcAddress(module_.get(), "IsLockHeld"));
  EXPECT_NE(is_lock_held, nullptr);
  // The loader lock should be held in the DllMain.
  EXPECT_NE(is_lock_held(), FALSE);

  // Clear flags and check again from the caller which does not
  // own the loader lock. The loader lock should not be detected.
  CheckProc clear_flags_and_check_again = reinterpret_cast<CheckProc>(
      ::GetProcAddress(module_.get(), "ClearFlagsAndCheckAgain"));
  EXPECT_NE(clear_flags_and_check_again, nullptr);
  clear_flags_and_check_again();
  EXPECT_NE(is_lock_check_succeeded(), false);
  EXPECT_FALSE(is_lock_held());
}

TEST(WinUtilTest, WindowHandleTest) {
  // Should round-trip as long as the handle value is in uint32_t range.
  const HWND k32bitSource =
      reinterpret_cast<HWND>(static_cast<uintptr_t>(0x1234));
  EXPECT_EQ(
      WinUtil::DecodeWindowHandle(WinUtil::EncodeWindowHandle(k32bitSource)),
      k32bitSource);

#if defined(_M_X64)
  // OK to drop higher 32-bit.
  const HWND k64bitSource =
      reinterpret_cast<HWND>(static_cast<uintptr_t>(0xf0f1f2f3e4e5e6e7ULL));
  const HWND k64bitExpected =
      reinterpret_cast<HWND>(static_cast<uintptr_t>(0x00000000e4e5e6e7ULL));
  EXPECT_EQ(
      WinUtil::DecodeWindowHandle(WinUtil::EncodeWindowHandle(k64bitSource)),
      k64bitExpected);
#endif  // _M_X64
}

TEST(WinUtilTest, SystemEqualStringTest) {
  EXPECT_TRUE(WinUtil::SystemEqualString(L"abc", L"AbC", true));

  // case-sensitive
  EXPECT_FALSE(WinUtil::SystemEqualString(L"abc", L"AbC", false));

  // Test case in http://b/2977223
  EXPECT_FALSE(
      WinUtil::SystemEqualString(L"abc",
                                 L"a"
                                 L"\x202c"
                                 L"bc",  // U+202C: POP DIRECTIONAL FORMATTING
                                 true));

  // Test case in http://b/2977235
  EXPECT_TRUE(WinUtil::SystemEqualString(
      L"\x01bf",  // U+01BF: LATIN LETTER WYNN
      L"\x01f7",  // U+01F7: LATIN CAPITAL LETTER WYNN
      true));

  // http://www.siao2.com/2005/05/26/421987.aspx
  EXPECT_FALSE(WinUtil::SystemEqualString(
      L"\x03c2",  // U+03C2: GREEK SMALL LETTER FINAL SIGMA
      L"\x03a3",  // U+03A3: GREEK CAPITAL LETTER SIGMA
      true));

  // http://www.siao2.com/2005/05/26/421987.aspx
  EXPECT_TRUE(WinUtil::SystemEqualString(
      L"\x03c3",  // U+03C3: GREEK SMALL LETTER SIGMA
      L"\x03a3",  // U+03A3: GREEK CAPITAL LETTER SIGMA
      true));
}

TEST(WinUtilTest, SystemEqualStringTestForNUL) {
  {
    constexpr std::wstring_view kTest1 = L"abc";
    constexpr std::wstring_view kTest2 = std::wstring_view(L"abc\0", 4);

    EXPECT_EQ(kTest1.size(), 3);
    EXPECT_EQ(kTest2.size(), 4);
    EXPECT_FALSE(WinUtil::SystemEqualString(kTest1, kTest2, true));
  }
  {
    constexpr std::wstring_view kTest1 = L"abc";
    constexpr std::wstring_view kTest2 = std::wstring_view(L"abc\0def", 7);

    EXPECT_EQ(kTest1.size(), 3);
    EXPECT_EQ(kTest2.size(), 7);
    EXPECT_FALSE(WinUtil::SystemEqualString(kTest1, kTest2, true));
  }
}

TEST(WinUtilTest, AreEqualFileSystemObjectTest) {
  const std::wstring system_dir = SystemUtil::GetSystemDir();
  const std::wstring valid_path = system_dir + L"\\kernel32.dll";
  const std::wstring with_prefix = L"\\\\?\\" + valid_path;
  constexpr wchar_t kThisFileNeverExists[] = L"/this/file/never/exists";

  EXPECT_TRUE(WinUtil::AreEqualFileSystemObject(system_dir, system_dir))
      << "Can work against a directory";

  EXPECT_FALSE(WinUtil::AreEqualFileSystemObject(system_dir, valid_path));

  EXPECT_TRUE(WinUtil::AreEqualFileSystemObject(valid_path, with_prefix))
      << "Long path prefix should be supported.";

  EXPECT_FALSE(WinUtil::AreEqualFileSystemObject(kThisFileNeverExists,
                                                 kThisFileNeverExists))
      << "Must returns false against a file that does not exist.";
}

TEST(WinUtilTest, GetNtPath) {
  const std::wstring system_dir = SystemUtil::GetSystemDir();
  const std::wstring valid_path = system_dir + L"\\kernel32.dll";
  constexpr wchar_t kThisFileNeverExists[] = L"/this/file/never/exists";

  std::wstring nt_system_dir;
  EXPECT_TRUE(WinUtil::GetNtPath(system_dir, &nt_system_dir))
      << "Can work against a directory.";
  EXPECT_THAT(nt_system_dir, LooksLikeNtPath());

  EXPECT_FALSE(WinUtil::GetNtPath(system_dir, nullptr))
      << "Must fail against a nullptr argument.";

  std::wstring nt_valid_path;
  EXPECT_TRUE(WinUtil::GetNtPath(valid_path, &nt_valid_path))
      << "Can work against a file.";
  EXPECT_THAT(nt_valid_path, LooksLikeNtPath());

  EXPECT_TRUE(nt_system_dir != nt_valid_path);

  std::wstring nt_not_exists = L"foo";
  EXPECT_FALSE(WinUtil::GetNtPath(kThisFileNeverExists, &nt_not_exists))
      << "Must fail against non-exist file.";
  EXPECT_TRUE(nt_not_exists.empty()) << "Must be cleared when fails.";
}

TEST(WinUtilTest, GetProcessInitialNtPath) {
  std::wstring nt_path;
  EXPECT_TRUE(
      WinUtil::GetProcessInitialNtPath(::GetCurrentProcessId(), &nt_path));
  EXPECT_THAT(nt_path, LooksLikeNtPath());
}

}  // namespace
}  // namespace mozc
