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

#include "base/status.h"

#include <sstream>
#include <string>
#include <utility>

#include "testing/base/public/gunit.h"

namespace mozc {
namespace {

static const char kMessage[] = "test message";

TEST(StatusTest, DefaultConstructor) {
  const Status s;
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(StatusCode::kOk, s.code());
  EXPECT_TRUE(s.message().empty());
}

TEST(StatusTest, ConstructorWithParams) {
  const Status s(StatusCode::kUnknown, kMessage);
  EXPECT_FALSE(s.ok());
  EXPECT_EQ(StatusCode::kUnknown, s.code());
  EXPECT_EQ(kMessage, s.message());
}

TEST(StatusTest, CopyConstructor) {
  const Status s(StatusCode::kUnknown, kMessage);
  const Status t(s);
  EXPECT_FALSE(t.ok());
  EXPECT_EQ(StatusCode::kUnknown, t.code());
  EXPECT_EQ(kMessage, t.message());
}

TEST(StatusTest, CopyAssign) {
  const Status s(StatusCode::kUnknown, kMessage);
  Status t(StatusCode::kOutOfRange, "another message");
  t = s;
  EXPECT_FALSE(t.ok());
  EXPECT_EQ(StatusCode::kUnknown, t.code());
  EXPECT_EQ(kMessage, t.message());
}

TEST(StatusTest, MoveConstructor) {
  Status s(StatusCode::kUnknown, kMessage);
  const Status t(std::move(s));
  EXPECT_FALSE(t.ok());
  EXPECT_EQ(StatusCode::kUnknown, t.code());
  EXPECT_EQ(kMessage, t.message());
}

TEST(StatusTest, MoveAssign) {
  Status s(StatusCode::kUnknown, kMessage);
  Status t(StatusCode::kOutOfRange, "another message");
  t = std::move(s);
  EXPECT_FALSE(t.ok());
  EXPECT_EQ(StatusCode::kUnknown, t.code());
  EXPECT_EQ(kMessage, t.message());
}

TEST(StatusTest, WriteToOstream) {
  {
    Status s;
    std::stringstream strm;
    strm << s;
    const std::string &str = strm.str();
    EXPECT_NE(std::string::npos, str.find("OK"));
  }
  {
    Status s(StatusCode::kUnknown, kMessage);
    std::stringstream strm;
    strm << s;
    const std::string &str = strm.str();
    EXPECT_NE(std::string::npos, str.find("UNKNOWN"));
    EXPECT_NE(std::string::npos, str.find(kMessage));
  }
}

}  // namespace
}  // namespace mozc
