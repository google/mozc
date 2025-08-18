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

#include "base/android_util.h"

#include <string>

#include "base/util.h"
#include "testing/gunit.h"
#include "testing/test_peer.h"

namespace mozc {

class AndroidUtilTestPeer : public testing::TestPeer<AndroidUtil> {
 public:
  PEER_STATIC_METHOD(ParseLine);
};

TEST(AndroidUtilTest, GetSystemProperty) {
  // Valid cases
  EXPECT_NE("", AndroidUtil::GetSystemProperty(
                    AndroidUtil::kSystemPropertyOsVersion, ""));
  // Check cache
  EXPECT_NE("", AndroidUtil::GetSystemProperty(
                    AndroidUtil::kSystemPropertyOsVersion, ""));
  EXPECT_NE("", AndroidUtil::GetSystemProperty(
                    AndroidUtil::kSystemPropertyModel, ""));

  // Invalid cases.
  EXPECT_EQ(AndroidUtil::GetSystemProperty("INVALID_KEY", ""), "");
  // Check cache.
  EXPECT_EQ(AndroidUtil::GetSystemProperty("INVALID_KEY", ""), "");
  EXPECT_EQ(AndroidUtil::GetSystemProperty("INVALID=KEY", ""), "");
  EXPECT_EQ(AndroidUtil::GetSystemProperty("", ""), "");
  // Check default value.
  EXPECT_EQ(AndroidUtil::GetSystemProperty("INVALID_KEY", "FAIL"), "FAIL");
  // Check fail cache.
  EXPECT_EQ(AndroidUtil::GetSystemProperty("INVALID_KEY", "FAIL"), "FAIL");
  // Default value should not be cached.
  EXPECT_EQ(AndroidUtil::GetSystemProperty("INVALID_KEY", "FAIL2"), "FAIL2");
}

TEST(AndroidUtilTest, ParseLine_valid) {
  struct TestCase {
    std::string line;
    std::string lhs;
    std::string rhs;
  };
  // Valid patterns.
  const TestCase testcases[] = {
      {"1=2 ", "1", "2 "},      {"1=2=3", "1", "2=3"},   {" 1=2\n", "1", "2"},
      {"\t 1=#2\n", "1", "#2"}, {"1 = 2\n", "1 ", " 2"},
  };
  std::string lhs;
  std::string rhs;
  for (size_t i = 0; i < std::size(testcases); ++i) {
    const TestCase& testcase = testcases[i];
    SCOPED_TRACE(testcase.line);
    SCOPED_TRACE(testcase.lhs);
    SCOPED_TRACE(testcase.rhs);
    EXPECT_TRUE(AndroidUtilTestPeer::ParseLine(testcase.line, &lhs, &rhs));
    EXPECT_EQ(lhs, testcase.lhs);
    EXPECT_EQ(rhs, testcase.rhs);
  }
}

TEST(AndroidUtilTest, ParseLine_invalid) {
  const char* testcases[] = {
      "1", "123", "=2", " \n", "", "#", " # 1=2\n",
  };
  std::string lhs;
  std::string rhs;
  for (size_t i = 0; i < std::size(testcases); ++i) {
    const char* testcase = testcases[i];
    SCOPED_TRACE(testcase);
    EXPECT_FALSE(AndroidUtilTestPeer::ParseLine(testcase, &lhs, &rhs));
  }
}

}  // namespace mozc
