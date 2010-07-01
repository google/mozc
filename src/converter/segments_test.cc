// Copyright 2010, Google Inc.
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

#include "base/util.h"
#include "transliteration/transliteration.h"
#include "converter/character_form_manager.h"
#include "converter/segments.h"
#include "session/config.pb.h"
#include "session/config_handler.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {

class SegmentsTest : public testing::Test {
 protected:
  virtual void SetUp() {
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
    config::ConfigHandler::GetDefaultConfig(&default_config_);
    config::ConfigHandler::SetConfig(default_config_);
  }

  virtual void TearDown() {
    config::ConfigHandler::SetConfig(default_config_);
  }
 private:
  config::Config default_config_;
};

class CandidateTest : public testing::Test {
 protected:
  virtual void SetUp() {
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
    config::ConfigHandler::GetDefaultConfig(&default_config_);
    config::ConfigHandler::SetConfig(default_config_);
  }

 private:
  config::Config default_config_;
};

class SegmentTest : public testing::Test {
 protected:
  virtual void SetUp() {
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
    config::ConfigHandler::GetDefaultConfig(&default_config_);
    config::ConfigHandler::SetConfig(default_config_);
  }

  virtual void TearDown() {
    config::ConfigHandler::SetConfig(default_config_);
  }
 private:
  config::Config default_config_;
};

TEST_F(SegmentsTest, BasicTest) {
  Segments segments;

  // flags
  segments.set_request_type(Segments::CONVERSION);
  EXPECT_EQ(Segments::CONVERSION, segments.request_type());

  segments.set_request_type(Segments::SUGGESTION);
  EXPECT_EQ(Segments::SUGGESTION, segments.request_type());

  segments.enable_user_history();
  EXPECT_TRUE(segments.use_user_history());

  segments.disable_user_history();
  EXPECT_FALSE(segments.use_user_history());

  EXPECT_EQ(0, segments.segments_size());
  EXPECT_TRUE(NULL != segments.lattice());

  const int kSegmentsSize = 5;
  Segment *seg[kSegmentsSize];
  for (int i = 0; i < kSegmentsSize; ++i) {
    EXPECT_EQ(i, segments.segments_size());
    seg[i] = segments.push_back_segment();
    EXPECT_EQ(i + 1, segments.segments_size());
  }

  string output;
  segments.DebugString(&output);
  EXPECT_FALSE(output.empty());

  EXPECT_FALSE(segments.has_resized());
  segments.set_resized(true);
  EXPECT_TRUE(segments.has_resized());
  segments.set_resized(false);
  EXPECT_FALSE(segments.has_resized());

  segments.set_max_history_segments_size(10);
  EXPECT_EQ(10, segments.max_history_segments_size());

  segments.set_max_history_segments_size(5);
  EXPECT_EQ(5, segments.max_history_segments_size());

  segments.set_max_prediction_candidates_size(10);
  EXPECT_EQ(10, segments.max_prediction_candidates_size());

  segments.set_max_prediction_candidates_size(5);
  EXPECT_EQ(5, segments.max_prediction_candidates_size());

  for (int i = 0; i < kSegmentsSize; ++i) {
    EXPECT_EQ(seg[i], segments.mutable_segment(i));
  }

  segments.pop_back_segment();
  EXPECT_EQ(seg[3], segments.mutable_segment(3));

  segments.pop_front_segment();
  EXPECT_EQ(seg[1], segments.mutable_segment(0));

  segments.clear();
  EXPECT_EQ(0, segments.segments_size());

  for (int i = 0; i < kSegmentsSize; ++i) {
    seg[i] = segments.push_back_segment();
  }

  // erase
  segments.erase_segment(1);
  EXPECT_EQ(seg[0], segments.mutable_segment(0));
  EXPECT_EQ(seg[2], segments.mutable_segment(1));

  segments.erase_segments(1, 2);
  EXPECT_EQ(seg[0], segments.mutable_segment(0));
  EXPECT_EQ(seg[4], segments.mutable_segment(1));

  EXPECT_EQ(2, segments.segments_size());

  // insert
  seg[1] = segments.insert_segment(1);
  EXPECT_EQ(seg[1], segments.mutable_segment(1));
  EXPECT_EQ(seg[4], segments.mutable_segment(2));

  segments.clear();
  for (int i = 0; i < kSegmentsSize; ++i) {
    seg[i] = segments.push_back_segment();
    if (i < 2) {
      seg[i]->set_segment_type(Segment::HISTORY);
    }
  }

  // history/conversion
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(segments.mutable_segment(i + segments.history_segments_size()),
              segments.mutable_conversion_segment(i));
  }

  EXPECT_EQ(2, segments.history_segments_size());
  EXPECT_EQ(3, segments.conversion_segments_size());

  EXPECT_EQ(seg[2], segments.mutable_conversion_segment(0));
  EXPECT_EQ(seg[0], segments.mutable_history_segment(0));

  segments.clear_history_segments();
  EXPECT_EQ(3, segments.segments_size());

  segments.clear_conversion_segments();
  EXPECT_EQ(0, segments.segments_size());
}

TEST_F(CandidateTest, BasicTest) {
  Segment segment;

  const char str[] = "this is a test";
  segment.set_key(str);
  EXPECT_EQ(segment.key(), str);

  segment.set_segment_type(Segment::FIXED_BOUNDARY);
  EXPECT_EQ(Segment::FIXED_BOUNDARY, segment.segment_type());

  const int kCandidatesSize = 5;
  Segment::Candidate *cand[kCandidatesSize];
  for (int i = 0; i < kCandidatesSize; ++i) {
    EXPECT_EQ(i, segment.candidates_size());
    cand[i] = segment.push_back_candidate();
    EXPECT_EQ(i + 1, segment.candidates_size());
  }

  for (int i = 0; i < kCandidatesSize; ++i) {
    EXPECT_EQ(cand[i], segment.mutable_candidate(i));
  }

  for (int i = 0; i < kCandidatesSize; ++i) {
    EXPECT_EQ(i, segment.indexOf(segment.mutable_candidate(i)));
  }

  for (int i = -static_cast<int>
           (segment.meta_candidates_size() - 1); i >= 0; ++i) {
    EXPECT_EQ(i, segment.indexOf(segment.mutable_candidate(i)));
  }

  EXPECT_EQ(segment.candidates_size(),
            segment.indexOf(NULL));

  segment.pop_back_candidate();
  EXPECT_EQ(cand[3], segment.mutable_candidate(3));

  segment.pop_front_candidate();
  EXPECT_EQ(cand[1], segment.mutable_candidate(0));

  segment.clear();
  EXPECT_EQ(0, segment.candidates_size());

  for (int i = 0; i < kCandidatesSize; ++i) {
    cand[i] = segment.push_back_candidate();
  }

  // erase
  segment.erase_candidate(1);
  EXPECT_EQ(cand[0], segment.mutable_candidate(0));
  EXPECT_EQ(cand[2], segment.mutable_candidate(1));

  segment.erase_candidates(1, 2);
  EXPECT_EQ(cand[0], segment.mutable_candidate(0));
  EXPECT_EQ(cand[4], segment.mutable_candidate(1));
  EXPECT_EQ(2, segment.candidates_size());

  // insert
  cand[1] = segment.insert_candidate(1);
  EXPECT_EQ(cand[1], segment.mutable_candidate(1));
  EXPECT_EQ(cand[4], segment.mutable_candidate(2));

  // move
  segment.clear();
  for (int i = 0; i < kCandidatesSize; ++i) {
    cand[i] = segment.push_back_candidate();
  }

  segment.move_candidate(2, 0);
  EXPECT_EQ(cand[2], segment.mutable_candidate(0));
  EXPECT_EQ(cand[0], segment.mutable_candidate(1));
  EXPECT_EQ(cand[1], segment.mutable_candidate(2));

  segment.clear();
  for (int i = 0; i < kCandidatesSize; ++i) {
    char buf[32];
    snprintf(buf, sizeof(buf), "value_%d", i);
    cand[i] = segment.push_back_candidate();
    cand[i]->value = buf;
  }

  EXPECT_TRUE(segment.has_candidate_value("value_0"));
  EXPECT_TRUE(segment.has_candidate_value("value_3"));
  EXPECT_FALSE(segment.has_candidate_value("foo"));
}

TEST_F(SegmentsTest, RevertEntryTest) {
  Segments segments;
  EXPECT_EQ(0, segments.revert_entries_size());

  const int kSize = 10;
  for (int i = 0; i < kSize; ++i) {
    Segments::RevertEntry *e = segments.push_back_revert_entry();
    e->key = "test" + Util::SimpleItoa(i);
    e->id = i;
  }

  EXPECT_EQ(kSize, segments.revert_entries_size());

  for (int i = 0; i < kSize; ++i) {
    {
      const Segments::RevertEntry &e = segments.revert_entry(i);
      EXPECT_EQ(string("test") + Util::SimpleItoa(i), e.key);
      EXPECT_EQ(i, e.id);
    }
    {
      Segments::RevertEntry *e = segments.mutable_revert_entry(i);
      EXPECT_EQ(string("test") + Util::SimpleItoa(i), e->key);
      EXPECT_EQ(i, e->id);
    }
  }

  for (int i = 0; i < kSize; ++i) {
    Segments::RevertEntry *e = segments.mutable_revert_entry(i);
    e->id = kSize - i;
    e->key = "test2" + Util::SimpleItoa(i);
  }

  for (int i = 0; i < kSize; ++i) {
    const Segments::RevertEntry &e = segments.revert_entry(i);
    EXPECT_EQ(string("test2") + Util::SimpleItoa(i), e.key);
    EXPECT_EQ(kSize - i, e.id);
  }

  segments.clear_revert_entries();
  EXPECT_EQ(0, segments.revert_entries_size());
}

TEST_F(CandidateTest, SetDefaultDescription) {
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "HalfASCII";
    candidate.content_value = candidate.value;
    candidate.content_key = "halfascii";
    candidate.SetDefaultDescription(
        Segment::Candidate::FULL_HALF_WIDTH |
        Segment::Candidate::CHARACTER_FORM |
        Segment::Candidate::PLATFORM_DEPENDENT_CHARACTER);
    // "[半] アルファベット"
    EXPECT_EQ("\x5b\xe5\x8d\x8a\x5d\x20\xe3\x82\xa2\xe3\x83\xab\xe3\x83\x95\xe3"
              "\x82\xa1\xe3\x83\x99\xe3\x83\x83\xe3\x83\x88",
              candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "!@#";
    candidate.content_value = candidate.value;
    candidate.content_key = "!@#";
    candidate.SetDefaultDescription(
        Segment::Candidate::FULL_HALF_WIDTH |
        Segment::Candidate::CHARACTER_FORM |
        Segment::Candidate::PLATFORM_DEPENDENT_CHARACTER);
    // TODO(komatsu): Make sure this is an expected behavior.
    EXPECT_TRUE(candidate.description.empty());
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    // "「ＡＢＣ」"
    candidate.value = "\xe3\x80\x8c\xef\xbc\xa1\xef\xbc\xa2\xef\xbc\xa3\xe3"
                      "\x80\x8d";
    candidate.content_value = candidate.value;
    candidate.content_key = "[ABC]";
    candidate.SetDefaultDescription(
        Segment::Candidate::FULL_HALF_WIDTH |
        Segment::Candidate::CHARACTER_FORM |
        Segment::Candidate::PLATFORM_DEPENDENT_CHARACTER);
    // TODO(komatsu): Make sure this is an expected behavior.
    EXPECT_TRUE(candidate.description.empty());
  }
}

TEST_F(CandidateTest, SetTransliterationDescription) {
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "HalfASCII";
    candidate.content_value = candidate.value;
    candidate.content_key = "halfascii";
    candidate.SetTransliterationDescription();
    // "[半] アルファベット"
    EXPECT_EQ("\x5b\xe5\x8d\x8a\x5d\x20\xe3\x82\xa2\xe3\x83\xab\xe3\x83\x95\xe3"
              "\x82\xa1\xe3\x83\x99\xe3\x83\x83\xe3\x83\x88",
              candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "!@#";
    candidate.content_value = candidate.value;
    candidate.content_key = "!@#";
    candidate.SetTransliterationDescription();
    // "[半]"
    EXPECT_EQ("\x5b\xe5\x8d\x8a\x5d", candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    // "「ＡＢＣ」"
    candidate.value = "\xe3\x80\x8c\xef\xbc\xa1\xef\xbc\xa2\xef\xbc\xa3\xe3"
                      "\x80\x8d";
    candidate.content_value = candidate.value;
    candidate.content_key = "[ABC]";
    candidate.SetTransliterationDescription();
    // "[全]"
    EXPECT_EQ("\x5b\xe5\x85\xa8\x5d", candidate.description);
  }
}

TEST_F(CandidateTest, SetDescription) {
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "!";
    candidate.content_value = candidate.value;
    candidate.content_key = "!";
    candidate.SetDescription(Segment::Candidate::FULL_HALF_WIDTH,
                             "\xe3\x81\xb3\xe3\x81\xa3\xe3\x81\x8f"
                             "\xe3\x82\x8a");  // "びっくり"
    EXPECT_EQ("[\xe5\x8d\x8a] \xe3\x81\xb3\xe3\x81\xa3"
              "\xe3\x81\x8f\xe3\x82\x8a",  // "[半] びっくり"
              candidate.description);
  }
}

TEST_F(CandidateTest, ResetDescription) {
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "!";
    candidate.content_value = candidate.value;
    candidate.content_key = "!";
    candidate.SetDescription(Segment::Candidate::FULL_HALF_WIDTH,
                             "[\xe3\x81\xaa\xe3\x82\x93\xe3\x81\xa8"
                             "]\xe3\x81\xb3\xe3\x81\xa3\xe3\x81\x8f"
                             "\xe3\x82\x8a(\xe3\x81\xaa\xe3\x82\x93"
                             "\xe3\x81\xa8)");  // "[なんと]びっくり(なんと)"
    EXPECT_EQ("[\xe5\x8d\x8a] [\xe3\x81\xaa\xe3\x82\x93\xe3\x81\xa8]"
              "\xe3\x81\xb3\xe3\x81\xa3\xe3\x81\x8f\xe3\x82\x8a("
              "\xe3\x81\xaa\xe3\x82\x93\xe3\x81\xa8)",
              // "[半] [なんと]びっくり(なんと)"
              candidate.description);
    candidate.value = "\xef\xbc\x81";  // "！"
    candidate.ResetDescription(Segment::Candidate::FULL_HALF_WIDTH);
    EXPECT_EQ("[\xe5\x85\xa8] [\xe3\x81\xaa\xe3\x82\x93\xe3\x81\xa8]"
              "\xe3\x81\xb3\xe3\x81\xa3\xe3\x81\x8f\xe3\x82\x8a("
              "\xe3\x81\xaa\xe3\x82\x93\xe3\x81\xa8)",
              // "[全] [なんと]びっくり(なんと)",
              candidate.description);
  }
}

TEST_F(SegmentTest, ExpandAlternative) {
  Segments segments;

  Segment *seg = segments.push_back_segment();

  {
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->Init();
    // "あいう"
    candidate->value = "\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86";
    // "あいう"
    candidate->content_value = "\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86";
    EXPECT_FALSE(seg->ExpandAlternative(0));
    seg->clear_candidates();
  }

  {
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->Init();
    candidate->value = "012";
    candidate->content_value = "012";
    CharacterFormManager::GetCharacterFormManager()->
      SetCharacterForm("012", config::Config::FULL_WIDTH);

    EXPECT_TRUE(seg->ExpandAlternative(0));
    EXPECT_EQ(2, seg->candidates_size());
    // "０１２"
    EXPECT_EQ("\xef\xbc\x90\xef\xbc\x91\xef\xbc\x92", seg->candidate(0).value);
    // "０１２"
    EXPECT_EQ("\xef\xbc\x90\xef\xbc\x91\xef\xbc\x92",
              seg->candidate(0).content_value);
    EXPECT_EQ("012", seg->candidate(1).value);
    EXPECT_EQ("012", seg->candidate(1).content_value);
    seg->clear_candidates();
  }

  {
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->Init();
    candidate->value = "Google";
    candidate->content_value = "Google";
    CharacterFormManager::GetCharacterFormManager()->
      SetCharacterForm("abc", config::Config::FULL_WIDTH);

    EXPECT_TRUE(seg->ExpandAlternative(0));
    EXPECT_EQ(2, seg->candidates_size());
    // "Ｇｏｏｇｌｅ"
    EXPECT_EQ("\xef\xbc\xa7\xef\xbd\x8f\xef\xbd\x8f\xef\xbd\x87\xef\xbd\x8c\xef"
              "\xbd\x85", seg->candidate(0).value);
    // "Ｇｏｏｇｌｅ"
    EXPECT_EQ("\xef\xbc\xa7\xef\xbd\x8f\xef\xbd\x8f\xef\xbd\x87\xef\xbd\x8c\xef"
              "\xbd\x85", seg->candidate(0).content_value);
    EXPECT_EQ("Google", seg->candidate(1).value);
    EXPECT_EQ("Google", seg->candidate(1).content_value);
    seg->clear_candidates();
  }


  {
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->Init();
    candidate->value = "@";
    candidate->content_value = "@";
    CharacterFormManager::GetCharacterFormManager()->
      SetCharacterForm("@", config::Config::FULL_WIDTH);

    EXPECT_TRUE(seg->ExpandAlternative(0));
    EXPECT_EQ(2, seg->candidates_size());
    // "＠"
    EXPECT_EQ("\xef\xbc\xa0", seg->candidate(0).value);
    // "＠"
    EXPECT_EQ("\xef\xbc\xa0", seg->candidate(0).content_value);
    EXPECT_EQ("@", seg->candidate(1).value);
    EXPECT_EQ("@", seg->candidate(1).content_value);
    seg->clear_candidates();
  }

  {
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->Init();
    // "グーグル"
    candidate->value = "\xe3\x82\xb0\xe3\x83\xbc\xe3\x82\xb0\xe3\x83\xab";
    // "グーグル"
    candidate->content_value = "\xe3\x82\xb0\xe3\x83\xbc\xe3\x82\xb0\xe3\x83"
                               "\xab";
    CharacterFormManager::GetCharacterFormManager()->
      // "アイウ"
      SetCharacterForm("\xe3\x82\xa2\xe3\x82\xa4\xe3\x82\xa6",
                       config::Config::FULL_WIDTH);

    EXPECT_FALSE(seg->ExpandAlternative(0));
    seg->clear_candidates();
  }

  {
     Segment::Candidate *candidate = seg->add_candidate();
     candidate->Init();
     // "グーグル"
     candidate->value = "\xe3\x82\xb0\xe3\x83\xbc\xe3\x82\xb0\xe3\x83\xab";
     // "グーグル"
     candidate->content_value = "\xe3\x82\xb0\xe3\x83\xbc\xe3\x82\xb0\xe3\x83"
                                "\xab";
     CharacterFormManager::GetCharacterFormManager()->Clear();
     CharacterFormManager::GetCharacterFormManager()->
         // "アイウ"
         AddConversionRule("\xe3\x82\xa2\xe3\x82\xa4\xe3\x82\xa6",
                           config::Config::HALF_WIDTH);

     EXPECT_TRUE(seg->ExpandAlternative(0));
     EXPECT_EQ(2, seg->candidates_size());
     // "ｸﾞｰｸﾞﾙ"
     EXPECT_EQ("\xef\xbd\xb8\xef\xbe\x9e\xef\xbd\xb0\xef\xbd\xb8\xef\xbe\x9e"
               "\xef\xbe\x99", seg->candidate(0).value);
     // "ｸﾞｰｸﾞﾙ"
     EXPECT_EQ("\xef\xbd\xb8\xef\xbe\x9e\xef\xbd\xb0\xef\xbd\xb8\xef\xbe\x9e"
               "\xef\xbe\x99", seg->candidate(0).content_value);
     // "グーグル"
     EXPECT_EQ("\xe3\x82\xb0\xe3\x83\xbc\xe3\x82\xb0\xe3\x83\xab",
               seg->candidate(1).value);
     // "グーグル"
     EXPECT_EQ("\xe3\x82\xb0\xe3\x83\xbc\xe3\x82\xb0\xe3\x83\xab",
               seg->candidate(1).content_value);
     seg->clear_candidates();
  }

  {
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->Init();
    candidate->value = "Google";
    candidate->content_value = "Google";
    candidate->can_expand_alternative = false;
    CharacterFormManager::GetCharacterFormManager()->
      SetCharacterForm("abc", config::Config::FULL_WIDTH);

    EXPECT_EQ(1, seg->candidates_size());
    EXPECT_EQ("Google", seg->candidate(0).value);
    EXPECT_EQ("Google", seg->candidate(0).content_value);
    seg->clear_candidates();
  }
}

TEST_F(SegmentTest, SetTransliterations) {
  Segments segments;
  Segment *seg = segments.push_back_segment();

  // "じしん"
  seg->set_key("\xe3\x81\x98\xe3\x81\x97\xe3\x82\x93");
  // "じしん"
  EXPECT_EQ("\xe3\x81\x98\xe3\x81\x97\xe3\x82\x93",
            seg->meta_candidate(transliteration::HIRAGANA).value);
  // "ジシン"
  EXPECT_EQ("\xe3\x82\xb8\xe3\x82\xb7\xe3\x83\xb3",
            seg->meta_candidate(transliteration::FULL_KATAKANA).value);
  EXPECT_EQ("zisin", seg->meta_candidate(transliteration::HALF_ASCII).value);
  // "ｚｉｓｉｎ"
  EXPECT_EQ("\xef\xbd\x9a\xef\xbd\x89\xef\xbd\x93\xef\xbd\x89\xef\xbd\x8e",
            seg->meta_candidate(transliteration::FULL_ASCII).value);
  // "ｼﾞｼﾝ"
  EXPECT_EQ("\xef\xbd\xbc\xef\xbe\x9e\xef\xbd\xbc\xef\xbe\x9d",
            seg->meta_candidate(transliteration::HALF_KATAKANA).value);
  EXPECT_FALSE(seg->initialized_transliterations());

  vector<string> t13ns(transliteration::NUM_T13N_TYPES);
  // "じしん"
  t13ns[transliteration::HIRAGANA] = "\xe3\x81\x98\xe3\x81\x97\xe3\x82\x93";
  // "ジシン"
  t13ns[transliteration::FULL_KATAKANA] = "\xe3\x82\xb8\xe3\x82\xb7"
                                          "\xe3\x83\xb3";
  t13ns[transliteration::HALF_ASCII] = "jishinn";
  // "ｊｉｓｈｉｎｎ"
  t13ns[transliteration::FULL_ASCII] = "\xef\xbd\x8a\xef\xbd\x89\xef\xbd\x93"
                                       "\xef\xbd\x88\xef\xbd\x89\xef\xbd\x8e"
                                       "\xef\xbd\x8e";
  // "ｼﾞｼﾝ"
  t13ns[transliteration::HALF_KATAKANA] = "\xef\xbd\xbc\xef\xbe\x9e"
                                          "\xef\xbd\xbc\xef\xbe\x9d";

  seg->SetTransliterations(t13ns);
  // "じしん"
  EXPECT_EQ("\xe3\x81\x98\xe3\x81\x97\xe3\x82\x93",
            seg->meta_candidate(transliteration::HIRAGANA).value);
  // "ジシン"
  EXPECT_EQ("\xe3\x82\xb8\xe3\x82\xb7\xe3\x83\xb3",
            seg->meta_candidate(transliteration::FULL_KATAKANA).value);
  EXPECT_EQ("jishinn", seg->meta_candidate(transliteration::HALF_ASCII).value);
  // "ｊｉｓｈｉｎｎ"
  EXPECT_EQ("\xef\xbd\x8a\xef\xbd\x89\xef\xbd\x93\xef\xbd\x88\xef\xbd\x89\xef"
            "\xbd\x8e\xef\xbd\x8e",
            seg->meta_candidate(transliteration::FULL_ASCII).value);
  // "ｼﾞｼﾝ"
  EXPECT_EQ("\xef\xbd\xbc\xef\xbe\x9e\xef\xbd\xbc\xef\xbe\x9d",
            seg->meta_candidate(transliteration::HALF_KATAKANA).value);
  EXPECT_TRUE(seg->initialized_transliterations());

  seg->Clear();
  EXPECT_FALSE(seg->initialized_transliterations());
}

}  // namespace mozc
