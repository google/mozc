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

#include "dictionary/user_pos.h"

#include <string>
#include <vector>

#include "base/base.h"
#include "base/util.h"
#include "base/logging.h"
#include "data_manager/testing/mock_user_pos_manager.h"
#include "dictionary/user_pos_interface.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

namespace mozc {

class UserPOSTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    const testing::MockUserPosManager user_pos_manager;
    user_pos_.reset(new UserPOS(user_pos_manager.GetUserPOSData()));
    CHECK(user_pos_.get());
  }

  scoped_ptr<const UserPOSInterface> user_pos_;
};

TEST_F(UserPOSTest, UserPOSBasicTest) {
  vector<string> pos_list;
  user_pos_->GetPOSList(&pos_list);
  EXPECT_FALSE(pos_list.empty());

  uint16 id = 0;
  for (size_t i = 0; i < pos_list.size(); ++i) {
    EXPECT_TRUE(user_pos_->IsValidPOS(pos_list[i]));
    EXPECT_TRUE(user_pos_->GetPOSIDs(pos_list[i], &id));
    EXPECT_GT(id, 0);
  }
}

TEST_F(UserPOSTest, UserPOSGetTokensTest) {
  vector<string> pos_list;
  user_pos_->GetPOSList(&pos_list);

  vector<UserPOS::Token> tokens;
  EXPECT_FALSE(user_pos_->GetTokens("", "test",
                                    pos_list[0],
                                    &tokens));
  EXPECT_FALSE(user_pos_->GetTokens("test", "",
                                    pos_list[0],
                                    &tokens));
  EXPECT_FALSE(user_pos_->GetTokens("test", "test",
                                    "",
                                    &tokens));
  EXPECT_TRUE(user_pos_->GetTokens("test", "test",
                                   pos_list[0],
                                   &tokens));

  // http://b/2674666
  // "あか,赤,形容詞"
  EXPECT_TRUE(user_pos_->GetTokens("\xE3\x81\x82\xE3\x81\x8B",
                                   "\xE8\xB5\xA4",
                                   "\xE5\xBD\xA2\xE5\xAE\xB9\xE8\xA9\x9E",
                                   &tokens));

  for (size_t i = 0; i < pos_list.size(); ++i) {
    EXPECT_TRUE(user_pos_->GetTokens("test", "test",
                                     pos_list[i],
                                     &tokens));
  }
}

TEST_F(UserPOSTest, ConjugationTest) {
  vector<UserPOS::Token> tokens1, tokens2;
  // EXPECT_TRUE(user_pos_->GetTokens("わら", "嗤",
  // "動詞ワ行五段", &tokens1));
  // EXPECT_TRUE(user_pos_->GetTokens("わらう", "嗤う",
  // "動詞ワ行五段", &tokens2));
  EXPECT_TRUE(user_pos_->GetTokens("\xE3\x82\x8F\xE3\x82\x89", "\xE5\x97\xA4",
                                   "\xE5\x8B\x95\xE8\xA9\x9E\xE3\x83\xAF"
                                   "\xE8\xA1\x8C\xE4\xBA\x94\xE6\xAE\xB5",
                                   &tokens1));
  EXPECT_TRUE(user_pos_->GetTokens("\xE3\x82\x8F\xE3\x82\x89\xE3\x81\x86",
                                   "\xE5\x97\xA4\xE3\x81\x86",
                                   "\xE5\x8B\x95\xE8\xA9\x9E\xE3\x83\xAF"
                                   "\xE8\xA1\x8C\xE4\xBA\x94\xE6\xAE\xB5",
                                   &tokens2));
  EXPECT_EQ(tokens1.size(), tokens2.size());
  for (size_t i = 0; i < tokens1.size(); ++i) {
    EXPECT_EQ(tokens1[i].key, tokens2[i].key);
    EXPECT_EQ(tokens1[i].value, tokens2[i].value);
    EXPECT_EQ(tokens1[i].id, tokens2[i].id);
    EXPECT_EQ(tokens1[i].cost, tokens2[i].cost);
  }

  // EXPECT_TRUE(user_pos_->GetTokens("おそれ", "惧れ",
  // "動詞一段", &tokens1));
  // EXPECT_TRUE(user_pos_->GetTokens("おそれる", "惧れる",
  // "動詞一段", &tokens2));
  EXPECT_TRUE(user_pos_->GetTokens("\xE3\x81\x8A\xE3\x81\x9D\xE3\x82\x8C",
                                   "\xE6\x83\xA7\xE3\x82\x8C",
                                   "\xE5\x8B\x95\xE8\xA9\x9E"
                                   "\xE4\xB8\x80\xE6\xAE\xB5", &tokens1));
  EXPECT_TRUE(user_pos_->GetTokens("\xE3\x81\x8A\xE3\x81\x9D"
                                   "\xE3\x82\x8C\xE3\x82\x8B",
                                   "\xE6\x83\xA7\xE3\x82\x8C\xE3\x82\x8B",
                                   "\xE5\x8B\x95\xE8\xA9\x9E"
                                   "\xE4\xB8\x80\xE6\xAE\xB5", &tokens2));
  EXPECT_EQ(tokens1.size(), tokens2.size());
  for (size_t i = 0; i < tokens1.size(); ++i) {
    EXPECT_EQ(tokens1[i].key, tokens2[i].key);
    EXPECT_EQ(tokens1[i].value, tokens2[i].value);
    EXPECT_EQ(tokens1[i].id, tokens2[i].id);
    EXPECT_EQ(tokens1[i].cost, tokens2[i].cost);
  }
}

}  // namespace mozc
