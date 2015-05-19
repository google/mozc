// Copyright 2010-2013, Google Inc.
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

#include "base/coordinates.h"
#include "renderer/unix/cairo_wrapper_mock.h"
#include "renderer/unix/const.h"
#include "renderer/unix/draw_tool.h"
#include "testing/base/public/gmock.h"
#include "testing/base/public/gunit.h"

using ::testing::_;
using ::testing::Expectation;

namespace mozc {
namespace renderer {
namespace gtk {

TEST(DrawToolTest, SaveTest) {
  DrawTool draw_tool;
  CairoWrapperMock *mock = new CairoWrapperMock();
  draw_tool.Reset(mock);

  EXPECT_CALL(*mock, Save());
  draw_tool.Save();
}

TEST(DrawToolTest, RestoreTest) {
  DrawTool draw_tool;
  CairoWrapperMock *mock = new CairoWrapperMock();
  draw_tool.Reset(mock);

  EXPECT_CALL(*mock, Restore());
  draw_tool.Restore();
}

TEST(DrawToolTest, FillRectTest) {
  DrawTool draw_tool;
  CairoWrapperMock *mock = new CairoWrapperMock();
  draw_tool.Reset(mock);

  const RGBA color = {0x10, 0x20, 0x30, 0x40};
  const Rect rect(10, 20, 30, 40);

  Expectation set_source_rgba_expectation = EXPECT_CALL(*mock, SetSourceRGBA(
      color.red/255.0, color.green/255.0,
      color.blue/255.0, color.alpha/255.0));
  Expectation rectangle_expectation
      = EXPECT_CALL(*mock, Rectangle(rect.origin.x, rect.origin.y,
                                     rect.size.width, rect.size.height));
  EXPECT_CALL(*mock, Fill())
      .After(rectangle_expectation)
      .After(set_source_rgba_expectation);

  draw_tool.FillRect(rect, color);
}

TEST(DrawToolTest, FrameRectTest) {
  DrawTool draw_tool;
  CairoWrapperMock *mock = new CairoWrapperMock();
  draw_tool.Reset(mock);

  const RGBA color = {0x10, 0x20, 0x30, 0x40};
  const Rect rect(10, 20, 30, 40);
  const uint32 line_width = 3;

  Expectation set_source_rgba_expectation = EXPECT_CALL(*mock, SetSourceRGBA(
      color.red/255.0, color.green/255.0,
      color.blue/255.0, color.alpha/255.0));
  Expectation set_line_width_expectation = EXPECT_CALL(
      *mock, SetLineWidth(static_cast<double>(line_width)));
  Expectation rectangle_expectation
      = EXPECT_CALL(*mock, Rectangle(rect.origin.x, rect.origin.y,
                                     rect.size.width, rect.size.height));
  EXPECT_CALL(*mock, Stroke())
      .After(rectangle_expectation)
      .After(set_line_width_expectation)
      .After(set_source_rgba_expectation);

  draw_tool.FrameRect(rect, color, line_width);
}

TEST(DrawToolTest, DrawLineTest) {
  DrawTool draw_tool;
  CairoWrapperMock *mock = new CairoWrapperMock();
  draw_tool.Reset(mock);

  const RGBA color = {0x10, 0x20, 0x30, 0x40};
  const uint32 line_width = 3;
  const Point from(10, 20);
  const Point to(15, 25);

  Expectation set_source_rgba_expectation = EXPECT_CALL(*mock, SetSourceRGBA(
      color.red/255.0, color.green/255.0,
      color.blue/255.0, color.alpha/255.0));
  Expectation set_line_width_expectation = EXPECT_CALL(
      *mock, SetLineWidth(static_cast<double>(line_width)));
  Expectation move_to_expectation = EXPECT_CALL(
      *mock, MoveTo(from.x, from.y));
  Expectation line_to_expectation
      = EXPECT_CALL(*mock, LineTo(to.x, to.y))
          .After(move_to_expectation);
  EXPECT_CALL(*mock, Stroke())
      .After(line_to_expectation)
      .After(set_line_width_expectation)
      .After(set_source_rgba_expectation);

  draw_tool.DrawLine(from, to, color, line_width);
}

}  // namespace gtk
}  // namespace renderer
}  // namespace mozc
