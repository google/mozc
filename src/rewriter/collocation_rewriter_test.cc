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
#include <memory>
#include <string>

#include "base/logging.h"
#include "base/system_util.h"
#include "config/config_handler.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/pos_matcher.h"
#include "request/conversion_request.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "absl/flags/flag.h"
#include "absl/memory/memory.h"

namespace mozc {
namespace {

using dictionary::POSMatcher;

class CollocationRewriterTest : public ::testing::Test {
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

  CollocationRewriterTest() = default;
  ~CollocationRewriterTest() override = default;

  void SetUp() override {
    SystemUtil::SetUserProfileDirectory(absl::GetFlag(FLAGS_test_tmpdir));

    const mozc::testing::MockDataManager data_manager;
    pos_matcher_.Set(data_manager.GetPOSMatcherData());
    collocation_rewriter_ =
        absl::make_unique<CollocationRewriter>(&data_manager);
  }

  // Makes a segment from SegmentData.
  static void MakeSegment(const SegmentData &data, Segment *segment) {
    segment->Clear();
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
    for (size_t i = 0; i < segments.conversion_segments_size(); ++i) {
      const Segment::Candidate &candidate =
          segments.conversion_segment(i).candidate(0);
      result.append(candidate.value);
    }
    return result;
  }

  POSMatcher pos_matcher_;

 private:
  std::unique_ptr<const CollocationRewriter> collocation_rewriter_;
  DISALLOW_COPY_AND_ASSIGN(CollocationRewriterTest);
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
      {kNekowo, kNekowoCands, arraysize(kNekowoCands)},
      {kKaitaiHiragana, kKaitaiCands, arraysize(kKaitaiCands)},
  };
  const SegmentsData kSegments = {kSegmentData, arraysize(kSegmentData)};

  Segments segments;
  MakeSegments(kSegments, &segments);

  // "猫を飼いたい" should be promoted.
  EXPECT_TRUE(Rewrite(&segments));
  EXPECT_EQ("猫を飼いたい", GetTopValue(segments)) << segments.DebugString();
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
      {kMagurowo, kMagurowoCands, arraysize(kMagurowoCands)},
      {kKaitaiHiragana, kKaitaiCands, arraysize(kKaitaiCands)},
  };
  const SegmentsData kSegments = {kSegmentData, arraysize(kSegmentData)};

  Segments segments;
  MakeSegments(kSegments, &segments);

  // "マグロを解体" should be promoted.
  EXPECT_TRUE(Rewrite(&segments));
  EXPECT_EQ("マグロを解体", GetTopValue(segments)) << segments.DebugString();
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
      {kNekowo, kNekowoCands, arraysize(kNekowoCands)},
      {kSugoku, kSugokuCands, arraysize(kSugokuCands)},
      {kKaitaiHiragana, kKaitaiCands, arraysize(kKaitaiCands)},
  };
  const SegmentsData kSegments = {kSegmentData, arraysize(kSegmentData)};

  Segments segments;
  MakeSegments(kSegments, &segments);

  // "猫を飼いたい" should be promoted.
  EXPECT_TRUE(Rewrite(&segments));
  EXPECT_EQ("猫をすごく飼いたい", GetTopValue(segments))
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
      {kNekowo, kNekowoCands, arraysize(kNekowoCands)},
      {kSugoku, kSugokuCands, arraysize(kSugokuCands)},
      {kKaitaiHiragana, kKaitaiCands, arraysize(kKaitaiCands)},
  };
  const SegmentsData kSegments = {kSegmentData, arraysize(kSegmentData)};

  Segments segments;
  MakeSegments(kSegments, &segments);

  EXPECT_FALSE(Rewrite(&segments));
  EXPECT_NE("猫をすごく飼いたい", GetTopValue(segments))
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
      {kNekowo, kNekowoCands, arraysize(kNekowoCands)},
      {kKaitaiHiragana, kKaitaiCands, arraysize(kKaitaiCands)},
  };
  const SegmentsData kSegments = {kSegmentData, arraysize(kSegmentData)};

  Segments segments;
  MakeSegments(kSegments, &segments);

  // "猫を飼いたい" should NOT be promoted.
  EXPECT_FALSE(Rewrite(&segments));
  EXPECT_NE("猫を飼いたい", GetTopValue(segments)) << segments.DebugString();
}

}  // namespace
}  // namespace mozc
