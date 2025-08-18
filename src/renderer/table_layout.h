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

#ifndef MOZC_RENDERER_TABLE_LAYOUT_H_
#define MOZC_RENDERER_TABLE_LAYOUT_H_

#include <vector>

#include "base/coordinates.h"
#include "renderer/table_layout_interface.h"

namespace mozc {
namespace renderer {

// ------------------------------------------------------------------------
// Schematic view of layout system
// ------------------------------------------------------------------------
//
//     +++++++++++++++++++++++++++++++++++++++++++++++++++++++
//     +HHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHH+
//     +HHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHH+
//     +...................................................II+
//     +.0000000001111122222222222222222222233333333      .II+
//     +.0000000001111122222222222222222222233333333      .II+
//     +.0000000001111122222222222222222222233333333      .II+
//     +...................................................II+
//     +...................................................II+
//     +.0000000001111122222222222222222222233333333      .II+
//     +.0000000001111122222222222222222222233333333      .II+
//     +.0000000001111122222222222222222222233333333      .II+
//     +...................................................II+
//     +...................................................II+
//     +.0000000001111122222222222222222222233333333      .II+
//     +.0000000001111122222222222222222222233333333      .II+
//     +.0000000001111122222222222222222222233333333      .II+
//     +...................................................II+
//     +FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF+
//     +FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF+
//     +FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF+
//     +++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//   Legend:
//   +: WindowBorder   (window_border_pixels_)
//   .: RowRectPadding (row_rect_padding_pixels_)
//   I: Position Indicator
//   H: Header       width = total_size_.width - 2 * window_border_pixels_
//                           >= minimum_header_size_.width
//                   height = minimum_header_size_.height
//   F: Footer       width = total_size_.width - 2 * window_border_pixels_
//                           >= minimum_footer_size_.width
//                   height = minimum_footer_size_.height
//   1: First Cell   width = column_width_[0]
//                   height = row_height_ - 2 * row_rect_padding_pixels_
//   2: Second Cell  width = column_width_[1]
//                   height = row_height_ - 2 * row_rect_padding_pixels_
//
//   All cells have the same height.
//   All cells within the same column have the same width.
//
//  GetRowRect(1)
//     ..................................................
//     .0000000001111122222222222222222222233333333     .
//     .0000000001111122222222222222222222233333333     .
//     .0000000001111122222222222222222222233333333     .
//     ..................................................
//
//  GetColumnRect(1)
//     .....
//     11111
//     11111
//     11111
//     .....
//     .....
//     11111
//     11111
//     11111
//     .....
//     .....
//     11111
//     11111
//     11111
//     .....

class TableLayout : public TableLayoutInterface {
 public:
  TableLayout();
  TableLayout(const TableLayout&) = delete;
  TableLayout& operator=(const TableLayout&) = delete;

  // Reset layout freeze and initialize the number of rows and columns.
  void Initialize(int num_rows, int num_columns) override;

  // Set layout element.
  void SetVScrollBar(int width_in_pixels) override;
  void SetWindowBorder(int width_in_pixels) override;
  void SetRowRectPadding(int width_pixels) override;

  // Ensure the cell size is same to or larger than the specified size.
  // - size.width affects cells within the specified column.
  // - size.height affects all cells.
  // You should not call this function when the layout is frozen.
  void EnsureCellSize(int column, const Size& size) override;

  // Ensure the total width from "from_column" to "to_width" is at
  // least "width" or larger.  If total width is smaller than the
  // "width", the last column (== "to_column") will extend.  If this
  // method is called twice, parameters specified by the second call
  // will be used.  Note that "to_column" should be bigger than
  // "from_column".  Otherwise, the call will be ignored.  If you
  // want to ensure a cell width, you should use EnsureCellSize instead.
  void EnsureColumnsWidth(int from_column, int to_column, int width) override;

  // Ensure the size of header/footer is same to or larger than the
  // specified size.
  // You should not call this function when the layout is frozen.
  void EnsureFooterSize(const Size& size_in_pixels) override;
  void EnsureHeaderSize(const Size& size_in_pixels) override;

  // Fix the layout and calculate the total size.
  void FreezeLayout() override;
  bool IsLayoutFrozen() const override;

  // Get the rect which is bounding the specified cell.
  // This rect does not include RowRectPadding.
  // You should call FreezeLayout prior to this function.
  Rect GetCellRect(int row, int column) const override;

  // Get specified component rect.
  // You should call FreezeLayout prior to these function.
  Size GetTotalSize() const override;
  Rect GetHeaderRect() const override;
  Rect GetFooterRect() const override;
  Rect GetVScrollBarRect() const override;
  Rect GetVScrollIndicatorRect(int begin_index, int end_index,
                               int candidates_total) const override;

  // Get the rect which is bounding the specified row.
  // This rect includes RowRectPadding.
  // You should call FreezeLayout prior to these function.
  Rect GetRowRect(int row) const override;

  // Get specified row rect.
  // This rect includes RowRectPadding.
  // You should call FreezeLayout prior to these function.
  Rect GetColumnRect(int column) const override;

  // Parameter getters
  int number_of_rows() const override;
  int number_of_columns() const override;

 private:
  std::vector<int> column_width_list_;
  Size padding_pixels_;
  Size total_size_;  // this value is valid only when the layout is frozen.
  Size minimum_footer_size_;
  Size minimum_header_size_;

  int ensure_width_from_column_;
  int ensure_width_to_column_;
  int ensure_width_;

  int number_of_rows_;
  int number_of_columns_;
  int window_border_pixels_;
  int row_rect_padding_pixels_;
  int row_height_;  // this value includes (row_rect_padding * 2)
  int vscroll_width_pixels_;

  bool layout_frozen_;
};

}  // namespace renderer
}  // namespace mozc

#endif  // MOZC_RENDERER_TABLE_LAYOUT_H_
