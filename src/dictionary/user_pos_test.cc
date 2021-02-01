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

#include "dictionary/user_pos.h"

#include <memory>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/util.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/user_pos_interface.h"
#include "testing/base/public/gunit.h"
#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace dictionary {
namespace {

class UserPOSTest : public ::testing::Test {
 protected:
  void SetUp() override {
    absl::string_view token_array_data, string_array_data;
    mock_data_manager_.GetUserPOSData(&token_array_data, &string_array_data);
    user_pos_ = absl::make_unique<UserPOS>(token_array_data, string_array_data);
    CHECK(user_pos_.get());
  }

  std::unique_ptr<const UserPOSInterface> user_pos_;

 private:
  const testing::MockDataManager mock_data_manager_;
};

TEST_F(UserPOSTest, UserPOSBasicTest) {
  std::vector<std::string> pos_list;
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
  std::vector<std::string> pos_list;
  user_pos_->GetPOSList(&pos_list);

  std::vector<UserPOS::Token> tokens;
  EXPECT_FALSE(user_pos_->GetTokens("", "test", pos_list[0], &tokens));
  EXPECT_FALSE(user_pos_->GetTokens("test", "", pos_list[0], &tokens));
  EXPECT_FALSE(user_pos_->GetTokens("test", "test", "", &tokens));
  EXPECT_TRUE(user_pos_->GetTokens("test", "test", pos_list[0], &tokens));

  // http://b/2674666
  EXPECT_TRUE(user_pos_->GetTokens("あか", "赤", "形容詞", &tokens));

  for (size_t i = 0; i < pos_list.size(); ++i) {
    EXPECT_TRUE(user_pos_->GetTokens("test", "test", pos_list[i], &tokens));
  }
}

TEST_F(UserPOSTest, UserPOSGetTokensWithLocaleTest) {
  std::vector<std::string> pos_list;
  user_pos_->GetPOSList(&pos_list);

  std::vector<UserPOS::Token> tokens, tokens_ja, tokens_en;
  EXPECT_TRUE(user_pos_->GetTokens("あか", "赤", "形容詞", "", &tokens));
  EXPECT_TRUE(user_pos_->GetTokens("あか", "赤", "形容詞", "ja", &tokens_ja));
  EXPECT_TRUE(user_pos_->GetTokens("あか", "赤", "形容詞", "en", &tokens_en));
  EXPECT_EQ(tokens.size(), tokens_ja.size());
  EXPECT_EQ(tokens.size(), tokens_en.size());

  for (size_t i = 0; i < tokens.size(); ++i) {
    EXPECT_EQ(tokens[i].cost, tokens_ja[i].cost);
    EXPECT_GT(tokens_en[i].cost, tokens_ja[i].cost);
    EXPECT_GT(tokens_en[i].cost, tokens[i].cost);
  }
}

TEST_F(UserPOSTest, ConjugationTest) {
  std::vector<UserPOS::Token> tokens1, tokens2;
  EXPECT_TRUE(user_pos_->GetTokens("わら", "嗤", "動詞ワ行五段", &tokens1));
  EXPECT_TRUE(user_pos_->GetTokens("わらう", "嗤う", "動詞ワ行五段", &tokens2));
  EXPECT_EQ(tokens1.size(), tokens2.size());
  for (size_t i = 0; i < tokens1.size(); ++i) {
    EXPECT_EQ(tokens1[i].key, tokens2[i].key);
    EXPECT_EQ(tokens1[i].value, tokens2[i].value);
    EXPECT_EQ(tokens1[i].id, tokens2[i].id);
    EXPECT_EQ(tokens1[i].cost, tokens2[i].cost);
  }

  EXPECT_TRUE(user_pos_->GetTokens("おそれ", "惧れ", "動詞一段", &tokens1));
  EXPECT_TRUE(user_pos_->GetTokens("おそれる", "惧れる", "動詞一段", &tokens2));
  EXPECT_EQ(tokens1.size(), tokens2.size());
  for (size_t i = 0; i < tokens1.size(); ++i) {
    EXPECT_EQ(tokens1[i].key, tokens2[i].key);
    EXPECT_EQ(tokens1[i].value, tokens2[i].value);
    EXPECT_EQ(tokens1[i].id, tokens2[i].id);
    EXPECT_EQ(tokens1[i].cost, tokens2[i].cost);
  }
}

}  // namespace
}  // namespace dictionary
}  // namespace mozc
