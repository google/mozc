// Copyright 2010-2018, Google Inc.
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

#ifndef OS_NACL
TEST_F(SystemUtilTest, GetTotalPhysicalMemoryTest) {
  EXPECT_GT(SystemUtil::GetTotalPhysicalMemory(), 0);
}
#endif  // OS_NACL

#ifdef OS_ANDROID
TEST_F(SystemUtilTest, GetOSVersionStringTestForAndroid) {
  string result = SystemUtil::GetOSVersionString();
  // |result| must start with "Android ".
  EXPECT_TRUE(Util::StartsWith(result, "Android "));
}
#endif  // OS_ANDROID

}  // namespace mozc
