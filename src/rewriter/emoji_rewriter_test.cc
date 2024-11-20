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

#include "rewriter/emoji_rewriter.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "base/container/serialized_string_array.h"
#include "config/config_handler.h"
#include "converter/segments.h"
#include "data_manager/emoji_data.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/pos_matcher.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"
#include "rewriter/variants_rewriter.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"
#include "usage_stats/usage_stats.h"
#include "usage_stats/usage_stats_testing_util.h"

namespace mozc {
namespace {

using mozc::commands::Request;

constexpr char kEmoji[] = "えもじ";

// Makes |segments| to have only a segment with a key-value paired candidate.
void SetSegment(const absl::string_view key, const absl::string_view value,
                Segments *segments) {
  segments->Clear();
  Segment *seg = segments->push_back_segment();
  seg->set_key(key);
  Segment::Candidate *candidate = seg->add_candidate();
  candidate->value = std::string(key);
  candidate->content_key = std::string(key);
  candidate->content_value = std::string(value);
}

// Counts the number of enumerated emoji candidates in the segments.
int CountEmojiCandidates(const Segments &segments) {
  int emoji_size = 0;
  for (const Segment &segment : segments) {
    for (size_t j = 0; j < segment.candidates_size(); ++j) {
      if (EmojiRewriter::IsEmojiCandidate(segment.candidate(j))) {
        ++emoji_size;
      }
    }
  }
  return emoji_size;
}

// Checks if the first segment has a specific candidate.
bool HasExpectedCandidate(const Segments &segments,
                          const absl::string_view expect_value) {
  CHECK_LE(1, segments.segments_size());
  const Segment &segment = segments.segment(0);
  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    const Segment::Candidate &candidate = segment.candidate(i);
    if (candidate.value == expect_value) {
      return true;
    }
  }
  return false;
}

// Replaces an emoji candidate into the 0-th index, as the Mozc converter
// does with a committed candidate.
void ChooseEmojiCandidate(Segments *segments) {
  CHECK_LE(1, segments->segments_size());
  Segment *segment = segments->mutable_segment(0);
  for (size_t i = 0; i < segment->candidates_size(); ++i) {
    if (EmojiRewriter::IsEmojiCandidate(segment->candidate(i))) {
      segment->move_candidate(i, 0);
      break;
    }
  }
  segment->set_segment_type(Segment::FIXED_VALUE);
}

struct EmojiData {
  const char *key;
  const char *unicode;
  const EmojiVersion unicode_version;
  const char *description_unicode;
  const char *unused_field_0;
  const char *unused_field_1;
  const char *unused_field_2;
};

// Elements must be sorted lexicographically by key (first string).
const EmojiData kTestEmojiList[] = {
    // An actual emoji character
    {"Emoji", "🐭", EmojiVersion::E0_6, "nezumi picture", "", "", ""},

    // Meta candidates.
    {"Inu", "DOG", EmojiVersion::E0_6, "inu", "", "", ""},
    {"Neko", "CAT", EmojiVersion::E0_6, "neko", "", "", ""},
    {"Nezumi", "MOUSE", EmojiVersion::E0_6, "nezumi", "", "", ""},
    {"Nezumi", "RAT", EmojiVersion::E0_6, "nezumi", "", "", ""},
    {"X", "COW", EmojiVersion::E0_6, "ushi", "", "", ""},
    {"X", "TIGER", EmojiVersion::E0_6, "tora", "", "", ""},
    {"X", "RABIT", EmojiVersion::E0_6, "usagi", "", "", ""},
    {"X", "DRAGON", EmojiVersion::E0_6, "ryu", "", "", ""},
};

// This data manager overrides GetEmojiRewriterData() to return the above test
// data for EmojiRewriter.
class TestDataManager : public testing::MockDataManager {
 public:
  TestDataManager() {
    // Collect all the strings and temporarily assign 0 as index.
    absl::btree_map<std::string, size_t> string_index;
    for (const EmojiData &data : kTestEmojiList) {
      string_index[data.key] = 0;
      string_index[data.unicode] = 0;
      string_index[data.description_unicode] = 0;
      string_index[data.unused_field_0] = 0;
      string_index[data.unused_field_1] = 0;
      string_index[data.unused_field_2] = 0;
    }

    // Set index.
    std::vector<absl::string_view> strings;
    size_t index = 0;
    for (auto &iter : string_index) {
      strings.push_back(iter.first);
      iter.second = index++;
    }

    // Create token array.
    for (const EmojiData &data : kTestEmojiList) {
      token_array_.push_back(string_index[data.key]);
      token_array_.push_back(string_index[data.unicode]);
      token_array_.push_back(data.unicode_version);
      token_array_.push_back(string_index[data.description_unicode]);
      token_array_.push_back(string_index[data.unused_field_0]);
      token_array_.push_back(string_index[data.unused_field_1]);
      token_array_.push_back(string_index[data.unused_field_2]);
    }

    // Create string array.
    string_array_data_ =
        SerializedStringArray::SerializeToBuffer(strings, &string_array_buf_);
  }

  void GetEmojiRewriterData(
      absl::string_view *token_array_data,
      absl::string_view *string_array_data) const override {
    *token_array_data =
        absl::string_view(reinterpret_cast<const char *>(token_array_.data()),
                          token_array_.size() * sizeof(uint32_t));
    *string_array_data = string_array_data_;
  }

 private:
  std::vector<uint32_t> token_array_;
  absl::string_view string_array_data_;
  std::unique_ptr<uint32_t[]> string_array_buf_;
};

class EmojiRewriterTest : public testing::TestWithTempUserProfile {
 protected:
  void SetUp() override {
    // Enable emoji conversion
    config::ConfigHandler::GetDefaultConfig(&config_);
    config_.set_use_emoji_conversion(true);

    mozc::usage_stats::UsageStats::ClearAllStatsForTest();

    rewriter_ = std::make_unique<EmojiRewriter>(test_data_manager_);
    full_data_rewriter_ = std::make_unique<EmojiRewriter>(mock_data_manager_);
  }

  void TearDown() override {
    mozc::usage_stats::UsageStats::ClearAllStatsForTest();
  }

  static ConversionRequest ConvReq(const config::Config &config,
                                   const commands::Request &request) {
    return ConversionRequestBuilder()
        .SetConfig(config)
        .SetRequest(request)
        .Build();
  }

  commands::Request request_;
  config::Config config_;
  std::unique_ptr<EmojiRewriter> rewriter_;
  std::unique_ptr<EmojiRewriter> full_data_rewriter_;

 private:
  const testing::MockDataManager mock_data_manager_;
  const TestDataManager test_data_manager_;
  usage_stats::scoped_usage_stats_enabler usage_stats_enabler_;
};

TEST_F(EmojiRewriterTest, Capability) {
  request_.set_emoji_rewriter_capability(Request::NOT_AVAILABLE);
  const ConversionRequest convreq1 = ConvReq(config_, request_);
  EXPECT_EQ(rewriter_->capability(convreq1), RewriterInterface::NOT_AVAILABLE);

  request_.set_emoji_rewriter_capability(Request::CONVERSION);
  const ConversionRequest convreq2 = ConvReq(config_, request_);
  EXPECT_EQ(rewriter_->capability(convreq2), RewriterInterface::CONVERSION);

  request_.set_emoji_rewriter_capability(Request::PREDICTION);
  const ConversionRequest convreq3 = ConvReq(config_, request_);
  EXPECT_EQ(rewriter_->capability(convreq3), RewriterInterface::PREDICTION);

  request_.set_emoji_rewriter_capability(Request::SUGGESTION);
  const ConversionRequest convreq4 = ConvReq(config_, request_);
  EXPECT_EQ(rewriter_->capability(convreq4), RewriterInterface::SUGGESTION);

  request_.set_emoji_rewriter_capability(Request::ALL);
  const ConversionRequest convreq5 = ConvReq(config_, request_);
  EXPECT_EQ(rewriter_->capability(convreq5), RewriterInterface::ALL);
}

TEST_F(EmojiRewriterTest, ConvertedSegmentsHasEmoji) {
  // This test runs an EmojiRewriter with a few strings, and checks
  //   - no conversion occur with unknown string,
  //   - expected characters are added in a conversion with a string,
  //   - all emojis are added with a specific string.

  Segments segments;
  SetSegment("neko", "test", &segments);
  const ConversionRequest convreq = ConvReq(config_, request_);
  EXPECT_FALSE(rewriter_->Rewrite(convreq, &segments));
  EXPECT_EQ(CountEmojiCandidates(segments), 0);

  SetSegment("Neko", "test", &segments);
  EXPECT_TRUE(rewriter_->Rewrite(convreq, &segments));
  EXPECT_EQ(CountEmojiCandidates(segments), 1);
  EXPECT_TRUE(HasExpectedCandidate(segments, "CAT"));

  SetSegment("Nezumi", "test", &segments);
  EXPECT_TRUE(rewriter_->Rewrite(convreq, &segments));
  EXPECT_EQ(CountEmojiCandidates(segments), 2);
  EXPECT_TRUE(HasExpectedCandidate(segments, "MOUSE"));
  EXPECT_TRUE(HasExpectedCandidate(segments, "RAT"));

  SetSegment(kEmoji, "test", &segments);
  EXPECT_TRUE(rewriter_->Rewrite(convreq, &segments));
  EXPECT_EQ(CountEmojiCandidates(segments), 9);
}

TEST_F(EmojiRewriterTest, NoConversionWithDisabledSettings) {
  // This test checks no emoji conversion occur if emoji conversion is disabled
  // in settings. Same segments are tested with ConvertedSegmentsHasEmoji test.

  // Disable emoji conversion in settings
  config_.set_use_emoji_conversion(false);

  Segments segments;
  SetSegment("test", "test", &segments);
  const ConversionRequest convreq = ConvReq(config_, request_);
  EXPECT_FALSE(rewriter_->Rewrite(convreq, &segments));
  EXPECT_EQ(CountEmojiCandidates(segments), 0);

  SetSegment("Neko", "test", &segments);
  EXPECT_FALSE(rewriter_->Rewrite(convreq, &segments));
  EXPECT_EQ(CountEmojiCandidates(segments), 0);
  EXPECT_FALSE(HasExpectedCandidate(segments, "CAT"));

  SetSegment("Nezumi", "test", &segments);
  EXPECT_FALSE(rewriter_->Rewrite(convreq, &segments));
  EXPECT_EQ(CountEmojiCandidates(segments), 0);
  EXPECT_FALSE(HasExpectedCandidate(segments, "MOUSE"));
  EXPECT_FALSE(HasExpectedCandidate(segments, "RAT"));

  SetSegment(kEmoji, "test", &segments);
  EXPECT_FALSE(rewriter_->Rewrite(convreq, &segments));
  EXPECT_EQ(CountEmojiCandidates(segments), 0);
}

TEST_F(EmojiRewriterTest, CheckDescription) {
  const testing::MockDataManager data_manager;
  Segments segments;
  VariantsRewriter variants_rewriter(
      dictionary::PosMatcher(data_manager.GetPosMatcherData()));

  SetSegment("Emoji", "test", &segments);
  const ConversionRequest convreq = ConvReq(config_, request_);
  EXPECT_TRUE(rewriter_->Rewrite(convreq, &segments));
  EXPECT_TRUE(variants_rewriter.Rewrite(convreq, &segments));
  ASSERT_LT(0, CountEmojiCandidates(segments));
  const Segment &segment = segments.segment(0);
  for (int i = 0; i < segment.candidates_size(); ++i) {
    const Segment::Candidate &candidate = segment.candidate(i);
    const std::string &description = candidate.description;
    // Skip non emoji candidates.
    if (!EmojiRewriter::IsEmojiCandidate(candidate)) {
      continue;
    }
    EXPECT_EQ(description.find("[全]"), std::string::npos)
        << "for \"" << candidate.value << "\" : \"" << description << "\"";
  }
}

TEST_F(EmojiRewriterTest, CheckInsertPosition) {
  // This test checks if emoji candidates are inserted into the expected
  // position.

  // |kExpectPosition| has the same number with |kDefaultInsertPos| defined in
  // emoji_rewriter.cc.
  constexpr int kExpectPosition = 6;

  Segments segments;
  {
    Segment *segment = segments.push_back_segment();
    segment->set_key("Neko");
    for (int i = 0; i < kExpectPosition * 2; ++i) {
      std::string value = "candidate" + std::to_string(i);
      Segment::Candidate *candidate = segment->add_candidate();
      candidate->value = value;
      candidate->content_key = "Neko";
      candidate->content_value = value;
    }
  }
  const ConversionRequest convreq = ConvReq(config_, request_);
  EXPECT_TRUE(rewriter_->Rewrite(convreq, &segments));

  ASSERT_EQ(segments.segments_size(), 1);
  const Segment &segment = segments.segment(0);
  ASSERT_LE(kExpectPosition, segment.candidates_size());
  for (int i = 0; i < kExpectPosition; ++i) {
    EXPECT_FALSE(EmojiRewriter::IsEmojiCandidate(segment.candidate(i)));
  }
  const Segment::Candidate &candidate = segment.candidate(kExpectPosition);
  EXPECT_TRUE(EmojiRewriter::IsEmojiCandidate(candidate));
  EXPECT_EQ(candidate.value, "CAT");
}

TEST_F(EmojiRewriterTest, CheckUsageStats) {
  // This test checks the data stored in usage stats for EmojiRewriter.

  constexpr char kStatsKey[] = "CommitEmoji";
  Segments segments;

  // No use, no registered keys
  EXPECT_STATS_NOT_EXIST(kStatsKey);

  // Converting non-emoji candidates does not matter.
  SetSegment("test", "test", &segments);
  const ConversionRequest convreq = ConvReq(config_, request_);
  EXPECT_FALSE(rewriter_->Rewrite(convreq, &segments));
  rewriter_->Finish(convreq, &segments);
  EXPECT_STATS_NOT_EXIST(kStatsKey);

  // Converting an emoji candidate.
  SetSegment("Nezumi", "test", &segments);
  EXPECT_TRUE(rewriter_->Rewrite(convreq, &segments));
  ChooseEmojiCandidate(&segments);
  rewriter_->Finish(convreq, &segments);
  EXPECT_COUNT_STATS(kStatsKey, 1);
  SetSegment(kEmoji, "test", &segments);
  EXPECT_TRUE(rewriter_->Rewrite(convreq, &segments));
  ChooseEmojiCandidate(&segments);
  rewriter_->Finish(convreq, &segments);
  EXPECT_COUNT_STATS(kStatsKey, 2);

  // Converting non-emoji keeps the previous usage stats.
  SetSegment("test", "test", &segments);
  EXPECT_FALSE(rewriter_->Rewrite(convreq, &segments));
  rewriter_->Finish(convreq, &segments);
  EXPECT_COUNT_STATS(kStatsKey, 2);
}

TEST_F(EmojiRewriterTest, QueryNormalization) {
  {
    Segments segments;
    SetSegment("Ｎｅｋｏ", "Neko", &segments);
    const ConversionRequest convreq = ConvReq(config_, request_);
    EXPECT_TRUE(rewriter_->Rewrite(convreq, &segments));
  }
  {
    Segments segments;
    SetSegment("Neko", "Neko", &segments);
    const ConversionRequest convreq = ConvReq(config_, request_);
    EXPECT_TRUE(rewriter_->Rewrite(convreq, &segments));
  }
}

TEST_F(EmojiRewriterTest, FullDataTest) {
  // U+1F646 (FACE WITH OK GESTURE)
  {
    Segments segments;
    SetSegment("ＯＫ", "OK", &segments);
    const ConversionRequest convreq = ConvReq(config_, request_);
    EXPECT_TRUE(full_data_rewriter_->Rewrite(convreq, &segments));
  }
  {
    Segments segments;
    SetSegment("OK", "OK", &segments);
    const ConversionRequest convreq = ConvReq(config_, request_);
    EXPECT_TRUE(full_data_rewriter_->Rewrite(convreq, &segments));
  }
  // U+2795 (HEAVY PLUS SIGN)
  {
    Segments segments;
    SetSegment("＋", "+", &segments);
    const ConversionRequest convreq = ConvReq(config_, request_);
    EXPECT_TRUE(full_data_rewriter_->Rewrite(convreq, &segments));
  }
  {
    Segments segments;
    SetSegment("+", "+", &segments);
    const ConversionRequest convreq = ConvReq(config_, request_);
    EXPECT_TRUE(full_data_rewriter_->Rewrite(convreq, &segments));
  }
  // U+1F522 (INPUT SYMBOL FOR NUMBERS)
  {
    Segments segments;
    SetSegment("１２３４", "1234", &segments);
    const ConversionRequest convreq = ConvReq(config_, request_);
    EXPECT_TRUE(full_data_rewriter_->Rewrite(convreq, &segments));
  }
  {
    Segments segments;
    SetSegment("1234", "1234", &segments);
    const ConversionRequest convreq = ConvReq(config_, request_);
    EXPECT_TRUE(full_data_rewriter_->Rewrite(convreq, &segments));
  }
  // U+1F552 (CLOCK FACE THREE OCLOCK)
  {
    Segments segments;
    SetSegment("３じ", "3ji", &segments);
    const ConversionRequest convreq = ConvReq(config_, request_);
    EXPECT_TRUE(full_data_rewriter_->Rewrite(convreq, &segments));
  }
  {
    Segments segments;
    SetSegment("3じ", "3ji", &segments);
    const ConversionRequest convreq = ConvReq(config_, request_);
    EXPECT_TRUE(full_data_rewriter_->Rewrite(convreq, &segments));
  }
  // U+31 U+FE0F U+20E3 (KEYCAP 1)
  {
    Segments segments;
    SetSegment("１", "1", &segments);
    const ConversionRequest convreq = ConvReq(config_, request_);
    EXPECT_TRUE(full_data_rewriter_->Rewrite(convreq, &segments));
  }
  {
    Segments segments;
    SetSegment("1", "1", &segments);
    const ConversionRequest convreq = ConvReq(config_, request_);
    EXPECT_TRUE(full_data_rewriter_->Rewrite(convreq, &segments));
  }
}

}  // namespace
}  // namespace mozc
