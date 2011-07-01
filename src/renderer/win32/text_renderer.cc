// Copyright 2010-2011, Google Inc.
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

#include "renderer/win32/text_renderer.h"

namespace mozc {
namespace renderer {
namespace win32 {
using WTL::CLogFont;
using WTL::CPoint;
using WTL::CRect;
using WTL::CSize;

namespace {
// Font color scheme
const COLORREF kShortcutColor = RGB(0x61, 0x61, 0x61);
const COLORREF kDefaultColor = RGB(0x00, 0x00, 0x00);
const COLORREF kDescriptionColor = RGB(0x88, 0x88, 0x88);
const COLORREF kFooterIndexColor = RGB(0x4c, 0x4c, 0x4c);
const COLORREF kFooterLabelColor = RGB(0x4c, 0x4c, 0x4c);
const COLORREF kFooterSubLabelColor = RGB(0xA7, 0xA7, 0xA7);
}  // anonymous namespace

struct FontInfo {
  FontInfo() : color(0), style(0) {}
  COLORREF color;
  DWORD style;
  CFont font;
};

TextRenderer::TextRenderer()
  : fonts_(new FontInfo[SIZE_OF_FONT_TYPE]) {
  mem_dc_.CreateCompatibleDC();
}

TextRenderer::~TextRenderer() {}

void TextRenderer::Init() {
  // delete old fonts
  for (size_t i = 0; i < SIZE_OF_FONT_TYPE; ++i) {
    if (!fonts_[i].font.IsNull()) {
      fonts_[i].font.DeleteObject();
    }
  }

  CLogFont main_font;
  // TODO(yukawa): verify the font can render U+005C as a yen sign.
  //               (http://b/1992773)
  main_font.SetMessageBoxFont();
  main_font.MakeLarger(3);
  main_font.lfWeight = FW_NORMAL;
  fonts_[FONTSET_CANDIDATE].font.CreateFontIndirectW(&main_font);
  main_font.lfWeight = FW_BOLD;
  fonts_[FONTSET_SHORTCUT].font.CreateFontIndirectW(&main_font);

  CLogFont smaller_font;
  // TODO(yukawa): to confirm the font can render Yen-mark. (http://b/1992773)
  smaller_font.SetMessageBoxFont();
  smaller_font.lfWeight = FW_NORMAL;
  fonts_[FONTSET_DESCRIPTION].font.CreateFontIndirectW(&smaller_font);
  fonts_[FONTSET_FOOTER_INDEX].font.CreateFontIndirectW(&smaller_font);
  fonts_[FONTSET_FOOTER_LABEL].font.CreateFontIndirectW(&smaller_font);
  fonts_[FONTSET_FOOTER_SUBLABEL].font.CreateFontIndirectW(&smaller_font);

  const DWORD common_style = DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX;
  fonts_[FONTSET_CANDIDATE].style = DT_LEFT | common_style;
  fonts_[FONTSET_DESCRIPTION].style = DT_LEFT | common_style;
  fonts_[FONTSET_FOOTER_INDEX].style = DT_RIGHT | common_style;
  fonts_[FONTSET_FOOTER_LABEL].style = DT_CENTER | common_style;
  fonts_[FONTSET_FOOTER_SUBLABEL].style = DT_CENTER | common_style;
  fonts_[FONTSET_SHORTCUT].style = DT_CENTER | common_style;

  fonts_[FONTSET_CANDIDATE].color = kDefaultColor;
  fonts_[FONTSET_DESCRIPTION].color = kDescriptionColor;
  fonts_[FONTSET_FOOTER_INDEX].color = kFooterIndexColor;
  fonts_[FONTSET_FOOTER_LABEL].color = kFooterLabelColor;
  fonts_[FONTSET_FOOTER_SUBLABEL].color = kFooterSubLabelColor;
  fonts_[FONTSET_SHORTCUT].color = kShortcutColor;
}

// Retrive the font handle
CFontHandle TextRenderer::GetFont(FONT_TYPE font_type) const {
  DCHECK(0 <= font_type && font_type < SIZE_OF_FONT_TYPE);
  return fonts_[font_type].font.m_hFont;
}

// Retrive the font color
COLORREF TextRenderer::GetFontColor(FONT_TYPE font_type) const {
  DCHECK(0 <= font_type && font_type < SIZE_OF_FONT_TYPE);
  return fonts_[font_type].color;
}

// Retrive the font style
DWORD TextRenderer::GetFontStyle(FONT_TYPE font_type) const {
  DCHECK(0 <= font_type && font_type < SIZE_OF_FONT_TYPE);
  return fonts_[font_type].style;
}

// Retrive the bounding box for a given string.
Size TextRenderer::MeasureString(
    FONT_TYPE font_type, const wstring &str) const {
  mem_dc_.SelectFont(GetFont(font_type));
  CRect rect;
  mem_dc_.DrawTextW(str.c_str(), str.length(), &rect,
                    DT_NOPREFIX | DT_LEFT | DT_SINGLELINE | DT_CALCRECT);
  return Size(rect.Size());
}

void TextRenderer::RenderText(CDCHandle dc, const wstring &text,
                              const Rect &rect, FONT_TYPE font_type) const {
  CFontHandle old_font = dc.SelectFont(GetFont(font_type));
  CRect temp_rect = rect.ToCRect();
  const COLORREF previous_color = dc.SetTextColor(GetFontColor(font_type));
  dc.DrawTextW(text.c_str(), text.size(), &temp_rect,
               GetFontStyle(font_type));
  dc.SelectFont(old_font);
  dc.SetTextColor(previous_color);
}

void TextRenderer::RenderText(CDCHandle dc,
                              const vector<TextRenderingInfo> &display_list,
                              FONT_TYPE font_type) const {
  CFontHandle old_font = dc.SelectFont(GetFont(font_type));
  const COLORREF previous_color =
      dc.SetTextColor(GetFontColor(font_type));
  for (vector<TextRenderingInfo>::const_iterator i = display_list.begin();
       i != display_list.end(); ++i) {
    const TextRenderingInfo &rendering_info = *i;
    CRect temp_rect = rendering_info.rect.ToCRect();
    const wstring &text = rendering_info.text;
    dc.DrawTextW(text.c_str(), text.size(), &temp_rect,
                 GetFontStyle(font_type));
  }
  dc.SetTextColor(previous_color);
  dc.SelectFont(old_font);
}
}  // namespace win32
}  // namespace renderer
}  // namespace mozc
