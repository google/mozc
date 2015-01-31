// Copyright 2010-2015, Google Inc.
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

#include "base/win_util.h"

#include "base/system_util.h"
#include "base/util.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

namespace mozc {

namespace {

bool LooksLikeNtPath(const wstring &nt_path) {
  const wchar_t kPrefix[] = L"\\Device\\";

  return nt_path.find(kPrefix) != wstring::npos;
}

}  // namespace

class WinUtilLoaderLockTest : public testing::Test {
 protected:
  WinUtilLoaderLockTest()
      : module_(nullptr) {}

  virtual void SetUp() {
    // Dynamically load the DLL to test the loader lock detection.
    // This dll checks the loader lock in the DllMain and
    // returns the result via IsLockHeld exported function.
    if (module_ == nullptr) {
      module_= ::LoadLibraryW(L"win_util_test_dll.dll");
    }
  }

  virtual void TearDown() {
    if (module_ != nullptr) {
      ::FreeLibrary(module_);
    }
    module_ = nullptr;
  }

  HMODULE module_;
};

TEST_F(WinUtilLoaderLockTest, IsDLLSynchronizationHeldTest) {
  ASSERT_NE(nullptr, module_);

  typedef int (__stdcall *CheckProc)();

  CheckProc is_lock_check_succeeded = reinterpret_cast<CheckProc>(
    ::GetProcAddress(module_, "IsLockCheckSucceeded"));
  EXPECT_NE(nullptr, is_lock_check_succeeded);
  EXPECT_NE(FALSE, is_lock_check_succeeded());

  CheckProc is_lock_held = reinterpret_cast<CheckProc>(
    ::GetProcAddress(module_, "IsLockHeld"));
  EXPECT_NE(nullptr, is_lock_held);
  // The loader lock should be held in the DllMain.
  EXPECT_NE(FALSE, is_lock_held());

  // Clear flags and check again from the caller which does not
  // own the loader lock. The loader lock should not be detected.
  CheckProc clear_flags_and_check_again = reinterpret_cast<CheckProc>(
    ::GetProcAddress(module_, "ClearFlagsAndCheckAgain"));
  EXPECT_NE(nullptr, clear_flags_and_check_again);
  clear_flags_and_check_again();
  EXPECT_NE(FALSE, is_lock_check_succeeded());
  EXPECT_FALSE(is_lock_held());
}

TEST(WinUtilTest, Win32EqualStringTest) {
  bool result = false;

  result = false;
  EXPECT_TRUE(WinUtil::Win32EqualString(
      L"abc",
      L"AbC",
      true,
      &result));
  EXPECT_TRUE(result);

  // case-sensitive
  EXPECT_TRUE(WinUtil::Win32EqualString(
      L"abc",
      L"AbC",
      false,
      &result));
  EXPECT_FALSE(result);

  // Test case in http://b/2977223
  result = false;
  EXPECT_TRUE(WinUtil::Win32EqualString(
      L"abc",
      L"a" L"\x202c" L"bc",   // U+202C: POP DIRECTIONAL FORMATTING
      true,
      &result));
  EXPECT_FALSE(result);

  // Test case in http://b/2977235
  result = false;
  EXPECT_TRUE(WinUtil::Win32EqualString(
      L"\x01bf",    // U+01BF: LATIN LETTER WYNN
      L"\x01f7",    // U+01F7: LATIN CAPITAL LETTER WYNN
      true,
      &result));
  EXPECT_TRUE(result);

  // http://blogs.msdn.com/b/michkap/archive/2005/05/26/421987.aspx
  result = false;
  EXPECT_TRUE(WinUtil::Win32EqualString(
      L"\x03c2",    // U+03C2: GREEK SMALL LETTER FINAL SIGMA
      L"\x03a3",    // U+03A3: GREEK CAPITAL LETTER SIGMA
      true,
      &result));
  // Windows XP En/Ja: U+03C2 and U+03A3 are the same caracter.
  // Windows Vista En/Ja: U+03C2 and U+03A3 are the same caracter.
  // Windows 7 En/Ja: U+03C2 and U+03A3 are different from each other.
  if (SystemUtil::IsWindows7OrLater()) {
    EXPECT_FALSE(result);
  } else {
    EXPECT_TRUE(result);
  }

  // http://blogs.msdn.com/b/michkap/archive/2005/05/26/421987.aspx
  result = false;
  EXPECT_TRUE(WinUtil::Win32EqualString(
      L"\x03c3",    // U+03C3: GREEK SMALL LETTER SIGMA
      L"\x03a3",    // U+03A3: GREEK CAPITAL LETTER SIGMA
      true,
      &result));
  EXPECT_TRUE(result);
}

TEST(WinUtilTest, NativeEqualStringTest) {
  bool result = false;

  result = false;
  EXPECT_TRUE(WinUtil::NativeEqualString(
      L"abc",
      L"AbC",
      true,
      &result));
  EXPECT_TRUE(result);

  // case-sensitive
  EXPECT_TRUE(WinUtil::NativeEqualString(
      L"abc",
      L"AbC",
      false,
      &result));
  EXPECT_FALSE(result);

  // Test case in http://b/2977223
  result = false;
  EXPECT_TRUE(WinUtil::NativeEqualString(
      L"abc",
      L"a" L"\x202c" L"bc",   // U+202C: POP DIRECTIONAL FORMATTING
      true,
      &result));
  EXPECT_FALSE(result);

  // Test case in http://b/2977235
  result = false;
  EXPECT_TRUE(WinUtil::NativeEqualString(
      L"\x01bf",    // U+01BF: LATIN LETTER WYNN
      L"\x01f7",    // U+01F7: LATIN CAPITAL LETTER WYNN
      true,
      &result));
  EXPECT_TRUE(result);

  // http://blogs.msdn.com/b/michkap/archive/2005/05/26/421987.aspx
  result = false;
  EXPECT_TRUE(WinUtil::NativeEqualString(
      L"\x03c2",    // U+03C2: GREEK SMALL LETTER FINAL SIGMA
      L"\x03a3",    // U+03A3: GREEK CAPITAL LETTER SIGMA
      true,
      &result));
  // Windows XP En/Ja: U+03C2 and U+03A3 are the same caracter.
  // Windows Vista En/Ja: U+03C2 and U+03A3 are the same caracter.
  // Windows 7 En/Ja: U+03C2 and U+03A3 are different from each other.
  if (SystemUtil::IsWindows7OrLater()) {
    EXPECT_FALSE(result);
  } else {
    EXPECT_TRUE(result);
  }

  // http://blogs.msdn.com/b/michkap/archive/2005/05/26/421987.aspx
  result = false;
  EXPECT_TRUE(WinUtil::NativeEqualString(
      L"\x03c3",    // U+03C3: GREEK SMALL LETTER SIGMA
      L"\x03a3",    // U+03A3: GREEK CAPITAL LETTER SIGMA
      true,
      &result));
  EXPECT_TRUE(result);
}

TEST(WinUtilTest, CrtEqualStringTest) {
  bool result = false;
  WinUtil::CrtEqualString(
    L"abc",
    L"AbC",
    true,
    &result);
  EXPECT_TRUE(result);

  // case-sensitive
  WinUtil::CrtEqualString(
      L"abc",
      L"AbC",
      false,
      &result);
  EXPECT_FALSE(result);

  // Test case in http://b/2977223
  result = false;
  WinUtil::CrtEqualString(
    L"abc",
    L"a" L"\x202c" L"bc",   // U+202C: POP DIRECTIONAL FORMATTING
    true,
    &result);
  EXPECT_FALSE(result);

  // Test case in http://b/2977235
  result = false;
  WinUtil::CrtEqualString(
    L"\x01bf",    // U+01BF: LATIN LETTER WYNN
    L"\x01f7",    // U+01F7: LATIN CAPITAL LETTER WYNN
    true,
    &result);
  EXPECT_TRUE(result);

  // http://blogs.msdn.com/b/michkap/archive/2005/05/26/421987.aspx
  result = false;
  WinUtil::CrtEqualString(
      L"\x03c2",    // U+03C2: GREEK SMALL LETTER FINAL SIGMA
      L"\x03a3",    // U+03A3: GREEK CAPITAL LETTER SIGMA
      true,
      &result);
  // Unfortunately, this result is not compatible with Win32EqualString/
  // NativeEqualString.
  EXPECT_FALSE(result);

  // http://blogs.msdn.com/b/michkap/archive/2005/05/26/421987.aspx
  result = false;
  WinUtil::CrtEqualString(
      L"\x03c3",    // U+03C3: GREEK SMALL LETTER SIGMA
      L"\x03a3",    // U+03A3: GREEK CAPITAL LETTER SIGMA
      true,
      &result);
  EXPECT_TRUE(result);
}

// Actually WinUtil::SystemEqualString raises DCHECK error when argument
// strings contain any NUL character in debug build.
#if !defined(DEBUG)
TEST(WinUtilTest, SystemEqualStringTestForNUL) {
  {
    const wchar_t kTestBuffer[] = L"abc";
    const wstring test_string1(kTestBuffer);
    const wstring test_string2(kTestBuffer,
                               kTestBuffer + arraysize(kTestBuffer));

    EXPECT_EQ(3, test_string1.size());
    EXPECT_EQ(4, test_string2.size());
    EXPECT_TRUE(WinUtil::SystemEqualString(
      test_string1,
      test_string2,
      true));
  }
  {
    const wchar_t kTestBuffer[] = L"abc\0def";
    const wstring test_string1(kTestBuffer);
    const wstring test_string2(kTestBuffer,
                               kTestBuffer + arraysize(kTestBuffer));

    EXPECT_EQ(3, test_string1.size());
    EXPECT_EQ(8, test_string2.size());
    EXPECT_TRUE(WinUtil::SystemEqualString(
      test_string1,
      test_string2,
      true));
  }
}
#endif  // DEBUG

TEST(WinUtilTest, AreEqualFileSystemObjectTest) {
  const wstring system_dir = SystemUtil::GetSystemDir();
  const wstring notepad = system_dir + L"\\notepad.exe";
  const wstring notepad_with_prefix = L"\\\\?\\" + notepad;
  const wchar_t kThisFileNeverExists[] = L"/this/file/never/exists";

  EXPECT_TRUE(WinUtil::AreEqualFileSystemObject(system_dir, system_dir))
      << "Can work against a directory";

  EXPECT_FALSE(WinUtil::AreEqualFileSystemObject(system_dir, notepad));

  EXPECT_TRUE(WinUtil::AreEqualFileSystemObject(notepad, notepad_with_prefix))
      << "Long path prefix should be supported.";

  EXPECT_FALSE(WinUtil::AreEqualFileSystemObject(
      kThisFileNeverExists, kThisFileNeverExists))
      << "Must returns false against a file that does not exist.";
}

TEST(WinUtilTest, GetNtPath) {
  const wstring system_dir = SystemUtil::GetSystemDir();
  const wstring notepad = system_dir + L"\\notepad.exe";
  const wchar_t kThisFileNeverExists[] = L"/this/file/never/exists";

  wstring nt_system_dir;
  EXPECT_TRUE(WinUtil::GetNtPath(system_dir, &nt_system_dir))
      << "Can work against a directory.";
  EXPECT_TRUE(LooksLikeNtPath(nt_system_dir));

  EXPECT_FALSE(WinUtil::GetNtPath(system_dir, nullptr))
      << "Must fail against a NULL argument.";

  wstring nt_notepad;
  EXPECT_TRUE(WinUtil::GetNtPath(notepad, &nt_notepad))
      << "Can work against a file.";
  EXPECT_TRUE(LooksLikeNtPath(nt_notepad));

  EXPECT_TRUE(nt_system_dir != nt_notepad);

  wstring nt_not_exists = L"foo";
  EXPECT_FALSE(WinUtil::GetNtPath(kThisFileNeverExists, &nt_not_exists))
      << "Must fail against non-exist file.";
  EXPECT_TRUE(nt_not_exists.empty())
      << "Must be cleared when fails.";
}

TEST(WinUtilTest, GetProcessInitialNtPath) {
  wstring nt_path;
  EXPECT_TRUE(WinUtil::GetProcessInitialNtPath(::GetCurrentProcessId(),
                                               &nt_path));
  EXPECT_TRUE(LooksLikeNtPath(nt_path));
}

}  // namespace mozc
