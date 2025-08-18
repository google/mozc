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

#include <algorithm>
#include <numeric>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "base/coordinates.h"

namespace mozc {
namespace renderer {
namespace {
// The minimum height of the indicator in the VScrollBar.
constexpr int kMinimumIndicatorHeight = 1;
}  // namespace

// ------------------------------------------------------------------------
// Initializations
// ------------------------------------------------------------------------

TableLayout::TableLayout()
    : minimum_footer_size_(0, 0),
      minimum_header_size_(0, 0),
      ensure_width_from_column_(0),
      ensure_width_to_column_(0),
      ensure_width_(0),
      number_of_rows_(1),
      number_of_columns_(1),
      window_border_pixels_(1),
      row_rect_padding_pixels_(0),
      row_height_(1),
      vscroll_width_pixels_(0),
      layout_frozen_(false) {}

void TableLayout::Initialize(int num_rows, int num_columns) {
  number_of_rows_ = num_rows;
  number_of_columns_ = num_columns;

  layout_frozen_ = false;
  window_border_pixels_ = 0;
  minimum_footer_size_ = Size(0, 0);
  minimum_header_size_ = Size(0, 0);
  row_rect_padding_pixels_ = 0;
  row_height_ = 0;
  vscroll_width_pixels_ = 0;

  column_width_list_.clear();
  column_width_list_.resize(num_columns);

  total_size_ = Size();
}

// ------------------------------------------------------------------------
// Add layout element
// ------------------------------------------------------------------------

void TableLayout::SetVScrollBar(int width_in_pixels) {
  if (layout_frozen_) {
    LOG(ERROR) << "Layout already frozen";
    return;
  }

  vscroll_width_pixels_ = width_in_pixels;
}

void TableLayout::SetWindowBorder(int width_in_pixels) {
  if (layout_frozen_) {
    LOG(ERROR) << "Layout already frozen";
    return;
  }

  window_border_pixels_ = width_in_pixels;
}

void TableLayout::SetRowRectPadding(int width_pixels) {
  if (layout_frozen_) {
    LOG(ERROR) << "Layout already frozen";
    return;
  }

  row_rect_padding_pixels_ = width_pixels;
}

void TableLayout::EnsureCellSize(int column, const Size& size) {
  if (layout_frozen_) {
    LOG(ERROR) << "Layout already frozen";
    return;
  }
  DCHECK(0 <= column && column < column_width_list_.size());

  column_width_list_[column] = std::max(column_width_list_[column], size.width);
  row_height_ =
      std::max(row_height_, size.height + row_rect_padding_pixels_ * 2);
}

void TableLayout::EnsureColumnsWidth(int from_column, int to_column,
                                     int width) {
  ensure_width_from_column_ = from_column;
  ensure_width_to_column_ = to_column;
  ensure_width_ = width;
}

void TableLayout::EnsureFooterSize(const Size& size_in_pixels) {
  if (layout_frozen_) {
    LOG(ERROR) << "Layout already frozen";
    return;
  }
  minimum_footer_size_.height =
      std::max(minimum_footer_size_.height, size_in_pixels.height);
  minimum_footer_size_.width =
      std::max(minimum_footer_size_.width, size_in_pixels.width);
}

void TableLayout::EnsureHeaderSize(const Size& size_in_pixels) {
  if (layout_frozen_) {
    LOG(ERROR) << "Layout already frozen";
    return;
  }
  minimum_header_size_.height =
      std::max(minimum_header_size_.height, size_in_pixels.height);
  minimum_header_size_.width =
      std::max(minimum_header_size_.width, size_in_pixels.width);
}

// ------------------------------------------------------------------------
// Layout status
// ------------------------------------------------------------------------
void TableLayout::FreezeLayout() {
  if (layout_frozen_) {
    LOG(ERROR) << "Layout already frozen";
    return;
  }

  // extends necessary column for EnsureColumnsWidth()
  if (ensure_width_from_column_ >= 0 &&
      ensure_width_from_column_ < number_of_columns_ &&
      ensure_width_to_column_ > ensure_width_from_column_ &&
      ensure_width_to_column_ < number_of_columns_ && ensure_width_ > 0) {
    int ensured_range_width = 0;
    for (int i = ensure_width_from_column_; i <= ensure_width_to_column_; ++i) {
      ensured_range_width += column_width_list_[i];
    }
    if (ensured_range_width < ensure_width_) {
      column_width_list_[ensure_width_to_column_] +=
          (ensure_width_ - ensured_range_width);
    }
  }

  const int all_cell_width =
      std::accumulate(column_width_list_.begin(), column_width_list_.end(), 0);

  const int table_width =
      row_rect_padding_pixels_ * 2 +  // padding left and right
      all_cell_width +                // sum of all cell
      vscroll_width_pixels_;          // scrollbar width

  // contetnt width is determined as the maximum of
  // { table width, header width, footer width }
  const int content_width = std::max(
      table_width,
      std::max(minimum_footer_size_.width, minimum_header_size_.width));

  const int width = content_width +             // total content width
                    window_border_pixels_ * 2;  // border left and right

  const int all_cell_height = row_height_ * number_of_rows_;

  const int height = window_border_pixels_ * 2 +    // border top and bottom
                     minimum_header_size_.height +  // header height
                     all_cell_height +              // sum of all cell
                     minimum_footer_size_.height;   // footer height

  total_size_ = Size(width, height);
  layout_frozen_ = true;
}

bool TableLayout::IsLayoutFrozen() const { return layout_frozen_; }

// ------------------------------------------------------------------------
// Get component rects
// ------------------------------------------------------------------------

Rect TableLayout::GetCellRect(int row, int column) const {
  if (!layout_frozen_) {
    LOG(ERROR) << "Layout is not frozen yet";
    return Rect();
  }
  DCHECK(0 <= row && row < number_of_rows_);
  DCHECK(0 <= column && column < column_width_list_.size());

  const int width_of_left_cells = std::accumulate(
      column_width_list_.begin(), column_width_list_.begin() + column, 0);

  const int left = window_border_pixels_ +     // border left
                   row_rect_padding_pixels_ +  // row padding left
                   width_of_left_cells;        // left cells

  const int height_of_upper_cells = row_height_ * row;

  const int top = window_border_pixels_ +        // border top
                  minimum_header_size_.height +  // header height
                  height_of_upper_cells;         // upper cells

  const int width = column_width_list_[column];

  Rect rect(left, top, width, row_height_);

  // deflate top and bottom since row_height_ includes the padding
  rect.DeflateRect(0, row_rect_padding_pixels_, 0, row_rect_padding_pixels_);

  return rect;
}

Size TableLayout::GetTotalSize() const {
  if (!layout_frozen_) {
    LOG(ERROR) << "Layout is not frozen yet";
    return Size();
  }

  return total_size_;
}

Rect TableLayout::GetHeaderRect() const {
  if (!layout_frozen_) {
    LOG(ERROR) << "Layout is not frozen yet";
    return Rect();
  }

  const int width = total_size_.width -         // total width
                    window_border_pixels_ * 2;  // border left and rigth

  return Rect(window_border_pixels_, window_border_pixels_, width,
              minimum_header_size_.height);
}

Rect TableLayout::GetFooterRect() const {
  if (!layout_frozen_) {
    LOG(ERROR) << "Layout is not frozen yet";
    return Rect();
  }

  const int top = total_size_.height -           // total width
                  minimum_footer_size_.height -  // footer height
                  window_border_pixels_;         // border bottom

  const int width = total_size_.width -         // total width
                    window_border_pixels_ * 2;  // border left and right

  return Rect(window_border_pixels_, top, width, minimum_footer_size_.height);
}

Rect TableLayout::GetVScrollBarRect() const {
  if (!layout_frozen_) {
    LOG(ERROR) << "Layout is not frozen yet";
    return Rect();
  }

  const int left = total_size_.width -      // total width
                   window_border_pixels_ -  // border right
                   vscroll_width_pixels_;   // vscroll width

  const int top = window_border_pixels_ +       // border top
                  minimum_header_size_.height;  // header height

  const int height = total_size_.height -           // total height
                     window_border_pixels_ * 2 -    // border top and bottom
                     minimum_header_size_.height -  // header height
                     minimum_footer_size_.height;   // footer height

  return Rect(left, top, vscroll_width_pixels_, height);
}

Rect TableLayout::GetVScrollIndicatorRect(int begin_index, int end_index,
                                          int candidates_total) const {
  const Rect& vscroll_rect = GetVScrollBarRect();

  const float scroll_bar_height =
      static_cast<float>(vscroll_rect.Height()) / candidates_total;
  const float top = vscroll_rect.Top() + scroll_bar_height * begin_index;
  const float bottom = vscroll_rect.Top() + scroll_bar_height * (end_index + 1);

  // add 0.5f to round up
  int rounded_top = static_cast<int>(top + 0.5f);
  int rounded_height = static_cast<int>(bottom - top + 0.5f);

  if (rounded_height < kMinimumIndicatorHeight) {
    rounded_height = kMinimumIndicatorHeight;
  }
  if (rounded_top + rounded_height > vscroll_rect.Bottom()) {
    rounded_top = vscroll_rect.Bottom() - rounded_height;
  }

  return Rect(Point(vscroll_rect.Left(), rounded_top),
              Size(vscroll_rect.Width(), rounded_height));
}

Rect TableLayout::GetRowRect(int row) const {
  if (!layout_frozen_) {
    LOG(ERROR) << "Layout is not frozen yet";
    return Rect();
  }
  DCHECK(0 <= row && row < number_of_rows_);

  const int top = window_border_pixels_ +        // border top
                  minimum_header_size_.height +  // header height
                  row_height_ * row;             // upper cells

  const int width = total_size_.width -          // total width
                    window_border_pixels_ * 2 -  // border left and right
                    vscroll_width_pixels_;       // vscroll width

  return Rect(window_border_pixels_, top, width, row_height_);
}

Rect TableLayout::GetColumnRect(int column) const {
  if (!layout_frozen_) {
    LOG(ERROR) << "Layout is not frozen yet";
    return Rect();
  }
  DCHECK(0 <= column && column < column_width_list_.size());

  const int width_of_left_cells = std::accumulate(
      column_width_list_.begin(), column_width_list_.begin() + column, 0);

  const int left = window_border_pixels_ +     // border left
                   row_rect_padding_pixels_ +  // padding left
                   width_of_left_cells;        // left cells

  const int top = window_border_pixels_ +       // border top
                  minimum_header_size_.height;  // header height

  const int width = column_width_list_[column];

  const int height = row_height_ * number_of_rows_;

  return Rect(left, top, width, height);
}

// ------------------------------------------------------------------------
// Parameter getters
// ------------------------------------------------------------------------
int TableLayout::number_of_rows() const { return number_of_rows_; }

int TableLayout::number_of_columns() const { return number_of_columns_; }

}  // namespace renderer
}  // namespace mozc
