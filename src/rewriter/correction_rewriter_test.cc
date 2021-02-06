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

#include "rewriter/correction_rewriter.h"

#include <memory>
#include <string>

#include "base/port.h"
#include "base/serialized_string_array.h"
#include "config/config_handler.h"
#include "converter/segments.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "testing/base/public/gunit.h"
#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace {

Segment *AddSegment(const std::string &key, Segments *segments) {
  Segment *segment = segments->push_back_segment();
  segment->set_key(key);
  return segment;
}

Segment::Candidate *AddCandidate(const std::string &key,
                                 const std::string &value,
                                 const std::string &content_key,
                                 const std::string &content_value,
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
  CorrectionRewriterTest() {
    convreq_.set_request(&request_);
    convreq_.set_config(&config_);
  }

  void SetUp() override {
    // Create a rewriter with one entry: (TSUKIGIME, gekkyoku, tsukigime)
    const std::vector<absl::string_view> values = {"TSUKIGIME"};
    const std::vector<absl::string_view> errors = {"gekkyoku"};
    const std::vector<absl::string_view> corrections = {"tsukigime"};
    rewriter_ = absl::make_unique<CorrectionRewriter>(
        SerializedStringArray::SerializeToBuffer(values, &values_buf_),
        SerializedStringArray::SerializeToBuffer(errors, &errors_buf_),
        SerializedStringArray::SerializeToBuffer(corrections,
                                                 &corrections_buf_));
    config::ConfigHandler::GetDefaultConfig(&config_);
    config_.set_use_spelling_correction(true);
  }

  std::unique_ptr<CorrectionRewriter> rewriter_;
  ConversionRequest convreq_;
  commands::Request request_;
  config::Config config_;

 private:
  std::unique_ptr<uint32[]> values_buf_;
  std::unique_ptr<uint32[]> errors_buf_;
  std::unique_ptr<uint32[]> corrections_buf_;
};

TEST_F(CorrectionRewriterTest, CapabilityTest) {
  EXPECT_EQ(RewriterInterface::ALL, rewriter_->capability(convreq_));
}

TEST_F(CorrectionRewriterTest, RewriteTest) {
  Segments segments;

  Segment *segment = AddSegment("gekkyokuwo", &segments);
  Segment::Candidate *candidate = AddCandidate(
      "gekkyokuwo", "TSUKIGIMEwo", "gekkyoku", "TSUKIGIME", segment);
  candidate->attributes |= Segment::Candidate::RERANKED;

  AddCandidate("gekkyokuwo", "GEKKYOKUwo", "gekkyoku", "GEKKYOKU", segment);

  config_.set_use_spelling_correction(false);

  EXPECT_FALSE(rewriter_->Rewrite(convreq_, &segments));

  config_.set_use_spelling_correction(true);
  EXPECT_TRUE(rewriter_->Rewrite(convreq_, &segments));

  // candidate 0
  EXPECT_EQ(
      (Segment::Candidate::RERANKED | Segment::Candidate::SPELLING_CORRECTION),
      segments.conversion_segment(0).candidate(0).attributes);
  EXPECT_EQ("<もしかして: tsukigime>",
            segments.conversion_segment(0).candidate(0).description);

  // candidate 1
  EXPECT_EQ(Segment::Candidate::DEFAULT_ATTRIBUTE,
            segments.conversion_segment(0).candidate(1).attributes);
  EXPECT_TRUE(segments.conversion_segment(0).candidate(1).description.empty());
}

}  // namespace mozc
