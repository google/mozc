// Copyright 2010-2011, Google Inc.
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
#include "composer/composer.h"
#include "composer/table.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/converter_interface.h"
#include "converter/character_form_manager.h"
#include "converter/segments.h"
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

  segments.set_user_history_enabled(true);
  EXPECT_TRUE(segments.user_history_enabled());

  segments.set_user_history_enabled(false);
  EXPECT_FALSE(segments.user_history_enabled());

  EXPECT_EQ(0, segments.segments_size());

  const int kSegmentsSize = 5;
  Segment *seg[kSegmentsSize];
  for (int i = 0; i < kSegmentsSize; ++i) {
    EXPECT_EQ(i, segments.segments_size());
    seg[i] = segments.push_back_segment();
    EXPECT_EQ(i + 1, segments.segments_size());
  }

  const string output = segments.DebugString();
  EXPECT_FALSE(output.empty());

  EXPECT_FALSE(segments.resized());
  segments.set_resized(true);
  EXPECT_TRUE(segments.resized());
  segments.set_resized(false);
  EXPECT_FALSE(segments.resized());

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

  segments.Clear();
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

  segments.erase_segments(0, 1);
  EXPECT_EQ(1, segments.segments_size());
  EXPECT_EQ(seg[4], segments.mutable_segment(0));

  // insert
  seg[1] = segments.insert_segment(1);
  EXPECT_EQ(seg[1], segments.mutable_segment(1));

  segments.Clear();
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

  for (int i = -static_cast<int>(segment.meta_candidates_size()) - 1;
       i >= 0; ++i) {
    EXPECT_EQ(i, segment.indexOf(segment.mutable_candidate(i)));
  }

  EXPECT_EQ(segment.candidates_size(),
            segment.indexOf(NULL));

  segment.pop_back_candidate();
  EXPECT_EQ(cand[3], segment.mutable_candidate(3));

  segment.pop_front_candidate();
  EXPECT_EQ(cand[1], segment.mutable_candidate(0));

  segment.Clear();
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
  segment.Clear();
  for (int i = 0; i < kCandidatesSize; ++i) {
    cand[i] = segment.push_back_candidate();
  }

  segment.move_candidate(2, 0);
  EXPECT_EQ(cand[2], segment.mutable_candidate(0));
  EXPECT_EQ(cand[0], segment.mutable_candidate(1));
  EXPECT_EQ(cand[1], segment.mutable_candidate(2));
}

TEST_F(CandidateTest, CopyFrom) {
  Segment::Candidate src, dest;
  src.Init();

  src.key = "key";
  src.value = "value";
  src.content_key = "content_key";
  src.content_value = "content_value";
  src.prefix = "prefix";
  src.suffix = "suffix";
  src.description = "description";
  src.usage_title = "usage_title";
  src.usage_description = "usage_description";
  src.cost = 1;
  src.wcost = 2;
  src.structure_cost = 3;
  src.lid = 4;
  src.rid = 5;
  src.attributes = 6;
  src.style = 7;

  dest.CopyFrom(src);

  EXPECT_EQ(src.key, dest.key);
  EXPECT_EQ(src.value, dest.value);
  EXPECT_EQ(src.content_key, dest.content_key);
  EXPECT_EQ(src.content_value, dest.content_value);
  EXPECT_EQ(src.prefix, dest.prefix);
  EXPECT_EQ(src.suffix, dest.suffix);
  EXPECT_EQ(src.description, dest.description);
  EXPECT_EQ(src.usage_title, dest.usage_title);
  EXPECT_EQ(src.usage_description, dest.usage_description);
  EXPECT_EQ(src.cost, dest.cost);
  EXPECT_EQ(src.wcost, dest.wcost);
  EXPECT_EQ(src.structure_cost, dest.structure_cost);
  EXPECT_EQ(src.lid, dest.lid);
  EXPECT_EQ(src.rid, dest.rid);
  EXPECT_EQ(src.attributes, dest.attributes);
  EXPECT_EQ(src.style, dest.style);
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

  {
    const Segments::RevertEntry &src = segments.revert_entry(0);
    Segments::RevertEntry dest;
    dest.CopyFrom(src);
    EXPECT_EQ(src.revert_entry_type, dest.revert_entry_type);
    EXPECT_EQ(src.id, dest.id);
    EXPECT_EQ(src.timestamp, dest.timestamp);
    EXPECT_EQ(src.key, dest.key);
  }

  segments.clear_revert_entries();
  EXPECT_EQ(0, segments.revert_entries_size());
}

TEST_F(SegmentsTest, RemoveTailOfHistorySegments) {
  Segments segments;
  {
    Segment *segment = segments.add_segment();
    segment->set_segment_type(Segment::HISTORY);
    Segment::Candidate *candidate = segment->add_candidate();
    // "Mozc を"
    candidate->value = "Mozc \xE3\x82\x92";
  }
  {
    Segment *segment = segments.add_segment();
    segment->set_segment_type(Segment::HISTORY);
    Segment::Candidate *candidate = segment->add_candidate();
    // "使う"
    candidate->value = "\xE4\xBD\xBF\xE3\x81\x86";
  }

  segments.RemoveTailOfHistorySegments(1);
  EXPECT_EQ(2, segments.history_segments_size());
  // "使"
  EXPECT_EQ("\xE4\xBD\xBF", segments.history_segment(1).candidate(0).value);

  segments.RemoveTailOfHistorySegments(2);
  EXPECT_EQ(1, segments.history_segments_size());
  EXPECT_EQ("Mozc ", segments.history_segment(0).candidate(0).value);

  segments.RemoveTailOfHistorySegments(2);
  EXPECT_EQ("Moz", segments.history_segment(0).candidate(0).value);
}

TEST_F(SegmentsTest, ComposerAccessorTest) {
  Segments segments;
  EXPECT_TRUE(segments.composer() == NULL);
  composer::Composer composer;
  segments.set_composer(&composer);
  EXPECT_EQ(&composer, segments.composer());
}

TEST_F(SegmentsTest, CopyFromTest) {
  Segments src;

  src.set_max_history_segments_size(1);
  src.set_max_prediction_candidates_size(2);
  src.set_max_conversion_candidates_size(2);
  src.set_resized(true);
  src.set_user_history_enabled(true);
  src.set_request_type(Segments::PREDICTION);

  const int kSegmentsSize = 3;
  const int kCandidatesSize = 2;

  for (int i = 0; i < kSegmentsSize; ++i) {
    Segment *segment = src.add_segment();
    segment->set_key("segment_" + i);
    for (int j = 0; j < kCandidatesSize; ++j) {
      Segment::Candidate *candidate = segment->add_candidate();
      candidate->key = "candidate" + i;
    }
  }
  EXPECT_EQ(kSegmentsSize, src.segments_size());
  EXPECT_EQ(kCandidatesSize, src.segment(0).candidates_size());

  Segments dest;
  dest.CopyFrom(src);
  EXPECT_EQ(src.max_history_segments_size(), dest.max_history_segments_size());
  EXPECT_EQ(src.max_prediction_candidates_size(),
            dest.max_prediction_candidates_size());
  EXPECT_EQ(src.max_conversion_candidates_size(),
            dest.max_conversion_candidates_size());
  EXPECT_EQ(src.resized(), dest.resized());
  EXPECT_EQ(src.user_history_enabled(), dest.user_history_enabled());
  EXPECT_EQ(src.request_type(), dest.request_type());

  EXPECT_EQ(kSegmentsSize, dest.segments_size());
  EXPECT_EQ(kCandidatesSize, dest.segment(0).candidates_size());

  for (int i = 0; i < kSegmentsSize; ++i) {
    EXPECT_EQ(src.segment(i).key(), dest.segment(i).key());
    for (int j = 0; j < kCandidatesSize; ++j) {
      EXPECT_EQ(src.segment(i).candidate(j).key,
                dest.segment(i).candidate(j).key);
    }
  }

  // TODO(hsumita) copy composer correctly
  EXPECT_EQ(reinterpret_cast<composer::Composer *>(NULL), src.composer());
}

TEST_F(CandidateTest, functional_key) {
  Segment::Candidate candidate;
  candidate.Init();

  candidate.key = "testfoobar";
  candidate.content_key = "test";
  EXPECT_EQ("foobar", candidate.functional_key());

  candidate.key = "testfoo";
  candidate.content_key = "test";
  EXPECT_EQ("foo", candidate.functional_key());

  // This is unexpected key/context_key.
  // This method doesn't check the prefix part.
  candidate.key = "abcdefg";
  candidate.content_key = "test";
  EXPECT_EQ("efg", candidate.functional_key());

  candidate.key = "test";
  candidate.content_key = "test";
  EXPECT_EQ("", candidate.functional_key());

  candidate.key = "test";
  candidate.content_key = "testfoobar";
  EXPECT_EQ("", candidate.functional_key());

  candidate.key = "";
  candidate.content_key = "";
  EXPECT_EQ("", candidate.functional_key());
}

TEST_F(CandidateTest, functional_value) {
  Segment::Candidate candidate;
  candidate.Init();

  candidate.value = "testfoobar";
  candidate.content_value = "test";
  EXPECT_EQ("foobar", candidate.functional_value());

  candidate.value = "testfoo";
  candidate.content_value = "test";
  EXPECT_EQ("foo", candidate.functional_value());

  // This is unexpected value/context_value.
  // This method doesn't check the prefix part.
  candidate.value = "abcdefg";
  candidate.content_value = "test";
  EXPECT_EQ("efg", candidate.functional_value());

  candidate.value = "test";
  candidate.content_value = "test";
  EXPECT_EQ("", candidate.functional_value());

  candidate.value = "test";
  candidate.content_value = "testfoobar";
  EXPECT_EQ("", candidate.functional_value());

  candidate.value = "";
  candidate.content_value = "";
  EXPECT_EQ("", candidate.functional_value());
}

TEST_F(SegmentTest, CopyFrom) {
  Segment src, dest;

  src.set_key("key");
  src.set_segment_type(Segment::FIXED_VALUE);
  Segment::Candidate *candidate1 = src.add_candidate();
  candidate1->key = "candidate1->key";
  Segment::Candidate *candidate2 = src.add_candidate();
  candidate2->key = "candidate2->key";
  Segment::Candidate *meta_candidate = src.add_meta_candidate();
  meta_candidate->key = "meta_candidate->key";

  dest.CopyFrom(src);

  EXPECT_EQ(src.key(), dest.key());
  EXPECT_EQ(src.segment_type(), dest.segment_type());
  EXPECT_EQ(src.candidate(0).key, dest.candidate(0).key);
  EXPECT_EQ(src.candidate(1).key, dest.candidate(1).key);
  EXPECT_EQ(src.meta_candidate(0).key, dest.meta_candidate(0).key);
}

TEST_F(SegmentTest, MetaCandidateTest) {
  Segment segment;

  EXPECT_EQ(0, segment.meta_candidates_size());

  const int kCandidatesSize = 5;
  vector<string> values;
  for (size_t i = 0; i < kCandidatesSize; ++i) {
    values.push_back(string('a' + i, 1));
  }

  // add_meta_candidate()
  for (size_t i = 0; i < kCandidatesSize; ++i) {
    EXPECT_EQ(i, segment.meta_candidates_size());
    Segment::Candidate *cand = segment.add_meta_candidate();
    cand->value = values[i];
    EXPECT_EQ(i + 1, segment.meta_candidates_size());
  }

  // mutable_candidate()
  for (size_t i = 0; i < kCandidatesSize; ++i) {
    const int meta_idx = -static_cast<int>(i)-1;
    Segment::Candidate *cand = segment.mutable_candidate(meta_idx);
    EXPECT_EQ(values[i], cand->value);
  }

  // mutable_meta_candidate()
  for (size_t i = 0; i < kCandidatesSize; ++i) {
    Segment::Candidate *cand = segment.mutable_meta_candidate(i);
    EXPECT_EQ(values[i], cand->value);
  }

  // candidate()
  for (size_t i = 0; i < kCandidatesSize; ++i) {
    const int meta_idx = -static_cast<int>(i)-1;
    const Segment::Candidate &cand = segment.candidate(meta_idx);
    EXPECT_EQ(values[i], cand.value);
  }

  // meta_candidate()
  for (size_t i = 0; i < kCandidatesSize; ++i) {
    const Segment::Candidate &cand = segment.meta_candidate(i);
    EXPECT_EQ(values[i], cand.value);
  }

  // indexOf
  for (size_t i = 0; i < kCandidatesSize; ++i) {
    const int meta_idx = -static_cast<int>(i)-1;
    EXPECT_EQ(meta_idx, segment.indexOf(segment.mutable_candidate(meta_idx)));
  }

  EXPECT_EQ(segment.candidates_size(),
            segment.indexOf(NULL));

  // mutable_meta_candidates
  {
    vector<Segment::Candidate> *meta_candidates =
        segment.mutable_meta_candidates();
    EXPECT_EQ(kCandidatesSize, meta_candidates->size());
    Segment::Candidate cand;
    cand.Init();
    cand.value = "Test";
    meta_candidates->push_back(cand);
  }

  // meta_candidates
  {
    const vector<Segment::Candidate> &meta_candidates =
        segment.meta_candidates();
    EXPECT_EQ(kCandidatesSize + 1, meta_candidates.size());
    for (size_t i = 0; i < kCandidatesSize; ++i) {
      EXPECT_EQ(values[i], meta_candidates[i].value);
    }
    EXPECT_EQ("Test", meta_candidates[kCandidatesSize].value);
  }
  // clear
  segment.clear_meta_candidates();
  EXPECT_EQ(0, segment.meta_candidates_size());
}
}  // namespace
