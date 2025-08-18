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

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "converter/candidate.h"
#include "testing/gmock.h"
#include "testing/gunit.h"
#include "testing/test_peer.h"

namespace mozc {
namespace converter {

class SegmentsPoolAccessorTestPeer : testing::TestPeer<Segments> {
 public:
  explicit SegmentsPoolAccessorTestPeer(Segments& segments)
      : testing::TestPeer<Segments>(segments) {}

  size_t released_size() const { return value_.pool_.released_size(); }
};

using ::testing::ElementsAre;

template <typename T>
static std::vector<absl::string_view> ToKeys(const T& segments) {
  std::vector<absl::string_view> keys;
  for (const Segment& segment : segments) {
    keys.push_back(segment.key());
  }
  return keys;
}

TEST(SegmentsTest, BasicTest) {
  Segments segments;

  EXPECT_EQ(segments.segments_size(), 0);

  constexpr int kSegmentsSize = 5;
  Segment* seg[kSegmentsSize];
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

  const SegmentsPoolAccessorTestPeer sp(segments);

  // erase
  segments.erase_segment(1);
  EXPECT_EQ(segments.mutable_segment(0), seg[0]);
  EXPECT_EQ(segments.mutable_segment(1), seg[2]);
  EXPECT_EQ(sp.released_size(), 1);

  segments.erase_segments(1, 2);
  EXPECT_EQ(segments.mutable_segment(0), seg[0]);
  EXPECT_EQ(segments.mutable_segment(1), seg[4]);
  EXPECT_EQ(sp.released_size(), 3);

  EXPECT_EQ(segments.segments_size(), 2);

  segments.erase_segments(0, 1);
  EXPECT_EQ(segments.segments_size(), 1);
  EXPECT_EQ(segments.mutable_segment(0), seg[4]);
  EXPECT_EQ(sp.released_size(), 4);

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

TEST(SegmentTest, CandidateTest) {
  Segment segment;

  const absl::string_view str = "this is a test";
  segment.set_key(str);
  EXPECT_EQ(segment.key(), str);

  segment.set_segment_type(Segment::FIXED_BOUNDARY);
  EXPECT_EQ(segment.segment_type(), Segment::FIXED_BOUNDARY);

  constexpr int kCandidatesSize = 5;
  Candidate* cand[kCandidatesSize];
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
  auto c0 = std::make_unique<Candidate>();
  c0->value = "c0";
  segment.insert_candidate(1, std::move(c0));
  EXPECT_EQ(segment.mutable_candidate(0), cand[0]);
  EXPECT_EQ(segment.candidate(1).value, "c0");
  EXPECT_EQ(segment.mutable_candidate(2), cand[1]);

  // insert multiple candidates
  std::vector<std::unique_ptr<Candidate>> cand_vec;
  cand[2] = cand_vec.emplace_back(std::make_unique<Candidate>()).get();
  cand[3] = cand_vec.emplace_back(std::make_unique<Candidate>()).get();
  segment.insert_candidates(2, std::move(cand_vec));
  EXPECT_EQ(segment.candidate(1).value, "c0");
  EXPECT_EQ(segment.mutable_candidate(2), cand[2]);
  EXPECT_EQ(segment.mutable_candidate(3), cand[3]);
  EXPECT_EQ(segment.mutable_candidate(4), cand[1]);
  EXPECT_EQ(segment.mutable_candidate(5), cand[4]);

  // insert candidate with index out of the range.
  segment.Clear();
  EXPECT_EQ(segment.candidates_size(), 0);
  auto c3 = std::make_unique<Candidate>();
  segment.insert_candidate(-1, std::move(c3));  // less than lower
  EXPECT_EQ(segment.candidates_size(), 1);
  auto c4 = std::make_unique<Candidate>();
  segment.insert_candidate(5, std::move(c4));  // more than upper
  EXPECT_EQ(segment.candidates_size(), 2);

  // insert candidates with index out of the range.
  segment.Clear();
  EXPECT_EQ(segment.candidates_size(), 0);
  std::vector<std::unique_ptr<Candidate>> cand_vec2(2);
  segment.insert_candidates(-1, std::move(cand_vec2));  // less than lower
  EXPECT_EQ(segment.candidates_size(), 2);
  std::vector<std::unique_ptr<Candidate>> cand_vec3(3);
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

TEST(SegmentsTest, RevertEntryTest) {
  Segments segments;
  EXPECT_EQ(segments.revert_id(), 0);

  segments.set_revert_id(123);
  EXPECT_EQ(segments.revert_id(), 123);

  segments.Clear();
  EXPECT_EQ(segments.revert_id(), 0);
}

TEST(SegmentsTest, CopyTest) {
  Segments src;

  src.set_max_history_segments_size(1);
  src.set_resized(true);

  constexpr int kSegmentsSize = 3;
  constexpr int kCandidatesSize = 2;

  for (int i = 0; i < kSegmentsSize; ++i) {
    Segment* segment = src.add_segment();
    segment->set_key(absl::StrFormat("segment_%d", i));
    for (int j = 0; j < kCandidatesSize; ++j) {
      Candidate* candidate = segment->add_candidate();
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

TEST(SegmentsTest, InitForConvert) {
  Segments segments;
  segments.InitForConvert("first");
  EXPECT_EQ(segments.conversion_segments_size(), 1);
  EXPECT_EQ(segments.conversion_segment(0).key(), "first");
  EXPECT_EQ(segments.conversion_segment(0).segment_type(), Segment::FREE);
  EXPECT_EQ(segments.conversion_segment(0).candidates_size(), 0);

  Segment* segment = segments.add_segment();
  segment->set_key("second");
  EXPECT_EQ(segments.conversion_segments_size(), 2);

  segments.InitForConvert("update");
  EXPECT_EQ(segments.conversion_segments_size(), 1);
  EXPECT_EQ(segments.conversion_segment(0).key(), "update");
  EXPECT_EQ(segments.conversion_segment(0).segment_type(), Segment::FREE);
}

TEST(SegmentsTest, InitForCommit) {
  Segments segments;
  segments.InitForCommit("key", "value");
  EXPECT_EQ(segments.conversion_segments_size(), 1);
  const Segment& segment = segments.conversion_segment(0);
  EXPECT_EQ(segment.key(), "key");
  EXPECT_EQ(segment.segment_type(), Segment::FIXED_VALUE);
  ASSERT_EQ(segment.candidates_size(), 1);
  EXPECT_EQ(segment.candidate(0).key, "key");
  EXPECT_EQ(segment.candidate(0).value, "value");
  EXPECT_EQ(segment.candidate(0).content_key, "key");
  EXPECT_EQ(segment.candidate(0).content_value, "value");
}

TEST(SegmentsTest, PrependCandidates) {
  Segments segments;
  segments.InitForConvert("key");
  EXPECT_EQ(segments.conversion_segments_size(), 1);
  EXPECT_EQ(segments.conversion_segment(0).key(), "key");
  EXPECT_EQ(segments.conversion_segment(0).segment_type(), Segment::FREE);
  EXPECT_EQ(segments.conversion_segment(0).candidates_size(), 0);
  EXPECT_EQ(segments.conversion_segment(0).meta_candidates_size(), 0);

  Candidate* candidate =
      segments.mutable_conversion_segment(0)->add_candidate();
  candidate->key = "key";
  candidate->value = "base";

  std::vector<Candidate>* meta_candidates =
      segments.mutable_conversion_segment(0)->mutable_meta_candidates();
  Candidate& meta_candidate = meta_candidates->emplace_back();
  meta_candidate.key = "key";
  meta_candidate.value = "meta";

  EXPECT_EQ(segments.conversion_segment(0).candidates_size(), 1);
  EXPECT_EQ(segments.conversion_segment(0).meta_candidates_size(), 1);

  Segment prepended_segment;
  {
    prepended_segment.set_key("key2");
    Candidate* candidate1 = prepended_segment.add_candidate();
    candidate1->key = "key2";
    candidate1->value = "prepended1";
    Candidate* candidate2 = prepended_segment.add_candidate();
    candidate2->key = "key2";
    candidate2->value = "prepended2";

    std::vector<Candidate>* meta_candidates =
        prepended_segment.mutable_meta_candidates();
    Candidate& meta_candidate = meta_candidates->emplace_back();
    meta_candidate.key = "key2";
    meta_candidate.value = "prepended_meta";
  }

  segments.PrependCandidates(prepended_segment);
  EXPECT_EQ(segments.conversion_segments_size(), 1);
  EXPECT_EQ(segments.conversion_segment(0).key(), "key");
  EXPECT_EQ(segments.conversion_segment(0).candidates_size(), 3);
  EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, "prepended1");
  EXPECT_EQ(segments.conversion_segment(0).candidate(1).value, "prepended2");
  EXPECT_EQ(segments.conversion_segment(0).candidate(2).value, "base");

  EXPECT_EQ(segments.conversion_segment(0).meta_candidates_size(), 1);
  EXPECT_EQ(segments.conversion_segment(0).meta_candidate(0).value,
            "prepended_meta");
}

namespace {
Segment& AddSegment(absl::string_view key, Segment::SegmentType type,
                    Segments& segments) {
  Segment* segment = segments.add_segment();
  segment->set_key(key);
  segment->set_segment_type(type);
  return *segment;
}

void PushBackCandidate(absl::string_view text, Segment& segment) {
  Candidate* cand = segment.push_back_candidate();
  cand->key = std::string(text);
  cand->content_key = cand->key;
  cand->value = cand->key;
  cand->content_value = cand->key;
}
}  // namespace

TEST(SegmentsTest, Resize) {
  constexpr Segment::SegmentType kFixedBoundary = Segment::FIXED_BOUNDARY;
  constexpr Segment::SegmentType kFree = Segment::FREE;

  {
    // Resize {"あいうえ"} to {"あいう", "え"}
    Segments segments;
    AddSegment("あいうえ", kFree, segments);
    const int start_segment_index = 0;
    const std::array<uint8_t, 2> size_array = {3, 1};
    EXPECT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_TRUE(segments.Resize(start_segment_index, size_array));
    ASSERT_EQ(segments.conversion_segments_size(), 2);
    EXPECT_EQ(segments.conversion_segment(0).key(), "あいう");
    EXPECT_EQ(segments.conversion_segment(1).key(), "え");
    EXPECT_EQ(segments.conversion_segment(0).segment_type(), kFixedBoundary);
    EXPECT_EQ(segments.conversion_segment(1).segment_type(), kFixedBoundary);
  }

  {
    // Empty resizes
    Segments segments;
    AddSegment("あいうえ", kFree, segments);
    const int start_segment_index = 0;
    const std::array<uint8_t, 0> size_array = {};
    EXPECT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_FALSE(segments.Resize(start_segment_index, size_array));
    ASSERT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_EQ(segments.conversion_segment(0).key(), "あいうえ");
    EXPECT_EQ(segments.conversion_segment(0).segment_type(), kFree);
  }

  {
    // Resize {"あいうえ"} to {"あいう", "え"} with history segments.
    // Even if segments has histroy segments, arguments for ResizeSegment is not
    // changed.
    Segments segments;
    Segment& history0 = AddSegment("やゆよ", Segment::HISTORY, segments);
    PushBackCandidate("ヤユヨ", history0);
    Segment& history1 = AddSegment("わをん", Segment::HISTORY, segments);
    PushBackCandidate("ワヲン", history1);
    AddSegment("あいうえ", Segment::FREE, segments);
    // start_segment_index should skip history segments.
    const int start_segment_index = 2;
    const std::array<uint8_t, 2> size_array = {3, 1};
    EXPECT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_TRUE(segments.Resize(start_segment_index, size_array));
    ASSERT_EQ(segments.conversion_segments_size(), 2);
    EXPECT_EQ(segments.conversion_segment(0).key(), "あいう");
    EXPECT_EQ(segments.conversion_segment(1).key(), "え");
    EXPECT_EQ(segments.conversion_segment(0).segment_type(), kFixedBoundary);
    EXPECT_EQ(segments.conversion_segment(1).segment_type(), kFixedBoundary);
  }

  {
    // Resize {"あいうえ"} to {"あいう", "え"} where size_array contains 0
    // values.
    Segments segments;
    AddSegment("あいうえ", Segment::FREE, segments);
    const int start_segment_index = 0;
    const std::array<uint8_t, 8> size_array = {3, 1, 0, 0, 0, 0, 0, 0};
    EXPECT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_TRUE(segments.Resize(start_segment_index, size_array));
    ASSERT_EQ(segments.conversion_segments_size(), 2);
    EXPECT_EQ(segments.conversion_segment(0).key(), "あいう");
    EXPECT_EQ(segments.conversion_segment(1).key(), "え");
    EXPECT_EQ(segments.conversion_segment(0).segment_type(), kFixedBoundary);
    EXPECT_EQ(segments.conversion_segment(1).segment_type(), kFixedBoundary);
  }

  {
    // Resize {"あいうえおかきくけ"} to {"あい", "うえ", "お", "かき", "くけ"}
    Segments segments;
    AddSegment("あいうえおかきくけ", Segment::FREE, segments);
    const int start_segment_index = 0;
    const std::array<uint8_t, 5> size_array = {2, 2, 1, 2, 2};
    EXPECT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_TRUE(segments.Resize(start_segment_index, size_array));
    ASSERT_EQ(segments.conversion_segments_size(), 5);
    EXPECT_EQ(segments.conversion_segment(0).key(), "あい");
    EXPECT_EQ(segments.conversion_segment(1).key(), "うえ");
    EXPECT_EQ(segments.conversion_segment(2).key(), "お");
    EXPECT_EQ(segments.conversion_segment(3).key(), "かき");
    EXPECT_EQ(segments.conversion_segment(4).key(), "くけ");
    EXPECT_EQ(segments.conversion_segment(0).segment_type(), kFixedBoundary);
    EXPECT_EQ(segments.conversion_segment(1).segment_type(), kFixedBoundary);
    EXPECT_EQ(segments.conversion_segment(2).segment_type(), kFixedBoundary);
    EXPECT_EQ(segments.conversion_segment(3).segment_type(), kFixedBoundary);
    EXPECT_EQ(segments.conversion_segment(4).segment_type(), kFixedBoundary);
  }

  {
    // Resize {"あいう", "えお", "かき", "くけ"} to
    // {"あいうえ", "お", "かきくけ"}
    Segments segments;
    AddSegment("あいう", Segment::FREE, segments);
    AddSegment("えお", Segment::FREE, segments);
    AddSegment("かき", Segment::FREE, segments);
    AddSegment("くけ", Segment::FREE, segments);
    const int start_segment_index = 0;
    const std::array<uint8_t, 3> size_array = {4, 1, 4};
    EXPECT_EQ(segments.conversion_segments_size(), 4);
    EXPECT_TRUE(segments.Resize(start_segment_index, size_array));
    ASSERT_EQ(segments.conversion_segments_size(), 3);
    EXPECT_EQ(segments.conversion_segment(0).key(), "あいうえ");
    EXPECT_EQ(segments.conversion_segment(1).key(), "お");
    EXPECT_EQ(segments.conversion_segment(2).key(), "かきくけ");
    EXPECT_EQ(segments.conversion_segment(0).segment_type(), kFixedBoundary);
    EXPECT_EQ(segments.conversion_segment(1).segment_type(), kFixedBoundary);
    EXPECT_EQ(segments.conversion_segment(2).segment_type(), kFixedBoundary);
  }

  {
    // Resize {"あいう", "えお", "かき", "くけ"} to
    // {"あいうえ", "お"} and keeping {"かき", "くけ"} as-is.
    Segments segments;
    AddSegment("あいう", Segment::FREE, segments);
    AddSegment("えお", Segment::FREE, segments);
    AddSegment("かき", Segment::FREE, segments);
    AddSegment("くけ", Segment::FREE, segments);
    const int start_segment_index = 0;
    const std::array<uint8_t, 2> size_array = {4, 1};
    EXPECT_EQ(segments.conversion_segments_size(), 4);
    EXPECT_TRUE(segments.Resize(start_segment_index, size_array));
    ASSERT_EQ(segments.conversion_segments_size(), 4);
    EXPECT_EQ(segments.conversion_segment(0).key(), "あいうえ");
    EXPECT_EQ(segments.conversion_segment(1).key(), "お");
    EXPECT_EQ(segments.conversion_segment(2).key(), "かき");
    EXPECT_EQ(segments.conversion_segment(3).key(), "くけ");
    EXPECT_EQ(segments.conversion_segment(0).segment_type(), kFixedBoundary);
    EXPECT_EQ(segments.conversion_segment(1).segment_type(), kFixedBoundary);
    EXPECT_EQ(segments.conversion_segment(2).segment_type(), kFree);
    EXPECT_EQ(segments.conversion_segment(3).segment_type(), kFree);
  }

  {
    // Resize {"あいうえ"} to {"あいう"} and keeping {"え"} as-is.
    Segments segments;
    AddSegment("あいうえ", Segment::FREE, segments);
    const int start_segment_index = 0;
    const std::array<uint8_t, 1> size_array = {3};
    EXPECT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_TRUE(segments.Resize(start_segment_index, size_array));
    ASSERT_EQ(segments.conversion_segments_size(), 2);
    EXPECT_EQ(segments.conversion_segment(0).key(), "あいう");
    EXPECT_EQ(segments.conversion_segment(1).key(), "え");
    EXPECT_EQ(segments.conversion_segment(0).segment_type(), kFixedBoundary);
    // Non specified segment (i.e. "え") is FREE to keep the consistency
    // with ResizeSegment.
    EXPECT_EQ(segments.conversion_segment(1).segment_type(), kFree);
  }

  {
    // Resize {"あいう", "えお", "かき", "くけ"} to {"かきくけ"} while
    // {"あいう", "えお"} are free to be modified.
    Segments segments;
    AddSegment("あいう", Segment::FREE, segments);
    AddSegment("えお", Segment::FREE, segments);
    AddSegment("かき", Segment::FREE, segments);
    AddSegment("くけ", Segment::FREE, segments);
    const int start_segment_index = 2;
    const std::array<uint8_t, 1> size_array = {4};
    EXPECT_EQ(segments.conversion_segments_size(), 4);
    EXPECT_TRUE(segments.Resize(start_segment_index, size_array));

    // Since {"あいう", "えお"} may be modified too, the segment index for
    // "かきくけ" may be different from 2.
    const size_t resized_size = segments.conversion_segments_size();
    const Segment& last_segment = segments.conversion_segment(resized_size - 1);
    EXPECT_EQ(last_segment.key(), "かきくけ");
    EXPECT_EQ(last_segment.segment_type(), kFixedBoundary);
  }
}

TEST(SegmentTest, KeyLength) {
  Segment segment;
  segment.set_key("test");
  EXPECT_EQ(segment.key_len(), 4);
  segment.set_key("あいう");
  EXPECT_EQ(segment.key_len(), 3);
}

TEST(SegmentTest, Copy) {
  Segment src;

  src.set_key("key");
  src.set_segment_type(Segment::FIXED_VALUE);
  Candidate* candidate1 = src.add_candidate();
  candidate1->key = "candidate1->key";
  Candidate* candidate2 = src.add_candidate();
  candidate2->key = "candidate2->key";
  Candidate* meta_candidate = src.add_meta_candidate();
  meta_candidate->key = "meta_candidate->key";

  // Test copy constructor.
  Segment dest(src);
  EXPECT_EQ(dest.key(), src.key());
  EXPECT_EQ(dest.key_len(), src.key_len());
  EXPECT_EQ(dest.segment_type(), src.segment_type());
  EXPECT_EQ(dest.candidate(0).key, src.candidate(0).key);
  EXPECT_EQ(dest.candidate(1).key, src.candidate(1).key);
  EXPECT_EQ(dest.meta_candidate(0).key, src.meta_candidate(0).key);

  // Test copy assignment.
  dest.add_candidate()->key = "placeholder";
  dest.add_candidate()->key = "placeholder";
  dest = src;
  EXPECT_EQ(dest.key(), src.key());
  EXPECT_EQ(dest.key_len(), src.key_len());
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
    Candidate* cand = segment.add_meta_candidate();
    cand->value = values[i];
    EXPECT_EQ(segment.meta_candidates_size(), i + 1);
  }

  // mutable_candidate()
  for (size_t i = 0; i < kCandidatesSize; ++i) {
    const int meta_idx = -static_cast<int>(i) - 1;
    Candidate* cand = segment.mutable_candidate(meta_idx);
    EXPECT_EQ(cand->value, values[i]);
  }

  // mutable_meta_candidate()
  for (size_t i = 0; i < kCandidatesSize; ++i) {
    Candidate* cand = segment.mutable_meta_candidate(i);
    EXPECT_EQ(cand->value, values[i]);
  }

  // candidate()
  for (size_t i = 0; i < kCandidatesSize; ++i) {
    const int meta_idx = -static_cast<int>(i) - 1;
    const Candidate& cand = segment.candidate(meta_idx);
    EXPECT_EQ(cand.value, values[i]);
  }

  // meta_candidate()
  for (size_t i = 0; i < kCandidatesSize; ++i) {
    const Candidate& cand = segment.meta_candidate(i);
    EXPECT_EQ(cand.value, values[i]);
  }

  // mutable_meta_candidates
  {
    std::vector<Candidate>* meta_candidates = segment.mutable_meta_candidates();
    EXPECT_EQ(meta_candidates->size(), kCandidatesSize);
    Candidate cand;
    cand.value = "Test";
    meta_candidates->push_back(std::move(cand));
  }

  // meta_candidates
  {
    absl::Span<const Candidate> meta_candidates = segment.meta_candidates();
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

TEST(SegmentTest, Iterator) {
  // Create a test `Segments`.
  constexpr int kHistorySize = 2;
  constexpr int kConversionSize = 3;
  Segments segments;
  std::vector<Segment*> segment_list;
  for (int i = 0; i < kHistorySize; ++i) {
    Segment* segment = segments.push_back_segment();
    segment->set_segment_type(Segment::HISTORY);
    segment_list.push_back(segment);
  }
  for (int i = 0; i < kConversionSize; ++i) {
    Segment* segment = segments.push_back_segment();
    segment->set_segment_type(Segment::FIXED_VALUE);
    segment_list.push_back(segment);
  }

  // Test the iterator for `Segments`.
  int i = 0;
  for (const Segment& segment : segments) {
    EXPECT_EQ(&segment, segment_list[i++]);
  }
  EXPECT_EQ(i, kHistorySize + kConversionSize);

  // Test the iterator for `Segments::history_segments()`.
  i = 0;
  for (const Segment& segment : segments.history_segments()) {
    EXPECT_EQ(&segment, segment_list[i++]);
  }
  EXPECT_EQ(i, kHistorySize);
  EXPECT_EQ(segments.history_segments_size(), kHistorySize);
  EXPECT_EQ(segments.history_segments().size(), kHistorySize);

  // Test the iterator for `Segments::conversion_segments()`.
  for (const Segment& segment : segments.conversion_segments()) {
    EXPECT_EQ(&segment, segment_list[i++]);
  }
  EXPECT_EQ(i, kHistorySize + kConversionSize);
  EXPECT_EQ(segments.conversion_segments_size(), kConversionSize);
  EXPECT_EQ(segments.conversion_segments().size(), kConversionSize);
}

TEST(SegmentTest, IteratorConstness) {
  // Create a test `Segments`.
  Segments segments;
  Segment* segment = segments.push_back_segment();
  segment->set_segment_type(Segment::FIXED_VALUE);

  // Dereferencing the iterator of non-const `Segments` should be non-const.
  static_assert(
      !std::is_const_v<std::remove_reference_t<decltype(*segments.begin())>>);

  // Dereferencing the iterator of const `Segments` should be const.
  const Segments& segments_const_ref = segments;
  auto it_const = segments_const_ref.begin();
  static_assert(std::is_const_v<std::remove_reference_t<decltype(*it_const)>>);
  auto& value_const_ref = *it_const;
  static_assert(
      std::is_const_v<std::remove_reference_t<decltype(value_const_ref)>>);

  // Check `conversion_segments()` is const too when it's from `const Segments`.
  auto it_const_conversion_segments =
      segments_const_ref.conversion_segments().begin();
  static_assert(
      std::is_const_v<
          std::remove_reference_t<decltype(*it_const_conversion_segments)>>);
  auto& value_const_conversion_segments = *it_const_conversion_segments;
  static_assert(
      std::is_const_v<
          std::remove_reference_t<decltype(value_const_conversion_segments)>>);

  // As per the STL requirements, `iterator` should be type convertible to
  // `const_iterator`.
  it_const = segments.begin();
  auto& value_const_ref2 = *it_const;
  static_assert(
      std::is_const_v<std::remove_reference_t<decltype(value_const_ref2)>>);
}

TEST(SegmentTest, IteratorRange) {
  // Create a test `Segments`.
  Segments segments;
  for (int i = 0; i < 5; ++i) {
    Segment* segment = segments.push_back_segment();
    segment->set_segment_type(Segment::FIXED_VALUE);
    segment->set_key(std::to_string(i));
  }

  EXPECT_THAT(ToKeys(segments), ElementsAre("0", "1", "2", "3", "4"));
  EXPECT_THAT(ToKeys(segments.all()), ElementsAre("0", "1", "2", "3", "4"));

  // Test simple cases.
  EXPECT_THAT(ToKeys(segments.all().drop(2)), ElementsAre("2", "3", "4"));
  EXPECT_THAT(ToKeys(segments.all().drop(4)), ElementsAre("4"));
  EXPECT_THAT(ToKeys(segments.all().take(2)), ElementsAre("0", "1"));
  EXPECT_THAT(ToKeys(segments.all().take(4)), ElementsAre("0", "1", "2", "3"));
  EXPECT_THAT(ToKeys(segments.all().subrange(2, 2)), ElementsAre("2", "3"));

  // Test when `count` is 0.
  EXPECT_THAT(ToKeys(segments.all().drop(0)),
              ElementsAre("0", "1", "2", "3", "4"));
  EXPECT_THAT(ToKeys(segments.all().take(0)), ElementsAre());
  EXPECT_THAT(ToKeys(segments.all().subrange(0, 0)), ElementsAre());
  EXPECT_THAT(ToKeys(segments.all().subrange(1, 0)), ElementsAre());

  // Test when `count` is equal to `size()`.
  EXPECT_THAT(ToKeys(segments.all().drop(5)), ElementsAre());
  EXPECT_THAT(ToKeys(segments.all().take(5)),
              ElementsAre("0", "1", "2", "3", "4"));
  EXPECT_THAT(ToKeys(segments.all().subrange(3, 2)), ElementsAre("3", "4"));

  // Test when `count` exceeds `size()`.
  EXPECT_THAT(ToKeys(segments.all().drop(6)), ElementsAre());
  EXPECT_THAT(ToKeys(segments.all().take(6)),
              ElementsAre("0", "1", "2", "3", "4"));
  EXPECT_THAT(ToKeys(segments.all().subrange(4, 2)), ElementsAre("4"));
  EXPECT_THAT(ToKeys(segments.all().subrange(5, 2)), ElementsAre());
  EXPECT_THAT(ToKeys(segments.all().subrange(6, 2)), ElementsAre());
}
}  // namespace converter
}  // namespace mozc
