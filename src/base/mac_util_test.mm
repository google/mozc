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

#import "base/mac_util.h"

#include "testing/base/public/gunit.h"

namespace mozc {

TEST(MacUtil, IsSuppressSuggestionWindow) {
  EXPECT_FALSE(MacUtil::IsSuppressSuggestionWindow(
      "", ""));
  EXPECT_FALSE(MacUtil::IsSuppressSuggestionWindow(
      "", "Test"));
  EXPECT_FALSE(MacUtil::IsSuppressSuggestionWindow(
      "Test", ""));
  EXPECT_FALSE(MacUtil::IsSuppressSuggestionWindow(
      "Test", "Test"));

  EXPECT_TRUE(MacUtil::IsSuppressSuggestionWindow(
      "Google", "Google Chrome"));
  EXPECT_TRUE(MacUtil::IsSuppressSuggestionWindow(
      "Google", "Safari"));
  EXPECT_FALSE(MacUtil::IsSuppressSuggestionWindow(
      "Google", "Firefox"));
  // "ABC - Google 検索"
  EXPECT_TRUE(MacUtil::IsSuppressSuggestionWindow(
      "ABC - Google \xE6\xA4\x9C\xE7\xB4\xA2", "Google Chrome"));
  EXPECT_TRUE(MacUtil::IsSuppressSuggestionWindow(
      "ABC - Google \xE6\xA4\x9C\xE7\xB4\xA2", "Safari"));
  EXPECT_FALSE(MacUtil::IsSuppressSuggestionWindow(
      "ABC - Google \xE6\xA4\x9C\xE7\xB4\xA2", "Firefox"));
  EXPECT_TRUE(MacUtil::IsSuppressSuggestionWindow(
      "ABC - Google Search", "Google Chrome"));
  EXPECT_TRUE(MacUtil::IsSuppressSuggestionWindow(
      "ABC - Google Search", "Safari"));
  EXPECT_FALSE(MacUtil::IsSuppressSuggestionWindow(
      "ABC - Google Search", "Firefox"));
}

}  // namespace mozc
