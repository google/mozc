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

#include "rewriter/fortune_rewriter.h"

#include <cstdio>
#include <string>

#include "base/base.h"
#include "base/logging.h"
#include "base/util.h"
#include "converter/conversion_request.h"
#include "converter/segments.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {

namespace {
void AddSegment(const string &key, const string &value,
                Segments *segments) {
  segments->Clear();
  Segment *seg = segments->push_back_segment();
  seg->set_key(key);
  Segment::Candidate *candidate = seg->add_candidate();
  candidate->Init();
  candidate->value = key;
  candidate->content_key = key;
  candidate->content_value = value;
}

bool HasFortune(const Segments &segments) {
  CHECK_EQ(segments.segments_size(), 1);
  for (size_t i = 0; i < segments.segment(0).candidates_size(); ++i) {
    const Segment::Candidate &candidate = segments.segment(0).candidate(i);
    // description is "今日の運勢"
    if ("\xE4\xBB\x8A\xE6\x97\xA5\xE3\x81\xAE\xE9\x81\x8B\xE5\x8B\xA2"
        == candidate.description) {
      // has valid value?
      if ("\xE5\xA4\xA7\xE5\x90\x89" == candidate.value ||  // "大吉"
          "\xE5\x90\x89" == candidate.value ||              // "吉"
          "\xE4\xB8\xAD\xE5\x90\x89" == candidate.value ||  // "中吉"
          "\xE5\xB0\x8F\xE5\x90\x89" == candidate.value ||  // "小吉"
          "\xE6\x9C\xAB\xE5\x90\x89" == candidate.value ||  // "末吉"
          "\xE5\x87\xB6" == candidate.value) {              // "凶"
        return true;
      }
    }
  }
  return false;
}
}  // namespace

class FortuneRewriterTest : public testing::Test {
 protected:
  virtual void SetUp() {
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
  }
};

TEST_F(FortuneRewriterTest, BasicTest) {
  FortuneRewriter fortune_rewriter;
  const ConversionRequest request;

  Segments segments;
  AddSegment("test", "test", &segments);
  fortune_rewriter.Rewrite(request, &segments);
  EXPECT_FALSE(HasFortune(segments));

  // "おみくじ"
  AddSegment("\xE3\x81\x8A\xE3\x81\xBF\xE3\x81\x8F\xE3\x81\x98",
             "test", &segments);
  fortune_rewriter.Rewrite(request, &segments);
  EXPECT_TRUE(HasFortune(segments));
}
}  // namespace mozc
