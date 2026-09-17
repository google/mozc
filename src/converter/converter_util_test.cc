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
#include "converter/attribute.h"
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

TEST(ConverterUtilTest, ConversionSegmentsToResults) {
  // Empty segments.
  {
    const Segments segments;
    EXPECT_TRUE(ConversionSegmentsToResults(segments).empty());
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
        ConversionSegmentsToResults(segments);
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

    // Custom max_results parameter.
    const std::vector<prediction::Result> results_custom =
        ConversionSegmentsToResults(segments, /*max_results=*/8);
    EXPECT_EQ(results_custom.size(), 8);
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
        ConversionSegmentsToResults(segments);
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

TEST(ConverterUtilTest, ApplyResultToSegmentsMultiSegment) {
  // Conversion segments: "ここで"(9B) | "はきものを"(15B) | "ぬぐ"(6B)
  Segments segments;
  Segment* s0 = segments.add_segment();
  s0->set_key("ここで");
  Candidate* c0 = s0->add_candidate();
  c0->key = "ここで";
  c0->value = "ここで";
  c0->content_key = "ここで";
  c0->content_value = "ここで";

  Segment* s1 = segments.add_segment();
  s1->set_key("はきものを");
  Candidate* c1 = s1->add_candidate();
  c1->key = "はきものを";
  c1->value = "履物を";
  c1->content_key = "はきもの";
  c1->content_value = "履物";

  Segment* s2 = segments.add_segment();
  s2->set_key("ぬぐ");
  Candidate* c2_other = s2->add_candidate();
  c2_other->key = "ぬぐ";
  c2_other->value = "塗ぐ";
  c2_other->content_key = "ぬぐ";
  c2_other->content_value = "塗ぐ";
  Candidate* c2 = s2->add_candidate();
  c2->key = "ぬぐ";
  c2->value = "脱ぐ";
  c2->content_key = "ぬぐ";
  c2->content_value = "脱ぐ";

  // Result 0: "ここでは"(12B) | "着物を"(9B, key: "きものを" 12B) | "脱ぐ"(6B,
  // key: "ぬぐ" 6B)
  prediction::Result pred_result0;
  pred_result0.key = "ここではきものをぬぐ";
  pred_result0.value = "ここでは着物を脱ぐ";
  pred_result0.inner_segment_boundary =
      BuildInnerSegmentBoundary({{12, 12, 9, 9}, {12, 9, 9, 6}, {6, 6, 6, 6}},
                                pred_result0.key, pred_result0.value);

  // Result 1: "此処では"(12B) | "着物を"(9B, key: "きものを" 12B) | "脱ぐ"(6B,
  // key: "ぬぐ" 6B)
  prediction::Result pred_result1;
  pred_result1.key = "ここではきものをぬぐ";
  pred_result1.value = "此処では着物を脱ぐ";
  pred_result1.inner_segment_boundary =
      BuildInnerSegmentBoundary({{12, 12, 9, 9}, {12, 9, 9, 6}, {6, 6, 6, 6}},
                                pred_result1.key, pred_result1.value);

  // Apply result 0 at target_pos = 0.
  ApplyResultToSegmentsMultiSegment(pred_result0, /*target_pos=*/0, segments);

  // Segments structure is preserved.
  ASSERT_EQ(segments.conversion_segments_size(), 3);
  EXPECT_EQ(segments.conversion_segment(0).key(), "ここで");
  EXPECT_EQ(segments.conversion_segment(1).key(), "はきものを");
  EXPECT_EQ(segments.conversion_segment(2).key(), "ぬぐ");

  // Segment 0 contains multi-segment candidate "ここでは着物を" with
  // converted_segment_count = 2 at pos 0.
  const Segment& seg0 = segments.conversion_segment(0);
  ASSERT_GT(seg0.candidates_size(), 0);
  EXPECT_EQ(seg0.candidate(0).value, "ここでは着物を");
  EXPECT_EQ(seg0.candidate(0).key, "ここではきものを");
  EXPECT_EQ(seg0.candidate(0).content_value, "ここでは着物");
  EXPECT_EQ(seg0.candidate(0).content_key, "ここではきもの");
  EXPECT_EQ(seg0.candidate(0).converted_segment_count, 2);
  EXPECT_EQ(seg0.candidate(0).inner_segment_boundary.size(), 2);

  // Segment 2 top candidate is moved to "脱ぐ".
  const Segment& seg2 = segments.conversion_segment(2);
  ASSERT_GE(seg2.candidates_size(), 2);
  EXPECT_EQ(seg2.candidate(0).value, "脱ぐ");
  EXPECT_EQ(seg2.candidate(0).inner_segment_boundary.size(), 1);
  EXPECT_EQ(seg2.candidate(1).value, "塗ぐ");

  // Apply result 1 at target_pos = 1.
  ApplyResultToSegmentsMultiSegment(pred_result1, /*target_pos=*/1, segments);

  // Segment 0 now contains "ここでは着物を" at pos 0 and "此処では着物を" at
  // pos 1.
  ASSERT_GE(seg0.candidates_size(), 2);
  EXPECT_EQ(seg0.candidate(0).value, "ここでは着物を");
  EXPECT_EQ(seg0.candidate(0).converted_segment_count, 2);
  EXPECT_EQ(seg0.candidate(1).value, "此処では着物を");
  EXPECT_EQ(seg0.candidate(1).converted_segment_count, 2);

  // Segment 2 top candidate "脱ぐ" was NOT demoted to pos 1.
  EXPECT_EQ(seg2.candidate(0).value, "脱ぐ");
  EXPECT_EQ(seg2.candidate(1).value, "塗ぐ");
}

TEST(ConverterUtilTest,
     ApplyResultToSegmentsMultiSegmentEmptyInnerSegmentBoundary) {
  {
    // Multi-segment fallback when inner_segment_boundary is empty.
    Segments segments;
    Segment* s0 = segments.add_segment();
    s0->set_key("わたしの");
    Candidate* c0 = s0->add_candidate();
    c0->key = "わたしの";
    c0->value = "私の";
    c0->content_key = "わたしの";
    c0->content_value = "私の";

    Segment* s1 = segments.add_segment();
    s1->set_key("なまえ");
    Candidate* c1 = s1->add_candidate();
    c1->key = "なまえ";
    c1->value = "名前";
    c1->content_key = "なまえ";
    c1->content_value = "名前";

    prediction::Result pred_result;
    pred_result.key = "わたしのなまえ";
    pred_result.value = "僕の名字";
    pred_result.attributes =
        Attribute::USER_HISTORY_PREDICTION |
        Attribute::USER_HISTORY_EMPTY_INNER_SEGMENT_BOUNDARY;

    ApplyResultToSegmentsMultiSegment(pred_result, /*target_pos=*/0, segments);

    ASSERT_EQ(segments.conversion_segments_size(), 2);
    const Segment& seg0 = segments.conversion_segment(0);
    ASSERT_GT(seg0.candidates_size(), 0);
    EXPECT_EQ(seg0.candidate(0).value, "僕の名字");
    EXPECT_EQ(seg0.candidate(0).key, "わたしのなまえ");
    EXPECT_EQ(seg0.candidate(0).converted_segment_count, 2);
    EXPECT_TRUE(seg0.candidate(0).attributes &
                Attribute::USER_HISTORY_PREDICTION);
  }

  {
    // Single-segment fallback when inner_segment_boundary is empty.
    Segments segments;
    Segment* s0 = segments.add_segment();
    s0->set_key("あめ");
    Candidate* c0 = s0->add_candidate();
    c0->key = "あめ";
    c0->value = "飴";
    c0->content_key = "あめ";
    c0->content_value = "飴";

    prediction::Result pred_result;
    pred_result.key = "あめ";
    pred_result.value = "雨";

    ApplyResultToSegmentsMultiSegment(pred_result, /*target_pos=*/0, segments);

    ASSERT_EQ(segments.conversion_segments_size(), 1);
    const Segment& seg0 = segments.conversion_segment(0);
    ASSERT_GT(seg0.candidates_size(), 0);
    EXPECT_EQ(seg0.candidate(0).value, "雨");
    EXPECT_EQ(seg0.candidate(0).key, "あめ");
    EXPECT_EQ(seg0.candidate(0).converted_segment_count, 1);
    EXPECT_TRUE(seg0.candidate(0).inner_segment_boundary.empty());
  }
}

TEST(ConverterUtilTest, ApplyResultToSegmentsMultiSegmentKeyLengthMismatch) {
  {
    // Conversion segments key is shorter than prediction result key.
    Segments segments;
    Segment* s0 = segments.add_segment();
    s0->set_key("ここで");
    Candidate* c0 = s0->add_candidate();
    c0->key = "ここで";
    c0->value = "ここで";

    prediction::Result pred_result;
    pred_result.key = "ここではきものをぬぐ";
    pred_result.value = "ここでは着物を脱ぐ";
    pred_result.inner_segment_boundary =
        BuildInnerSegmentBoundary({{12, 12, 9, 9}, {12, 9, 9, 6}, {6, 6, 6, 6}},
                                  pred_result.key, pred_result.value);

    ApplyResultToSegmentsMultiSegment(pred_result, /*target_pos=*/0, segments);

    ASSERT_EQ(segments.conversion_segments_size(), 1);
    const Segment& seg0 = segments.conversion_segment(0);
    EXPECT_EQ(seg0.candidates_size(), 1);
    EXPECT_EQ(seg0.candidate(0).value, "ここで");
  }

  {
    // Prediction result key is shorter than conversion segments key.
    Segments segments;
    Segment* s0 = segments.add_segment();
    s0->set_key("ここで");
    Candidate* c0 = s0->add_candidate();
    c0->key = "ここで";
    c0->value = "ここで";

    Segment* s1 = segments.add_segment();
    s1->set_key("はきものを");
    Candidate* c1 = s1->add_candidate();
    c1->key = "はきものを";
    c1->value = "履物を";

    prediction::Result pred_result;
    pred_result.key = "ここで";
    pred_result.value = "此処で";
    pred_result.inner_segment_boundary = BuildInnerSegmentBoundary(
        {{9, 9, 9, 9}}, pred_result.key, pred_result.value);

    ApplyResultToSegmentsMultiSegment(pred_result, /*target_pos=*/0, segments);

    ASSERT_EQ(segments.conversion_segments_size(), 2);
    // Segment 0 matches and gets updated/promoted.
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, "此処で");
    // Segment 1 is untouched.
    EXPECT_EQ(segments.conversion_segment(1).candidate(0).value, "履物を");
  }
}

TEST(ConverterUtilTest,
     ApplyResultToSegmentsMultiSegmentSingleVsMultiSegmentCandidateMatching) {
  Segments segments;
  Segment* s0 = segments.add_segment();
  s0->set_key("とう");
  Candidate* c0_single = s0->add_candidate();
  c0_single->key = "とう";
  c0_single->value = "東京";
  c0_single->converted_segment_count = 1;

  Candidate* c0_other = s0->add_candidate();
  c0_other->key = "とう";
  c0_other->value = "党";
  c0_other->converted_segment_count = 1;

  Segment* s1 = segments.add_segment();
  s1->set_key("きょう");
  Candidate* c1 = s1->add_candidate();
  c1->key = "きょう";
  c1->value = "京";
  c1->converted_segment_count = 1;

  prediction::Result pred_result_multi;
  pred_result_multi.key = "とうきょう";
  pred_result_multi.value = "東京";
  pred_result_multi.inner_segment_boundary =
      BuildInnerSegmentBoundary({{9, 3, 9, 3}, {6, 3, 6, 3}},
                                pred_result_multi.key, pred_result_multi.value);

  // 1. Apply multi-segment result (num_segs = 2).
  // Existing single-segment candidate "東京" (converted_segment_count = 1)
  // must NOT match. A new multi-segment candidate (converted_segment_count = 2)
  // is inserted at pos 0.
  ApplyResultToSegmentsMultiSegment(pred_result_multi, /*target_pos=*/0,
                                    segments);

  Segment* seg0 = segments.mutable_conversion_segment(0);
  ASSERT_EQ(seg0->candidates_size(), 3);
  EXPECT_EQ(seg0->candidate(0).value, "東京");
  EXPECT_EQ(seg0->candidate(0).converted_segment_count, 2);
  EXPECT_EQ(seg0->candidate(1).value, "東京");
  EXPECT_EQ(seg0->candidate(1).converted_segment_count, 1);
  EXPECT_EQ(seg0->candidate(2).value, "党");

  // 2. Apply multi-segment result again at target_pos = 0.
  // This time it matches existing candidate 0 (converted_segment_count = 2),
  // updating in place without inserting a new candidate.
  ApplyResultToSegmentsMultiSegment(pred_result_multi, /*target_pos=*/0,
                                    segments);
  ASSERT_EQ(seg0->candidates_size(), 3);
  EXPECT_EQ(seg0->candidate(0).value, "東京");
  EXPECT_EQ(seg0->candidate(0).converted_segment_count, 2);

  // 3. Apply single-segment result for "とう" -> "党".
  // Matches existing single-segment candidate "党" and moves it to pos 0.
  prediction::Result pred_result_single;
  pred_result_single.key = "とう";
  pred_result_single.value = "党";
  pred_result_single.inner_segment_boundary = BuildInnerSegmentBoundary(
      {{6, 6, 3, 3}}, pred_result_single.key, pred_result_single.value);

  Segments single_seg_segments;
  Segment* ss0 = single_seg_segments.add_segment();
  ss0->set_key("とう");
  Candidate* sc0 = ss0->add_candidate();
  sc0->key = "とう";
  sc0->value = "東京";
  sc0->converted_segment_count = 2;  // multi-segment candidate
  Candidate* sc1 = ss0->add_candidate();
  sc1->key = "とう";
  sc1->value = "党";
  sc1->converted_segment_count = 1;

  ApplyResultToSegmentsMultiSegment(pred_result_single, /*target_pos=*/0,
                                    single_seg_segments);

  const Segment& ss_seg0 = single_seg_segments.conversion_segment(0);
  ASSERT_EQ(ss_seg0.candidates_size(), 2);
  EXPECT_EQ(ss_seg0.candidate(0).value, "党");
  EXPECT_EQ(ss_seg0.candidate(0).converted_segment_count, 1);
  EXPECT_EQ(ss_seg0.candidate(1).value, "東京");
  EXPECT_EQ(ss_seg0.candidate(1).converted_segment_count, 2);
}

TEST(ConverterUtilTest,
     ApplyResultToSegmentsMultiSegmentBoundaryAndPositionVariations) {
  {
    // Single-segment new candidate insertion with non-empty boundary on empty
    // candidate list (push_front_candidate path).
    Segments segments;
    Segment* s0 = segments.add_segment();
    s0->set_key("あめ");

    prediction::Result pred_result;
    pred_result.key = "あめ";
    pred_result.value = "雨";
    pred_result.inner_segment_boundary = BuildInnerSegmentBoundary(
        {{6, 3, 6, 3}}, pred_result.key, pred_result.value);

    ApplyResultToSegmentsMultiSegment(pred_result, /*target_pos=*/0, segments);

    const Segment& seg0 = segments.conversion_segment(0);
    ASSERT_EQ(seg0.candidates_size(), 1);
    EXPECT_EQ(seg0.candidate(0).value, "雨");
    EXPECT_EQ(seg0.candidate(0).converted_segment_count, 1);
    EXPECT_FALSE(seg0.candidate(0).inner_segment_boundary.empty());
  }

  {
    // Single-segment existing candidate update with non-empty boundary.
    Segments segments;
    Segment* s0 = segments.add_segment();
    s0->set_key("あめ");
    Candidate* c0 = s0->add_candidate();
    c0->key = "あめ";
    c0->value = "雨";
    c0->converted_segment_count = 1;

    prediction::Result pred_result;
    pred_result.key = "あめ";
    pred_result.value = "雨";
    pred_result.inner_segment_boundary = BuildInnerSegmentBoundary(
        {{6, 3, 6, 3}}, pred_result.key, pred_result.value);

    ApplyResultToSegmentsMultiSegment(pred_result, /*target_pos=*/0, segments);

    const Segment& seg0 = segments.conversion_segment(0);
    ASSERT_EQ(seg0.candidates_size(), 1);
    EXPECT_FALSE(seg0.candidate(0).inner_segment_boundary.empty());
  }

  {
    // Multi-segment existing candidate update sets sliced
    // inner_segment_boundary.
    Segments segments;
    Segment* s0 = segments.add_segment();
    s0->set_key("とう");
    Candidate* c0 = s0->add_candidate();
    c0->key = "とうきょう";
    c0->value = "東京";
    c0->converted_segment_count = 2;

    Segment* s1 = segments.add_segment();
    s1->set_key("きょう");

    prediction::Result pred_result;
    pred_result.key = "とうきょう";
    pred_result.value = "東京";
    pred_result.inner_segment_boundary = BuildInnerSegmentBoundary(
        {{9, 3, 9, 3}, {6, 3, 6, 3}}, pred_result.key, pred_result.value);

    ApplyResultToSegmentsMultiSegment(pred_result, /*target_pos=*/0, segments);

    const Segment& seg0 = segments.conversion_segment(0);
    ASSERT_EQ(seg0.candidates_size(), 1);
    EXPECT_EQ(seg0.candidate(0).inner_segment_boundary.size(), 2);
  }

  {
    // Target pos beyond candidate list size inserts at end.
    Segments segments;
    Segment* s0 = segments.add_segment();
    s0->set_key("あめ");
    Candidate* c0 = s0->add_candidate();
    c0->key = "あめ";
    c0->value = "飴";
    c0->converted_segment_count = 1;

    prediction::Result pred_result;
    pred_result.key = "あめ";
    pred_result.value = "雨";

    ApplyResultToSegmentsMultiSegment(pred_result, /*target_pos=*/10, segments);

    const Segment& seg0 = segments.conversion_segment(0);
    ASSERT_EQ(seg0.candidates_size(), 2);
    EXPECT_EQ(seg0.candidate(0).value, "飴");
    EXPECT_EQ(seg0.candidate(1).value, "雨");
  }
}

TEST(ConverterUtilTest, MergePredictionResultsNormalHistory) {
  prediction::Result h0, h1, pc0, pc1;
  h0.value = "history0";
  h1.value = "history1";
  pc0.value = "pc0";
  pc1.value = "history0";  // duplicate with h0

  std::vector<prediction::Result> merged =
      MergePredictionResults({h0, h1}, {pc0, pc1});
  ASSERT_EQ(merged.size(), 3);
  EXPECT_EQ(merged[0].value, "history0");
  EXPECT_EQ(merged[1].value, "history1");
  EXPECT_EQ(merged[2].value, "pc0");
}

TEST(ConverterUtilTest, MergePredictionResultsWeakHistory) {
  prediction::Result h0, h1, pc0, pc1;
  h0.value = "weak_history0";
  h0.attributes = Attribute::WEAK_USER_HISTORY_PREDICTION;
  h1.value = "weak_history1";
  pc0.value = "pc0";
  pc1.value = "pc1";

  std::vector<prediction::Result> merged =
      MergePredictionResults({h0, h1}, {pc0, pc1});
  ASSERT_EQ(merged.size(), 4);
  // pc0 is placed at position 0, history follows, then remaining pc results.
  EXPECT_EQ(merged[0].value, "pc0");
  EXPECT_EQ(merged[1].value, "weak_history0");
  EXPECT_EQ(merged[2].value, "weak_history1");
  EXPECT_EQ(merged[3].value, "pc1");
}

TEST(ConverterUtilTest, MergePredictionResultsEmptyInputs) {
  prediction::Result r0;
  r0.value = "value0";

  EXPECT_TRUE(MergePredictionResults({}, {}).empty());

  std::vector<prediction::Result> only_history =
      MergePredictionResults({r0}, {});
  ASSERT_EQ(only_history.size(), 1);
  EXPECT_EQ(only_history[0].value, "value0");

  std::vector<prediction::Result> only_pc = MergePredictionResults({}, {r0});
  ASSERT_EQ(only_pc.size(), 1);
  EXPECT_EQ(only_pc[0].value, "value0");
}

}  // namespace
}  // namespace mozc::converter
