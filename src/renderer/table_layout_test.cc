// Copyright 2010, Google Inc.
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
#include "testing/base/public/gunit.h"

namespace mozc {
namespace renderer {
namespace win32 {

enum COLUMN_TYPE {
  COLUMN_SHORTCUT = 0,
  COLUMN_GAP1,
  COLUMN_CANDIDATE,
  COLUMN_DESCRIPTION,
  NUMBER_OF_COLUMNS,
};


TEST(TableLayoutTest, AllElement) {
  const int kWindowBorder = 1;
  const int kNumberOfRow = 10;
  const int kHeaderHeight = 9;
  const int kFooterHeight = 13;
  const int kVSCrollBarWidth = 11;
  const int kRowRectPadding = 2;

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

  const Size kExpectedSize(47, 164);

  EXPECT_EQ(kExpectedSize.width, layout.GetTotalSize().width);
  EXPECT_EQ(kExpectedSize.height, layout.GetTotalSize().height);

  EXPECT_EQ(kExpectedSize.width - 2, layout.GetHeaderRect().Width());
  EXPECT_EQ(kHeaderHeight, layout.GetHeaderRect().Height());

  EXPECT_EQ(kExpectedSize.width - 2, layout.GetFooterRect().Width());
  EXPECT_EQ(kFooterHeight, layout.GetFooterRect().Height());

  EXPECT_EQ(kVSCrollBarWidth, layout.GetVScrollBarRect().Width());
  EXPECT_EQ(kExpectedSize.height - 24,
            layout.GetVScrollBarRect().Height());

  EXPECT_EQ(kExpectedSize.width - 2 - kVSCrollBarWidth,
            layout.GetRowRect(1).Width());
  EXPECT_EQ(14, layout.GetRowRect(1).Height());

  EXPECT_EQ(10, layout.GetColumnRect(COLUMN_CANDIDATE).Width());
  EXPECT_EQ(140, layout.GetColumnRect(COLUMN_CANDIDATE).Height());

  EXPECT_EQ(0, layout.GetCellRect(1, COLUMN_SHORTCUT).Width());
  EXPECT_EQ(10, layout.GetCellRect(1, COLUMN_SHORTCUT).Height());

  // Although the specified cell with of column 1 is 2, the actual layout
  // width of this cell is 10 because of the width of column 9.
  EXPECT_EQ(10, layout.GetCellRect(1, COLUMN_CANDIDATE).Width());
  EXPECT_EQ(10, layout.GetCellRect(1, COLUMN_CANDIDATE).Height());

  EXPECT_EQ(15, layout.GetCellRect(1, COLUMN_DESCRIPTION).Width());
  EXPECT_EQ(10, layout.GetCellRect(1, COLUMN_DESCRIPTION).Height());
}

TEST(TableLayoutTest, AllElementWithFooterWidthInsurance) {
  const int kWindowBorder = 1;
  const int kNumberOfRow = 10;
  const int kHeaderHeight = 9;
  const int kFooterHeight = 13;
  const int kFooterWidth = 100;
  const int kVSCrollBarWidth = 11;
  const int kRowRectPadding = 2;

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
  const Size kExpectedSize(102, 164);
  EXPECT_EQ(kExpectedSize.width, layout.GetTotalSize().width);
  EXPECT_EQ(kExpectedSize.height, layout.GetTotalSize().height);

  EXPECT_EQ(kExpectedSize.width - 2, layout.GetHeaderRect().Width());
  EXPECT_EQ(kHeaderHeight, layout.GetHeaderRect().Height());

  EXPECT_EQ(kExpectedSize.width - 2, layout.GetFooterRect().Width());
  EXPECT_EQ(kFooterHeight, layout.GetFooterRect().Height());

  EXPECT_EQ(kVSCrollBarWidth, layout.GetVScrollBarRect().Width());
  EXPECT_EQ(kExpectedSize.height - 24,
            layout.GetVScrollBarRect().Height());

  EXPECT_EQ(90, layout.GetVScrollBarRect().Left());

  EXPECT_EQ(kExpectedSize.width - 2 - kVSCrollBarWidth,
            layout.GetRowRect(1).Width());
  EXPECT_EQ(14, layout.GetRowRect(1).Height());

  EXPECT_EQ(10, layout.GetColumnRect(COLUMN_CANDIDATE).Width());
  EXPECT_EQ(140, layout.GetColumnRect(COLUMN_CANDIDATE).Height());

  EXPECT_EQ(0, layout.GetCellRect(1, COLUMN_SHORTCUT).Width());
  EXPECT_EQ(10, layout.GetCellRect(1, COLUMN_SHORTCUT).Height());

  // Although the specified cell with of column 1 is 2, the actual layout
  // width of this cell is 10 because of the width of column 9.
  EXPECT_EQ(10, layout.GetCellRect(1, COLUMN_CANDIDATE).Width());
  EXPECT_EQ(10, layout.GetCellRect(1, COLUMN_CANDIDATE).Height());

  EXPECT_EQ(15, layout.GetCellRect(1, COLUMN_DESCRIPTION).Width());
  EXPECT_EQ(10, layout.GetCellRect(1, COLUMN_DESCRIPTION).Height());
}

TEST(TableLayoutTest, EnsureCellsWidth) {
  TableLayout layout;
  layout.Initialize(1, 4);
  for (int i = 0; i < 4; ++i) {
    layout.EnsureCellSize(i, Size(10, 10));
  }
  layout.EnsureColumnsWidth(1, 2, 100);
  layout.FreezeLayout();

  EXPECT_EQ(10 + 100 + 10, layout.GetTotalSize().width);
  EXPECT_EQ(10, layout.GetColumnRect(0).Width());
  EXPECT_EQ(10, layout.GetColumnRect(1).Width());
  EXPECT_EQ(100 - 10, layout.GetColumnRect(2).Width());
  EXPECT_EQ(10, layout.GetColumnRect(3).Width());
}

TEST(TableLayoutTest, EnsureCellsWidthCallTwice) {
  TableLayout layout;
  layout.Initialize(1, 4);
  for (int i = 0; i < 4; ++i) {
    layout.EnsureCellSize(i, Size(10, 10));
  }
  layout.EnsureColumnsWidth(1, 2, 100);
  layout.EnsureColumnsWidth(0, 1, 100);
  layout.FreezeLayout();

  EXPECT_EQ(100 + 10 + 10, layout.GetTotalSize().width);
  EXPECT_EQ(10, layout.GetColumnRect(0).Width());
  EXPECT_EQ(100 - 10, layout.GetColumnRect(1).Width());
  EXPECT_EQ(10, layout.GetColumnRect(2).Width());
  EXPECT_EQ(10, layout.GetColumnRect(3).Width());
}

TEST(TableLayoutTest, VScrollIndicatorPositions) {
  TableLayout layout;
  // Set the size to 100.
  layout.Initialize(1, 1);
  layout.EnsureCellSize(0, Size(1, 100));
  layout.SetVScrollBar(10);
  layout.FreezeLayout();

  const int candidates_total = 15;
  const Rect vscrollBarRect = layout.GetVScrollBarRect();
  Rect indicatorRect = layout.GetVScrollIndicatorRect(0, 5, candidates_total);
  EXPECT_EQ(vscrollBarRect.Left(), indicatorRect.Left());
  EXPECT_EQ(vscrollBarRect.Right(), indicatorRect.Right());
  EXPECT_EQ(0, indicatorRect.Top());
  EXPECT_EQ(static_cast<int>(100.0 * 6 / 15 + 0.5), indicatorRect.Bottom());

  indicatorRect = layout.GetVScrollIndicatorRect(5, 10, candidates_total);
  EXPECT_EQ(vscrollBarRect.Left(), indicatorRect.Left());
  EXPECT_EQ(vscrollBarRect.Right(), indicatorRect.Right());
  EXPECT_EQ(static_cast<int>(100.0 * 5 / 15 + 0.5), indicatorRect.Top());
  EXPECT_EQ(static_cast<int>(100.0 * 11 / 15 + 0.5), indicatorRect.Bottom());

  indicatorRect = layout.GetVScrollIndicatorRect(10, 14, candidates_total);
  EXPECT_EQ(vscrollBarRect.Left(), indicatorRect.Left());
  EXPECT_EQ(vscrollBarRect.Right(), indicatorRect.Right());
  EXPECT_EQ(static_cast<int>(100.0 * 10 / 15 + 0.5), indicatorRect.Top());
  EXPECT_EQ(100, indicatorRect.Bottom());
}

TEST(TableLayoutTest, VScrollVerySmallIndicator) {
  TableLayout layout;
  layout.Initialize(1, 1);
  layout.EnsureCellSize(0, Size(1, 100));
  layout.SetVScrollBar(10);
  layout.FreezeLayout();

  const int candidates_total = 200;
  const Rect vscrollbar_rect = layout.GetVScrollBarRect();
  Rect indicator_rect = layout.GetVScrollIndicatorRect(0, 1, candidates_total);
  EXPECT_EQ(0, indicator_rect.Top());
  EXPECT_EQ(1, indicator_rect.Bottom());
  indicator_rect = layout.GetVScrollIndicatorRect(199, 199, candidates_total);
  EXPECT_LE(indicator_rect.Bottom(), vscrollbar_rect.Bottom());
  EXPECT_GT(indicator_rect.Height(), 0);
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

}  // namespace win32
}  // namespace renderer
}  // namespace mozc
