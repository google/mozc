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

#include "rewriter/remove_redundant_candidate_rewriter.h"

#include "converter/candidate.h"
#include "converter/segments.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"
#include "testing/gunit.h"

namespace mozc {
TEST(RemoveRedundantCandidateRewriterTest, RemoveTest) {
  RemoveRedundantCandidateRewriter rewriter;
  Segments segments;
  Segment* segment = segments.add_segment();
  segment->set_key("a");
  converter::Candidate* candidate = segment->add_candidate();
  candidate->key = "a";
  candidate->value = "a";

  const ConversionRequest default_request;
  EXPECT_TRUE(rewriter.Rewrite(default_request, &segments));
  EXPECT_EQ(segment->candidates_size(), 0);
}

TEST(RemoveRedundantCandidateRewriterTest, NoRemoveTest) {
  RemoveRedundantCandidateRewriter rewriter;
  Segments segments;
  Segment* segment = segments.add_segment();
  segment->set_key("a");
  converter::Candidate* candidate = segment->add_candidate();
  candidate->key = "a";
  candidate->value = "aa";

  const ConversionRequest default_request;
  EXPECT_FALSE(rewriter.Rewrite(default_request, &segments));
  EXPECT_EQ(segment->candidates_size(), 1);
}

TEST(RemoveRedundantCandidateRewriterTest, CapabilityTest) {
  RemoveRedundantCandidateRewriter rewriter;
  commands::Request request;
  {
    const ConversionRequest convreq =
        ConversionRequestBuilder().SetRequest(request).Build();
    EXPECT_EQ(rewriter.capability(convreq), RewriterInterface::NOT_AVAILABLE);
  }

  {
    request.set_mixed_conversion(true);
    const ConversionRequest convreq =
        ConversionRequestBuilder().SetRequest(request).Build();
    EXPECT_EQ(rewriter.capability(convreq), RewriterInterface::ALL);
  }
}
}  // namespace mozc
