// Copyright 2010-2013, Google Inc.
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

#include <string>

#include "base/clock_mock.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/number_util.h"
#include "base/system_util.h"
#include "base/util.h"
#include "config/character_form_manager.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/conversion_request.h"
#include "converter/segments.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/pos_group.h"
#include "dictionary/pos_matcher.h"
#include "rewriter/number_rewriter.h"
#include "rewriter/variants_rewriter.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace {

using config::CharacterFormManager;
using config::Config;
using config::ConfigHandler;

const size_t kCandidatesSize = 20;

void InitSegments(Segments *segments, size_t size,
                  size_t candidate_size) {
  segments->Clear();
  for (size_t i = 0; i < size; ++i) {
    Segment *segment = segments->add_segment();
    CHECK(segment);
    segment->set_key(string("segment") + NumberUtil::SimpleItoa(i));
    for (size_t j = 0; j < candidate_size; ++j) {
      Segment::Candidate *c = segment->add_candidate();
      c->content_key = segment->key();
      c->content_value = string("candidate") + NumberUtil::SimpleItoa(j);
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
                           const string &suffix,
                           uint16 lid, uint16 rid) {
  segment->set_key(segment->key() + suffix);
  segment->mutable_candidate(index)->value += suffix;
  segment->mutable_candidate(index)->lid = lid;
  segment->mutable_candidate(index)->rid = rid;
}

void AppendCandidateSuffixWithLid(Segment *segment, size_t index,
                                  const string &suffix,
                                  uint16 lid) {
  // if lid == 0 and rid == 0, we assume that candidate is t13n.
  // we set 1 as rid to avoid this.
  AppendCandidateSuffix(segment, index, suffix, lid, 1);
}

void SetIncognito(bool incognito) {
  Config input;
  ConfigHandler::GetConfig(&input);
  input.set_incognito_mode(incognito);
  ConfigHandler::SetConfig(input);
  EXPECT_EQ(incognito, GET_CONFIG(incognito_mode));
}

void SetLearningLevel(Config::HistoryLearningLevel level) {
  Config input;
  ConfigHandler::GetConfig(&input);
  input.set_history_learning_level(level);
  ConfigHandler::SetConfig(input);
  EXPECT_EQ(level, GET_CONFIG(history_learning_level));
}

void SetNumberForm(Config::CharacterForm form) {
  Config input;
  ConfigHandler::GetConfig(&input);
  for (size_t i = 0; i < input.character_form_rules_size(); ++i) {
    Config::CharacterFormRule *rule =
        input.mutable_character_form_rules(i);
    if (rule->group() == "0") {
      rule->set_conversion_character_form(form);
    }
  }
  ConfigHandler::SetConfig(input);
  CharacterFormManager::GetCharacterFormManager()->Reload();
  EXPECT_EQ(form,
            CharacterFormManager::GetCharacterFormManager()->
            GetConversionCharacterForm("0"));
}
}  // namespace

class UserSegmentHistoryRewriterTest : public ::testing::Test {
 protected:
  UserSegmentHistoryRewriterTest() {}

  virtual void SetUp() {
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);

    Config config;
    ConfigHandler::GetDefaultConfig(&config);
    for (int i = 0; i < config.character_form_rules_size(); ++i) {
      Config::CharacterFormRule *rule =
          config.mutable_character_form_rules(i);
      if (rule->group() == "0" || rule->group() == "A" ||
          rule->group() == "(){}[]") {
        rule->set_preedit_character_form(Config::HALF_WIDTH);
        rule->set_conversion_character_form(Config::HALF_WIDTH);
      }
    }
    ConfigHandler::SetConfig(config);
    CharacterFormManager::GetCharacterFormManager()->Reload();

    Util::SetClockHandler(NULL);

    testing::MockDataManager data_manager;
    pos_matcher_ = data_manager.GetPOSMatcher();
    pos_group_.reset(new PosGroup(data_manager.GetPosGroupData()));
    ASSERT_TRUE(pos_matcher_ != NULL);
    ASSERT_TRUE(pos_group_.get() != NULL);
  }

  virtual void TearDown() {
    Util::SetClockHandler(NULL);

    scoped_ptr<UserSegmentHistoryRewriter> rewriter(
        CreateUserSegmentHistoryRewriter());
    rewriter->Clear();
    // reset config of test_tmpdir.
    Config config;
    ConfigHandler::GetDefaultConfig(&config);
    ConfigHandler::SetConfig(config);
    CharacterFormManager::GetCharacterFormManager()->SetDefaultRule();
  }

  const POSMatcher &pos_matcher() const {
    return *pos_matcher_;
  }

  NumberRewriter *CreateNumberRewriter() const {
    return new NumberRewriter(pos_matcher_);
  }

  UserSegmentHistoryRewriter *CreateUserSegmentHistoryRewriter() const {
    return new UserSegmentHistoryRewriter(pos_matcher_, pos_group_.get());
  }

 private:
  const POSMatcher *pos_matcher_;
  scoped_ptr<const PosGroup> pos_group_;
  DISALLOW_COPY_AND_ASSIGN(UserSegmentHistoryRewriterTest);
};

TEST_F(UserSegmentHistoryRewriterTest, CreateFile) {
  scoped_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());
  const string history_file = FileUtil::JoinPath(FLAGS_test_tmpdir,
                                                 "/segment.db");
  EXPECT_TRUE(FileUtil::FileExists(history_file));
}

TEST_F(UserSegmentHistoryRewriterTest, InvalidInputsTest) {
  SetLearningLevel(Config::DEFAULT_HISTORY);
  SetIncognito(false);
  Segments segments;
  scoped_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());
  segments.Clear();
  const ConversionRequest default_request;
  EXPECT_FALSE(rewriter->Rewrite(default_request, &segments));
  rewriter->Finish(&segments);
}

TEST_F(UserSegmentHistoryRewriterTest, IncognitoModeTest) {
  SetLearningLevel(Config::DEFAULT_HISTORY);
  Segments segments;
  scoped_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());
  const ConversionRequest request;

  {
    SetIncognito(false);
    InitSegments(&segments, 1);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes
        |= Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(&segments);
    InitSegments(&segments, 1);
    rewriter->Rewrite(request, &segments);
    EXPECT_EQ("candidate2",
              segments.segment(0).candidate(0).value);

    SetIncognito(true);
    InitSegments(&segments, 1);
    rewriter->Rewrite(request, &segments);
    EXPECT_EQ("candidate0",
              segments.segment(0).candidate(0).value);
  }

  {
    rewriter->Clear();   // clear history
    SetIncognito(true);
    InitSegments(&segments, 1);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes
        |= Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(&segments);
    InitSegments(&segments, 1);
    rewriter->Rewrite(request, &segments);
    EXPECT_EQ("candidate0",
              segments.segment(0).candidate(0).value);
  }
}

TEST_F(UserSegmentHistoryRewriterTest, ConfigTest) {
  SetIncognito(false);
  Segments segments;
  scoped_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());
  const ConversionRequest request;

  {
    SetLearningLevel(Config::DEFAULT_HISTORY);
    InitSegments(&segments, 1);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes
        |= Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(&segments);
    InitSegments(&segments, 1);
    rewriter->Rewrite(request, &segments);
    EXPECT_EQ("candidate2",
              segments.segment(0).candidate(0).value);

    SetLearningLevel(Config::NO_HISTORY);
    InitSegments(&segments, 1);
    rewriter->Rewrite(request, &segments);
    EXPECT_EQ("candidate0",
              segments.segment(0).candidate(0).value);

    SetLearningLevel(Config::READ_ONLY);
    InitSegments(&segments, 1);
    rewriter->Rewrite(request, &segments);
    EXPECT_EQ("candidate2",
              segments.segment(0).candidate(0).value);
  }

  {
    SetLearningLevel(Config::NO_HISTORY);
    InitSegments(&segments, 1);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes
        |= Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(&segments);
    InitSegments(&segments, 1);
    rewriter->Rewrite(request, &segments);
    EXPECT_EQ("candidate0",
              segments.segment(0).candidate(0).value);
  }
}

TEST_F(UserSegmentHistoryRewriterTest, DisableTest) {
  SetIncognito(false);
  SetLearningLevel(Config::DEFAULT_HISTORY);
  Segments segments;
  scoped_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());
  const ConversionRequest request;

  {
    InitSegments(&segments, 1);
    segments.set_user_history_enabled(true);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes
        |= Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(&segments);
    InitSegments(&segments, 1);
    rewriter->Rewrite(request, &segments);
    EXPECT_EQ("candidate2",
              segments.segment(0).candidate(0).value);

    InitSegments(&segments, 1);
    segments.set_user_history_enabled(false);
    rewriter->Rewrite(request, &segments);
    EXPECT_EQ("candidate0",
              segments.segment(0).candidate(0).value);

    InitSegments(&segments, 1);
    segments.set_user_history_enabled(true);
    rewriter->Rewrite(request, &segments);
    EXPECT_EQ("candidate2",
              segments.segment(0).candidate(0).value);
  }

  {
    InitSegments(&segments, 1);
    segments.set_user_history_enabled(false);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes
        |= Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(&segments);
    InitSegments(&segments, 1);
    rewriter->Rewrite(request, &segments);
    segments.set_user_history_enabled(true);
    EXPECT_EQ("candidate0",
              segments.segment(0).candidate(0).value);
  }
}

TEST_F(UserSegmentHistoryRewriterTest, BasicTest) {
  SetLearningLevel(Config::DEFAULT_HISTORY);
  Segments segments;
  scoped_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());
  const ConversionRequest request;

  rewriter->Clear();

  {
    InitSegments(&segments, 2);

    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes
        |= Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(&segments);
    InitSegments(&segments, 2);
    rewriter->Rewrite(request, &segments);

    EXPECT_EQ("candidate2",
              segments.segment(0).candidate(0).value);
    EXPECT_EQ("candidate0",
              segments.segment(1).candidate(0).value);

    InitSegments(&segments, 1);
    rewriter->Rewrite(request, &segments);
    EXPECT_EQ("candidate2",
              segments.segment(0).candidate(0).value);
    segments.mutable_segment(0)->move_candidate(1, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes
        |= Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(&segments);

    InitSegments(&segments, 1);
    rewriter->Rewrite(request, &segments);

    EXPECT_EQ("candidate0",
              segments.segment(0).candidate(0).value);

    InitSegments(&segments, 2);
    rewriter->Rewrite(request, &segments);
    EXPECT_EQ("candidate2",
              segments.segment(0).candidate(0).value);
    EXPECT_EQ("candidate0",
              segments.segment(1).candidate(0).value);
    segments.mutable_segment(1)->move_candidate(3, 0);
    segments.mutable_segment(1)->mutable_candidate(0)->attributes
        |= Segment::Candidate::RERANKED;
    segments.mutable_segment(1)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(&segments);

    InitSegments(&segments, 2);
    rewriter->Rewrite(request, &segments);
    EXPECT_EQ("candidate2",
              segments.segment(0).candidate(0).value);
    EXPECT_EQ("candidate3",
              segments.segment(1).candidate(0).value);
  }

  rewriter->Clear();
  {
    InitSegments(&segments, 2);

    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes
        |= Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(&segments);
    InitSegments(&segments, 2);
    rewriter->Rewrite(request, &segments);

    EXPECT_EQ("candidate2",
              segments.segment(0).candidate(0).value);
    EXPECT_EQ("candidate0",
              segments.segment(1).candidate(0).value);

    InitSegments(&segments, 1);
    rewriter->Rewrite(request, &segments);
    EXPECT_EQ("candidate2",
              segments.segment(0).candidate(0).value);

    // back to the orignal
    segments.mutable_segment(0)->move_candidate(1, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes
        |= Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(&segments);

    InitSegments(&segments, 1);
    rewriter->Rewrite(request, &segments);
    EXPECT_EQ("candidate0",
              segments.segment(0).candidate(0).value);
  }
}

// Test for Issue 2155278
TEST_F(UserSegmentHistoryRewriterTest, SequenceTest) {
  SetLearningLevel(Config::DEFAULT_HISTORY);
  Segments segments;
  scoped_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());
  const ConversionRequest request;

  rewriter->Clear();

  const uint64 kSeconds = 0;
  const uint32 kMicroSeconds = 0;
  ClockMock clock(kSeconds, kMicroSeconds);
  Util::SetClockHandler(&clock);

  {
    InitSegments(&segments, 1);

    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes
        |= Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    EXPECT_EQ("candidate2",
              segments.segment(0).candidate(0).value);
    rewriter->Finish(&segments);  // learn "candidate2"

    // Next timestamp of learning should be newer than previous learning.
    clock.PutClockForward(1, 0);

    InitSegments(&segments, 2);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);
    segments.mutable_segment(1)->set_key(segments.segment(0).key());
    EXPECT_EQ(1, segments.history_segments_size());
    rewriter->Rewrite(request, &segments);

    EXPECT_EQ("candidate2",
              segments.segment(0).candidate(0).value);
    EXPECT_EQ("candidate2",
              segments.segment(1).candidate(0).value);
    // 2 0 1 3 4 ..

    segments.mutable_segment(1)->move_candidate(3, 0);
    segments.mutable_segment(1)->mutable_candidate(0)->attributes
        |= Segment::Candidate::RERANKED;
    segments.mutable_segment(1)->set_segment_type(Segment::FIXED_VALUE);
    EXPECT_EQ("candidate3",
              segments.segment(1).candidate(0).value);
    rewriter->Finish(&segments);  // learn "candidate3"

    clock.PutClockForward(1, 0);

    InitSegments(&segments, 3);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);
    segments.mutable_segment(1)->move_candidate(3, 0);
    segments.mutable_segment(1)->set_segment_type(Segment::HISTORY);
    segments.mutable_segment(1)->set_key(segments.segment(0).key());
    segments.mutable_segment(2)->set_key(segments.segment(0).key());
    EXPECT_EQ(2, segments.history_segments_size());
    rewriter->Rewrite(request, &segments);

    EXPECT_EQ("candidate2",
              segments.segment(0).candidate(0).value);
    EXPECT_EQ("candidate3",
              segments.segment(1).candidate(0).value);
    EXPECT_EQ("candidate3",
              segments.segment(2).candidate(0).value);
    // 3 2 0 1 4 ..

    segments.mutable_segment(2)->move_candidate(1, 0);
    segments.mutable_segment(2)->mutable_candidate(0)->attributes
        |= Segment::Candidate::RERANKED;
    segments.mutable_segment(2)->set_segment_type(Segment::FIXED_VALUE);
    EXPECT_EQ("candidate2",
              segments.segment(2).candidate(0).value);
    rewriter->Finish(&segments);  // learn "candidate2"

    clock.PutClockForward(1, 0);

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
    EXPECT_EQ(3, segments.history_segments_size());
    rewriter->Rewrite(request, &segments);

    EXPECT_EQ("candidate2",
              segments.segment(0).candidate(0).value);
    EXPECT_EQ("candidate3",
              segments.segment(1).candidate(0).value);
    EXPECT_EQ("candidate2",
              segments.segment(2).candidate(0).value);
    EXPECT_EQ("candidate2",
              segments.segment(3).candidate(0).value);
    // 2 3 0 1 4 ..
  }
}

TEST_F(UserSegmentHistoryRewriterTest, DupTest) {
  SetLearningLevel(Config::DEFAULT_HISTORY);
  Segments segments;
  scoped_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());
  const ConversionRequest request;

  rewriter->Clear();

  const uint64 kSeconds = 0;
  const uint32 kMicroSeconds = 0;
  ClockMock clock(kSeconds, kMicroSeconds);
  Util::SetClockHandler(&clock);

  {
    InitSegments(&segments, 1);
    segments.mutable_segment(0)->move_candidate(4, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes
        |= Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(&segments);
    InitSegments(&segments, 1);
    rewriter->Rewrite(request, &segments);

    // restored
    // 4,0,1,2,3,5,...
    EXPECT_EQ("candidate4",
              segments.segment(0).candidate(0).value);
    segments.mutable_segment(0)->move_candidate(4, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes
        |= Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    clock.PutClockForward(1, 0);
    rewriter->Finish(&segments);
    InitSegments(&segments, 1);
    rewriter->Rewrite(request, &segments);

    // 3,4,0,1,2,5
    EXPECT_EQ("candidate3",
              segments.segment(0).candidate(0).value);
    EXPECT_EQ("candidate4",
              segments.segment(0).candidate(1).value);
    segments.mutable_segment(0)->move_candidate(4, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes
        |= Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    clock.PutClockForward(1, 0);
    rewriter->Finish(&segments);
    InitSegments(&segments, 1);
    rewriter->Rewrite(request, &segments);

    EXPECT_EQ("candidate2",
              segments.segment(0).candidate(0).value);
    EXPECT_EQ("candidate3",
              segments.segment(0).candidate(1).value);
    EXPECT_EQ("candidate4",
              segments.segment(0).candidate(2).value);
  }
}

TEST_F(UserSegmentHistoryRewriterTest, LearningType) {
  SetLearningLevel(Config::DEFAULT_HISTORY);
  Segments segments;
  scoped_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());
  const ConversionRequest request;

  {
    rewriter->Clear();
    InitSegments(&segments, 1);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes
        |= Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes
        |= Segment::Candidate::NO_LEARNING;
    rewriter->Finish(&segments);
    InitSegments(&segments, 1);
    rewriter->Rewrite(request, &segments);
    EXPECT_EQ("candidate0",
              segments.segment(0).candidate(0).value);
  }

  {
    rewriter->Clear();
    InitSegments(&segments, 1);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes
        |= Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::NO_HISTORY_LEARNING;
    rewriter->Finish(&segments);
    InitSegments(&segments, 1);
    rewriter->Rewrite(request, &segments);
    EXPECT_EQ("candidate0",
              segments.segment(0).candidate(0).value);
  }

  {
    rewriter->Clear();
    InitSegments(&segments, 1);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes
        |= Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::NO_SUGGEST_LEARNING;
    rewriter->Finish(&segments);
    InitSegments(&segments, 1);
    rewriter->Rewrite(request, &segments);
    EXPECT_EQ("candidate2",
              segments.segment(0).candidate(0).value);
  }
}

TEST_F(UserSegmentHistoryRewriterTest, ContextSensitive) {
  SetLearningLevel(Config::DEFAULT_HISTORY);
  Segments segments;
  scoped_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());
  const ConversionRequest request;

  rewriter->Clear();
  {
    InitSegments(&segments, 2);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes
        |= Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes
        |= Segment::Candidate::CONTEXT_SENSITIVE;
    rewriter->Finish(&segments);
    InitSegments(&segments, 2);
    rewriter->Rewrite(request, &segments);

    // fire if two segments
    EXPECT_EQ("candidate2",
              segments.segment(0).candidate(0).value);
    // not fire if single segment
    InitSegments(&segments, 1);
    rewriter->Rewrite(request, &segments);
    EXPECT_EQ("candidate0",
              segments.segment(0).candidate(0).value);
  }

  rewriter->Clear();
  {
    InitSegments(&segments, 1);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes
        |= Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes
        |= Segment::Candidate::CONTEXT_SENSITIVE;
    rewriter->Finish(&segments);

    // fire if even in single segment
    InitSegments(&segments, 1);
    rewriter->Rewrite(request, &segments);
    EXPECT_EQ("candidate2",
              segments.segment(0).candidate(0).value);

    // not fire if two segments
    InitSegments(&segments, 2);
    rewriter->Rewrite(request, &segments);
    EXPECT_EQ("candidate0",
              segments.segment(0).candidate(0).value);
  }
}

TEST_F(UserSegmentHistoryRewriterTest, ContentValueLearning) {
  SetLearningLevel(Config::DEFAULT_HISTORY);
  Segments segments;
  scoped_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());
  const ConversionRequest request;

  rewriter->Clear();
  {
    InitSegments(&segments, 1);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 0, ":all", 0);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 2, ":all", 0);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes
        |= Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);

    rewriter->Finish(&segments);

    InitSegments(&segments, 1);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 0, ":all", 0);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 2, ":all", 0);

    rewriter->Rewrite(request, &segments);

    // exact match
    EXPECT_EQ("candidate2:all",
              segments.segment(0).candidate(0).value);

    InitSegments(&segments, 1);
    rewriter->Rewrite(request, &segments);

    // content value only:
    // in both learning/applying phase, lid and suffix are the same
    // as those of top candidates.
    EXPECT_EQ("candidate2",
              segments.segment(0).candidate(0).value);

    InitSegments(&segments, 1);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 0, ":other", 0);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 2, ":other", 0);

    rewriter->Rewrite(request, &segments);

    EXPECT_EQ("candidate2:other",
              segments.segment(0).candidate(0).value);
  }

  // In learning phase, lid is different
  rewriter->Clear();
  {
    InitSegments(&segments, 1);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 0, ":all", 0);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 2, ":all", 1);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes
        |= Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(&segments);
    InitSegments(&segments, 1);
    rewriter->Rewrite(request, &segments);
    EXPECT_EQ("candidate0",
              segments.segment(0).candidate(0).value);
  }

  // In learning phase, suffix (functional value) is different
  rewriter->Clear();
  {
    InitSegments(&segments, 1);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 0, "", 0);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 2, ":other", 1);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes
        |= Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(&segments);
    InitSegments(&segments, 1);
    rewriter->Rewrite(request, &segments);
    EXPECT_EQ("candidate0",
              segments.segment(0).candidate(0).value);
  }

  // In apply phase, lid is different
  rewriter->Clear();
  {
    InitSegments(&segments, 1);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 0, ":all", 0);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 2, ":all", 0);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes
        |= Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(&segments);
    InitSegments(&segments, 1);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 0, ":other", 0);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 2, ":other", 1);
    rewriter->Rewrite(request, &segments);
    EXPECT_EQ("candidate0:other",
              segments.segment(0).candidate(0).value);
  }

  // In apply phase, suffix (functional value) is different
  rewriter->Clear();
  {
    InitSegments(&segments, 1);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 0, ":all", 0);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 2, ":all", 0);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes
        |= Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(&segments);
    InitSegments(&segments, 1);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 0, "", 0);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 2, ":other", 0);
    rewriter->Rewrite(request, &segments);
    EXPECT_EQ("candidate0",
              segments.segment(0).candidate(0).value);
  }
}

TEST_F(UserSegmentHistoryRewriterTest, ReplaceableTest) {
  SetLearningLevel(Config::DEFAULT_HISTORY);
  Segments segments;
  scoped_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());
  const ConversionRequest request;

  rewriter->Clear();
  {
    InitSegments(&segments, 2);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 0, ":all", 0);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 2, ":all", 0);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes
        |= Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);

    rewriter->Finish(&segments);

    InitSegments(&segments, 1);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 0, ":all", 0);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 2, ":all", 0);

    rewriter->Rewrite(request, &segments);

    EXPECT_EQ("candidate2:all",
              segments.segment(0).candidate(0).value);

    InitSegments(&segments, 1);
    rewriter->Rewrite(request, &segments);
  }

  rewriter->Clear();
  {
    InitSegments(&segments, 1);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 0, ":all", 0);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 2, ":all", 0);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes
        |= Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);

    rewriter->Finish(&segments);

    InitSegments(&segments, 2);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 0, ":all", 0);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 2, ":all", 0);

    rewriter->Rewrite(request, &segments);

    EXPECT_EQ("candidate2:all",
              segments.segment(0).candidate(0).value);

    InitSegments(&segments, 1);
    rewriter->Rewrite(request, &segments);
  }

  rewriter->Clear();
  {
    InitSegments(&segments, 2);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 0, ":all", 0);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 2, ":all", 1);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes
        |= Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(&segments);
    InitSegments(&segments, 1);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 0, ":all", 0);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 2, ":all", 0);
    rewriter->Rewrite(request, &segments);
    EXPECT_EQ("candidate0:all",
              segments.segment(0).candidate(0).value);
  }

  rewriter->Clear();
  {
    InitSegments(&segments, 2);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 0, ":all", 0);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 2, ":all", 0);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes
        |= Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(&segments);
    InitSegments(&segments, 1);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 0, ":all", 0);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 2, ":all", 1);
    rewriter->Rewrite(request, &segments);
    EXPECT_EQ("candidate0:all",
              segments.segment(0).candidate(0).value);
  }

  rewriter->Clear();
  {
    InitSegments(&segments, 1);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 0, ":all", 0);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 2, ":all", 1);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes
        |= Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(&segments);
    InitSegments(&segments, 2);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 0, ":all", 0);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 2, ":all", 0);
    rewriter->Rewrite(request, &segments);
    EXPECT_EQ("candidate0:all",
              segments.segment(0).candidate(0).value);
  }

  rewriter->Clear();
  {
    InitSegments(&segments, 1);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 0, ":all", 0);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 2, ":all", 0);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes
        |= Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(&segments);
    InitSegments(&segments, 2);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 0, ":all", 0);
    AppendCandidateSuffixWithLid(segments.mutable_segment(0), 2, ":all", 1);
    rewriter->Rewrite(request, &segments);
    EXPECT_EQ("candidate0:all",
              segments.segment(0).candidate(0).value);
  }
}

TEST_F(UserSegmentHistoryRewriterTest, NotReplaceableForDifferentId) {
  SetLearningLevel(Config::DEFAULT_HISTORY);
  Segments segments;
  scoped_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());
  const ConversionRequest request;

  rewriter->Clear();
  {
    InitSegments(&segments, 2);
    AppendCandidateSuffix(segments.mutable_segment(0), 0, ":all", 1, 1);
    AppendCandidateSuffix(segments.mutable_segment(0), 2, ":all", 200, 300);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes
        |= Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);

    rewriter->Finish(&segments);

    InitSegments(&segments, 2);
    AppendCandidateSuffix(segments.mutable_segment(0), 0, ":all", 1, 1);
    AppendCandidateSuffix(segments.mutable_segment(0), 2, ":all", 200, 300);
    segments.mutable_segment(1)->mutable_candidate(0)->value =
        "not_same_as_before";

    rewriter->Rewrite(request, &segments);

    EXPECT_NE("candidate2:all",
              segments.segment(0).candidate(0).value);
  }
}

TEST_F(UserSegmentHistoryRewriterTest, ReplaceableForSameId) {
  SetLearningLevel(Config::DEFAULT_HISTORY);
  Segments segments;
  scoped_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());
  const ConversionRequest request;

  rewriter->Clear();
  {
    InitSegments(&segments, 2);
    AppendCandidateSuffix(segments.mutable_segment(0), 0, ":all", 1, 1);
    AppendCandidateSuffix(segments.mutable_segment(0), 2, ":all", 1, 1);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes
        |= Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);

    rewriter->Finish(&segments);

    InitSegments(&segments, 2);
    AppendCandidateSuffix(segments.mutable_segment(0), 0, ":all", 1, 1);
    AppendCandidateSuffix(segments.mutable_segment(0), 2, ":all", 1, 1);
    segments.mutable_segment(1)->mutable_candidate(0)->value =
        "not_same_as_before";

    rewriter->Rewrite(request, &segments);

    EXPECT_EQ("candidate2:all",
              segments.segment(0).candidate(0).value);
  }
}

TEST_F(UserSegmentHistoryRewriterTest, ReplaceableT13NTest) {
  SetLearningLevel(Config::DEFAULT_HISTORY);
  Segments segments;
  scoped_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());
  const ConversionRequest request;

  rewriter->Clear();
  {
    InitSegments(&segments, 2);
    AppendCandidateSuffix(segments.mutable_segment(0), 0, ":all", 1, 1);
    // Prepare candidate2 as T13N candidate.
    AppendCandidateSuffix(segments.mutable_segment(0), 2, ":all", 0, 0);
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes
        |= Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);

    rewriter->Finish(&segments);

    InitSegments(&segments, 2);
    AppendCandidateSuffix(segments.mutable_segment(0), 0, ":all", 1, 1);
    AppendCandidateSuffix(segments.mutable_segment(0), 2, ":all", 0, 0);
    segments.mutable_segment(1)->mutable_candidate(0)->value =
        "not_same_as_before";

    rewriter->Rewrite(request, &segments);

    EXPECT_EQ("candidate2:all",
              segments.segment(0).candidate(0).value);
  }
}

TEST_F(UserSegmentHistoryRewriterTest, LeftRightNumber) {
  SetLearningLevel(Config::DEFAULT_HISTORY);
  Segments segments;
  scoped_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());
  const ConversionRequest request;

  rewriter->Clear();

  {
    InitSegments(&segments, 2);

    segments.mutable_segment(0)->mutable_candidate(0)->value = "1234";
    segments.mutable_segment(1)->move_candidate(2, 0);
    segments.mutable_segment(1)->mutable_candidate(0)->attributes
        |= Segment::Candidate::RERANKED;
    segments.mutable_segment(1)->mutable_candidate(0)->attributes
        |= Segment::Candidate::CONTEXT_SENSITIVE;
    segments.mutable_segment(1)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(&segments);
    EXPECT_EQ("1234",
              segments.segment(0).candidate(0).value);
    EXPECT_EQ("candidate2",
              segments.segment(1).candidate(0).value);

    InitSegments(&segments, 2);
    // different num.
    segments.mutable_segment(0)->mutable_candidate(0)->value = "5678";
    rewriter->Rewrite(request, &segments);
    EXPECT_EQ("5678",
              segments.segment(0).candidate(0).value);
    EXPECT_EQ("candidate2",
              segments.segment(1).candidate(0).value);
  }

  {
    InitSegments(&segments, 2);

    segments.mutable_segment(1)->mutable_candidate(0)->value = "1234";
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes
        |= Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->mutable_candidate(0)->attributes
        |= Segment::Candidate::CONTEXT_SENSITIVE;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(&segments);
    EXPECT_EQ("candidate2",
              segments.segment(0).candidate(0).value);
    EXPECT_EQ("1234",
              segments.segment(1).candidate(0).value);

    InitSegments(&segments, 2);
    // different num.
    segments.mutable_segment(1)->mutable_candidate(0)->value = "5678";
    rewriter->Rewrite(request, &segments);
    EXPECT_EQ("candidate2",
              segments.segment(0).candidate(0).value);
    EXPECT_EQ("5678",
              segments.segment(1).candidate(0).value);
  }
}

TEST_F(UserSegmentHistoryRewriterTest, BacketMatching) {
  SetLearningLevel(Config::DEFAULT_HISTORY);
  Segments segments;
  scoped_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());
  const ConversionRequest request;

  rewriter->Clear();

  {
    InitSegments(&segments, 1);
    Segment::Candidate *candidate =
        segments.mutable_segment(0)->insert_candidate(2);
    candidate->value = "(";
    candidate->content_value = "(";
    candidate->content_key = "(";
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes
        |= Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(&segments);
  }

  {
    InitSegments(&segments, 1);
    Segment::Candidate *candidate =
        segments.mutable_segment(0)->insert_candidate(2);
    candidate->value = ")";
    candidate->content_value = ")";
    candidate->content_key = ")";

    rewriter->Rewrite(request, &segments);

    EXPECT_EQ(")",
              segments.segment(0).candidate(0).value);
  }
}

// issue 2262691
TEST_F(UserSegmentHistoryRewriterTest, MultipleLearning) {
  SetLearningLevel(Config::DEFAULT_HISTORY);
  Segments segments;
  scoped_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());
  const ConversionRequest request;

  rewriter->Clear();

  {
    InitSegments(&segments, 1);
    segments.mutable_segment(0)->set_key("key1");
    Segment::Candidate *candidate =
        segments.mutable_segment(0)->insert_candidate(2);
    candidate->value = "value1";
    candidate->content_value = "value1";
    candidate->content_key = "key1";
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes
        |= Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(&segments);
  }

  {
    InitSegments(&segments, 1);
    segments.mutable_segment(0)->set_key("key2");
    Segment::Candidate *candidate =
        segments.mutable_segment(0)->insert_candidate(2);
    candidate->value = "value2";
    candidate->content_value = "value2";
    candidate->content_key = "key2";
    segments.mutable_segment(0)->move_candidate(2, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes
        |= Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(&segments);
  }

  {
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

    rewriter->Rewrite(request, &segments);

    EXPECT_EQ("value1",
              segments.segment(0).candidate(0).value);
  }
}

TEST_F(UserSegmentHistoryRewriterTest, NumberSpecial) {
  SetLearningLevel(Config::DEFAULT_HISTORY);
  Segments segments;
  scoped_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());
  scoped_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());
  const ConversionRequest request;

  rewriter->Clear();

  {
    segments.Clear();
    segments.add_segment();
    segments.mutable_segment(0)->set_key("12");
    Segment::Candidate *candidate =
        segments.mutable_segment(0)->insert_candidate(0);
    candidate->value = "\xE2\x91\xAB";  // circled 12
    candidate->content_value = "\xE2\x91\xAB";
    candidate->content_key = "12";
    candidate->lid = pos_matcher().GetNumberId();
    candidate->rid = pos_matcher().GetNumberId();
    candidate->style = NumberUtil::NumberString::NUMBER_CIRCLED;
    segments.mutable_segment(0)->mutable_candidate(0)->attributes
        |= Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(&segments);
  }

  {
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
    EXPECT_TRUE(number_rewriter->Rewrite(request, &segments));
    rewriter->Rewrite(request, &segments);

    EXPECT_EQ("\xE2\x91\xAD",  // circled 14
              segments.segment(0).candidate(0).value);
  }
}

TEST_F(UserSegmentHistoryRewriterTest, NumberHalfWidth) {
  SetLearningLevel(Config::DEFAULT_HISTORY);
  SetNumberForm(Config::HALF_WIDTH);
  Segments segments;
  scoped_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());
  scoped_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());
  const ConversionRequest request;

  rewriter->Clear();

  {
    segments.Clear();
    segments.add_segment();
    segments.mutable_segment(0)->set_key("1234");
    Segment::Candidate *candidate =
        segments.mutable_segment(0)->insert_candidate(0);
    // "１，２３４"
    candidate->value =
        "\xEF\xBC\x91\xEF\xBC\x8C\xEF\xBC\x92\xEF\xBC\x93\xEF\xBC\x94";
    candidate->content_value =
        "\xEF\xBC\x91\xEF\xBC\x8C\xEF\xBC\x92\xEF\xBC\x93\xEF\xBC\x94";
    candidate->content_key = "1234";
    candidate->lid = pos_matcher().GetNumberId();
    candidate->rid = pos_matcher().GetNumberId();
    candidate->style =
        NumberUtil::NumberString::NUMBER_SEPARATED_ARABIC_FULLWIDTH;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(&segments);  // full-width for separated number
  }

  {
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

    EXPECT_TRUE(number_rewriter->Rewrite(request, &segments));
    rewriter->Rewrite(request, &segments);

    EXPECT_EQ("1,234",
              segments.segment(0).candidate(0).value);
  }
}

TEST_F(UserSegmentHistoryRewriterTest, NumberFullWidth) {
  SetLearningLevel(Config::DEFAULT_HISTORY);
  SetNumberForm(Config::FULL_WIDTH);
  Segments segments;
  scoped_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());
  scoped_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());
  const ConversionRequest request;

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
    rewriter->Finish(&segments);  // half-width for separated number
  }

  {
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
    EXPECT_TRUE(number_rewriter->Rewrite(request, &segments));
    rewriter->Rewrite(request, &segments);

    // "１，２３４"
    EXPECT_EQ("\xEF\xBC\x91\xEF\xBC\x8C\xEF\xBC\x92\xEF\xBC\x93\xEF\xBC\x94",
              segments.segment(0).candidate(0).value);
  }
}

TEST_F(UserSegmentHistoryRewriterTest, NumberNoSeparated) {
  SetLearningLevel(Config::DEFAULT_HISTORY);
  SetNumberForm(Config::HALF_WIDTH);
  Segments segments;
  scoped_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());
  scoped_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());
  const ConversionRequest request;

  rewriter->Clear();

  {
    segments.Clear();
    segments.add_segment();
    segments.mutable_segment(0)->set_key("10");
    Segment::Candidate *candidate =
        segments.mutable_segment(0)->insert_candidate(0);
    candidate->value = "\xe5\x8d\x81";  // "十"
    candidate->content_value = "\xe5\x8d\x81";
    candidate->content_key = "10";
    candidate->lid = pos_matcher().GetNumberId();
    candidate->rid = pos_matcher().GetNumberId();
    candidate->style = NumberUtil::NumberString::NUMBER_KANJI;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(&segments);  // learn kanji
  }
  {
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
    rewriter->Finish(&segments);  // learn kanji
  }

  {
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
    EXPECT_TRUE(number_rewriter->Rewrite(request, &segments));
    rewriter->Rewrite(request, &segments);

    // 9, not "九"
    EXPECT_EQ("9", segments.segment(0).candidate(0).value);
  }
}

TEST_F(UserSegmentHistoryRewriterTest, Regression2459519) {
  SetLearningLevel(Config::DEFAULT_HISTORY);
  Segments segments;
  scoped_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());
  const ConversionRequest request;

  rewriter->Clear();

  const uint64 kSeconds = 0;
  const uint32 kMicroSeconds = 0;
  ClockMock clock(kSeconds, kMicroSeconds);
  Util::SetClockHandler(&clock);

  InitSegments(&segments, 1);
  segments.mutable_segment(0)->move_candidate(2, 0);
  segments.mutable_segment(0)->mutable_candidate(0)->attributes
      |= Segment::Candidate::RERANKED;
  segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
  rewriter->Finish(&segments);

  InitSegments(&segments, 1);
  rewriter->Rewrite(request, &segments);
  EXPECT_EQ("candidate2",
            segments.segment(0).candidate(0).value);
  EXPECT_EQ("candidate0",
            segments.segment(0).candidate(1).value);

  segments.mutable_segment(0)->move_candidate(1, 0);
  segments.mutable_segment(0)->mutable_candidate(0)->attributes
      |= Segment::Candidate::RERANKED;
  segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
  clock.PutClockForward(1, 0);
  rewriter->Finish(&segments);

  InitSegments(&segments, 1);
  rewriter->Rewrite(request, &segments);
  EXPECT_EQ("candidate0",
            segments.segment(0).candidate(0).value);
  EXPECT_EQ("candidate2",
            segments.segment(0).candidate(1).value);

  segments.mutable_segment(0)->move_candidate(1, 0);
  segments.mutable_segment(0)->mutable_candidate(0)->attributes
      |= Segment::Candidate::RERANKED;
  segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
  clock.PutClockForward(1, 0);
  rewriter->Finish(&segments);

  InitSegments(&segments, 1);
  rewriter->Rewrite(request, &segments);
  EXPECT_EQ("candidate2",
            segments.segment(0).candidate(0).value);
  EXPECT_EQ("candidate0",
            segments.segment(0).candidate(1).value);
}

TEST_F(UserSegmentHistoryRewriterTest, Regression2459520) {
  SetLearningLevel(Config::DEFAULT_HISTORY);
  Segments segments;
  scoped_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());
  const ConversionRequest request;

  rewriter->Clear();

  InitSegments(&segments, 2);
  segments.mutable_segment(0)->move_candidate(2, 0);
  segments.mutable_segment(0)->mutable_candidate(0)->attributes
      |= Segment::Candidate::RERANKED;
  segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);

  segments.mutable_segment(1)->move_candidate(3, 0);
  segments.mutable_segment(1)->mutable_candidate(0)->attributes
      |= Segment::Candidate::RERANKED;
  segments.mutable_segment(1)->set_segment_type(Segment::FIXED_VALUE);
  rewriter->Finish(&segments);

  InitSegments(&segments, 2);
  rewriter->Rewrite(request, &segments);
  EXPECT_EQ("candidate2",
            segments.segment(0).candidate(0).value);
  EXPECT_EQ("candidate3",
            segments.segment(1).candidate(0).value);
}

TEST_F(UserSegmentHistoryRewriterTest, PuntuationsTest) {
  SetLearningLevel(Config::DEFAULT_HISTORY);
  Segments segments;
  scoped_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());
  const ConversionRequest request;

  const uint16 id = pos_matcher().GetJapanesePunctuationsId();

  rewriter->Clear();

  InitSegments(&segments, 2);
  segments.mutable_segment(1)->set_key(".");
  for (int i = 1; i < kCandidatesSize; ++i) {
    segments.mutable_segment(1)->mutable_candidate(i)->lid = id;
    segments.mutable_segment(1)->mutable_candidate(i)->rid = id;
    segments.mutable_segment(1)->mutable_candidate(i)->value = ".";
  }
  segments.mutable_segment(1)->move_candidate(2, 0);
  segments.mutable_segment(1)->mutable_candidate(0)->attributes
      |= Segment::Candidate::RERANKED;
  segments.mutable_segment(1)->set_segment_type(Segment::FIXED_VALUE);
  rewriter->Finish(&segments);

  InitSegments(&segments, 2);
  segments.mutable_segment(1)->set_key(".");
  for (int i = 1; i < kCandidatesSize; ++i) {
    segments.mutable_segment(1)->mutable_candidate(i)->lid = id;
    segments.mutable_segment(1)->mutable_candidate(i)->rid = id;
    segments.mutable_segment(1)->mutable_candidate(i)->value = ".";
  }

  // Punctuation is not remembered
  rewriter->Rewrite(request, &segments);
  EXPECT_EQ("candidate0", segments.segment(1).candidate(0).value);
}

TEST_F(UserSegmentHistoryRewriterTest, Regression3264619) {
  SetLearningLevel(Config::DEFAULT_HISTORY);
  Segments segments;
  scoped_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());
  const ConversionRequest request;

  // Too big candidates
  InitSegments(&segments, 2, 1024);
  segments.mutable_segment(0)->move_candidate(512, 0);
  segments.mutable_segment(0)->mutable_candidate(0)->attributes
      |= Segment::Candidate::RERANKED;
  segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
  rewriter->Finish(&segments);
  InitSegments(&segments, 2, 1024);
  rewriter->Rewrite(request, &segments);

  EXPECT_EQ("candidate512",
            segments.segment(0).candidate(0).value);
  EXPECT_EQ("candidate0",
            segments.segment(1).candidate(0).value);
}

TEST_F(UserSegmentHistoryRewriterTest, RandomTest) {
  SetLearningLevel(Config::DEFAULT_HISTORY);
  Segments segments;
  scoped_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());
  const ConversionRequest request;

  const uint64 kSeconds = 0;
  const uint32 kMicroSeconds = 0;
  ClockMock clock(kSeconds, kMicroSeconds);
  Util::SetClockHandler(&clock);

  rewriter->Clear();
  for (int i = 0; i < 5; ++i) {
    InitSegments(&segments, 1);
    const int n = Util::Random(10);
    const string expected = segments.segment(0).candidate(n).value;
    segments.mutable_segment(0)->move_candidate(n, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes
        |= Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    EXPECT_EQ(expected,
              segments.segment(0).candidate(0).value);
    rewriter->Finish(&segments);
    InitSegments(&segments, 1);
    rewriter->Rewrite(request, &segments);
    EXPECT_EQ(expected,
              segments.segment(0).candidate(0).value);
    clock.PutClockForward(1, 0);  // update LRU timer
  }
}

TEST_F(UserSegmentHistoryRewriterTest, AnnotationAfterLearning) {
  SetLearningLevel(Config::DEFAULT_HISTORY);
  Segments segments;
  scoped_ptr<UserSegmentHistoryRewriter> rewriter(
      CreateUserSegmentHistoryRewriter());
  const ConversionRequest request;

  {
    segments.Clear();
    InitSegments(&segments, 1, 2);
    segments.mutable_segment(0)->set_key("abc");
    Segment::Candidate *candidate =
        segments.mutable_segment(0)->mutable_candidate(1);
    // "ａｂｃ"
    candidate->value = "\xEF\xBD\x81\xEF\xBD\x82\xEF\xBD\x83";
    // "ａｂｃ"
    candidate->content_value = "\xEF\xBD\x81\xEF\xBD\x82\xEF\xBD\x83";
    candidate->content_key = "abc";
    // "[全] アルファベット"
    candidate->description = "[\xE5\x85\xA8] \xE3\x82\xA2\xE3\x83\xAB"
        "\xE3\x83\x95\xE3\x82\xA1\xE3\x83\x99\xE3\x83\x83\xE3\x83\x88";
    segments.mutable_segment(0)->move_candidate(1, 0);
    segments.mutable_segment(0)->mutable_candidate(0)->attributes
        |= Segment::Candidate::RERANKED;
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    rewriter->Finish(&segments);
  }

  {
    segments.Clear();
    InitSegments(&segments, 1, 2);
    segments.mutable_segment(0)->set_key("abc");
    Segment::Candidate *candidate =
        segments.mutable_segment(0)->mutable_candidate(1);
    // "ａｂｃ"
    candidate->value = "\xEF\xBD\x81\xEF\xBD\x82\xEF\xBD\x83";
    // "ａｂｃ"
    candidate->content_value = "\xEF\xBD\x81\xEF\xBD\x82\xEF\xBD\x83";
    candidate->content_key = "abc";
    // "[全] アルファベット"
    candidate->description = "[\xE5\x85\xA8]\xE3\x82\xA2\xE3\x83\xAB"
        "\xE3\x83\x95\xE3\x82\xA1\xE3\x83\x99\xE3\x83\x83\xE3\x83\x88";
    candidate->content_key = "abc";
    rewriter->Rewrite(request, &segments);
    EXPECT_EQ("abc", segments.segment(0).candidate(0).content_value);
    // "[半] アルファベット"
    string expectation = VariantsRewriter::kHalfWidth;
    const string alphabet = VariantsRewriter::kAlphabet;
    if (alphabet.size() != 0) {
      expectation += ' ' + alphabet;
    }
    EXPECT_EQ(expectation,
              segments.segment(0).candidate(0).description);
    rewriter->Finish(&segments);
  }
}
}  // namespace mozc
