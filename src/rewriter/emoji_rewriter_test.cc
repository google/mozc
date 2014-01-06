// Copyright 2010-2014, Google Inc.
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
#include <string>

#include "base/logging.h"
#include "base/number_util.h"
#include "base/system_util.h"
#include "base/util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/conversion_request.h"
#include "converter/segments.h"
#include "data_manager/user_pos_manager.h"
#include "dictionary/pos_matcher.h"
#include "rewriter/variants_rewriter.h"
#include "session/commands.pb.h"
#include "testing/base/public/gunit.h"
#include "usage_stats/usage_stats.h"
#include "usage_stats/usage_stats_testing_util.h"

DECLARE_string(test_tmpdir);

namespace mozc {

using mozc::commands::Request;

namespace {

// "えもじ"
const char kEmoji[] = "\xE3\x81\x88\xE3\x82\x82\xE3\x81\x98";

// Makes |segments| to have only a segment with a key-value paired candidate.
void SetSegment(const string &key, const string &value, Segments *segments) {
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
                          const string &expect_value) {
  CHECK_LE(1, segments.segments_size());
  const Segment &segment = segments.segment(0);
  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    const Segment::Candidate &candidate = segment.candidate(i);
    LOG(ERROR) << "[i]" << candidate.value;
    if (candidate.value == expect_value) {
      return true;
    }
  }
  return false;
}

// Replaces an emoji candidate into the 0-th index, as the Mozc converter
// does with a commited candidate.
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

// Dictionary data set for tests.
const EmojiRewriter::EmojiData kTestEmojiData[] = {
  // An actual emoji character
  {"\xF0\x9F\x90\xAD", 0, "nezumi picture", NULL, NULL, NULL},

  // Meta candidates.
  {"DOG", 0, "inu", NULL, NULL, NULL},
  {"CAT", 0, "neko", NULL, NULL, NULL},
  {"MOUSE", 0, "nezumi", NULL, NULL, NULL},
  {"RAT", 0, "nezumi", NULL, NULL, NULL},

  // Test data for carrier.
  {"COW", 0xFE001, "ushi", NULL, NULL, NULL},
  {"TIGER", 0xFE002, "tora", "docomo", NULL, NULL},
  {"RABIT", 0xFE003, "usagi", NULL, "softbank", NULL},
  {"DRAGON", 0xFE004, "ryu", NULL, NULL, "kddi"},

  // No unicode available.
  {NULL, 0xFE011, NULL, "docomo", NULL, NULL},
  {NULL, 0xFE012, NULL, NULL, "softbank", NULL},
  {NULL, 0xFE013, NULL, NULL, NULL, "kddi"},

  // Multiple carriers available.
  {NULL, 0xFE021, NULL, "docomo", "softbank", NULL},
  {NULL, 0xFE022, NULL, "docomo", NULL, "kddi"},
  {NULL, 0xFE023, NULL, NULL, "softbank", "kddi"},
  {NULL, 0xFE024, NULL, "docomo", "softbank", "kddi"},
};

const uint16 kValueList[] = {0,
                             1, 2, 3, 4,
                             5, 6, 7, 8,
                             9, 10, 11,
                             12, 13, 14, 15};
const EmojiRewriter::Token kTestToken[] = {
  // Keys (the first string of each element) must be sorted lexicographically.
  {"Emoji", kValueList, 1},  // value is ""
  {"Inu", kValueList + 1, 1},  // value is "DOG"
  {"Neko", kValueList + 2, 1},  // value is "CAT"
  {"Nezumi", kValueList + 3, 2},  // values are "MOUSE" and "RAT"
  {"X", kValueList + 5, 11},  // values for carrier test.
};

string ToAndroidPuaString(int pua) {
  string str;
  Util::UCS4ToUTF8(pua, &str);
  return str;
}

}  // namespace

class EmojiRewriterTest : public ::testing::Test {
 protected:
  // This empty constructor is written to avoid bugs of Visual Studio.
  EmojiRewriterTest() {}

  virtual void SetUp() {
    original_profile_directory_ = SystemUtil::GetUserProfileDirectory();
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);

    // Enable emoji conversion
    config::ConfigHandler::GetConfig(&original_config_);
    config::Config config(original_config_);
    config.set_use_emoji_conversion(true);
    config::ConfigHandler::SetConfig(config);

    mozc::usage_stats::UsageStats::ClearAllStatsForTest();

    rewriter_.reset(new EmojiRewriter(
        kTestEmojiData, arraysize(kTestEmojiData),
        kTestToken, arraysize(kTestToken),
        kValueList));
  }

  virtual void TearDown() {
    mozc::usage_stats::UsageStats::ClearAllStatsForTest();

    config::ConfigHandler::SetConfig(original_config_);
    SystemUtil::SetUserProfileDirectory(original_profile_directory_);
  }

  const ConversionRequest request_;
  scoped_ptr<EmojiRewriter> rewriter_;

 private:
  string original_profile_directory_;
  config::Config original_config_;
  usage_stats::scoped_usage_stats_enabler usage_stats_enabler_;
};

TEST_F(EmojiRewriterTest, Capability) {
  Request request;
  ConversionRequest conversion_request(NULL, &request);

  request.set_emoji_rewriter_capability(Request::NOT_AVAILABLE);
  EXPECT_EQ(RewriterInterface::NOT_AVAILABLE,
            rewriter_->capability(conversion_request));

  request.set_emoji_rewriter_capability(Request::CONVERSION);
  EXPECT_EQ(RewriterInterface::CONVERSION,
            rewriter_->capability(conversion_request));

  request.set_emoji_rewriter_capability(Request::PREDICTION);
  EXPECT_EQ(RewriterInterface::PREDICTION,
            rewriter_->capability(conversion_request));

  request.set_emoji_rewriter_capability(Request::SUGGESTION);
  EXPECT_EQ(RewriterInterface::SUGGESTION,
            rewriter_->capability(conversion_request));

  request.set_emoji_rewriter_capability(Request::ALL);
  EXPECT_EQ(RewriterInterface::ALL,
            rewriter_->capability(conversion_request));
}

TEST_F(EmojiRewriterTest, ConvertedSegmentsHasEmoji) {
  // This test runs an EmojiRewriter with a few strings, and checks
  //   - no conversion occur with unknown string,
  //   - expected characters are added in a conversion with a string,
  //   - all emojis are added with a specific string.

  Segments segments;
  SetSegment("neko", "test", &segments);
  EXPECT_FALSE(rewriter_->Rewrite(request_, &segments));
  EXPECT_EQ(0, CountEmojiCandidates(segments));

  SetSegment("Neko", "test", &segments);
  EXPECT_TRUE(rewriter_->Rewrite(request_, &segments));
  EXPECT_EQ(1, CountEmojiCandidates(segments));
  EXPECT_TRUE(HasExpectedCandidate(segments, "CAT"));

  SetSegment("Nezumi", "test", &segments);
  EXPECT_TRUE(rewriter_->Rewrite(request_, &segments));
  EXPECT_EQ(2, CountEmojiCandidates(segments));
  EXPECT_TRUE(HasExpectedCandidate(segments, "MOUSE"));
  EXPECT_TRUE(HasExpectedCandidate(segments, "RAT"));

  SetSegment(kEmoji, "test", &segments);
  EXPECT_TRUE(rewriter_->Rewrite(request_, &segments));
  EXPECT_EQ(9, CountEmojiCandidates(segments));
}

TEST_F(EmojiRewriterTest, CarrierEmojiSelectionEmpty) {
  Request request;
  ConversionRequest conversion_request(NULL, &request);

  Segments segments;
  SetSegment("X", "test", &segments);
  request.set_available_emoji_carrier(0);  // Disable emoji rewriting.
  ASSERT_FALSE(rewriter_->Rewrite(conversion_request, &segments));
  ASSERT_EQ(0, CountEmojiCandidates(segments));
}

TEST_F(EmojiRewriterTest, CarrierEmojiSelectionUnicode) {
  Request request;
  ConversionRequest conversion_request(NULL, &request);

  Segments segments;
  SetSegment("X", "test", &segments);
  request.set_available_emoji_carrier(Request::UNICODE_EMOJI);
  ASSERT_TRUE(rewriter_->Rewrite(conversion_request, &segments));
  ASSERT_EQ(4, CountEmojiCandidates(segments));
  EXPECT_TRUE(HasExpectedCandidate(segments, "COW"));
  EXPECT_TRUE(HasExpectedCandidate(segments, "TIGER"));
  EXPECT_TRUE(HasExpectedCandidate(segments, "RABIT"));
  EXPECT_TRUE(HasExpectedCandidate(segments, "DRAGON"));
}

TEST_F(EmojiRewriterTest, CarrierEmojiSelectionDocomo) {
  Request request;
  ConversionRequest conversion_request(NULL, &request);

  Segments segments;
  SetSegment("X", "test", &segments);
  request.set_available_emoji_carrier(Request::DOCOMO_EMOJI);
  ASSERT_TRUE(rewriter_->Rewrite(conversion_request, &segments));
  ASSERT_EQ(5, CountEmojiCandidates(segments));
  EXPECT_TRUE(HasExpectedCandidate(segments, ToAndroidPuaString(0xFE002)));
  EXPECT_TRUE(HasExpectedCandidate(segments, ToAndroidPuaString(0xFE011)));
  EXPECT_TRUE(HasExpectedCandidate(segments, ToAndroidPuaString(0xFE021)));
  EXPECT_TRUE(HasExpectedCandidate(segments, ToAndroidPuaString(0xFE022)));
  EXPECT_TRUE(HasExpectedCandidate(segments, ToAndroidPuaString(0xFE024)));
}

TEST_F(EmojiRewriterTest, CarrierEmojiSelectionSoftbank) {
  Request request;
  ConversionRequest conversion_request(NULL, &request);

  Segments segments;
  SetSegment("X", "test", &segments);
  request.set_available_emoji_carrier(Request::SOFTBANK_EMOJI);
  ASSERT_TRUE(rewriter_->Rewrite(conversion_request, &segments));
  ASSERT_EQ(5, CountEmojiCandidates(segments));
  EXPECT_TRUE(HasExpectedCandidate(segments, ToAndroidPuaString(0xFE003)));
  EXPECT_TRUE(HasExpectedCandidate(segments, ToAndroidPuaString(0xFE012)));
  EXPECT_TRUE(HasExpectedCandidate(segments, ToAndroidPuaString(0xFE021)));
  EXPECT_TRUE(HasExpectedCandidate(segments, ToAndroidPuaString(0xFE023)));
  EXPECT_TRUE(HasExpectedCandidate(segments, ToAndroidPuaString(0xFE024)));
}

TEST_F(EmojiRewriterTest, CarrierEmojiSelectionKddi) {
  Request request;
  ConversionRequest conversion_request(NULL, &request);

  Segments segments;
  SetSegment("X", "test", &segments);
  request.set_available_emoji_carrier(Request::KDDI_EMOJI);
  ASSERT_TRUE(rewriter_->Rewrite(conversion_request, &segments));
  ASSERT_EQ(5, CountEmojiCandidates(segments));
  EXPECT_TRUE(HasExpectedCandidate(segments, ToAndroidPuaString(0xFE004)));
  EXPECT_TRUE(HasExpectedCandidate(segments, ToAndroidPuaString(0xFE013)));
  EXPECT_TRUE(HasExpectedCandidate(segments, ToAndroidPuaString(0xFE022)));
  EXPECT_TRUE(HasExpectedCandidate(segments, ToAndroidPuaString(0xFE023)));
  EXPECT_TRUE(HasExpectedCandidate(segments, ToAndroidPuaString(0xFE024)));
}

// The combination of unicode and android carrier dependent emoji.
TEST_F(EmojiRewriterTest, CarrierEmojiSelectionDocomoUnicode) {
  Request request;
  ConversionRequest conversion_request(NULL, &request);

  Segments segments;
  SetSegment("X", "test", &segments);
  request.set_available_emoji_carrier(
      Request::DOCOMO_EMOJI | Request::UNICODE_EMOJI);
  ASSERT_TRUE(rewriter_->Rewrite(conversion_request, &segments));
  ASSERT_EQ(9, CountEmojiCandidates(segments));
  EXPECT_TRUE(HasExpectedCandidate(segments, "COW"));
  EXPECT_TRUE(HasExpectedCandidate(segments, "TIGER"));
  EXPECT_TRUE(HasExpectedCandidate(segments, "RABIT"));
  EXPECT_TRUE(HasExpectedCandidate(segments, "DRAGON"));
  EXPECT_TRUE(HasExpectedCandidate(segments, ToAndroidPuaString(0xFE002)));
  EXPECT_TRUE(HasExpectedCandidate(segments, ToAndroidPuaString(0xFE011)));
  EXPECT_TRUE(HasExpectedCandidate(segments, ToAndroidPuaString(0xFE021)));
  EXPECT_TRUE(HasExpectedCandidate(segments, ToAndroidPuaString(0xFE022)));
  EXPECT_TRUE(HasExpectedCandidate(segments, ToAndroidPuaString(0xFE024)));
}

TEST_F(EmojiRewriterTest, CarrierEmojiSelectionSoftbankUnicode) {
  Request request;
  ConversionRequest conversion_request(NULL, &request);

  Segments segments;
  SetSegment("X", "test", &segments);
  request.set_available_emoji_carrier(
      Request::SOFTBANK_EMOJI | Request::UNICODE_EMOJI);
  ASSERT_TRUE(rewriter_->Rewrite(conversion_request, &segments));
  ASSERT_EQ(9, CountEmojiCandidates(segments));
  EXPECT_TRUE(HasExpectedCandidate(segments, "COW"));
  EXPECT_TRUE(HasExpectedCandidate(segments, "TIGER"));
  EXPECT_TRUE(HasExpectedCandidate(segments, "RABIT"));
  EXPECT_TRUE(HasExpectedCandidate(segments, "DRAGON"));
  EXPECT_TRUE(HasExpectedCandidate(segments, ToAndroidPuaString(0xFE003)));
  EXPECT_TRUE(HasExpectedCandidate(segments, ToAndroidPuaString(0xFE012)));
  EXPECT_TRUE(HasExpectedCandidate(segments, ToAndroidPuaString(0xFE021)));
  EXPECT_TRUE(HasExpectedCandidate(segments, ToAndroidPuaString(0xFE023)));
  EXPECT_TRUE(HasExpectedCandidate(segments, ToAndroidPuaString(0xFE024)));
}

TEST_F(EmojiRewriterTest, CarrierEmojiSelectionKddiUnicode) {
  Request request;
  ConversionRequest conversion_request(NULL, &request);

  Segments segments;
  SetSegment("X", "test", &segments);
  request.set_available_emoji_carrier(
      Request::KDDI_EMOJI | Request::UNICODE_EMOJI);
  ASSERT_TRUE(rewriter_->Rewrite(conversion_request, &segments));
  ASSERT_EQ(9, CountEmojiCandidates(segments));
  EXPECT_TRUE(HasExpectedCandidate(segments, "COW"));
  EXPECT_TRUE(HasExpectedCandidate(segments, "TIGER"));
  EXPECT_TRUE(HasExpectedCandidate(segments, "RABIT"));
  EXPECT_TRUE(HasExpectedCandidate(segments, "DRAGON"));
  EXPECT_TRUE(HasExpectedCandidate(segments, ToAndroidPuaString(0xFE004)));
  EXPECT_TRUE(HasExpectedCandidate(segments, ToAndroidPuaString(0xFE013)));
  EXPECT_TRUE(HasExpectedCandidate(segments, ToAndroidPuaString(0xFE022)));
  EXPECT_TRUE(HasExpectedCandidate(segments, ToAndroidPuaString(0xFE023)));
  EXPECT_TRUE(HasExpectedCandidate(segments, ToAndroidPuaString(0xFE024)));
}

TEST_F(EmojiRewriterTest, NoConversionWithDisabledSettings) {
  // This test checks no emoji conversion occur if emoji convrsion is disabled
  // in settings. Same segments are tested with ConvertedSegmentsHasEmoji test.

  // Disable emoji conversion in settings
  config::Config config;
  config::ConfigHandler::GetConfig(&config);
  config.set_use_emoji_conversion(false);
  config::ConfigHandler::SetConfig(config);

  Segments segments;
  SetSegment("test", "test", &segments);
  EXPECT_FALSE(rewriter_->Rewrite(request_, &segments));
  EXPECT_EQ(0, CountEmojiCandidates(segments));

  SetSegment("Neko", "test", &segments);
  EXPECT_FALSE(rewriter_->Rewrite(request_, &segments));
  EXPECT_EQ(0, CountEmojiCandidates(segments));
  EXPECT_FALSE(HasExpectedCandidate(segments, "CAT"));

  SetSegment("Nezumi", "test", &segments);
  EXPECT_FALSE(rewriter_->Rewrite(request_, &segments));
  EXPECT_EQ(0, CountEmojiCandidates(segments));
  EXPECT_FALSE(HasExpectedCandidate(segments, "MOUSE"));
  EXPECT_FALSE(HasExpectedCandidate(segments, "RAT"));

  SetSegment(kEmoji, "test", &segments);
  EXPECT_FALSE(rewriter_->Rewrite(request_, &segments));
  EXPECT_EQ(0, CountEmojiCandidates(segments));
}

TEST_F(EmojiRewriterTest, CheckDescription) {
  Segments segments;
  VariantsRewriter variants_rewriter(
      UserPosManager::GetUserPosManager()->GetPOSMatcher());

  SetSegment("Emoji", "test", &segments);
  EXPECT_TRUE(rewriter_->Rewrite(request_, &segments));
  EXPECT_TRUE(variants_rewriter.Rewrite(request_, &segments));
  ASSERT_LT(0, CountEmojiCandidates(segments));
  const Segment &segment = segments.segment(0);
  for (int i = 0; i < segment.candidates_size(); ++i) {
    const Segment::Candidate &candidate = segment.candidate(i);
    const string &description = candidate.description;
    // Skip non emoji candidates.
    if (!EmojiRewriter::IsEmojiCandidate(candidate)) {
      continue;
    }
    // "<機種依存文字>"
    EXPECT_NE(string::npos, description.find("<"
        "\xE6\xA9\x9F\xE7\xA8\xAE\xE4\xBE\x9D\xE5\xAD\x98"
        "\xE6\x96\x87\xE5\xAD\x97" ">"))
        << "for \"" << candidate.value << "\" : \"" << description << "\"";
    // "[全]"
    EXPECT_EQ(string::npos, description.find("[" "\xE5\x85\xA8" "]"))
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
      string value = "candidate" + NumberUtil::SimpleItoa(i);
      Segment::Candidate *candidate = segment->add_candidate();
      candidate->Init();
      candidate->value = value;
      candidate->content_key = "Neko";
      candidate->content_value = value;
    }
  }
  EXPECT_TRUE(rewriter_->Rewrite(request_, &segments));

  ASSERT_EQ(1, segments.segments_size());
  const Segment& segment = segments.segment(0);
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
  EXPECT_FALSE(rewriter_->Rewrite(request_, &segments));
  rewriter_->Finish(request_, &segments);
  EXPECT_STATS_NOT_EXIST(kStatsKey);

  // Converting an emoji candidate.
  SetSegment("Nezumi", "test", &segments);
  EXPECT_TRUE(rewriter_->Rewrite(request_, &segments));
  ChooseEmojiCandidate(&segments);
  rewriter_->Finish(request_, &segments);
  EXPECT_COUNT_STATS(kStatsKey, 1);
  SetSegment(kEmoji, "test", &segments);
  EXPECT_TRUE(rewriter_->Rewrite(request_, &segments));
  ChooseEmojiCandidate(&segments);
  rewriter_->Finish(request_, &segments);
  EXPECT_COUNT_STATS(kStatsKey, 2);

  // Converting non-emoji keeps the previous usage stats.
  SetSegment("test", "test", &segments);
  EXPECT_FALSE(rewriter_->Rewrite(request_, &segments));
  rewriter_->Finish(request_, &segments);
  EXPECT_COUNT_STATS(kStatsKey, 2);
}

}  // namespace mozc
