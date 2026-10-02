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
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/strings/string_view.h"
#include "data_manager/testing/mock_data_manager.h"
#include "testing/gunit.h"

namespace mozc {
namespace dictionary {

class UserPosTest : public ::testing::Test {
 public:
  UserPosTest()
      : user_pos_(std::make_from_tuple<UserPos>(
            mock_data_manager_.GetUserPosData())) {}

 protected:
  const testing::MockDataManager mock_data_manager_;
  const UserPos user_pos_;
};

TEST_F(UserPosTest, UserPosBasicTest) {
  const std::vector<std::string> pos_list = user_pos_.GetPosList();
  EXPECT_FALSE(pos_list.empty());
  // test contains
  EXPECT_TRUE(absl::c_contains(pos_list, "名詞サ変"));
  EXPECT_TRUE(absl::c_contains(pos_list, "サジェストのみ"));
  EXPECT_TRUE(absl::c_contains(pos_list, "短縮よみ"));
  EXPECT_TRUE(absl::c_contains(pos_list, "抑制単語"));
  EXPECT_TRUE(absl::c_contains(pos_list, "品詞なし"));
  for (size_t i = 0; i < pos_list.size(); ++i) {
    EXPECT_GT(user_pos_.GetPosIds(pos_list[i]).value(), 0);
  }

  EXPECT_FALSE(user_pos_.GetPosIds("__ERROR__").has_value());
}

TEST_F(UserPosTest, UserPosGetTokensTest) {
  const std::vector<std::string> pos_list = user_pos_.GetPosList();

  const auto pos_type0 = UserPos::ToPosType(pos_list[0]);
  EXPECT_TRUE(user_pos_.GetTokens("", "test", pos_type0).empty());
  EXPECT_TRUE(user_pos_.GetTokens("test", "", pos_type0).empty());
  EXPECT_TRUE(
      user_pos_.GetTokens("test", "test", UserPos::ToPosType("")).empty());
  EXPECT_FALSE(user_pos_.GetTokens("test", "test", pos_type0).empty());

  // http://b/2674666
  EXPECT_FALSE(
      user_pos_
          .GetTokens("あか", "赤", user_dictionary::UserDictionary::ADJECTIVE)
          .empty());

  for (size_t i = 0; i < pos_list.size(); ++i) {
    EXPECT_FALSE(
        user_pos_.GetTokens("test", "test", UserPos::ToPosType(pos_list[i]))
            .empty());
  }
}

TEST_F(UserPosTest, ConjugationTest) {
  auto tokens1 = user_pos_.GetTokens(
      "わら", "嗤", user_dictionary::UserDictionary::WA_GROUP1_VERB);
  auto tokens2 = user_pos_.GetTokens(
      "わらう", "嗤う", user_dictionary::UserDictionary::WA_GROUP1_VERB);
  EXPECT_FALSE(tokens1.empty());
  EXPECT_FALSE(tokens2.empty());
  EXPECT_EQ(tokens1.size(), tokens2.size());
  for (size_t i = 0; i < tokens1.size(); ++i) {
    EXPECT_EQ(tokens1[i].key, tokens2[i].key);
    EXPECT_EQ(tokens1[i].value, tokens2[i].value);
    EXPECT_EQ(tokens1[i].id, tokens2[i].id);
  }

  tokens1 = user_pos_.GetTokens("おそれ", "惧れ",
                                user_dictionary::UserDictionary::GROUP2_VERB);
  tokens2 = user_pos_.GetTokens("おそれる", "惧れる",
                                user_dictionary::UserDictionary::GROUP2_VERB);
  EXPECT_EQ(tokens1.size(), tokens2.size());
  for (size_t i = 0; i < tokens1.size(); ++i) {
    EXPECT_EQ(tokens1[i].key, tokens2[i].key);
    EXPECT_EQ(tokens1[i].value, tokens2[i].value);
    EXPECT_EQ(tokens1[i].id, tokens2[i].id);
  }
}

TEST_F(UserPosTest, SwapToken) {
  UserPos::Token token1 = {"key1", "value1", 1, 1};
  UserPos::Token token2 = {"key2", "value2", 2, 2};

  using std::swap;
  swap(token1, token2);

  EXPECT_EQ(token1.key, "key2");
  EXPECT_EQ(token1.value, "value2");
  EXPECT_EQ(token1.id, 2);
  EXPECT_EQ(token1.raw_pos_type, 2);

  EXPECT_EQ(token2.key, "key1");
  EXPECT_EQ(token2.value, "value1");
  EXPECT_EQ(token2.id, 1);
  EXPECT_EQ(token2.raw_pos_type, 1);
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

TEST_F(UserPosTest, GetCostFromPosTypeWithPenalty) {
  // When cost is explicitly defined (> 0) in user_pos.def, cost + cost_penalty
  // is returned. NOUN cost is 2500. With penalty 346: 2500 + 346 = 2846.
  EXPECT_EQ(
      UserPos::GetCostFromPosType(user_dictionary::UserDictionary::NOUN, 346),
      2846);

  // When cost is 0 in user_pos.def (e.g. verbs/adjectives), default cost 5000
  // is returned without penalty.
  EXPECT_EQ(UserPos::GetCostFromPosType(
                user_dictionary::UserDictionary::WA_GROUP1_VERB, 346),
            5000);
}

TEST_F(UserPosTest, PosTypeRoundTrip) {
  const std::vector<std::string> pos_list = user_pos_.GetPosList();
  for (absl::string_view pos : pos_list) {
    const auto pos_type = UserPos::ToPosType(pos);
    EXPECT_EQ(UserPos::GetStringPosType(pos_type), pos);
  }
}
}  // namespace dictionary
}  // namespace mozc
