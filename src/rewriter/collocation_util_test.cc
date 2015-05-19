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

#include "rewriter/collocation_util.h"

#include "testing/base/public/gunit.h"

namespace mozc {

TEST(CollocationUtilTest, GetNormalizedScript) {
  string result;
  // "あいうえお"
  CollocationUtil::GetNormalizedScript(
      "\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86\xe3\x81\x88\xe3\x81\x8a",
      true, &result);
  // "あいうえお"
  EXPECT_EQ(
      "\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86\xe3\x81\x88\xe3\x81\x8a", result);

  // "あいうえお"
  CollocationUtil::GetNormalizedScript(
      "\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86\xe3\x81\x88\xe3\x81\x8a",
      false, &result);
  // "あいうえお"
  EXPECT_EQ(
      "\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86\xe3\x81\x88\xe3\x81\x8a", result);

  // "1個"
  CollocationUtil::GetNormalizedScript("\x31\xe5\x80\x8b", true, &result);
  // "個"
  EXPECT_EQ("\xe5\x80\x8b", result);
  // "1個"
  CollocationUtil::GetNormalizedScript("\x31\xe5\x80\x8b", false, &result);
  // "1個"
  EXPECT_EQ("\x31\xe5\x80\x8b", result);

  // "１個"
  CollocationUtil::GetNormalizedScript(
      "\xef\xbc\x91\xe5\x80\x8b", true, &result);
  // "個"
  EXPECT_EQ("\xe5\x80\x8b", result);
  // "１個"
  CollocationUtil::GetNormalizedScript(
      "\xef\xbc\x91\xe5\x80\x8b", false, &result);
  // "１個"
  EXPECT_EQ("\xef\xbc\x91\xe5\x80\x8b", result);

  CollocationUtil::GetNormalizedScript("", true, &result);
  EXPECT_EQ("", result);
  CollocationUtil::GetNormalizedScript("", false, &result);
  EXPECT_EQ("", result);

  // "＄"
  CollocationUtil::GetNormalizedScript("\xef\xbc\x84", true, &result);
  EXPECT_EQ("", result);
  // "＄"
  CollocationUtil::GetNormalizedScript("\xef\xbc\x84", false, &result);
  EXPECT_EQ("", result);

  // "等々"
  CollocationUtil::GetNormalizedScript(
      "\xe7\xad\x89\xe3\x80\x85", true, &result);
  // "等々"
  EXPECT_EQ("\xe7\xad\x89\xe3\x80\x85", result);
  // "等々"
  CollocationUtil::GetNormalizedScript(
      "\xe7\xad\x89\xe3\x80\x85", false, &result);
  // "等々"
  EXPECT_EQ("\xe7\xad\x89\xe3\x80\x85", result);


  // "％%％"
  CollocationUtil::GetNormalizedScript(
      "\xef\xbc\x85\x25\xef\xbc\x85", true, &result);
  EXPECT_EQ("%%%", result);
  // "％%％"
  CollocationUtil::GetNormalizedScript(
      "\xef\xbc\x85\x25\xef\xbc\x85", false, &result);
  EXPECT_EQ("%%%", result);
}

}  // namespace mozc
