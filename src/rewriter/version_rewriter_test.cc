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

#include "rewriter/version_rewriter.h"

#include <cstddef>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "converter/candidate.h"
#include "converter/segments.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace {

constexpr absl::string_view kDummyDataVersion = "dataversion";

class VersionRewriterTest : public testing::TestWithTempUserProfile {
 protected:
  static void AddSegment(std::string key, std::string value,
                         Segments *segments) {
    Segment *segment = segments->push_back_segment();
    segment->set_key(key);
    AddCandidate(std::move(key), std::move(value), segment);
  }

  static void AddCandidate(std::string key, std::string value,
                           Segment *segment) {
    converter::Candidate *candidate = segment->add_candidate();
    candidate->value = value;
    candidate->content_value = std::move(value);
    candidate->content_key = std::move(key);
  }

  static bool FindCandidateWithPrefix(const absl::string_view prefix,
                                      const Segments &segments) {
    for (const Segment &segment : segments) {
      for (size_t j = 0; j < segment.candidates_size(); ++j) {
        if (segment.candidate(j).value.starts_with(prefix)) {
          return true;
        }
      }
    }
    return false;
  }
};

TEST_F(VersionRewriterTest, CapabilityTest) {
  // Default request.
  const ConversionRequest request;
  VersionRewriter rewriter(kDummyDataVersion);
  EXPECT_EQ(rewriter.capability(request), RewriterInterface::CONVERSION);
}

TEST_F(VersionRewriterTest, MobileEnvironmentTest) {
  commands::Request request;
  VersionRewriter rewriter(kDummyDataVersion);

  {
    request.set_mixed_conversion(true);
    const ConversionRequest convreq =
        ConversionRequestBuilder().SetRequest(request).Build();
    EXPECT_EQ(rewriter.capability(convreq), RewriterInterface::ALL);
  }

  {
    request.set_mixed_conversion(false);
    const ConversionRequest convreq =
        ConversionRequestBuilder().SetRequest(request).Build();
    EXPECT_EQ(rewriter.capability(convreq), RewriterInterface::CONVERSION);
  }
}


TEST_F(VersionRewriterTest, RewriteTestVersion) {
#ifdef GOOGLE_JAPANESE_INPUT_BUILD
  constexpr absl::string_view kVersionPrefixExpected = "GoogleJapaneseInput-";
  constexpr absl::string_view kVersionPrefixUnexpected = "Mozc-";
#else   // GOOGLE_JAPANESE_INPUT_BUILD
  constexpr absl::string_view kVersionPrefixExpected = "Mozc-";
  constexpr absl::string_view kVersionPrefixUnexpected = "GoogleJapaneseInput-";
#endif  // GOOGLE_JAPANESE_INPUT_BUILD

  VersionRewriter version_rewriter(kDummyDataVersion);

  const ConversionRequest request;
  {
    Segments segments;
    VersionRewriterTest::AddSegment("ばーじょん", "バージョン", &segments);

    EXPECT_TRUE(version_rewriter.Rewrite(request, &segments));
    EXPECT_TRUE(VersionRewriterTest::FindCandidateWithPrefix(
        kVersionPrefixExpected, segments));
    EXPECT_FALSE(VersionRewriterTest::FindCandidateWithPrefix(
        kVersionPrefixUnexpected, segments));
  }
  {
    Segments segments;
    VersionRewriterTest::AddSegment("Version", "Version", &segments);

    EXPECT_TRUE(version_rewriter.Rewrite(request, &segments));
    EXPECT_TRUE(VersionRewriterTest::FindCandidateWithPrefix(
        kVersionPrefixExpected, segments));
    EXPECT_FALSE(VersionRewriterTest::FindCandidateWithPrefix(
        kVersionPrefixUnexpected, segments));
  }
}

}  // namespace
}  // namespace mozc
