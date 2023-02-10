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

#include "renderer/unix/gtk_window_base.h"

#include "renderer/unix/gtk_wrapper_mock.h"
#include "testing/gmock.h"
#include "testing/gunit.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrEq;

namespace mozc {
namespace renderer {
namespace gtk {

namespace {
// Following variable is used testing and it is constant but due to API
// restriction, can not modify const modifier.
GtkWidget *kDummyWindow = reinterpret_cast<GtkWidget *>(0x12345678);
GtkWidget *kDummyCanvas = reinterpret_cast<GtkWidget *>(0x87654321);
GdkWindow *kDummyGdkWindow = reinterpret_cast<GdkWindow *>(0x29828374);

MATCHER_P(PointEq, expected_point, "The expected point does not match") {
  return (arg.x == expected_point.x) && (arg.y == expected_point.y);
}

}  // namespace

class GtkWindowBaseTest : public testing::Test {
 protected:
  GtkWrapperMock *GetGtkMock() {
    GtkWrapperMock *mock = new GtkWrapperMock();

    // Following functions are expected called by constructor.
    EXPECT_CALL(*mock, GtkWindowNew(GTK_WINDOW_POPUP))
        .WillOnce(Return(kDummyWindow));
    EXPECT_CALL(*mock, GtkDrawingAreaNew()).WillOnce(Return(kDummyCanvas));
    EXPECT_CALL(*mock,
                GSignalConnect(kDummyWindow, StrEq("destroy"),
                               G_CALLBACK(GtkWindowBase::OnDestroyThunk), _));
    EXPECT_CALL(*mock,
                GSignalConnect(kDummyWindow, StrEq("button-press-event"),
                               G_CALLBACK(GtkWindowBase::OnMouseDownThunk), _));
    EXPECT_CALL(*mock,
                GSignalConnect(kDummyWindow, StrEq("button-release-event"),
                               G_CALLBACK(GtkWindowBase::OnMouseUpThunk), _));
    EXPECT_CALL(*mock,
                GSignalConnect(kDummyCanvas, StrEq("expose-event"),
                               G_CALLBACK(GtkWindowBase::OnPaintThunk), _));
    EXPECT_CALL(*mock, GtkContainerAdd(kDummyWindow, kDummyCanvas));
    EXPECT_CALL(*mock, GtkWidgetAddEvents(kDummyWindow, GDK_BUTTON_PRESS_MASK));
    EXPECT_CALL(*mock,
                GtkWidgetAddEvents(kDummyWindow, GDK_BUTTON_RELEASE_MASK));
    EXPECT_CALL(*mock, GtkWidgetRealize(kDummyWindow));
    EXPECT_CALL(*mock, GdkWindowSetTypeHint(kDummyWindow,
                                            GDK_WINDOW_TYPE_HINT_POPUP_MENU));

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

  EXPECT_EQ(window.GetWindowWidget(), kDummyWindow);
}

TEST_F(GtkWindowBaseTest, GetCanvasWidgetTest) {
  GtkWrapperMock *mock = GetGtkMock();
  GtkWindowBase window(mock);

  EXPECT_EQ(window.GetCanvasWidget(), kDummyCanvas);
}

TEST_F(GtkWindowBaseTest, GetWindowPosTest) {
  GtkWrapperMock *mock = GetGtkMock();

  const Point expected_pos(10, 20);

  EXPECT_CALL(*mock, GtkWindowGetPosition(kDummyWindow, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(expected_pos.x),
                      SetArgPointee<2>(expected_pos.y)));

  GtkWindowBase window(mock);
  Point actual_pos = window.GetWindowPos();
  EXPECT_EQ(actual_pos.x, expected_pos.x);
  EXPECT_EQ(actual_pos.y, expected_pos.y);
}

TEST_F(GtkWindowBaseTest, GetWindowSizeTest) {
  GtkWrapperMock *mock = GetGtkMock();

  const Size expected_size(15, 25);

  EXPECT_CALL(*mock, GtkWindowGetSize(kDummyWindow, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(expected_size.width),
                      SetArgPointee<2>(expected_size.height)));

  GtkWindowBase window(mock);
  Size actual_size = window.GetWindowSize();

  EXPECT_EQ(actual_size.width, expected_size.width);
  EXPECT_EQ(actual_size.height, expected_size.height);
}

TEST_F(GtkWindowBaseTest, GetWindowRectTest) {
  GtkWrapperMock *mock = GetGtkMock();

  const Rect expected_rect(10, 20, 15, 25);

  EXPECT_CALL(*mock, GtkWindowGetPosition(kDummyWindow, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(expected_rect.origin.x),
                      SetArgPointee<2>(expected_rect.origin.y)));

  EXPECT_CALL(*mock, GtkWindowGetSize(kDummyWindow, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(expected_rect.size.width),
                      SetArgPointee<2>(expected_rect.size.height)));

  GtkWindowBase window(mock);
  Rect actual_rect = window.GetWindowRect();

  EXPECT_EQ(actual_rect.origin.x, expected_rect.origin.x);
  EXPECT_EQ(actual_rect.origin.y, expected_rect.origin.y);
  EXPECT_EQ(actual_rect.size.width, expected_rect.size.width);
  EXPECT_EQ(actual_rect.size.height, expected_rect.size.height);
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

  EXPECT_CALL(*mock, GtkWindowGetSize(kDummyWindow, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(expected_size.width),
                      SetArgPointee<2>(expected_size.height)));
  EXPECT_CALL(*mock,
              GtkWidgetQueueDrawArea(kDummyWindow, 0, 0, expected_size.width,
                                     expected_size.height));

  GtkWindowBase window(mock);
  window.Redraw();
}

class OverriddenCallTestableGtkWindowBase : public GtkWindowBase {
 public:
  explicit OverriddenCallTestableGtkWindowBase(GtkWrapperInterface *gtk)
      : GtkWindowBase(gtk) {}
  MOCK_METHOD(void, OnMouseLeftDown, (const Point &pos));
  MOCK_METHOD(void, OnMouseLeftUp, (const Point &pos));
  MOCK_METHOD(void, OnMouseRightDown, (const Point &pos));
  MOCK_METHOD(void, OnMouseRightUp, (const Point &pos));
};

TEST_F(GtkWindowBaseTest, LeftRightTest) {
  const Point expected_pos(10, 15);
  {
    SCOPED_TRACE("Left button is pressed, then call OnMouseLeftDown");
    GtkWrapperMock *mock = GetGtkMock();
    GdkEventButton event;
    event.window = kDummyGdkWindow;
    event.x = static_cast<double>(expected_pos.x);
    event.y = static_cast<double>(expected_pos.y);
    event.state = GDK_BUTTON1_MASK;

    OverriddenCallTestableGtkWindowBase window(mock);
    EXPECT_CALL(window, OnMouseLeftDown(PointEq(expected_pos)));
    EXPECT_TRUE(window.OnMouseDown(kDummyWindow, &event));
  }
  {
    SCOPED_TRACE("Right button is pressed, then call OnMouseRightDown");
    GtkWrapperMock *mock = GetGtkMock();
    GdkEventButton event;
    event.window = kDummyGdkWindow;
    event.x = static_cast<double>(expected_pos.x);
    event.y = static_cast<double>(expected_pos.y);
    event.state = GDK_BUTTON3_MASK;

    OverriddenCallTestableGtkWindowBase window(mock);
    EXPECT_CALL(window, OnMouseRightDown(PointEq(expected_pos)));
    EXPECT_TRUE(window.OnMouseDown(kDummyWindow, &event));
  }
  {
    SCOPED_TRACE("Left button is released, then call OnMouseLeftUp");
    GtkWrapperMock *mock = GetGtkMock();
    GdkEventButton event;
    event.window = kDummyGdkWindow;
    event.x = static_cast<double>(expected_pos.x);
    event.y = static_cast<double>(expected_pos.y);
    event.state = GDK_BUTTON1_MASK;

    OverriddenCallTestableGtkWindowBase window(mock);
    EXPECT_CALL(window, OnMouseLeftUp(PointEq(expected_pos)));
    EXPECT_TRUE(window.OnMouseUp(kDummyWindow, &event));
  }
  {
    SCOPED_TRACE("Right button is released, then call OnMouseRightUp");
    GtkWrapperMock *mock = GetGtkMock();
    GdkEventButton event;
    event.window = kDummyGdkWindow;
    event.x = static_cast<double>(expected_pos.x);
    event.y = static_cast<double>(expected_pos.y);
    event.state = GDK_BUTTON3_MASK;

    OverriddenCallTestableGtkWindowBase window(mock);
    EXPECT_CALL(window, OnMouseRightUp(PointEq(expected_pos)));
    EXPECT_TRUE(window.OnMouseUp(kDummyWindow, &event));
  }
  {
    SCOPED_TRACE("Other button is ignored and return 0");
    // The state mask is up to 2^12, so try all state.
    for (size_t flags = 0; flags < (1 << 13); ++flags) {
      GtkWrapperMock *mock = GetGtkMock();
      GdkEventButton event;
      event.window = kDummyGdkWindow;
      event.x = static_cast<double>(expected_pos.x);
      event.y = static_cast<double>(expected_pos.y);
      event.state = flags & ~(GDK_BUTTON1_MASK | GDK_BUTTON3_MASK);

      OverriddenCallTestableGtkWindowBase window(mock);
      EXPECT_CALL(window, OnMouseLeftDown(_)).Times(0);
      EXPECT_CALL(window, OnMouseLeftUp(_)).Times(0);
      EXPECT_CALL(window, OnMouseRightDown(_)).Times(0);
      EXPECT_CALL(window, OnMouseRightUp(_)).Times(0);

      EXPECT_TRUE(window.OnMouseUp(kDummyWindow, &event));
    }
  }
}
}  // namespace gtk
}  // namespace renderer
}  // namespace mozc
