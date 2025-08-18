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

#include "rewriter/single_kanji_rewriter.h"

#include <cstddef>
#include <memory>
#include <string>

#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "converter/candidate.h"
#include "converter/segments.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/pos_matcher.h"
#include "engine/modules.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"
#include "request/request_test_util.h"
#include "rewriter/rewriter_interface.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {

using dictionary::PosMatcher;

class SingleKanjiRewriterTest : public testing::TestWithTempUserProfile {
 public:
  SingleKanjiRewriterTest()
      : modules_(engine::Modules::Create(
                     std::make_unique<testing::MockDataManager>())
                     .value()) {}

  std::unique_ptr<const SingleKanjiRewriter> CreateSingleKanjiRewriter() const {
    return std::make_unique<SingleKanjiRewriter>(
        modules_->GetPosMatcher(), modules_->GetSingleKanjiDictionary());
  }

  const PosMatcher& pos_matcher() { return modules_->GetPosMatcher(); }

  static void InitSegments(absl::string_view key, absl::string_view value,
                           Segments* segments) {
    Segment* segment = segments->add_segment();
    segment->set_key(key);

    converter::Candidate* candidate = segment->add_candidate();
    candidate->key.assign(key.data(), key.size());
    candidate->content_key.assign(key.data(), key.size());
    candidate->value.assign(value.data(), value.size());
    candidate->content_value.assign(value.data(), value.size());
  }

  static bool Contains(const Segments& segments, absl::string_view word) {
    const Segment& segment = segments.segment(0);
    for (int i = 0; i < segment.candidates_size(); ++i) {
      if (segment.candidate(i).value == word) {
        return true;
      }
    }
    return false;
  }

  static ConversionRequest ConvReq(const commands::Request& request,
                                   ConversionRequest::RequestType type) {
    return ConversionRequestBuilder()
        .SetRequest(request)
        .SetRequestType(type)
        .Build();
  }

  std::unique_ptr<engine::Modules> modules_;
  const ConversionRequest default_request_;
};

TEST_F(SingleKanjiRewriterTest, CapabilityTest) {
  std::unique_ptr<const SingleKanjiRewriter> rewriter =
      CreateSingleKanjiRewriter();

  commands::Request request;
  request.set_mixed_conversion(false);
  const ConversionRequest convreq =
      ConvReq(request, ConversionRequest::CONVERSION);
  EXPECT_EQ(rewriter->capability(convreq), RewriterInterface::CONVERSION);
}

TEST_F(SingleKanjiRewriterTest, SetKeyTest) {
  std::unique_ptr<const SingleKanjiRewriter> rewriter =
      CreateSingleKanjiRewriter();

  Segments segments;
  Segment* segment = segments.add_segment();
  const std::string kKey = "あ";
  segment->set_key(kKey);
  converter::Candidate* candidate = segment->add_candidate();
  // First candidate may be inserted by other rewriters.
  candidate->key = "strange key";
  candidate->content_key = "starnge key";
  candidate->value = "starnge value";
  candidate->content_value = "strange value";

  EXPECT_EQ(segment->candidates_size(), 1);
  rewriter->Rewrite(default_request_, &segments);
  EXPECT_GT(segment->candidates_size(), 1);
  for (size_t i = 1; i < segment->candidates_size(); ++i) {
    EXPECT_EQ(segment->candidate(i).key, kKey);
  }
}

TEST_F(SingleKanjiRewriterTest, MobileEnvironmentTest) {
  commands::Request request;
  std::unique_ptr<const SingleKanjiRewriter> rewriter =
      CreateSingleKanjiRewriter();

  {
    request.set_mixed_conversion(true);
    const ConversionRequest convreq =
        ConvReq(request, ConversionRequest::CONVERSION);
    EXPECT_EQ(rewriter->capability(convreq), RewriterInterface::ALL);
  }

  {
    request.set_mixed_conversion(false);
    const ConversionRequest convreq =
        ConvReq(request, ConversionRequest::CONVERSION);
    EXPECT_EQ(rewriter->capability(convreq), RewriterInterface::CONVERSION);
  }
}

TEST_F(SingleKanjiRewriterTest, NounPrefixTest) {
  std::unique_ptr<const SingleKanjiRewriter> rewriter =
      CreateSingleKanjiRewriter();

  Segments segments;
  Segment* segment1 = segments.add_segment();

  segment1->set_key("み");
  converter::Candidate* candidate1 = segment1->add_candidate();

  candidate1->key = "み";
  candidate1->content_key = "見";
  candidate1->value = "見";
  candidate1->content_value = "見";

  EXPECT_EQ(segment1->candidates_size(), 1);
  rewriter->Rewrite(default_request_, &segments);

  EXPECT_EQ(segment1->candidate(0).value, "未");

  Segment* segment2 = segments.add_segment();

  segment2->set_key("こうたい");
  converter::Candidate* candidate2 = segment2->add_candidate();

  candidate2->key = "こうたい";
  candidate2->content_key = "後退";
  candidate2->value = "後退";

  candidate2->lid = pos_matcher().GetContentWordWithConjugationId();
  candidate2->rid = pos_matcher().GetContentWordWithConjugationId();

  candidate1 = segment1->mutable_candidate(0);
  *candidate1 = converter::Candidate();
  candidate1->key = "み";
  candidate1->content_key = "見";
  candidate1->value = "見";
  candidate1->content_value = "見";

  rewriter->Rewrite(default_request_, &segments);
  EXPECT_EQ(segment1->candidate(0).value, "見");

  // Only applied when right word's POS is noun.
  candidate2->lid = pos_matcher().GetContentNounId();
  candidate2->rid = pos_matcher().GetContentNounId();

  rewriter->Rewrite(default_request_, &segments);
  EXPECT_EQ(segment1->candidate(0).value, "未");

  EXPECT_EQ(segment1->candidate(0).lid, pos_matcher().GetNounPrefixId());
  EXPECT_EQ(segment1->candidate(0).rid, pos_matcher().GetNounPrefixId());
}

TEST_F(SingleKanjiRewriterTest, InsertionPositionTest) {
  std::unique_ptr<const SingleKanjiRewriter> rewriter =
      CreateSingleKanjiRewriter();

  Segments segments;
  Segment* segment = segments.add_segment();

  segment->set_key("あ");
  for (int i = 0; i < 10; ++i) {
    converter::Candidate* candidate = segment->add_candidate();
    candidate->key = segment->key();
    candidate->content_key = segment->key();
    candidate->value = absl::StrFormat("cand%d", i);
    candidate->content_value = candidate->value;
  }

  EXPECT_EQ(segment->candidates_size(), 10);
  EXPECT_TRUE(rewriter->Rewrite(default_request_, &segments));
  EXPECT_LT(10, segment->candidates_size());  // Some candidates were inserted.

  for (int i = 0; i < 10; ++i) {
    // First 10 candidates have not changed.
    const converter::Candidate& candidate = segment->candidate(i);
    EXPECT_EQ(candidate.value, absl::StrFormat("cand%d", i));
  }
}

TEST_F(SingleKanjiRewriterTest, AddDescriptionTest) {
  std::unique_ptr<const SingleKanjiRewriter> rewriter =
      CreateSingleKanjiRewriter();

  Segments segments;

  auto init_segment = [&segments]() {
    segments.Clear();
    Segment* segment = segments.add_segment();

    segment->set_key("あ");
    {
      converter::Candidate* candidate = segment->add_candidate();
      candidate->key = segment->key();
      candidate->content_key = segment->key();
      candidate->value = "亞";  // variant of "亜".
      candidate->content_value = candidate->value;
    }
    return segment;
  };

  // desktop
  {
    Segment* segment = init_segment();
    EXPECT_EQ(segment->candidates_size(), 1);
    EXPECT_TRUE(segment->candidate(0).description.empty());
    EXPECT_TRUE(rewriter->Rewrite(default_request_, &segments));
    EXPECT_LT(1, segment->candidates_size());  // Some candidates were inserted.
    EXPECT_EQ(segment->candidate(0).description, "亜の旧字体");
  }

  // Only sets the description in mixed conversion mode.
  {
    commands::Request request;
    request_test_util::FillMobileRequest(&request);
    Segment* segment = init_segment();
    const ConversionRequest convreq =
        ConvReq(request, ConversionRequest::PREDICTION);
    EXPECT_EQ(segment->candidates_size(), 1);
    EXPECT_TRUE(segment->candidate(0).description.empty());
    EXPECT_TRUE(rewriter->Rewrite(convreq, &segments));
    EXPECT_EQ(1, segment->candidates_size());  // No candidates were inserted.
    EXPECT_EQ(segment->candidate(0).description, "亜の旧字体");
  }
}

TEST_F(SingleKanjiRewriterTest, TriggerConditionForPrediction) {
  std::unique_ptr<const SingleKanjiRewriter> rewriter =
      CreateSingleKanjiRewriter();

  {
    Segments segments;
    InitSegments("あ", "あ", &segments);

    commands::Request request;
    request_test_util::FillMobileRequest(&request);
    const ConversionRequest convreq =
        ConvReq(request, ConversionRequest::PREDICTION);
    ASSERT_TRUE(rewriter->capability(convreq) & RewriterInterface::PREDICTION);
    EXPECT_TRUE(rewriter->Rewrite(convreq, &segments));
  }

  {
    Segments segments;
    InitSegments("あ", "あ", &segments);

    commands::Request request;
    request_test_util::FillMobileRequestWithHardwareKeyboard(&request);
    const ConversionRequest convreq =
        ConvReq(request, ConversionRequest::PREDICTION);
    ASSERT_FALSE(rewriter->capability(convreq) & RewriterInterface::PREDICTION);
  }

  {
    Segments segments;
    InitSegments("あ", "あ", &segments);

    commands::Request request;
    request_test_util::FillMobileRequestWithHardwareKeyboard(&request);
    const ConversionRequest convreq =
        ConvReq(request, ConversionRequest::CONVERSION);
    ASSERT_TRUE(rewriter->capability(convreq) & RewriterInterface::CONVERSION);
    EXPECT_TRUE(rewriter->Rewrite(convreq, &segments));
  }
}

TEST_F(SingleKanjiRewriterTest, NoVariationTest) {
  std::unique_ptr<const SingleKanjiRewriter> rewriter =
      CreateSingleKanjiRewriter();

  Segments segments;
  InitSegments("かみ", "神", &segments);  // U+795E

  commands::Request request;
  request.mutable_decoder_experiment_params()->set_variation_character_types(
      commands::DecoderExperimentParams::NO_VARIATION);
  const ConversionRequest svs_convreq =
      ConvReq(request, ConversionRequest::CONVERSION);

  EXPECT_EQ(segments.segment(0).candidates_size(), 1);
  EXPECT_TRUE(rewriter->Rewrite(svs_convreq, &segments));
  EXPECT_FALSE(Contains(segments, "\u795E\uFE00"));  // 神︀ SVS character.
  EXPECT_TRUE(Contains(segments, "\uFA19"));         // 神 CJK compat ideograph.
}

TEST_F(SingleKanjiRewriterTest, SvsVariationTest) {
  std::unique_ptr<const SingleKanjiRewriter> rewriter =
      CreateSingleKanjiRewriter();

  Segments segments;
  InitSegments("かみ", "神", &segments);  // U+795E

  commands::Request request;
  request.mutable_decoder_experiment_params()->set_variation_character_types(
      commands::DecoderExperimentParams::SVS_JAPANESE);
  const ConversionRequest svs_convreq =
      ConvReq(request, ConversionRequest::CONVERSION);

  EXPECT_EQ(segments.segment(0).candidates_size(), 1);
  EXPECT_TRUE(rewriter->Rewrite(svs_convreq, &segments));
  EXPECT_TRUE(Contains(segments, "\u795E\uFE00"));  // 神︀ SVS character.
  EXPECT_FALSE(Contains(segments, "\uFA19"));       // 神 CJK compat ideograph.
}

TEST_F(SingleKanjiRewriterTest, EmptySegments) {
  std::unique_ptr<const SingleKanjiRewriter> rewriter =
      CreateSingleKanjiRewriter();

  Segments segments;

  EXPECT_EQ(segments.conversion_segments_size(), 0);
  EXPECT_FALSE(rewriter->Rewrite(default_request_, &segments));
}

TEST_F(SingleKanjiRewriterTest, EmptyCandidates) {
  std::unique_ptr<const SingleKanjiRewriter> rewriter =
      CreateSingleKanjiRewriter();
  Segments segments;
  Segment* segment = segments.add_segment();
  segment->set_key("み");

  EXPECT_EQ(segments.conversion_segments_size(), 1);
  EXPECT_EQ(segments.conversion_segment(0).candidates_size(), 0);
  EXPECT_FALSE(rewriter->Rewrite(default_request_, &segments));
}
}  // namespace mozc
