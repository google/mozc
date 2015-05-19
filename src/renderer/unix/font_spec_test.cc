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

#include "base/scoped_ptr.h"
#include "renderer/renderer_style.pb.h"
#include "renderer/renderer_style_handler.h"
#include "renderer/unix/const.h"
#include "renderer/unix/font_spec.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace renderer {
namespace gtk {

namespace {
RGBA RGBAColor2RGBA(const RendererStyle::RGBAColor &rgba) {
  RGBA result = {
    static_cast<uint8>(rgba.r()),
    static_cast<uint8>(rgba.g()),
    static_cast<uint8>(rgba.b()),
    static_cast<uint8>(rgba.a() * 255.0)
  };
  return result;
}

}  // namespace

class FontSpecTest : public testing::Test {
 protected:
  virtual void SetUp() {
    font_spec_.reset(new FontSpec());
  }

  void ExpectAlignment(const PangoAlignment &expected,
                       FontSpecInterface::FONT_TYPE font_type) {
    EXPECT_EQ(expected, font_spec_->GetFontAlignment(font_type));
  }

  void ExpectFontDescription(const string &expected,
                             FontSpecInterface::FONT_TYPE font_type) {
    EXPECT_EQ(expected, pango_font_description_to_string(
        font_spec_->GetFontDescription(font_type)));
  }

  void ExpectFontAttribute(const RGBA &color, double scale,
                           FontSpecInterface::FONT_TYPE font_type) {
    PangoAttrList *attributes = font_spec_->GetFontAttributes(font_type);
    // Check color attribute
    {
      PangoAttrIterator *it = pango_attr_list_get_iterator(attributes);
      gint start, end;
      pango_attr_iterator_range(it, &start, &end);
      PangoAttribute *attribute
          = pango_attr_iterator_get(it, PANGO_ATTR_FOREGROUND);
      ASSERT_TRUE(attribute);
      PangoAttribute *expected_attribute = pango_attr_foreground_new(
          color.red << 8, color.green << 8, color.blue << 8);
      EXPECT_TRUE(pango_attribute_equal(expected_attribute, attribute));
      pango_attr_iterator_destroy(it);
      pango_attribute_destroy(expected_attribute);
    }

    // Check scale attribute
    {
      PangoAttrIterator *it = pango_attr_list_get_iterator(attributes);
      gint start, end;
      pango_attr_iterator_range(it, &start, &end);
      PangoAttribute *attribute = pango_attr_iterator_get(it, PANGO_ATTR_SCALE);
      ASSERT_TRUE(attribute);
      PangoAttribute *expected_attribute = pango_attr_scale_new(scale);
      EXPECT_TRUE(pango_attribute_equal(expected_attribute, attribute));
      pango_attr_iterator_destroy(it);
      pango_attribute_destroy(expected_attribute);
    }
  }

  scoped_ptr<FontSpec> font_spec_;
};

TEST_F(FontSpecTest, AlignTest) {
  ExpectAlignment(PANGO_ALIGN_LEFT, FontSpecInterface::FONTSET_CANDIDATE);
  ExpectAlignment(PANGO_ALIGN_LEFT, FontSpecInterface::FONTSET_DESCRIPTION);
  ExpectAlignment(PANGO_ALIGN_RIGHT, FontSpecInterface::FONTSET_FOOTER_INDEX);
  ExpectAlignment(PANGO_ALIGN_CENTER,
                  FontSpecInterface::FONTSET_FOOTER_LABEL);
  ExpectAlignment(PANGO_ALIGN_CENTER,
                  FontSpecInterface::FONTSET_FOOTER_SUBLABEL);
  ExpectAlignment(PANGO_ALIGN_CENTER, FontSpecInterface::FONTSET_SHORTCUT);
  ExpectAlignment(PANGO_ALIGN_LEFT,
                  FontSpecInterface::FONTSET_INFOLIST_CAPTION);
  ExpectAlignment(PANGO_ALIGN_LEFT, FontSpecInterface::FONTSET_INFOLIST_TITLE);
  ExpectAlignment(PANGO_ALIGN_LEFT,
                  FontSpecInterface::FONTSET_INFOLIST_DESCRIPTION);
}

TEST_F(FontSpecTest, FontDescriptionTest) {
  ExpectFontDescription(kNormalFont, FontSpecInterface::FONTSET_CANDIDATE);
  ExpectFontDescription(kNormalFont, FontSpecInterface::FONTSET_DESCRIPTION);
  ExpectFontDescription(kNormalFont, FontSpecInterface::FONTSET_FOOTER_INDEX);
  ExpectFontDescription(kNormalFont, FontSpecInterface::FONTSET_FOOTER_LABEL);
  ExpectFontDescription(kNormalFont,
                  FontSpecInterface::FONTSET_FOOTER_SUBLABEL);
  ExpectFontDescription(kShortcutFont, FontSpecInterface::FONTSET_SHORTCUT);
  ExpectFontDescription(kNormalFont,
                  FontSpecInterface::FONTSET_INFOLIST_CAPTION);
  ExpectFontDescription(kNormalFont, FontSpecInterface::FONTSET_INFOLIST_TITLE);
  ExpectFontDescription(kNormalFont,
                  FontSpecInterface::FONTSET_INFOLIST_DESCRIPTION);
}

TEST_F(FontSpecTest, AttributeTest) {
  RendererStyle style;
  RendererStyleHandler::GetRendererStyle(&style);
  const RendererStyle::InfolistStyle infostyle = style.infolist_style();

  ExpectFontAttribute(kDefaultColor, PANGO_SCALE_SMALL,
                      FontSpecInterface::FONTSET_CANDIDATE);
  ExpectFontAttribute(kDescriptionColor, PANGO_SCALE_SMALL,
                      FontSpecInterface::FONTSET_DESCRIPTION);
  ExpectFontAttribute(kFooterIndexColor, PANGO_SCALE_X_SMALL,
                      FontSpecInterface::FONTSET_FOOTER_INDEX);
  ExpectFontAttribute(kFooterLabelColor, PANGO_SCALE_X_SMALL,
                      FontSpecInterface::FONTSET_FOOTER_LABEL);
  ExpectFontAttribute(kFooterSubLabelColor, PANGO_SCALE_X_SMALL,
                      FontSpecInterface::FONTSET_FOOTER_SUBLABEL);
  ExpectFontAttribute(kShortcutColor, PANGO_SCALE_SMALL,
                      FontSpecInterface::FONTSET_SHORTCUT);
  ExpectFontAttribute(
      RGBAColor2RGBA(infostyle.title_style().foreground_color()),
      PANGO_SCALE_SMALL, FontSpecInterface::FONTSET_INFOLIST_TITLE);
  ExpectFontAttribute(
      RGBAColor2RGBA(infostyle.title_style().foreground_color()),
      PANGO_SCALE_SMALL, FontSpecInterface::FONTSET_INFOLIST_TITLE);
  ExpectFontAttribute(
      RGBAColor2RGBA(infostyle.description_style().foreground_color()),
      PANGO_SCALE_SMALL, FontSpecInterface::FONTSET_INFOLIST_DESCRIPTION);
}
}  // namespace gtk
}  // namespace renderer
}  // namespace mozc
