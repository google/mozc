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

#include "converter/segments.h"

#include <string>
#include <vector>

#include "base/number_util.h"
#include "base/system_util.h"
#include "base/util.h"
#include "config/config_handler.h"
#include "testing/base/public/gunit.h"
#include "absl/strings/string_view.h"

namespace mozc {

TEST(SegmentsTest, BasicTest) {
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

  constexpr int kSegmentsSize = 5;
  Segment *seg[kSegmentsSize];
  for (int i = 0; i < kSegmentsSize; ++i) {
    EXPECT_EQ(i, segments.segments_size());
    seg[i] = segments.push_back_segment();
    EXPECT_EQ(i + 1, segments.segments_size());
  }

  const std::string output = segments.DebugString();
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

TEST(CandidateTest, BasicTest) {
  Segment segment;

  const char str[] = "this is a test";
  segment.set_key(str);
  EXPECT_EQ(segment.key(), str);

  segment.set_segment_type(Segment::FIXED_BOUNDARY);
  EXPECT_EQ(Segment::FIXED_BOUNDARY, segment.segment_type());

  constexpr int kCandidatesSize = 5;
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
    EXPECT_TRUE(segment.is_valid_index(i));
  }
  EXPECT_FALSE(segment.is_valid_index(kCandidatesSize));

  const int kMetaCandidatesSize =
      static_cast<int>(segment.meta_candidates_size());
  for (int i = -kMetaCandidatesSize; i < 0; ++i) {
    EXPECT_EQ(i, segment.indexOf(segment.mutable_candidate(i)));
    EXPECT_TRUE(segment.is_valid_index(i));
  }
  EXPECT_TRUE(segment.is_valid_index(-kMetaCandidatesSize));
  EXPECT_FALSE(segment.is_valid_index(-kMetaCandidatesSize - 1));

  EXPECT_EQ(segment.candidates_size(), segment.indexOf(nullptr));

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

TEST(CandidateTest, IsValid) {
  Segment::Candidate c;
  c.Init();
  EXPECT_TRUE(c.IsValid());

  c.key = "key";
  c.value = "value";
  c.content_key = "content_key";
  c.content_value = "content_value";
  c.prefix = "prefix";
  c.suffix = "suffix";
  c.description = "description";
  c.usage_title = "usage_title";
  c.usage_description = "usage_description";
  c.cost = 1;
  c.wcost = 2;
  c.structure_cost = 3;
  c.lid = 4;
  c.rid = 5;
  c.attributes = 6;
  c.style = NumberUtil::NumberString::NUMBER_CIRCLED;
  c.command = Segment::Candidate::DISABLE_PRESENTATION_MODE;
  EXPECT_TRUE(c.IsValid());  // Empty inner_segment_boundary

  // Valid inner_segment_boundary.
  c.inner_segment_boundary.push_back(
      Segment::Candidate::EncodeLengths(1, 3, 8, 8));
  c.inner_segment_boundary.push_back(
      Segment::Candidate::EncodeLengths(2, 2, 3, 5));
  EXPECT_TRUE(c.IsValid());

  // Invalid inner_segment_boundary.
  c.inner_segment_boundary.clear();
  c.inner_segment_boundary.push_back(
      Segment::Candidate::EncodeLengths(1, 1, 2, 2));
  c.inner_segment_boundary.push_back(
      Segment::Candidate::EncodeLengths(2, 2, 3, 3));
  c.inner_segment_boundary.push_back(
      Segment::Candidate::EncodeLengths(3, 3, 4, 4));
  EXPECT_FALSE(c.IsValid());
}

TEST(SegmentsTest, RevertEntryTest) {
  Segments segments;
  EXPECT_EQ(0, segments.revert_entries_size());

  constexpr int kSize = 10;
  for (int i = 0; i < kSize; ++i) {
    Segments::RevertEntry *e = segments.push_back_revert_entry();
    e->key = "test" + std::to_string(i);
    e->id = i;
  }

  EXPECT_EQ(kSize, segments.revert_entries_size());

  for (int i = 0; i < kSize; ++i) {
    {
      const Segments::RevertEntry &e = segments.revert_entry(i);
      EXPECT_EQ(std::string("test") + std::to_string(i), e.key);
      EXPECT_EQ(i, e.id);
    }
    {
      Segments::RevertEntry *e = segments.mutable_revert_entry(i);
      EXPECT_EQ(std::string("test") + std::to_string(i), e->key);
      EXPECT_EQ(i, e->id);
    }
  }

  for (int i = 0; i < kSize; ++i) {
    Segments::RevertEntry *e = segments.mutable_revert_entry(i);
    e->id = kSize - i;
    e->key = "test2" + std::to_string(i);
  }

  for (int i = 0; i < kSize; ++i) {
    const Segments::RevertEntry &e = segments.revert_entry(i);
    EXPECT_EQ(std::string("test2") + std::to_string(i), e.key);
    EXPECT_EQ(kSize - i, e.id);
  }

  {
    const Segments::RevertEntry &src = segments.revert_entry(0);
    Segments::RevertEntry dest = src;
    EXPECT_EQ(src.revert_entry_type, dest.revert_entry_type);
    EXPECT_EQ(src.id, dest.id);
    EXPECT_EQ(src.timestamp, dest.timestamp);
    EXPECT_EQ(src.key, dest.key);
  }

  segments.clear_revert_entries();
  EXPECT_EQ(0, segments.revert_entries_size());
}

TEST(SegmentsTest, CopyTest) {
  Segments src;

  src.set_max_history_segments_size(1);
  src.set_resized(true);
  src.set_user_history_enabled(true);
  src.set_request_type(Segments::PREDICTION);

  constexpr int kSegmentsSize = 3;
  constexpr int kCandidatesSize = 2;

  for (int i = 0; i < kSegmentsSize; ++i) {
    Segment *segment = src.add_segment();
    segment->set_key(Util::StringPrintf("segment_%d", i));
    for (int j = 0; j < kCandidatesSize; ++j) {
      Segment::Candidate *candidate = segment->add_candidate();
      candidate->key = Util::StringPrintf("candidate_%d", i);
    }
  }
  EXPECT_EQ(kSegmentsSize, src.segments_size());
  EXPECT_EQ(kCandidatesSize, src.segment(0).candidates_size());

  Segments dest = src;
  EXPECT_EQ(src.max_history_segments_size(), dest.max_history_segments_size());
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
}

TEST(CandidateTest, functional_key) {
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

TEST(CandidateTest, functional_value) {
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

TEST(CandidateTest, InnerSegmentIterator) {
  {
    // For empty inner_segment_boundary, the initial state is done.
    Segment::Candidate candidate;
    candidate.Init();
    candidate.key = "testfoobar";
    candidate.value = "redgreenblue";
    Segment::Candidate::InnerSegmentIterator iter(&candidate);
    EXPECT_TRUE(iter.Done());
  }
  {
    //           key: test | foobar
    //         value:  red | greenblue
    //   content key: test | foo
    // content value:  red | green
    Segment::Candidate candidate;
    candidate.Init();
    candidate.key = "testfoobar";
    candidate.value = "redgreenblue";
    candidate.PushBackInnerSegmentBoundary(4, 3, 4, 3);
    candidate.PushBackInnerSegmentBoundary(6, 9, 3, 5);
    std::vector<absl::string_view> keys, values, content_keys, content_values;
    for (Segment::Candidate::InnerSegmentIterator iter(&candidate);
         !iter.Done(); iter.Next()) {
      keys.push_back(iter.GetKey());
      values.push_back(iter.GetValue());
      content_keys.push_back(iter.GetContentKey());
      content_values.push_back(iter.GetContentValue());
    }

    ASSERT_EQ(2, keys.size());
    EXPECT_EQ("test", keys[0]);
    EXPECT_EQ("foobar", keys[1]);

    ASSERT_EQ(2, values.size());
    EXPECT_EQ("red", values[0]);
    EXPECT_EQ("greenblue", values[1]);

    ASSERT_EQ(2, content_keys.size());
    EXPECT_EQ("test", content_keys[0]);
    EXPECT_EQ("foo", content_keys[1]);

    ASSERT_EQ(2, content_values.size());
    EXPECT_EQ("red", content_values[0]);
    EXPECT_EQ("green", content_values[1]);
  }
}

TEST(SegmentTest, Copy) {
  Segment src;

  src.set_key("key");
  src.set_segment_type(Segment::FIXED_VALUE);
  Segment::Candidate *candidate1 = src.add_candidate();
  candidate1->key = "candidate1->key";
  Segment::Candidate *candidate2 = src.add_candidate();
  candidate2->key = "candidate2->key";
  Segment::Candidate *meta_candidate = src.add_meta_candidate();
  meta_candidate->key = "meta_candidate->key";

  // Test copy constructor.
  Segment dest(src);
  EXPECT_EQ(src.key(), dest.key());
  EXPECT_EQ(src.segment_type(), dest.segment_type());
  EXPECT_EQ(src.candidate(0).key, dest.candidate(0).key);
  EXPECT_EQ(src.candidate(1).key, dest.candidate(1).key);
  EXPECT_EQ(src.meta_candidate(0).key, dest.meta_candidate(0).key);

  // Test copy assignment.
  dest.add_candidate()->key = "dummy";
  dest.add_candidate()->key = "dummy";
  dest = src;
  EXPECT_EQ(src.key(), dest.key());
  EXPECT_EQ(src.segment_type(), dest.segment_type());
  EXPECT_EQ(src.candidate(0).key, dest.candidate(0).key);
  EXPECT_EQ(src.candidate(1).key, dest.candidate(1).key);
  EXPECT_EQ(src.meta_candidate(0).key, dest.meta_candidate(0).key);
}

TEST(SegmentTest, MetaCandidateTest) {
  Segment segment;

  EXPECT_EQ(0, segment.meta_candidates_size());

  constexpr int kCandidatesSize = 5;
  std::vector<std::string> values;
  for (size_t i = 0; i < kCandidatesSize; ++i) {
    values.push_back(std::string(1, 'a' + i));
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
    const int meta_idx = -static_cast<int>(i) - 1;
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
    const int meta_idx = -static_cast<int>(i) - 1;
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
    const int meta_idx = -static_cast<int>(i) - 1;
    EXPECT_EQ(meta_idx, segment.indexOf(segment.mutable_candidate(meta_idx)));
  }

  EXPECT_EQ(segment.candidates_size(), segment.indexOf(nullptr));

  // mutable_meta_candidates
  {
    std::vector<Segment::Candidate> *meta_candidates =
        segment.mutable_meta_candidates();
    EXPECT_EQ(kCandidatesSize, meta_candidates->size());
    Segment::Candidate cand;
    cand.Init();
    cand.value = "Test";
    meta_candidates->push_back(cand);
  }

  // meta_candidates
  {
    const std::vector<Segment::Candidate> &meta_candidates =
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
}  // namespace mozc
