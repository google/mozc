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

#include <string>

#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "converter/candidate.h"
#include "converter/segments.h"
#include "testing/gunit.h"

namespace mozc {
namespace {

void AddCandidate(const absl::string_view key, const absl::string_view value,
                  Segment *segment) {
  converter::Candidate *candidate = segment->push_back_candidate();
  candidate->key = std::string(key);
  candidate->value = std::string(value);
}

TEST(RewriterUtilTest, CalculateInsertPositionTest_UserHistory) {
  Segment segment;
  for (int i = 0; i < 5; ++i) {
    AddCandidate(absl::StrFormat("key%d", i), absl::StrFormat("value%d", i),
                 &segment);
  }

  for (int i = 0; i < 3; ++i) {
    segment.mutable_candidate(i)->attributes =
        converter::Candidate::USER_HISTORY_PREDICTION;
  }

  EXPECT_EQ(RewriterUtil::CalculateInsertPosition(segment, 0), 3);
  EXPECT_EQ(RewriterUtil::CalculateInsertPosition(segment, 1), 4);
  EXPECT_EQ(RewriterUtil::CalculateInsertPosition(segment, 2), 5);
  EXPECT_EQ(RewriterUtil::CalculateInsertPosition(segment, 3), 5);
}

TEST(RewriterUtilTest, CalculateInsertPositionTest_NoUserHistory) {
  Segment segment;
  for (int i = 0; i < 5; ++i) {
    AddCandidate(absl::StrFormat("key%d", i), absl::StrFormat("value%d", i),
                 &segment);
  }

  EXPECT_EQ(RewriterUtil::CalculateInsertPosition(segment, 0), 0);
  EXPECT_EQ(RewriterUtil::CalculateInsertPosition(segment, 1), 1);
  EXPECT_EQ(RewriterUtil::CalculateInsertPosition(segment, 5), 5);
  EXPECT_EQ(RewriterUtil::CalculateInsertPosition(segment, 6), 5);
}

}  // namespace
}  // namespace mozc
