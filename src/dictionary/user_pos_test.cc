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

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/user_pos.h"
#include "testing/gunit.h"

namespace mozc {
namespace dictionary {

class UserPosTest : public ::testing::Test {
 protected:
  void SetUp() override {
    absl::string_view token_array_data, string_array_data;
    mock_data_manager_.GetUserPosData(&token_array_data, &string_array_data);
    user_pos_ = std::make_unique<UserPos>(token_array_data, string_array_data);
    CHECK(user_pos_.get());
  }

  std::unique_ptr<const UserPos> user_pos_;

 private:
  const testing::MockDataManager mock_data_manager_;
};

TEST_F(UserPosTest, GetPosListDefaultIndex) {
  const std::vector<std::string> pos_list = user_pos_->GetPosList();
  EXPECT_EQ(pos_list[user_pos_->GetPosListDefaultIndex()], "名詞");
}

TEST_F(UserPosTest, UserPosBasicTest) {
  const std::vector<std::string> pos_list = user_pos_->GetPosList();
  EXPECT_FALSE(pos_list.empty());
  // test contains
  EXPECT_TRUE(absl::c_contains(pos_list, "名詞サ変"));
  EXPECT_TRUE(absl::c_contains(pos_list, "サジェストのみ"));
  EXPECT_TRUE(absl::c_contains(pos_list, "短縮よみ"));
  EXPECT_TRUE(absl::c_contains(pos_list, "抑制単語"));
  EXPECT_TRUE(absl::c_contains(pos_list, "品詞なし"));
  for (size_t i = 0; i < pos_list.size(); ++i) {
    EXPECT_GT(user_pos_->GetPosIds(pos_list[i]).value(), 0);
  }

  EXPECT_FALSE(user_pos_->GetPosIds("__ERROR__").has_value());
}

TEST_F(UserPosTest, UserPosGetTokensTest) {
  const std::vector<std::string> pos_list = user_pos_->GetPosList();

  std::vector<UserPos::Token> tokens;
  EXPECT_TRUE(user_pos_->GetTokens("", "test", pos_list[0]).empty());
  EXPECT_TRUE(user_pos_->GetTokens("test", "", pos_list[0]).empty());
  EXPECT_TRUE(user_pos_->GetTokens("test", "test", "").empty());
  EXPECT_FALSE(user_pos_->GetTokens("test", "test", pos_list[0]).empty());

  // http://b/2674666
  EXPECT_FALSE(user_pos_->GetTokens("あか", "赤", "形容詞").empty());

  for (size_t i = 0; i < pos_list.size(); ++i) {
    EXPECT_FALSE(user_pos_->GetTokens("test", "test", pos_list[i]).empty());
  }
}

TEST_F(UserPosTest, UserPosGetTokensWithAttributesTest) {
  std::vector<UserPos::Token> tokens;

  tokens = user_pos_->GetTokens("test", "test", "サジェストのみ", "en");
  EXPECT_EQ(tokens.size(), 1);
  EXPECT_TRUE(tokens[0].has_attribute(UserPos::Token::NON_JA_LOCALE));

  tokens = user_pos_->GetTokens("test", "test", "短縮よみ", "en");
  EXPECT_EQ(tokens.size(), 1);
  EXPECT_TRUE(tokens[0].has_attribute(UserPos::Token::NON_JA_LOCALE));

  tokens = user_pos_->GetTokens("test", "test", "品詞なし", "en");
  EXPECT_EQ(tokens.size(), 1);
  EXPECT_TRUE(tokens[0].has_attribute(UserPos::Token::NON_JA_LOCALE));
}

TEST_F(UserPosTest, UserPosGetTokensWithLocaleTest) {
  auto tokens = user_pos_->GetTokens("あか", "赤", "形容詞", "");
  auto tokens_ja = user_pos_->GetTokens("あか", "赤", "形容詞", "ja");
  auto tokens_en = user_pos_->GetTokens("あか", "赤", "形容詞", "en");
  EXPECT_EQ(tokens.size(), tokens_ja.size());
  EXPECT_EQ(tokens.size(), tokens_en.size());

  for (size_t i = 0; i < tokens.size(); ++i) {
    EXPECT_EQ(tokens[i].attributes, tokens_ja[i].attributes);
    EXPECT_NE(tokens_en[i].attributes, tokens_ja[i].attributes);
    EXPECT_NE(tokens_en[i].attributes, tokens[i].attributes);
  }
}

TEST_F(UserPosTest, ConjugationTest) {
  auto tokens1 = user_pos_->GetTokens("わら", "嗤", "動詞ワ行五段");
  auto tokens2 = user_pos_->GetTokens("わらう", "嗤う", "動詞ワ行五段");
  EXPECT_FALSE(tokens1.empty());
  EXPECT_FALSE(tokens2.empty());
  EXPECT_EQ(tokens1.size(), tokens2.size());
  for (size_t i = 0; i < tokens1.size(); ++i) {
    EXPECT_EQ(tokens1[i].key, tokens2[i].key);
    EXPECT_EQ(tokens1[i].value, tokens2[i].value);
    EXPECT_EQ(tokens1[i].id, tokens2[i].id);
    EXPECT_EQ(tokens1[i].attributes, tokens2[i].attributes);
  }

  tokens1 = user_pos_->GetTokens("おそれ", "惧れ", "動詞一段");
  tokens2 = user_pos_->GetTokens("おそれる", "惧れる", "動詞一段");
  EXPECT_EQ(tokens1.size(), tokens2.size());
  for (size_t i = 0; i < tokens1.size(); ++i) {
    EXPECT_EQ(tokens1[i].key, tokens2[i].key);
    EXPECT_EQ(tokens1[i].value, tokens2[i].value);
    EXPECT_EQ(tokens1[i].id, tokens2[i].id);
    EXPECT_EQ(tokens1[i].attributes, tokens2[i].attributes);
  }
}

TEST_F(UserPosTest, SwapToken) {
  UserPos::Token token1 = {"key1", "value1", 1, 1, 1, "comment1"};
  UserPos::Token token2 = {"key2", "value2", 2, 2, 2, "comment2"};

  using std::swap;
  swap(token1, token2);

  EXPECT_EQ(token1.key, "key2");
  EXPECT_EQ(token1.value, "value2");
  EXPECT_EQ(token1.id, 2);
  EXPECT_EQ(token1.attributes, 2);
  EXPECT_EQ(token1.raw_pos_type, 2);
  EXPECT_EQ(token1.comment, "comment2");

  EXPECT_EQ(token2.key, "key1");
  EXPECT_EQ(token2.value, "value1");
  EXPECT_EQ(token2.id, 1);
  EXPECT_EQ(token2.attributes, 1);
  EXPECT_EQ(token2.raw_pos_type, 1);
  EXPECT_EQ(token2.comment, "comment1");
}

TEST_F(UserPosTest, Attributes) {
  UserPos::Token token;

  EXPECT_EQ(token.attributes, 0);
  token.add_attribute(UserPos::Token::NON_JA_LOCALE);
  EXPECT_TRUE(token.has_attribute(UserPos::Token::NON_JA_LOCALE));

  token.remove_attribute(UserPos::Token::NON_JA_LOCALE);
  EXPECT_FALSE(token.has_attribute(UserPos::Token::NON_JA_LOCALE));
}

TEST_F(UserPosTest, ToPosType) {
  EXPECT_EQ(UserPos::ToPosType("品詞なし"),
            user_dictionary::UserDictionary::NO_POS);
  EXPECT_EQ(UserPos::ToPosType("サジェストのみ"),
            user_dictionary::UserDictionary::SUGGESTION_ONLY);
  EXPECT_EQ(UserPos::ToPosType("動詞ワ行五段"),
            user_dictionary::UserDictionary::WA_GROUP1_VERB);
  EXPECT_EQ(UserPos::ToPosType("抑制単語"),
            user_dictionary::UserDictionary::SUPPRESSION_WORD);
}

TEST_F(UserPosTest, GetStringPosType) {
  EXPECT_EQ(UserPos::GetStringPosType(user_dictionary::UserDictionary::NO_POS),
            "品詞なし");
  EXPECT_EQ(UserPos::GetStringPosType(
                user_dictionary::UserDictionary::SUGGESTION_ONLY),
            "サジェストのみ");
  EXPECT_EQ(UserPos::GetStringPosType(
                user_dictionary::UserDictionary::WA_GROUP1_VERB),
            "動詞ワ行五段");
  EXPECT_EQ(UserPos::GetStringPosType(
                user_dictionary::UserDictionary::SUPPRESSION_WORD),
            "抑制単語");
}

TEST_F(UserPosTest, PosTypeRoundTrip) {
  const std::vector<std::string> pos_list = user_pos_->GetPosList();
  for (absl::string_view pos : pos_list) {
    const auto pos_type = UserPos::ToPosType(pos);
    EXPECT_EQ(UserPos::GetStringPosType(pos_type), pos);
  }
}
}  // namespace dictionary
}  // namespace mozc
