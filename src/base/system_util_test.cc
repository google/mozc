// Copyright 2010-2014, Google Inc.
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

#include "base/system_util.h"

#include <cstring>
#include <string>
#include <vector>

#include "base/number_util.h"
#include "base/port.h"
#include "base/util.h"
#include "testing/base/public/gunit.h"

namespace mozc {

class SystemUtilTest : public testing::Test {};

namespace {
// Initialize argc and argv for unittest.
class Arguments {
 public:
  Arguments(int argc, const char** argv)
      : argc_(argc) {
    argv_ = new char *[argc];
    for (int i = 0; i < argc; ++i) {
      int len = strlen(argv[i]);
      argv_[i] = new char[len + 1];
      strncpy(argv_[i], argv[i], len + 1);
    }
  }

  virtual ~Arguments() {
    for (int i = 0; i < argc_; ++i) {
      delete [] argv_[i];
    }
    delete [] argv_;
  }

  int argc() { return argc_; }
  char **argv() { return argv_; }

 private:
  int argc_;
  char **argv_;
};
}  // namespace

TEST_F(SystemUtilTest, CommandLineRotateArguments) {
  const char *arguments[] = {"command",
                             "--key1=value1",
                             "--key2", "v2",
                             "--k3=value3"};
  Arguments arg(arraysize(arguments), arguments);
  int argc = arg.argc();
  char **argv = arg.argv();

  SystemUtil::CommandLineRotateArguments(argc, &argv);
  EXPECT_EQ(5, argc);
  EXPECT_STREQ("--key1=value1", argv[0]);
  EXPECT_STREQ("--key2", argv[1]);
  EXPECT_STREQ("v2", argv[2]);
  EXPECT_STREQ("--k3=value3", argv[3]);
  EXPECT_STREQ("command", argv[4]);

  --argc;
  ++argv;
  SystemUtil::CommandLineRotateArguments(argc, &argv);
  EXPECT_EQ(4, argc);
  EXPECT_STREQ("v2", argv[0]);
  EXPECT_STREQ("--k3=value3", argv[1]);
  EXPECT_STREQ("command", argv[2]);
  EXPECT_STREQ("--key2", argv[3]);

  SystemUtil::CommandLineRotateArguments(argc, &argv);
  EXPECT_STREQ("--k3=value3", argv[0]);
  EXPECT_STREQ("command", argv[1]);
  EXPECT_STREQ("--key2", argv[2]);
  EXPECT_STREQ("v2", argv[3]);

  // Make sure the result of the rotations.
  argc = arg.argc();
  argv = arg.argv();
  EXPECT_EQ(5, argc);
  EXPECT_STREQ("--key1=value1", argv[0]);
  EXPECT_STREQ("--k3=value3", argv[1]);
  EXPECT_STREQ("command", argv[2]);
  EXPECT_STREQ("--key2", argv[3]);
  EXPECT_STREQ("v2", argv[4]);
}

TEST_F(SystemUtilTest, CommandLineGetFlag) {
  const char *arguments[] = {"command",
                             "--key1=value1",
                             "--key2", "v2",
                             "invalid_value3",
                             "--only_key3"};
  Arguments arg(arraysize(arguments), arguments);
  int argc = arg.argc();
  char **argv = arg.argv();

  string key, value;
  int used_args = 0;

  // The first argument should be skipped because it is the command name.
  --argc;
  ++argv;

  // Parse "--key1=value1".
  EXPECT_TRUE(
      SystemUtil::CommandLineGetFlag(argc, argv, &key, &value, &used_args));
  EXPECT_EQ("key1", key);
  EXPECT_EQ("value1", value);
  EXPECT_EQ(1, used_args);
  argc -= used_args;
  argv += used_args;

  // Parse "--key2" and "value2".
  EXPECT_TRUE(
      SystemUtil::CommandLineGetFlag(argc, argv, &key, &value, &used_args));
  EXPECT_EQ("key2", key);
  EXPECT_EQ("v2", value);
  EXPECT_EQ(2, used_args);
  argc -= used_args;
  argv += used_args;

  // Parse "invalid_value3".
  EXPECT_FALSE(
      SystemUtil::CommandLineGetFlag(argc, argv, &key, &value, &used_args));
  EXPECT_TRUE(key.empty());
  EXPECT_TRUE(value.empty());
  EXPECT_EQ(1, used_args);
  argc -= used_args;
  argv += used_args;

  // Parse "--only_key3".
  EXPECT_TRUE(
      SystemUtil::CommandLineGetFlag(argc, argv, &key, &value, &used_args));
  EXPECT_EQ("only_key3", key);
  EXPECT_TRUE(value.empty());
  EXPECT_EQ(1, used_args);
  argc -= used_args;
  argv += used_args;

  EXPECT_EQ(0, argc);
}

TEST_F(SystemUtilTest, IsWindowsX64Test) {
  // just make sure we can compile it.
  SystemUtil::IsWindowsX64();
}

TEST_F(SystemUtilTest, SetIsWindowsX64ModeForTest) {
  SystemUtil::SetIsWindowsX64ModeForTest(
      SystemUtil::IS_WINDOWS_X64_EMULATE_64BIT_MACHINE);
  EXPECT_TRUE(SystemUtil::IsWindowsX64());

  SystemUtil::SetIsWindowsX64ModeForTest(
      SystemUtil::IS_WINDOWS_X64_EMULATE_32BIT_MACHINE);
  EXPECT_FALSE(SystemUtil::IsWindowsX64());

  // Clear the emulation.
  SystemUtil::SetIsWindowsX64ModeForTest(
      SystemUtil::IS_WINDOWS_X64_DEFAULT_MODE);
}

#ifdef OS_WIN
TEST_F(SystemUtilTest, GetFileVersion) {
  const wchar_t kDllName[] = L"kernel32.dll";

  wstring path = SystemUtil::GetSystemDir();
  path += L"\\";
  path += kDllName;

  int major, minor, build, revision;
  EXPECT_TRUE(
      SystemUtil::GetFileVersion(path, &major, &minor, &build, &revision));
}

TEST_F(SystemUtilTest, GetFileVersionStringTest) {
  const wchar_t kDllName[] = L"kernel32.dll";

  wstring path = SystemUtil::GetSystemDir();
  path += L"\\";
  path += kDllName;

  const string version_string = SystemUtil::GetFileVersionString(path);

  vector<string> numbers;
  Util::SplitStringUsing(version_string, ".", &numbers);

  // must be 4 digits.
  ASSERT_EQ(numbers.size(), 4);

  // must be integer.
  uint32 dummy = 0;
  ASSERT_TRUE(NumberUtil::SafeStrToUInt32(numbers[0], &dummy));
  ASSERT_TRUE(NumberUtil::SafeStrToUInt32(numbers[1], &dummy));
  ASSERT_TRUE(NumberUtil::SafeStrToUInt32(numbers[2], &dummy));
  ASSERT_TRUE(NumberUtil::SafeStrToUInt32(numbers[3], &dummy));
}
#endif  // OS_WIN

TEST_F(SystemUtilTest, GetTotalPhysicalMemoryTest) {
  EXPECT_GT(SystemUtil::GetTotalPhysicalMemory(), 0);
}

#ifdef OS_ANDROID
TEST_F(SystemUtilTest, GetOSVersionStringTestForAndroid) {
  string result = SystemUtil::GetOSVersionString();
  // |result| must start with "Android ".
  EXPECT_TRUE(Util::StartsWith(result, "Android "));
}
#endif  // OS_ANDROID

#ifdef OS_WIN
TEST_F(SystemUtilTest, WindowsMaybeMLockTest) {
  size_t data_len = 32;
  void *addr = malloc(data_len);
  EXPECT_EQ(-1, SystemUtil::MaybeMLock(addr, data_len));
  EXPECT_EQ(-1, SystemUtil::MaybeMUnlock(addr, data_len));
  free(addr);
}
#endif  // OS_WIN

#ifdef OS_MACOSX
TEST_F(SystemUtilTest, MacMaybeMLockTest) {
  size_t data_len = 32;
  void *addr = malloc(data_len);
  EXPECT_EQ(0, SystemUtil::MaybeMLock(addr, data_len));
  EXPECT_EQ(0, SystemUtil::MaybeMUnlock(addr, data_len));
  free(addr);
}
#endif  // OS_MACOSX

TEST_F(SystemUtilTest, LinuxMaybeMLockTest) {
  size_t data_len = 32;
  void *addr = malloc(data_len);
#ifdef OS_LINUX
#if defined(OS_ANDROID) || defined(__native_client__)
  EXPECT_EQ(-1, SystemUtil::MaybeMLock(addr, data_len));
  EXPECT_EQ(-1, SystemUtil::MaybeMUnlock(addr, data_len));
#else
  EXPECT_EQ(0, SystemUtil::MaybeMLock(addr, data_len));
  EXPECT_EQ(0, SystemUtil::MaybeMUnlock(addr, data_len));
#endif  // defined(OS_ANDROID) || defined(__native_client__)
#endif  // OS_LINUX
  free(addr);
}

}  // namespace mozc
