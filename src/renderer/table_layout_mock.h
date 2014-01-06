// Copyright 2010-2014, Google Inc.
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
#include "testing/base/public/gmock.h"

namespace mozc {
namespace renderer {

class TableLayoutMock : public TableLayoutInterface {
 public:
  MOCK_METHOD2(Initialize, void(int num_rows, int num_columns));
  MOCK_METHOD1(SetVScrollBar, void(int width_in_pixels));
  MOCK_METHOD1(SetWindowBorder, void(int width_in_pixels));
  MOCK_METHOD1(SetRowRectPadding, void(int width_pixels));
  MOCK_METHOD2(EnsureCellSize, void(int column, const Size &size));
  MOCK_METHOD3(EnsureColumnsWidth,
               void(int from_column, int to_column, int width));
  MOCK_METHOD1(EnsureFooterSize, void(const Size &size_in_pixels));
  MOCK_METHOD1(EnsureHeaderSize, void(const Size &size_in_pixels));
  MOCK_METHOD0(FreezeLayout, void());
  MOCK_CONST_METHOD0(IsLayoutFrozen, int());
  MOCK_CONST_METHOD2(GetCellRect, Rect(int row, int column));
  MOCK_CONST_METHOD0(GetTotalSize, Size());
  MOCK_CONST_METHOD0(GetHeaderRect, Rect());
  MOCK_CONST_METHOD0(GetFooterRect, Rect());
  MOCK_CONST_METHOD0(GetVScrollBarRect, Rect());
  MOCK_CONST_METHOD3(GetVScrollIndicatorRect,
                     Rect(int begin_index, int end_index,
                          int candidates_total));
  MOCK_CONST_METHOD1(GetRowRect, Rect(int row));
  MOCK_CONST_METHOD1(GetColumnRect, Rect(int column));
  MOCK_CONST_METHOD0(number_of_rows, int());
  MOCK_CONST_METHOD0(number_of_columns, int());
};

}  // namespace renderer
}  // namespace mozc

#endif  // MOZC_RENDERER_TABLE_LAYOUT_MOCK_H_
