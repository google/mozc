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

#include "rewriter/ivs_variants_rewriter.h"

#include "converter/candidate.h"
#include "converter/segments.h"
#include "request/conversion_request.h"
#include "testing/gunit.h"

namespace mozc {
namespace {

class IvsVariantsRewriterTest : public ::testing::Test {};

TEST_F(IvsVariantsRewriterTest, ExpandIvsVariantsWithSegment_singleCandidate) {
  IvsVariantsRewriter rewriter;
  Segments segments;
  const ConversionRequest request;

  // value == content_value
  {
    Segment* seg = segments.push_back_segment();
    converter::Candidate* candidate = seg->add_candidate();
    candidate->key = "かつらぎし";
    candidate->content_key = "かつらぎし";
    candidate->value = "葛城市";
    candidate->content_value = "葛城市";

    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_EQ(seg->candidates_size(), 2);
    // The original candidate comes first.
    EXPECT_EQ(seg->candidate(0).key, "かつらぎし");
    EXPECT_EQ(seg->candidate(0).content_key, "かつらぎし");
    EXPECT_EQ(seg->candidate(0).value, "葛城市");
    EXPECT_EQ(seg->candidate(0).content_value, "葛城市");
    // Then an IVS candidate comes next.
    EXPECT_EQ(seg->candidate(1).key, "かつらぎし");
    EXPECT_EQ(seg->candidate(1).content_key, "かつらぎし");
    EXPECT_EQ(seg->candidate(1).value, "葛\U000E0100城市");
    EXPECT_EQ(seg->candidate(1).content_value, "葛\U000E0100城市");
    EXPECT_EQ(seg->candidate(1).description, "環境依存(IVS) 正式字体");
  }
  // value != content_value
  // No deciated description.
  {
    Segment* seg = segments.push_back_segment();
    converter::Candidate* candidate = seg->add_candidate();
    candidate->key = "ぎおんの";
    candidate->content_key = "ぎおん";
    candidate->value = "祇園の";
    candidate->content_value = "祇園";

    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_EQ(seg->candidates_size(), 2);
    // The original candidate comes first.
    EXPECT_EQ(seg->candidate(0).key, "ぎおんの");
    EXPECT_EQ(seg->candidate(0).content_key, "ぎおん");
    EXPECT_EQ(seg->candidate(0).value, "祇園の");
    EXPECT_EQ(seg->candidate(0).content_value, "祇園");
    // Then an IVS candidate comes next.
    EXPECT_EQ(seg->candidate(1).key, "ぎおんの");
    EXPECT_EQ(seg->candidate(1).content_key, "ぎおん");
    EXPECT_EQ(seg->candidate(1).value, "祇\U000E0100園の");
    EXPECT_EQ(seg->candidate(1).content_value, "祇\U000E0100園");
    EXPECT_EQ(seg->candidate(1).description, "環境依存(IVS) 礻");
  }
}

TEST_F(IvsVariantsRewriterTest, ExpandIvsVariantsWithSegment_noMatching) {
  IvsVariantsRewriter rewriter;
  Segments segments;
  const ConversionRequest request;

  // content_key doesn't match.
  {
    Segment* seg = segments.push_back_segment();
    converter::Candidate* candidate = seg->add_candidate();
    candidate->key = "かつらぎし";
    candidate->content_key = "かつらぎし？";
    candidate->value = "葛城市";
    candidate->content_value = "葛城市";

    EXPECT_FALSE(rewriter.Rewrite(request, &segments));
  }
  // content_value doesn't match.
  {
    Segment* seg = segments.push_back_segment();
    converter::Candidate* candidate = seg->add_candidate();
    candidate->key = "かつらぎし";
    candidate->content_key = "かつらぎし";
    candidate->value = "葛城市";
    candidate->content_value = "葛城市？";

    EXPECT_FALSE(rewriter.Rewrite(request, &segments));
  }
  // content_key/value needs exact match.
  {
    Segment* seg = segments.push_back_segment();
    converter::Candidate* candidate = seg->add_candidate();
    candidate->key = "かつらぎしりつとしょかん";
    candidate->content_key = "かつらぎしりつとしょかん";
    candidate->value = "葛城市立図書館";
    candidate->content_value = "葛城市市立図書館";

    EXPECT_FALSE(rewriter.Rewrite(request, &segments));
  }
}

TEST_F(IvsVariantsRewriterTest,
       ExpandIvsVariantsWithSegment_multipleCandidate) {
  IvsVariantsRewriter rewriter;
  Segments segments;
  const ConversionRequest request;
  Segment* seg = segments.push_back_segment();

  {
    // IVS 1
    {
      converter::Candidate* candidate = seg->add_candidate();
      candidate->key = "かつらぎし";
      candidate->content_key = "かつらぎし";
      candidate->value = "葛城市";
      candidate->content_value = "葛城市";
    }
    // Non-IVS 1
    {
      converter::Candidate* candidate = seg->add_candidate();
      candidate->key = "いか";
      candidate->content_key = "いか";
      candidate->value = "くコ:彡";
      candidate->content_value = "くコ:彡";
    }
    // IVS 2
    {
      converter::Candidate* candidate = seg->add_candidate();
      candidate->key = "ぎおん";
      candidate->content_key = "ぎおん";
      candidate->value = "祇園";
      candidate->content_value = "祇園";
    }
    // Non-IVS 2
    {
      converter::Candidate* candidate = seg->add_candidate();
      candidate->key = "たこ";
      candidate->content_key = "たこ";
      candidate->value = "Ｃ:。ミ";
      candidate->content_value = "Ｃ:。ミ";
    }

    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_EQ(seg->candidates_size(), 6);
    EXPECT_EQ(seg->candidate(0).value, "葛城市");
    EXPECT_EQ(seg->candidate(1).value, "葛\U000E0100城市");
    EXPECT_EQ(seg->candidate(2).value, "くコ:彡");
    EXPECT_EQ(seg->candidate(3).value, "祇園");
    EXPECT_EQ(seg->candidate(4).value, "祇\U000E0100園");
    EXPECT_EQ(seg->candidate(5).value, "Ｃ:。ミ");
  }
}

}  // namespace
}  // namespace mozc
