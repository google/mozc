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

#include "renderer/vertical_layout.h"

#include "base/coordinates.h"
#include "testing/gunit.h"

namespace mozc::renderer {
namespace {

void ExpectRect(const Rect &actual, const Rect &expected) {
  EXPECT_EQ(actual.Left(), expected.Left());
  EXPECT_EQ(actual.Top(), expected.Top());
  EXPECT_EQ(actual.Width(), expected.Width());
  EXPECT_EQ(actual.Height(), expected.Height());
}

TEST(VerticalLayoutTest, CandidateColumnsAdvanceRightToLeft) {
  const Rect first = VerticalClientRect(Rect(2, 1, 80, 20), 62);
  const Rect second = VerticalClientRect(Rect(2, 21, 80, 20), 62);
  ExpectRect(first, Rect(41, 2, 20, 80));
  ExpectRect(second, Rect(21, 2, 20, 80));
  EXPECT_TRUE(first.PtrInRect(Point(50, 40)));
  EXPECT_FALSE(second.PtrInRect(Point(50, 40)));
  EXPECT_TRUE(second.PtrInRect(Point(30, 40)));
  EXPECT_FALSE(first.PtrInRect(Point(30, 40)));
}

TEST(VerticalLayoutTest, FooterAndScrollbarUseTheSameCoordinateSystem) {
  const Size logical(200, 120);
  const Size physical = VerticalClientSize(logical);
  EXPECT_EQ(physical.width, 120);
  EXPECT_EQ(physical.height, 200);
  ExpectRect(VerticalClientRect(Rect(1, 100, 198, 19), logical.height),
             Rect(1, 1, 19, 198));
  ExpectRect(VerticalClientRect(Rect(190, 1, 9, 99), logical.height),
             Rect(20, 190, 99, 9));
}

TEST(VerticalLayoutTest, LayoutScalesWithoutNegativeRectangles) {
  for (const int scale : {1, 2, 3}) {
    const Rect logical(5 * scale, 10 * scale, 30 * scale, 20 * scale);
    const Rect physical = VerticalClientRect(logical, 80 * scale);
    ExpectRect(physical, Rect(50 * scale, 5 * scale, 20 * scale, 30 * scale));
    EXPECT_GT(physical.Width(), 0);
    EXPECT_GT(physical.Height(), 0);
  }
}

}  // namespace
}  // namespace mozc::renderer
