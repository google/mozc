// Copyright 2010-2012, Google Inc.
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

#include "base/util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/conversion_request.h"
#include "converter/segments.h"
#include "session/commands.pb.h"
#include "session/request_handler.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {

class VersionRewriterTest : public testing::Test {
 protected:
  virtual void SetUp() {
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);
    // Reserve the previous request before starting a test.
    previous_request_.CopyFrom(commands::RequestHandler::GetRequest());
  }

  virtual void TearDown() {
    // and gets back to the request at the end of a test.
    commands::RequestHandler::SetRequest(previous_request_);
  }

  static void AddSegment(const string &key, const string &value,
                         Segments *segments) {
    Segment *segment = segments->push_back_segment();
    segment->set_key(key);
    AddCandidate(key, value, segment);
  }

  static void AddCandidate(const string &key, const string &value,
                           Segment *segment) {
    Segment::Candidate *candidate = segment->add_candidate();
    candidate->Init();
    candidate->value = value;
    candidate->content_value = value;
    candidate->content_key = key;
  }

  static bool FindCandidateWithPrefix(const string &prefix,
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

 private:
  commands::Request previous_request_;
};

TEST_F(VersionRewriterTest, CapabilityTest) {
  // default_request is just declared but not touched at all, so it
  // holds all default values.
  commands::Request default_request;
  commands::RequestHandler::SetRequest(default_request);
  VersionRewriter rewriter;
  EXPECT_EQ(RewriterInterface::CONVERSION, rewriter.capability());
}

TEST_F(VersionRewriterTest, MobileEnvironmentTest) {
  commands::Request input;
  VersionRewriter rewriter;

  input.set_mixed_conversion(true);
  commands::RequestHandler::SetRequest(input);
  EXPECT_EQ(RewriterInterface::ALL, rewriter.capability());

  input.set_mixed_conversion(false);
  commands::RequestHandler::SetRequest(input);
  EXPECT_EQ(RewriterInterface::CONVERSION, rewriter.capability());
}


TEST_F(VersionRewriterTest, RewriteTest_Version) {
#ifdef GOOGLE_JAPANESE_INPUT_BUILD
  static const char kVersionPrefixExpected[] = "GoogleJapaneseInput-";
  static const char kVersionPrefixUnexpected[] = "Mozc-";
#else
  static const char kVersionPrefixExpected[] = "Mozc-";
  static const char kVersionPrefixUnexpected[] = "GoogleJapaneseInput-";
#endif

  VersionRewriter version_rewriter;

  const ConversionRequest request;
  Segments segments;
  VersionRewriterTest::AddSegment(
      // "ばーじょん"
      "\xe3\x81\xb0\xe3\x83\xbc\xe3\x81\x98\xe3\x82\x87\xe3\x82\x93",
      // "バージョン"
      "\xe3\x83\x90\xe3\x83\xbc\xe3\x82\xb8\xe3\x83\xa7\xe3\x83\xb3",
      &segments);

  EXPECT_TRUE(version_rewriter.Rewrite(request, &segments));
  EXPECT_TRUE(VersionRewriterTest::FindCandidateWithPrefix(
      kVersionPrefixExpected, segments));
  EXPECT_FALSE(VersionRewriterTest::FindCandidateWithPrefix(
      kVersionPrefixUnexpected, segments));
}


}  // namespace mozc
