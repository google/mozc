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

#include "renderer/unix/infolist_window.h"

#include "base/number_util.h"
#include "renderer/unix/cairo_factory_mock.h"
#include "renderer/unix/const.h"
#include "renderer/unix/draw_tool_mock.h"
#include "renderer/unix/gtk_wrapper_mock.h"
#include "renderer/unix/text_renderer_mock.h"
#include "testing/base/public/gmock.h"
#include "testing/base/public/gunit.h"

using ::testing::Return;
using ::testing::StrEq;
using ::testing::StrictMock;
using ::testing::_;

namespace mozc {
namespace renderer {
namespace gtk {

namespace {
// Following variable is used testing and it is contant but due to API
// restriction, can not modify const modifier.
GtkWidget *kDummyWindow = reinterpret_cast<GtkWidget*>(0x12345678);
GtkWidget *kDummyCanvas = reinterpret_cast<GtkWidget*>(0x87654321);

const char kSampleTitle[] = "TITLE";
const char kSampleDescription[] = "DESCRIPTION";

void SetInformations(int count, commands::InformationList *usages) {
  usages->Clear();
  for (int i = 0; i < count; ++i) {
    commands::Information *info = usages->add_information();

    const string id_str = NumberUtil::SimpleItoa(i);
    info->set_title(kSampleTitle + id_str);
    info->set_description(kSampleDescription + id_str);
  }
}

string GetExpectedTitle(int row) {
  const string id_str = NumberUtil::SimpleItoa(row);
  return kSampleTitle + id_str;
}

string GetExpectedDescription(int row) {
  const string id_str = NumberUtil::SimpleItoa(row);
  return kSampleDescription + id_str;
}

MATCHER_P(RectEq, expected_rect, "The expected rect does not match") {
  return (arg.origin.x == expected_rect.origin.x) &&
      (arg.origin.y == expected_rect.origin.y) &&
      (arg.size.width == expected_rect.size.width) &&
      (arg.size.height == expected_rect.size.height);
}

MATCHER_P(RGBAEq, expected_rgba, "The expected RGBA does not match") {
  return (arg.red == expected_rgba.red) &&
      (arg.green == expected_rgba.green) &&
      (arg.blue == expected_rgba.blue) &&
      (arg.alpha == expected_rgba.alpha);
}

RGBA StyleColorToRGBA(
    const RendererStyle::RGBAColor &rgbacolor) {
  const RGBA rgba = {
    static_cast<uint8>(rgbacolor.r()),
    static_cast<uint8>(rgbacolor.g()),
    static_cast<uint8>(rgbacolor.b()),
    0xFF };
  return rgba;
}
}  // namespace

class InfolistWindowTest : public testing::Test {
 protected:
  struct InfolistWindowTestKit {
    GtkWrapperMock *gtk_mock;
    TextRendererMock *text_renderer_mock;
    DrawToolMock *draw_tool_mock;
    CairoFactoryMock *cairo_factory_mock;
    InfolistWindow *window;
  };

  void SetUpInfolistWindowConstractorCallExpectations(
      InfolistWindowTestKit *testkit) {
    // Following functions are expected to be called by constructor.
    EXPECT_CALL(*testkit->gtk_mock, GtkWindowNew(GTK_WINDOW_POPUP))
        .WillOnce(Return(kDummyWindow));
    EXPECT_CALL(*testkit->gtk_mock, GtkDrawingAreaNew())
        .WillOnce(Return(kDummyCanvas));
    EXPECT_CALL(*testkit->gtk_mock, GSignalConnect(
        kDummyWindow, StrEq("destroy"),
        G_CALLBACK(GtkWindowBase::OnDestroyThunk), _));
    EXPECT_CALL(*testkit->gtk_mock, GSignalConnect(
        kDummyWindow, StrEq("button-press-event"),
        G_CALLBACK(GtkWindowBase::OnMouseDownThunk), _));
    EXPECT_CALL(*testkit->gtk_mock, GSignalConnect(
        kDummyWindow, StrEq("button-release-event"),
        G_CALLBACK(GtkWindowBase::OnMouseUpThunk), _));
    EXPECT_CALL(*testkit->gtk_mock, GSignalConnect(
        kDummyCanvas, StrEq("expose-event"),
        G_CALLBACK(GtkWindowBase::OnPaintThunk), _));
    EXPECT_CALL(*testkit->gtk_mock,
                GtkContainerAdd(kDummyWindow, kDummyCanvas));
    EXPECT_CALL(*testkit->gtk_mock,
                GtkWidgetAddEvents(kDummyWindow, GDK_BUTTON_PRESS_MASK));
    EXPECT_CALL(*testkit->gtk_mock,
                GtkWidgetAddEvents(kDummyWindow, GDK_BUTTON_RELEASE_MASK));
  }

  InfolistWindowTestKit SetUpInfolistWindow() {
    InfolistWindowTestKit testkit;
    testkit.gtk_mock = new GtkWrapperMock();
    testkit.text_renderer_mock = new TextRendererMock();
    testkit.draw_tool_mock = new DrawToolMock();
    testkit.cairo_factory_mock = new CairoFactoryMock();

    SetUpInfolistWindowConstractorCallExpectations(&testkit);

    testkit.window = new InfolistWindow(testkit.text_renderer_mock,
                                        testkit.draw_tool_mock,
                                        testkit.gtk_mock,
                                        testkit.cairo_factory_mock);
    return testkit;
  }

  InfolistWindowTestKit SetUpInfolistWindowWithStrictMock() {
    InfolistWindowTestKit testkit;
    testkit.gtk_mock = new StrictMock<GtkWrapperMock>();
    testkit.text_renderer_mock = new StrictMock<TextRendererMock>();
    testkit.draw_tool_mock = new StrictMock<DrawToolMock>();
    testkit.cairo_factory_mock = new StrictMock<CairoFactoryMock>();

    SetUpInfolistWindowConstractorCallExpectations(&testkit);

    testkit.window = new InfolistWindow(testkit.text_renderer_mock,
                                        testkit.draw_tool_mock,
                                        testkit.gtk_mock,
                                        testkit.cairo_factory_mock);
    return testkit;
  }

  void FinalizeTestKit(InfolistWindowTestKit *testkit) {
    delete testkit->window;
  }
};

TEST_F(InfolistWindowTest, DrawFrameTest) {
  InfolistWindowTestKit testkit = SetUpInfolistWindow();
  const RendererStyle::InfolistStyle &infostyle
      = testkit.window->style_->infolist_style();
  const int height = 1234;
  const Rect expected_rect(0, 0, infostyle.window_width(), height);
  EXPECT_CALL(*testkit.draw_tool_mock,
              FrameRect(RectEq(expected_rect),
                        RGBAEq(StyleColorToRGBA(infostyle.border_color())),
                        1));
  testkit.window->DrawFrame(height);
  FinalizeTestKit(&testkit);
}

TEST_F(InfolistWindowTest, GetRowRectsTest) {
  InfolistWindowTestKit testkit = SetUpInfolistWindow();
  const RendererStyle::InfolistStyle &infostyle
      = testkit.window->style_->infolist_style();
  const renderer::RendererStyle::TextStyle title_style
      = infostyle.title_style();
  const renderer::RendererStyle::TextStyle description_style
      = infostyle.description_style();
  const int ypos = 123;
  const FontSpecInterface::FONT_TYPE infolist_font_type
      = FontSpecInterface::FONTSET_INFOLIST_TITLE;
  const FontSpecInterface::FONT_TYPE description_font_type
      = FontSpecInterface::FONTSET_INFOLIST_DESCRIPTION;
  const Size text_size(10, 20);

  SetInformations(10, testkit.window->candidates_.mutable_usages());

  EXPECT_CALL(*testkit.text_renderer_mock,
              GetMultiLinePixelSize(infolist_font_type,
                                    GetExpectedTitle(0), _))
      .WillOnce(Return(text_size));
  EXPECT_CALL(*testkit.text_renderer_mock,
              GetMultiLinePixelSize(description_font_type,
                                    GetExpectedDescription(0), _))
      .WillOnce(Return(text_size));

  InfolistWindow::RenderingRowRects row_rects
      = testkit.window->GetRowRects(0, ypos);

  EXPECT_EQ(row_rects.title_back_rect.Height(),
            row_rects.desc_back_rect.Top() - row_rects.title_back_rect.Top());
  EXPECT_EQ(row_rects.title_back_rect.Top(), row_rects.whole_rect.Top());
  EXPECT_EQ(row_rects.title_back_rect.Left(), row_rects.whole_rect.Left());
  EXPECT_EQ(row_rects.title_back_rect.Width(), row_rects.whole_rect.Width());
  EXPECT_EQ(row_rects.title_back_rect.Height()
                + row_rects.desc_back_rect.Height(),
            row_rects.whole_rect.Height());

  FinalizeTestKit(&testkit);
}

TEST_F(InfolistWindowTest, DrawRowTest) {
  {
    SCOPED_TRACE("Draw focused area");
    for (int i = 0; i < 10; ++i) {
      InfolistWindowTestKit testkit = SetUpInfolistWindow();
      const RendererStyle::InfolistStyle &infostyle
          = testkit.window->style_->infolist_style();
      const FontSpecInterface::FONT_TYPE infolist_font_type
          = FontSpecInterface::FONTSET_INFOLIST_TITLE;
      const FontSpecInterface::FONT_TYPE description_font_type
          = FontSpecInterface::FONTSET_INFOLIST_DESCRIPTION;
      const Size text_size(10, 20);
      SetInformations(10, testkit.window->candidates_.mutable_usages());
      const int ypos = i * 15;

      testkit.window->candidates_.mutable_usages()->set_focused_index(i);

      EXPECT_CALL(*testkit.text_renderer_mock,
                  GetMultiLinePixelSize(infolist_font_type,
                                        GetExpectedTitle(i), _))
          .WillRepeatedly(Return(text_size));
      EXPECT_CALL(*testkit.text_renderer_mock,
                  GetMultiLinePixelSize(description_font_type,
                                        GetExpectedDescription(i), _))
          .WillRepeatedly(Return(text_size));

      const InfolistWindow::RenderingRowRects sample_row_rects =
          testkit.window->GetRowRects(i, ypos);

      EXPECT_CALL(*testkit.draw_tool_mock,
                  FillRect(RectEq(sample_row_rects.whole_rect),
                           RGBAEq(StyleColorToRGBA(
                               infostyle.focused_background_color()))));
      EXPECT_CALL(*testkit.draw_tool_mock,
                  FrameRect(RectEq(sample_row_rects.whole_rect),
                            RGBAEq(StyleColorToRGBA(
                                infostyle.focused_border_color())),
                            1));
      EXPECT_CALL(*testkit.text_renderer_mock,
                  RenderText(StrEq(GetExpectedTitle(i)),
                             RectEq(sample_row_rects.title_rect),
                             FontSpecInterface::FONTSET_INFOLIST_TITLE));
      EXPECT_CALL(*testkit.text_renderer_mock,
                  RenderText(StrEq(GetExpectedDescription(i)),
                             RectEq(sample_row_rects.desc_rect),
                             FontSpecInterface::FONTSET_INFOLIST_DESCRIPTION));

      testkit.window->DrawRow(i, ypos);
      FinalizeTestKit(&testkit);
    }
  }
  {
    SCOPED_TRACE("Draw focus-outed area with default color");
    for (int i = 0; i < 10; ++i) {
      InfolistWindowTestKit testkit = SetUpInfolistWindow();
      RendererStyle::InfolistStyle *infostyle
          = testkit.window->style_->mutable_infolist_style();
      infostyle->mutable_title_style()->clear_background_color();
      infostyle->mutable_description_style()->clear_background_color();
      const FontSpecInterface::FONT_TYPE infolist_font_type
          = FontSpecInterface::FONTSET_INFOLIST_TITLE;
      const FontSpecInterface::FONT_TYPE description_font_type
          = FontSpecInterface::FONTSET_INFOLIST_DESCRIPTION;
      const Size text_size(10, 20);
      SetInformations(10, testkit.window->candidates_.mutable_usages());
      const int ypos = i * 15;
      SetInformations(10, testkit.window->candidates_.mutable_usages());

      EXPECT_CALL(*testkit.text_renderer_mock,
                  GetMultiLinePixelSize(infolist_font_type,
                                        GetExpectedTitle(i), _))
          .WillRepeatedly(Return(text_size));
      EXPECT_CALL(*testkit.text_renderer_mock,
                  GetMultiLinePixelSize(description_font_type,
                                        GetExpectedDescription(i), _))
          .WillRepeatedly(Return(text_size));

      const InfolistWindow::RenderingRowRects sample_row_rects =
          testkit.window->GetRowRects(i, ypos);

      EXPECT_CALL(*testkit.draw_tool_mock,
                  FillRect(RectEq(sample_row_rects.title_back_rect),
                           RGBAEq(kWhite)));
      EXPECT_CALL(*testkit.draw_tool_mock,
                  FillRect(RectEq(sample_row_rects.desc_back_rect),
                           RGBAEq(kWhite)));
      EXPECT_CALL(*testkit.text_renderer_mock,
                  RenderText(StrEq(GetExpectedTitle(i)),
                             RectEq(sample_row_rects.title_rect),
                             FontSpecInterface::FONTSET_INFOLIST_TITLE));
      EXPECT_CALL(*testkit.text_renderer_mock,
                  RenderText(StrEq(GetExpectedDescription(i)),
                             RectEq(sample_row_rects.desc_rect),
                             FontSpecInterface::FONTSET_INFOLIST_DESCRIPTION));

      testkit.window->DrawRow(i, ypos);
      FinalizeTestKit(&testkit);
    }
  }
  {
    SCOPED_TRACE("Draw focus-outed area with specified color");
    for (int i = 0; i < 10; ++i) {
      InfolistWindowTestKit testkit = SetUpInfolistWindow();
      RendererStyle::InfolistStyle *infostyle
          = testkit.window->style_->mutable_infolist_style();
      infostyle->mutable_title_style()->mutable_background_color();
      infostyle->mutable_description_style()->mutable_background_color();
      const FontSpecInterface::FONT_TYPE infolist_font_type
          = FontSpecInterface::FONTSET_INFOLIST_TITLE;
      const FontSpecInterface::FONT_TYPE description_font_type
          = FontSpecInterface::FONTSET_INFOLIST_DESCRIPTION;
      const Size text_size(10, 20);
      SetInformations(10, testkit.window->candidates_.mutable_usages());
      const int ypos = i * 15;

      SetInformations(10, testkit.window->candidates_.mutable_usages());

      EXPECT_CALL(*testkit.text_renderer_mock,
                  GetMultiLinePixelSize(infolist_font_type,
                                        GetExpectedTitle(i), _))
          .WillRepeatedly(Return(text_size));
      EXPECT_CALL(*testkit.text_renderer_mock,
                  GetMultiLinePixelSize(description_font_type,
                                        GetExpectedDescription(i), _))
          .WillRepeatedly(Return(text_size));

      const InfolistWindow::RenderingRowRects sample_row_rects =
          testkit.window->GetRowRects(i, ypos);

      EXPECT_CALL(*testkit.draw_tool_mock,
                  FillRect(RectEq(sample_row_rects.title_back_rect),
                           RGBAEq(StyleColorToRGBA(
                               infostyle->title_style().background_color()))));
      EXPECT_CALL(*testkit.draw_tool_mock,
          FillRect(RectEq(sample_row_rects.desc_back_rect),
                   RGBAEq(StyleColorToRGBA(
                       infostyle->description_style().background_color()))));
      EXPECT_CALL(*testkit.text_renderer_mock,
                  RenderText(StrEq(GetExpectedTitle(i)),
                             RectEq(sample_row_rects.title_rect),
                             FontSpecInterface::FONTSET_INFOLIST_TITLE));
      EXPECT_CALL(*testkit.text_renderer_mock,
                  RenderText(StrEq(GetExpectedDescription(i)),
                             RectEq(sample_row_rects.desc_rect),
                             FontSpecInterface::FONTSET_INFOLIST_DESCRIPTION));

      testkit.window->DrawRow(i, ypos);
      FinalizeTestKit(&testkit);
    }
  }
}

TEST_F(InfolistWindowTest, DrawCaptionTest) {
  {
    SCOPED_TRACE("If there is no caption, do nothing");
    InfolistWindowTestKit testkit = SetUpInfolistWindowWithStrictMock();
    testkit.window->style_.reset(new RendererStyle());
    testkit.window->DrawCaption();
    FinalizeTestKit(&testkit);
  }
  {
    InfolistWindowTestKit testkit = SetUpInfolistWindow();
    const RendererStyle::InfolistStyle &infostyle
        = testkit.window->style_->infolist_style();
    const RendererStyle::TextStyle &caption_style = infostyle.caption_style();

    const Rect bg_expected_rect(
        infostyle.window_border(),
        infostyle.window_border(),
        infostyle.window_width() - infostyle.window_border() * 2,
        infostyle.caption_height());
    const RGBA bg_expected_color
        = StyleColorToRGBA(infostyle.caption_background_color());
    EXPECT_CALL(*testkit.draw_tool_mock,
                FillRect(RectEq(bg_expected_rect), RGBAEq(bg_expected_color)));

    const Rect caption_expected_rect(
        bg_expected_rect.Left()
            + infostyle.caption_padding() + caption_style.left_padding(),
        bg_expected_rect.Top() + infostyle.caption_padding(),
        bg_expected_rect.Width()
            - infostyle.caption_padding() - caption_style.left_padding(),
        infostyle.caption_height() - infostyle.caption_padding());
    EXPECT_CALL(*testkit.text_renderer_mock,
                RenderText(infostyle.caption_string(),
                           RectEq(caption_expected_rect),
                           FontSpecInterface::FONTSET_INFOLIST_CAPTION));

    EXPECT_EQ(infostyle.caption_height(), testkit.window->DrawCaption());

    FinalizeTestKit(&testkit);
  }
}

TEST_F(InfolistWindowTest, GetRenderingRectsTest) {
  // TODO(nona): rectangle argument verification.
  {
    SCOPED_TRACE("title style");
    InfolistWindowTestKit testkit = SetUpInfolistWindow();
    const RendererStyle::InfolistStyle &infostyle
        = testkit.window->style_->infolist_style();
    const renderer::RendererStyle::TextStyle title_style
        = infostyle.title_style();
    const int ypos = 123;
    const FontSpecInterface::FONT_TYPE font_type
        = FontSpecInterface::FONTSET_INFOLIST_TITLE;
    const Size text_size(10, 20);
    EXPECT_CALL(*testkit.text_renderer_mock,
                GetMultiLinePixelSize(font_type, kSampleTitle, _))
        .WillOnce(Return(text_size));

    Rect bg_rect, text_rect;
    testkit.window->GetRenderingRects(title_style, kSampleTitle,
                                      font_type, ypos, &bg_rect, &text_rect);
    FinalizeTestKit(&testkit);
  }
  {
    SCOPED_TRACE("description style");
    InfolistWindowTestKit testkit = SetUpInfolistWindow();
    const RendererStyle::InfolistStyle &infostyle
        = testkit.window->style_->infolist_style();
    const renderer::RendererStyle::TextStyle description_style
        = infostyle.description_style();
    const int ypos = 234;
    const FontSpecInterface::FONT_TYPE font_type
        = FontSpecInterface::FONTSET_INFOLIST_DESCRIPTION;
    const Size text_size(10, 20);
    EXPECT_CALL(*testkit.text_renderer_mock,
                GetMultiLinePixelSize(font_type, kSampleDescription, _))
        .WillOnce(Return(text_size));

    Rect bg_rect, text_rect;
    testkit.window->GetRenderingRects(description_style, kSampleDescription,
                                      font_type, ypos, &bg_rect, &text_rect);
    FinalizeTestKit(&testkit);
  }
}

TEST_F(InfolistWindowTest, ReloadFontConfigTest) {
  InfolistWindowTestKit testkit = SetUpInfolistWindow();
  const char kDummyFontDescription[] = "Foo,Bar,Baz";
  EXPECT_CALL(*testkit.text_renderer_mock,
              ReloadFontConfig(StrEq(kDummyFontDescription)));
  testkit.window->ReloadFontConfig(kDummyFontDescription);
  FinalizeTestKit(&testkit);
}

}  // namespace gtk
}  // namespace renderer
}  // namespace mozc
