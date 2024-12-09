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

#include "rewriter/user_boundary_history_rewriter.h"

#include <string>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/file_util.h"
#include "base/system_util.h"
#include "config/config_handler.h"
#include "converter/converter_mock.h"
#include "converter/segments.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "testing/gmock.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace {

using ::testing::_;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::Ref;
using ::testing::Return;
using ::testing::SetArgPointee;

// Creates a simple candidate whose key and value are set to `text`.
Segment::Candidate MakeCandidate(absl::string_view text) {
  Segment::Candidate cand;
  cand.key = std::string(text);
  cand.content_key = cand.key;
  cand.value = cand.key;
  cand.content_value = cand.key;
  return cand;
}

// Creates a segment that contains a single candidate created by
// MakeCandidate().
Segment MakeSegment(absl::string_view text, Segment::SegmentType type) {
  Segment segment;
  segment.set_key(text);
  segment.set_segment_type(type);
  *segment.add_candidate() = MakeCandidate(text);
  return segment;
}

// Creates a simple segments.
Segments MakeSegments(absl::Span<const absl::string_view> segments_texts,
                      Segment::SegmentType type) {
  Segments segments;
  for (absl::string_view text : segments_texts) {
    *segments.add_segment() = MakeSegment(text, type);
  }
  return segments;
}

class UserBoundaryHistoryRewriterTest
    : public testing::TestWithTempUserProfile {
 protected:
  void SetUp() override { config::ConfigHandler::GetDefaultConfig(&config_); }

  void TearDown() override {
    MockConverter converter;
    UserBoundaryHistoryRewriter rewriter(&converter);
    // Clear history
    rewriter.Clear();
    config::ConfigHandler::GetDefaultConfig(&config_);
  }

  void SetIncognito(bool incognito) { config_.set_incognito_mode(incognito); }

  void SetLearningLevel(config::Config::HistoryLearningLevel level) {
    config_.set_history_learning_level(level);
  }

  ConversionRequest CreateConversionRequest() {
    return ConversionRequestBuilder().SetConfig(config_).Build();
  }

  config::Config config_;
};

TEST_F(UserBoundaryHistoryRewriterTest, CreateFile) {
  MockConverter converter;
  const UserBoundaryHistoryRewriter rewriter(&converter);
  const std::string history_file =
      FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(), "boundary.db");
  EXPECT_OK(FileUtil::FileExists(history_file));
}

// Test if the rewriter can learn the splitting position at which user
// explicitly resized segments.
TEST_F(UserBoundaryHistoryRewriterTest, SplitSegmentByHistory) {
  SetIncognito(false);
  SetLearningLevel(config::Config::DEFAULT_HISTORY);
  const ConversionRequest convreq = CreateConversionRequest();

  MockConverter converter;
  UserBoundaryHistoryRewriter rewriter(&converter);
  {
    // Suppose that a user splits the segment ["たんぽぽ"] into
    // ["たん", "ぽぽ"]. Let the rewriter learn this split.
    Segments segments = MakeSegments({"たん", "ぽぽ"}, Segment::FIXED_VALUE);
    // This field needs to be set to indicate that user resized this segments.
    segments.set_resized(true);
    rewriter.Finish(convreq, &segments);
  }
  {
    // Verify if ["たんぽぽ"] is split into ["たん", "ぽぽ"]. Since the actual
    // split is handled by the underlying converter, we verify that
    // ResizeSegment() is called with the length array [2, 2, 0, 0, 0, 0, 0, 0].
    // TODO(noriyukit): The current implementation always sets the length array
    // size to 8 with padded zeros. Better to set the actual length.
    Segments segments = MakeSegments({"たんぽぽ"}, Segment::FREE);
    EXPECT_CALL(converter, ResizeSegments(&segments, Ref(convreq),
                                          /*start_segment_index=*/0,
                                          ElementsAre(2, 2, 0, 0, 0, 0, 0, 0)))
        .WillOnce(Return(true));
    EXPECT_TRUE(rewriter.Rewrite(convreq, &segments));
  }
}

// Test if the rewriter can learn the joining key for which user explicitly
// resized segments.
TEST_F(UserBoundaryHistoryRewriterTest, JoinSegmentsByHistory) {
  SetIncognito(false);
  SetLearningLevel(config::Config::DEFAULT_HISTORY);
  const ConversionRequest convreq = CreateConversionRequest();

  MockConverter converter;
  UserBoundaryHistoryRewriter rewriter(&converter);
  {
    // Suppose that a user joins the segment ["たん", "ぽぽ"] to
    // ["たんぽぽ"]. Let the rewriter learn this.
    Segments segments = MakeSegments({"たんぽぽ"}, Segment::FIXED_VALUE);
    // This field needs to be set to indicate that user resized this segments.
    segments.set_resized(true);
    rewriter.Finish(convreq, &segments);
  }
  {
    // Verify if ["たん", "ぽぽ"] is joined to ["たんぽぽ"]. Since the actual
    // split is handled by the underlying converter, we verify that
    // ResizeSegment() is called with the length array [4, 0, 0, 0, 0, 0, 0, 0].
    // TODO(noriyukit): The current implementation always sets the length array
    // size to 8 with padded zeros. Better to set the actual length.
    Segments segments = MakeSegments({"たん", "ぽぽ"}, Segment::FREE);
    EXPECT_CALL(converter, ResizeSegments(&segments, Ref(convreq),
                                          /*start_segment_index=*/0,
                                          ElementsAre(4, 0, 0, 0, 0, 0, 0, 0)))
        .WillOnce(Return(true));
    EXPECT_TRUE(rewriter.Rewrite(convreq, &segments));
  }
}

TEST_F(UserBoundaryHistoryRewriterTest, NoInsertWhenIncognito) {
  SetLearningLevel(config::Config::DEFAULT_HISTORY);

  MockConverter converter;
  UserBoundaryHistoryRewriter rewriter(&converter);
  {
    // History should not be learned during incognito mode.
    SetIncognito(true);
    const ConversionRequest convreq = CreateConversionRequest();
    Segments segments = MakeSegments({"たん", "ぽぽ"}, Segment::FIXED_VALUE);
    segments.set_resized(true);
    rewriter.Finish(convreq, &segments);
  }
  {
    // Turn off the incognito mode. ResizeSegment() should not be called.
    SetIncognito(false);
    const ConversionRequest convreq = CreateConversionRequest();
    Segments segments = MakeSegments({"たんぽぽ"}, Segment::FREE);
    EXPECT_FALSE(rewriter.Rewrite(convreq, &segments));
  }
}

TEST_F(UserBoundaryHistoryRewriterTest, NoInsertWhenReadOnly) {
  SetIncognito(false);

  MockConverter converter;
  UserBoundaryHistoryRewriter rewriter(&converter);
  {
    // History should not be learned in read only mode.
    SetLearningLevel(config::Config::READ_ONLY);
    const ConversionRequest convreq = CreateConversionRequest();
    Segments segments = MakeSegments({"たん", "ぽぽ"}, Segment::FIXED_VALUE);
    segments.set_resized(true);
    rewriter.Finish(convreq, &segments);
  }
  {
    // Enable learning again. ResizeSegment() should not be called.
    SetLearningLevel(config::Config::DEFAULT_HISTORY);
    const ConversionRequest convreq = CreateConversionRequest();
    Segments segments = MakeSegments({"たんぽぽ"}, Segment::FREE);
    EXPECT_FALSE(rewriter.Rewrite(convreq, &segments));
  }
}

TEST_F(UserBoundaryHistoryRewriterTest, NoInsertWhenDisableUserHistory) {
  SetIncognito(false);
  SetLearningLevel(config::Config::DEFAULT_HISTORY);

  MockConverter converter;
  UserBoundaryHistoryRewriter rewriter(&converter);
  {
    // History should not be learned when user history is disabled.
    const ConversionRequest convreq =
        ConversionRequestBuilder()
            .SetConfig(config_)
            .SetOptions({.enable_user_history_for_conversion = false})
            .Build();
    Segments segments = MakeSegments({"たん", "ぽぽ"}, Segment::FIXED_VALUE);
    segments.set_resized(true);
    rewriter.Finish(convreq, &segments);
  }
  {
    // Enable learning again. ResizeSegment() should not be called.
    const ConversionRequest convreq =
        ConversionRequestBuilder()
            .SetConfig(config_)
            .SetOptions({.enable_user_history_for_conversion = true})
            .Build();
    Segments segments = MakeSegments({"たんぽぽ"}, Segment::FREE);
    EXPECT_FALSE(rewriter.Rewrite(convreq, &segments));
  }
}

TEST_F(UserBoundaryHistoryRewriterTest, NoInsertWhenNotResized) {
  SetIncognito(false);
  SetLearningLevel(config::Config::DEFAULT_HISTORY);
  const ConversionRequest convreq = CreateConversionRequest();

  MockConverter converter;
  UserBoundaryHistoryRewriter rewriter(&converter);
  {
    // History should not be learned when sements is not resized.
    Segments segments = MakeSegments({"たん", "ぽぽ"}, Segment::FIXED_VALUE);
    segments.set_resized(false);  // Not resized!
    rewriter.Finish(convreq, &segments);
  }
  {
    // ResizeSegment() should not be called.
    Segments segments = MakeSegments({"たんぽぽ"}, Segment::FREE);
    EXPECT_FALSE(rewriter.Rewrite(convreq, &segments));
  }
}

TEST_F(UserBoundaryHistoryRewriterTest, NoRewriteAfterClear) {
  SetIncognito(false);
  SetLearningLevel(config::Config::DEFAULT_HISTORY);
  const ConversionRequest convreq = CreateConversionRequest();

  MockConverter converter;
  UserBoundaryHistoryRewriter rewriter(&converter);
  {
    // History IS learned.
    Segments segments = MakeSegments({"たん", "ぽぽ"}, Segment::FIXED_VALUE);
    segments.set_resized(true);
    rewriter.Finish(convreq, &segments);
  }
  {
    // ResizeSegment() should not be called after clearing the history.
    rewriter.Clear();
    Segments segments = MakeSegments({"たんぽぽ"}, Segment::FREE);
    EXPECT_FALSE(rewriter.Rewrite(convreq, &segments));
  }
}

TEST_F(UserBoundaryHistoryRewriterTest, NoRewriteWhenIncognito) {
  SetIncognito(false);
  SetLearningLevel(config::Config::DEFAULT_HISTORY);

  MockConverter converter;
  UserBoundaryHistoryRewriter rewriter(&converter);
  {
    // History IS learned.
    const ConversionRequest convreq = CreateConversionRequest();
    Segments segments = MakeSegments({"たん", "ぽぽ"}, Segment::FIXED_VALUE);
    segments.set_resized(true);
    rewriter.Finish(convreq, &segments);
  }
  {
    // ResizeSegment() should not be called in incognito mode even after the
    // rewriter learned the history.
    SetIncognito(true);
    const ConversionRequest convreq = CreateConversionRequest();
    Segments segments = MakeSegments({"たんぽぽ"}, Segment::FREE);
    EXPECT_FALSE(rewriter.Rewrite(convreq, &segments));
  }
}

TEST_F(UserBoundaryHistoryRewriterTest, NoRewriteWhenNoHistory) {
  SetIncognito(false);
  SetLearningLevel(config::Config::DEFAULT_HISTORY);

  MockConverter converter;
  UserBoundaryHistoryRewriter rewriter(&converter);
  {
    // History IS learned.
    const ConversionRequest convreq = CreateConversionRequest();
    Segments segments = MakeSegments({"たん", "ぽぽ"}, Segment::FIXED_VALUE);
    segments.set_resized(true);
    rewriter.Finish(convreq, &segments);
  }
  {
    // ResizeSegment() should not be called when history is disabled in config
    // even after the rewriter learned the history.
    const ConversionRequest convreq = CreateConversionRequest();
    SetLearningLevel(config::Config::NO_HISTORY);
    Segments segments = MakeSegments({"たんぽぽ"}, Segment::FREE);
    EXPECT_FALSE(rewriter.Rewrite(convreq, &segments));
  }
}

TEST_F(UserBoundaryHistoryRewriterTest, NoRewriteWhenDisabledUserHistory) {
  SetIncognito(false);
  SetLearningLevel(config::Config::DEFAULT_HISTORY);

  MockConverter converter;
  UserBoundaryHistoryRewriter rewriter(&converter);
  {
    // History IS learned.
    const ConversionRequest convreq =
        ConversionRequestBuilder()
            .SetConfig(config_)
            .SetOptions({.enable_user_history_for_conversion = true})
            .Build();
    Segments segments = MakeSegments({"たん", "ぽぽ"}, Segment::FIXED_VALUE);
    segments.set_resized(true);
    rewriter.Finish(convreq, &segments);
  }
  {
    // ResizeSegment() should not be called when history is disabled in request
    // even after the rewriter learned the history.
    const ConversionRequest convreq =
        ConversionRequestBuilder()
            .SetConfig(config_)
            .SetOptions({.enable_user_history_for_conversion = false})
            .Build();
    Segments segments = MakeSegments({"たんぽぽ"}, Segment::FREE);
    EXPECT_FALSE(rewriter.Rewrite(convreq, &segments));
  }
}

TEST_F(UserBoundaryHistoryRewriterTest, NoRewriteWhenAlreadyResized) {
  SetIncognito(false);
  SetLearningLevel(config::Config::DEFAULT_HISTORY);

  MockConverter converter;
  UserBoundaryHistoryRewriter rewriter(&converter);
  {
    // History IS learned.
    const ConversionRequest convreq = CreateConversionRequest();
    Segments segments = MakeSegments({"たん", "ぽぽ"}, Segment::FIXED_VALUE);
    segments.set_resized(true);
    rewriter.Finish(convreq, &segments);
  }
  {
    // ResizeSegment() should not be called when the input segment is already
    // resized by the user.
    const ConversionRequest convreq = CreateConversionRequest();
    Segments segments = MakeSegments({"たんぽぽ"}, Segment::FREE);
    segments.set_resized(true);
    EXPECT_FALSE(rewriter.Rewrite(convreq, &segments));
  }
}

TEST_F(UserBoundaryHistoryRewriterTest, FailureOfSplitIsNotFatal) {
  SetIncognito(false);
  SetLearningLevel(config::Config::DEFAULT_HISTORY);

  MockConverter converter;
  UserBoundaryHistoryRewriter rewriter(&converter);
  {
    const ConversionRequest convreq = CreateConversionRequest();
    Segments segments = MakeSegments({"たん", "ぽぽ"}, Segment::FIXED_VALUE);
    segments.set_resized(true);
    rewriter.Finish(convreq, &segments);

    segments = MakeSegments({"わた", "げ"}, Segment::FIXED_VALUE);
    segments.set_resized(true);
    rewriter.Finish(convreq, &segments);
  }
  {
    const ConversionRequest convreq = CreateConversionRequest();
    Segments segments = MakeSegments({"たんぽぽ", "わたげ"}, Segment::FREE);
    EXPECT_CALL(converter, ResizeSegments(&segments, Ref(convreq),
                                          /*start_segment_index=*/0, _))
        .WillOnce(Return(false));
    EXPECT_CALL(converter, ResizeSegments(&segments, Ref(convreq),
                                          /*start_segment_index=*/1, _))
        .WillOnce(Return(false));
    EXPECT_FALSE(rewriter.Rewrite(convreq, &segments));
  }
  {
    const ConversionRequest convreq = CreateConversionRequest();
    Segments segments = MakeSegments({"たんぽぽ", "わたげ"}, Segment::FREE);
    const Segments resized =
        MakeSegments({"たん", "ぽぽ", "わたげ"}, Segment::FREE);
    EXPECT_CALL(converter, ResizeSegments(&segments, Ref(convreq),
                                          /*start_segment_index=*/0, _))
        .WillOnce(DoAll(SetArgPointee<0>(resized), Return(true)));
    EXPECT_CALL(converter, ResizeSegments(&segments, Ref(convreq),
                                          /*start_segment_index=*/1, _))
        .WillOnce(Return(false));
    EXPECT_TRUE(rewriter.Rewrite(convreq, &segments));
  }
  {
    const ConversionRequest convreq = CreateConversionRequest();
    Segments segments = MakeSegments({"たんぽぽ", "わたげ"}, Segment::FREE);
    const Segments resized =
        MakeSegments({"たんぽぽ", "わた", "げ"}, Segment::FREE);
    EXPECT_CALL(converter, ResizeSegments(&segments, Ref(convreq),
                                          /*start_segment_index=*/0, _))
        .WillOnce(Return(false));
    EXPECT_CALL(converter, ResizeSegments(&segments, Ref(convreq),
                                          /*start_segment_index=*/1, _))
        .WillOnce(DoAll(SetArgPointee<0>(resized), Return(true)));
    EXPECT_TRUE(rewriter.Rewrite(convreq, &segments));
  }
}

}  // namespace
}  // namespace mozc
