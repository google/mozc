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

#include "rewriter/dictionary_generator.h"

#include <memory>
#include <sstream>
#include <string>

#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/user_pos.h"
#include "testing/gunit.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace rewriter {
namespace {

TEST(TokenTest, Comparators) {
  Token token_empty;
  Token token_simple;
  token_simple.key = "mozc";
  token_simple.value = "MOZC";
  token_simple.pos = "noun";

  Token token_rich;
  token_rich.key = "mozc";
  token_rich.value = "MOZC";
  token_rich.pos = "noun";
  token_rich.sorting_key = 100;
  token_rich.description = "Google IME";

  EXPECT_NE(token_simple, token_empty);
  EXPECT_EQ(token_rich, token_simple);
}

TEST(TokenTest, MergeFrom) {
  Token token_to;
  token_to.key = "mozc";
  token_to.value = "MOZC";
  token_to.pos = "noun";

  Token token_from;
  token_from.value = "moooozc";
  // pos is not set.
  token_from.sorting_key = 100;
  token_from.description = "Google IME";

  token_to.MergeFrom(token_from);
  EXPECT_EQ(token_to.key, "mozc");
  EXPECT_EQ(token_to.value, "moooozc");
  EXPECT_EQ(token_to.pos, "noun");
  EXPECT_EQ(token_to.sorting_key, 100);
  EXPECT_EQ(token_to.description, "Google IME");

  // It is possible that the IDs are different.
  EXPECT_NE(token_to, token_from);
}

std::string MakeExpectedRaw(absl::string_view key, absl::string_view value,
                            absl::AlphaNum pos, absl::AlphaNum cost,
                            absl::string_view description,
                            absl::string_view additional_description) {
  return absl::StrJoin({key, pos.Piece(), pos.Piece(), cost.Piece(), value,
                        description, additional_description},
                       "\t");
}

TEST(DictionaryGeneratorTest, SmokeTest) {
  testing::MockDataManager data_manager;
  DictionaryGenerator generator(data_manager);
  generator.AddToken({0, "とうてん", "、", "句読点", "てん", "読点"});
  generator.AddToken({1, ",", "、", "句読点", "てん", "読点"});
  generator.AddToken({2, "、", "、", "句読点", "てん", "読点"});
  generator.AddToken({0, ",", "，", "句読点"});
  generator.AddToken({0, ",", "，", "句読点", "てん", "カンマ"});
  generator.AddToken({3, "[", "（", "括弧開", "始め丸括弧"});
  generator.AddToken({4, "]", "）", "括弧閉", "終わり丸括弧"});
  std::ostringstream os;
  generator.Output(os);
  dictionary::PosMatcher pos_matcher(data_manager.GetPosMatcherData());
  std::unique_ptr<dictionary::UserPos> user_pos =
      dictionary::UserPos::CreateFromDataManager(data_manager);
  uint16_t pos;
  user_pos->GetPosIds("句読点", &pos);
  const std::string expected = absl::StrCat(
      MakeExpectedRaw(",", "，", pos, 0, "てん", "カンマ"), "\n",
      MakeExpectedRaw(",", "、", pos, 10, "てん", "読点"), "\n",
      MakeExpectedRaw("[", "（", pos_matcher.GetOpenBracketId(), 0,
                      "始め丸括弧", ""),
      "\n",
      MakeExpectedRaw("]", "）", pos_matcher.GetCloseBracketId(), 0,
                      "終わり丸括弧", ""),
      "\n", MakeExpectedRaw("、", "、", pos, 0, "てん", "読点"), "\n",
      MakeExpectedRaw("とうてん", "、", pos, 0, "てん", "読点"), "\n");
  EXPECT_EQ(os.str(), expected);
}

}  // namespace
}  // namespace rewriter
}  // namespace mozc
