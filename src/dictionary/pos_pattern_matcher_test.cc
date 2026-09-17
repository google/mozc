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

#include "dictionary/pos_pattern_matcher.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/pos_id_map.h"
#include "testing/gunit.h"

namespace mozc::dictionary {
namespace {

class PosPatternMatcherTest : public ::testing::Test {
 protected:
  const testing::MockDataManager mock_data_manager_;
  const PosIdMap pos_id_map_{mock_data_manager_.GetPosIdMapData()};

  uint16_t FindPosId(absl::string_view pos_string) const {
    for (size_t id = 0; id < pos_id_map_.GetPosIdCount(); ++id) {
      if (pos_id_map_.GetPosString(static_cast<uint16_t>(id)) == pos_string) {
        return static_cast<uint16_t>(id);
      }
    }
    ADD_FAILURE() << "POS string not found in PosIdMap: " << pos_string;
    return 0;
  }
};

TEST_F(PosPatternMatcherTest, ExactMatchWithDollar) {
  const uint16_t to_id = FindPosId("助詞,並立助詞,*,*,*,*,と");
  const uint16_t toka_id = FindPosId("助詞,並立助詞,*,*,*,*,とか");
  const uint16_t te_id = FindPosId("助詞,接続助詞,*,*,*,*,て");

  // Pattern with trailing '$' strictly matches "と", but NOT "とか".
  const PosPatternMatcher exact_matcher("助詞,並立助詞,*,*,*,*,と$",
                                        pos_id_map_);
  EXPECT_FALSE(exact_matcher.Match(te_id));
  EXPECT_TRUE(exact_matcher.Match(to_id));     // "と" -> exact match
  EXPECT_FALSE(exact_matcher.Match(toka_id));  // "とか" -> excluded by '$'

  // Without '$', prefix matching matches both "と" and "とか".
  const PosPatternMatcher prefix_matcher("助詞,並立助詞,*,*,*,*,と",
                                         pos_id_map_);
  EXPECT_TRUE(prefix_matcher.Match(to_id));    // "と"
  EXPECT_TRUE(prefix_matcher.Match(toka_id));  // "とか"
  EXPECT_FALSE(prefix_matcher.Match(te_id));
}

TEST_F(PosPatternMatcherTest, PrefixCollisionWithTrailingComma) {
  const uint16_t fuku_joshi_id = FindPosId("助詞,副助詞,*,*,*,*,かも");
  const uint16_t fukushika_id = FindPosId("助詞,副詞化,*,*,*,*,と");

  // "助詞,副助詞," with trailing comma strictly matches "助詞,副助詞,...",
  // but does NOT match "助詞,副詞化,...".
  const PosPatternMatcher strict_matcher("助詞,副助詞,", pos_id_map_);
  EXPECT_TRUE(strict_matcher.Match(fuku_joshi_id));
  EXPECT_FALSE(strict_matcher.Match(fukushika_id));

  // Without trailing comma, "助詞,副" matches both because it is a prefix of
  // both.
  const PosPatternMatcher lenient_matcher("助詞,副", pos_id_map_);
  EXPECT_TRUE(lenient_matcher.Match(fuku_joshi_id));
  EXPECT_TRUE(lenient_matcher.Match(fukushika_id));
}

TEST_F(PosPatternMatcherTest, PrefixMatch) {
  const uint16_t general_noun_id = FindPosId("名詞,一般,*,*,*,*,*");
  const uint16_t proper_noun_id = FindPosId("名詞,固有名詞,人名,名,*,*,*");
  const uint16_t verb_id =
      FindPosId("動詞,自立,*,*,五段・カ行促音便,連用タ接続,行く");
  const uint16_t te_id = FindPosId("助詞,接続助詞,*,*,*,*,て");
  const uint16_t to_id = FindPosId("助詞,並立助詞,*,*,*,*,と");
  const uint16_t toka_id = FindPosId("助詞,並立助詞,*,*,*,*,とか");

  // Stopping at "名詞," matches any POS ID starting with "名詞,"
  const PosPatternMatcher matcher("名詞,", pos_id_map_);
  EXPECT_TRUE(matcher.Match(general_noun_id));
  EXPECT_TRUE(matcher.Match(proper_noun_id));
  EXPECT_FALSE(matcher.Match(verb_id));

  // Stopping at "助詞," matches any particle
  const PosPatternMatcher matcher2("助詞,", pos_id_map_);
  EXPECT_TRUE(matcher2.Match(te_id));
  EXPECT_TRUE(matcher2.Match(to_id));
  EXPECT_TRUE(matcher2.Match(toka_id));
  EXPECT_FALSE(matcher2.Match(general_noun_id));
}

TEST_F(PosPatternMatcherTest, SubCategoryNaturalPrefixMatch) {
  const uint16_t godan_id =
      FindPosId("動詞,自立,*,*,五段・カ行促音便,連用タ接続,行く");
  const uint16_t non_content_verb_id =
      FindPosId("動詞,非自立,*,*,五段・カ行促音便,仮定形,いく");
  const uint16_t adjective_id =
      FindPosId("形容詞,自立,*,*,形容詞・アウオ段,ガル接続,*");

  // "動詞,自立,*,*,五段" naturally prefixes
  // "動詞,自立,*,*,五段・カ行促音便,..."
  const PosPatternMatcher matcher("動詞,自立,*,*,五段", pos_id_map_);
  EXPECT_TRUE(matcher.Match(godan_id));
  EXPECT_FALSE(matcher.Match(non_content_verb_id));

  // List of patterns
  const PosPatternMatcher matcher2(
      {"動詞,自立,*,*,五段", "動詞,自立,*,*,一段", "形容詞,自立,"},
      pos_id_map_);
  EXPECT_TRUE(matcher2.Match(godan_id));
  EXPECT_FALSE(matcher2.Match(non_content_verb_id));
  EXPECT_TRUE(matcher2.Match(adjective_id));
}

TEST_F(PosPatternMatcherTest, PatternListAlternation) {
  const uint16_t general_noun_id = FindPosId("名詞,一般,*,*,*,*,*");
  const uint16_t proper_noun_id = FindPosId("名詞,固有名詞,人名,名,*,*,*");
  const uint16_t verb_id =
      FindPosId("動詞,自立,*,*,五段・カ行促音便,連用タ接続,行く");
  const uint16_t te_id = FindPosId("助詞,接続助詞,*,*,*,*,て");
  const uint16_t to_id = FindPosId("助詞,並立助詞,*,*,*,*,と");
  const uint16_t aux_id = FindPosId("助動詞,*,*,*,特殊・タ,基本形,た");

  // Match multiple patterns using initializer_list
  const PosPatternMatcher matcher({"名詞,固有名詞,人名,", "助詞,"},
                                  pos_id_map_);
  EXPECT_FALSE(matcher.Match(general_noun_id));
  EXPECT_TRUE(matcher.Match(proper_noun_id));
  EXPECT_FALSE(matcher.Match(verb_id));
  EXPECT_TRUE(matcher.Match(te_id));
  EXPECT_TRUE(matcher.Match(to_id));
  EXPECT_FALSE(matcher.Match(aux_id));

  // Another list of patterns
  const PosPatternMatcher matcher2({"名詞,一般,", "動詞,自立,", "助動詞,"},
                                   pos_id_map_);
  EXPECT_TRUE(matcher2.Match(general_noun_id));
  EXPECT_FALSE(matcher2.Match(proper_noun_id));
  EXPECT_TRUE(matcher2.Match(verb_id));
  EXPECT_TRUE(matcher2.Match(aux_id));
}

TEST_F(PosPatternMatcherTest, PatternSpanAlternation) {
  const uint16_t general_noun_id = FindPosId("名詞,一般,*,*,*,*,*");
  const uint16_t verb_id =
      FindPosId("動詞,自立,*,*,五段・カ行促音便,連用タ接続,行く");
  const uint16_t te_id = FindPosId("助詞,接続助詞,*,*,*,*,て");
  const uint16_t to_id = FindPosId("助詞,並立助詞,*,*,*,*,と");
  const uint16_t non_content_verb_id =
      FindPosId("動詞,非自立,*,*,五段・カ行促音便,仮定形,いく");

  const std::vector<absl::string_view> patterns = {
      "助詞,接続助詞,", "助詞,並立助詞,", "動詞,非自立,"};
  const PosPatternMatcher matcher(absl::MakeConstSpan(patterns), pos_id_map_);
  EXPECT_FALSE(matcher.Match(general_noun_id));
  EXPECT_TRUE(matcher.Match(te_id));
  EXPECT_TRUE(matcher.Match(to_id));
  EXPECT_TRUE(matcher.Match(non_content_verb_id));
  EXPECT_FALSE(matcher.Match(verb_id));
}

TEST_F(PosPatternMatcherTest, WhitespaceHandling) {
  const uint16_t te_id = FindPosId("助詞,接続助詞,*,*,*,*,て");
  const uint16_t to_id = FindPosId("助詞,並立助詞,*,*,*,*,と");
  const uint16_t general_noun_id = FindPosId("名詞,一般,*,*,*,*,*");

  // Whitespace around pattern is trimmed
  const PosPatternMatcher matcher({"  助詞,接続助詞,  ", " 助詞,並立助詞, "},
                                  pos_id_map_);
  EXPECT_TRUE(matcher.Match(te_id));
  EXPECT_TRUE(matcher.Match(to_id));
  EXPECT_FALSE(matcher.Match(general_noun_id));
}

TEST_F(PosPatternMatcherTest, MoveOperations) {
  const uint16_t general_noun_id = FindPosId("名詞,一般,*,*,*,*,*");
  const uint16_t verb_id =
      FindPosId("動詞,自立,*,*,五段・カ行促音便,連用タ接続,行く");

  PosPatternMatcher matcher1("名詞,一般,", pos_id_map_);
  EXPECT_TRUE(matcher1.Match(general_noun_id));

  // Move constructor
  PosPatternMatcher matcher2(std::move(matcher1));
  EXPECT_TRUE(matcher2.Match(general_noun_id));

  // Move assignment
  PosPatternMatcher matcher3("動詞,", pos_id_map_);
  EXPECT_TRUE(matcher3.Match(verb_id));
  matcher3 = std::move(matcher2);
  EXPECT_TRUE(matcher3.Match(general_noun_id));
  EXPECT_FALSE(matcher3.Match(verb_id));
}

TEST_F(PosPatternMatcherTest, OutOfRangeAndEdgeCases) {
  const PosPatternMatcher matcher("名詞,", pos_id_map_);
  EXPECT_TRUE(matcher.IsAvailable());
  EXPECT_FALSE(matcher.Match(pos_id_map_.GetPosIdCount() + 100));
  EXPECT_FALSE(matcher.Match(std::numeric_limits<uint16_t>::max()));

  // Empty pattern matches nothing and is not available.
  const PosPatternMatcher empty_pattern("", pos_id_map_);
  EXPECT_FALSE(empty_pattern.IsAvailable());
  EXPECT_FALSE(empty_pattern.Match(0));
  EXPECT_FALSE(empty_pattern.Match(1));

  // Empty pattern list matches nothing and is not available.
  const PosPatternMatcher empty_list({}, pos_id_map_);
  EXPECT_FALSE(empty_list.IsAvailable());
  EXPECT_FALSE(empty_list.Match(0));
  EXPECT_FALSE(empty_list.Match(1));

  // List with empty pattern matches nothing and is not available.
  const PosPatternMatcher list_with_empty({""}, pos_id_map_);
  EXPECT_FALSE(list_with_empty.IsAvailable());
  EXPECT_FALSE(list_with_empty.Match(0));
  EXPECT_FALSE(list_with_empty.Match(1));

  // Empty PosIdMap -> IsAvailable() returns false and Match() returns false
  // safely.
  const PosIdMap empty_map("");
  const PosPatternMatcher matcher_for_empty("名詞,", empty_map);
  EXPECT_FALSE(matcher_for_empty.IsAvailable());
  EXPECT_FALSE(matcher_for_empty.Match(0));
  EXPECT_FALSE(matcher_for_empty.Match(1));

  // Default constructed matcher is not available and Match() returns false
  // safely.
  const PosPatternMatcher default_matcher;
  EXPECT_FALSE(default_matcher.IsAvailable());
  EXPECT_FALSE(default_matcher.Match(0));
  EXPECT_FALSE(default_matcher.Match(1));
}

TEST_F(PosPatternMatcherTest, GetId) {
  // Pattern matching a specific unique SOS POS
  const PosPatternMatcher exact_matcher("BOS/EOS,*,*,*,*,*,*", pos_id_map_);
  EXPECT_TRUE(exact_matcher.GetId().has_value());
  const uint16_t bos_id = exact_matcher.GetId().value();
  EXPECT_TRUE(exact_matcher.Match(bos_id));
  EXPECT_EQ(bos_id, FindPosId("BOS/EOS,*,*,*,*,*,*"));

  // Broad pattern should return the first matching ID
  const PosPatternMatcher noun_matcher("名詞,", pos_id_map_);
  EXPECT_TRUE(noun_matcher.GetId().has_value());
  EXPECT_TRUE(noun_matcher.Match(noun_matcher.GetId().value()));
  EXPECT_TRUE(absl::StartsWith(
      pos_id_map_.GetPosString(noun_matcher.GetId().value()), "名詞,"));

  // Pattern with no matches
  const PosPatternMatcher no_match_matcher("存在しない品詞,", pos_id_map_);
  EXPECT_FALSE(no_match_matcher.GetId().has_value());

  // Default constructed
  const PosPatternMatcher default_matcher;
  EXPECT_FALSE(default_matcher.GetId().has_value());
}

TEST_F(PosPatternMatcherTest, ComprehensiveRuleMatching) {
  ASSERT_GT(pos_id_map_.GetPosIdCount(), 2000);

  // Functional rules as prefix pattern list:
  const PosPatternMatcher functional_matcher(
      {
          "助詞,",
          "助動詞,",
          "動詞,非自立,",
          "名詞,非自立,",
          "形容詞,非自立,",
          "動詞,接尾,",
          "名詞,接尾,",
          "形容詞,接尾,",
      },
      pos_id_map_);

  // FirstName: "名詞,固有名詞,人名,名,"
  const PosPatternMatcher first_name_matcher("名詞,固有名詞,人名,名,",
                                             pos_id_map_);

  // GeneralNoun with exact match '$': "名詞,一般,*,*,*,*,*$"
  const PosPatternMatcher general_noun_matcher("名詞,一般,*,*,*,*,*$",
                                               pos_id_map_);

  // ContentWordWithConjugation:
  const PosPatternMatcher content_word_matcher(
      {
          "動詞,自立,*,*,五段",
          "動詞,自立,*,*,一段",
          "形容詞,自立,",
      },
      pos_id_map_);

  // Exact particle "と": "助詞,並立助詞,*,*,*,*,と$"
  const PosPatternMatcher exact_to_matcher("助詞,並立助詞,*,*,*,*,と$",
                                           pos_id_map_);

  bool found_first_name = false;
  bool found_general_noun = false;
  bool found_functional = false;
  bool found_content_word = false;
  bool found_exact_to = false;
  bool checked_toka_excluded = false;

  for (size_t pos_id_idx = 0; pos_id_idx < pos_id_map_.GetPosIdCount();
       ++pos_id_idx) {
    const uint16_t pos_id = static_cast<uint16_t>(pos_id_idx);
    const absl::string_view pos_str = pos_id_map_.GetPosString(pos_id);
    if (absl::StartsWith(pos_str, "名詞,固有名詞,人名,名,")) {
      EXPECT_TRUE(first_name_matcher.Match(pos_id));
      found_first_name = true;
    }
    if (pos_str == "名詞,一般,*,*,*,*,*") {
      EXPECT_TRUE(general_noun_matcher.Match(pos_id));
      found_general_noun = true;
    }
    if (absl::StartsWith(pos_str, "助詞,") ||
        absl::StartsWith(pos_str, "助動詞,")) {
      EXPECT_TRUE(functional_matcher.Match(pos_id));
      found_functional = true;
    }
    if (absl::StartsWith(pos_str, "動詞,自立,*,*,五段・")) {
      EXPECT_TRUE(content_word_matcher.Match(pos_id));
      found_content_word = true;
    }
    if (pos_str == "助詞,並立助詞,*,*,*,*,と") {
      EXPECT_TRUE(exact_to_matcher.Match(pos_id));
      found_exact_to = true;
    }
    if (pos_str == "助詞,並立助詞,*,*,*,*,とか") {
      EXPECT_FALSE(exact_to_matcher.Match(pos_id));
      checked_toka_excluded = true;
    }
  }

  EXPECT_TRUE(found_first_name);
  EXPECT_TRUE(found_general_noun);
  EXPECT_TRUE(found_functional);
  EXPECT_TRUE(found_content_word);
  EXPECT_TRUE(found_exact_to);
  EXPECT_TRUE(checked_toka_excluded);
}

}  // namespace
}  // namespace mozc::dictionary
