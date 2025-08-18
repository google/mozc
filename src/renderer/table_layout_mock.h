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

#ifndef MOZC_RENDERER_TABLE_LAYOUT_MOCK_H_
#define MOZC_RENDERER_TABLE_LAYOUT_MOCK_H_

#include "renderer/table_layout_interface.h"
#include "testing/gmock.h"

namespace mozc {
namespace renderer {

class TableLayoutMock : public TableLayoutInterface {
 public:
  MOCK_METHOD(void, Initialize, (int num_rows, int num_columns), (override));
  MOCK_METHOD(void, SetVScrollBar, (int width_in_pixels), (override));
  MOCK_METHOD(void, SetWindowBorder, (int width_in_pixels), (override));
  MOCK_METHOD(void, SetRowRectPadding, (int width_pixels), (override));
  MOCK_METHOD(void, EnsureCellSize, (int column, const Size& size), (override));
  MOCK_METHOD(void, EnsureColumnsWidth,
              (int from_column, int to_column, int width), (override));
  MOCK_METHOD(void, EnsureFooterSize, (const Size& size_in_pixels), (override));
  MOCK_METHOD(void, EnsureHeaderSize, (const Size& size_in_pixels), (override));
  MOCK_METHOD(void, FreezeLayout, (), (override));
  MOCK_METHOD(bool, IsLayoutFrozen, (), (const, override));
  MOCK_METHOD(Rect, GetCellRect, (int row, int column), (const, override));
  MOCK_METHOD(Size, GetTotalSize, (), (const, override));
  MOCK_METHOD(Rect, GetHeaderRect, (), (const, override));
  MOCK_METHOD(Rect, GetFooterRect, (), (const, override));
  MOCK_METHOD(Rect, GetVScrollBarRect, (), (const, override));
  MOCK_METHOD(Rect, GetVScrollIndicatorRect,
              (int begin_index, int end_index, int candidates_total),
              (const, override));
  MOCK_METHOD(Rect, GetRowRect, (int row), (const, override));
  MOCK_METHOD(Rect, GetColumnRect, (int column), (const, override));
  MOCK_METHOD(int, number_of_rows, (), (const, override));
  MOCK_METHOD(int, number_of_columns, (), (const, override));
};

}  // namespace renderer
}  // namespace mozc

#endif  // MOZC_RENDERER_TABLE_LAYOUT_MOCK_H_
