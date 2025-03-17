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

#include "rewriter/user_segment_history_rewriter.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "absl/log/check.h"
#include "absl/random/random.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "base/clock.h"
#include "base/clock_mock.h"
#include "base/file_util.h"
#include "base/number_util.h"
#include "base/system_util.h"
#include "config/character_form_manager.h"
#include "config/config_handler.h"
#include "converter/segments.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/pos_group.h"
#include "dictionary/pos_matcher.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "request/request_test_util.h"
#include "rewriter/number_rewriter.h"
#include "rewriter/variants_rewriter.h"
#include "testing/gmock.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {

class UserSegmentHistoryRewriterTestPeer {
 public:
  UserSegmentHistoryRewriterTestPeer() = delete;

  static Segments MakeLearningSegmentsFromInnerSegments(
      const ConversionRequest &request, const Segments &segments) {
    return UserSegmentHistoryRewriter::MakeLearningSegmentsFromInnerSegments(
        request, segments);
  }
};

namespace {

using config::CharacterFormManager;
using config::Config;
using config::ConfigHandler;
using dictionary::PosGroup;
using dictionary::PosMatcher;
using ::testing::WithParamInterface;

constexpr size_t kCandidatesSize = 20;

void InitSegments(Segments *segments, size_t size, size_t candidate_size) {
  segments->Clear();
  for (size_t i = 0; i < size; ++i) {
    Segment *segment = segments->add_segment();
    CHECK(segment);
    segment->set_key(std::string("segment") +
                     std::to_string(static_cast<uint32_t>(i)));
    for (size_t j = 0; j < candidate_size; ++j) {
      Segment::Candidate *c = segment->add_candidate();
      c->content_key = segment->key();
      c->content_value =
          std::string("candidate") + std::to_string(static_cast<uint32_t>(j));
      c->value = c->content_value;
      if (j == 0) {
        c->attributes |= Segment::Candidate::BEST_CANDIDATE;
      }
    }
    CHECK_EQ(segment->candidates_size(), candidate_size);
  }
  CHECK_EQ(segments->segments_size(), size);
}

void InitSegments(Segments *segments, size_t size) {
  InitSegments(segments, size, kCandidatesSize);
}

void AppendCandidateSuffix(Segment *segment, size_t index,
                           const absl::string_view suffix, uint16_t lid,
                           uint16_t rid) {
  segment->set_key(absl::StrCat(segment->key(), suffix));
  absl::StrAppend(&segment->mutable_candidate(index)->value, suffix);
  segment->mutable_candidate(index)->lid = lid;
  segment->mutable_candidate(index)->rid = rid;
}

void AppendCandidateSuffixWithLid(Segment *segment, size_t index,
                                  const absl::string_view suffix,
                                  uint16_t lid) {
  // if lid == 0 and rid == 0, we assume that candidate is t13n.
  // we set 1 as rid to avoid this.
  AppendCandidateSuffix(segment, index, suffix, lid, 1);
}

class UserSegmentHistoryRewriterTest : public testing::TestWithTempUserProfile {
 protected:
  UserSegmentHistoryRewriterTest()
      : pos_matcher_(mock_data_manager_.GetPosMatcherData()),
        pos_group_(mock_data_manager_.GetPosGroupData()) {}

  void SetUp() override {
    request_ = std::make_unique<commands::Request>();
    config_ = std::make_unique<config::Config>();
    ConfigHandler::GetDefaultConfig(config_.get());
    for (int i = 0; i < config_->character_form_rules_size(); ++i) {
      Config::CharacterFormRule *rule =
          config_->mutable_character_form_rules(i);
      if (rule->group() == "0" || rule->group() == "A" ||
          rule->group() == "(){}[]") {
        rule->set_preedit_character_form(Config::HALF_WIDTH);
        rule->set_conversion_character_form(Config::HALF_WIDTH);
      }
    }
    CharacterFormManager::GetCharacterFormManager()->ReloadConfig(*config_);

    Clock::SetClockForUnitTest(nullptr);
  }

  void TearDown() override {
    CharacterFormManager::GetCharacterFormManager()->SetDefaultRule();
    std::unique_ptr<UserSegmentHistoryRewriter> rewriter(
        CreateUserSegmentHistoryRewriter());
    rewriter->Clear();
  }

  const PosMatcher &pos_matcher() const { return pos_matcher_; }
  const PosGroup &pos_group() const { return pos_group_; }

  NumberRewriter *CreateNumberRewriter() const {
    return new NumberRewriter(mock_data_manager_);
  }

  UserSegmentHistoryRewriter *CreateUserSegmentHistoryRewriter() const {
    return new UserSegmentHistoryRewriter(pos_matcher_, pos_group_);
  }

  void SetNumberForm(Config::CharacterForm form) {
    for (size_t i = 0; i < config_->character_form_rules_size(); ++i) {
      Config::CharacterFormRule *rule =
          config_->mutable_character_form_rules(i);
      if (rule->group() == "0") {
        rule->set_conversion_character_form(form);
      }
    }
    CharacterFormManager::GetCharacterFormManager()->ReloadConfig(*config_);
    EXPECT_EQ(CharacterFormManager::GetCharacterFormManager()
                  ->GetConversionCharacterForm("0"),
              form);
  }

  ConversionRequest CreateConversionRequest() const {
    return ConversionRequestBuilder()
        .SetConfig(*config_)
        .SetRequest(*request_)
        .Build();
  }

  std::unique_ptr<config::Config> config_;
  std::unique_ptr<commands::Request> request_;

 private:
  const testing::MockDataManager mock_data_manager_;
  const PosMatcher pos_matcher_;
  const PosGroup pos_group_;
};

TEST_F(UserSegmentHistoryRewriterTest, CreateFile) {
  std::unique_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());
  const std::string history_file =
      FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(), "/segment.db");
  EXPECT_OK(FileUtil::FileExists(history_file));
}

TEST_F(UserSegmentHistoryRewriterTest, InvalidInputsTest) {
  Segments segments;
  std::unique_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());
  segments.Clear();

  const ConversionRequest convreq = CreateConversionRequest();
  EXPECT_FALSE(rewriter->Rewrite(convreq, &segments));
  rewriter->Finish(convreq, &segments);
}

TEST_F(UserSegmentHistoryRewriterTest, IncognitoModeTest) {
  Segments segments;
  std::unique_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());

  {
    config_->set_incognito_mode(false);
    const ConversionRequest convreq1 = CreateConversionRequest();
    InitSegments(&segments, 1);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(convreq1, &segments);
    InitSegments(&segments, 1);
    rewriter->Rewrite(convreq1, &segments);
    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate2");

    config_->set_incognito_mode(true);
    InitSegments(&segments, 1);
    const ConversionRequest convreq2 = CreateConversionRequest();
    rewriter->Rewrite(convreq2, &segments);
    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate0");
  }

  {
    rewriter->Clear();  // clear history
    config_->set_incognito_mode(true);
    const ConversionRequest convreq = CreateConversionRequest();
    InitSegments(&segments, 1);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(convreq, &segments);
    InitSegments(&segments, 1);
    rewriter->Rewrite(convreq, &segments);
    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate0");
  }
}

TEST_F(UserSegmentHistoryRewriterTest, ConfigTest) {
  Segments segments;
  std::unique_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());

  {
    config_->set_history_learning_level(Config::DEFAULT_HISTORY);
    const ConversionRequest convreq1 = CreateConversionRequest();
    InitSegments(&segments, 1);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(convreq1, &segments);
    InitSegments(&segments, 1);
    rewriter->Rewrite(convreq1, &segments);
    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate2");

    config_->set_history_learning_level(Config::NO_HISTORY);
    const ConversionRequest convreq2 = CreateConversionRequest();
    InitSegments(&segments, 1);
    rewriter->Rewrite(convreq2, &segments);
    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate0");

    config_->set_history_learning_level(Config::READ_ONLY);
    const ConversionRequest convreq3 = CreateConversionRequest();
    InitSegments(&segments, 1);
    rewriter->Rewrite(convreq3, &segments);
    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate2");
  }

  {
    config_->set_history_learning_level(Config::NO_HISTORY);
    const ConversionRequest convreq = CreateConversionRequest();
    InitSegments(&segments, 1);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(convreq, &segments);
    InitSegments(&segments, 1);
    rewriter->Rewrite(convreq, &segments);
    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate0");
  }
}

TEST_F(UserSegmentHistoryRewriterTest, DisableTest) {
  Segments segments;
  std::unique_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());
  const ConversionRequest convreq_hist_true =
      ConversionRequestBuilder()
          .SetConfig(*config_)
          .SetRequest(*request_)
          .SetOptions({.enable_user_history_for_conversion = true})
          .Build();
  const ConversionRequest convreq_hist_false =
      ConversionRequestBuilder()
          .SetConfig(*config_)
          .SetRequest(*request_)
          .SetOptions({.enable_user_history_for_conversion = false})
          .Build();

  {
    InitSegments(&segments, 1);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(convreq_hist_true, &segments);
    InitSegments(&segments, 1);
    rewriter->Rewrite(convreq_hist_true, &segments);
    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate2");

    InitSegments(&segments, 1);
    rewriter->Rewrite(convreq_hist_false, &segments);
    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate0");

    InitSegments(&segments, 1);
    rewriter->Rewrite(convreq_hist_true, &segments);
    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate2");
  }

  {
    InitSegments(&segments, 1);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(convreq_hist_false, &segments);
    InitSegments(&segments, 1);
    rewriter->Rewrite(convreq_hist_false, &segments);
    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate0");
  }
}

TEST_F(UserSegmentHistoryRewriterTest, BasicTest) {
  Segments segments;
  std::unique_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());

  rewriter->Clear();

  {
    const ConversionRequest convreq = CreateConversionRequest();
    InitSegments(&segments, 2);

    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(convreq, &segments);
    InitSegments(&segments, 2);
    rewriter->Rewrite(convreq, &segments);

    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate2");
    EXPECT_TRUE(segments.segment(0).candidate(0).attributes &
                Segment::Candidate::USER_SEGMENT_HISTORY_REWRITER);
    EXPECT_EQ(segments.segment(1).candidate(0).value, "candidate0");
    EXPECT_FALSE(segments.segment(1).candidate(0).attributes &
                 Segment::Candidate::USER_SEGMENT_HISTORY_REWRITER);

    InitSegments(&segments, 1);
    rewriter->Rewrite(convreq, &segments);
    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate2");
    EXPECT_TRUE(segments.segment(0).candidate(0).attributes &
                Segment::Candidate::USER_SEGMENT_HISTORY_REWRITER);
    segments.mutable_segment(0)->move_candidate(1, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(convreq, &segments);

    InitSegments(&segments, 1);
    rewriter->Rewrite(convreq, &segments);

    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate0");
    EXPECT_FALSE(segments.segment(0).candidate(0).attributes &
                 Segment::Candidate::USER_SEGMENT_HISTORY_REWRITER);

    InitSegments(&segments, 2);
    rewriter->Rewrite(convreq, &segments);
    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate2");
    EXPECT_TRUE(segments.segment(0).candidate(0).attributes &
                Segment::Candidate::USER_SEGMENT_HISTORY_REWRITER);
    EXPECT_EQ(segments.segment(1).candidate(0).value, "candidate0");
    EXPECT_FALSE(segments.segment(1).candidate(0).attributes &
                 Segment::Candidate::USER_SEGMENT_HISTORY_REWRITER);
    segments.mutable_segment(1)->move_candidate(3, 0);
    segments.mutable_segment(1)->mutable_candidate(0)->attributes |=
        Segment::Candidate::RERANKED;
    segments.mutable_segment(1)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(convreq, &segments);

    InitSegments(&segments, 2);
    rewriter->Rewrite(convreq, &segments);
    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate2");
    EXPECT_TRUE(segments.segment(0).candidate(0).attributes &
                Segment::Candidate::USER_SEGMENT_HISTORY_REWRITER);
    EXPECT_EQ(segments.segment(1).candidate(0).value, "candidate3");
    EXPECT_TRUE(segments.segment(1).candidate(0).attributes &
                Segment::Candidate::USER_SEGMENT_HISTORY_REWRITER);
  }

  rewriter->Clear();
  {
    const ConversionRequest convreq = CreateConversionRequest();
    InitSegments(&segments, 2);

    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(convreq, &segments);
    InitSegments(&segments, 2);
    rewriter->Rewrite(convreq, &segments);

    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate2");
    EXPECT_TRUE(segments.segment(0).candidate(0).attributes &
                Segment::Candidate::USER_SEGMENT_HISTORY_REWRITER);
    EXPECT_EQ(segments.segment(1).candidate(0).value, "candidate0");
    EXPECT_FALSE(segments.segment(1).candidate(0).attributes &
                 Segment::Candidate::USER_SEGMENT_HISTORY_REWRITER);

    InitSegments(&segments, 1);
    rewriter->Rewrite(convreq, &segments);
    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate2");
    EXPECT_TRUE(segments.segment(0).candidate(0).attributes &
                Segment::Candidate::USER_SEGMENT_HISTORY_REWRITER);

    // back to the original
    segments.mutable_segment(0)->move_candidate(1, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(convreq, &segments);

    InitSegments(&segments, 1);
    rewriter->Rewrite(convreq, &segments);
    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate0");
    EXPECT_FALSE(segments.segment(0).candidate(0).attributes &
                 Segment::Candidate::USER_SEGMENT_HISTORY_REWRITER);
  }
}

// Test for Issue 2155278
TEST_F(UserSegmentHistoryRewriterTest, SequenceTest) {
  Segments segments;
  std::unique_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());

  rewriter->Clear();

  ClockMock clock(absl::UnixEpoch());
  Clock::SetClockForUnitTest(&clock);

  {
    const ConversionRequest convreq = CreateConversionRequest();
    InitSegments(&segments, 1);

    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate2");
    rewriter->Finish(convreq, &segments);  // learn "candidate2"

    // Next timestamp of learning should be newer than previous learning.
    clock.Advance(absl::Seconds(1));

    InitSegments(&segments, 2);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);
    segments.mutable_segment(1)->set_key(segments.segment(0).key());
    EXPECT_EQ(segments.history_segments_size(), 1);
    rewriter->Rewrite(convreq, &segments);

    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate2");
    EXPECT_EQ(segments.segment(1).candidate(0).value, "candidate2");
    // 2 0 1 3 4 ..

    segments.mutable_segment(1)->move_candidate(3, 0);
    segments.mutable_segment(1)->mutable_candidate(0)->attributes |=
        Segment::Candidate::RERANKED;
    segments.mutable_segment(1)->set_segment_type(Segment::FIXED_VALUE);
    EXPECT_EQ(segments.segment(1).candidate(0).value, "candidate3");
    rewriter->Finish(convreq, &segments);  // learn "candidate3"

    clock.Advance(absl::Seconds(1));

    InitSegments(&segments, 3);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);
    segments.mutable_segment(1)->move_candidate(3, 0);
    segments.mutable_segment(1)->set_segment_type(Segment::HISTORY);
    segments.mutable_segment(1)->set_key(segments.segment(0).key());
    segments.mutable_segment(2)->set_key(segments.segment(0).key());
    EXPECT_EQ(segments.history_segments_size(), 2);
    rewriter->Rewrite(convreq, &segments);

    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate2");
    EXPECT_EQ(segments.segment(1).candidate(0).value, "candidate3");
    EXPECT_EQ(segments.segment(2).candidate(0).value, "candidate3");
    // 3 2 0 1 4 ..

    segments.mutable_segment(2)->move_candidate(1, 0);
    segments.mutable_segment(2)->mutable_candidate(0)->attributes |=
        Segment::Candidate::RERANKED;
    segments.mutable_segment(2)->set_segment_type(Segment::FIXED_VALUE);
    EXPECT_EQ(segments.segment(2).candidate(0).value, "candidate2");
    rewriter->Finish(convreq, &segments);  // learn "candidate2"

    clock.Advance(absl::Seconds(1));

    InitSegments(&segments, 4);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);
    segments.mutable_segment(1)->move_candidate(3, 0);
    segments.mutable_segment(1)->set_segment_type(Segment::HISTORY);
    segments.mutable_segment(1)->set_key(segments.segment(0).key());
    segments.mutable_segment(2)->move_candidate(2, 0);
    segments.mutable_segment(2)->set_segment_type(Segment::HISTORY);
    segments.mutable_segment(2)->set_key(segments.segment(0).key());
    segments.mutable_segment(3)->set_key(segments.segment(0).key());
    EXPECT_EQ(segments.history_segments_size(), 3);
    rewriter->Rewrite(convreq, &segments);

    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate2");
    EXPECT_EQ(segments.segment(1).candidate(0).value, "candidate3");
    EXPECT_EQ(segments.segment(2).candidate(0).value, "candidate2");
    EXPECT_EQ(segments.segment(3).candidate(0).value, "candidate2");
    // 2 3 0 1 4 ..
  }

  Clock::SetClockForUnitTest(nullptr);
}

TEST_F(UserSegmentHistoryRewriterTest, DupTest) {
  Segments segments;
  std::unique_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());

  rewriter->Clear();

  ClockMock clock(absl::UnixEpoch());
  Clock::SetClockForUnitTest(&clock);

  {
    const ConversionRequest convreq = CreateConversionRequest();
    InitSegments(&segments, 1);
    segments.mutable_segment(0)->move_candidate(4, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(convreq, &segments);
    InitSegments(&segments, 1);
    rewriter->Rewrite(convreq, &segments);

    // restored
    // 4,0,1,2,3,5,...
    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate4");
    segments.mutable_segment(0)->move_candidate(4, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    clock.Advance(absl::Seconds(1));
    rewriter->Finish(convreq, &segments);
    InitSegments(&segments, 1);
    rewriter->Rewrite(convreq, &segments);

    // 3,4,0,1,2,5
    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate3");
    EXPECT_EQ(segments.segment(0).candidate(1).value, "candidate4");
    segments.mutable_segment(0)->move_candidate(4, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    clock.Advance(absl::Seconds(1));
    rewriter->Finish(convreq, &segments);
    InitSegments(&segments, 1);
    rewriter->Rewrite(convreq, &segments);

    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate2");
    EXPECT_EQ(segments.segment(0).candidate(1).value, "candidate3");
    EXPECT_EQ(segments.segment(0).candidate(2).value, "candidate4");
  }

  Clock::SetClockForUnitTest(nullptr);
}

TEST_F(UserSegmentHistoryRewriterTest, LearningType) {
  Segments segments;
  std::unique_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());

  {
    const ConversionRequest convreq = CreateConversionRequest();
    rewriter->Clear();
    InitSegments(&segments, 1);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::NO_LEARNING;
    rewriter->Finish(convreq, &segments);
    InitSegments(&segments, 1);
    rewriter->Rewrite(convreq, &segments);
    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate0");
  }

  {
    const ConversionRequest convreq = CreateConversionRequest();
    rewriter->Clear();
    InitSegments(&segments, 1);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::NO_HISTORY_LEARNING;
    rewriter->Finish(convreq, &segments);
    InitSegments(&segments, 1);
    rewriter->Rewrite(convreq, &segments);
    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate0");
  }

  {
    const ConversionRequest convreq = CreateConversionRequest();
    rewriter->Clear();
    InitSegments(&segments, 1);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::NO_SUGGEST_LEARNING;
    rewriter->Finish(convreq, &segments);
    InitSegments(&segments, 1);
    rewriter->Rewrite(convreq, &segments);
    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate2");
  }
}

TEST_F(UserSegmentHistoryRewriterTest, ContextSensitive) {
  Segments segments;
  std::unique_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());

  rewriter->Clear();
  {
    const ConversionRequest convreq = CreateConversionRequest();
    InitSegments(&segments, 2);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::CONTEXT_SENSITIVE;
    rewriter->Finish(convreq, &segments);
    InitSegments(&segments, 2);
    rewriter->Rewrite(convreq, &segments);

    // fire if two segments
    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate2");
    // not fire if single segment
    InitSegments(&segments, 1);
    rewriter->Rewrite(convreq, &segments);
    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate0");
  }

  rewriter->Clear();
  {
    const ConversionRequest convreq = CreateConversionRequest();
    InitSegments(&segments, 1);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::CONTEXT_SENSITIVE;
    rewriter->Finish(convreq, &segments);

    // fire if even in single segment
    InitSegments(&segments, 1);
    rewriter->Rewrite(convreq, &segments);
    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate2");

    // not fire if two segments
    InitSegments(&segments, 2);
    rewriter->Rewrite(convreq, &segments);
    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate0");
  }
}

TEST_F(UserSegmentHistoryRewriterTest, ContentValueLearning) {
  Segments segments;
  std::unique_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());

  rewriter->Clear();
  {
    const ConversionRequest convreq = CreateConversionRequest();
    InitSegments(&segments, 1);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 0, ":all", 0);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 2, ":all", 0);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);

    rewriter->Finish(convreq, &segments);

    InitSegments(&segments, 1);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 0, ":all", 0);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 2, ":all", 0);

    rewriter->Rewrite(convreq, &segments);

    // exact match
    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate2:all");

    InitSegments(&segments, 1);
    rewriter->Rewrite(convreq, &segments);

    // content value only:
    // in both learning/applying phase, lid and suffix are the same
    // as those of top candidates.
    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate2");

    InitSegments(&segments, 1);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 0, ":other", 0);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 2, ":other", 0);

    rewriter->Rewrite(convreq, &segments);

    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate2:other");
  }

  // In learning phase, lid is different
  rewriter->Clear();
  {
    const ConversionRequest convreq = CreateConversionRequest();
    InitSegments(&segments, 1);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 0, ":all", 0);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 2, ":all", 1);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(convreq, &segments);
    InitSegments(&segments, 1);
    rewriter->Rewrite(convreq, &segments);
    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate0");
  }

  // In learning phase, suffix (functional value) is different
  rewriter->Clear();
  {
    const ConversionRequest convreq = CreateConversionRequest();
    InitSegments(&segments, 1);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 0, "", 0);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 2, ":other", 1);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(convreq, &segments);
    InitSegments(&segments, 1);
    rewriter->Rewrite(convreq, &segments);
    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate0");
  }

  // In apply phase, lid is different
  rewriter->Clear();
  {
    const ConversionRequest convreq = CreateConversionRequest();
    InitSegments(&segments, 1);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 0, ":all", 0);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 2, ":all", 0);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(convreq, &segments);
    InitSegments(&segments, 1);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 0, ":other", 0);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 2, ":other", 1);
    rewriter->Rewrite(convreq, &segments);
    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate0:other");
  }

  // In apply phase, suffix (functional value) is different
  rewriter->Clear();
  {
    const ConversionRequest convreq = CreateConversionRequest();
    InitSegments(&segments, 1);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 0, ":all", 0);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 2, ":all", 0);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(convreq, &segments);
    InitSegments(&segments, 1);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 0, "", 0);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 2, ":other", 0);
    rewriter->Rewrite(convreq, &segments);
    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate0");
  }
}

TEST_F(UserSegmentHistoryRewriterTest, ReplaceableTest) {
  Segments segments;
  std::unique_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());

  rewriter->Clear();
  {
    const ConversionRequest convreq = CreateConversionRequest();
    InitSegments(&segments, 2);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 0, ":all", 0);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 2, ":all", 0);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);

    rewriter->Finish(convreq, &segments);

    InitSegments(&segments, 1);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 0, ":all", 0);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 2, ":all", 0);

    rewriter->Rewrite(convreq, &segments);

    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate2:all");

    InitSegments(&segments, 1);
    rewriter->Rewrite(convreq, &segments);
  }

  rewriter->Clear();
  {
    const ConversionRequest convreq = CreateConversionRequest();
    InitSegments(&segments, 1);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 0, ":all", 0);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 2, ":all", 0);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);

    rewriter->Finish(convreq, &segments);

    InitSegments(&segments, 2);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 0, ":all", 0);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 2, ":all", 0);

    rewriter->Rewrite(convreq, &segments);

    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate2:all");

    InitSegments(&segments, 1);
    rewriter->Rewrite(convreq, &segments);
  }

  rewriter->Clear();
  {
    const ConversionRequest convreq = CreateConversionRequest();
    InitSegments(&segments, 2);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 0, ":all", 0);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 2, ":all", 1);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(convreq, &segments);
    InitSegments(&segments, 1);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 0, ":all", 0);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 2, ":all", 0);
    rewriter->Rewrite(convreq, &segments);
    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate0:all");
  }

  rewriter->Clear();
  {
    const ConversionRequest convreq = CreateConversionRequest();
    InitSegments(&segments, 2);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 0, ":all", 0);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 2, ":all", 0);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(convreq, &segments);
    InitSegments(&segments, 1);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 0, ":all", 0);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 2, ":all", 1);
    rewriter->Rewrite(convreq, &segments);
    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate0:all");
  }

  rewriter->Clear();
  {
    const ConversionRequest convreq = CreateConversionRequest();
    InitSegments(&segments, 1);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 0, ":all", 0);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 2, ":all", 1);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(convreq, &segments);
    InitSegments(&segments, 2);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 0, ":all", 0);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 2, ":all", 0);
    rewriter->Rewrite(convreq, &segments);
    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate0:all");
  }

  rewriter->Clear();
  {
    const ConversionRequest convreq = CreateConversionRequest();
    InitSegments(&segments, 1);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 0, ":all", 0);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 2, ":all", 0);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(convreq, &segments);
    InitSegments(&segments, 2);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 0, ":all", 0);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 2, ":all", 1);
    rewriter->Rewrite(convreq, &segments);
    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate0:all");
  }
}

TEST_F(UserSegmentHistoryRewriterTest, NotReplaceableForDifferentId) {
  Segments segments;
  std::unique_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());

  rewriter->Clear();
  {
    const ConversionRequest convreq = CreateConversionRequest();
    InitSegments(&segments, 2);
    AppendCandidateSuffix(segments.mutable_segment(0), 0, ":all", 1, 1);
    AppendCandidateSuffix(segments.mutable_segment(0), 2, ":all", 200, 300);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);

    rewriter->Finish(convreq, &segments);

    InitSegments(&segments, 2);
    AppendCandidateSuffix(segments.mutable_segment(0), 0, ":all", 1, 1);
    AppendCandidateSuffix(segments.mutable_segment(0), 2, ":all", 200, 300);
    segments.mutable_segment(1)->mutable_candidate(0)->value =
        "not_same_as_before";

    rewriter->Rewrite(convreq, &segments);

    EXPECT_NE(segments.segment(0).candidate(0).value, "candidate2:all");
  }
}

TEST_F(UserSegmentHistoryRewriterTest, ReplaceableForSameId) {
  Segments segments;
  std::unique_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());

  rewriter->Clear();
  {
    const ConversionRequest convreq = CreateConversionRequest();
    InitSegments(&segments, 2);
    AppendCandidateSuffix(segments.mutable_segment(0), 0, ":all", 1, 1);
    AppendCandidateSuffix(segments.mutable_segment(0), 2, ":all", 1, 1);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);

    rewriter->Finish(convreq, &segments);

    InitSegments(&segments, 2);
    AppendCandidateSuffix(segments.mutable_segment(0), 0, ":all", 1, 1);
    AppendCandidateSuffix(segments.mutable_segment(0), 2, ":all", 1, 1);
    segments.mutable_segment(1)->mutable_candidate(0)->value =
        "not_same_as_before";

    rewriter->Rewrite(convreq, &segments);

    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate2:all");
  }
}

TEST_F(UserSegmentHistoryRewriterTest, ReplaceableT13NTest) {
  Segments segments;
  std::unique_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());

  rewriter->Clear();
  {
    const ConversionRequest convreq = CreateConversionRequest();
    InitSegments(&segments, 2);
    AppendCandidateSuffix(segments.mutable_segment(0), 0, ":all", 1, 1);
    // Prepare candidate2 as T13N candidate.
    AppendCandidateSuffix(segments.mutable_segment(0), 2, ":all", 0, 0);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);

    rewriter->Finish(convreq, &segments);

    InitSegments(&segments, 2);
    AppendCandidateSuffix(segments.mutable_segment(0), 0, ":all", 1, 1);
    AppendCandidateSuffix(segments.mutable_segment(0), 2, ":all", 0, 0);
    segments.mutable_segment(1)->mutable_candidate(0)->value =
        "not_same_as_before";

    rewriter->Rewrite(convreq, &segments);

    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate2:all");
  }

  rewriter->Clear();
  {
    auto set_up_segments = [&]() {
      InitSegments(&segments, 2);
      AppendCandidateSuffix(segments.mutable_segment(0), 0, "", 1, 1);
      // Prepare candidate2 as T13N candidate (lid, rid != 0)
      {
        Segment::Candidate *c =
            segments.mutable_segment(0)->mutable_candidate(2);
        c->value = "ひらがな";
        c->content_value = "ひらがな";
        c->lid = 10;
        c->rid = 10;
      }
    };
    const ConversionRequest convreq = CreateConversionRequest();

    set_up_segments();
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);

    rewriter->Finish(convreq, &segments);

    set_up_segments();
    segments.mutable_segment(1)->mutable_candidate(0)->value =
        "not_same_as_before";

    rewriter->Rewrite(convreq, &segments);

    EXPECT_EQ(segments.segment(0).candidate(0).value, "ひらがな");
  }
}

TEST_F(UserSegmentHistoryRewriterTest, LeftRightNumber) {
  Segments segments;
  std::unique_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());

  rewriter->Clear();

  {
    const ConversionRequest convreq = CreateConversionRequest();
    InitSegments(&segments, 2);

    segments.mutable_segment(0)->mutable_candidate(0)->value = "1234";
    segments.mutable_segment(1)->move_candidate(2, 0);
    segments.mutable_segment(1)->mutable_candidate(0)->attributes |=
        Segment::Candidate::RERANKED;
    segments.mutable_segment(1)->mutable_candidate(0)->attributes |=
        Segment::Candidate::CONTEXT_SENSITIVE;
    segments.mutable_segment(1)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(convreq, &segments);
    EXPECT_EQ(segments.segment(0).candidate(0).value, "1234");
    EXPECT_EQ(segments.segment(1).candidate(0).value, "candidate2");

    InitSegments(&segments, 2);
    // different num.
    segments.mutable_segment(0)->mutable_candidate(0)->value = "5678";
    rewriter->Rewrite(convreq, &segments);
    EXPECT_EQ(segments.segment(0).candidate(0).value, "5678");
    EXPECT_EQ(segments.segment(1).candidate(0).value, "candidate2");
  }

  {
    const ConversionRequest convreq = CreateConversionRequest();
    InitSegments(&segments, 2);

    segments.mutable_segment(1)->mutable_candidate(0)->value = "1234";
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::CONTEXT_SENSITIVE;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(convreq, &segments);
    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate2");
    EXPECT_EQ(segments.segment(1).candidate(0).value, "1234");

    InitSegments(&segments, 2);
    // different num.
    segments.mutable_segment(1)->mutable_candidate(0)->value = "5678";
    rewriter->Rewrite(convreq, &segments);
    EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate2");
    EXPECT_EQ(segments.segment(1).candidate(0).value, "5678");
  }
}

TEST_F(UserSegmentHistoryRewriterTest, BacketMatching) {
  Segments segments;
  std::unique_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());

  rewriter->Clear();

  {
    const ConversionRequest convreq = CreateConversionRequest();
    InitSegments(&segments, 1);
    Segment::Candidate *candidate =
        segments.mutable_segment(0)->insert_candidate(2);
    candidate->value = "(";
    candidate->content_value = "(";
    candidate->content_key = "(";
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(convreq, &segments);
  }

  {
    const ConversionRequest convreq = CreateConversionRequest();
    InitSegments(&segments, 1);
    Segment::Candidate *candidate =
        segments.mutable_segment(0)->insert_candidate(2);
    candidate->value = ")";
    candidate->content_value = ")";
    candidate->content_key = ")";

    rewriter->Rewrite(convreq, &segments);

    EXPECT_EQ(segments.segment(0).candidate(0).value, ")");
  }
}

// issue 2262691
TEST_F(UserSegmentHistoryRewriterTest, MultipleLearning) {
  Segments segments;
  std::unique_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());

  rewriter->Clear();

  {
    const ConversionRequest convreq = CreateConversionRequest();
    InitSegments(&segments, 1);
    segments.mutable_segment(0)->set_key("key1");
    Segment::Candidate *candidate =
        segments.mutable_segment(0)->insert_candidate(2);
    candidate->value = "value1";
    candidate->content_value = "value1";
    candidate->content_key = "key1";
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(convreq, &segments);
  }

  {
    const ConversionRequest convreq = CreateConversionRequest();
    InitSegments(&segments, 1);
    segments.mutable_segment(0)->set_key("key2");
    Segment::Candidate *candidate =
        segments.mutable_segment(0)->insert_candidate(2);
    candidate->value = "value2";
    candidate->content_value = "value2";
    candidate->content_key = "key2";
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(convreq, &segments);
  }

  {
    const ConversionRequest convreq = CreateConversionRequest();
    InitSegments(&segments, 1);
    segments.mutable_segment(0)->set_key("key1");
    Segment::Candidate *candidate =
        segments.mutable_segment(0)->insert_candidate(2);
    candidate->value = "value2";
    candidate->content_value = "value2";
    candidate->content_key = "key2";
    candidate = segments.mutable_segment(0)->insert_candidate(3);
    candidate->value = "value1";
    candidate->content_value = "value1";
    candidate->content_key = "key1";

    rewriter->Rewrite(convreq, &segments);

    EXPECT_EQ(segments.segment(0).candidate(0).value, "value1");
  }
}

TEST_F(UserSegmentHistoryRewriterTest, NumberSpecial) {
  Segments segments;
  std::unique_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());
  std::unique_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());

  rewriter->Clear();

  {
    const ConversionRequest convreq = CreateConversionRequest();
    segments.Clear();
    segments.add_segment();
    segments.mutable_segment(0)->set_key("12");
    Segment::Candidate *candidate =
        segments.mutable_segment(0)->insert_candidate(0);
    candidate->value = "⑫";
    candidate->content_value = "⑫";
    candidate->content_key = "12";
    candidate->lid = pos_matcher().GetNumberId();
    candidate->rid = pos_matcher().GetNumberId();
    candidate->style = NumberUtil::NumberString::NUMBER_CIRCLED;
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(convreq, &segments);
  }

  {
    const ConversionRequest convreq = CreateConversionRequest();
    segments.Clear();
    segments.add_segment();
    segments.mutable_segment(0)->set_key("14");
    {
      Segment::Candidate *candidate =
          segments.mutable_segment(0)->insert_candidate(0);
      candidate->value = "14";
      candidate->content_value = "14";
      candidate->content_key = "14";
      candidate->lid = pos_matcher().GetNumberId();
      candidate->rid = pos_matcher().GetNumberId();
    }
    EXPECT_TRUE(number_rewriter->Rewrite(convreq, &segments));
    rewriter->Rewrite(convreq, &segments);

    EXPECT_EQ(segments.segment(0).candidate(0).value, "⑭");
  }
}

TEST_F(UserSegmentHistoryRewriterTest, NumberHalfWidth) {
  SetNumberForm(Config::HALF_WIDTH);
  Segments segments;
  std::unique_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());
  std::unique_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());

  rewriter->Clear();

  {
    const ConversionRequest convreq = CreateConversionRequest();
    segments.Clear();
    segments.add_segment();
    segments.mutable_segment(0)->set_key("1234");
    Segment::Candidate *candidate =
        segments.mutable_segment(0)->insert_candidate(0);
    candidate->value = "１，２３４";
    candidate->content_value = "１，２３４";
    candidate->content_key = "1234";
    candidate->lid = pos_matcher().GetNumberId();
    candidate->rid = pos_matcher().GetNumberId();
    candidate->style =
        NumberUtil::NumberString::NUMBER_SEPARATED_ARABIC_FULLWIDTH;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(convreq, &segments);  // full-width for separated number
  }

  {
    const ConversionRequest convreq = CreateConversionRequest();
    segments.Clear();
    segments.add_segment();
    segments.mutable_segment(0)->set_key("1234");
    {
      Segment::Candidate *candidate =
          segments.mutable_segment(0)->insert_candidate(0);
      candidate->value = "1234";
      candidate->content_value = "1234";
      candidate->content_key = "1234";
      candidate->lid = pos_matcher().GetNumberId();
      candidate->rid = pos_matcher().GetNumberId();
    }

    EXPECT_TRUE(number_rewriter->Rewrite(convreq, &segments));
    rewriter->Rewrite(convreq, &segments);

    EXPECT_EQ("1,234", segments.segment(0).candidate(0).value);
  }
}

TEST_F(UserSegmentHistoryRewriterTest, NumberFullWidth) {
  SetNumberForm(Config::FULL_WIDTH);
  Segments segments;
  std::unique_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());
  std::unique_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());

  rewriter->Clear();

  {
    const ConversionRequest convreq = CreateConversionRequest();
    segments.Clear();
    segments.add_segment();
    segments.mutable_segment(0)->set_key("1234");
    Segment::Candidate *candidate =
        segments.mutable_segment(0)->insert_candidate(0);
    candidate->value = "1,234";
    candidate->content_value = "1,2344";
    candidate->content_key = "1234";
    candidate->lid = pos_matcher().GetNumberId();
    candidate->rid = pos_matcher().GetNumberId();
    candidate->style =
        NumberUtil::NumberString::NUMBER_SEPARATED_ARABIC_HALFWIDTH;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(convreq, &segments);  // half-width for separated number
  }

  {
    const ConversionRequest convreq = CreateConversionRequest();
    segments.Clear();
    segments.add_segment();
    {
      segments.mutable_segment(0)->set_key("1234");
      Segment::Candidate *candidate =
          segments.mutable_segment(0)->insert_candidate(0);
      candidate->value = "1234";
      candidate->content_value = "1234";
      candidate->content_key = "1234";
      candidate->lid = pos_matcher().GetNumberId();
      candidate->rid = pos_matcher().GetNumberId();
    }
    EXPECT_TRUE(number_rewriter->Rewrite(convreq, &segments));
    rewriter->Rewrite(convreq, &segments);

    EXPECT_EQ(segments.segment(0).candidate(0).value, "１，２３４");
  }
}

class UserSegmentHistoryNumberTest
    : public UserSegmentHistoryRewriterTest,
      public WithParamInterface<commands::Request> {};

INSTANTIATE_TEST_SUITE_P(
    NumberStyleLearningTestForRequest, UserSegmentHistoryNumberTest,
    ::testing::Values(
        []() {
          commands::Request request;
          request_test_util::FillMobileRequest(&request);
          return request;
        }(),
        []() {
          commands::Request request;
          request_test_util::FillMobileRequestWithHardwareKeyboard(&request);
          return request;
        }()));

TEST_P(UserSegmentHistoryNumberTest, UserSegmentHistoryRewriterTest) {
  const commands::Request request = GetParam();
  const ConversionRequest convreq = ConversionRequestBuilder()
                                        .SetConfig(*config_)
                                        .SetRequest(request)
                                        .Build();

  SetNumberForm(Config::FULL_WIDTH);
  Segments segments;
  std::unique_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());
  std::unique_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());

  rewriter->Clear();

  {
    segments.Clear();
    segments.add_segment();
    segments.mutable_segment(0)->set_key("1234");
    Segment::Candidate *candidate =
        segments.mutable_segment(0)->insert_candidate(0);
    candidate->value = "1,234";
    candidate->content_value = "1,2344";
    candidate->content_key = "1234";
    candidate->lid = pos_matcher().GetNumberId();
    candidate->rid = pos_matcher().GetNumberId();
    candidate->style =
        NumberUtil::NumberString::NUMBER_SEPARATED_ARABIC_HALFWIDTH;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(convreq, &segments);  // half-width for separated number
  }

  {
    // This rewriter does not handle number candidate
    segments.Clear();
    segments.add_segment();
    {
      segments.mutable_segment(0)->set_key("1234");
      Segment::Candidate *candidate =
          segments.mutable_segment(0)->insert_candidate(0);
      candidate->value = "1234";
      candidate->content_value = "1234";
      candidate->content_key = "1234";
      candidate->lid = pos_matcher().GetNumberId();
      candidate->rid = pos_matcher().GetNumberId();
    }
    EXPECT_TRUE(number_rewriter->Rewrite(convreq, &segments));
    rewriter->Rewrite(convreq, &segments);

    EXPECT_EQ(segments.segment(0).candidate(0).value, "1234");
  }
}

TEST_F(UserSegmentHistoryRewriterTest, NumberNoSeparated) {
  SetNumberForm(Config::HALF_WIDTH);
  Segments segments;
  std::unique_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());
  std::unique_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());

  rewriter->Clear();

  {
    const ConversionRequest convreq = CreateConversionRequest();
    segments.Clear();
    segments.add_segment();
    segments.mutable_segment(0)->set_key("10");
    Segment::Candidate *candidate =
        segments.mutable_segment(0)->insert_candidate(0);
    candidate->value = "十";
    candidate->content_value = "十";
    candidate->content_key = "10";
    candidate->lid = pos_matcher().GetNumberId();
    candidate->rid = pos_matcher().GetNumberId();
    candidate->style = NumberUtil::NumberString::NUMBER_KANJI;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(convreq, &segments);  // learn kanji
  }
  {
    const ConversionRequest convreq = CreateConversionRequest();
    segments.Clear();
    segments.add_segment();
    segments.mutable_segment(0)->set_key("1234");
    Segment::Candidate *candidate =
        segments.mutable_segment(0)->insert_candidate(0);
    candidate->value = "1,234";
    candidate->content_value = "1,234";
    candidate->content_key = "1234";
    candidate->lid = pos_matcher().GetNumberId();
    candidate->rid = pos_matcher().GetNumberId();
    candidate->style =
        NumberUtil::NumberString::NUMBER_SEPARATED_ARABIC_HALFWIDTH;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(convreq, &segments);  // learn kanji
  }

  {
    const ConversionRequest convreq = CreateConversionRequest();
    InitSegments(&segments, 1);
    segments.mutable_segment(0)->set_key("9");
    {
      Segment::Candidate *candidate =
          segments.mutable_segment(0)->insert_candidate(0);
      candidate->value = "9";
      candidate->content_value = "9";
      candidate->content_key = "9";
      candidate->lid = pos_matcher().GetNumberId();
      candidate->rid = pos_matcher().GetNumberId();
    }
    EXPECT_TRUE(number_rewriter->Rewrite(convreq, &segments));
    rewriter->Rewrite(convreq, &segments);

    // 9, not "九"
    EXPECT_EQ(segments.segment(0).candidate(0).value, "9");
  }
}

TEST_F(UserSegmentHistoryRewriterTest, Regression2459519) {
  Segments segments;
  std::unique_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());

  rewriter->Clear();

  ClockMock clock(absl::UnixEpoch());
  Clock::SetClockForUnitTest(&clock);

  const ConversionRequest convreq = CreateConversionRequest();
  InitSegments(&segments, 1);
  segments.mutable_segment(0)->move_candidate(2, 0);
  segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
      Segment::Candidate::RERANKED;
  segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
  rewriter->Finish(convreq, &segments);

  InitSegments(&segments, 1);
  rewriter->Rewrite(convreq, &segments);
  EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate2");
  EXPECT_EQ(segments.segment(0).candidate(1).value, "candidate0");

  segments.mutable_segment(0)->move_candidate(1, 0);
  segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
      Segment::Candidate::RERANKED;
  segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
  clock.Advance(absl::Seconds(1));
  rewriter->Finish(convreq, &segments);

  InitSegments(&segments, 1);
  rewriter->Rewrite(convreq, &segments);
  EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate0");
  EXPECT_EQ(segments.segment(0).candidate(1).value, "candidate2");

  segments.mutable_segment(0)->move_candidate(1, 0);
  segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
      Segment::Candidate::RERANKED;
  segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
  clock.Advance(absl::Seconds(1));
  rewriter->Finish(convreq, &segments);

  InitSegments(&segments, 1);
  rewriter->Rewrite(convreq, &segments);
  EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate2");
  EXPECT_EQ(segments.segment(0).candidate(1).value, "candidate0");

  Clock::SetClockForUnitTest(nullptr);
}

TEST_F(UserSegmentHistoryRewriterTest, Regression2459520) {
  Segments segments;
  std::unique_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());

  rewriter->Clear();

  const ConversionRequest convreq = CreateConversionRequest();
  InitSegments(&segments, 2);
  segments.mutable_segment(0)->move_candidate(2, 0);
  segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
      Segment::Candidate::RERANKED;
  segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);

  segments.mutable_segment(1)->move_candidate(3, 0);
  segments.mutable_segment(1)->mutable_candidate(0)->attributes |=
      Segment::Candidate::RERANKED;
  segments.mutable_segment(1)->set_segment_type(Segment::FIXED_VALUE);
  rewriter->Finish(convreq, &segments);

  InitSegments(&segments, 2);
  rewriter->Rewrite(convreq, &segments);
  EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate2");
  EXPECT_EQ(segments.segment(1).candidate(0).value, "candidate3");
}

TEST_F(UserSegmentHistoryRewriterTest, PuntuationsTest) {
  Segments segments;
  std::unique_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());

  const uint16_t id = pos_matcher().GetJapanesePunctuationsId();

  rewriter->Clear();

  const ConversionRequest convreq = CreateConversionRequest();
  InitSegments(&segments, 2);
  segments.mutable_segment(1)->set_key(".");
  for (int i = 1; i < kCandidatesSize; ++i) {
    segments.mutable_segment(1)->mutable_candidate(i)->lid = id;
    segments.mutable_segment(1)->mutable_candidate(i)->rid = id;
    segments.mutable_segment(1)->mutable_candidate(i)->value = ".";
  }
  segments.mutable_segment(1)->move_candidate(2, 0);
  segments.mutable_segment(1)->mutable_candidate(0)->attributes |=
      Segment::Candidate::RERANKED;
  segments.mutable_segment(1)->set_segment_type(Segment::FIXED_VALUE);
  rewriter->Finish(convreq, &segments);

  InitSegments(&segments, 2);
  segments.mutable_segment(1)->set_key(".");
  for (int i = 1; i < kCandidatesSize; ++i) {
    segments.mutable_segment(1)->mutable_candidate(i)->lid = id;
    segments.mutable_segment(1)->mutable_candidate(i)->rid = id;
    segments.mutable_segment(1)->mutable_candidate(i)->value = ".";
  }

  // Punctuation is not remembered
  rewriter->Rewrite(convreq, &segments);
  EXPECT_EQ(segments.segment(1).candidate(0).value, "candidate0");
}

TEST_F(UserSegmentHistoryRewriterTest, Regression3264619) {
  Segments segments;
  std::unique_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());

  // Too big candidates
  const ConversionRequest convreq = CreateConversionRequest();
  InitSegments(&segments, 2, 1024);
  segments.mutable_segment(0)->move_candidate(512, 0);
  segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
      Segment::Candidate::RERANKED;
  segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
  rewriter->Finish(convreq, &segments);
  InitSegments(&segments, 2, 1024);
  rewriter->Rewrite(convreq, &segments);

  EXPECT_EQ(segments.segment(0).candidate(0).value, "candidate512");
  EXPECT_EQ(segments.segment(1).candidate(0).value, "candidate0");
}

TEST_F(UserSegmentHistoryRewriterTest, RandomTest) {
  Segments segments;
  std::unique_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());

  ClockMock clock(absl::UnixEpoch());
  Clock::SetClockForUnitTest(&clock);

  rewriter->Clear();
  absl::BitGen gen;
  for (int i = 0; i < 5; ++i) {
    const ConversionRequest convreq = CreateConversionRequest();
    InitSegments(&segments, 1);
    const int n = absl::Uniform(gen, 0, 10);
    const std::string expected = segments.segment(0).candidate(n).value;
    segments.mutable_segment(0)->move_candidate(n, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    EXPECT_EQ(segments.segment(0).candidate(0).value, expected);
    rewriter->Finish(convreq, &segments);
    InitSegments(&segments, 1);
    rewriter->Rewrite(convreq, &segments);
    EXPECT_EQ(segments.segment(0).candidate(0).value, expected);
    clock.Advance(absl::Seconds(1));  // update LRU timer
  }

  Clock::SetClockForUnitTest(nullptr);
}

TEST_F(UserSegmentHistoryRewriterTest, AnnotationAfterLearning) {
  Segments segments;
  std::unique_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());

  {
    const ConversionRequest convreq = CreateConversionRequest();
    segments.Clear();
    InitSegments(&segments, 1, 2);
    segments.mutable_segment(0)->set_key("abc");
    Segment::Candidate *candidate =
        segments.mutable_segment(0)->mutable_candidate(1);
    candidate->value = "ａｂｃ";
    candidate->content_value = "ａｂｃ";
    candidate->content_key = "abc";
    candidate->description = "[全] アルファベット";
    segments.mutable_segment(0)->move_candidate(1, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(convreq, &segments);
  }

  {
    const ConversionRequest convreq = CreateConversionRequest();
    segments.Clear();
    InitSegments(&segments, 1, 2);
    segments.mutable_segment(0)->set_key("abc");
    Segment::Candidate *candidate =
        segments.mutable_segment(0)->mutable_candidate(1);
    candidate->value = "ａｂｃ";
    candidate->content_value = "ａｂｃ";
    candidate->content_key = "abc";
    candidate->description = "[全]アルファベット";
    candidate->content_key = "abc";
    rewriter->Rewrite(convreq, &segments);
    EXPECT_EQ(segments.segment(0).candidate(0).content_value, "abc");
    // "アルファベット"
    EXPECT_EQ(VariantsRewriter::kAlphabet,
              segments.segment(0).candidate(0).description);
    rewriter->Finish(convreq, &segments);
  }
}

TEST_F(UserSegmentHistoryRewriterTest, SupportInnerSegmentsOnLearning) {
  request_test_util::FillMobileRequest(request_.get());
  Segments segments;
  std::unique_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());

  {
    segments.Clear();
    InitSegments(&segments, 1, 2);
    constexpr absl::string_view kKey = "わたしのなまえはなかのです";
    constexpr absl::string_view kValue = "私の名前は中野です";
    segments.mutable_segment(0)->set_key(kKey);
    Segment::Candidate *candidate =
        segments.mutable_segment(0)->mutable_candidate(1);

    candidate->value = kValue;
    candidate->content_value = kValue;
    candidate->key = kKey;
    candidate->content_key = kKey;
    // "わたしの, 私の", "わたし, 私"
    candidate->PushBackInnerSegmentBoundary(12, 6, 9, 3);
    // "なまえは, 名前は", "なまえ, 名前"
    candidate->PushBackInnerSegmentBoundary(12, 9, 9, 6);
    // "なかのです, 中野です", "なかの, 中野"
    candidate->PushBackInnerSegmentBoundary(15, 12, 9, 6);
    candidate->lid = 10;
    candidate->rid = 20;

    segments.mutable_segment(0)->move_candidate(1, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);

    const ConversionRequest default_mobile_convreq = CreateConversionRequest();
    {
      const Segments learning_segments = UserSegmentHistoryRewriterTestPeer::
          MakeLearningSegmentsFromInnerSegments(default_mobile_convreq,
                                                segments);
      EXPECT_EQ(learning_segments.segments_size(), 3);
      EXPECT_EQ(learning_segments.segment(0).key(), "わたしの");
      EXPECT_EQ(learning_segments.segment(0).candidate(0).key, "わたしの");
      EXPECT_EQ(learning_segments.segment(0).candidate(0).value, "私の");
      EXPECT_EQ(learning_segments.segment(0).candidate(0).content_key,
                "わたし");
      EXPECT_EQ(learning_segments.segment(0).candidate(0).content_value, "私");
      EXPECT_EQ(learning_segments.segment(0).candidate(0).lid, 10);
      EXPECT_EQ(learning_segments.segment(0).candidate(0).rid, 10);
      EXPECT_EQ(learning_segments.segment(0).segment_type(),
                Segment::FIXED_VALUE);

      EXPECT_EQ(learning_segments.segment(1).key(), "なまえは");
      EXPECT_EQ(learning_segments.segment(1).candidate(0).key, "なまえは");
      EXPECT_EQ(learning_segments.segment(1).candidate(0).value, "名前は");
      EXPECT_EQ(learning_segments.segment(1).candidate(0).content_key,
                "なまえ");
      EXPECT_EQ(learning_segments.segment(1).candidate(0).content_value,
                "名前");
      EXPECT_EQ(learning_segments.segment(1).candidate(0).lid, 0);
      EXPECT_EQ(learning_segments.segment(1).candidate(0).rid, 0);
      EXPECT_EQ(learning_segments.segment(1).segment_type(),
                Segment::FIXED_VALUE);

      EXPECT_EQ(learning_segments.segment(2).key(), "なかのです");
      EXPECT_EQ(learning_segments.segment(2).candidate(0).key, "なかのです");
      EXPECT_EQ(learning_segments.segment(2).candidate(0).value, "中野です");
      EXPECT_EQ(learning_segments.segment(2).candidate(0).content_key,
                "なかの");
      EXPECT_EQ(learning_segments.segment(2).candidate(0).content_value,
                "中野");
      EXPECT_EQ(learning_segments.segment(2).candidate(0).lid, 20);
      EXPECT_EQ(learning_segments.segment(2).candidate(0).rid, 20);
      EXPECT_EQ(learning_segments.segment(2).segment_type(),
                Segment::FIXED_VALUE);
    }

    rewriter->Finish(default_mobile_convreq, &segments);
  }

  {
    // Inner segment boundary with size 1 may have better information.
    segments.Clear();
    InitSegments(&segments, 1, 2);
    constexpr absl::string_view kKey = "わたしの";
    constexpr absl::string_view kValue = "私の";
    segments.mutable_segment(0)->set_key(kKey);
    Segment::Candidate *candidate =
        segments.mutable_segment(0)->mutable_candidate(1);

    candidate->value = kValue;
    candidate->content_value = kValue;
    candidate->key = kKey;
    candidate->content_key = kKey;
    // "わたしの, 私の", "わたし, 私"
    candidate->PushBackInnerSegmentBoundary(12, 6, 9, 3);
    candidate->lid = 10;
    candidate->rid = 10;

    segments.mutable_segment(0)->move_candidate(1, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);

    const ConversionRequest default_mobile_convreq = CreateConversionRequest();
    {
      const Segments learning_segments = UserSegmentHistoryRewriterTestPeer::
          MakeLearningSegmentsFromInnerSegments(default_mobile_convreq,
                                                segments);
      EXPECT_EQ(learning_segments.segments_size(), 1);
      EXPECT_EQ(learning_segments.segment(0).key(), "わたしの");
      EXPECT_EQ(learning_segments.segment(0).candidate(0).key, "わたしの");
      EXPECT_EQ(learning_segments.segment(0).candidate(0).value, "私の");
      EXPECT_EQ(learning_segments.segment(0).candidate(0).content_key,
                "わたし");
      EXPECT_EQ(learning_segments.segment(0).candidate(0).content_value, "私");
      EXPECT_EQ(learning_segments.segment(0).candidate(0).lid, 10);
      EXPECT_EQ(learning_segments.segment(0).candidate(0).rid, 10);
      EXPECT_EQ(learning_segments.segment(0).segment_type(),
                Segment::FIXED_VALUE);
    }

    rewriter->Finish(default_mobile_convreq, &segments);
  }

  {
    segments.Clear();
    InitSegments(&segments, 1, 2);
    segments.mutable_segment(0)->set_key("なかの");
    Segment::Candidate *candidate =
        segments.mutable_segment(0)->mutable_candidate(0);
    candidate->value = "中埜";
    candidate->content_value = "中埜";
    candidate->content_key = "なかの";
    candidate->content_key = "なかの";

    candidate = segments.mutable_segment(0)->mutable_candidate(1);
    candidate->value = "中野";
    candidate->content_value = "中野";
    candidate->content_key = "なかの";
    candidate->content_key = "なかの";

    const ConversionRequest default_mobile_convreq = CreateConversionRequest();
    EXPECT_TRUE(rewriter->Rewrite(default_mobile_convreq, &segments));
    EXPECT_EQ(segments.segment(0).candidate(0).value, "中野");
  }

  {
    // Disable inner segment boundary for single segment
    request_->mutable_decoder_experiment_params()
        ->set_apply_single_inner_segment_boundary(false);

    // Inner segment boundary with size 1 may have better information.
    segments.Clear();
    InitSegments(&segments, 1, 2);
    constexpr absl::string_view kKey = "わたしの";
    constexpr absl::string_view kValue = "私の";
    segments.mutable_segment(0)->set_key(kKey);
    Segment::Candidate *candidate =
        segments.mutable_segment(0)->mutable_candidate(1);

    candidate->value = kValue;
    candidate->content_value = kValue;
    candidate->key = kKey;
    candidate->content_key = kKey;
    // "わたしの, 私の", "わたし, 私"
    candidate->PushBackInnerSegmentBoundary(12, 6, 9, 3);
    candidate->lid = 10;
    candidate->rid = 10;

    segments.mutable_segment(0)->move_candidate(1, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);

    const ConversionRequest convreq = CreateConversionRequest();
    {
      const Segments learning_segments = UserSegmentHistoryRewriterTestPeer::
          MakeLearningSegmentsFromInnerSegments(convreq, segments);
      EXPECT_EQ(learning_segments.segments_size(), 1);
      EXPECT_EQ(learning_segments.segment(0).key(), "わたしの");
      EXPECT_EQ(learning_segments.segment(0).candidate(0).key, "わたしの");
      EXPECT_EQ(learning_segments.segment(0).candidate(0).value, "私の");
      EXPECT_EQ(learning_segments.segment(0).candidate(0).content_key,
                "わたしの");
      EXPECT_EQ(learning_segments.segment(0).candidate(0).content_value,
                "私の");
      EXPECT_EQ(learning_segments.segment(0).candidate(0).lid, 10);
      EXPECT_EQ(learning_segments.segment(0).candidate(0).rid, 10);
      EXPECT_EQ(learning_segments.segment(0).segment_type(),
                Segment::FIXED_VALUE);
    }

    rewriter->Finish(convreq, &segments);
  }
}

TEST_F(UserSegmentHistoryRewriterTest, Revert) {
  Segments segments;
  std::unique_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());

  {
    InitSegments(&segments, 1, 2);
    segments.mutable_segment(0)->set_key("abc");
    Segment::Candidate *candidate =
        segments.mutable_segment(0)->mutable_candidate(1);
    candidate->value = "ａｂｃ";
    candidate->content_value = "ａｂｃ";
    candidate->content_key = "abc";
    segments.mutable_segment(0)->move_candidate(1, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    const ConversionRequest convreq = CreateConversionRequest();
    rewriter->Finish(convreq, &segments);
  }

  rewriter->Revert(&segments);

  {
    segments.Clear();
    InitSegments(&segments, 1, 2);
    segments.mutable_segment(0)->set_key("abc");
    Segment::Candidate *candidate =
        segments.mutable_segment(0)->mutable_candidate(1);
    candidate->value = "ａｂｃ";
    candidate->content_value = "ａｂｃ";
    candidate->content_key = "abc";
    candidate->content_key = "abc";
    const ConversionRequest convreq = CreateConversionRequest();
    EXPECT_FALSE(rewriter->Rewrite(convreq, &segments));
  }
}

}  // namespace
}  // namespace mozc
