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

#include "base/system_util.h"
#include "base/util.h"
#include "config/config_handler.h"
#include "converter/segments.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "absl/flags/flag.h"

namespace mozc {
namespace {

const char *kDummyDataVersion = "dataversion";

}  // namespace

class VersionRewriterTest : public ::testing::Test {
 protected:
  void SetUp() override {
    SystemUtil::SetUserProfileDirectory(absl::GetFlag(FLAGS_test_tmpdir));
  }

  static void AddSegment(const std::string &key, const std::string &value,
                         Segments *segments) {
    Segment *segment = segments->push_back_segment();
    segment->set_key(key);
    AddCandidate(key, value, segment);
  }

  static void AddCandidate(const std::string &key, const std::string &value,
                           Segment *segment) {
    Segment::Candidate *candidate = segment->add_candidate();
    candidate->Init();
    candidate->value = value;
    candidate->content_value = value;
    candidate->content_key = key;
  }

  static bool FindCandidateWithPrefix(const std::string &prefix,
                                      const Segments &segments) {
    for (size_t i = 0; i < segments.segments_size(); ++i) {
      for (size_t j = 0; j < segments.segment(i).candidates_size(); ++j) {
        if (Util::StartsWith(segments.segment(i).candidate(j).value, prefix)) {
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
  EXPECT_EQ(RewriterInterface::CONVERSION, rewriter.capability(request));
}

TEST_F(VersionRewriterTest, MobileEnvironmentTest) {
  ConversionRequest convreq;
  commands::Request request;
  convreq.set_request(&request);
  VersionRewriter rewriter(kDummyDataVersion);

  {
    request.set_mixed_conversion(true);
    EXPECT_EQ(RewriterInterface::ALL, rewriter.capability(convreq));
  }

  {
    request.set_mixed_conversion(false);
    EXPECT_EQ(RewriterInterface::CONVERSION, rewriter.capability(convreq));
  }
}


TEST_F(VersionRewriterTest, RewriteTest_Version) {
#ifdef GOOGLE_JAPANESE_INPUT_BUILD
  static const char kVersionPrefixExpected[] = "GoogleJapaneseInput-";
  static const char kVersionPrefixUnexpected[] = "Mozc-";
#else
  static const char kVersionPrefixExpected[] = "Mozc-";
  static const char kVersionPrefixUnexpected[] = "GoogleJapaneseInput-";
#endif  // GOOGLE_JAPANESE_INPUT_BUILD

  VersionRewriter version_rewriter(kDummyDataVersion);

  const ConversionRequest request;
  Segments segments;
  VersionRewriterTest::AddSegment("ばーじょん", "バージョン", &segments);

  EXPECT_TRUE(version_rewriter.Rewrite(request, &segments));
  EXPECT_TRUE(VersionRewriterTest::FindCandidateWithPrefix(
      kVersionPrefixExpected, segments));
  EXPECT_FALSE(VersionRewriterTest::FindCandidateWithPrefix(
      kVersionPrefixUnexpected, segments));
}

}  // namespace mozc
