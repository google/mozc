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

#include "renderer/table_layout.h"

#include "testing/gunit.h"

namespace mozc {
namespace renderer {

#define EXPECT_SIZE_EQ(expect_width, expect_height, actual_size) \
  do {                                                           \
    EXPECT_EQ((actual_size).width, (expect_width));              \
    EXPECT_EQ((actual_size).height, (expect_height));            \
  } while (false)

#define EXPECT_RECT_EQ(expect_left, expect_top, expect_width, expect_height, \
                       actual_rect)                                          \
  do {                                                                       \
    EXPECT_EQ((actual_rect).origin.x, (expect_left));                        \
    EXPECT_EQ((actual_rect).origin.y, (expect_top));                         \
    EXPECT_EQ((actual_rect).Width(), (expect_width));                        \
    EXPECT_EQ((actual_rect).Height(), (expect_height));                      \
  } while (false)

enum COLUMN_TYPE {
  COLUMN_SHORTCUT = 0,
  COLUMN_GAP1,
  COLUMN_CANDIDATE,
  COLUMN_DESCRIPTION,
  NUMBER_OF_COLUMNS,
};

TEST(TableLayoutTest, AllElement) {
  constexpr int kWindowBorder = 1;
  constexpr int kNumberOfRow = 10;
  constexpr int kHeaderHeight = 9;
  constexpr int kFooterHeight = 13;
  constexpr int kVSCrollBarWidth = 11;
  constexpr int kRowRectPadding = 2;

  TableLayout layout;
  layout.Initialize(kNumberOfRow, NUMBER_OF_COLUMNS);
  layout.SetVScrollBar(kVSCrollBarWidth);
  layout.SetRowRectPadding(kRowRectPadding);
  layout.SetWindowBorder(kWindowBorder);

  const Size kGap1(5, 0);

  layout.EnsureHeaderSize(Size(0, kHeaderHeight));
  layout.EnsureFooterSize(Size(0, kFooterHeight));

  layout.EnsureCellSize(COLUMN_GAP1, kGap1);
  for (size_t row = 0; row < kNumberOfRow; ++row) {
    const Size candidate(row + 1, 10);
    const Size description(15, 5);
    layout.EnsureCellSize(COLUMN_CANDIDATE, candidate);
    layout.EnsureCellSize(COLUMN_DESCRIPTION, description);
  }

  layout.FreezeLayout();

  EXPECT_SIZE_EQ(47, 164, layout.GetTotalSize());
  EXPECT_RECT_EQ(1, 1, 45, 9, layout.GetHeaderRect());
  EXPECT_RECT_EQ(1, 150, 45, 13, layout.GetFooterRect());
  EXPECT_RECT_EQ(35, 10, 11, 140, layout.GetVScrollBarRect());
  EXPECT_RECT_EQ(1, 24, 34, 14, layout.GetRowRect(1));
  EXPECT_RECT_EQ(8, 10, 10, 140, layout.GetColumnRect(COLUMN_CANDIDATE));
  EXPECT_RECT_EQ(3, 26, 0, 10, layout.GetCellRect(1, COLUMN_SHORTCUT));
  // Although the specified cell with of column 1 is 2, the actual layout
  // width of this cell is 10 because of the width of column 9.
  EXPECT_RECT_EQ(8, 26, 10, 10, layout.GetCellRect(1, COLUMN_CANDIDATE));
  EXPECT_RECT_EQ(18, 26, 15, 10, layout.GetCellRect(1, COLUMN_DESCRIPTION));
}

TEST(TableLayoutTest, AllElementWithMinimumFooterWidth) {
  constexpr int kWindowBorder = 1;
  constexpr int kNumberOfRow = 10;
  constexpr int kHeaderHeight = 9;
  constexpr int kFooterHeight = 13;
  constexpr int kFooterWidth = 100;
  constexpr int kVSCrollBarWidth = 11;
  constexpr int kRowRectPadding = 2;

  TableLayout layout;
  layout.Initialize(kNumberOfRow, NUMBER_OF_COLUMNS);
  layout.SetVScrollBar(kVSCrollBarWidth);
  layout.SetRowRectPadding(kRowRectPadding);
  layout.SetWindowBorder(kWindowBorder);

  const Size kGap1(5, 0);

  layout.EnsureHeaderSize(Size(0, kHeaderHeight));
  layout.EnsureFooterSize(Size(kFooterWidth, kFooterHeight));

  layout.EnsureCellSize(COLUMN_GAP1, kGap1);
  for (size_t row = 0; row < kNumberOfRow; ++row) {
    const Size candidate(row + 1, 10);
    const Size description(15, 5);
    layout.EnsureCellSize(COLUMN_CANDIDATE, candidate);
    layout.EnsureCellSize(COLUMN_DESCRIPTION, description);
  }

  layout.FreezeLayout();

  // Although the maximum width of cells are 10 + 15 = 25,
  // the expected window width is 102 because of footer width.
  EXPECT_SIZE_EQ(102, 164, layout.GetTotalSize());
  EXPECT_RECT_EQ(1, 1, 100, 9, layout.GetHeaderRect());
  EXPECT_RECT_EQ(1, 150, 100, 13, layout.GetFooterRect());
  EXPECT_RECT_EQ(90, 10, 11, 140, layout.GetVScrollBarRect());
  EXPECT_RECT_EQ(1, 24, 89, 14, layout.GetRowRect(1));
  EXPECT_RECT_EQ(8, 10, 10, 140, layout.GetColumnRect(COLUMN_CANDIDATE));
  EXPECT_RECT_EQ(3, 26, 0, 10, layout.GetCellRect(1, COLUMN_SHORTCUT));
  // Although the specified cell with of column 1 is 2, the actual layout
  // width of this cell is 10 because of the width of column 9.
  EXPECT_RECT_EQ(8, 26, 10, 10, layout.GetCellRect(1, COLUMN_CANDIDATE));
  EXPECT_RECT_EQ(18, 26, 15, 10, layout.GetCellRect(1, COLUMN_DESCRIPTION));
}

TEST(TableLayoutTest, EnsureCellsWidth) {
  TableLayout layout;
  layout.Initialize(1, 4);
  for (size_t i = 0; i < 4; ++i) {
    layout.EnsureCellSize(i, Size(10, 10));
  }
  layout.EnsureColumnsWidth(1, 2, 100);
  layout.FreezeLayout();

  EXPECT_SIZE_EQ(120, 10, layout.GetTotalSize());
  EXPECT_RECT_EQ(0, 0, 10, 10, layout.GetColumnRect(0));
  EXPECT_RECT_EQ(10, 0, 10, 10, layout.GetColumnRect(1));
  EXPECT_RECT_EQ(20, 0, 90, 10, layout.GetColumnRect(2));
  EXPECT_RECT_EQ(110, 0, 10, 10, layout.GetColumnRect(3));
}

TEST(TableLayoutTest, EnsureCellsWidthCallTwice) {
  TableLayout layout;
  layout.Initialize(1, 4);
  for (size_t i = 0; i < 4; ++i) {
    layout.EnsureCellSize(i, Size(10, 10));
  }
  layout.EnsureColumnsWidth(1, 2, 100);
  layout.EnsureColumnsWidth(0, 1, 100);
  layout.FreezeLayout();

  EXPECT_SIZE_EQ(120, 10, layout.GetTotalSize());
  EXPECT_RECT_EQ(0, 0, 10, 10, layout.GetColumnRect(0));
  EXPECT_RECT_EQ(10, 0, 90, 10, layout.GetColumnRect(1));
  EXPECT_RECT_EQ(100, 0, 10, 10, layout.GetColumnRect(2));
  EXPECT_RECT_EQ(110, 0, 10, 10, layout.GetColumnRect(3));
}

TEST(TableLayoutTest, VScrollIndicatorPositions) {
  TableLayout layout;
  // Set the size to 100.
  layout.Initialize(1, 1);
  layout.EnsureCellSize(0, Size(1, 100));
  layout.SetVScrollBar(10);
  layout.FreezeLayout();

  constexpr int kCandidatesTotal = 15;
  const Rect vscrollBarRect = layout.GetVScrollBarRect();
  EXPECT_RECT_EQ(1, 0, 10, 100, layout.GetVScrollBarRect());

  Rect indicatorRect = layout.GetVScrollIndicatorRect(0, 5, kCandidatesTotal);
  EXPECT_EQ(indicatorRect.Left(), vscrollBarRect.Left());
  EXPECT_EQ(indicatorRect.Right(), vscrollBarRect.Right());
  EXPECT_EQ(indicatorRect.Top(), 0);
  EXPECT_EQ(indicatorRect.Bottom(), static_cast<int>(100.0 * 6 / 15 + 0.5));

  indicatorRect = layout.GetVScrollIndicatorRect(5, 10, kCandidatesTotal);
  EXPECT_EQ(indicatorRect.Left(), vscrollBarRect.Left());
  EXPECT_EQ(indicatorRect.Right(), vscrollBarRect.Right());
  EXPECT_EQ(indicatorRect.Top(), static_cast<int>(100.0 * 5 / 15 + 0.5));
  EXPECT_EQ(indicatorRect.Bottom(), static_cast<int>(100.0 * 11 / 15 + 0.5));

  indicatorRect = layout.GetVScrollIndicatorRect(10, 14, kCandidatesTotal);
  EXPECT_EQ(indicatorRect.Left(), vscrollBarRect.Left());
  EXPECT_EQ(indicatorRect.Right(), vscrollBarRect.Right());
  EXPECT_EQ(indicatorRect.Top(), static_cast<int>(100.0 * 10 / 15 + 0.5));
  EXPECT_EQ(indicatorRect.Bottom(), 100);
}

TEST(TableLayoutTest, VScrollVerySmallIndicator) {
  TableLayout layout;
  layout.Initialize(1, 1);
  layout.EnsureCellSize(0, Size(1, 100));
  layout.SetVScrollBar(10);
  layout.FreezeLayout();

  constexpr int kCandidatesTotal = 200;
  EXPECT_RECT_EQ(1, 0, 10, 100, layout.GetVScrollBarRect());
  EXPECT_RECT_EQ(1, 0, 10, 1,
                 layout.GetVScrollIndicatorRect(0, 1, kCandidatesTotal));
  EXPECT_RECT_EQ(1, 99, 10, 1,
                 layout.GetVScrollIndicatorRect(199, 199, kCandidatesTotal));
}

TEST(TableLayoutTest, LayoutFreeze) {
  TableLayout layout;
  layout.Initialize(1, 1);

  EXPECT_FALSE(layout.IsLayoutFrozen());

  layout.FreezeLayout();

  EXPECT_TRUE(layout.IsLayoutFrozen());

  layout.Initialize(1, 1);

  EXPECT_FALSE(layout.IsLayoutFrozen());
}

}  // namespace renderer
}  // namespace mozc
