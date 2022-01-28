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

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
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

class UserPosTest : public ::testing::Test {
 protected:
  void SetUp() override {
    absl::string_view token_array_data, string_array_data;
    mock_data_manager_.GetUserPosData(&token_array_data, &string_array_data);
    user_pos_ = absl::make_unique<UserPos>(token_array_data, string_array_data);
    CHECK(user_pos_.get());
  }

  std::unique_ptr<const UserPosInterface> user_pos_;

 private:
  const testing::MockDataManager mock_data_manager_;
};

TEST_F(UserPosTest, UserPosBasicTest) {
  std::vector<std::string> pos_list;
  user_pos_->GetPosList(&pos_list);
  EXPECT_FALSE(pos_list.empty());

  uint16_t id = 0;
  for (size_t i = 0; i < pos_list.size(); ++i) {
    EXPECT_TRUE(user_pos_->IsValidPos(pos_list[i]));
    EXPECT_TRUE(user_pos_->GetPosIds(pos_list[i], &id));
    EXPECT_GT(id, 0);
  }
}

TEST_F(UserPosTest, UserPosGetTokensTest) {
  std::vector<std::string> pos_list;
  user_pos_->GetPosList(&pos_list);

  std::vector<UserPos::Token> tokens;
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

TEST_F(UserPosTest, UserPosGetTokensWithAttributesTest) {
  std::vector<UserPos::Token> tokens;

  EXPECT_TRUE(user_pos_->GetTokens("test", "test", "サジェストのみ", &tokens));
  EXPECT_EQ(tokens.size(), 1);
  EXPECT_TRUE(tokens[0].has_attribute(UserPos::Token::SUGGESTION_ONLY));

  EXPECT_TRUE(user_pos_->GetTokens("test", "test", "短縮よみ", &tokens));
  EXPECT_EQ(tokens.size(), 1);
  EXPECT_TRUE(tokens[0].has_attribute(UserPos::Token::ISOLATED_WORD));

  EXPECT_TRUE(
      user_pos_->GetTokens("test", "test", "サジェストのみ", "en", &tokens));
  EXPECT_EQ(tokens.size(), 1);
  EXPECT_TRUE(tokens[0].has_attribute(UserPos::Token::SUGGESTION_ONLY));
  EXPECT_TRUE(tokens[0].has_attribute(UserPos::Token::NON_JA_LOCALE));

  EXPECT_TRUE(user_pos_->GetTokens("test", "test", "短縮よみ", "en", &tokens));
  EXPECT_EQ(tokens.size(), 1);
  EXPECT_TRUE(tokens[0].has_attribute(UserPos::Token::ISOLATED_WORD));
  EXPECT_TRUE(tokens[0].has_attribute(UserPos::Token::NON_JA_LOCALE));
}

TEST_F(UserPosTest, UserPosGetTokensWithLocaleTest) {
  std::vector<std::string> pos_list;
  user_pos_->GetPosList(&pos_list);

  std::vector<UserPos::Token> tokens, tokens_ja, tokens_en;
  EXPECT_TRUE(user_pos_->GetTokens("あか", "赤", "形容詞", "", &tokens));
  EXPECT_TRUE(user_pos_->GetTokens("あか", "赤", "形容詞", "ja", &tokens_ja));
  EXPECT_TRUE(user_pos_->GetTokens("あか", "赤", "形容詞", "en", &tokens_en));
  EXPECT_EQ(tokens.size(), tokens_ja.size());
  EXPECT_EQ(tokens.size(), tokens_en.size());

  for (size_t i = 0; i < tokens.size(); ++i) {
    EXPECT_EQ(tokens[i].attributes, tokens_ja[i].attributes);
    EXPECT_NE(tokens_en[i].attributes, tokens_ja[i].attributes);
    EXPECT_NE(tokens_en[i].attributes, tokens[i].attributes);
  }
}

TEST_F(UserPosTest, ConjugationTest) {
  std::vector<UserPos::Token> tokens1, tokens2;
  EXPECT_TRUE(user_pos_->GetTokens("わら", "嗤", "動詞ワ行五段", &tokens1));
  EXPECT_TRUE(user_pos_->GetTokens("わらう", "嗤う", "動詞ワ行五段", &tokens2));
  EXPECT_EQ(tokens1.size(), tokens2.size());
  for (size_t i = 0; i < tokens1.size(); ++i) {
    EXPECT_EQ(tokens1[i].key, tokens2[i].key);
    EXPECT_EQ(tokens1[i].value, tokens2[i].value);
    EXPECT_EQ(tokens1[i].id, tokens2[i].id);
    EXPECT_EQ(tokens1[i].attributes, tokens2[i].attributes);
  }

  EXPECT_TRUE(user_pos_->GetTokens("おそれ", "惧れ", "動詞一段", &tokens1));
  EXPECT_TRUE(user_pos_->GetTokens("おそれる", "惧れる", "動詞一段", &tokens2));
  EXPECT_EQ(tokens1.size(), tokens2.size());
  for (size_t i = 0; i < tokens1.size(); ++i) {
    EXPECT_EQ(tokens1[i].key, tokens2[i].key);
    EXPECT_EQ(tokens1[i].value, tokens2[i].value);
    EXPECT_EQ(tokens1[i].id, tokens2[i].id);
    EXPECT_EQ(tokens1[i].attributes, tokens2[i].attributes);
  }
}

TEST_F(UserPosTest, SwapToken) {
  UserPos::Token token1 = {"key1", "value1", 1, 1, "comment1"};
  UserPos::Token token2 = {"key2", "value2", 2, 2, "comment2"};

  using std::swap;
  swap(token1, token2);

  EXPECT_EQ("key2", token1.key);
  EXPECT_EQ("value2", token1.value);
  EXPECT_EQ(2, token1.id);
  EXPECT_EQ(2, token1.attributes);
  EXPECT_EQ("comment2", token1.comment);

  EXPECT_EQ("key1", token2.key);
  EXPECT_EQ("value1", token2.value);
  EXPECT_EQ(1, token2.id);
  EXPECT_EQ(1, token2.attributes);
  EXPECT_EQ("comment1", token2.comment);
}

TEST_F(UserPosTest, Attributes) {
  UserPos::Token token;

  EXPECT_EQ(token.attributes, 0);
  token.add_attribute(UserPos::Token::SHORTCUT);
  token.add_attribute(UserPos::Token::SUGGESTION_ONLY);

  EXPECT_TRUE(token.has_attribute(UserPos::Token::SHORTCUT));
  EXPECT_TRUE(token.has_attribute(UserPos::Token::SUGGESTION_ONLY));
  EXPECT_FALSE(token.has_attribute(UserPos::Token::ISOLATED_WORD));

  token.remove_attribute(UserPos::Token::SUGGESTION_ONLY);
  EXPECT_TRUE(token.has_attribute(UserPos::Token::SHORTCUT));
  EXPECT_FALSE(token.has_attribute(UserPos::Token::SUGGESTION_ONLY));
  EXPECT_FALSE(token.has_attribute(UserPos::Token::ISOLATED_WORD));
}

}  // namespace
}  // namespace dictionary
}  // namespace mozc
