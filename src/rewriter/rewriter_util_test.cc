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

#include "rewriter/rewriter_util.h"

#include "converter/segments.h"
#include "testing/base/public/gunit.h"
#include "absl/strings/str_format.h"

namespace mozc {
namespace {

void AddCandidate(const std::string &key, const std::string &value,
                  Segment *segment) {
  Segment::Candidate *candidate = segment->push_back_candidate();
  candidate->Init();
  candidate->key = key;
  candidate->value = value;
}

TEST(RewriterUtilTest, CalculateInsertPositionTest_UserHistory) {
  Segment segment;
  for (int i = 0; i < 5; ++i) {
    AddCandidate(absl::StrFormat("key%d", i), absl::StrFormat("value%d", i),
                 &segment);
  }

  for (int i = 0; i < 3; ++i) {
    segment.mutable_candidate(i)->attributes =
        Segment::Candidate::USER_HISTORY_PREDICTION;
  }

  EXPECT_EQ(3, RewriterUtil::CalculateInsertPosition(segment, 0));
  EXPECT_EQ(3, RewriterUtil::CalculateInsertPosition(segment, 1));
  EXPECT_EQ(3, RewriterUtil::CalculateInsertPosition(segment, 2));
  EXPECT_EQ(3, RewriterUtil::CalculateInsertPosition(segment, 3));
  EXPECT_EQ(4, RewriterUtil::CalculateInsertPosition(segment, 4));
  EXPECT_EQ(5, RewriterUtil::CalculateInsertPosition(segment, 5));
}

TEST(RewriterUtilTest, CalculateInsertPositionTest_NoUserHistory) {
  Segment segment;
  for (int i = 0; i < 5; ++i) {
    AddCandidate(absl::StrFormat("key%d", i), absl::StrFormat("value%d", i),
                 &segment);
  }

  EXPECT_EQ(0, RewriterUtil::CalculateInsertPosition(segment, 0));
  EXPECT_EQ(1, RewriterUtil::CalculateInsertPosition(segment, 1));
  EXPECT_EQ(5, RewriterUtil::CalculateInsertPosition(segment, 5));
  EXPECT_EQ(5, RewriterUtil::CalculateInsertPosition(segment, 6));
}

}  // namespace
}  // namespace mozc
