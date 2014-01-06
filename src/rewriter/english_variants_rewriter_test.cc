// Copyright 2010-2014, Google Inc.
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
#include <string>

#include "base/number_util.h"
#include "base/system_util.h"
#include "converter/conversion_request.h"
#include "converter/segments.h"
#include "session/commands.pb.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {

class EnglishVariantsRewriterTest : public testing::Test {
 protected:
  virtual void SetUp() {
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
  }
};

TEST_F(EnglishVariantsRewriterTest, ExpandEnglishVariants) {
  EnglishVariantsRewriter rewriter;
  vector<string> variants;

  EXPECT_TRUE(
      rewriter.ExpandEnglishVariants(
          "foo",
          &variants));
  EXPECT_EQ(2, variants.size());
  EXPECT_EQ("Foo", variants[0]);
  EXPECT_EQ("FOO", variants[1]);

  EXPECT_TRUE(
      rewriter.ExpandEnglishVariants(
          "Bar",
          &variants));
  EXPECT_EQ(2, variants.size());
  EXPECT_EQ("bar", variants[0]);
  EXPECT_EQ("BAR", variants[1]);

  EXPECT_TRUE(
      rewriter.ExpandEnglishVariants(
          "HOGE",
          &variants));
  EXPECT_EQ(2, variants.size());
  EXPECT_EQ("hoge", variants[0]);
  EXPECT_EQ("Hoge", variants[1]);

  EXPECT_FALSE(
      rewriter.ExpandEnglishVariants(
          "Foo Bar",
          &variants));

  EXPECT_TRUE(
      rewriter.ExpandEnglishVariants(
          "iPhone",
          &variants));
  EXPECT_EQ(1, variants.size());
  EXPECT_EQ("iphone", variants[0]);

  EXPECT_TRUE(
      rewriter.ExpandEnglishVariants(
          "MeCab",
          &variants));
  EXPECT_EQ(1, variants.size());
  EXPECT_EQ("mecab", variants[0]);

  EXPECT_FALSE(
      rewriter.ExpandEnglishVariants(
          // "グーグル"
          "\xe3\x82\xb0\xe3\x83\xbc\xe3\x82\xb0\xe3\x83\xab",
          &variants));
}

TEST_F(EnglishVariantsRewriterTest, RewriteTest) {
  EnglishVariantsRewriter rewriter;
  Segments segments;
  const ConversionRequest request;
  Segment *seg = segments.push_back_segment();

  // T13N
  {
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->Init();
    // "ぐーぐる"
    candidate->content_key =
        "\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90\xE3\x82\x8B";
    candidate->key =
        "\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90\xE3\x82\x8B";
    candidate->value = "Google";
    candidate->content_value = "Google";
    candidate->attributes &= ~Segment::Candidate::NO_VARIANTS_EXPANSION;

    EXPECT_EQ(1, seg->candidates_size());
    EXPECT_EQ("Google", seg->candidate(0).value);
    EXPECT_EQ("Google", seg->candidate(0).content_value);
    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_EQ(3, seg->candidates_size());
    EXPECT_EQ("Google", seg->candidate(0).value);
    EXPECT_EQ("Google", seg->candidate(0).content_value);
    EXPECT_EQ("google", seg->candidate(1).value);
    EXPECT_EQ("google", seg->candidate(1).content_value);
    EXPECT_EQ("GOOGLE", seg->candidate(2).value);
    EXPECT_EQ("GOOGLE", seg->candidate(2).content_value);
  }

  {
    seg->Clear();

    for (int i = 0; i < 10; ++i) {
      Segment::Candidate *candidate1 = seg->add_candidate();
      candidate1->Init();
      candidate1->value = NumberUtil::SimpleItoa(i);
      candidate1->content_value = NumberUtil::SimpleItoa(i);
      Segment::Candidate *candidate2 = seg->add_candidate();
      candidate2->Init();
      // "ぐーぐる"
      candidate2->content_key =
          "\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90\xE3\x82\x8B";
      candidate2->key =
          "\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90\xE3\x82\x8B";
      candidate2->value = "Google";
      candidate2->content_value = "Google";
      candidate2->attributes &= ~Segment::Candidate::NO_VARIANTS_EXPANSION;
    }

    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_EQ(40, seg->candidates_size());

    for (int i = 0; i < 10; ++i) {
      EXPECT_EQ(NumberUtil::SimpleItoa(i), seg->candidate(4 * i).value);
      EXPECT_EQ(NumberUtil::SimpleItoa(i), seg->candidate(4 * i).content_value);
      EXPECT_EQ("Google", seg->candidate(4 * i + 1).value);
      EXPECT_EQ("Google", seg->candidate(4 * i + 1).content_value);
      EXPECT_EQ("google", seg->candidate(4 * i + 2).value);
      EXPECT_EQ("google", seg->candidate(4 * i + 2).content_value);
      EXPECT_EQ("GOOGLE", seg->candidate(4 * i + 3).value);
      EXPECT_EQ("GOOGLE", seg->candidate(4 * i + 3).content_value);
    }
  }
}

TEST_F(EnglishVariantsRewriterTest, Regression3242753) {
  EnglishVariantsRewriter rewriter;
  Segments segments;
  const ConversionRequest request;
  Segment *seg = segments.push_back_segment();

  // Multi-word English candidate should not be expanded.
  // NO_VARIANTS_EXPANSION is passed to the candidate only.
  {
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->Init();
    // "まいけるじゃくそん";
    candidate->content_key =
        "\xE3\x81\xBE\xE3\x81\x84\xE3\x81\x91"
        "\xE3\x82\x8B\xE3\x81\x98\xE3\x82\x83"
        "\xE3\x81\x8F\xE3\x81\x9D\xE3\x82\x93";
    candidate->key =
        "\xE3\x81\xBE\xE3\x81\x84\xE3\x81\x91"
        "\xE3\x82\x8B\xE3\x81\x98\xE3\x82\x83"
        "\xE3\x81\x8F\xE3\x81\x9D\xE3\x82\x93";
    candidate->value = "Michael Jackson";
    candidate->content_value = "Michael Jackson";
    candidate->attributes &= ~Segment::Candidate::NO_VARIANTS_EXPANSION;

    EXPECT_EQ(1, seg->candidates_size());
    EXPECT_EQ("Michael Jackson", seg->candidate(0).value);
    EXPECT_EQ("Michael Jackson", seg->candidate(0).content_value);
    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_EQ(1, seg->candidates_size());
    EXPECT_EQ("Michael Jackson", seg->candidate(0).value);
    EXPECT_EQ("Michael Jackson", seg->candidate(0).content_value);
    EXPECT_TRUE(seg->candidate(0).attributes &
                Segment::Candidate::NO_VARIANTS_EXPANSION);
  }
}

TEST_F(EnglishVariantsRewriterTest, Regression5137299) {
  EnglishVariantsRewriter rewriter;
  Segments segments;
  const ConversionRequest request;
  Segment *seg = segments.push_back_segment();

  {
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->Init();
    // "ぐーぐる"
    candidate->content_key =
        "\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90\xE3\x82\x8B";
    candidate->key =
        "\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90\xE3\x82\x8B";
    candidate->value = "Google";
    candidate->content_value = "Google";
    candidate->attributes |= Segment::Candidate::NO_VARIANTS_EXPANSION;
    EXPECT_EQ(1, seg->candidates_size());
    EXPECT_EQ("Google", seg->candidate(0).value);
    EXPECT_EQ("Google", seg->candidate(0).content_value);
    EXPECT_FALSE(rewriter.Rewrite(request, &segments));
    EXPECT_EQ(1, seg->candidates_size());
  }

  {
    seg->clear_candidates();
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->Init();
    // "ぐーぐる"
    candidate->content_key =
        "\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90\xE3\x82\x8B";
    candidate->key =
        "\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90\xE3\x82\x8B";
    candidate->value = "Google";
    candidate->content_value = "Google";
    candidate->attributes |= Segment::Candidate::NO_VARIANTS_EXPANSION;
    candidate->attributes |= Segment::Candidate::USER_DICTIONARY;

    EXPECT_EQ(1, seg->candidates_size());
    EXPECT_EQ("Google", seg->candidate(0).value);
    EXPECT_EQ("Google", seg->candidate(0).content_value);
    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_EQ(3, seg->candidates_size());
    EXPECT_EQ("Google", seg->candidate(0).value);
    EXPECT_EQ("Google", seg->candidate(0).content_value);
    EXPECT_EQ("google", seg->candidate(1).value);
    EXPECT_EQ("google", seg->candidate(1).content_value);
    EXPECT_EQ("GOOGLE", seg->candidate(2).value);
    EXPECT_EQ("GOOGLE", seg->candidate(2).content_value);
  }
}

TEST_F(EnglishVariantsRewriterTest, ExpandEnglishEntry) {
  // Fix variants
  EnglishVariantsRewriter rewriter;
  Segments segments;
  const ConversionRequest request;
  Segment *seg = segments.push_back_segment();

  {
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->Init();
    candidate->content_key = "google";
    candidate->key = "google";
    candidate->value = "Google";
    candidate->content_value = "Google";
    candidate->attributes &= ~Segment::Candidate::NO_VARIANTS_EXPANSION;

    EXPECT_EQ(1, seg->candidates_size());
    EXPECT_EQ("Google", seg->candidate(0).value);
    EXPECT_EQ("Google", seg->candidate(0).content_value);
    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_EQ(1, seg->candidates_size());
    EXPECT_EQ("Google", seg->candidate(0).value);
    EXPECT_EQ("Google", seg->candidate(0).content_value);
    EXPECT_TRUE(seg->candidate(0).attributes &
                Segment::Candidate::NO_VARIANTS_EXPANSION);
  }
}

TEST_F(EnglishVariantsRewriterTest, MobileEnvironmentTest) {
  commands::Request input;
  EnglishVariantsRewriter rewriter;

  {
    input.set_mixed_conversion(true);
    const ConversionRequest request(NULL, &input);
    EXPECT_EQ(RewriterInterface::ALL, rewriter.capability(request));
  }

  {
    input.set_mixed_conversion(false);
    const ConversionRequest request(NULL, &input);
    EXPECT_EQ(RewriterInterface::CONVERSION, rewriter.capability(request));
  }
}

}  // namespace mozc
