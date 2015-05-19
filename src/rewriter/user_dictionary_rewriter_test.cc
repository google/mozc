// Copyright 2010-2013, Google Inc.
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

#include "rewriter/user_dictionary_rewriter.h"

#include <cstddef>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/system_util.h"
#include "base/util.h"
#include "converter/conversion_request.h"
#include "converter/segments.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {

namespace {
void AddCandidate(const string &value,
                  bool is_user_dictionary,
                  Segments *segments) {
  segments->set_request_type(Segments::CONVERSION);
  Segment *seg = NULL;
  if (segments->segments_size() == 0) {
    seg = segments->push_back_segment();
    seg->set_key("test");
  } else {
    seg = segments->mutable_segment(0);
  }
  Segment::Candidate *candidate = seg->add_candidate();
  candidate->Init();
  candidate->key = value;
  candidate->content_key = value;
  candidate->value = value;
  candidate->content_value = value;
  if (is_user_dictionary) {
    candidate->attributes |= Segment::Candidate::USER_DICTIONARY;
  }
}

string GetCandidates(const Segments &segments) {
  CHECK_EQ(1, segments.segments_size());
  const Segment &seg = segments.segment(0);
  vector<string> results;
  for (size_t i = 0; i < seg.candidates_size(); ++i) {
    results.push_back(seg.candidate(i).value);
  }
  string result;
  Util::JoinStrings(results, " ", &result);
  return result;
}
}  // namespace

class UserDictionaryRewriterTest : public testing::Test {
 protected:
  UserDictionaryRewriterTest() {}
  ~UserDictionaryRewriterTest() {}

  virtual void SetUp() {
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
  }

  virtual void TearDown() {}
};

TEST_F(UserDictionaryRewriterTest, RewriteTest) {
  UserDictionaryRewriter rewriter;
  const ConversionRequest request;

  {
    Segments segments;
    AddCandidate("1", false, &segments);
    AddCandidate("2", false, &segments);
    AddCandidate("3", false, &segments);
    AddCandidate("4", false, &segments);
    AddCandidate("5", false, &segments);
    EXPECT_FALSE(rewriter.Rewrite(request, &segments));
    EXPECT_EQ("1 2 3 4 5", GetCandidates(segments));
  }

  {
    Segments segments;
    AddCandidate("1", true, &segments);
    AddCandidate("2", false, &segments);
    AddCandidate("3", false, &segments);
    AddCandidate("4", false, &segments);
    AddCandidate("5", false, &segments);
    EXPECT_FALSE(rewriter.Rewrite(request, &segments));
    EXPECT_EQ("1 2 3 4 5", GetCandidates(segments));
  }

  {
    Segments segments;
    AddCandidate("1", false, &segments);
    AddCandidate("2", true, &segments);
    AddCandidate("3", false, &segments);
    AddCandidate("4", false, &segments);
    AddCandidate("5", false, &segments);
    EXPECT_FALSE(rewriter.Rewrite(request, &segments));
    EXPECT_EQ("1 2 3 4 5", GetCandidates(segments));
  }

  {
    Segments segments;
    AddCandidate("1", false, &segments);
    AddCandidate("2", false, &segments);
    AddCandidate("3", true, &segments);
    AddCandidate("4", false, &segments);
    AddCandidate("5", false, &segments);
    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_EQ("1 3 2 4 5", GetCandidates(segments));
  }
  {
    Segments segments;
    AddCandidate("1", false, &segments);
    AddCandidate("2", false, &segments);
    AddCandidate("3", true, &segments);
    AddCandidate("4", true, &segments);
    AddCandidate("5", false, &segments);
    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_EQ("1 3 4 2 5", GetCandidates(segments));
  }
  {
    Segments segments;
    AddCandidate("1", false, &segments);
    AddCandidate("2", true, &segments);
    AddCandidate("3", true, &segments);
    AddCandidate("4", true, &segments);
    AddCandidate("5", false, &segments);
    EXPECT_FALSE(rewriter.Rewrite(request, &segments));
    EXPECT_EQ("1 2 3 4 5", GetCandidates(segments));
  }
  {
    Segments segments;
    AddCandidate("1", true, &segments);
    AddCandidate("2", true, &segments);
    AddCandidate("3", true, &segments);
    AddCandidate("4", true, &segments);
    AddCandidate("5", true, &segments);
    EXPECT_FALSE(rewriter.Rewrite(request, &segments));
    EXPECT_EQ("1 2 3 4 5", GetCandidates(segments));
  }
  {
    Segments segments;
    AddCandidate("1", true, &segments);
    AddCandidate("2", false, &segments);
    AddCandidate("3", false, &segments);
    AddCandidate("4", true, &segments);
    AddCandidate("5", false, &segments);
    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_EQ("1 4 2 3 5", GetCandidates(segments));
  }
}
}  // namespace mozc
