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

#include "renderer/unix/candidate_window.h"

#include <sstream>
#include <string>

#include "base/logging.h"
#include "base/util.h"
#include "client/client_interface.h"
#include "renderer/table_layout_mock.h"
#include "renderer/unix/cairo_factory_mock.h"
#include "renderer/unix/cairo_wrapper_mock.h"
#include "renderer/unix/const.h"
#include "renderer/unix/draw_tool_mock.h"
#include "renderer/unix/gtk_wrapper_mock.h"
#include "renderer/unix/text_renderer_mock.h"
#include "testing/base/public/gmock.h"
#include "testing/base/public/gunit.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Expectation;
using ::testing::Ne;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrEq;
using ::testing::StrictMock;

namespace mozc {
namespace renderer {
namespace gtk {

namespace {
// Following variable is used testing and it is constant but due to API
// restriction, can not modify const modifier.
GtkWidget *kDummyWindow = reinterpret_cast<GtkWidget *>(0x12345678);
GtkWidget *kDummyCanvas = reinterpret_cast<GtkWidget *>(0x87654321);

constexpr char kSampleValue[] = "VALUE";
constexpr char kSampleShortcut[] = "SHORTCUT";
constexpr char kSampleDescription[] = "DESCRIPTION";
constexpr char kSamplePrefix[] = "PREFIX";
constexpr char kSampleSuffix[] = "SUFFIX";

MATCHER_P(PointEq, expected_point, "The expected point does not match") {
  return (arg.x == expected_point.x) && (arg.y == expected_point.y);
}

MATCHER_P(SizeEq, expected_size, "The expected size does not match") {
  return (arg.width == expected_size.width) &&
         (arg.height == expected_size.height);
}

MATCHER_P(RectEq, expected_rect, "The expected rect does not match") {
  return (arg.origin.x == expected_rect.origin.x) &&
         (arg.origin.y == expected_rect.origin.y) &&
         (arg.size.width == expected_rect.size.width) &&
         (arg.size.height == expected_rect.size.height);
}

MATCHER_P(RGBAEq, expected_rgba, "The expected RGBA does not match") {
  return (arg.red == expected_rgba.red) && (arg.green == expected_rgba.green) &&
         (arg.blue == expected_rgba.blue) && (arg.alpha == expected_rgba.alpha);
}

void SetTestCandidates(uint32 count, bool has_value, bool has_shortcut,
                       bool has_description, bool has_prefix, bool has_suffix,
                       commands::Candidates *candidates) {
  candidates->Clear();
  candidates->set_size(count);

  for (uint32 i = 0; i < count; ++i) {
    commands::Candidates_Candidate *candidate = candidates->add_candidate();
    candidate->set_index(i);
    candidate->set_id(i * 0x10);

    const std::string id_str = std::to_string(i);

    if (has_value) {
      candidate->set_value(kSampleValue + id_str);
    }

    commands::Annotation *annotation = candidate->mutable_annotation();

    if (has_shortcut) {
      annotation->set_shortcut(kSampleShortcut + id_str);
    }

    if (has_description) {
      annotation->set_description(kSampleDescription + id_str);
    }

    if (has_prefix) {
      annotation->set_prefix(kSamplePrefix + id_str);
    }

    if (has_suffix) {
      annotation->set_suffix(kSampleSuffix + id_str);
    }
  }
}

std::string GetExpectedValue(int index, bool has_prefix, bool has_suffix) {
  std::string result;

  const std::string id_str = std::to_string(index);
  const std::string expected_prefix = kSamplePrefix + id_str;
  const std::string expected_value = kSampleValue + id_str;
  const std::string expected_suffix = kSampleSuffix + id_str;
  if (has_prefix) {
    result.append(expected_prefix);
  }
  result.append(expected_value);
  if (has_suffix) {
    result.append(expected_suffix);
  }
  return result;
}

std::string GetExpectedShortcut(int index) {
  const std::string id_str = std::to_string(index);
  return kSampleShortcut + id_str;
}

std::string GetExpectedDescription(int index) {
  const std::string id_str = std::to_string(index);
  return kSampleDescription + id_str;
}

class SendCommandInterfaceMock : public client::SendCommandInterface {
 public:
  MOCK_METHOD(bool, SendCommand,
              (const commands::SessionCommand &command,
               commands::Output *output),
              (override));
};

MATCHER_P(SelectCommandEq, id, "") {
  if (!arg.has_type()) {
    *result_listener << "type does not exist.";
    return false;
  }

  if (!arg.has_id()) {
    *result_listener << "id does not exist.";
    return false;
  }

  if (arg.type() != commands::SessionCommand::SELECT_CANDIDATE) {
    *result_listener << "type does not match\n"
                     << "  expected: SessionCommand::SELECT_CANDIDATE\n"
                     << "  actual  : " << arg.type();
    return false;
  }

  if (arg.id() != id) {
    *result_listener << "id does not match\n"
                     << "  expected: " << id << "\n"
                     << "  actual  :" << arg.id();
    return false;
  }
  return true;
}

class MouseHandlingTestableCandidateWindow : public CandidateWindow {
 public:
  MouseHandlingTestableCandidateWindow(TableLayoutInterface *table_layout,
                                       TextRendererInterface *text_renderer,
                                       DrawToolInterface *draw_tool,
                                       GtkWrapperInterface *gtk,
                                       CairoFactoryInterface *cairo_factory)
      : CandidateWindow(table_layout, text_renderer, draw_tool, gtk,
                        cairo_factory) {}
  virtual ~MouseHandlingTestableCandidateWindow() {}

  MOCK_METHOD(int, GetSelectedRowIndex, (const Point &pos), (const));
  using CandidateWindow::OnMouseLeftUp;
};

}  // namespace

class CandidateWindowTest : public testing::Test {
 protected:
  template <class WindowType>
  struct GenericCandidateWindowTestKit {
    GtkWrapperMock *gtk_mock;
    TableLayoutMock *table_layout_mock;
    TextRendererMock *text_renderer_mock;
    DrawToolMock *draw_tool_mock;
    CairoFactoryMock *cairo_factory_mock;
    WindowType *window;
  };

  typedef GenericCandidateWindowTestKit<CandidateWindow> CandidateWindowTestKit;
  typedef GenericCandidateWindowTestKit<MouseHandlingTestableCandidateWindow>
      MouseHandlingTestableCandidateWindowTestKit;

  template <class WindowType>
  static void FinalizeTestKit(
      GenericCandidateWindowTestKit<WindowType> *testkit) {
    delete testkit->window;
  }

  static CandidateWindowTestKit SetUpCandidateWindow() {
    return SetUpTestKit<CandidateWindow>(kUseVanillaMock);
  }

  static CandidateWindowTestKit SetUpCandidateWindowWithStrictMock() {
    return SetUpTestKit<CandidateWindow>(kUseStrictMock);
  }

  static MouseHandlingTestableCandidateWindowTestKit
  SetUpMouseHandlingTestableCandidateWindow() {
    return SetUpTestKit<MouseHandlingTestableCandidateWindow>(kUseVanillaMock);
  }

  static MouseHandlingTestableCandidateWindowTestKit
  SetUpMouseHandlingTestableCandidateWindowWithStrictMock() {
    return SetUpTestKit<MouseHandlingTestableCandidateWindow>(kUseStrictMock);
  }

 private:
  enum MockWrapperType {
    kUseVanillaMock,
    kUseStrictMock,
    kUseNiceMock,
  };

  template <class WindowType>
  static void SetUpCandidateWindowConstractorCallExpectations(
      GenericCandidateWindowTestKit<WindowType> *testkit) {
    // Following functions are expected to be called by constructor.
    EXPECT_CALL(*testkit->gtk_mock, GtkWindowNew(GTK_WINDOW_POPUP))
        .WillOnce(Return(kDummyWindow));
    EXPECT_CALL(*testkit->gtk_mock, GtkDrawingAreaNew())
        .WillOnce(Return(kDummyCanvas));
    EXPECT_CALL(*testkit->gtk_mock,
                GSignalConnect(kDummyWindow, StrEq("destroy"),
                               G_CALLBACK(GtkWindowBase::OnDestroyThunk), _));
    EXPECT_CALL(*testkit->gtk_mock,
                GSignalConnect(kDummyWindow, StrEq("button-press-event"),
                               G_CALLBACK(GtkWindowBase::OnMouseDownThunk), _));
    EXPECT_CALL(*testkit->gtk_mock,
                GSignalConnect(kDummyWindow, StrEq("button-release-event"),
                               G_CALLBACK(GtkWindowBase::OnMouseUpThunk), _));
    EXPECT_CALL(*testkit->gtk_mock,
                GSignalConnect(kDummyCanvas, StrEq("expose-event"),
                               G_CALLBACK(GtkWindowBase::OnPaintThunk), _));
    EXPECT_CALL(*testkit->gtk_mock,
                GtkContainerAdd(kDummyWindow, kDummyCanvas));
    EXPECT_CALL(*testkit->gtk_mock,
                GtkWidgetAddEvents(kDummyWindow, GDK_BUTTON_PRESS_MASK));
    EXPECT_CALL(*testkit->gtk_mock,
                GtkWidgetAddEvents(kDummyWindow, GDK_BUTTON_RELEASE_MASK));
    EXPECT_CALL(*testkit->gtk_mock, GtkWidgetRealize(kDummyWindow));
    EXPECT_CALL(
        *testkit->gtk_mock,
        GdkWindowSetTypeHint(kDummyWindow, GDK_WINDOW_TYPE_HINT_POPUP_MENU));
  }

  template <class TargetMockType>
  static TargetMockType *CreateMock(MockWrapperType mock_wrapper_type) {
    switch (mock_wrapper_type) {
      case kUseStrictMock:
        return new StrictMock<TargetMockType>();
      case kUseNiceMock:
        return new NiceMock<TargetMockType>();
      case kUseVanillaMock:
      default:
        return new TargetMockType();
    }
  }

  template <class WindowType>
  static GenericCandidateWindowTestKit<WindowType> SetUpTestKit(
      MockWrapperType mock_wrapper_type) {
    GenericCandidateWindowTestKit<WindowType> testkit;
    testkit.gtk_mock = CreateMock<GtkWrapperMock>(mock_wrapper_type);
    testkit.table_layout_mock = CreateMock<TableLayoutMock>(mock_wrapper_type);
    testkit.text_renderer_mock =
        CreateMock<TextRendererMock>(mock_wrapper_type);
    testkit.draw_tool_mock = CreateMock<DrawToolMock>(mock_wrapper_type);
    testkit.cairo_factory_mock =
        CreateMock<CairoFactoryMock>(mock_wrapper_type);

    SetUpCandidateWindowConstractorCallExpectations(&testkit);

    testkit.window = new WindowType(
        testkit.table_layout_mock, testkit.text_renderer_mock,
        testkit.draw_tool_mock, testkit.gtk_mock, testkit.cairo_factory_mock);
    return testkit;
  }
};

TEST_F(CandidateWindowTest, DrawBackgroundTest) {
  CandidateWindowTestKit testkit = SetUpCandidateWindow();

  const Size assumed_size(15, 25);
  EXPECT_CALL(*testkit.gtk_mock, GtkWindowGetSize(kDummyWindow, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(assumed_size.width),
                      SetArgPointee<2>(assumed_size.height)));

  const Rect expect_rendering_area(Point(0, 0), assumed_size);
  EXPECT_CALL(
      *testkit.draw_tool_mock,
      FillRect(RectEq(expect_rendering_area), RGBAEq(kDefaultBackgroundColor)));
  testkit.window->DrawBackground();
  FinalizeTestKit(&testkit);
}

TEST_F(CandidateWindowTest, DrawShortcutBackgroundTest) {
  LOG(INFO) << __FUNCTION__ << ":" << __LINE__;
  {
    // Instantiate strict mock to detect unintended call.
    SCOPED_TRACE("Empty column test, expected to do nothing.");
    CandidateWindowTestKit testkit = SetUpCandidateWindowWithStrictMock();

    EXPECT_CALL(*testkit.table_layout_mock, number_of_columns())
        .WillOnce(Return(0));

    testkit.window->DrawShortcutBackground();
    FinalizeTestKit(&testkit);
  }
  return;
  {
    // Instantiate strict mock to detect unintended call.
    SCOPED_TRACE("GetColumnRect returns empty rectangle.");
    CandidateWindowTestKit testkit = SetUpCandidateWindowWithStrictMock();

    EXPECT_CALL(*testkit.table_layout_mock, number_of_columns())
        .WillOnce(Return(1));

    const Rect empty_rect(0, 0, 0, 0);
    const Rect non_empty_rect(1, 2, 3, 4);
    EXPECT_CALL(*testkit.table_layout_mock, GetColumnRect(0))
        .WillOnce(Return(empty_rect));
    EXPECT_CALL(*testkit.table_layout_mock, GetRowRect(0))
        .WillOnce(Return(non_empty_rect));

    testkit.window->DrawShortcutBackground();
    FinalizeTestKit(&testkit);
  }
  {
    // Instantiate strict mock to detect unintended call.
    SCOPED_TRACE("GetRowRect returns empty rectangle.");
    CandidateWindowTestKit testkit = SetUpCandidateWindowWithStrictMock();

    EXPECT_CALL(*testkit.table_layout_mock, number_of_columns())
        .WillOnce(Return(1));

    const Rect empty_rect(0, 0, 0, 0);
    const Rect non_empty_rect(1, 2, 3, 4);
    EXPECT_CALL(*testkit.table_layout_mock, GetColumnRect(0))
        .WillOnce(Return(non_empty_rect));
    EXPECT_CALL(*testkit.table_layout_mock, GetRowRect(0))
        .WillOnce(Return(empty_rect));

    testkit.window->DrawShortcutBackground();
    FinalizeTestKit(&testkit);
  }
  {
    // Instantiate strict mock to detect unintended call.
    SCOPED_TRACE("Both GetColumnRect and GetRowRect return empty rectangle.");
    CandidateWindowTestKit testkit = SetUpCandidateWindowWithStrictMock();

    EXPECT_CALL(*testkit.table_layout_mock, number_of_columns())
        .WillOnce(Return(1));

    const Rect empty_rect(0, 0, 0, 0);
    EXPECT_CALL(*testkit.table_layout_mock, GetColumnRect(0))
        .WillOnce(Return(empty_rect));
    EXPECT_CALL(*testkit.table_layout_mock, GetRowRect(0))
        .WillOnce(Return(empty_rect));

    testkit.window->DrawShortcutBackground();
    FinalizeTestKit(&testkit);
  }
  {
    CandidateWindowTestKit testkit = SetUpCandidateWindow();

    EXPECT_CALL(*testkit.table_layout_mock, number_of_columns())
        .WillOnce(Return(3));

    const Rect first_column_rect(10, 20, 30, 40);
    const Rect first_row_rect(15, 25, 35, 45);
    EXPECT_CALL(*testkit.table_layout_mock, GetColumnRect(0))
        .WillOnce(Return(first_column_rect));
    EXPECT_CALL(*testkit.table_layout_mock, GetRowRect(0))
        .WillOnce(Return(first_row_rect));

    const Rect rendering_target(first_row_rect.origin, first_column_rect.size);

    EXPECT_CALL(
        *testkit.draw_tool_mock,
        FillRect(RectEq(rendering_target), RGBAEq(kShortcutBackgroundColor)));

    testkit.window->DrawShortcutBackground();
    FinalizeTestKit(&testkit);
  }
}
TEST_F(CandidateWindowTest, DrawSelectedRectTest) {
  {
    SCOPED_TRACE("Candidates has no focused index.");
    CandidateWindowTestKit testkit = SetUpCandidateWindowWithStrictMock();
    testkit.window->CandidateWindow::DrawSelectedRect();
    FinalizeTestKit(&testkit);
  }
  {
    CandidateWindowTestKit testkit = SetUpCandidateWindow();

    const int assume_focused_id = 3;
    const Rect rendering_area(10, 20, 30, 40);
    EXPECT_CALL(*testkit.table_layout_mock, GetRowRect(assume_focused_id))
        .WillOnce(Return(rendering_area));
    EXPECT_CALL(
        *testkit.draw_tool_mock,
        FillRect(RectEq(rendering_area), RGBAEq(kSelectedRowBackgroundColor)));
    EXPECT_CALL(
        *testkit.draw_tool_mock,
        FrameRect(RectEq(rendering_area), RGBAEq(kSelectedRowFrameColor), 1.0));
    SetTestCandidates(10, true, true, true, true, true,
                      &testkit.window->candidates_);
    testkit.window->candidates_.set_focused_index(assume_focused_id);
    testkit.window->CandidateWindow::DrawSelectedRect();
    FinalizeTestKit(&testkit);
  }
}

TEST_F(CandidateWindowTest, GetDisplayStringTest) {
  std::string expected_prefixed_value = kSamplePrefix;
  expected_prefixed_value.append(kSampleValue);
  std::string expected_suffixed_value = kSampleValue;
  expected_suffixed_value.append(kSampleSuffix);
  std::string expected_presuffixed_value = kSamplePrefix;
  expected_presuffixed_value.append(kSampleValue);
  expected_presuffixed_value.append(kSampleSuffix);

  {
    SCOPED_TRACE("Candidate does not have value.");
    commands::Candidates::Candidate candidate;

    std::string value, shortcut, description;
    CandidateWindow::GetDisplayString(candidate, &shortcut, &value,
                                      &description);

    EXPECT_TRUE(shortcut.empty());
    EXPECT_TRUE(value.empty());
    EXPECT_TRUE(description.empty());
  }
  {
    SCOPED_TRACE("Candidate has no annotation.");

    commands::Candidates::Candidate candidate;
    candidate.set_value(kSampleValue);

    std::string value, shortcut, description;
    CandidateWindow::GetDisplayString(candidate, &shortcut, &value,
                                      &description);

    EXPECT_TRUE(shortcut.empty());
    EXPECT_EQ(kSampleValue, value);
    EXPECT_TRUE(description.empty());
  }
  {
    SCOPED_TRACE("Annotation has shortcut.");

    commands::Candidates::Candidate candidate;
    commands::Annotation *annotation = candidate.mutable_annotation();
    candidate.set_value(kSampleValue);
    annotation->set_shortcut(kSampleShortcut);

    std::string value, shortcut, description;
    CandidateWindow::GetDisplayString(candidate, &shortcut, &value,
                                      &description);

    EXPECT_EQ(kSampleShortcut, shortcut);
    EXPECT_EQ(kSampleValue, value);
    EXPECT_TRUE(description.empty());
  }
  {
    SCOPED_TRACE("Annotation has prefix");
    commands::Candidates::Candidate candidate;
    commands::Annotation *annotation = candidate.mutable_annotation();
    candidate.set_value(kSampleValue);
    annotation->set_prefix(kSamplePrefix);

    std::string value, shortcut, description;
    CandidateWindow::GetDisplayString(candidate, &shortcut, &value,
                                      &description);

    EXPECT_TRUE(shortcut.empty());
    EXPECT_EQ(expected_prefixed_value, value);
    EXPECT_TRUE(description.empty());
  }
  {
    SCOPED_TRACE("Annotation has suffix");
    commands::Candidates::Candidate candidate;
    commands::Annotation *annotation = candidate.mutable_annotation();
    candidate.set_value(kSampleValue);
    annotation->set_suffix(kSampleSuffix);

    std::string value, shortcut, description;
    CandidateWindow::GetDisplayString(candidate, &shortcut, &value,
                                      &description);

    EXPECT_TRUE(shortcut.empty());
    EXPECT_EQ(expected_suffixed_value, value);
    EXPECT_TRUE(description.empty());
  }
  {
    SCOPED_TRACE("Annotation has both prefix and suffix");
    commands::Candidates::Candidate candidate;
    commands::Annotation *annotation = candidate.mutable_annotation();
    candidate.set_value(kSampleValue);
    annotation->set_prefix(kSamplePrefix);
    annotation->set_suffix(kSampleSuffix);

    std::string value, shortcut, description;
    CandidateWindow::GetDisplayString(candidate, &shortcut, &value,
                                      &description);

    EXPECT_TRUE(shortcut.empty());
    EXPECT_EQ(expected_presuffixed_value, value);
    EXPECT_TRUE(description.empty());
  }
}

TEST_F(CandidateWindowTest, DrawCellsTest) {
  {
    SCOPED_TRACE("Empty candidates does nothing.");
    CandidateWindowTestKit testkit = SetUpCandidateWindowWithStrictMock();
    testkit.window->DrawCells();
    FinalizeTestKit(&testkit);
  }
  {
    SCOPED_TRACE("Value shortcut description without prefix suffix case");
    CandidateWindowTestKit testkit = SetUpCandidateWindow();

    for (int i = 0; i < 10; ++i) {
      const Rect value_render_area(i * 2, i * 3, i * 4, i * 5);
      Expectation get_value_cell_rect =
          EXPECT_CALL(*testkit.table_layout_mock,
                      GetCellRect(i, CandidateWindow::COLUMN_CANDIDATE))
              .WillOnce(Return(value_render_area));
      EXPECT_CALL(*testkit.text_renderer_mock,
                  RenderText(GetExpectedValue(i, false, false),
                             RectEq(value_render_area),
                             FontSpecInterface::FONTSET_CANDIDATE))
          .After(get_value_cell_rect);

      const Rect shortcut_render_area(i * 3, i * 4, i * 5, i * 6);
      Expectation get_shortcut_cell_rect =
          EXPECT_CALL(*testkit.table_layout_mock,
                      GetCellRect(i, CandidateWindow::COLUMN_SHORTCUT))
              .WillOnce(Return(shortcut_render_area));
      EXPECT_CALL(
          *testkit.text_renderer_mock,
          RenderText(GetExpectedShortcut(i), RectEq(shortcut_render_area),
                     FontSpecInterface::FONTSET_SHORTCUT))
          .After(get_shortcut_cell_rect);

      const Rect desc_render_area(i * 4, i * 5, i * 6, i * 7);
      Expectation get_desc_cell_rect =
          EXPECT_CALL(*testkit.table_layout_mock,
                      GetCellRect(i, CandidateWindow::COLUMN_DESCRIPTION))
              .WillOnce(Return(desc_render_area));
      EXPECT_CALL(
          *testkit.text_renderer_mock,
          RenderText(GetExpectedDescription(i), RectEq(desc_render_area),
                     FontSpecInterface::FONTSET_DESCRIPTION))
          .After(get_desc_cell_rect);
    }

    SetTestCandidates(10, true, true, true, false, false,
                      &testkit.window->candidates_);

    testkit.window->DrawCells();
    FinalizeTestKit(&testkit);
  }
  {
    SCOPED_TRACE("Value shortcut description with prefix case");
    CandidateWindowTestKit testkit = SetUpCandidateWindow();

    for (int i = 0; i < 10; ++i) {
      const Rect value_render_area(i * 2, i * 3, i * 4, i * 5);
      Expectation get_value_cell_rect =
          EXPECT_CALL(*testkit.table_layout_mock,
                      GetCellRect(i, CandidateWindow::COLUMN_CANDIDATE))
              .WillOnce(Return(value_render_area));
      EXPECT_CALL(*testkit.text_renderer_mock,
                  RenderText(GetExpectedValue(i, true, false),
                             RectEq(value_render_area),
                             FontSpecInterface::FONTSET_CANDIDATE))
          .After(get_value_cell_rect);

      const Rect shortcut_render_area(i * 3, i * 4, i * 5, i * 6);
      Expectation get_shortcut_cell_rect =
          EXPECT_CALL(*testkit.table_layout_mock,
                      GetCellRect(i, CandidateWindow::COLUMN_SHORTCUT))
              .WillOnce(Return(shortcut_render_area));
      EXPECT_CALL(
          *testkit.text_renderer_mock,
          RenderText(GetExpectedShortcut(i), RectEq(shortcut_render_area),
                     FontSpecInterface::FONTSET_SHORTCUT))
          .After(get_shortcut_cell_rect);

      const Rect desc_render_area(i * 4, i * 5, i * 6, i * 7);
      Expectation get_desc_cell_rect =
          EXPECT_CALL(*testkit.table_layout_mock,
                      GetCellRect(i, CandidateWindow::COLUMN_DESCRIPTION))
              .WillOnce(Return(desc_render_area));
      EXPECT_CALL(
          *testkit.text_renderer_mock,
          RenderText(GetExpectedDescription(i), RectEq(desc_render_area),
                     FontSpecInterface::FONTSET_DESCRIPTION))
          .After(get_desc_cell_rect);
    }

    SetTestCandidates(10, true, true, true, true, false,
                      &testkit.window->candidates_);

    testkit.window->DrawCells();
    FinalizeTestKit(&testkit);
  }
  {
    SCOPED_TRACE("Value shortcut description with suffix case");
    CandidateWindowTestKit testkit = SetUpCandidateWindow();

    for (int i = 0; i < 10; ++i) {
      const Rect value_render_area(i * 2, i * 3, i * 4, i * 5);
      Expectation get_value_cell_rect =
          EXPECT_CALL(*testkit.table_layout_mock,
                      GetCellRect(i, CandidateWindow::COLUMN_CANDIDATE))
              .WillOnce(Return(value_render_area));
      EXPECT_CALL(*testkit.text_renderer_mock,
                  RenderText(GetExpectedValue(i, false, true),
                             RectEq(value_render_area),
                             FontSpecInterface::FONTSET_CANDIDATE))
          .After(get_value_cell_rect);

      const Rect shortcut_render_area(i * 3, i * 4, i * 5, i * 6);
      Expectation get_shortcut_cell_rect =
          EXPECT_CALL(*testkit.table_layout_mock,
                      GetCellRect(i, CandidateWindow::COLUMN_SHORTCUT))
              .WillOnce(Return(shortcut_render_area));
      EXPECT_CALL(
          *testkit.text_renderer_mock,
          RenderText(GetExpectedShortcut(i), RectEq(shortcut_render_area),
                     FontSpecInterface::FONTSET_SHORTCUT))
          .After(get_shortcut_cell_rect);

      const Rect desc_render_area(i * 4, i * 5, i * 6, i * 7);
      Expectation get_desc_cell_rect =
          EXPECT_CALL(*testkit.table_layout_mock,
                      GetCellRect(i, CandidateWindow::COLUMN_DESCRIPTION))
              .WillOnce(Return(desc_render_area));
      EXPECT_CALL(
          *testkit.text_renderer_mock,
          RenderText(GetExpectedDescription(i), RectEq(desc_render_area),
                     FontSpecInterface::FONTSET_DESCRIPTION))
          .After(get_desc_cell_rect);
    }

    SetTestCandidates(10, true, true, true, false, true,
                      &testkit.window->candidates_);

    testkit.window->DrawCells();
    FinalizeTestKit(&testkit);
  }
  {
    SCOPED_TRACE("Value shortcut description both prefix and suffix case");
    CandidateWindowTestKit testkit = SetUpCandidateWindow();

    for (int i = 0; i < 10; ++i) {
      const Rect value_render_area(i * 2, i * 3, i * 4, i * 5);
      Expectation get_value_cell_rect =
          EXPECT_CALL(*testkit.table_layout_mock,
                      GetCellRect(i, CandidateWindow::COLUMN_CANDIDATE))
              .WillOnce(Return(value_render_area));
      EXPECT_CALL(
          *testkit.text_renderer_mock,
          RenderText(GetExpectedValue(i, true, true), RectEq(value_render_area),
                     FontSpecInterface::FONTSET_CANDIDATE))
          .After(get_value_cell_rect);

      const Rect shortcut_render_area(i * 3, i * 4, i * 5, i * 6);
      Expectation get_shortcut_cell_rect =
          EXPECT_CALL(*testkit.table_layout_mock,
                      GetCellRect(i, CandidateWindow::COLUMN_SHORTCUT))
              .WillOnce(Return(shortcut_render_area));
      EXPECT_CALL(
          *testkit.text_renderer_mock,
          RenderText(GetExpectedShortcut(i), RectEq(shortcut_render_area),
                     FontSpecInterface::FONTSET_SHORTCUT))
          .After(get_shortcut_cell_rect);

      const Rect desc_render_area(i * 4, i * 5, i * 6, i * 7);
      Expectation get_desc_cell_rect =
          EXPECT_CALL(*testkit.table_layout_mock,
                      GetCellRect(i, CandidateWindow::COLUMN_DESCRIPTION))
              .WillOnce(Return(desc_render_area));
      EXPECT_CALL(
          *testkit.text_renderer_mock,
          RenderText(GetExpectedDescription(i), RectEq(desc_render_area),
                     FontSpecInterface::FONTSET_DESCRIPTION))
          .After(get_desc_cell_rect);
    }

    SetTestCandidates(10, true, true, true, true, true,
                      &testkit.window->candidates_);

    testkit.window->DrawCells();
    FinalizeTestKit(&testkit);
  }
  {
    SCOPED_TRACE("Only value with prefix suffix case");
    CandidateWindowTestKit testkit = SetUpCandidateWindow();

    for (int i = 0; i < 10; ++i) {
      const Rect value_render_area(i * 2, i * 3, i * 4, i * 5);
      Expectation get_value_cell_rect =
          EXPECT_CALL(*testkit.table_layout_mock,
                      GetCellRect(i, CandidateWindow::COLUMN_CANDIDATE))
              .WillOnce(Return(value_render_area));
      EXPECT_CALL(
          *testkit.text_renderer_mock,
          RenderText(GetExpectedValue(i, true, true), RectEq(value_render_area),
                     FontSpecInterface::FONTSET_CANDIDATE))
          .After(get_value_cell_rect);
    }

    // Expects never call shortcut and description rendering functions.
    EXPECT_CALL(*testkit.table_layout_mock,
                GetCellRect(_, CandidateWindow::COLUMN_SHORTCUT))
        .Times(0);
    EXPECT_CALL(*testkit.text_renderer_mock,
                RenderText(_, _, FontSpecInterface::FONTSET_SHORTCUT))
        .Times(0);

    EXPECT_CALL(*testkit.table_layout_mock,
                GetCellRect(_, CandidateWindow::COLUMN_DESCRIPTION))
        .Times(0);
    EXPECT_CALL(*testkit.text_renderer_mock,
                RenderText(_, _, FontSpecInterface::FONTSET_DESCRIPTION))
        .Times(0);

    SetTestCandidates(10, true, false, false, true, true,
                      &testkit.window->candidates_);

    testkit.window->DrawCells();
    FinalizeTestKit(&testkit);
  }
  {
    SCOPED_TRACE("Value shortcut but not description case");
    CandidateWindowTestKit testkit = SetUpCandidateWindow();

    for (int i = 0; i < 10; ++i) {
      const Rect value_render_area(i * 2, i * 3, i * 4, i * 5);
      Expectation get_value_cell_rect =
          EXPECT_CALL(*testkit.table_layout_mock,
                      GetCellRect(i, CandidateWindow::COLUMN_CANDIDATE))
              .WillOnce(Return(value_render_area));
      EXPECT_CALL(
          *testkit.text_renderer_mock,
          RenderText(GetExpectedValue(i, true, true), RectEq(value_render_area),
                     FontSpecInterface::FONTSET_CANDIDATE))
          .After(get_value_cell_rect);

      const Rect shortcut_render_area(i * 3, i * 4, i * 5, i * 6);
      Expectation get_shortcut_cell_rect =
          EXPECT_CALL(*testkit.table_layout_mock,
                      GetCellRect(i, CandidateWindow::COLUMN_SHORTCUT))
              .WillOnce(Return(shortcut_render_area));
      EXPECT_CALL(
          *testkit.text_renderer_mock,
          RenderText(GetExpectedShortcut(i), RectEq(shortcut_render_area),
                     FontSpecInterface::FONTSET_SHORTCUT))
          .After(get_shortcut_cell_rect);
    }

    EXPECT_CALL(*testkit.table_layout_mock,
                GetCellRect(_, CandidateWindow::COLUMN_DESCRIPTION))
        .Times(0);
    EXPECT_CALL(*testkit.text_renderer_mock,
                RenderText(_, _, FontSpecInterface::FONTSET_DESCRIPTION))
        .Times(0);

    SetTestCandidates(10, true, true, false, true, true,
                      &testkit.window->candidates_);

    testkit.window->DrawCells();
    FinalizeTestKit(&testkit);
  }
  {
    SCOPED_TRACE("Value description but not shortcut case");
    CandidateWindowTestKit testkit = SetUpCandidateWindow();

    for (int i = 0; i < 10; ++i) {
      const Rect value_render_area(i * 2, i * 3, i * 4, i * 5);
      Expectation get_value_cell_rect =
          EXPECT_CALL(*testkit.table_layout_mock,
                      GetCellRect(i, CandidateWindow::COLUMN_CANDIDATE))
              .WillOnce(Return(value_render_area));
      EXPECT_CALL(
          *testkit.text_renderer_mock,
          RenderText(GetExpectedValue(i, true, true), RectEq(value_render_area),
                     FontSpecInterface::FONTSET_CANDIDATE))
          .After(get_value_cell_rect);

      const Rect desc_render_area(i * 4, i * 5, i * 6, i * 7);
      Expectation get_desc_cell_rect =
          EXPECT_CALL(*testkit.table_layout_mock,
                      GetCellRect(i, CandidateWindow::COLUMN_DESCRIPTION))
              .WillOnce(Return(desc_render_area));
      EXPECT_CALL(
          *testkit.text_renderer_mock,
          RenderText(GetExpectedDescription(i), RectEq(desc_render_area),
                     FontSpecInterface::FONTSET_DESCRIPTION))
          .After(get_desc_cell_rect);
    }

    EXPECT_CALL(*testkit.table_layout_mock,
                GetCellRect(_, CandidateWindow::COLUMN_SHORTCUT))
        .Times(0);
    EXPECT_CALL(*testkit.text_renderer_mock,
                RenderText(_, _, FontSpecInterface::FONTSET_SHORTCUT))
        .Times(0);

    SetTestCandidates(10, true, false, true, true, true,
                      &testkit.window->candidates_);

    testkit.window->DrawCells();
    FinalizeTestKit(&testkit);
  }
}
TEST_F(CandidateWindowTest, DrawInformationIconTest) {
  CandidateWindowTestKit testkit = SetUpCandidateWindow();

  commands::Candidates candidates;

  for (int i = 0; i < 10; ++i) {
    commands::Candidates_Candidate *candidate = candidates.add_candidate();
    candidate->set_index(i);
    candidate->set_id(i * 0x10);
    if (Util::Random(2) == 0) {
      candidate->set_information_id(i * 0x20);
      const Rect row_rect(i * 10, i * 20, i * 30, i * 40);
      const Rect expected_icon_rect(row_rect.origin.x + row_rect.size.width - 6,
                                    row_rect.origin.y + 2, 4,
                                    row_rect.size.height - 4);
      Expectation get_row_rect =
          EXPECT_CALL(*testkit.table_layout_mock, GetRowRect(i))
              .WillOnce(Return(row_rect));
      Expectation fill_rect = EXPECT_CALL(*testkit.draw_tool_mock,
                                          FillRect(RectEq(expected_icon_rect),
                                                   RGBAEq(kIndicatorColor)))
                                  .After(get_row_rect);
      EXPECT_CALL(
          *testkit.draw_tool_mock,
          FrameRect(RectEq(expected_icon_rect), RGBAEq(kIndicatorColor), 1))
          .After(fill_rect);
    } else {
      EXPECT_CALL(*testkit.table_layout_mock, GetRowRect(i)).Times(0);
    }
  }

  testkit.window->candidates_ = candidates;
  testkit.window->DrawInformationIcon();
  FinalizeTestKit(&testkit);
}

TEST_F(CandidateWindowTest, DrawFooterTest) {
  {
    SCOPED_TRACE("Empty footer test, expected to do nothing.");
    CandidateWindowTestKit testkit = SetUpCandidateWindowWithStrictMock();
    testkit.window->DrawFooter();
    FinalizeTestKit(&testkit);
  }
  {
    SCOPED_TRACE("Empty footer rect test, expected to do nothing.");
    CandidateWindowTestKit testkit = SetUpCandidateWindowWithStrictMock();

    const Rect empty_rect(0, 0, 0, 0);
    EXPECT_CALL(*testkit.table_layout_mock, GetFooterRect())
        .WillOnce(Return(empty_rect));

    // Just allocate footer.
    testkit.window->candidates_.mutable_footer();
    testkit.window->DrawFooter();
    FinalizeTestKit(&testkit);
  }
  {
    // Other case of DrawFooter is tested by break-downed functions, which are
    // DrawFooterSeparator, DrawFooterIndex, DrawLogo, DrawFooterLabel.
    // And their call implementation is enough simple for test-free.
  }
}

TEST_F(CandidateWindowTest, DrawFooterSeparatorTest) {
  CandidateWindowTestKit testkit = SetUpCandidateWindow();
  const Rect footer_rect(10, 20, 30, 40);
  const Point expect_line_from = footer_rect.origin;
  const Point expect_line_to(footer_rect.Right(), footer_rect.Top());
  EXPECT_CALL(*testkit.draw_tool_mock,
              DrawLine(PointEq(expect_line_from), PointEq(expect_line_to),
                       RGBAEq(kFrameColor), kFooterSeparatorHeight));

  const Rect expect_rest_area(
      footer_rect.Left(), footer_rect.Top() + kFooterSeparatorHeight,
      footer_rect.Width(), footer_rect.Height() - kFooterSeparatorHeight);
  Rect result = footer_rect;
  testkit.window->DrawFooterSeparator(&result);
  EXPECT_EQ(expect_rest_area.origin.x, result.origin.x);
  EXPECT_EQ(expect_rest_area.origin.y, result.origin.y);
  EXPECT_EQ(expect_rest_area.size.width, result.size.width);
  EXPECT_EQ(expect_rest_area.size.height, result.size.height);
  FinalizeTestKit(&testkit);
}

TEST_F(CandidateWindowTest, DrawFooterIndexTest) {
  {
    SCOPED_TRACE("If there is no footer, do nothing");
    CandidateWindowTestKit testkit = SetUpCandidateWindowWithStrictMock();

    const Rect original_footer_content_area(10, 20, 30, 40);
    Rect footer_content_area = original_footer_content_area;
    testkit.window->DrawFooterIndex(&footer_content_area);
    EXPECT_EQ(original_footer_content_area.origin.x,
              footer_content_area.origin.x);
    EXPECT_EQ(original_footer_content_area.origin.y,
              footer_content_area.origin.y);
    EXPECT_EQ(original_footer_content_area.size.width,
              footer_content_area.size.width);
    EXPECT_EQ(original_footer_content_area.size.height,
              footer_content_area.size.height);
    FinalizeTestKit(&testkit);
  }
  {
    SCOPED_TRACE("If there is no focused_index, do nothing");
    CandidateWindowTestKit testkit = SetUpCandidateWindowWithStrictMock();

    // SetTestCandidates does not set focused_index.
    SetTestCandidates(10, true, true, true, true, true,
                      &testkit.window->candidates_);

    const Rect original_footer_content_area(10, 20, 30, 40);
    Rect footer_content_area = original_footer_content_area;
    testkit.window->DrawFooterIndex(&footer_content_area);
    EXPECT_EQ(original_footer_content_area.origin.x,
              footer_content_area.origin.x);
    EXPECT_EQ(original_footer_content_area.origin.y,
              footer_content_area.origin.y);
    EXPECT_EQ(original_footer_content_area.size.width,
              footer_content_area.size.width);
    EXPECT_EQ(original_footer_content_area.size.height,
              footer_content_area.size.height);

    FinalizeTestKit(&testkit);
  }
  {
    SCOPED_TRACE("If index is not visible, do nothing.");
    CandidateWindowTestKit testkit = SetUpCandidateWindowWithStrictMock();

    testkit.window->candidates_.mutable_footer()->set_index_visible(false);

    const Rect original_footer_content_area(10, 20, 30, 40);
    Rect footer_content_area = original_footer_content_area;
    testkit.window->DrawFooterIndex(&footer_content_area);
    EXPECT_EQ(original_footer_content_area.origin.x,
              footer_content_area.origin.x);
    EXPECT_EQ(original_footer_content_area.origin.y,
              footer_content_area.origin.y);
    EXPECT_EQ(original_footer_content_area.size.width,
              footer_content_area.size.width);
    EXPECT_EQ(original_footer_content_area.size.height,
              footer_content_area.size.height);
    FinalizeTestKit(&testkit);
  }
  {
    CandidateWindowTestKit testkit = SetUpCandidateWindow();

    Rect original_footer_content_area(100, 200, 300, 400);
    const int focused_index = 3;
    const int total_items = 7;
    std::stringstream footer_string;
    footer_string << focused_index + 1 << "/" << total_items << " ";
    const std::string index_guide_string = footer_string.str();
    const Size index_guide_size(10, 20);
    const Rect index_rect(
        original_footer_content_area.Right() - index_guide_size.width,
        original_footer_content_area.Top(), index_guide_size.width,
        original_footer_content_area.Height());
    const Rect expect_remaining_rect(
        original_footer_content_area.Left(), original_footer_content_area.Top(),
        original_footer_content_area.Width() - index_guide_size.width,
        original_footer_content_area.Height());

    SetTestCandidates(total_items, true, true, true, true, true,
                      &testkit.window->candidates_);
    testkit.window->candidates_.mutable_footer()->set_index_visible(true);
    testkit.window->candidates_.set_focused_index(focused_index);

    EXPECT_CALL(*testkit.text_renderer_mock,
                GetPixelSize(FontSpecInterface::FONTSET_FOOTER_INDEX,
                             index_guide_string))
        .WillOnce(Return(index_guide_size));

    EXPECT_CALL(*testkit.text_renderer_mock,
                RenderText(index_guide_string, RectEq(index_rect),
                           FontSpecInterface::FONTSET_FOOTER_INDEX));

    Rect footer_content_area = original_footer_content_area;
    testkit.window->DrawFooterIndex(&footer_content_area);

    EXPECT_EQ(expect_remaining_rect.origin.x, footer_content_area.origin.x);
    EXPECT_EQ(expect_remaining_rect.origin.y, footer_content_area.origin.y);
    EXPECT_EQ(expect_remaining_rect.size.width, footer_content_area.size.width);
    EXPECT_EQ(expect_remaining_rect.size.height,
              footer_content_area.size.height);
    FinalizeTestKit(&testkit);
  }
}

TEST_F(CandidateWindowTest, DrawLogoTest) {
  // TODO(nona): implement logo.
}

TEST_F(CandidateWindowTest, DrawFooterLabelTest) {
  {
    SCOPED_TRACE("If target content area is empty, do nothing.");
    CandidateWindowTestKit testkit = SetUpCandidateWindowWithStrictMock();
    const Rect empty_rect(0, 0, 0, 0);
    testkit.window->DrawFooterLabel(empty_rect);
    FinalizeTestKit(&testkit);
  }
  {
    SCOPED_TRACE("If no label and no sub-label, do nothing");
    CandidateWindowTestKit testkit = SetUpCandidateWindowWithStrictMock();
    const Rect footer_content_area(10, 20, 30, 40);
    testkit.window->DrawFooterLabel(footer_content_area);
    FinalizeTestKit(&testkit);
  }
  {
    SCOPED_TRACE("If there is label in candidates, drow it");
    CandidateWindowTestKit testkit = SetUpCandidateWindow();
    const Rect footer_content_area(10, 20, 30, 40);

    const std::string label_str = "LABEL";
    testkit.window->candidates_.mutable_footer()->set_label(label_str);
    EXPECT_CALL(*testkit.text_renderer_mock,
                RenderText(label_str, RectEq(footer_content_area),
                           FontSpecInterface::FONTSET_FOOTER_LABEL));

    testkit.window->DrawFooterLabel(footer_content_area);
    FinalizeTestKit(&testkit);
  }
  {
    SCOPED_TRACE("If there is label in candidates, drow it");
    CandidateWindowTestKit testkit = SetUpCandidateWindow();
    const Rect footer_content_area(10, 20, 30, 40);

    const std::string sub_label_str = "SUBLABEL";
    testkit.window->candidates_.mutable_footer()->set_sub_label(sub_label_str);
    EXPECT_CALL(*testkit.text_renderer_mock,
                RenderText(sub_label_str, RectEq(footer_content_area),
                           FontSpecInterface::FONTSET_FOOTER_SUBLABEL));

    testkit.window->DrawFooterLabel(footer_content_area);
    FinalizeTestKit(&testkit);
  }
  {
    SCOPED_TRACE("If there are both label and sublabel, drow label only");
    CandidateWindowTestKit testkit = SetUpCandidateWindow();
    const Rect footer_content_area(10, 20, 30, 40);

    const std::string label_str = "LABEL";
    const std::string sub_label_str = "SUBLABEL";
    testkit.window->candidates_.mutable_footer()->set_label(label_str);
    testkit.window->candidates_.mutable_footer()->set_sub_label(sub_label_str);
    EXPECT_CALL(*testkit.text_renderer_mock,
                RenderText(label_str, RectEq(footer_content_area),
                           FontSpecInterface::FONTSET_FOOTER_LABEL));
    EXPECT_CALL(*testkit.text_renderer_mock,
                RenderText(sub_label_str, RectEq(footer_content_area),
                           FontSpecInterface::FONTSET_FOOTER_SUBLABEL))
        .Times(0);

    testkit.window->DrawFooterLabel(footer_content_area);
    FinalizeTestKit(&testkit);
  }
}

TEST_F(CandidateWindowTest, DrawVScrollBarTest) {
  // TODO(nona): Implement scroll bar.
}

TEST_F(CandidateWindowTest, UpdateScrollBarSizeTest) {
  // TODO(nona): Implement scroll bar.
}

TEST_F(CandidateWindowTest, UpdateFooterSizeTest) {
  // TODO(nona): Implement this test. Need break-down?
}

TEST_F(CandidateWindowTest, UpdateTest) {
  // TODO(nona): Implement this test.
}

TEST_F(CandidateWindowTest, UpdateGap1SizeTest) {
  CandidateWindowTestKit testkit = SetUpCandidateWindow();

  const Size spacing_size(10, 20);
  EXPECT_CALL(*testkit.text_renderer_mock,
              GetPixelSize(FontSpecInterface::FONTSET_CANDIDATE, " "))
      .WillOnce(Return(spacing_size));

  EXPECT_CALL(
      *testkit.table_layout_mock,
      EnsureCellSize(CandidateWindow::COLUMN_GAP1, SizeEq(spacing_size)));

  testkit.window->UpdateGap1Size();
  FinalizeTestKit(&testkit);
}

TEST_F(CandidateWindowTest, UpdateCandidatesSizeTest) {
  {
    SCOPED_TRACE("If there is no candidates, do nothing");
    CandidateWindowTestKit testkit = SetUpCandidateWindowWithStrictMock();

    bool has_description;
    testkit.window->UpdateCandidatesSize(&has_description);
    EXPECT_FALSE(has_description);
    FinalizeTestKit(&testkit);
  }
  {
    SCOPED_TRACE("No shortcut, no description case");
    CandidateWindowTestKit testkit = SetUpCandidateWindow();

    const int candidate_count = 10;
    SetTestCandidates(candidate_count, true, false, false, true, true,
                      &testkit.window->candidates_);

    for (int i = 0; i < candidate_count; ++i) {
      const std::string expected_value = GetExpectedValue(i, true, true);
      const Size value_size(10 * i, 20 * i);
      EXPECT_CALL(
          *testkit.text_renderer_mock,
          GetPixelSize(FontSpecInterface::FONTSET_CANDIDATE, expected_value))
          .WillOnce(Return(value_size));
      EXPECT_CALL(*testkit.table_layout_mock,
                  EnsureCellSize(CandidateWindow::COLUMN_CANDIDATE,
                                 SizeEq(value_size)));
    }

    bool has_description;
    testkit.window->UpdateCandidatesSize(&has_description);
    EXPECT_FALSE(has_description);
    FinalizeTestKit(&testkit);
  }
  {
    SCOPED_TRACE("No description case");
    CandidateWindowTestKit testkit = SetUpCandidateWindow();

    const int candidate_count = 10;
    SetTestCandidates(candidate_count, true, true, false, true, true,
                      &testkit.window->candidates_);

    for (int i = 0; i < candidate_count; ++i) {
      const std::string expected_value = GetExpectedValue(i, true, true);

      // Shortcut string is padded with one spacing character.
      std::stringstream shortcut_stream;
      shortcut_stream << " " << GetExpectedShortcut(i) << " ";

      const std::string expected_shortcut = shortcut_stream.str();
      const Size value_size(10 * i, 20 * i);
      const Size shortcut_size(11 * i, 21 * i);
      EXPECT_CALL(
          *testkit.text_renderer_mock,
          GetPixelSize(FontSpecInterface::FONTSET_CANDIDATE, expected_value))
          .WillOnce(Return(value_size));
      EXPECT_CALL(
          *testkit.text_renderer_mock,
          GetPixelSize(FontSpecInterface::FONTSET_SHORTCUT, expected_shortcut))
          .WillOnce(Return(shortcut_size));

      EXPECT_CALL(*testkit.table_layout_mock,
                  EnsureCellSize(CandidateWindow::COLUMN_CANDIDATE,
                                 SizeEq(value_size)));
      EXPECT_CALL(*testkit.table_layout_mock,
                  EnsureCellSize(CandidateWindow::COLUMN_SHORTCUT,
                                 SizeEq(shortcut_size)));
    }

    bool has_description;
    testkit.window->UpdateCandidatesSize(&has_description);
    EXPECT_FALSE(has_description);
    FinalizeTestKit(&testkit);
  }
  {
    SCOPED_TRACE("No shortcut case");
    CandidateWindowTestKit testkit = SetUpCandidateWindow();

    const int candidate_count = 10;
    SetTestCandidates(candidate_count, true, false, true, true, true,
                      &testkit.window->candidates_);

    for (int i = 0; i < candidate_count; ++i) {
      const std::string expected_value = GetExpectedValue(i, true, true);

      // Description string is end-padded with one spacing character.
      std::stringstream description_stream;
      description_stream << GetExpectedDescription(i) << " ";

      const std::string expected_description = description_stream.str();
      const Size value_size(10 * i, 20 * i);
      const Size description_size(11 * i, 21 * i);
      EXPECT_CALL(
          *testkit.text_renderer_mock,
          GetPixelSize(FontSpecInterface::FONTSET_CANDIDATE, expected_value))
          .WillOnce(Return(value_size));
      EXPECT_CALL(*testkit.text_renderer_mock,
                  GetPixelSize(FontSpecInterface::FONTSET_DESCRIPTION,
                               expected_description))
          .WillOnce(Return(description_size));

      EXPECT_CALL(*testkit.table_layout_mock,
                  EnsureCellSize(CandidateWindow::COLUMN_CANDIDATE,
                                 SizeEq(value_size)));
      EXPECT_CALL(*testkit.table_layout_mock,
                  EnsureCellSize(CandidateWindow::COLUMN_DESCRIPTION,
                                 SizeEq(description_size)));
    }

    bool has_description;
    testkit.window->UpdateCandidatesSize(&has_description);
    EXPECT_TRUE(has_description);
    FinalizeTestKit(&testkit);
  }
  {
    SCOPED_TRACE("Both shortcut and description");
    CandidateWindowTestKit testkit = SetUpCandidateWindow();

    const int candidate_count = 10;
    SetTestCandidates(candidate_count, true, true, true, true, true,
                      &testkit.window->candidates_);

    for (int i = 0; i < candidate_count; ++i) {
      const std::string expected_value = GetExpectedValue(i, true, true);

      // Shortcut string is padded with one spacing character.
      std::stringstream shortcut_stream;
      shortcut_stream << " " << GetExpectedShortcut(i) << " ";
      const std::string expected_shortcut = shortcut_stream.str();

      // Description string is end-padded with one spacing character.
      std::stringstream description_stream;
      description_stream << GetExpectedDescription(i) << " ";
      const std::string expected_description = description_stream.str();

      const Size value_size(10 * i, 20 * i);
      const Size description_size(11 * i, 21 * i);
      const Size shortcut_size(12 * i, 22 * i);

      EXPECT_CALL(
          *testkit.text_renderer_mock,
          GetPixelSize(FontSpecInterface::FONTSET_CANDIDATE, expected_value))
          .WillOnce(Return(value_size));
      EXPECT_CALL(
          *testkit.text_renderer_mock,
          GetPixelSize(FontSpecInterface::FONTSET_SHORTCUT, expected_shortcut))
          .WillOnce(Return(shortcut_size));
      EXPECT_CALL(*testkit.text_renderer_mock,
                  GetPixelSize(FontSpecInterface::FONTSET_DESCRIPTION,
                               expected_description))
          .WillOnce(Return(description_size));

      EXPECT_CALL(*testkit.table_layout_mock,
                  EnsureCellSize(CandidateWindow::COLUMN_CANDIDATE,
                                 SizeEq(value_size)));
      EXPECT_CALL(*testkit.table_layout_mock,
                  EnsureCellSize(CandidateWindow::COLUMN_SHORTCUT,
                                 SizeEq(shortcut_size)));
      EXPECT_CALL(*testkit.table_layout_mock,
                  EnsureCellSize(CandidateWindow::COLUMN_DESCRIPTION,
                                 SizeEq(description_size)));
    }

    bool has_description;
    testkit.window->UpdateCandidatesSize(&has_description);
    EXPECT_TRUE(has_description);
    FinalizeTestKit(&testkit);
  }
}

TEST_F(CandidateWindowTest, UpdateGap2SizeTest) {
  {
    SCOPED_TRACE("If there is description, use three spacings");
    CandidateWindowTestKit testkit = SetUpCandidateWindow();

    const Size spacing_size(10, 20);
    EXPECT_CALL(*testkit.text_renderer_mock,
                GetPixelSize(FontSpecInterface::FONTSET_CANDIDATE, "   "))
        .WillOnce(Return(spacing_size));

    EXPECT_CALL(
        *testkit.table_layout_mock,
        EnsureCellSize(CandidateWindow::COLUMN_GAP2, SizeEq(spacing_size)));

    testkit.window->UpdateGap2Size(true);
    FinalizeTestKit(&testkit);
  }
  {
    SCOPED_TRACE("If there is no description, use a spacings");
    CandidateWindowTestKit testkit = SetUpCandidateWindow();

    const Size spacing_size(10, 20);
    EXPECT_CALL(*testkit.text_renderer_mock,
                GetPixelSize(FontSpecInterface::FONTSET_CANDIDATE, " "))
        .WillOnce(Return(spacing_size));

    EXPECT_CALL(
        *testkit.table_layout_mock,
        EnsureCellSize(CandidateWindow::COLUMN_GAP2, SizeEq(spacing_size)));

    testkit.window->UpdateGap2Size(false);
    FinalizeTestKit(&testkit);
  }
}

TEST_F(CandidateWindowTest, DrawFrameTest) {
  CandidateWindowTestKit testkit = SetUpCandidateWindow();

  const Size total_size(30, 40);
  const Rect expect_draw_area(Point(0, 0), total_size);
  EXPECT_CALL(*testkit.table_layout_mock, GetTotalSize())
      .WillOnce(Return(total_size));
  EXPECT_CALL(*testkit.draw_tool_mock,
              FrameRect(RectEq(expect_draw_area), RGBAEq(kFrameColor), 1));

  testkit.window->DrawFrame();
  FinalizeTestKit(&testkit);
}

TEST_F(CandidateWindowTest, OnMouseLeftUpTest) {
  const Point kPos(10, 20);
  const Rect kInRect(5, 5, 100, 100);
  const Rect kOutRect(20, 30, 100, 100);
  {
    SCOPED_TRACE("SendCommandInterface does not exist.");
    CandidateWindowTestKit testkit = SetUpCandidateWindow();
    testkit.window->OnMouseLeftUp(kPos);
    // We expects doing nothing except just logging ERROR message. However we
    // can't check.
    FinalizeTestKit(&testkit);
  }
  {
    SCOPED_TRACE("If selected pos is out of range, do nothing.");
    SendCommandInterfaceMock interface_mock;
    MouseHandlingTestableCandidateWindowTestKit testkit =
        SetUpMouseHandlingTestableCandidateWindow();
    testkit.window->SetSendCommandInterface(&interface_mock);
    EXPECT_CALL(*testkit.window, GetSelectedRowIndex(_)).WillOnce(Return(-1));
    EXPECT_CALL(interface_mock, SendCommand(_, _)).Times(0);
    testkit.window->OnMouseLeftUp(kPos);
    FinalizeTestKit(&testkit);
  }
  {
    SCOPED_TRACE("Expected ID will be set by candidate index.");
    constexpr size_t kTestRound = 10;
    for (size_t i = 0; i < kTestRound; ++i) {
      SendCommandInterfaceMock interface_mock;
      MouseHandlingTestableCandidateWindowTestKit testkit =
          SetUpMouseHandlingTestableCandidateWindow();
      testkit.window->SetSendCommandInterface(&interface_mock);
      SetTestCandidates(kTestRound, true, true, true, true, true,
                        &testkit.window->candidates_);
      EXPECT_CALL(*testkit.window, GetSelectedRowIndex(_)).WillOnce(Return(i));
      EXPECT_CALL(interface_mock, SendCommand(SelectCommandEq(i * 0x10), _))
          .Times(1);
      testkit.window->OnMouseLeftUp(kPos);
      FinalizeTestKit(&testkit);
    }
  }
}

TEST_F(CandidateWindowTest, GetSelectedRowIndexTest) {
  const Point kPos(10, 20);
  const Rect kInRect(5, 5, 100, 100);
  const Rect kOutRect(20, 30, 100, 100);
  {
    constexpr size_t kTestRound = 10;
    for (size_t i = 0; i < kTestRound; ++i) {
      CandidateWindowTestKit testkit = SetUpCandidateWindow();
      SetTestCandidates(kTestRound, true, true, true, true, true,
                        &testkit.window->candidates_);
      EXPECT_CALL(*testkit.table_layout_mock, GetRowRect(i))
          .WillOnce(Return(kInRect));
      EXPECT_CALL(*testkit.table_layout_mock, GetRowRect(Ne(i)))
          .WillRepeatedly(Return(kOutRect));
      EXPECT_EQ(i, testkit.window->GetSelectedRowIndex(kPos));
      FinalizeTestKit(&testkit);
    }
  }
  {
    SCOPED_TRACE("The case of out of candidate area clicking.");
    CandidateWindowTestKit testkit = SetUpCandidateWindow();
    SetTestCandidates(10, true, true, true, true, true,
                      &testkit.window->candidates_);
    EXPECT_CALL(*testkit.table_layout_mock, GetRowRect(_))
        .Times(10)
        .WillRepeatedly(Return(kOutRect));
    EXPECT_EQ(-1, testkit.window->GetSelectedRowIndex(kPos));
    FinalizeTestKit(&testkit);
  }
}

TEST_F(CandidateWindowTest, ReloadFontConfigTest) {
  CandidateWindowTestKit testkit = SetUpCandidateWindow();
  const char dummy_font[] = "Foo,Bar,Baz";
  EXPECT_CALL(*testkit.text_renderer_mock, ReloadFontConfig(StrEq(dummy_font)));
  testkit.window->ReloadFontConfig(dummy_font);
  FinalizeTestKit(&testkit);
}

}  // namespace gtk
}  // namespace renderer
}  // namespace mozc
