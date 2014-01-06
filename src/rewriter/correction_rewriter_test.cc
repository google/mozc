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

#include "rewriter/correction_rewriter.h"

#include <string>
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/conversion_request.h"
#include "converter/segments.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace {
static const ReadingCorrectionItem kReadingCorrectionTestItems[] = {
  { "TSUKIGIME", "gekkyoku", "tsukigime" },
};

Segment *AddSegment(const string &key, Segments *segments) {
  Segment *segment = segments->push_back_segment();
  segment->set_key(key);
  return segment;
}

Segment::Candidate *AddCandidate(
    const string &key, const string &value,
    const string &content_key, const string &content_value,
    Segment *segment) {
  Segment::Candidate *candidate = segment->add_candidate();
  candidate->Init();
  candidate->key = key;
  candidate->value = value;
  candidate->content_key = content_key;
  candidate->content_value = content_value;
  return candidate;
}
}  // namespace

class CorrectionRewriterTest : public testing::Test {
 protected:
  virtual void SetUp() {
    rewriter_.reset(new CorrectionRewriter(
                        kReadingCorrectionTestItems,
                        arraysize(kReadingCorrectionTestItems)));
    config::ConfigHandler::GetDefaultConfig(&default_config_);
    config::Config config(default_config_);
    config.set_use_spelling_correction(true);
    config::ConfigHandler::SetConfig(config);
  }

  virtual void TearDown() {
    config::ConfigHandler::SetConfig(default_config_);
  }

  scoped_ptr<CorrectionRewriter> rewriter_;

 private:
  config::Config default_config_;
};

TEST_F(CorrectionRewriterTest, CapabilityTest) {
  const ConversionRequest request;
  EXPECT_EQ(RewriterInterface::ALL, rewriter_->capability(request));
}

TEST_F(CorrectionRewriterTest, RewriteTest) {
  ConversionRequest request;
  Segments segments;

  Segment *segment = AddSegment("gekkyokuwo", &segments);
  Segment::Candidate *candidate =
      AddCandidate("gekkyokuwo", "TSUKIGIMEwo", "gekkyoku", "TSUKIGIME",
                   segment);
  candidate->attributes |= Segment::Candidate::RERANKED;

  AddCandidate("gekkyokuwo", "GEKKYOKUwo", "gekkyoku", "GEKKYOKU", segment);

  config::Config config;
  config.set_use_spelling_correction(false);
  config::ConfigHandler::SetConfig(config);

  EXPECT_FALSE(rewriter_->Rewrite(request, &segments));

  config.set_use_spelling_correction(true);
  config::ConfigHandler::SetConfig(config);
  EXPECT_TRUE(rewriter_->Rewrite(request, &segments));

  // candidate 0
  EXPECT_EQ(
      (Segment::Candidate::RERANKED | Segment::Candidate::SPELLING_CORRECTION),
      segments.conversion_segment(0).candidate(0).attributes);
  EXPECT_EQ(
      // "もしかして"
      "<\xE3\x82\x82\xE3\x81\x97\xE3\x81\x8B\xE3\x81\x97\xE3\x81\xA6: "
      "tsukigime>",
      segments.conversion_segment(0).candidate(0).description);

  // candidate 1
  EXPECT_EQ(Segment::Candidate::DEFAULT_ATTRIBUTE,
            segments.conversion_segment(0).candidate(1).attributes);
  EXPECT_TRUE(segments.conversion_segment(0).candidate(1).description.empty());

}

}  // namespace mozc
