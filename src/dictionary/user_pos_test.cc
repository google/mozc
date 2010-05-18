// Copyright 2010, Google Inc.
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

#include "base/util.h"
#include "dictionary/user_pos.h"

#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace {

TEST(UserPOSTest, UserPOSBasicTest) {
  vector<string> pos_list;
  UserPOS::GetPOSList(&pos_list);
  EXPECT_FALSE(pos_list.empty());

  uint16 id = 0;
  for (size_t i = 0; i < pos_list.size(); ++i) {
    EXPECT_TRUE(UserPOS::IsValidPOS(pos_list[i]));
    EXPECT_TRUE(UserPOS::GetPOSIDs(pos_list[i], &id));
    EXPECT_GT(id, 0);
  }
}

TEST(UserPOSTest, UserPOSGetTokensTest) {
  vector<string> pos_list;
  UserPOS::GetPOSList(&pos_list);

  vector<UserPOS::Token> tokens;
  EXPECT_FALSE(UserPOS::GetTokens("", "test",
                                  pos_list[0],
                                  UserPOS::DEFAULT, &tokens));
  EXPECT_FALSE(UserPOS::GetTokens("test", "",
                                  pos_list[0],
                                  UserPOS::DEFAULT, &tokens));
  EXPECT_FALSE(UserPOS::GetTokens("test", "test",
                                  "", UserPOS::DEFAULT,
                                  &tokens));
  EXPECT_TRUE(UserPOS::GetTokens("test", "test",
                                 pos_list[0], UserPOS::DEFAULT,
                                 &tokens));

  // http://b/2674666
  // "あか,赤,形容詞"
  EXPECT_TRUE(UserPOS::GetTokens("\xE3\x81\x82\xE3\x81\x8B",
                                 "\xE8\xB5\xA4",
                                 "\xE5\xBD\xA2\xE5\xAE\xB9\xE8\xA9\x9E",
                                 UserPOS::DEFAULT,
                                 &tokens));

  for (size_t i = 0; i < pos_list.size(); ++i) {
    EXPECT_TRUE(UserPOS::GetTokens("test", "test",
                                   pos_list[i],
                                   UserPOS::DEFAULT,
                                   &tokens));
  }
}
}  // namespace
}  // namespace mozc
