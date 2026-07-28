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

#include "converter/immutable_decoder.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/string_view.h"
#include "converter/attribute.h"
#include "converter/candidate.h"
#include "converter/immutable_converter.h"
#include "converter/inner_segment.h"
#include "converter/segments.h"
#include "data_manager/testing/mock_data_manager.h"
#include "engine/modules.h"
#include "prediction/result.h"
#include "request/conversion_request.h"
#include "testing/gunit.h"

namespace mozc {
using ::mozc::converter::Candidate;

class ImmutableDecoderTestPeer {
 public:
  static prediction::Result CandidateToResult(const ImmutableDecoder& decoder,
                                              absl::string_view key,
                                              const Candidate& candidate) {
    return decoder.CandidateToResult(key, candidate);
  }
};

namespace {

// Copy of RealtimeDecoder's MakeSegments for testing compatibility.
Segments MakeSegments(const ConversionRequest& request) {
  converter::Segments segments;
  const prediction::Result& result = request.history_result();

  auto add_history_segment = [&](absl::string_view key, absl::string_view value,
                                 absl::string_view content_key,
                                 absl::string_view content_value) {
    converter::Segment* seg = segments.add_segment();
    seg->set_key(key);
    seg->set_segment_type(converter::Segment::HISTORY);
    converter::Candidate* candidate = seg->add_candidate();
    candidate->key = key;
    candidate->value = value;
    candidate->content_key = content_key;
    candidate->content_value = content_value;
  };

  for (const auto& iter : result.inner_segments()) {
    add_history_segment(iter.GetKey(), iter.GetValue(), iter.GetContentKey(),
                        iter.GetContentValue());
  }

  const int history_size = segments.history_segments_size();
  if (history_size > 0) {
    converter::Candidate* candidate =
        segments.mutable_history_segment(history_size - 1)
            ->mutable_candidate(0);
    candidate->cost = result.cost;
    candidate->rid = result.rid;
  }

  segments.add_segment()->set_key(request.key());

  return segments;
}

class MockDataAndDecoders {
 public:
  MockDataAndDecoders() {
    auto modules_or =
        engine::Modules::Create(std::make_unique<testing::MockDataManager>());
    CHECK_OK(modules_or.status());
    modules_ = std::move(modules_or).value();
    converter_ = std::make_unique<ImmutableConverter>(*modules_);
    decoder_ = std::make_unique<ImmutableDecoder>(*modules_);
  }

  ImmutableConverter* GetConverter() { return converter_.get(); }
  ImmutableDecoder* GetDecoder() { return decoder_.get(); }

 private:
  std::unique_ptr<engine::Modules> modules_;
  std::unique_ptr<ImmutableConverter> converter_;
  std::unique_ptr<ImmutableDecoder> decoder_;
};

void CompareResults(const ConversionRequest& request,
                    ImmutableConverter* converter, ImmutableDecoder* decoder) {
  Segments segments = MakeSegments(request);
  ASSERT_TRUE(converter->Convert(request.options(), &segments));

  std::vector<prediction::Result> decoder_results = decoder->Decode(
      request.key(), request.options(), request.history_result());

  ASSERT_EQ(segments.conversion_segments_size(), 1);
  const Segment& segment = segments.conversion_segment(0);

  LOG(INFO) << "Converter candidates size: " << segment.candidates_size();
  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    const Candidate& c = segment.candidate(i);
    LOG(INFO) << "  Converter candidate " << i << ": value=" << c.value
              << " cost=" << c.cost << " wcost=" << c.wcost << " lid=" << c.lid
              << " rid=" << c.rid << " attr=" << c.attributes
              << " struct_cost=" << c.structure_cost;
  }
  LOG(INFO) << "Decoder results size: " << decoder_results.size();
  for (size_t i = 0; i < decoder_results.size(); ++i) {
    const auto& r = decoder_results[i];
    LOG(INFO) << "  Decoder result " << i << ": value=" << r.value
              << " cost=" << r.cost << " wcost=" << r.wcost << " lid=" << r.lid
              << " rid=" << r.rid << " attr=" << r.attributes;
  }

  ASSERT_EQ(segment.candidates_size(), decoder_results.size());

  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    const Candidate& candidate = segment.candidate(i);
    const prediction::Result& result = decoder_results[i];

    EXPECT_EQ(candidate.key, result.key);
    EXPECT_EQ(candidate.value, result.value);
    EXPECT_EQ(candidate.cost, result.cost);
    EXPECT_EQ(candidate.wcost, result.wcost);
    EXPECT_EQ(candidate.lid, result.lid);
    EXPECT_EQ(candidate.rid, result.rid);
    if (request.options().request_type != ConversionRequest::CONVERSION) {
      EXPECT_EQ(candidate.inner_segment_boundary,
                result.inner_segment_boundary);
    }

    uint32_t expected_attributes = candidate.attributes;
    uint32_t actual_attributes = result.GetBehavioralAttributes();
    constexpr uint32_t kMask = ~(converter::Attribute::NO_VARIANTS_EXPANSION |
                                 converter::Attribute::PARTIALLY_KEY_CONSUMED |
                                 converter::Attribute::REALTIME_CONVERSION);
    expected_attributes &= kMask;
    actual_attributes &= kMask;
    EXPECT_EQ(expected_attributes, actual_attributes);
  }
}

class ImmutableDecoderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    data_and_decoders_ = std::make_unique<MockDataAndDecoders>();
  }

  std::unique_ptr<MockDataAndDecoders> data_and_decoders_;
};

TEST_F(ImmutableDecoderTest, CompareWithoutHistory) {
  const ConversionRequest request =
      ConversionRequestBuilder()
          .SetOptions({.request_type = ConversionRequest::PREDICTION,
                       .max_conversion_candidates_size = 10})
          .SetKey("よろしくおねがいしま")
          .Build();
  CompareResults(request, data_and_decoders_->GetConverter(),
                 data_and_decoders_->GetDecoder());
}

TEST_F(ImmutableDecoderTest, CompareWithHistory) {
  prediction::Result history;
  history.cost = 1000;
  history.rid = 100;
  history.key = "わたし";
  history.value = "私";
  history.inner_segment_boundary = converter::BuildInnerSegmentBoundary(
      {{9, 3, 9, 3}}, history.key, history.value);

  const ConversionRequest request =
      ConversionRequestBuilder()
          .SetOptions({.request_type = ConversionRequest::PREDICTION,
                       .max_conversion_candidates_size = 10})
          .SetHistoryResult(history)
          .SetKey("のなまえ")
          .Build();
  CompareResults(request, data_and_decoders_->GetConverter(),
                 data_and_decoders_->GetDecoder());
}

TEST_F(ImmutableDecoderTest, CompareWithPartialCandidates) {
  const ConversionRequest request =
      ConversionRequestBuilder()
          .SetOptions({.request_type = ConversionRequest::PREDICTION,
                       .max_conversion_candidates_size = 10,
                       .create_partial_candidates = true})
          .SetKey("わたしのなまえはなかのです")
          .Build();
  CompareResults(request, data_and_decoders_->GetConverter(),
                 data_and_decoders_->GetDecoder());
}

TEST_F(ImmutableDecoderTest, CandidateToResult_Basic) {
  Candidate candidate;
  candidate.key = "きー";
  candidate.value = "キー";
  candidate.wcost = 10;
  candidate.cost = 20;
  candidate.lid = 100;
  candidate.rid = 200;
  candidate.attributes = converter::Attribute::NO_VARIANTS_EXPANSION;
  candidate.inner_segment_boundary = {1, 2};

  const ImmutableDecoder* decoder = data_and_decoders_->GetDecoder();
  prediction::Result result = ImmutableDecoderTestPeer::CandidateToResult(
      *decoder, "きーよ", candidate);

  EXPECT_EQ(result.key, "きー");
  EXPECT_EQ(result.value, "キー");
  EXPECT_EQ(result.wcost, 10);
  EXPECT_EQ(result.cost, 20);
  EXPECT_EQ(result.lid, 100);
  EXPECT_EQ(result.rid, 200);
  EXPECT_EQ(result.inner_segment_boundary, candidate.inner_segment_boundary);
  EXPECT_TRUE(result.attributes & converter::Attribute::REALTIME_CONVERSION);

  const uint32_t expected_attributes =
      converter::Attribute::NO_VARIANTS_EXPANSION |
      converter::Attribute::PARTIALLY_KEY_CONSUMED |
      converter::Attribute::REALTIME_CONVERSION;
  EXPECT_EQ(result.GetBehavioralAttributes(), expected_attributes);
  EXPECT_EQ(result.consumed_key_size, 2);  // "きー" has 2 chars
}

TEST_F(ImmutableDecoderTest, CandidateToResult_KeyExpanded) {
  Candidate candidate;
  candidate.key = "きー";
  candidate.value = "キー";
  candidate.attributes = converter::Attribute::KEY_EXPANDED_IN_DICTIONARY;

  const ImmutableDecoder* decoder = data_and_decoders_->GetDecoder();
  prediction::Result result = ImmutableDecoderTestPeer::CandidateToResult(
      *decoder, "きーよ", candidate);

  EXPECT_TRUE(result.attributes &
              converter::Attribute::KEY_EXPANDED_IN_DICTIONARY);
  EXPECT_EQ(result.consumed_key_size, 2);  // "きー" has 2 chars
}

TEST_F(ImmutableDecoderTest, CompareWithHistoryAndCompound) {
  prediction::Result history;
  history.cost = 1000;
  history.rid = 100;
  history.key = "だい";
  history.value = "大";
  history.inner_segment_boundary = converter::BuildInnerSegmentBoundary(
      {{6, 3, 6, 3}}, history.key, history.value);

  const ConversionRequest request =
      ConversionRequestBuilder()
          .SetOptions({.request_type = ConversionRequest::CONVERSION,
                       .max_conversion_candidates_size = 10})
          .SetHistoryResult(history)
          .SetKey("じ")
          .Build();
  CompareResults(request, data_and_decoders_->GetConverter(),
                 data_and_decoders_->GetDecoder());
}

TEST_F(ImmutableDecoderTest, CompareWithAlphabet) {
  const ConversionRequest request =
      ConversionRequestBuilder()
          .SetOptions({.request_type = ConversionRequest::PREDICTION,
                       .max_conversion_candidates_size = 10})
          .SetKey("abc")
          .Build();
  CompareResults(request, data_and_decoders_->GetConverter(),
                 data_and_decoders_->GetDecoder());
}

TEST_F(ImmutableDecoderTest, CompareWithKatakana) {
  const ConversionRequest request =
      ConversionRequestBuilder()
          .SetOptions({.request_type = ConversionRequest::PREDICTION,
                       .max_conversion_candidates_size = 10})
          .SetKey("テレビ")
          .Build();
  CompareResults(request, data_and_decoders_->GetConverter(),
                 data_and_decoders_->GetDecoder());
}

}  // namespace
}  // namespace mozc
