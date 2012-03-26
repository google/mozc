// Copyright 2010-2012, Google Inc.
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

#include "renderer/unix/gtk_window_base.h"

#include "renderer/unix/gtk_wrapper_mock.h"
#include "testing/base/public/gmock.h"
#include "testing/base/public/gunit.h"

using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrEq;
using ::testing::_;

namespace mozc {
namespace renderer {
namespace gtk {

namespace {
// Following variable is used testing and it is contant but due to API
// restriction, can not modify const modifier.
GtkWidget *kDummyWindow = reinterpret_cast<GtkWidget*>(0x12345678);
GtkWidget *kDummyCanvas = reinterpret_cast<GtkWidget*>(0x87654321);
}  // namespace

class GtkWindowBaseTest : public testing::Test {
 protected:
  GtkWrapperMock *GetGtkMock() {
    GtkWrapperMock *mock = new GtkWrapperMock();

    // Following functions are expected called by constructor.
    EXPECT_CALL(*mock, GtkWindowNew(GTK_WINDOW_POPUP))
        .WillOnce(Return(kDummyWindow));
    EXPECT_CALL(*mock, GtkDrawingAreaNew()).WillOnce(Return(kDummyCanvas));
    EXPECT_CALL(*mock, GSignalConnect(
        kDummyWindow, StrEq("destroy"),
        G_CALLBACK(GtkWindowBase::OnDestroyThunk), _));
    EXPECT_CALL(*mock, GSignalConnect(
        kDummyCanvas, StrEq("expose-event"),
        G_CALLBACK(GtkWindowBase::OnPaintThunk), _));
    EXPECT_CALL(*mock, GtkContainerAdd(kDummyWindow, kDummyCanvas));

    return mock;
  }
};

TEST_F(GtkWindowBaseTest, ShowWindowTest) {
  GtkWrapperMock *mock = GetGtkMock();
  EXPECT_CALL(*mock, GtkWidgetShowAll(kDummyWindow));

  GtkWindowBase window(mock);
  window.ShowWindow();
}

TEST_F(GtkWindowBaseTest, HideWindowTest) {
  GtkWrapperMock *mock = GetGtkMock();
  EXPECT_CALL(*mock, GtkWidgetHideAll(kDummyWindow));

  GtkWindowBase window(mock);
  window.HideWindow();
}

TEST_F(GtkWindowBaseTest, GetWindowWidgetTest) {
  GtkWrapperMock *mock = GetGtkMock();
  GtkWindowBase window(mock);

  EXPECT_EQ(kDummyWindow, window.GetWindowWidget());
}

TEST_F(GtkWindowBaseTest, GetCanvasWidgetTest) {
  GtkWrapperMock *mock = GetGtkMock();
  GtkWindowBase window(mock);

  EXPECT_EQ(kDummyCanvas, window.GetCanvasWidget());
}

TEST_F(GtkWindowBaseTest, GetWindowPosTest) {
  GtkWrapperMock *mock = GetGtkMock();

  const Point expected_pos(10, 20);

  EXPECT_CALL(*mock, GtkWindowGetPosition(kDummyWindow, _, _)).
      WillOnce(DoAll(SetArgPointee<1>(expected_pos.x),
                     SetArgPointee<2>(expected_pos.y)));

  GtkWindowBase window(mock);
  Point actual_pos = window.GetWindowPos();
  EXPECT_EQ(expected_pos.x, actual_pos.x);
  EXPECT_EQ(expected_pos.y, actual_pos.y);
}

TEST_F(GtkWindowBaseTest, GetWindowSizeTest) {
  GtkWrapperMock *mock = GetGtkMock();

  const Size expected_size(15, 25);

  EXPECT_CALL(*mock, GtkWindowGetSize(kDummyWindow, _, _)).
      WillOnce(DoAll(SetArgPointee<1>(expected_size.width),
                     SetArgPointee<2>(expected_size.height)));

  GtkWindowBase window(mock);
  Size actual_size = window.GetWindowSize();

  EXPECT_EQ(expected_size.width, actual_size.width);
  EXPECT_EQ(expected_size.height, actual_size.height);
}

TEST_F(GtkWindowBaseTest, GetWindowRectTest) {
  GtkWrapperMock *mock = GetGtkMock();

  const Rect expected_rect(10, 20, 15, 25);

  EXPECT_CALL(*mock, GtkWindowGetPosition(kDummyWindow, _, _)).
      WillOnce(DoAll(SetArgPointee<1>(expected_rect.origin.x),
                     SetArgPointee<2>(expected_rect.origin.y)));

  EXPECT_CALL(*mock, GtkWindowGetSize(kDummyWindow, _, _)).
      WillOnce(DoAll(SetArgPointee<1>(expected_rect.size.width),
                     SetArgPointee<2>(expected_rect.size.height)));

  GtkWindowBase window(mock);
  Rect actual_rect = window.GetWindowRect();

  EXPECT_EQ(expected_rect.origin.x, actual_rect.origin.x);
  EXPECT_EQ(expected_rect.origin.y, actual_rect.origin.y);
  EXPECT_EQ(expected_rect.size.width, actual_rect.size.width);
  EXPECT_EQ(expected_rect.size.height, actual_rect.size.height);
}

TEST_F(GtkWindowBaseTest, IsActiveTest) {
  GtkWrapperMock *mock = GetGtkMock();
  EXPECT_CALL(*mock, GtkWindowIsActive(kDummyWindow));

  GtkWindowBase window(mock);
  window.IsActive();
}

TEST_F(GtkWindowBaseTest, DestroyWindowTest) {
  // TODO(nona): Implement this.
}

TEST_F(GtkWindowBaseTest, MoveTest) {
  GtkWrapperMock *mock = GetGtkMock();
  const Point pos(10, 20);
  EXPECT_CALL(*mock, GtkWindowMove(kDummyWindow, pos.x, pos.y));

  GtkWindowBase window(mock);
  window.Move(pos);
}

TEST_F(GtkWindowBaseTest, ResizeTest) {
  GtkWrapperMock *mock = GetGtkMock();
  const Size size(15, 25);
  EXPECT_CALL(*mock, GtkWindowResize(kDummyWindow, size.width, size.height));

  GtkWindowBase window(mock);
  window.Resize(size);
}

TEST_F(GtkWindowBaseTest, RedrawTest) {
  GtkWrapperMock *mock = GetGtkMock();

  const Size expected_size(15, 25);

  EXPECT_CALL(*mock, GtkWindowGetSize(kDummyWindow, _, _)).
      WillOnce(DoAll(SetArgPointee<1>(expected_size.width),
                     SetArgPointee<2>(expected_size.height)));
  EXPECT_CALL(*mock, GtkWidgetQueueDrawArea(kDummyWindow,
                                            0,
                                            0,
                                            expected_size.width,
                                            expected_size.height));

  GtkWindowBase window(mock);
  window.Redraw();
}
}  // namespace gtk
}  // namespace renderer
}  // namespace mozc
