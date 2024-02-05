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

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "base/number_util.h"
#include "testing/gunit.h"

namespace mozc {

TEST(SegmentsTest, BasicTest) {
  Segments segments;

  EXPECT_EQ(segments.segments_size(), 0);

  constexpr int kSegmentsSize = 5;
  Segment *seg[kSegmentsSize];
  for (int i = 0; i < kSegmentsSize; ++i) {
    EXPECT_EQ(segments.segments_size(), i);
    seg[i] = segments.push_back_segment();
    EXPECT_EQ(segments.segments_size(), i + 1);
  }

  const std::string output = segments.DebugString();
  EXPECT_FALSE(output.empty());

  EXPECT_FALSE(segments.resized());
  segments.set_resized(true);
  EXPECT_TRUE(segments.resized());
  segments.set_resized(false);
  EXPECT_FALSE(segments.resized());

  segments.set_max_history_segments_size(10);
  EXPECT_EQ(segments.max_history_segments_size(), 10);

  segments.set_max_history_segments_size(5);
  EXPECT_EQ(segments.max_history_segments_size(), 5);

  for (int i = 0; i < kSegmentsSize; ++i) {
    EXPECT_EQ(segments.mutable_segment(i), seg[i]);
  }

  segments.pop_back_segment();
  EXPECT_EQ(segments.mutable_segment(3), seg[3]);

  segments.pop_front_segment();
  EXPECT_EQ(segments.mutable_segment(0), seg[1]);

  segments.Clear();
  EXPECT_EQ(segments.segments_size(), 0);

  for (int i = 0; i < kSegmentsSize; ++i) {
    seg[i] = segments.push_back_segment();
  }

  // erase
  segments.erase_segment(1);
  EXPECT_EQ(segments.mutable_segment(0), seg[0]);
  EXPECT_EQ(segments.mutable_segment(1), seg[2]);

  segments.erase_segments(1, 2);
  EXPECT_EQ(segments.mutable_segment(0), seg[0]);
  EXPECT_EQ(segments.mutable_segment(1), seg[4]);

  EXPECT_EQ(segments.segments_size(), 2);

  segments.erase_segments(0, 1);
  EXPECT_EQ(segments.segments_size(), 1);
  EXPECT_EQ(segments.mutable_segment(0), seg[4]);

  // insert
  seg[1] = segments.insert_segment(1);
  EXPECT_EQ(segments.mutable_segment(1), seg[1]);

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

  EXPECT_EQ(segments.history_segments_size(), 2);
  EXPECT_EQ(segments.conversion_segments_size(), 3);

  EXPECT_EQ(segments.mutable_conversion_segment(0), seg[2]);
  EXPECT_EQ(segments.mutable_history_segment(0), seg[0]);

  segments.mutable_history_segment(0)->add_candidate()->key = "a";
  segments.mutable_history_segment(1)->add_candidate()->key = "b";

  EXPECT_EQ(segments.history_key(), "ab");
  EXPECT_EQ(segments.history_key(1), "b");
  EXPECT_EQ(segments.history_key(2), "ab");
  EXPECT_EQ(segments.history_key(3), "ab");

  segments.mutable_history_segment(0)->mutable_candidate(0)->value = "A";
  segments.mutable_history_segment(1)->mutable_candidate(0)->value = "B";

  EXPECT_EQ(segments.history_value(), "AB");
  EXPECT_EQ(segments.history_value(1), "B");
  EXPECT_EQ(segments.history_value(2), "AB");
  EXPECT_EQ(segments.history_value(3), "AB");

  segments.clear_history_segments();
  EXPECT_EQ(segments.segments_size(), 3);

  segments.clear_conversion_segments();
  EXPECT_EQ(segments.segments_size(), 0);
}

TEST(CandidateTest, BasicTest) {
  Segment segment;

  const char str[] = "this is a test";
  segment.set_key(str);
  EXPECT_EQ(segment.key(), str);

  segment.set_segment_type(Segment::FIXED_BOUNDARY);
  EXPECT_EQ(segment.segment_type(), Segment::FIXED_BOUNDARY);

  constexpr int kCandidatesSize = 5;
  Segment::Candidate *cand[kCandidatesSize];
  for (int i = 0; i < kCandidatesSize; ++i) {
    EXPECT_EQ(segment.candidates_size(), i);
    cand[i] = segment.push_back_candidate();
    EXPECT_EQ(segment.candidates_size(), i + 1);
  }

  for (int i = 0; i < kCandidatesSize; ++i) {
    EXPECT_EQ(segment.mutable_candidate(i), cand[i]);
  }

  for (int i = 0; i < kCandidatesSize; ++i) {
    EXPECT_TRUE(segment.is_valid_index(i));
  }
  EXPECT_FALSE(segment.is_valid_index(kCandidatesSize));

  const int kMetaCandidatesSize =
      static_cast<int>(segment.meta_candidates_size());
  for (int i = -kMetaCandidatesSize; i < 0; ++i) {
    EXPECT_TRUE(segment.is_valid_index(i));
  }
  EXPECT_TRUE(segment.is_valid_index(-kMetaCandidatesSize));
  EXPECT_FALSE(segment.is_valid_index(-kMetaCandidatesSize - 1));

  segment.pop_back_candidate();
  EXPECT_EQ(segment.mutable_candidate(3), cand[3]);

  segment.pop_front_candidate();
  EXPECT_EQ(segment.mutable_candidate(0), cand[1]);

  // Pop functions against empty candidates should not raise a runtime error.
  segment.Clear();
  segment.pop_back_candidate();
  segment.pop_front_candidate();
  EXPECT_EQ(segment.candidates_size(), 0);

  for (int i = 0; i < kCandidatesSize; ++i) {
    cand[i] = segment.push_back_candidate();
  }

  // erase
  segment.erase_candidate(1);
  EXPECT_EQ(segment.mutable_candidate(0), cand[0]);
  EXPECT_EQ(segment.mutable_candidate(1), cand[2]);

  segment.erase_candidates(1, 2);
  EXPECT_EQ(segment.mutable_candidate(0), cand[0]);
  EXPECT_EQ(segment.mutable_candidate(1), cand[4]);
  EXPECT_EQ(segment.candidates_size(), 2);

  // insert
  cand[1] = segment.insert_candidate(1);
  EXPECT_EQ(segment.mutable_candidate(1), cand[1]);
  EXPECT_EQ(segment.mutable_candidate(2), cand[4]);

  // insert with a candidate
  auto c0 = std::make_unique<Segment::Candidate>();
  c0->value = "c0";
  segment.insert_candidate(1, std::move(c0));
  EXPECT_EQ(segment.mutable_candidate(0), cand[0]);
  EXPECT_EQ(segment.candidate(1).value, "c0");
  EXPECT_EQ(segment.mutable_candidate(2), cand[1]);

  // insert multiple candidates
  std::vector<std::unique_ptr<Segment::Candidate>> cand_vec;
  cand[2] = cand_vec.emplace_back(std::make_unique<Segment::Candidate>()).get();
  cand[3] = cand_vec.emplace_back(std::make_unique<Segment::Candidate>()).get();
  segment.insert_candidates(2, std::move(cand_vec));
  EXPECT_EQ(segment.candidate(1).value, "c0");
  EXPECT_EQ(segment.mutable_candidate(2), cand[2]);
  EXPECT_EQ(segment.mutable_candidate(3), cand[3]);
  EXPECT_EQ(segment.mutable_candidate(4), cand[1]);
  EXPECT_EQ(segment.mutable_candidate(5), cand[4]);

  // insert candidate with index out of the range.
  segment.Clear();
  EXPECT_EQ(segment.candidates_size(), 0);
  auto c3 = std::make_unique<Segment::Candidate>();
  segment.insert_candidate(-1, std::move(c3));  // less than lower
  EXPECT_EQ(segment.candidates_size(), 1);
  auto c4 = std::make_unique<Segment::Candidate>();
  segment.insert_candidate(5, std::move(c4));  // more than upper
  EXPECT_EQ(segment.candidates_size(), 2);

  // insert candidates with index out of the range.
  segment.Clear();
  EXPECT_EQ(segment.candidates_size(), 0);
  std::vector<std::unique_ptr<Segment::Candidate>> cand_vec2(2);
  segment.insert_candidates(-1, std::move(cand_vec2));  // less than lower
  EXPECT_EQ(segment.candidates_size(), 2);
  std::vector<std::unique_ptr<Segment::Candidate>> cand_vec3(3);
  segment.insert_candidates(5, std::move(cand_vec3));  // more than upper
  EXPECT_EQ(segment.candidates_size(), 5);

  // move
  segment.Clear();
  for (int i = 0; i < kCandidatesSize; ++i) {
    cand[i] = segment.push_back_candidate();
  }

  segment.move_candidate(2, 0);
  EXPECT_EQ(segment.mutable_candidate(0), cand[2]);
  EXPECT_EQ(segment.mutable_candidate(1), cand[0]);
  EXPECT_EQ(segment.mutable_candidate(2), cand[1]);
}

TEST(CandidateTest, IsValid) {
  Segment::Candidate c;
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
  EXPECT_EQ(segments.revert_entries_size(), 0);

  constexpr int kSize = 10;
  for (int i = 0; i < kSize; ++i) {
    Segments::RevertEntry *e = segments.push_back_revert_entry();
    e->key = "test" + std::to_string(i);
    e->id = i;
  }

  EXPECT_EQ(segments.revert_entries_size(), kSize);

  for (int i = 0; i < kSize; ++i) {
    {
      const Segments::RevertEntry &e = segments.revert_entry(i);
      EXPECT_EQ(e.key, std::string("test") + std::to_string(i));
      EXPECT_EQ(e.id, i);
    }
    {
      Segments::RevertEntry *e = segments.mutable_revert_entry(i);
      EXPECT_EQ(e->key, std::string("test") + std::to_string(i));
      EXPECT_EQ(e->id, i);
    }
  }

  for (int i = 0; i < kSize; ++i) {
    Segments::RevertEntry *e = segments.mutable_revert_entry(i);
    e->id = kSize - i;
    e->key = "test2" + std::to_string(i);
  }

  for (int i = 0; i < kSize; ++i) {
    const Segments::RevertEntry &e = segments.revert_entry(i);
    EXPECT_EQ(e.key, std::string("test2") + std::to_string(i));
    EXPECT_EQ(e.id, kSize - i);
  }

  segments.clear_revert_entries();
  EXPECT_EQ(segments.revert_entries_size(), 0);
}

TEST(SegmentsTest, CopyTest) {
  Segments src;

  src.set_max_history_segments_size(1);
  src.set_resized(true);

  constexpr int kSegmentsSize = 3;
  constexpr int kCandidatesSize = 2;

  for (int i = 0; i < kSegmentsSize; ++i) {
    Segment *segment = src.add_segment();
    segment->set_key(absl::StrFormat("segment_%d", i));
    for (int j = 0; j < kCandidatesSize; ++j) {
      Segment::Candidate *candidate = segment->add_candidate();
      candidate->key = absl::StrFormat("candidate_%d", i);
    }
  }
  EXPECT_EQ(src.segments_size(), kSegmentsSize);
  EXPECT_EQ(src.segment(0).candidates_size(), kCandidatesSize);

  Segments dest = src;
  EXPECT_EQ(dest.max_history_segments_size(), src.max_history_segments_size());
  EXPECT_EQ(dest.resized(), src.resized());

  EXPECT_EQ(dest.segments_size(), kSegmentsSize);
  EXPECT_EQ(dest.segment(0).candidates_size(), kCandidatesSize);

  for (int i = 0; i < kSegmentsSize; ++i) {
    EXPECT_EQ(dest.segment(i).key(), src.segment(i).key());
    for (int j = 0; j < kCandidatesSize; ++j) {
      EXPECT_EQ(src.segment(i).candidate(j).key,
                dest.segment(i).candidate(j).key);
    }
  }
}

TEST(CandidateTest, functional_key) {
  Segment::Candidate candidate;

  candidate.key = "testfoobar";
  candidate.content_key = "test";
  EXPECT_EQ(candidate.functional_key(), "foobar");

  candidate.key = "testfoo";
  candidate.content_key = "test";
  EXPECT_EQ(candidate.functional_key(), "foo");

  // This is unexpected key/context_key.
  // This method doesn't check the prefix part.
  candidate.key = "abcdefg";
  candidate.content_key = "test";
  EXPECT_EQ(candidate.functional_key(), "efg");

  candidate.key = "test";
  candidate.content_key = "test";
  EXPECT_EQ(candidate.functional_key(), "");

  candidate.key = "test";
  candidate.content_key = "testfoobar";
  EXPECT_EQ(candidate.functional_key(), "");

  candidate.key = "";
  candidate.content_key = "";
  EXPECT_EQ(candidate.functional_key(), "");
}

TEST(CandidateTest, functional_value) {
  Segment::Candidate candidate;

  candidate.value = "testfoobar";
  candidate.content_value = "test";
  EXPECT_EQ(candidate.functional_value(), "foobar");

  candidate.value = "testfoo";
  candidate.content_value = "test";
  EXPECT_EQ(candidate.functional_value(), "foo");

  // This is unexpected value/context_value.
  // This method doesn't check the prefix part.
  candidate.value = "abcdefg";
  candidate.content_value = "test";
  EXPECT_EQ(candidate.functional_value(), "efg");

  candidate.value = "test";
  candidate.content_value = "test";
  EXPECT_EQ(candidate.functional_value(), "");

  candidate.value = "test";
  candidate.content_value = "testfoobar";
  EXPECT_EQ(candidate.functional_value(), "");

  candidate.value = "";
  candidate.content_value = "";
  EXPECT_EQ(candidate.functional_value(), "");
}

TEST(CandidateTest, InnerSegmentIterator) {
  {
    // For empty inner_segment_boundary, the initial state is done.
    Segment::Candidate candidate;
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
    candidate.key = "testfoobar";
    candidate.value = "redgreenblue";
    candidate.PushBackInnerSegmentBoundary(4, 3, 4, 3);
    candidate.PushBackInnerSegmentBoundary(6, 9, 3, 5);
    std::vector<absl::string_view> keys, values, content_keys, content_values,
        functional_keys, functional_values;
    for (Segment::Candidate::InnerSegmentIterator iter(&candidate);
         !iter.Done(); iter.Next()) {
      keys.push_back(iter.GetKey());
      values.push_back(iter.GetValue());
      content_keys.push_back(iter.GetContentKey());
      content_values.push_back(iter.GetContentValue());
      functional_keys.push_back(iter.GetFunctionalKey());
      functional_values.push_back(iter.GetFunctionalValue());
    }

    ASSERT_EQ(keys.size(), 2);
    EXPECT_EQ(keys[0], "test");
    EXPECT_EQ(keys[1], "foobar");

    ASSERT_EQ(values.size(), 2);
    EXPECT_EQ(values[0], "red");
    EXPECT_EQ(values[1], "greenblue");

    ASSERT_EQ(content_keys.size(), 2);
    EXPECT_EQ(content_keys[0], "test");
    EXPECT_EQ(content_keys[1], "foo");

    ASSERT_EQ(content_values.size(), 2);
    EXPECT_EQ(content_values[0], "red");
    EXPECT_EQ(content_values[1], "green");

    ASSERT_EQ(functional_keys.size(), 2);
    EXPECT_EQ(functional_keys[0], "");
    EXPECT_EQ(functional_keys[1], "bar");

    ASSERT_EQ(functional_values.size(), 2);
    EXPECT_EQ(functional_values[0], "");
    EXPECT_EQ(functional_values[1], "blue");
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
  EXPECT_EQ(dest.key(), src.key());
  EXPECT_EQ(dest.segment_type(), src.segment_type());
  EXPECT_EQ(dest.candidate(0).key, src.candidate(0).key);
  EXPECT_EQ(dest.candidate(1).key, src.candidate(1).key);
  EXPECT_EQ(dest.meta_candidate(0).key, src.meta_candidate(0).key);

  // Test copy assignment.
  dest.add_candidate()->key = "dummy";
  dest.add_candidate()->key = "dummy";
  dest = src;
  EXPECT_EQ(dest.key(), src.key());
  EXPECT_EQ(dest.segment_type(), src.segment_type());
  EXPECT_EQ(dest.candidate(0).key, src.candidate(0).key);
  EXPECT_EQ(dest.candidate(1).key, src.candidate(1).key);
  EXPECT_EQ(dest.meta_candidate(0).key, src.meta_candidate(0).key);
}

TEST(SegmentTest, MetaCandidateTest) {
  Segment segment;

  EXPECT_EQ(segment.meta_candidates_size(), 0);

  constexpr int kCandidatesSize = 5;
  std::vector<std::string> values;
  for (size_t i = 0; i < kCandidatesSize; ++i) {
    values.push_back(std::string(1, 'a' + i));
  }

  // add_meta_candidate()
  for (size_t i = 0; i < kCandidatesSize; ++i) {
    EXPECT_EQ(segment.meta_candidates_size(), i);
    Segment::Candidate *cand = segment.add_meta_candidate();
    cand->value = values[i];
    EXPECT_EQ(segment.meta_candidates_size(), i + 1);
  }

  // mutable_candidate()
  for (size_t i = 0; i < kCandidatesSize; ++i) {
    const int meta_idx = -static_cast<int>(i) - 1;
    Segment::Candidate *cand = segment.mutable_candidate(meta_idx);
    EXPECT_EQ(cand->value, values[i]);
  }

  // mutable_meta_candidate()
  for (size_t i = 0; i < kCandidatesSize; ++i) {
    Segment::Candidate *cand = segment.mutable_meta_candidate(i);
    EXPECT_EQ(cand->value, values[i]);
  }

  // candidate()
  for (size_t i = 0; i < kCandidatesSize; ++i) {
    const int meta_idx = -static_cast<int>(i) - 1;
    const Segment::Candidate &cand = segment.candidate(meta_idx);
    EXPECT_EQ(cand.value, values[i]);
  }

  // meta_candidate()
  for (size_t i = 0; i < kCandidatesSize; ++i) {
    const Segment::Candidate &cand = segment.meta_candidate(i);
    EXPECT_EQ(cand.value, values[i]);
  }

  // mutable_meta_candidates
  {
    std::vector<Segment::Candidate> *meta_candidates =
        segment.mutable_meta_candidates();
    EXPECT_EQ(meta_candidates->size(), kCandidatesSize);
    Segment::Candidate cand;
    cand.value = "Test";
    meta_candidates->push_back(std::move(cand));
  }

  // meta_candidates
  {
    const std::vector<Segment::Candidate> &meta_candidates =
        segment.meta_candidates();
    EXPECT_EQ(meta_candidates.size(), kCandidatesSize + 1);
    for (size_t i = 0; i < kCandidatesSize; ++i) {
      EXPECT_EQ(meta_candidates[i].value, values[i]);
    }
    EXPECT_EQ(meta_candidates[kCandidatesSize].value, "Test");
  }
  // clear
  segment.clear_meta_candidates();
  EXPECT_EQ(segment.meta_candidates_size(), 0);
}
}  // namespace mozc
