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

#include "rewriter/environmental_filter_rewriter.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/container/serialized_string_array.h"
#include "base/text_normalizer.h"
#include "base/util.h"
#include "converter/attribute.h"
#include "converter/candidate.h"
#include "converter/segments.h"
#include "data_manager/emoji_data.h"
#include "data_manager/testing/mock_data_manager.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace {
constexpr char kKanaSupplement_6_0[] = "\U0001B001";
constexpr char kKanaSupplement_10_0[] = "\U0001B002";
constexpr char kKanaExtendedA_14_0[] = "\U0001B122";

void AddSegment(const absl::string_view key, const absl::string_view value,
                Segments* segments) {
  segments->Clear();
  Segment* seg = segments->push_back_segment();
  seg->set_key(key);
  converter::Candidate* candidate = seg->add_candidate();
  candidate->value = std::string(value);
  candidate->content_value = std::string(value);
}

void AddSegment(const absl::string_view key,
                absl::Span<const std::string> values, Segments* segments) {
  Segment* seg = segments->add_segment();
  seg->set_key(key);
  for (const std::string& value : values) {
    converter::Candidate* candidate = seg->add_candidate();
    candidate->content_key = std::string(key);
    candidate->value = value;
    candidate->content_value = value;
  }
}

struct EmojiData {
  const absl::string_view emoji;
  const EmojiVersion unicode_version;
};

// Elements must be sorted lexicographically by key (first string).
constexpr EmojiData kTestEmojiList[] = {
    // Emoji 12.1 Example
    {"🧑‍✈", EmojiVersion::E12_1},   // 1F9D1 200D 2708
    {"🧑‍⚖", EmojiVersion::E12_1},   // 1F9D1 200D 2696
    {"🧑‍🏭", EmojiVersion::E12_1},  // 1F9D1 200D 1F527
    {"🧑‍💻", EmojiVersion::E12_1},  // 1F9D1 200D 1F4BB
    {"🧑‍🏫", EmojiVersion::E12_1},  // 1F9D1 200D 1F3EB
    {"🧑‍🌾", EmojiVersion::E12_1},  // 1F9D1 200D 1F33E
    {"🧑‍🦼", EmojiVersion::E12_1},  // 1F9D1 200D 1F9BC
    {"🧑‍🦽", EmojiVersion::E12_1},  // 1F9D1 200D 1F9BD

    // Emoji 13.0 Example
    {"🛻", EmojiVersion::E13_0},           // 1F6FB
    {"🛼", EmojiVersion::E13_0},           // 1F6FC
    {"🤵‍♀", EmojiVersion::E13_0},   // 1F935 200D 2640
    {"🤵‍♂", EmojiVersion::E13_0},   // 1F935 200D 2642
    {"🥲", EmojiVersion::E13_0},           // 1F972
    {"🥷", EmojiVersion::E13_0},           // 1F977
    {"🥸", EmojiVersion::E13_0},           // 1F978
    {"🧑‍🎄", EmojiVersion::E13_0},  // 1F9D1 200D 1F384

    // Emoji 14.0 Example
    {"🩻", EmojiVersion::E14_0},  // 1FA7B
    {"🩼", EmojiVersion::E14_0},  // 1FA7C
    {"🪩", EmojiVersion::E14_0},  // 1FAA9
    {"🪪", EmojiVersion::E14_0},  // 1FAAA
    {"🪫", EmojiVersion::E14_0},  // 1FAAB
    {"🪬", EmojiVersion::E14_0},  // 1FAAC
    {"🫃", EmojiVersion::E14_0},  // 1FAC3
    {"🫠", EmojiVersion::E14_0},  // 1FAE0

    // Emoji 16.0 Example
    {"🪏", EmojiVersion::E16_0},  // 1FA8F
    {"🫆", EmojiVersion::E16_0},  // 1FAC6
    {"🫟", EmojiVersion::E16_0},  // 1FADF

    // Emoji 17.0 Example
    {"🛘", EmojiVersion::E17_0},  // 1F6D8 LANDSLIDE
    {"🫍", EmojiVersion::E17_0},  // 1FACD ORCA
    {"🫯", EmojiVersion::E17_0},  // 1FAEF FIGHT_CLOUD
};

// This data manager overrides GetEmojiRewriterData() to return the above test
// data for EmojiRewriter.
class TestDataManager : public testing::MockDataManager {
 public:
  TestDataManager() {
    // Collect all the strings and temporarily assign 0 as index.
    absl::btree_map<std::string, size_t> string_index;
    for (const EmojiData& data : kTestEmojiList) {
      string_index[data.emoji] = 0;
    }

    // Set index.
    std::vector<absl::string_view> strings;
    size_t index = 0;
    for (auto& iter : string_index) {
      strings.push_back(iter.first);
      iter.second = index++;
    }

    // Create token array.
    for (const EmojiData& data : kTestEmojiList) {
      token_array_.push_back(0);
      token_array_.push_back(string_index[data.emoji]);
      token_array_.push_back(data.unicode_version);
      token_array_.push_back(0);
      token_array_.push_back(0);
      token_array_.push_back(0);
      token_array_.push_back(0);
    }

    // Create string array.
    string_array_data_ =
        SerializedStringArray::SerializeToBuffer(strings, &string_array_buf_);
  }

  void GetEmojiRewriterData(
      absl::string_view* token_array_data,
      absl::string_view* string_array_data) const override {
    *token_array_data =
        absl::string_view(reinterpret_cast<const char*>(token_array_.data()),
                          token_array_.size() * sizeof(uint32_t));
    *string_array_data = string_array_data_;
  }

 private:
  std::vector<uint32_t> token_array_;
  absl::string_view string_array_data_;
  std::unique_ptr<uint32_t[]> string_array_buf_;
};

class EnvironmentalFilterRewriterTest
    : public testing::TestWithTempUserProfile {
 protected:
  void SetUp() override {
    rewriter_ =
        std::make_unique<EnvironmentalFilterRewriter>(test_data_manager_);
  }

  std::unique_ptr<EnvironmentalFilterRewriter> rewriter_;

 private:
  const TestDataManager test_data_manager_;
};

TEST_F(EnvironmentalFilterRewriterTest, CharacterGroupFinderTest) {
  // Test for CharacterGroupFinder, with meaningless filtering target rather
  // than Emoji data. As Emoji sometimes contains un-displayed characters, this
  // test can be more explicit than using actual filtering target.
  {
    CharacterGroupFinder finder;
    finder.Initialize({
        U"\U0001B001",
        U"\U0001B002",
        U"\U0001B122",
        U"\U0001B223",
        U"\U0001B224",
        U"\U0001B225",
        U"\U0001B229",
        U"\U0001F000",
        U"\U0001F001",
        U"\U0001B111\u200D\U0001B183",
        U"\U0001B111\u200D\U0001B142\u200D\U0001B924",
        U"\U0001B111\u3009",
        U"\U0001B142\u200D\u3009\U0001B924",
        U"\U0001B924\u200D\U0001B183",
    });
    EXPECT_TRUE(finder.FindMatch(U"\U0001B001"));
    EXPECT_TRUE(finder.FindMatch(U"\U0001B002"));
    EXPECT_TRUE(finder.FindMatch(U"\U0001B223"));
    EXPECT_TRUE(
        finder.FindMatch(U"\U0001B111\u200D\U0001B142\u200D\U0001B924"));
    EXPECT_TRUE(finder.FindMatch(U"\U0001B111\u3009"));
    EXPECT_FALSE(finder.FindMatch(U"\U0001B111\u200D\U0001B182"));
  }
  // Test CharacterGroupFinder with Emoji data. This is also necessary to
  // express how this finder should work.
  {
    CharacterGroupFinder finder;
    finder.Initialize({
        U"❤",
        U"😊",
        U"😋",
        Util::Utf8ToUtf32("🇺🇸"),
        Util::Utf8ToUtf32("🫱🏻"),
        Util::Utf8ToUtf32("❤️‍🔥"),
        Util::Utf8ToUtf32("👬🏿"),
    });
    EXPECT_TRUE(finder.FindMatch(Util::Utf8ToUtf32("これは❤です")));
    EXPECT_TRUE(finder.FindMatch(Util::Utf8ToUtf32("これは🫱🏻です")));
    EXPECT_TRUE(finder.FindMatch(Util::Utf8ToUtf32("これは😊です")));
    EXPECT_TRUE(finder.FindMatch(Util::Utf8ToUtf32("これは😋です")));
    EXPECT_FALSE(
        finder.FindMatch(Util::Utf8ToUtf32("これは😌（U+1F60C）です")));
    EXPECT_TRUE(finder.FindMatch(Util::Utf8ToUtf32("😋これは最初です")));
    EXPECT_TRUE(finder.FindMatch(Util::Utf8ToUtf32("これは最後です😋")));
    EXPECT_FALSE(finder.FindMatch(Util::Utf8ToUtf32("これは🫱です")));
    EXPECT_TRUE(finder.FindMatch(Util::Utf8ToUtf32("これは👬🏿です")));
    EXPECT_TRUE(finder.FindMatch(Util::Utf8ToUtf32("👬🏿最初です")));
    EXPECT_TRUE(finder.FindMatch(Util::Utf8ToUtf32("❤️‍🔥")));
    EXPECT_TRUE(finder.FindMatch(Util::Utf8ToUtf32("最後です👬🏿")));
    EXPECT_TRUE(finder.FindMatch(Util::Utf8ToUtf32("👬👬🏿")));
    EXPECT_FALSE(finder.FindMatch(Util::Utf8ToUtf32("これは👬です")));
    // This is expecting to find 🇺🇸 (US). Because flag Emojis use regional
    // indicators, and they lack ZWJ between, ambiguity is inevitable. The input
    // is AUSE in regional indicators, and therefore US is found between the two
    // flags.
    EXPECT_TRUE(finder.FindMatch(Util::Utf8ToUtf32("🇦🇺🇸🇪")));
  }
  {
    // Test with more than 16 chars.
    CharacterGroupFinder finder;
    finder.Initialize({Util::Utf8ToUtf32("01234567890abcdefghij")});
    EXPECT_FALSE(finder.FindMatch(Util::Utf8ToUtf32("01234567890abcdefghXYZ")));
  }
  {
    // Test with empty finder.
    CharacterGroupFinder finder;
    finder.Initialize({});
    EXPECT_FALSE(finder.FindMatch(
        Util::Utf8ToUtf32("Empty finder should find nothing")));
  }
}

// This test aims to check the ability using EnvironmentalFilterRewriter to
// filter Emoji.
TEST_F(EnvironmentalFilterRewriterTest, EmojiFilterTest) {
  // Emoji after Unicode 12.1 should be all filtered if no additional renderable
  // character group is specified.
  {
    Segments segments;
    const ConversionRequest request;

    segments.Clear();
    AddSegment("a", {"🛻", "🤵‍♀", "🥸", "🧑‍🌾", "🧑‍🏭"},
               &segments);

    EXPECT_TRUE(rewriter_->Rewrite(request, &segments));
    EXPECT_EQ(segments.conversion_segment(0).candidates_size(), 0);
  }

  // Emoji in Unicode 13.0 should be allowed in this case.
  {
    commands::Request request;
    request.add_additional_renderable_character_groups(
        commands::Request::EMOJI_13_0);
    const ConversionRequest conversion_request =
        ConversionRequestBuilder().SetRequest(request).Build();
    Segments segments;

    segments.Clear();
    AddSegment("a", {"🛻", "🤵‍♀", "🥸"}, &segments);

    EXPECT_FALSE(rewriter_->Rewrite(conversion_request, &segments));
    EXPECT_EQ(segments.conversion_segment(0).candidates_size(), 3);
  }
}

TEST_F(EnvironmentalFilterRewriterTest, EmojiFilterE160Test) {
  // Emoji 16.0 characters are filtered by default.
  {
    Segments segments;
    const ConversionRequest request;

    segments.Clear();
    AddSegment("えもじ", {"🪏", "🫆", "🫟"}, &segments);

    EXPECT_TRUE(rewriter_->Rewrite(request, &segments));
    EXPECT_EQ(segments.conversion_segment(0).candidates_size(), 0);
  }

  // Emoji 16.0 characters are added when they are renderable.
  {
    commands::Request request;
    request.add_additional_renderable_character_groups(
        commands::Request::EMOJI_16_0);
    Segments segments;
    const ConversionRequest conversion_request =
        ConversionRequestBuilder().SetRequest(request).Build();

    segments.Clear();
    AddSegment("えもじ", {"🪏", "🫆", "🫟"}, &segments);

    EXPECT_FALSE(rewriter_->Rewrite(conversion_request, &segments));
    EXPECT_EQ(segments.conversion_segment(0).candidates_size(), 3);
  }
}

TEST_F(EnvironmentalFilterRewriterTest, RemoveTest) {
  Segments segments;
  const ConversionRequest request;

  segments.Clear();
  AddSegment("a", {"a\t1", "a\n2", "a\n\r3"}, &segments);

  EXPECT_TRUE(rewriter_->Rewrite(request, &segments));
  EXPECT_EQ(segments.conversion_segment(0).candidates_size(), 0);
}

TEST_F(EnvironmentalFilterRewriterTest, NoRemoveTest) {
  Segments segments;
  AddSegment("a", {"aa1", "a.a", "a-a"}, &segments);

  const ConversionRequest request;
  EXPECT_FALSE(rewriter_->Rewrite(request, &segments));
  EXPECT_EQ(segments.conversion_segment(0).candidates_size(), 3);
}

TEST_F(EnvironmentalFilterRewriterTest, CandidateFilterTest) {
  {
    const ConversionRequest conversion_request;

    Segments segments;
    segments.Clear();
    // All should not be allowed.
    AddSegment("a",
               {kKanaSupplement_6_0, kKanaSupplement_10_0, kKanaExtendedA_14_0},
               &segments);

    EXPECT_TRUE(rewriter_->Rewrite(conversion_request, &segments));
    EXPECT_EQ(segments.conversion_segment(0).candidates_size(), 0);
  }

  {
    const ConversionRequest conversion_request;

    Segments segments;
    segments.Clear();
    // The second candidate that comes from the user dictionary is not filtered.
    AddSegment("a",
               {kKanaSupplement_6_0, kKanaSupplement_10_0, kKanaExtendedA_14_0},
               &segments);
    EXPECT_EQ(segments.conversion_segment(0).candidates_size(), 3);
    segments.mutable_conversion_segment(0)->mutable_candidate(1)->attributes =
        converter::Attribute::USER_DICTIONARY;

    EXPECT_TRUE(rewriter_->Rewrite(conversion_request, &segments));
    EXPECT_EQ(segments.conversion_segment(0).candidates_size(), 1);
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value,
              kKanaSupplement_10_0);
  }

  {
    commands::Request request;
    request.add_additional_renderable_character_groups(
        commands::Request::EMPTY);
    const ConversionRequest conversion_request =
        ConversionRequestBuilder().SetRequest(request).Build();

    Segments segments;
    segments.Clear();
    // All should not be allowed.
    AddSegment("a",
               {kKanaSupplement_6_0, kKanaSupplement_10_0, kKanaExtendedA_14_0},
               &segments);

    EXPECT_TRUE(rewriter_->Rewrite(conversion_request, &segments));
    EXPECT_EQ(segments.conversion_segment(0).candidates_size(), 0);
  }

  {
    commands::Request request;
    request.add_additional_renderable_character_groups(
        commands::Request::KANA_SUPPLEMENT_6_0);
    const ConversionRequest conversion_request =
        ConversionRequestBuilder().SetRequest(request).Build();

    Segments segments;
    segments.Clear();
    // Only first one should be allowed.
    AddSegment("a",
               {kKanaSupplement_6_0, kKanaSupplement_10_0, kKanaExtendedA_14_0},
               &segments);

    EXPECT_TRUE(rewriter_->Rewrite(conversion_request, &segments));
    EXPECT_EQ(segments.conversion_segment(0).candidates_size(), 1);
  }

  {
    commands::Request request;
    request.add_additional_renderable_character_groups(
        commands::Request::KANA_SUPPLEMENT_6_0);
    request.add_additional_renderable_character_groups(
        commands::Request::KANA_SUPPLEMENT_AND_KANA_EXTENDED_A_10_0);
    const ConversionRequest conversion_request =
        ConversionRequestBuilder().SetRequest(request).Build();

    Segments segments;
    segments.Clear();
    // First and second one should be allowed.
    AddSegment("a",
               {kKanaSupplement_6_0, kKanaSupplement_10_0, kKanaExtendedA_14_0},
               &segments);

    EXPECT_TRUE(rewriter_->Rewrite(conversion_request, &segments));
    EXPECT_EQ(segments.conversion_segment(0).candidates_size(), 2);
  }

  {
    commands::Request request;
    request.add_additional_renderable_character_groups(
        commands::Request::KANA_SUPPLEMENT_6_0);
    request.add_additional_renderable_character_groups(
        commands::Request::KANA_SUPPLEMENT_AND_KANA_EXTENDED_A_10_0);
    request.add_additional_renderable_character_groups(
        commands::Request::KANA_EXTENDED_A_14_0);
    const ConversionRequest conversion_request =
        ConversionRequestBuilder().SetRequest(request).Build();

    Segments segments;
    segments.Clear();
    // All should be allowed.
    AddSegment("a",
               {kKanaSupplement_6_0, kKanaSupplement_10_0, kKanaExtendedA_14_0},
               &segments);

    EXPECT_FALSE(rewriter_->Rewrite(conversion_request, &segments));
    EXPECT_EQ(segments.conversion_segment(0).candidates_size(), 3);
  }
}

TEST_F(EnvironmentalFilterRewriterTest, NormalizationTest) {
  Segments segments;
  const ConversionRequest request;

  segments.Clear();
  AddSegment("test", "test", &segments);
  EXPECT_FALSE(rewriter_->Rewrite(request, &segments));
  EXPECT_EQ(segments.segment(0).candidate(0).value, "test");

  segments.Clear();
  AddSegment("きょうと", "京都", &segments);
  EXPECT_FALSE(rewriter_->Rewrite(request, &segments));
  EXPECT_EQ(segments.segment(0).candidate(0).value, "京都");

  // Wave dash (U+301C) per platform
  segments.Clear();
  AddSegment("なみ", "〜", &segments);
  constexpr char description[] = "[全]波ダッシュ";
  segments.mutable_segment(0)->mutable_candidate(0)->description = description;
#ifdef _WIN32
  EXPECT_TRUE(rewriter_->Rewrite(request, &segments));
  // U+FF5E
  EXPECT_EQ(segments.segment(0).candidate(0).value, "～");
  EXPECT_TRUE(segments.segment(0).candidate(0).description.empty());
#else   // _WIN32
  EXPECT_FALSE(rewriter_->Rewrite(request, &segments));
  // U+301C
  EXPECT_EQ(segments.segment(0).candidate(0).value, "〜");
  EXPECT_EQ(segments.segment(0).candidate(0).description, description);
#endif  // _WIN32

  // Wave dash (U+301C) w/ normalization
  segments.Clear();
  AddSegment("なみ", "〜", &segments);
  segments.mutable_segment(0)->mutable_candidate(0)->description = description;

  rewriter_->SetNormalizationFlag(TextNormalizer::kAll);
  EXPECT_TRUE(rewriter_->Rewrite(request, &segments));
  // U+FF5E
  EXPECT_EQ(segments.segment(0).candidate(0).value, "～");
  EXPECT_TRUE(segments.segment(0).candidate(0).description.empty());

  // Wave dash (U+301C) w/o normalization
  segments.Clear();
  AddSegment("なみ", "〜", &segments);
  segments.mutable_segment(0)->mutable_candidate(0)->description = description;

  rewriter_->SetNormalizationFlag(TextNormalizer::kNone);
  EXPECT_FALSE(rewriter_->Rewrite(request, &segments));
  // U+301C
  EXPECT_EQ(segments.segment(0).candidate(0).value, "〜");
  EXPECT_EQ(segments.segment(0).candidate(0).description, description);

  // not normalized.
  segments.Clear();
  // U+301C
  AddSegment("なみ", "〜", &segments);
  segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
      converter::Attribute::USER_DICTIONARY;
  EXPECT_FALSE(rewriter_->Rewrite(request, &segments));
  // U+301C
  EXPECT_EQ(segments.segment(0).candidate(0).value, "〜");

  // not normalized.
  segments.Clear();
  // U+301C
  AddSegment("なみ", "〜", &segments);
  segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
      converter::Attribute::NO_MODIFICATION;
  EXPECT_FALSE(rewriter_->Rewrite(request, &segments));
  // U+301C
  EXPECT_EQ(segments.segment(0).candidate(0).value, "〜");
}

}  // namespace
}  // namespace mozc
