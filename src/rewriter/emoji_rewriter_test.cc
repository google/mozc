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
#include <memory>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/serialized_string_array.h"
#include "base/util.h"
#include "config/config_handler.h"
#include "converter/segments.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/pos_matcher.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "rewriter/variants_rewriter.h"
#include "testing/base/public/gunit.h"
#include "testing/base/public/mozctest.h"
#include "usage_stats/usage_stats.h"
#include "usage_stats/usage_stats_testing_util.h"
#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace {

using mozc::commands::Request;

const char kEmoji[] = "えもじ";

// Makes |segments| to have only a segment with a key-value paired candidate.
void SetSegment(const std::string &key, const std::string &value,
                Segments *segments) {
  segments->Clear();
  Segment *seg = segments->push_back_segment();
  seg->set_key(key);
  Segment::Candidate *candidate = seg->add_candidate();
  candidate->Init();
  candidate->value = key;
  candidate->content_key = key;
  candidate->content_value = value;
}

// Counts the number of enumerated emoji candidates in the segments.
int CountEmojiCandidates(const Segments &segments) {
  int emoji_size = 0;
  for (size_t i = 0; i < segments.segments_size(); ++i) {
    const Segment &segment = segments.segment(i);
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
                          const std::string &expect_value) {
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
  const uint32 android_pua;
  const char *description_unicode;
  const char *description_docomo;
  const char *description_softbank;
  const char *description_kddi;
};

// Elements must be sorted lexicographically by key (first string).
const EmojiData kTestEmojiList[] = {
    // An actual emoji character
    {"Emoji", "🐭", 0, "nezumi picture", "", "", ""},

    // Meta candidates.
    {"Inu", "DOG", 0, "inu", "", "", ""},
    {"Neko", "CAT", 0, "neko", "", "", ""},
    {"Nezumi", "MOUSE", 0, "nezumi", "", "", ""},
    {"Nezumi", "RAT", 0, "nezumi", "", "", ""},

    // Test data for carrier.
    {"X", "COW", 0xFE001, "ushi", "", "", ""},
    {"X", "TIGER", 0xFE002, "tora", "docomo", "", ""},
    {"X", "RABIT", 0xFE003, "usagi", "", "softbank", ""},
    {"X", "DRAGON", 0xFE004, "ryu", "", "", "kddi"},

    // No unicode available.
    {"X", "", 0xFE011, "", "docomo", "", ""},
    {"X", "", 0xFE012, "", "", "softbank", ""},
    {"X", "", 0xFE013, "", "", "", "kddi"},

    // Multiple carriers available.
    {"X", "", 0xFE021, "", "docomo", "softbank", ""},
    {"X", "", 0xFE022, "", "docomo", "", "kddi"},
    {"X", "", 0xFE023, "", "", "softbank", "kddi"},
    {"X", "", 0xFE024, "", "docomo", "softbank", "kddi"},
};

// This data manager overrides GetEmojiRewriterData() to return the above test
// data for EmojiRewriter.
class TestDataManager : public testing::MockDataManager {
 public:
  TestDataManager() {
    // Collect all the strings and temporarily assign 0 as index.
    std::map<std::string, size_t> string_index;
    for (const EmojiData &data : kTestEmojiList) {
      string_index[data.key] = 0;
      string_index[data.unicode] = 0;
      string_index[data.description_unicode] = 0;
      string_index[data.description_docomo] = 0;
      string_index[data.description_softbank] = 0;
      string_index[data.description_kddi] = 0;
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
      token_array_.push_back(data.android_pua);
      token_array_.push_back(string_index[data.description_unicode]);
      token_array_.push_back(string_index[data.description_docomo]);
      token_array_.push_back(string_index[data.description_softbank]);
      token_array_.push_back(string_index[data.description_kddi]);
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
                          token_array_.size() * sizeof(uint32));
    *string_array_data = string_array_data_;
  }

 private:
  std::vector<uint32> token_array_;
  absl::string_view string_array_data_;
  std::unique_ptr<uint32[]> string_array_buf_;
};

class EmojiRewriterTest : public ::testing::Test {
 protected:
  EmojiRewriterTest() {
    convreq_.set_request(&request_);
    convreq_.set_config(&config_);
  }

  void SetUp() override {
    // Enable emoji conversion
    config::ConfigHandler::GetDefaultConfig(&config_);
    config_.set_use_emoji_conversion(true);

    mozc::usage_stats::UsageStats::ClearAllStatsForTest();

    rewriter_ = absl::make_unique<EmojiRewriter>(test_data_manager_);
    full_data_rewriter_ = absl::make_unique<EmojiRewriter>(mock_data_manager_);
  }

  void TearDown() override {
    mozc::usage_stats::UsageStats::ClearAllStatsForTest();
  }

  ConversionRequest convreq_;
  commands::Request request_;
  config::Config config_;
  std::unique_ptr<EmojiRewriter> rewriter_;
  std::unique_ptr<EmojiRewriter> full_data_rewriter_;

 private:
  const testing::ScopedTmpUserProfileDirectory scoped_tmp_profile_dir_;
  const testing::MockDataManager mock_data_manager_;
  const TestDataManager test_data_manager_;
  usage_stats::scoped_usage_stats_enabler usage_stats_enabler_;
};

TEST_F(EmojiRewriterTest, Capability) {
  request_.set_emoji_rewriter_capability(Request::NOT_AVAILABLE);
  EXPECT_EQ(RewriterInterface::NOT_AVAILABLE, rewriter_->capability(convreq_));

  request_.set_emoji_rewriter_capability(Request::CONVERSION);
  EXPECT_EQ(RewriterInterface::CONVERSION, rewriter_->capability(convreq_));

  request_.set_emoji_rewriter_capability(Request::PREDICTION);
  EXPECT_EQ(RewriterInterface::PREDICTION, rewriter_->capability(convreq_));

  request_.set_emoji_rewriter_capability(Request::SUGGESTION);
  EXPECT_EQ(RewriterInterface::SUGGESTION, rewriter_->capability(convreq_));

  request_.set_emoji_rewriter_capability(Request::ALL);
  EXPECT_EQ(RewriterInterface::ALL, rewriter_->capability(convreq_));
}

TEST_F(EmojiRewriterTest, ConvertedSegmentsHasEmoji) {
  // This test runs an EmojiRewriter with a few strings, and checks
  //   - no conversion occur with unknown string,
  //   - expected characters are added in a conversion with a string,
  //   - all emojis are added with a specific string.

  Segments segments;
  SetSegment("neko", "test", &segments);
  EXPECT_FALSE(rewriter_->Rewrite(convreq_, &segments));
  EXPECT_EQ(0, CountEmojiCandidates(segments));

  SetSegment("Neko", "test", &segments);
  EXPECT_TRUE(rewriter_->Rewrite(convreq_, &segments));
  EXPECT_EQ(1, CountEmojiCandidates(segments));
  EXPECT_TRUE(HasExpectedCandidate(segments, "CAT"));

  SetSegment("Nezumi", "test", &segments);
  EXPECT_TRUE(rewriter_->Rewrite(convreq_, &segments));
  EXPECT_EQ(2, CountEmojiCandidates(segments));
  EXPECT_TRUE(HasExpectedCandidate(segments, "MOUSE"));
  EXPECT_TRUE(HasExpectedCandidate(segments, "RAT"));

  SetSegment(kEmoji, "test", &segments);
  EXPECT_TRUE(rewriter_->Rewrite(convreq_, &segments));
  EXPECT_EQ(9, CountEmojiCandidates(segments));
}

TEST_F(EmojiRewriterTest, CarrierEmojiSelectionEmpty) {
  Segments segments;
  SetSegment("X", "test", &segments);
  request_.set_available_emoji_carrier(0);  // Disable emoji rewriting.
  ASSERT_FALSE(rewriter_->Rewrite(convreq_, &segments));
  ASSERT_EQ(0, CountEmojiCandidates(segments));
}

TEST_F(EmojiRewriterTest, CarrierEmojiSelectionUnicode) {
  Segments segments;
  SetSegment("X", "test", &segments);
  request_.set_available_emoji_carrier(Request::UNICODE_EMOJI);
  ASSERT_TRUE(rewriter_->Rewrite(convreq_, &segments));
  ASSERT_EQ(4, CountEmojiCandidates(segments));
  EXPECT_TRUE(HasExpectedCandidate(segments, "COW"));
  EXPECT_TRUE(HasExpectedCandidate(segments, "TIGER"));
  EXPECT_TRUE(HasExpectedCandidate(segments, "RABIT"));
  EXPECT_TRUE(HasExpectedCandidate(segments, "DRAGON"));
}

// TODO(b/135127317): Delete carrier related tests once we delete PUA emoji
// related codes.

TEST_F(EmojiRewriterTest, CarrierEmojiSelectionDocomo) {
  Segments segments;
  SetSegment("X", "test", &segments);
  request_.set_available_emoji_carrier(Request::DOCOMO_EMOJI);
  ASSERT_FALSE(rewriter_->Rewrite(convreq_, &segments));
}

TEST_F(EmojiRewriterTest, CarrierEmojiSelectionSoftbank) {
  Segments segments;
  SetSegment("X", "test", &segments);
  request_.set_available_emoji_carrier(Request::SOFTBANK_EMOJI);
  ASSERT_FALSE(rewriter_->Rewrite(convreq_, &segments));
}

TEST_F(EmojiRewriterTest, CarrierEmojiSelectionKddi) {
  Segments segments;
  SetSegment("X", "test", &segments);
  request_.set_available_emoji_carrier(Request::KDDI_EMOJI);
  ASSERT_FALSE(rewriter_->Rewrite(convreq_, &segments));
}

// The combination of unicode and android carrier dependent emoji.
TEST_F(EmojiRewriterTest, CarrierEmojiSelectionDocomoUnicode) {
  Segments segments;
  SetSegment("X", "test", &segments);
  request_.set_available_emoji_carrier(Request::DOCOMO_EMOJI |
                                       Request::UNICODE_EMOJI);
  ASSERT_TRUE(rewriter_->Rewrite(convreq_, &segments));
  ASSERT_EQ(4, CountEmojiCandidates(segments));
  EXPECT_TRUE(HasExpectedCandidate(segments, "COW"));
  EXPECT_TRUE(HasExpectedCandidate(segments, "TIGER"));
  EXPECT_TRUE(HasExpectedCandidate(segments, "RABIT"));
  EXPECT_TRUE(HasExpectedCandidate(segments, "DRAGON"));
}

TEST_F(EmojiRewriterTest, CarrierEmojiSelectionSoftbankUnicode) {
  Segments segments;
  SetSegment("X", "test", &segments);
  request_.set_available_emoji_carrier(Request::SOFTBANK_EMOJI |
                                       Request::UNICODE_EMOJI);
  ASSERT_TRUE(rewriter_->Rewrite(convreq_, &segments));
  ASSERT_EQ(4, CountEmojiCandidates(segments));
  EXPECT_TRUE(HasExpectedCandidate(segments, "COW"));
  EXPECT_TRUE(HasExpectedCandidate(segments, "TIGER"));
  EXPECT_TRUE(HasExpectedCandidate(segments, "RABIT"));
  EXPECT_TRUE(HasExpectedCandidate(segments, "DRAGON"));
}

TEST_F(EmojiRewriterTest, CarrierEmojiSelectionKddiUnicode) {
  Segments segments;
  SetSegment("X", "test", &segments);
  request_.set_available_emoji_carrier(Request::KDDI_EMOJI |
                                       Request::UNICODE_EMOJI);
  ASSERT_TRUE(rewriter_->Rewrite(convreq_, &segments));
  ASSERT_EQ(4, CountEmojiCandidates(segments));
  EXPECT_TRUE(HasExpectedCandidate(segments, "COW"));
  EXPECT_TRUE(HasExpectedCandidate(segments, "TIGER"));
  EXPECT_TRUE(HasExpectedCandidate(segments, "RABIT"));
  EXPECT_TRUE(HasExpectedCandidate(segments, "DRAGON"));
}

TEST_F(EmojiRewriterTest, NoConversionWithDisabledSettings) {
  // This test checks no emoji conversion occur if emoji convrsion is disabled
  // in settings. Same segments are tested with ConvertedSegmentsHasEmoji test.

  // Disable emoji conversion in settings
  config_.set_use_emoji_conversion(false);

  Segments segments;
  SetSegment("test", "test", &segments);
  EXPECT_FALSE(rewriter_->Rewrite(convreq_, &segments));
  EXPECT_EQ(0, CountEmojiCandidates(segments));

  SetSegment("Neko", "test", &segments);
  EXPECT_FALSE(rewriter_->Rewrite(convreq_, &segments));
  EXPECT_EQ(0, CountEmojiCandidates(segments));
  EXPECT_FALSE(HasExpectedCandidate(segments, "CAT"));

  SetSegment("Nezumi", "test", &segments);
  EXPECT_FALSE(rewriter_->Rewrite(convreq_, &segments));
  EXPECT_EQ(0, CountEmojiCandidates(segments));
  EXPECT_FALSE(HasExpectedCandidate(segments, "MOUSE"));
  EXPECT_FALSE(HasExpectedCandidate(segments, "RAT"));

  SetSegment(kEmoji, "test", &segments);
  EXPECT_FALSE(rewriter_->Rewrite(convreq_, &segments));
  EXPECT_EQ(0, CountEmojiCandidates(segments));
}

TEST_F(EmojiRewriterTest, CheckDescription) {
  const testing::MockDataManager data_manager;
  Segments segments;
  VariantsRewriter variants_rewriter(
      dictionary::POSMatcher(data_manager.GetPOSMatcherData()));

  SetSegment("Emoji", "test", &segments);
  EXPECT_TRUE(rewriter_->Rewrite(convreq_, &segments));
  EXPECT_TRUE(variants_rewriter.Rewrite(convreq_, &segments));
  ASSERT_LT(0, CountEmojiCandidates(segments));
  const Segment &segment = segments.segment(0);
  for (int i = 0; i < segment.candidates_size(); ++i) {
    const Segment::Candidate &candidate = segment.candidate(i);
    const std::string &description = candidate.description;
    // Skip non emoji candidates.
    if (!EmojiRewriter::IsEmojiCandidate(candidate)) {
      continue;
    }
    EXPECT_NE(std::string::npos, description.find("<機種依存文字>"))
        << "for \"" << candidate.value << "\" : \"" << description << "\"";
    EXPECT_EQ(std::string::npos, description.find("[全]"))
        << "for \"" << candidate.value << "\" : \"" << description << "\"";
  }
}

TEST_F(EmojiRewriterTest, CheckInsertPosition) {
  // This test checks if emoji candidates are inserted into the expected
  // position.

  // |kExpectPosition| has the same number with |kDefaultInsertPos| defined in
  // emoji_rewriter.cc.
  const int kExpectPosition = 6;

  Segments segments;
  {
    Segment *segment = segments.push_back_segment();
    segment->set_key("Neko");
    for (int i = 0; i < kExpectPosition * 2; ++i) {
      std::string value = "candidate" + std::to_string(i);
      Segment::Candidate *candidate = segment->add_candidate();
      candidate->Init();
      candidate->value = value;
      candidate->content_key = "Neko";
      candidate->content_value = value;
    }
  }
  EXPECT_TRUE(rewriter_->Rewrite(convreq_, &segments));

  ASSERT_EQ(1, segments.segments_size());
  const Segment &segment = segments.segment(0);
  ASSERT_LE(kExpectPosition, segment.candidates_size());
  for (int i = 0; i < kExpectPosition; ++i) {
    EXPECT_FALSE(EmojiRewriter::IsEmojiCandidate(segment.candidate(i)));
  }
  const Segment::Candidate &candidate = segment.candidate(kExpectPosition);
  EXPECT_TRUE(EmojiRewriter::IsEmojiCandidate(candidate));
  EXPECT_EQ("CAT", candidate.value);
}

TEST_F(EmojiRewriterTest, CheckUsageStats) {
  // This test checks the data stored in usage stats for EmojiRewriter.

  const char kStatsKey[] = "CommitEmoji";
  Segments segments;

  // No use, no registered keys
  EXPECT_STATS_NOT_EXIST(kStatsKey);

  // Converting non-emoji candidates does not matter.
  SetSegment("test", "test", &segments);
  EXPECT_FALSE(rewriter_->Rewrite(convreq_, &segments));
  rewriter_->Finish(convreq_, &segments);
  EXPECT_STATS_NOT_EXIST(kStatsKey);

  // Converting an emoji candidate.
  SetSegment("Nezumi", "test", &segments);
  EXPECT_TRUE(rewriter_->Rewrite(convreq_, &segments));
  ChooseEmojiCandidate(&segments);
  rewriter_->Finish(convreq_, &segments);
  EXPECT_COUNT_STATS(kStatsKey, 1);
  SetSegment(kEmoji, "test", &segments);
  EXPECT_TRUE(rewriter_->Rewrite(convreq_, &segments));
  ChooseEmojiCandidate(&segments);
  rewriter_->Finish(convreq_, &segments);
  EXPECT_COUNT_STATS(kStatsKey, 2);

  // Converting non-emoji keeps the previous usage stats.
  SetSegment("test", "test", &segments);
  EXPECT_FALSE(rewriter_->Rewrite(convreq_, &segments));
  rewriter_->Finish(convreq_, &segments);
  EXPECT_COUNT_STATS(kStatsKey, 2);
}

TEST_F(EmojiRewriterTest, QueryNormalization) {
  {
    Segments segments;
    SetSegment("Ｎｅｋｏ", "Neko", &segments);
    EXPECT_TRUE(rewriter_->Rewrite(convreq_, &segments));
  }
  {
    Segments segments;
    SetSegment("Neko", "Neko", &segments);
    EXPECT_TRUE(rewriter_->Rewrite(convreq_, &segments));
  }
}

TEST_F(EmojiRewriterTest, FullDataTest) {
  // U+1F646 (FACE WITH OK GESTURE)
  {
    Segments segments;
    SetSegment("ＯＫ", "OK", &segments);
    EXPECT_TRUE(full_data_rewriter_->Rewrite(convreq_, &segments));
  }
  {
    Segments segments;
    SetSegment("OK", "OK", &segments);
    EXPECT_TRUE(full_data_rewriter_->Rewrite(convreq_, &segments));
  }
  // U+2795 (HEAVY PLUS SIGN)
  {
    Segments segments;
    SetSegment("＋", "+", &segments);
    EXPECT_TRUE(full_data_rewriter_->Rewrite(convreq_, &segments));
  }
  {
    Segments segments;
    SetSegment("+", "+", &segments);
    EXPECT_TRUE(full_data_rewriter_->Rewrite(convreq_, &segments));
  }
  // U+1F522 (INPUT SYMBOL FOR NUMBERS)
  {
    Segments segments;
    SetSegment("１２３４", "1234", &segments);
    EXPECT_TRUE(full_data_rewriter_->Rewrite(convreq_, &segments));
  }
  {
    Segments segments;
    SetSegment("1234", "1234", &segments);
    EXPECT_TRUE(full_data_rewriter_->Rewrite(convreq_, &segments));
  }
  // U+1F552 (CLOCK FACE THREE OCLOCK)
  {
    Segments segments;
    SetSegment("３じ", "3ji", &segments);
    EXPECT_TRUE(full_data_rewriter_->Rewrite(convreq_, &segments));
  }
  {
    Segments segments;
    SetSegment("3じ", "3ji", &segments);
    EXPECT_TRUE(full_data_rewriter_->Rewrite(convreq_, &segments));
  }
  // U+31 U+20E3 (KEYCAP 1)
  // Since emoji_rewriter does not support multi-codepoint characters,
  // Rewrite function returns false though ideally it should be supported.
  {
    Segments segments;
    SetSegment("１", "1", &segments);
    EXPECT_TRUE(full_data_rewriter_->Rewrite(convreq_, &segments));
  }
  {
    Segments segments;
    SetSegment("1", "1", &segments);
    EXPECT_TRUE(full_data_rewriter_->Rewrite(convreq_, &segments));
  }
}

}  // namespace
}  // namespace mozc
