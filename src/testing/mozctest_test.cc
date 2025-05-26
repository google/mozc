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

#include "testing/mozctest.h"

#include <fstream>
#include <string>

#include "base/file/temp_dir.h"
#include "base/system_util.h"
#include "testing/gmock.h"
#include "testing/gunit.h"

namespace mozc::testing {
namespace {

using ::testing::IsEmpty;
using ::testing::Not;

TEST(MozcTestTest, GetSourcePath) {
  const std::string path = GetSourcePath({"testing", "mozctest_test.cc"});
  std::ifstream ifs(path);
  EXPECT_TRUE(ifs.good()) << path;
}

TEST(MozcTestTest, GetSourceFileOrDie) {
  const std::string path = GetSourceFileOrDie({"testing", "mozctest_test.cc"});
  std::ifstream ifs(path);
  EXPECT_TRUE(ifs.good()) << path;
}

TEST(MozcTestTest, MakeTempDirectoryOrDie) {
  TempDirectory temp_dir = MakeTempDirectoryOrDie();
  EXPECT_THAT(temp_dir.path(), Not(IsEmpty()));
}

TEST(MozcTestTest, MakeTempFileOrDie) {
  TempFile temp_file = MakeTempFileOrDie();
  EXPECT_THAT(temp_file.path(), Not(IsEmpty()));
}

class TestWithTempUserProfileTest : public TestWithTempUserProfile {};

TEST_F(TestWithTempUserProfileTest, SmokeTest) {
  EXPECT_THAT(SystemUtil::GetUserProfileDirectory(), Not(IsEmpty()));
}

}  // namespace
}  // namespace mozc::testing
