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

#include "rewriter/english_variants_rewriter.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "converter/segments.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/pos_matcher.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {

using ::mozc::dictionary::PosMatcher;

class EnglishVariantsRewriterTest : public testing::TestWithTempUserProfile {
 protected:
  void SetUp() override {
    pos_matcher_.Set(mock_data_manager_.GetPosMatcherData());
    rewriter_ = std::make_unique<EnglishVariantsRewriter>(pos_matcher_);
  }

  bool GetRankFromValue(const Segment &segment, const absl::string_view value,
                        int *rank) {
    for (size_t i = 0; i < segment.candidates_size(); ++i) {
      if (segment.candidate(i).value == value) {
        *rank = static_cast<int>(i);
        return true;
      }
    }
    return false;
  }

  PosMatcher pos_matcher_;
  std::unique_ptr<EnglishVariantsRewriter> rewriter_;

 private:
  const testing::MockDataManager mock_data_manager_;
};

TEST_F(EnglishVariantsRewriterTest, ExpandEnglishVariants) {
  std::vector<std::string> variants;

  EXPECT_TRUE(rewriter_->ExpandEnglishVariants("foo", &variants));
  EXPECT_EQ(variants.size(), 2);
  EXPECT_EQ(variants[0], "Foo");
  EXPECT_EQ(variants[1], "FOO");

  EXPECT_TRUE(rewriter_->ExpandEnglishVariants("Bar", &variants));
  EXPECT_EQ(variants.size(), 2);
  EXPECT_EQ(variants[0], "bar");
  EXPECT_EQ(variants[1], "BAR");

  EXPECT_TRUE(rewriter_->ExpandEnglishVariants("HOGE", &variants));
  EXPECT_EQ(variants.size(), 2);
  EXPECT_EQ(variants[0], "hoge");
  EXPECT_EQ(variants[1], "Hoge");

  EXPECT_FALSE(rewriter_->ExpandEnglishVariants("Foo Bar", &variants));

  EXPECT_TRUE(rewriter_->ExpandEnglishVariants("iPhone", &variants));
  EXPECT_EQ(variants.size(), 1);
  EXPECT_EQ(variants[0], "iphone");

  EXPECT_TRUE(rewriter_->ExpandEnglishVariants("MeCab", &variants));
  EXPECT_EQ(variants.size(), 1);
  EXPECT_EQ(variants[0], "mecab");

  EXPECT_FALSE(rewriter_->ExpandEnglishVariants("グーグル", &variants));
}

TEST_F(EnglishVariantsRewriterTest, ExpandSpacePrefixedVariants) {
  {
    std::vector<std::string> variants;

    EXPECT_TRUE(rewriter_->ExpandSpacePrefixedVariants("Watch", &variants));
    EXPECT_EQ(variants.size(), 1);
    EXPECT_EQ(variants[0], " Watch");

    variants.clear();
    EXPECT_FALSE(rewriter_->ExpandSpacePrefixedVariants(" Watch", &variants));
    EXPECT_EQ(variants.size(), 0);

    variants.clear();
    EXPECT_FALSE(rewriter_->ExpandSpacePrefixedVariants("", &variants));
    EXPECT_EQ(variants.size(), 0);
  }
  {
    std::vector<std::string> variants;
    variants.push_back("PIXEL");
    variants.push_back("pixel");

    EXPECT_TRUE(rewriter_->ExpandSpacePrefixedVariants("Pixel", &variants));
    EXPECT_EQ(variants.size(), 5);
    EXPECT_EQ(variants[0], " Pixel");
    EXPECT_EQ(variants[1], "PIXEL");
    EXPECT_EQ(variants[2], " PIXEL");
    EXPECT_EQ(variants[3], "pixel");
    EXPECT_EQ(variants[4], " pixel");
  }
}

TEST_F(EnglishVariantsRewriterTest, RewriteTest) {
  // T13N
  {
    Segments segments;
    const ConversionRequest request;
    Segment *seg = segments.push_back_segment();
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->content_key = "ぐーぐる";
    candidate->key = "ぐーぐる";
    candidate->value = "Google";
    candidate->content_value = "Google";
    candidate->attributes &= ~Segment::Candidate::NO_VARIANTS_EXPANSION;

    EXPECT_EQ(seg->candidates_size(), 1);
    EXPECT_EQ(seg->candidate(0).value, "Google");
    EXPECT_EQ(seg->candidate(0).content_value, "Google");
    EXPECT_TRUE(rewriter_->Rewrite(request, &segments));
    EXPECT_EQ(seg->candidates_size(), 3);
    EXPECT_EQ(seg->candidate(0).value, "Google");
    EXPECT_EQ(seg->candidate(0).content_value, "Google");
    EXPECT_EQ(seg->candidate(1).value, "google");
    EXPECT_EQ(seg->candidate(1).content_value, "google");
    EXPECT_EQ(seg->candidate(2).value, "GOOGLE");
    EXPECT_EQ(seg->candidate(2).content_value, "GOOGLE");
  }

  // 'Google Japan'
  {
    Segments segments;
    commands::Request request;
    request.mutable_decoder_experiment_params()
        ->set_english_variation_space_insertion_mode(1);
    const ConversionRequest conversion_request = ConversionRequestBuilder()
            .SetRequest(request)
            .Build();

    Segment *seg1 = segments.push_back_segment();
    Segment *seg2 = segments.push_back_segment();
    Segment::Candidate *candidate1 = seg1->add_candidate();
    candidate1->content_key = "ぐーぐる";
    candidate1->key = "ぐーぐる";
    candidate1->value = "Google";
    candidate1->content_value = "Google";
    candidate1->attributes &= ~Segment::Candidate::NO_VARIANTS_EXPANSION;
    Segment::Candidate *candidate2 = seg2->add_candidate();
    candidate2->content_key = "じゃぱん";
    candidate2->key = "じゃぱん";
    candidate2->value = "Japan";
    candidate2->content_value = "Japan";
    candidate2->attributes &= ~Segment::Candidate::NO_VARIANTS_EXPANSION;
    EXPECT_EQ(seg1->candidates_size(), 1);
    EXPECT_EQ(seg1->candidate(0).value, "Google");
    EXPECT_EQ(seg1->candidate(0).content_value, "Google");
    EXPECT_EQ(seg2->candidates_size(), 1);
    EXPECT_EQ(seg2->candidate(0).value, "Japan");
    EXPECT_EQ(seg2->candidate(0).content_value, "Japan");
    EXPECT_TRUE(rewriter_->Rewrite(conversion_request, &segments));
    EXPECT_EQ(seg1->candidates_size(), 3);
    EXPECT_EQ(seg1->candidate(0).value, "Google");
    EXPECT_EQ(seg1->candidate(0).content_value, "Google");
    EXPECT_EQ(seg1->candidate(1).value, "google");
    EXPECT_EQ(seg1->candidate(1).content_value, "google");
    EXPECT_EQ(seg1->candidate(2).value, "GOOGLE");
    EXPECT_EQ(seg1->candidate(2).content_value, "GOOGLE");
    EXPECT_EQ(seg2->candidates_size(), 6);
    EXPECT_EQ(seg2->candidate(0).value, "Japan");
    EXPECT_EQ(seg2->candidate(0).content_value, "Japan");
    EXPECT_EQ(seg2->candidate(1).value, " Japan");
    EXPECT_EQ(seg2->candidate(1).content_value, " Japan");
    EXPECT_EQ(seg2->candidate(2).value, "japan");
    EXPECT_EQ(seg2->candidate(2).content_value, "japan");
    EXPECT_EQ(seg2->candidate(3).value, " japan");
    EXPECT_EQ(seg2->candidate(3).content_value, " japan");
    EXPECT_EQ(seg2->candidate(4).value, "JAPAN");
    EXPECT_EQ(seg2->candidate(4).content_value, "JAPAN");
    EXPECT_EQ(seg2->candidate(5).value, " JAPAN");
    EXPECT_EQ(seg2->candidate(5).content_value, " JAPAN");
  }

  // '<NO CANDIDATE> Japan'
  {
    Segments segments;
    commands::Request request;
    request.mutable_decoder_experiment_params()
        ->set_english_variation_space_insertion_mode(1);
    const ConversionRequest conversion_request = ConversionRequestBuilder()
            .SetRequest(request)
            .Build();

    Segment *seg1 = segments.push_back_segment();
    Segment *seg2 = segments.push_back_segment();
    // When seg1 has no candidates, seg2 will not be expanded.
    Segment::Candidate *candidate2 = seg2->add_candidate();
    candidate2->content_key = "じゃぱん";

    candidate2->key = "じゃぱん";
    candidate2->value = "Japan";
    candidate2->content_value = "Japan";
    candidate2->attributes &= ~Segment::Candidate::NO_VARIANTS_EXPANSION;
    EXPECT_EQ(seg1->candidates_size(), 0);
    EXPECT_EQ(seg2->candidates_size(), 1);
    EXPECT_EQ(seg2->candidate(0).value, "Japan");
    EXPECT_EQ(seg2->candidate(0).content_value, "Japan");
    EXPECT_TRUE(rewriter_->Rewrite(conversion_request, &segments));
    EXPECT_EQ(seg1->candidates_size(), 0);
    EXPECT_EQ(seg2->candidates_size(), 3);
    EXPECT_EQ(seg2->candidate(0).value, "Japan");
    EXPECT_EQ(seg2->candidate(0).content_value, "Japan");
    EXPECT_EQ(seg2->candidate(1).value, "japan");
    EXPECT_EQ(seg2->candidate(1).content_value, "japan");
    EXPECT_EQ(seg2->candidate(2).value, "JAPAN");
    EXPECT_EQ(seg2->candidate(2).content_value, "JAPAN");
  }
  // 'ぐーぐるJapan'
  {
    Segments segments;
    commands::Request request;
    request.mutable_decoder_experiment_params()
        ->set_english_variation_space_insertion_mode(1);
    const ConversionRequest conversion_request = ConversionRequestBuilder()
            .SetRequest(request)
            .Build();

    Segment *seg1 = segments.push_back_segment();
    Segment *seg2 = segments.push_back_segment();

    // When seg1 is not an English word, seg2 will not be expanded.
    Segment::Candidate *candidate1 = seg1->add_candidate();
    candidate1->content_key = "ぐーぐる";
    candidate1->key = "ぐーぐる";
    candidate1->value = "ぐーぐる";
    candidate1->content_value = "ぐーぐる";
    candidate1->attributes &= ~Segment::Candidate::NO_VARIANTS_EXPANSION;
    Segment::Candidate *candidate2 = seg2->add_candidate();
    candidate2->content_key = "じゃぱん";
    candidate2->key = "じゃぱん";
    candidate2->value = "Japan";
    candidate2->content_value = "Japan";
    candidate2->attributes &= ~Segment::Candidate::NO_VARIANTS_EXPANSION;
    EXPECT_EQ(seg1->candidates_size(), 1);
    EXPECT_EQ(seg1->candidate(0).value, "ぐーぐる");
    EXPECT_EQ(seg1->candidate(0).content_value, "ぐーぐる");
    EXPECT_TRUE(rewriter_->Rewrite(conversion_request, &segments));
    EXPECT_EQ(seg2->candidates_size(), 3);
    EXPECT_EQ(seg2->candidate(0).value, "Japan");
    EXPECT_EQ(seg2->candidate(0).content_value, "Japan");
    EXPECT_EQ(seg2->candidate(1).value, "japan");
    EXPECT_EQ(seg2->candidate(1).content_value, "japan");
    EXPECT_EQ(seg2->candidate(2).value, "JAPAN");
    EXPECT_EQ(seg2->candidate(2).content_value, "JAPAN");
  }

  {
    Segments segments;
    const ConversionRequest request;
    Segment *seg = segments.push_back_segment();
    seg->Clear();

    for (int i = 0; i < 10; ++i) {
      Segment::Candidate *candidate1 = seg->add_candidate();
      candidate1->value = std::to_string(i);
      candidate1->content_value = std::to_string(i);
      Segment::Candidate *candidate2 = seg->add_candidate();
      candidate2->content_key = "ぐーぐる";
      candidate2->key = "ぐーぐる";
      candidate2->value = "Google";
      candidate2->content_value = "Google";
      candidate2->attributes &= ~Segment::Candidate::NO_VARIANTS_EXPANSION;
    }

    EXPECT_TRUE(rewriter_->Rewrite(request, &segments));
    EXPECT_EQ(seg->candidates_size(), 40);

    for (int i = 0; i < 10; ++i) {
      EXPECT_EQ(seg->candidate(4 * i).value, std::to_string(i));
      EXPECT_EQ(seg->candidate(4 * i).content_value, std::to_string(i));
      EXPECT_EQ(seg->candidate(4 * i + 1).value, "Google");
      EXPECT_EQ(seg->candidate(4 * i + 1).content_value, "Google");
      EXPECT_EQ(seg->candidate(4 * i + 2).value, "google");
      EXPECT_EQ(seg->candidate(4 * i + 2).content_value, "google");
      EXPECT_EQ(seg->candidate(4 * i + 3).value, "GOOGLE");
      EXPECT_EQ(seg->candidate(4 * i + 3).content_value, "GOOGLE");
    }
  }
}

TEST_F(EnglishVariantsRewriterTest, Regression3242753) {
  Segments segments;
  const ConversionRequest request;
  Segment *seg = segments.push_back_segment();

  // Multi-word English candidate should not be expanded.
  // NO_VARIANTS_EXPANSION is passed to the candidate only.
  {
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->content_key = "まいけるじゃくそん";
    candidate->key = "まいけるじゃくそん";
    candidate->value = "Michael Jackson";
    candidate->content_value = "Michael Jackson";
    candidate->attributes &= ~Segment::Candidate::NO_VARIANTS_EXPANSION;

    EXPECT_EQ(seg->candidates_size(), 1);
    EXPECT_EQ(seg->candidate(0).value, "Michael Jackson");
    EXPECT_EQ(seg->candidate(0).content_value, "Michael Jackson");
    EXPECT_TRUE(rewriter_->Rewrite(request, &segments));
    EXPECT_EQ(seg->candidates_size(), 1);
    EXPECT_EQ(seg->candidate(0).value, "Michael Jackson");
    EXPECT_EQ(seg->candidate(0).content_value, "Michael Jackson");
    EXPECT_NE((seg->candidate(0).attributes &
               Segment::Candidate::NO_VARIANTS_EXPANSION),
              0);
  }
}

TEST_F(EnglishVariantsRewriterTest, Regression5137299) {
  Segments segments;
  const ConversionRequest request;
  Segment *seg = segments.push_back_segment();

  {
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->content_key = "ぐーぐる";
    candidate->key = "ぐーぐる";
    candidate->value = "Google";
    candidate->content_value = "Google";
    candidate->attributes |= Segment::Candidate::NO_VARIANTS_EXPANSION;
    EXPECT_EQ(seg->candidates_size(), 1);
    EXPECT_EQ(seg->candidate(0).value, "Google");
    EXPECT_EQ(seg->candidate(0).content_value, "Google");
    EXPECT_FALSE(rewriter_->Rewrite(request, &segments));
    EXPECT_EQ(seg->candidates_size(), 1);
  }

  {
    seg->clear_candidates();
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->content_key = "ぐーぐる";
    candidate->key = "ぐーぐる";
    candidate->value = "Google";
    candidate->content_value = "Google";
    candidate->attributes |= Segment::Candidate::NO_VARIANTS_EXPANSION;
    candidate->attributes |= Segment::Candidate::USER_DICTIONARY;

    EXPECT_EQ(seg->candidates_size(), 1);
    EXPECT_EQ(seg->candidate(0).value, "Google");
    EXPECT_EQ(seg->candidate(0).content_value, "Google");
    EXPECT_TRUE(rewriter_->Rewrite(request, &segments));
    EXPECT_EQ(seg->candidates_size(), 3);
    EXPECT_EQ(seg->candidate(0).value, "Google");
    EXPECT_EQ(seg->candidate(0).content_value, "Google");
    EXPECT_EQ(seg->candidate(1).value, "google");
    EXPECT_EQ(seg->candidate(1).content_value, "google");
    EXPECT_EQ(seg->candidate(2).value, "GOOGLE");
    EXPECT_EQ(seg->candidate(2).content_value, "GOOGLE");
  }
}

TEST_F(EnglishVariantsRewriterTest, DoNotAddDuplicatedCandidates) {
  Segments segments;
  const ConversionRequest request;
  Segment *seg = segments.push_back_segment();

  {
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->content_key = "ぐーぐる";
    candidate->key = "ぐーぐる";
    candidate->value = "GOOGLE";
    candidate->content_value = "GOOGLE";

    candidate = seg->add_candidate();
    candidate->content_key = "ぐーぐる";
    candidate->key = "ぐーぐる";
    candidate->value = "グーグル";
    candidate->content_value = "グーグル";

    candidate = seg->add_candidate();
    candidate->content_key = "ぐーぐる";
    candidate->key = "ぐーぐる";
    candidate->value = "google";
    candidate->content_value = "google";

    EXPECT_EQ(seg->candidates_size(), 3);
    EXPECT_TRUE(rewriter_->Rewrite(request, &segments));
    EXPECT_EQ(seg->candidates_size(), 4);  // Kana, lower, upper, capitalized
  }
}

TEST_F(EnglishVariantsRewriterTest, KeepRank) {
  Segments segments;
  const ConversionRequest request;
  Segment *seg = segments.push_back_segment();

  {
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->content_key = "ぐーぐる";
    candidate->key = "ぐーぐる";
    candidate->value = "GOOGLE";
    candidate->content_value = "GOOGLE";

    candidate = seg->add_candidate();
    candidate->content_key = "ぐーぐる";
    candidate->key = "ぐーぐる";
    candidate->value = "グーグル";
    candidate->content_value = "グーグル";

    candidate = seg->add_candidate();
    candidate->content_key = "ぐーぐる";
    candidate->key = "ぐーぐる";
    candidate->value = "google";
    candidate->content_value = "google";

    EXPECT_EQ(seg->candidates_size(), 3);
    EXPECT_TRUE(rewriter_->Rewrite(request, &segments));

    int upper_rank, lower_rank, capitalized_rank, kana_rank;
    EXPECT_TRUE(GetRankFromValue(*seg, "GOOGLE", &upper_rank));
    EXPECT_TRUE(GetRankFromValue(*seg, "google", &lower_rank));
    EXPECT_TRUE(GetRankFromValue(*seg, "Google", &capitalized_rank));
    EXPECT_TRUE(GetRankFromValue(*seg, "グーグル", &kana_rank));
    EXPECT_LT(upper_rank, lower_rank);
    EXPECT_LT(kana_rank, lower_rank);
    EXPECT_LT(lower_rank, capitalized_rank);
  }
}

TEST_F(EnglishVariantsRewriterTest, ExpandEnglishEntry) {
  // Fix variants
  Segments segments;
  const ConversionRequest request;
  Segment *seg = segments.push_back_segment();

  {
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->content_key = "google";
    candidate->key = "google";
    candidate->value = "Google";
    candidate->content_value = "Google";
    candidate->attributes &= ~Segment::Candidate::NO_VARIANTS_EXPANSION;

    EXPECT_EQ(seg->candidates_size(), 1);
    EXPECT_EQ(seg->candidate(0).value, "Google");
    EXPECT_EQ(seg->candidate(0).content_value, "Google");
    EXPECT_TRUE(rewriter_->Rewrite(request, &segments));
    EXPECT_EQ(seg->candidates_size(), 1);
    EXPECT_EQ(seg->candidate(0).value, "Google");
    EXPECT_EQ(seg->candidate(0).content_value, "Google");
    EXPECT_NE((seg->candidate(0).attributes &
               Segment::Candidate::NO_VARIANTS_EXPANSION),
              0);
  }
}

TEST_F(EnglishVariantsRewriterTest, DoNotExpandUpperCaseProperNouns) {
  Segments segments;
  const ConversionRequest request;
  Segment *seg = segments.push_back_segment();

  {
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->content_key = "なさ";
    candidate->key = "なさ";
    candidate->content_value = "NASA";
    candidate->value = "NASA";
    candidate->lid = pos_matcher_.GetUniqueNounId();
    candidate->rid = pos_matcher_.GetUniqueNounId();
    candidate->attributes &= ~Segment::Candidate::NO_VARIANTS_EXPANSION;
  }

  EXPECT_TRUE(rewriter_->Rewrite(request, &segments));
  EXPECT_EQ(seg->candidates_size(), 1);
  EXPECT_EQ(seg->candidate(0).value, "NASA");
  EXPECT_NE((seg->candidate(0).attributes &
             Segment::Candidate::NO_VARIANTS_EXPANSION),
            0);
}

TEST_F(EnglishVariantsRewriterTest, ProperNouns) {
  Segments segments;
  const ConversionRequest request;
  Segment *seg = segments.push_back_segment();

  {
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->content_key = "ぐーぐる";
    candidate->key = "ぐーぐる";
    candidate->content_value = "google";
    candidate->value = "google";
    candidate->lid = pos_matcher_.GetUniqueNounId();
    candidate->rid = pos_matcher_.GetUniqueNounId();
    candidate->attributes &= ~Segment::Candidate::NO_VARIANTS_EXPANSION;
  }

  EXPECT_TRUE(rewriter_->Rewrite(request, &segments));
  ASSERT_EQ(seg->candidates_size(), 3);
  EXPECT_EQ(seg->candidate(0).value, "google");
  EXPECT_NE((seg->candidate(0).attributes &
             Segment::Candidate::NO_VARIANTS_EXPANSION),
            0);
  EXPECT_EQ(seg->candidate(1).value, "Google");
  EXPECT_NE((seg->candidate(1).attributes &
             Segment::Candidate::NO_VARIANTS_EXPANSION),
            0);
  EXPECT_EQ(seg->candidate(2).value, "GOOGLE");
  EXPECT_NE((seg->candidate(2).attributes &
             Segment::Candidate::NO_VARIANTS_EXPANSION),
            0);
}

TEST_F(EnglishVariantsRewriterTest, FillConsumedKeySize) {
  Segments segments;
  const ConversionRequest request;
  Segment *seg = segments.push_back_segment();

  constexpr absl::string_view kKey = "なさ";
  {
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->content_key = kKey;
    candidate->key = kKey;
    candidate->content_value = "nasa";
    candidate->value = "nasa";
    candidate->consumed_key_size = kKey.size();
    candidate->attributes |= Segment::Candidate::PARTIALLY_KEY_CONSUMED;
  }

  EXPECT_TRUE(rewriter_->Rewrite(request, &segments));
  EXPECT_GT(seg->candidates_size(), 1);
  for (size_t i = 0; i < seg->candidates_size(); ++i) {
    const Segment::Candidate &c = seg->candidate(i);
    EXPECT_TRUE(c.attributes & Segment::Candidate::PARTIALLY_KEY_CONSUMED);
    EXPECT_EQ(c.consumed_key_size, kKey.size());
  }
}

TEST_F(EnglishVariantsRewriterTest, MobileEnvironmentTest) {
  commands::Request request;

  {
    request.set_mixed_conversion(true);
    const ConversionRequest convreq = ConversionRequestBuilder()
            .SetRequest(request)
            .Build();
    EXPECT_EQ(rewriter_->capability(convreq), RewriterInterface::ALL);
  }

  {
    request.set_mixed_conversion(false);
    const ConversionRequest convreq =
        ConversionRequestBuilder().SetRequest(request).Build();
    EXPECT_EQ(rewriter_->capability(convreq), RewriterInterface::CONVERSION);
  }
}

}  // namespace mozc
