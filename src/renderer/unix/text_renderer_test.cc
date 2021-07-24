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

#include "renderer/unix/text_renderer.h"
#include "renderer/unix/pango_wrapper_interface.h"
#include "testing/base/public/gmock.h"
#include "testing/base/public/gunit.h"

using ::testing::Expectation;
using ::testing::Return;

namespace mozc {
namespace renderer {
namespace gtk {

class FontSpecMock : public FontSpecInterface {
 public:
  MOCK_METHOD(PangoAlignment, GetFontAlignment, (FONT_TYPE font_type), (const));
  MOCK_METHOD(PangoAttrList *, GetFontAttributes, (FONT_TYPE font_type),
              (const));
  MOCK_METHOD(const PangoFontDescription *, GetFontDescription,
              (FONT_TYPE font_type), (const));
  MOCK_METHOD(void, Reload, (const std::string &font_description));
};

class PangoWrapperMock : public PangoWrapperInterface {
 public:
  MOCK_METHOD(void, RendererDrawLayout,
              (PangoLayoutWrapperInterface* layout, int x, int y));
  MOCK_METHOD(PangoContext *, GetContext, ());
  MOCK_METHOD(PangoAttrList *, CopyAttributes, (PangoAttrList* attr));
  MOCK_METHOD(void, AttributesUnref, (PangoAttrList* attr));
};

class PangoLayoutWrapperMock : public PangoLayoutWrapperInterface {
 public:
  MOCK_METHOD(void, SetText, (const std::string &text));
  MOCK_METHOD(void, SetAlignment, (PangoAlignment align));
  MOCK_METHOD(void, SetAttributes, (PangoAttrList* attr));
  MOCK_METHOD(void, SetFontDescription,
              (const PangoFontDescription *font_desc));
  MOCK_METHOD(void, SetWidth, (int width));
  MOCK_METHOD(void, SetHeight, (int height));
  MOCK_METHOD(Size, GetPixelSize, (), (const));
  MOCK_METHOD(PangoLayout *, GetPangoLayout, ());
};

class TextRendererTest : public testing::Test {
 protected:
  PangoWrapperMock *SetUpPangoMock(TextRenderer *text_renderer) {
    PangoWrapperMock *mock = new PangoWrapperMock();
    text_renderer->pango_.reset(mock);
    return mock;
  }
  FontSpecMock *SetUpFontSpecMock(TextRenderer *text_renderer) {
    FontSpecMock *mock = new FontSpecMock();
    text_renderer->font_spec_.reset(mock);
    return mock;
  }
};

TEST_F(TextRendererTest, GetPixelSizeTest) {
  FontSpecMock *font_spec_mock = new FontSpecMock();
  TextRenderer text_renderer(font_spec_mock);
  PangoWrapperMock *pango_mock = SetUpPangoMock(&text_renderer);

  const std::string text = "hogehoge";
  PangoLayoutWrapperMock layout_mock;
  PangoAlignment align = PANGO_ALIGN_CENTER;
  PangoAttrList *attributes = reinterpret_cast<PangoAttrList *>(0xabcdef01);
  PangoAttrList *copied_attributes =
      reinterpret_cast<PangoAttrList *>(0x237492);
  PangoFontDescription *font_desc =
      reinterpret_cast<PangoFontDescription *>(0x01abcdef);
  FontSpecInterface::FONT_TYPE font_type =
      FontSpecInterface::FONTSET_FOOTER_LABEL;
  Size size(12, 34);

  Expectation set_text_expectation = EXPECT_CALL(layout_mock, SetText(text));
  Expectation set_alignment_expectation =
      EXPECT_CALL(layout_mock, SetAlignment(align));
  Expectation set_attributes =
      EXPECT_CALL(layout_mock, SetAttributes(copied_attributes));
  Expectation set_font_description_expectation =
      EXPECT_CALL(layout_mock, SetFontDescription(font_desc));
  EXPECT_CALL(*font_spec_mock, GetFontAlignment(font_type))
      .WillOnce(Return(align));
  EXPECT_CALL(*font_spec_mock, GetFontAttributes(font_type))
      .WillOnce(Return(attributes));
  EXPECT_CALL(*pango_mock, CopyAttributes(attributes))
      .WillOnce(Return(copied_attributes));
  EXPECT_CALL(*font_spec_mock, GetFontDescription(font_type))
      .WillOnce(Return(font_desc));
  Expectation get_pixel_expectation =
      EXPECT_CALL(layout_mock, GetPixelSize())
          .After(set_text_expectation)
          .After(set_alignment_expectation)
          .After(set_attributes)
          .After(set_font_description_expectation)
          .WillOnce(Return(size));
  EXPECT_CALL(*pango_mock, AttributesUnref(copied_attributes))
      .After(set_attributes);

  Size actual_size = text_renderer.TextRenderer::GetPixelSizeInternal(
      font_type, text, &layout_mock);
  EXPECT_EQ(actual_size.width, size.width);
  EXPECT_EQ(actual_size.height, size.height);
}

TEST_F(TextRendererTest, GetMultilinePixelSizeTest) {
  FontSpecMock *font_spec_mock = new FontSpecMock();
  TextRenderer text_renderer(font_spec_mock);
  PangoWrapperMock *pango_mock = SetUpPangoMock(&text_renderer);

  const std::string text = "hogehoge";
  PangoLayoutWrapperMock layout_mock;
  PangoAlignment align = PANGO_ALIGN_CENTER;
  PangoAttrList *attributes = reinterpret_cast<PangoAttrList *>(0xabcdef01);
  PangoAttrList *copied_attributes =
      reinterpret_cast<PangoAttrList *>(0x237492);
  PangoFontDescription *font_desc =
      reinterpret_cast<PangoFontDescription *>(0x01abcdef);
  FontSpecInterface::FONT_TYPE font_type =
      FontSpecInterface::FONTSET_FOOTER_LABEL;
  int width = 12345;
  Size size(12, 34);

  Expectation set_text_expectation = EXPECT_CALL(layout_mock, SetText(text));
  Expectation set_alignment_expectation =
      EXPECT_CALL(layout_mock, SetAlignment(align));
  Expectation set_attributes =
      EXPECT_CALL(layout_mock, SetAttributes(copied_attributes));
  EXPECT_CALL(*pango_mock, CopyAttributes(attributes))
      .WillOnce(Return(copied_attributes));
  Expectation set_font_description_expectation =
      EXPECT_CALL(layout_mock, SetFontDescription(font_desc));
  Expectation set_width_expectation =
      EXPECT_CALL(layout_mock, SetWidth(width * PANGO_SCALE));
  EXPECT_CALL(*font_spec_mock, GetFontAlignment(font_type))
      .WillOnce(Return(align));
  EXPECT_CALL(*font_spec_mock, GetFontAttributes(font_type))
      .WillOnce(Return(attributes));
  EXPECT_CALL(*font_spec_mock, GetFontDescription(font_type))
      .WillOnce(Return(font_desc));
  Expectation get_pixel_expectation =
      EXPECT_CALL(layout_mock, GetPixelSize())
          .After(set_text_expectation)
          .After(set_alignment_expectation)
          .After(set_attributes)
          .After(set_font_description_expectation)
          .WillOnce(Return(size));
  EXPECT_CALL(*pango_mock, AttributesUnref(copied_attributes))
      .After(set_attributes);

  Size actual_size = text_renderer.TextRenderer::GetMultiLinePixelSizeInternal(
      font_type, text, width, &layout_mock);
  EXPECT_EQ(actual_size.width, size.width);
  EXPECT_EQ(actual_size.height, size.height);
}

TEST_F(TextRendererTest, RenderTextTest) {
  FontSpecMock *font_spec_mock = new FontSpecMock();
  TextRenderer text_renderer(font_spec_mock);
  PangoWrapperMock *pango_mock = SetUpPangoMock(&text_renderer);
  PangoLayoutWrapperMock layout_mock;

  const std::string text = "hogehoge";
  Rect rect(10, 20, 30, 40);
  Size size(12, 34);
  PangoAlignment align = PANGO_ALIGN_CENTER;
  PangoAttrList *attributes = reinterpret_cast<PangoAttrList *>(0xabcdef01);
  PangoAttrList *copied_attributes =
      reinterpret_cast<PangoAttrList *>(0x237492);
  PangoFontDescription *font_desc =
      reinterpret_cast<PangoFontDescription *>(0x01abcdef);
  FontSpecInterface::FONT_TYPE font_type =
      FontSpecInterface::FONTSET_FOOTER_LABEL;

  EXPECT_CALL(*font_spec_mock, GetFontAlignment(font_type))
      .WillRepeatedly(Return(align));
  EXPECT_CALL(*font_spec_mock, GetFontAttributes(font_type))
      .WillRepeatedly(Return(attributes));
  EXPECT_CALL(*font_spec_mock, GetFontDescription(font_type))
      .WillRepeatedly(Return(font_desc));

  Expectation set_alignment_expectation =
      EXPECT_CALL(layout_mock, SetAlignment(align));
  Expectation set_attributes =
      EXPECT_CALL(layout_mock, SetAttributes(copied_attributes));
  EXPECT_CALL(*pango_mock, CopyAttributes(attributes))
      .WillOnce(Return(copied_attributes));
  Expectation set_font_description_expectation =
      EXPECT_CALL(layout_mock, SetFontDescription(font_desc));

  Expectation set_text_expectation = EXPECT_CALL(layout_mock, SetText(text));
  Expectation set_width_expectation =
      EXPECT_CALL(layout_mock, SetWidth(rect.size.width * PANGO_SCALE));
  Expectation set_height_expectation =
      EXPECT_CALL(layout_mock, SetHeight(rect.size.height * PANGO_SCALE));
  Expectation get_pixel_size_expectation =
      EXPECT_CALL(layout_mock, GetPixelSize()).WillOnce(Return(size));
  EXPECT_CALL(*pango_mock, AttributesUnref(copied_attributes))
      .After(set_attributes);

  // Vertical centering
  const int expected_x = rect.origin.x * PANGO_SCALE;
  const int expected_y =
      (rect.origin.y + (rect.size.height - size.height) / 2) * PANGO_SCALE;

  Expectation rendering_expectation =
      EXPECT_CALL(*pango_mock,
                  RendererDrawLayout(&layout_mock, expected_x, expected_y))
          .After(set_alignment_expectation)
          .After(set_attributes)
          .After(set_font_description_expectation)
          .After(set_text_expectation)
          .After(set_width_expectation)
          .After(set_height_expectation)
          .After(get_pixel_size_expectation);

  text_renderer.RenderTextInternal(text, rect, font_type, &layout_mock);
}

TEST_F(TextRendererTest, ReloadFontConfigTest) {
  FontSpecMock *font_spec_mock = new FontSpecMock();
  TextRenderer text_renderer(font_spec_mock);
  constexpr char kDummyFontDescription[] = "Foo,Bar,Baz";

  EXPECT_CALL(*font_spec_mock, Reload(kDummyFontDescription));
  text_renderer.ReloadFontConfig(kDummyFontDescription);
}

}  // namespace gtk
}  // namespace renderer
}  // namespace mozc
