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

#include "rewriter/collocation_rewriter.h"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <string>

#include "converter/segments.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/pos_matcher.h"
#include "request/conversion_request.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace {

using ::mozc::dictionary::PosMatcher;

class CollocationRewriterTest : public testing::TestWithTempUserProfile {
 protected:
  // Helper data structures to define test cases.
  // Used to generate Segment::Candidate.
  struct CandidateData {
    const char *key;
    const char *content_key;
    const char *value;
    const char *content_value;
    const int32_t cost;
    const uint16_t lid;
    const uint16_t rid;
  };

  // Used to generate Segment.
  struct SegmentData {
    const char *key;
    const CandidateData *candidates;
    const size_t candidates_size;
  };

  // Used to generate Segments.
  struct SegmentsData {
    const SegmentData *segments;
    const size_t segments_size;
  };

  CollocationRewriterTest()
      : pos_matcher_(data_manager_.GetPosMatcherData()),
        collocation_rewriter_(CollocationRewriter::Create(data_manager_)) {
  }

  // Makes a segment from SegmentData.
  static void MakeSegment(const SegmentData &data, Segment *segment) {
    segment->set_key(data.key);
    for (size_t i = 0; i < data.candidates_size; ++i) {
      Segment::Candidate *cand = segment->add_candidate();
      const CandidateData &cand_data = data.candidates[i];
      cand->key = cand_data.key;
      cand->content_key = cand_data.content_key;
      cand->value = cand_data.value;
      cand->content_value = cand_data.content_value;
      cand->cost = cand_data.cost;
      cand->lid = cand_data.lid;
      cand->rid = cand_data.rid;
    }
  }

  // Makes a segments from SegmentsData.
  static void MakeSegments(const SegmentsData &data, Segments *segments) {
    segments->Clear();
    for (size_t i = 0; i < data.segments_size; ++i) {
      MakeSegment(data.segments[i], segments->add_segment());
    }
  }

  bool Rewrite(Segments *segments) const {
    const ConversionRequest request;
    return collocation_rewriter_->Rewrite(request, segments);
  }

  // Returns the concatenated string of top candidates.
  static std::string GetTopValue(const Segments &segments) {
    std::string result;
    for (const Segment &segment : segments.conversion_segments()) {
      const Segment::Candidate &candidate = segment.candidate(0);
      result.append(candidate.value);
    }
    return result;
  }

  const mozc::testing::MockDataManager data_manager_;
  PosMatcher pos_matcher_;

 private:
  std::unique_ptr<CollocationRewriter> collocation_rewriter_;
};

TEST_F(CollocationRewriterTest, NekowoKaitai) {
  // Make the following Segments:
  // "ねこを" | "かいたい"
  // --------------------
  // "ネコを" | "買いたい"
  // "猫を"   | "解体"
  //         | "飼いたい"
  const char *kNekowo = "ねこを";
  const char *kNeko = "ねこ";
  const uint16_t id = pos_matcher_.GetUnknownId();
  const CandidateData kNekowoCands[] = {
      {kNekowo, kNeko, "ネコを", "ネコを", 0, id, id},
      {kNekowo, kNeko, "猫を", "猫を", 0, id, id},
  };
  const char *kKaitaiHiragana = "かいたい";
  const char *kBuy = "買いたい";
  const char *kCut = "解体";
  const char *kFeed = "飼いたい";
  const CandidateData kKaitaiCands[] = {
      {kKaitaiHiragana, kKaitaiHiragana, kBuy, kBuy, 0, id, id},
      {kKaitaiHiragana, kKaitaiHiragana, kCut, kCut, 0, id, id},
      {kKaitaiHiragana, kKaitaiHiragana, kFeed, kFeed, 0, id, id},
  };
  const SegmentData kSegmentData[] = {
      {kNekowo, kNekowoCands, std::size(kNekowoCands)},
      {kKaitaiHiragana, kKaitaiCands, std::size(kKaitaiCands)},
  };
  const SegmentsData kSegments = {kSegmentData, std::size(kSegmentData)};

  Segments segments;
  MakeSegments(kSegments, &segments);

  // "猫を飼いたい" should be promoted.
  EXPECT_TRUE(Rewrite(&segments));
  EXPECT_EQ(GetTopValue(segments), "猫を飼いたい") << segments.DebugString();
}

TEST_F(CollocationRewriterTest, MagurowoKaitai) {
  // Make the following Segments:
  // "まぐろを" | "かいたい"
  // --------------------
  // "マグロを" | "買いたい"
  // "鮪を"    | "解体"
  //          | "飼いたい"
  const char *kMagurowo = "まぐろを";
  const char *kMaguro = "まぐろ";
  const uint16_t id = pos_matcher_.GetUnknownId();
  const CandidateData kMagurowoCands[] = {
      {kMagurowo, kMaguro, "マグロを", "マグロ", 0, id, id},
      {kMagurowo, kMaguro, "鮪を", "鮪", 0, id, id},
  };
  const char *kKaitaiHiragana = "かいたい";
  const char *kBuy = "買いたい";
  const char *kCut = "解体";
  const char *kFeed = "飼いたい";
  const CandidateData kKaitaiCands[] = {
      {kKaitaiHiragana, kKaitaiHiragana, kBuy, kBuy, 0, id, id},
      {kKaitaiHiragana, kKaitaiHiragana, kCut, kCut, 0, id, id},
      {kKaitaiHiragana, kKaitaiHiragana, kFeed, kFeed, 0, id, id},
  };
  const SegmentData kSegmentData[] = {
      {kMagurowo, kMagurowoCands, std::size(kMagurowoCands)},
      {kKaitaiHiragana, kKaitaiCands, std::size(kKaitaiCands)},
  };
  const SegmentsData kSegments = {kSegmentData, std::size(kSegmentData)};

  Segments segments;
  MakeSegments(kSegments, &segments);

  // "マグロを解体" should be promoted.
  EXPECT_TRUE(Rewrite(&segments));
  EXPECT_EQ(GetTopValue(segments), "マグロを解体") << segments.DebugString();
}

TEST_F(CollocationRewriterTest, CrossOverAdverbSegment) {
  // "ねこを"    | "ネコを" "猫を"
  // "すごく"    | "すごく"
  // "かいたい"  | "買いたい" "解体" "飼いたい"
  const char *kNekowo = "ねこを";
  const char *kNeko = "ねこ";
  const uint16_t id = pos_matcher_.GetUnknownId();
  const CandidateData kNekowoCands[] = {
      {kNekowo, kNeko, "ネコを", "ネコを", 0, id, id},
      {kNekowo, kNeko, "猫を", "猫を", 0, id, id},
  };

  const char *kSugoku = "すごく";
  const uint16_t adverb_id = pos_matcher_.GetAdverbId();
  const CandidateData kSugokuCands[] = {
      {kSugoku, kSugoku, kSugoku, kSugoku, 0, adverb_id, adverb_id},
  };

  const char *kKaitaiHiragana = "かいたい";
  const char *kBuy = "買いたい";
  const char *kCut = "解体";
  const char *kFeed = "飼いたい";
  const CandidateData kKaitaiCands[] = {
      {kKaitaiHiragana, kKaitaiHiragana, kBuy, kBuy, 0, id, id},
      {kKaitaiHiragana, kKaitaiHiragana, kCut, kCut, 0, id, id},
      {kKaitaiHiragana, kKaitaiHiragana, kFeed, kFeed, 0, id, id},
  };
  const SegmentData kSegmentData[] = {
      {kNekowo, kNekowoCands, std::size(kNekowoCands)},
      {kSugoku, kSugokuCands, std::size(kSugokuCands)},
      {kKaitaiHiragana, kKaitaiCands, std::size(kKaitaiCands)},
  };
  const SegmentsData kSegments = {kSegmentData, std::size(kSegmentData)};

  Segments segments;
  MakeSegments(kSegments, &segments);

  // "猫を飼いたい" should be promoted.
  EXPECT_TRUE(Rewrite(&segments));
  EXPECT_EQ(GetTopValue(segments), "猫をすごく飼いたい")
      << segments.DebugString();
}

TEST_F(CollocationRewriterTest, DoNotCrossOverNonAdverbSegment) {
  // "ねこを"    | "ネコを" "猫を"
  // "すごく"    | "すごく"
  // "かいたい"  | "買いたい" "解体" "飼いたい"
  const char *kNekowo = "ねこを";
  const char *kNeko = "ねこ";
  const uint16_t id = pos_matcher_.GetUnknownId();
  const CandidateData kNekowoCands[] = {
      {kNekowo, kNeko, "ネコを", "ネコを", 0, id, id},
      {kNekowo, kNeko, "猫を", "猫を", 0, id, id},
  };

  const char *kSugoku = "すごく";
  const CandidateData kSugokuCands[] = {
      {kSugoku, kSugoku, kSugoku, kSugoku, 0, id, id},
  };

  const char *kKaitaiHiragana = "かいたい";
  const char *kBuy = "買いたい";
  const char *kCut = "解体";
  const char *kFeed = "飼いたい";
  const CandidateData kKaitaiCands[] = {
      {kKaitaiHiragana, kKaitaiHiragana, kBuy, kBuy, 0, id, id},
      {kKaitaiHiragana, kKaitaiHiragana, kCut, kCut, 0, id, id},
      {kKaitaiHiragana, kKaitaiHiragana, kFeed, kFeed, 0, id, id},
  };
  const SegmentData kSegmentData[] = {
      {kNekowo, kNekowoCands, std::size(kNekowoCands)},
      {kSugoku, kSugokuCands, std::size(kSugokuCands)},
      {kKaitaiHiragana, kKaitaiCands, std::size(kKaitaiCands)},
  };
  const SegmentsData kSegments = {kSegmentData, std::size(kSegmentData)};

  Segments segments;
  MakeSegments(kSegments, &segments);

  EXPECT_FALSE(Rewrite(&segments));
  EXPECT_NE(GetTopValue(segments), "猫をすごく飼いたい")
      << segments.DebugString();
}

TEST_F(CollocationRewriterTest, DoNotPromoteHighCostCandidate) {
  // Actually, we want to test for the collocation entries that are NOT in the
  // data sources, but will be judged as existing entry due to false positive.
  // However, we can not expect which entry will be false positive, so here
  // we use the existing entry for this test.

  // Make the following Segments:
  // "ねこを" | "かいたい"
  // --------------------
  // "ネコを" | "買いたい"
  // "猫を"   | "解体"
  //         | "飼いたい" (high cost)
  const char *kNekowo = "ねこを";
  const char *kNeko = "ねこ";
  const uint16_t id = pos_matcher_.GetUnknownId();
  const CandidateData kNekowoCands[] = {
      {kNekowo, kNeko, "ネコを", "ネコを", 0, id, id},
      {kNekowo, kNeko, "猫を", "猫を", 0, id, id},
  };
  const char *kKaitaiHiragana = "かいたい";
  const char *kBuy = "買いたい";
  const char *kCut = "解体";
  const char *kFeed = "飼いたい";
  const CandidateData kKaitaiCands[] = {
      {kKaitaiHiragana, kKaitaiHiragana, kBuy, kBuy, 0, id, id},
      {kKaitaiHiragana, kKaitaiHiragana, kCut, kCut, 0, id, id},
      {kKaitaiHiragana, kKaitaiHiragana, kFeed, kFeed, 10000, id, id},
  };
  const SegmentData kSegmentData[] = {
      {kNekowo, kNekowoCands, std::size(kNekowoCands)},
      {kKaitaiHiragana, kKaitaiCands, std::size(kKaitaiCands)},
  };
  const SegmentsData kSegments = {kSegmentData, std::size(kSegmentData)};

  Segments segments;
  MakeSegments(kSegments, &segments);

  // "猫を飼いたい" should NOT be promoted.
  EXPECT_FALSE(Rewrite(&segments));
  EXPECT_NE(GetTopValue(segments), "猫を飼いたい") << segments.DebugString();
}

TEST_F(CollocationRewriterTest, ImmuneToInvalidSegments) {
  const uint16_t kUnkId = pos_matcher_.GetUnknownId();
  const CandidateData kNekowoCands[] = {
      {"ねこを", "ねこ", "猫", "猫を", 0, kUnkId, kUnkId},
  };
  const CandidateData kKaitaiCands[] = {
      {"かいたい", "かいたい", "飼いたい", "飼いたい", 0, kUnkId, kUnkId},
  };
  const SegmentData kSegmentData[] = {
      {"ねこを", kNekowoCands, std::size(kNekowoCands)},
      {"かいたい", kKaitaiCands, std::size(kKaitaiCands)},
  };
  const SegmentsData kSegments = {kSegmentData, std::size(kSegmentData)};

  {
    Segments segments;
    MakeSegments(kSegments, &segments);
    // If there's a fixed segment, rewrite fails.
    segments.mutable_segment(0)->set_segment_type(Segment::FIXED_VALUE);
    EXPECT_FALSE(Rewrite(&segments));
  }
  {
    Segments segments;
    MakeSegments(kSegments, &segments);
    // If there's a segment with no candidates, rewrite fails.
    segments.mutable_segment(0)->clear_candidates();
    EXPECT_FALSE(Rewrite(&segments));
  }
}

}  // namespace
}  // namespace mozc
