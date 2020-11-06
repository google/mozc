// Copyright 2010-2020, Google Inc.
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

#include "renderer/unix/font_spec.h"

#include "base/logging.h"
#include "protocol/renderer_style.pb.h"
#include "renderer/renderer_style_handler.h"
#include "renderer/unix/const.h"

namespace mozc {
namespace renderer {
namespace gtk {

namespace {
PangoAttrList *CreateAttrListByScaleColor(const RGBA &color, double scale) {
  PangoAttrList *attributes = pango_attr_list_new();
  pango_attr_list_insert(attributes, pango_attr_scale_new(scale));
  pango_attr_list_insert(
      attributes, pango_attr_foreground_new(color.red << 8, color.green << 8,
                                            color.blue << 8));
  return attributes;
}

RGBA RGBAColor2RGBA(const RendererStyle::RGBAColor &rgba) {
  RGBA result = {static_cast<uint8>(rgba.r()), static_cast<uint8>(rgba.g()),
                 static_cast<uint8>(rgba.b()),
                 static_cast<uint8>(rgba.a() * 255.0)};
  return result;
}
}  // namespace

struct FontInfo {
  PangoAlignment align;
  PangoAttrList *attributes;
  PangoFontDescription *font;
};

FontSpec::FontSpec(GtkWrapperInterface *gtk)
    : fonts_(SIZE_OF_FONT_TYPE), is_initialized_(false), gtk_(gtk) {
  LoadFontSpec(kDefaultFontDescription);
}

FontSpec::~FontSpec() {
  if (is_initialized_) {
    ReleaseFontSpec();
  }
}

void FontSpec::Reload(const string &font_description) {
  if (!is_initialized_) {
    ReleaseFontSpec();
  }
  LoadFontSpec(font_description);
}

void FontSpec::ReleaseFontSpec() {
  if (!is_initialized_) {
    DLOG(ERROR) << "Font spec is not initilaized.";
    return;
  }
  for (uint32 i = 0; i < fonts_.size(); ++i) {
    pango_font_description_free(fonts_[i].font);
    pango_attr_list_unref(fonts_[i].attributes);
  }
}

void FontSpec::LoadFontSpec(const string &font_description) {
  if (is_initialized_) {
    LOG(WARNING) << "Font spec is already loaded. reloading...";
    ReleaseFontSpec();
  }

  RendererStyle style;
  RendererStyleHandler::GetRendererStyle(&style);
  const RendererStyle::InfolistStyle infostyle = style.infolist_style();

  // Set alignments.
  fonts_[FONTSET_CANDIDATE].align = PANGO_ALIGN_LEFT;
  fonts_[FONTSET_DESCRIPTION].align = PANGO_ALIGN_LEFT;
  fonts_[FONTSET_FOOTER_INDEX].align = PANGO_ALIGN_RIGHT;
  fonts_[FONTSET_FOOTER_LABEL].align = PANGO_ALIGN_CENTER;
  fonts_[FONTSET_FOOTER_SUBLABEL].align = PANGO_ALIGN_CENTER;
  fonts_[FONTSET_SHORTCUT].align = PANGO_ALIGN_CENTER;
  fonts_[FONTSET_INFOLIST_CAPTION].align = PANGO_ALIGN_LEFT;
  fonts_[FONTSET_INFOLIST_TITLE].align = PANGO_ALIGN_LEFT;
  fonts_[FONTSET_INFOLIST_DESCRIPTION].align = PANGO_ALIGN_LEFT;

  // Set font descriptions.
  fonts_[FONTSET_CANDIDATE].font =
      pango_font_description_from_string(font_description.c_str());
  fonts_[FONTSET_DESCRIPTION].font =
      pango_font_description_from_string(font_description.c_str());
  fonts_[FONTSET_FOOTER_INDEX].font =
      pango_font_description_from_string(font_description.c_str());
  fonts_[FONTSET_FOOTER_LABEL].font =
      pango_font_description_from_string(font_description.c_str());
  fonts_[FONTSET_FOOTER_SUBLABEL].font =
      pango_font_description_from_string(font_description.c_str());
  fonts_[FONTSET_SHORTCUT].font =
      pango_font_description_from_string(font_description.c_str());
  fonts_[FONTSET_INFOLIST_CAPTION].font =
      pango_font_description_from_string(font_description.c_str());
  fonts_[FONTSET_INFOLIST_TITLE].font =
      pango_font_description_from_string(font_description.c_str());
  fonts_[FONTSET_INFOLIST_DESCRIPTION].font =
      pango_font_description_from_string(font_description.c_str());

  // Set color and scales.
  fonts_[FONTSET_CANDIDATE].attributes =
      CreateAttrListByScaleColor(kDefaultColor, PANGO_SCALE_MEDIUM);
  fonts_[FONTSET_DESCRIPTION].attributes =
      CreateAttrListByScaleColor(kDescriptionColor, PANGO_SCALE_MEDIUM);
  fonts_[FONTSET_FOOTER_INDEX].attributes =
      CreateAttrListByScaleColor(kFooterIndexColor, PANGO_SCALE_SMALL);
  fonts_[FONTSET_FOOTER_LABEL].attributes =
      CreateAttrListByScaleColor(kFooterLabelColor, PANGO_SCALE_SMALL);
  fonts_[FONTSET_FOOTER_SUBLABEL].attributes =
      CreateAttrListByScaleColor(kFooterSubLabelColor, PANGO_SCALE_SMALL);
  fonts_[FONTSET_SHORTCUT].attributes =
      CreateAttrListByScaleColor(kShortcutColor, PANGO_SCALE_MEDIUM);
  fonts_[FONTSET_INFOLIST_CAPTION].attributes = CreateAttrListByScaleColor(
      RGBAColor2RGBA(infostyle.caption_style().foreground_color()),
      PANGO_SCALE_MEDIUM);
  fonts_[FONTSET_INFOLIST_TITLE].attributes = CreateAttrListByScaleColor(
      RGBAColor2RGBA(infostyle.title_style().foreground_color()),
      PANGO_SCALE_MEDIUM);
  fonts_[FONTSET_INFOLIST_DESCRIPTION].attributes = CreateAttrListByScaleColor(
      RGBAColor2RGBA(infostyle.description_style().foreground_color()),
      PANGO_SCALE_MEDIUM);
  is_initialized_ = true;
}

PangoAlignment FontSpec::GetFontAlignment(FONT_TYPE font_type) const {
  DCHECK(0 <= font_type && font_type < SIZE_OF_FONT_TYPE);
  return fonts_[font_type].align;
}

PangoAttrList *FontSpec::GetFontAttributes(FONT_TYPE font_type) const {
  DCHECK(0 <= font_type && font_type < SIZE_OF_FONT_TYPE);
  return fonts_[font_type].attributes;
}

const PangoFontDescription *FontSpec::GetFontDescription(
    FONT_TYPE font_type) const {
  DCHECK(0 <= font_type && font_type < SIZE_OF_FONT_TYPE);
  return fonts_[font_type].font;
}

}  // namespace gtk
}  // namespace renderer
}  // namespace mozc
