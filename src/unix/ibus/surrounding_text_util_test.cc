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

#include "unix/ibus/surrounding_text_util.h"

#include <algorithm>
#include <cstdint>
#include <limits>

#include "testing/gunit.h"

namespace mozc {
namespace ibus {

TEST(SurroundingTextUtilTest, GetSafeDelta) {
  const uint kSafeInt32MaxAsUint =
      static_cast<uint>(std::numeric_limits<int32_t>::max());
  const uint kTooLargeUint = kSafeInt32MaxAsUint + 42;

  int32_t delta = 0;

  EXPECT_TRUE(SurroundingTextUtil::GetSafeDelta(42, 10, &delta));
  EXPECT_EQ(delta, 32);

  EXPECT_TRUE(SurroundingTextUtil::GetSafeDelta(1, 1, &delta));
  EXPECT_EQ(delta, 0);

  EXPECT_TRUE(SurroundingTextUtil::GetSafeDelta(0, 1, &delta));
  EXPECT_EQ(delta, -1);

  EXPECT_TRUE(
      SurroundingTextUtil::GetSafeDelta(kSafeInt32MaxAsUint, 0, &delta));
  EXPECT_EQ(delta, kSafeInt32MaxAsUint);
  EXPECT_GE(abs(delta), 0);

  EXPECT_TRUE(
      SurroundingTextUtil::GetSafeDelta(kSafeInt32MaxAsUint + 1, 1, &delta));
  EXPECT_EQ(delta, kSafeInt32MaxAsUint);
  EXPECT_GE(abs(delta), 0);

  EXPECT_TRUE(
      SurroundingTextUtil::GetSafeDelta(0, kSafeInt32MaxAsUint, &delta));
  EXPECT_EQ(delta, -static_cast<int64_t>(kSafeInt32MaxAsUint));
  EXPECT_GE(abs(delta), 0);

  // The result exceeds int32_t.
  EXPECT_FALSE(SurroundingTextUtil::GetSafeDelta(kTooLargeUint, 0, &delta));

  // The result exceeds int32_t.
  EXPECT_FALSE(SurroundingTextUtil::GetSafeDelta(kTooLargeUint, 0, &delta));

  // Underflow.
  EXPECT_FALSE(SurroundingTextUtil::GetSafeDelta(0, kTooLargeUint, &delta));

  // The abs(result) exceeds int32_t.
  EXPECT_FALSE(SurroundingTextUtil::GetSafeDelta(
      static_cast<uint>(-std::numeric_limits<int32_t>::min()), 0, &delta));
}

TEST(SurroundingTextUtilTest, GetAnchorPosFromSelection) {
  uint anchor_pos = 0;

  EXPECT_TRUE(SurroundingTextUtil::GetAnchorPosFromSelection("abcde", "abcde",
                                                             0, &anchor_pos));
  EXPECT_EQ(anchor_pos, 5);

  EXPECT_FALSE(
      SurroundingTextUtil::GetAnchorPosFromSelection("", "a", 0, &anchor_pos));

  EXPECT_FALSE(
      SurroundingTextUtil::GetAnchorPosFromSelection("a", "", 0, &anchor_pos));

  EXPECT_FALSE(SurroundingTextUtil::GetAnchorPosFromSelection("abcde", "aaa", 4,
                                                              &anchor_pos));

  EXPECT_FALSE(SurroundingTextUtil::GetAnchorPosFromSelection("abcde", "aaa",
                                                              10, &anchor_pos));

  EXPECT_FALSE(SurroundingTextUtil::GetAnchorPosFromSelection(
      "aaaaa", "aaaaaaaaaa", 2, &anchor_pos));

  EXPECT_TRUE(SurroundingTextUtil::GetAnchorPosFromSelection("abcde", "abcde",
                                                             5, &anchor_pos));
  EXPECT_EQ(anchor_pos, 0);

  EXPECT_TRUE(SurroundingTextUtil::GetAnchorPosFromSelection("abcde", "bc", 1,
                                                             &anchor_pos));
  EXPECT_EQ(anchor_pos, 3);

  EXPECT_TRUE(SurroundingTextUtil::GetAnchorPosFromSelection("abcde", "bcde", 1,
                                                             &anchor_pos));
  EXPECT_EQ(anchor_pos, 5);

  EXPECT_FALSE(SurroundingTextUtil::GetAnchorPosFromSelection("abcde", "bcdef",
                                                              1, &anchor_pos));

  EXPECT_TRUE(SurroundingTextUtil::GetAnchorPosFromSelection("abcde", "bc", 3,
                                                             &anchor_pos));
  EXPECT_EQ(anchor_pos, 1);

  EXPECT_TRUE(SurroundingTextUtil::GetAnchorPosFromSelection("abcde", "abc", 3,
                                                             &anchor_pos));
  EXPECT_EQ(anchor_pos, 0);

  EXPECT_FALSE(SurroundingTextUtil::GetAnchorPosFromSelection("abcde", "zabc",
                                                              3, &anchor_pos));

  EXPECT_FALSE(SurroundingTextUtil::GetAnchorPosFromSelection("abcde", "_bc", 3,
                                                              &anchor_pos));

  EXPECT_TRUE(SurroundingTextUtil::GetAnchorPosFromSelection("aaaa", "a", 1,
                                                             &anchor_pos));
  EXPECT_EQ(anchor_pos, 2);

  EXPECT_TRUE(SurroundingTextUtil::GetAnchorPosFromSelection("あいう", "あいう",
                                                             0, &anchor_pos));
  EXPECT_EQ(anchor_pos, 3);

  EXPECT_TRUE(SurroundingTextUtil::GetAnchorPosFromSelection("あいう", "あいう",
                                                             3, &anchor_pos));
  EXPECT_EQ(anchor_pos, 0);

  EXPECT_TRUE(SurroundingTextUtil::GetAnchorPosFromSelection("あいう", "いう",
                                                             1, &anchor_pos));
  EXPECT_EQ(anchor_pos, 3);

  EXPECT_TRUE(SurroundingTextUtil::GetAnchorPosFromSelection("あいう", "いう",
                                                             3, &anchor_pos));
  EXPECT_EQ(anchor_pos, 1);

  EXPECT_TRUE(SurroundingTextUtil::GetAnchorPosFromSelection("あいう", "あい",
                                                             2, &anchor_pos));
  EXPECT_EQ(anchor_pos, 0);
}

}  // namespace ibus
}  // namespace mozc
