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

#include "converter/converter_util.h"

#include <optional>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "converter/candidate.h"
#include "converter/inner_segment.h"
#include "converter/segments.h"
#include "prediction/result.h"
#include "request/conversion_request.h"
#include "testing/gunit.h"

namespace mozc::converter {
namespace {

TEST(ConverterUtilTest, ConversionSegmentsToResultEmpty) {
  Segments segments;
  EXPECT_FALSE(
      ConversionSegmentsToResult(segments.conversion_segments()).has_value());
}

TEST(ConverterUtilTest, ConversionSegmentsToResultMultiSegment) {
  Segments segments;
  for (int i = 0; i < 3; ++i) {
    Segment* segment = segments.add_segment();
    Candidate* c = segment->add_candidate();
    c->key = absl::StrCat("key", i, "_part");
    c->content_key = absl::StrCat("key", i);
    c->value = absl::StrCat("val", i, "_suff");
    c->content_value = absl::StrCat("val", i);
    c->lid = i + 1;
    c->rid = i + 2;
    c->cost = 10 * (i + 1);
    c->wcost = 5 * (i + 1);
    c->attributes = (1 << i);
  }

  std::optional<prediction::Result> result =
      ConversionSegmentsToResult(segments.conversion_segments());
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->key, "key0_partkey1_partkey2_part");
  EXPECT_EQ(result->value, "val0_suffval1_suffval2_suff");
  EXPECT_EQ(result->lid, 1);
  EXPECT_EQ(result->rid, 4);
  EXPECT_EQ(result->cost, 10 + 20 + 30);
  EXPECT_EQ(result->wcost, 5 + 10 + 15);
  EXPECT_EQ(result->attributes, 1 | 2 | 4);

  // Verify boundary information.
  ASSERT_EQ(result->inner_segments().size(), 3);
  int i = 0;
  for (const auto& inner_segment : result->inner_segments()) {
    EXPECT_EQ(inner_segment.GetKey(), absl::StrCat("key", i, "_part"));
    EXPECT_EQ(inner_segment.GetContentKey(), absl::StrCat("key", i));
    EXPECT_EQ(inner_segment.GetValue(), absl::StrCat("val", i, "_suff"));
    EXPECT_EQ(inner_segment.GetContentValue(), absl::StrCat("val", i));
    ++i;
  }
  EXPECT_EQ(i, 3);
}

TEST(ConverterUtilTest, HistorySegmentsToResult) {
  Segments segments;
  for (int i = 0; i < 3; ++i) {
    Segment* segment = segments.add_segment();
    segment->set_segment_type(Segment::HISTORY);
    Candidate* c = segment->add_candidate();
    c->key = absl::StrCat("key", i, "_part");
    c->content_key = absl::StrCat("key", i);
    c->value = absl::StrCat("val", i, "_suff");
    c->content_value = absl::StrCat("val", i);
    c->lid = i;
    c->rid = i + 1;
    c->cost = i;
  }

  prediction::Result result =
      HistorySegmentsToResult(segments.history_segments());
  EXPECT_EQ(result.key, "key0_partkey1_partkey2_part");
  EXPECT_EQ(result.value, "val0_suffval1_suffval2_suff");
  EXPECT_EQ(result.lid, 0);
  EXPECT_EQ(result.rid, 3);
  EXPECT_EQ(result.cost, 2);  // only the last cost

  // Verify boundary information.
  ASSERT_EQ(result.inner_segments().size(), 3);
  int i = 0;
  for (const auto& inner_segment : result.inner_segments()) {
    EXPECT_EQ(inner_segment.GetKey(), absl::StrCat("key", i, "_part"));
    EXPECT_EQ(inner_segment.GetContentKey(), absl::StrCat("key", i));
    EXPECT_EQ(inner_segment.GetValue(), absl::StrCat("val", i, "_suff"));
    EXPECT_EQ(inner_segment.GetContentValue(), absl::StrCat("val", i));
    ++i;
  }
  EXPECT_EQ(i, 3);
}

TEST(ConverterUtilTest, CandidateAndResultRoundTripWithEmptyBoundary) {
  Candidate candidate;
  candidate.key = "とうきょうに";
  candidate.value = "東京に";
  candidate.content_key = "とうきょう";
  candidate.content_value = "東京";
  candidate.lid = 100;
  candidate.rid = 200;
  candidate.cost = 500;
  candidate.wcost = 300;
  candidate.attributes = 4;
  candidate.consumed_key_size = 5;

  // CandidateToResult synthesizes single-segment boundary from
  // content_key/value.
  prediction::Result result = CandidateToResult(candidate);
  EXPECT_EQ(result.key, "とうきょうに");
  EXPECT_EQ(result.value, "東京に");
  EXPECT_EQ(result.lid, 100);
  EXPECT_EQ(result.rid, 200);
  EXPECT_EQ(result.cost, 500);
  EXPECT_EQ(result.wcost, 300);
  EXPECT_EQ(result.attributes, 4);
  EXPECT_EQ(result.consumed_key_size, 5);

  ASSERT_EQ(result.inner_segments().size(), 1);
  const auto inner = *result.inner_segments().begin();
  EXPECT_EQ(inner.GetKey(), "とうきょうに");
  EXPECT_EQ(inner.GetValue(), "東京に");
  EXPECT_EQ(inner.GetContentKey(), "とうきょう");
  EXPECT_EQ(inner.GetContentValue(), "東京");

  Candidate new_cand;
  PopulateCandidateFromResult(result, &new_cand);
  EXPECT_EQ(new_cand.key, candidate.key);
  EXPECT_EQ(new_cand.value, candidate.value);
  EXPECT_EQ(new_cand.content_key, candidate.content_key);
  EXPECT_EQ(new_cand.content_value, candidate.content_value);
  EXPECT_EQ(new_cand.lid, candidate.lid);
  EXPECT_EQ(new_cand.rid, candidate.rid);
  EXPECT_EQ(new_cand.cost, candidate.cost);
  EXPECT_EQ(new_cand.wcost, candidate.wcost);
  EXPECT_EQ(new_cand.attributes, candidate.attributes);
  EXPECT_EQ(new_cand.consumed_key_size, candidate.consumed_key_size);
  EXPECT_EQ(new_cand.inner_segment_boundary, result.inner_segment_boundary);
}

TEST(ConverterUtilTest, CandidateAndResultRoundTripWithMultiSegmentBoundary) {
  Candidate candidate;
  candidate.key = "とうきょうにいきました";
  candidate.value = "東京に行きました";
  // "とうきょう" (15 bytes), "東京" (6 bytes), "とうきょう" (15 bytes), "東京"
  // (6 bytes) "に" (3 bytes), "に" (3 bytes), "" (0 bytes), "" (0 bytes)
  // "いきました" (15 bytes), "行きました" (15 bytes), "いき" (6 bytes), "行" (3
  // bytes)
  candidate.inner_segment_boundary = BuildInnerSegmentBoundary(
      {
          {/*key*/ 15, /*val*/ 6, /*content_key*/ 15, /*content_val*/ 6},
          {/*key*/ 3, /*val*/ 3, /*content_key*/ 0, /*content_val*/ 0},
          {/*key*/ 15, /*val*/ 15, /*content_key*/ 6, /*content_val*/ 3},
      },
      candidate.key, candidate.value);
  ASSERT_FALSE(candidate.inner_segment_boundary.empty());
  candidate.lid = 10;
  candidate.rid = 20;
  candidate.cost = 1000;
  candidate.wcost = 600;

  prediction::Result result = CandidateToResult(candidate);
  EXPECT_EQ(result.inner_segment_boundary, candidate.inner_segment_boundary);
  ASSERT_EQ(result.inner_segments().size(), 3);

  Candidate new_cand;
  PopulateCandidateFromResult(result, &new_cand);
  EXPECT_EQ(new_cand.inner_segment_boundary, candidate.inner_segment_boundary);
  EXPECT_EQ(new_cand.key, candidate.key);
  EXPECT_EQ(new_cand.value, candidate.value);
  EXPECT_EQ(new_cand.lid, candidate.lid);
  EXPECT_EQ(new_cand.rid, candidate.rid);
  EXPECT_EQ(new_cand.cost, candidate.cost);
  EXPECT_EQ(new_cand.wcost, candidate.wcost);
}

TEST(ConverterUtilTest, PrepareSegmentsFromRequest) {
  Segments history_segs;
  // History segment 0: "きょうは" -> "今日は" (content: "きょう" -> "今日")
  {
    Segment* h_seg = history_segs.add_segment();
    h_seg->set_segment_type(Segment::HISTORY);
    Candidate* h_cand = h_seg->add_candidate();
    h_cand->key = "きょうは";
    h_cand->value = "今日は";
    h_cand->content_key = "きょう";
    h_cand->content_value = "今日";
    h_cand->lid = 10;
    h_cand->rid = 20;
    h_cand->cost = 50;
  }
  // History segment 1: "とうきょうへ" -> "東京へ" (content: "とうきょう" ->
  // "東京")
  {
    Segment* h_seg = history_segs.add_segment();
    h_seg->set_segment_type(Segment::HISTORY);
    Candidate* h_cand = h_seg->add_candidate();
    h_cand->key = "とうきょうへ";
    h_cand->value = "東京へ";
    h_cand->content_key = "とうきょう";
    h_cand->content_value = "東京";
    h_cand->lid = 11;
    h_cand->rid = 21;
    h_cand->cost = 100;
  }

  const prediction::Result history_result =
      HistorySegmentsToResult(history_segs.history_segments());

  ConversionRequest request = ConversionRequestBuilder()
                                  .SetHistoryResultView(history_result)
                                  .SetKey("はれ")
                                  .Build();

  Segments reconstructed = PrepareSegmentsFromRequest(request);
  ASSERT_EQ(reconstructed.history_segments_size(), 2);
  EXPECT_EQ(reconstructed.history_segment(0).key(), "きょうは");
  EXPECT_EQ(reconstructed.history_segment(0).candidate(0).value, "今日は");
  EXPECT_EQ(reconstructed.history_segment(0).candidate(0).content_key,
            "きょう");
  EXPECT_EQ(reconstructed.history_segment(0).candidate(0).content_value,
            "今日");

  EXPECT_EQ(reconstructed.history_segment(1).key(), "とうきょうへ");
  EXPECT_EQ(reconstructed.history_segment(1).candidate(0).value, "東京へ");
  EXPECT_EQ(reconstructed.history_segment(1).candidate(0).content_key,
            "とうきょう");
  EXPECT_EQ(reconstructed.history_segment(1).candidate(0).content_value,
            "東京");

  // The last history segment candidate should have the history cost and rid.
  EXPECT_EQ(reconstructed.history_segment(1).candidate(0).rid, 21);
  EXPECT_EQ(reconstructed.history_segment(1).candidate(0).cost, 100);

  ASSERT_EQ(reconstructed.conversion_segments_size(), 1);
  EXPECT_EQ(reconstructed.conversion_segment(0).key(), "はれ");
}

TEST(ConverterUtilTest, MakeLearningResultsFromSegments) {
  // Empty segments.
  {
    const Segments segments;
    EXPECT_TRUE(MakeLearningResultsFromSegments(segments).empty());
  }

  // Single segment and multiple candidates.
  {
    Segments segments;
    Segment* segment = segments.add_segment();
    for (int i = 0; i < 10; ++i) {
      Candidate* c = segment->add_candidate();
      c->key = absl::StrCat("k", i);
      c->content_key = "k";
      c->value = absl::StrCat("v", i);
      c->content_value = "v";
      c->description = "description";
      c->display_value = "display_value";
      c->lid = i;
      c->rid = i + 1;
      c->cost = i + 2;
      c->wcost = 10 * i;
    }

    const std::vector<prediction::Result> results =
        MakeLearningResultsFromSegments(segments);
    EXPECT_EQ(results.size(), 5);
    for (int i = 0; i < results.size(); ++i) {
      const Candidate& c = segment->candidate(i);
      const prediction::Result& result = results[i];
      EXPECT_EQ(c.key, result.key);
      EXPECT_EQ(c.value, result.value);
      EXPECT_EQ(c.description, result.description);
      EXPECT_EQ(c.display_value, result.display_value);
      EXPECT_EQ(c.lid, result.lid);
      EXPECT_EQ(c.rid, result.rid);
      EXPECT_EQ(c.cost, result.cost);
      EXPECT_EQ(c.wcost, result.wcost);

      ASSERT_EQ(result.inner_segments().size(), 1);
      for (const auto& iter : result.inner_segments()) {
        EXPECT_EQ(iter.GetKey(), c.key);
        EXPECT_EQ(iter.GetContentKey(), c.content_key);
        EXPECT_EQ(iter.GetValue(), c.value);
        EXPECT_EQ(iter.GetContentValue(), c.content_value);
      }
    }
  }

  // Multiple segments.
  {
    Segments segments;
    for (int i = 0; i < 3; ++i) {
      Segment* segment = segments.add_segment();
      Candidate* c = segment->add_candidate();
      c->key = absl::StrCat("k", i);
      c->content_key = "k";
      c->value = absl::StrCat("v", i);
      c->content_value = "v";
      c->lid = i;
      c->rid = i + 1;
      c->cost = i;
      c->wcost = 10 * i;
    }

    const std::vector<prediction::Result> results =
        MakeLearningResultsFromSegments(segments);
    EXPECT_EQ(results.size(), 1);

    const prediction::Result& result = results.front();
    EXPECT_EQ(result.key, "k0k1k2");
    EXPECT_EQ(result.value, "v0v1v2");
    EXPECT_EQ(result.lid, segments.segment(0).candidate(0).lid);
    EXPECT_EQ(result.rid, segments.segment(2).candidate(0).rid);
    EXPECT_EQ(result.cost, 0 + 1 + 2);
    EXPECT_EQ(result.wcost, 0 + 10 + 20);

    // Verify boundary iteration across multiple segments.
    ASSERT_EQ(result.inner_segments().size(), 3);
    int n = 0;
    for (const auto& iter : result.inner_segments()) {
      const Candidate& c = segments.segment(n).candidate(0);
      EXPECT_EQ(iter.GetKey(), c.key);
      EXPECT_EQ(iter.GetContentKey(), c.content_key);
      EXPECT_EQ(iter.GetValue(), c.value);
      EXPECT_EQ(iter.GetContentValue(), c.content_value);
      ++n;
    }
    EXPECT_EQ(n, 3);
  }
}

}  // namespace
}  // namespace mozc::converter
