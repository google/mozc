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

#include "renderer/win32/text_renderer.h"

#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
#include <atlbase.h>
#include <atlcom.h>
#include <d2d1.h>
#include <dwrite.h>
#include <objbase.h>

#include <memory>

#include "base/logging.h"
#include "base/system_util.h"
#include "protocol/renderer_style.pb.h"
#include "renderer/renderer_style_handler.h"

namespace mozc {
namespace renderer {
namespace win32 {

using ATL::CComPtr;
using ::mozc::renderer::RendererStyle;
using ::mozc::renderer::RendererStyleHandler;
using WTL::CDC;
using WTL::CDCHandle;
using WTL::CFont;
using WTL::CFontHandle;
using WTL::CLogFont;
using WTL::CPoint;
using WTL::CRect;
using WTL::CSize;

namespace {

WTL::CRect ToCRect(const Rect &rect) {
  return WTL::CRect(rect.Left(), rect.Top(), rect.Right(), rect.Bottom());
}

COLORREF GetTextColor(TextRenderer::FONT_TYPE type) {
  switch (type) {
    case TextRenderer::FONTSET_SHORTCUT:
      return RGB(0x61, 0x61, 0x61);
    case TextRenderer::FONTSET_CANDIDATE:
      return RGB(0x00, 0x00, 0x00);
    case TextRenderer::FONTSET_DESCRIPTION:
      return RGB(0x88, 0x88, 0x88);
    case TextRenderer::FONTSET_FOOTER_INDEX:
      return RGB(0x4c, 0x4c, 0x4c);
    case TextRenderer::FONTSET_FOOTER_LABEL:
      return RGB(0x4c, 0x4c, 0x4c);
    case TextRenderer::FONTSET_FOOTER_SUBLABEL:
      return RGB(0xA7, 0xA7, 0xA7);
  }

  // TODO(horo): Not only infolist fonts but also candidate fonts
  //             should be created from RendererStyle
  RendererStyle style;
  RendererStyleHandler::GetRendererStyle(&style);
  const auto &infostyle = style.infolist_style();
  switch (type) {
    case TextRenderer::FONTSET_INFOLIST_CAPTION:
      return RGB(infostyle.caption_style().foreground_color().r(),
                 infostyle.caption_style().foreground_color().g(),
                 infostyle.caption_style().foreground_color().b());
    case TextRenderer::FONTSET_INFOLIST_TITLE:
      return RGB(infostyle.title_style().foreground_color().r(),
                 infostyle.title_style().foreground_color().g(),
                 infostyle.title_style().foreground_color().b());
    case TextRenderer::FONTSET_INFOLIST_DESCRIPTION:
      return RGB(infostyle.description_style().foreground_color().r(),
                 infostyle.description_style().foreground_color().g(),
                 infostyle.description_style().foreground_color().b());
  }

  LOG(DFATAL) << "Unknown type: " << type;
  return RGB(0, 0, 0);
}

CLogFont GetLogFont(TextRenderer::FONT_TYPE type) {
  switch (type) {
    case TextRenderer::FONTSET_SHORTCUT: {
      CLogFont font;
      font.SetMessageBoxFont();
      font.MakeLarger(3);
      font.lfWeight = FW_BOLD;
      return font;
    }
    case TextRenderer::FONTSET_CANDIDATE: {
      CLogFont font;
      font.SetMessageBoxFont();
      font.MakeLarger(3);
      font.lfWeight = FW_NORMAL;
      return font;
    }
    case TextRenderer::FONTSET_DESCRIPTION:
    case TextRenderer::FONTSET_FOOTER_INDEX:
    case TextRenderer::FONTSET_FOOTER_LABEL:
    case TextRenderer::FONTSET_FOOTER_SUBLABEL: {
      CLogFont font;
      font.SetMessageBoxFont();
      font.lfWeight = FW_NORMAL;
      return font;
    }
  }

  // TODO(horo): Not only infolist fonts but also candidate fonts
  //             should be created from RendererStyle
  RendererStyle style;
  RendererStyleHandler::GetRendererStyle(&style);
  const auto &infostyle = style.infolist_style();
  switch (type) {
    case TextRenderer::FONTSET_INFOLIST_CAPTION: {
      CLogFont font;
      font.SetMessageBoxFont();
      font.lfHeight = -infostyle.caption_style().font_size();
      return font;
    }
    case TextRenderer::FONTSET_INFOLIST_TITLE: {
      CLogFont font;
      font.SetMessageBoxFont();
      font.lfHeight = -infostyle.title_style().font_size();
      return font;
    }
    case TextRenderer::FONTSET_INFOLIST_DESCRIPTION: {
      CLogFont font;
      font.SetMessageBoxFont();
      font.lfHeight = -infostyle.description_style().font_size();
      return font;
    }
  }

  LOG(DFATAL) << "Unknown type: " << type;
  CLogFont font;
  font.SetMessageBoxFont();
  return font;
}

DWORD GetGdiDrawTextStyle(TextRenderer::FONT_TYPE type) {
  switch (type) {
    case TextRenderer::FONTSET_CANDIDATE:
      return DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX;
    case TextRenderer::FONTSET_DESCRIPTION:
      return DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX;
    case TextRenderer::FONTSET_FOOTER_INDEX:
      return DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX;
    case TextRenderer::FONTSET_FOOTER_LABEL:
      return DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX;
    case TextRenderer::FONTSET_FOOTER_SUBLABEL:
      return DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX;
    case TextRenderer::FONTSET_SHORTCUT:
      return DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX;
    case TextRenderer::FONTSET_INFOLIST_CAPTION:
      return DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX;
    case TextRenderer::FONTSET_INFOLIST_TITLE:
      return DT_LEFT | DT_SINGLELINE | DT_WORDBREAK | DT_EDITCONTROL |
             DT_NOPREFIX;
    case TextRenderer::FONTSET_INFOLIST_DESCRIPTION:
      return DT_LEFT | DT_WORDBREAK | DT_EDITCONTROL | DT_NOPREFIX;
    default:
      LOG(DFATAL) << "Unknown type: " << type;
      return 0;
  }
}

class GdiTextRenderer : public TextRenderer {
 public:
  GdiTextRenderer() : render_info_(new RenderInfo[SIZE_OF_FONT_TYPE]) {
    mem_dc_.CreateCompatibleDC();
    OnThemeChanged();
  }

  virtual ~GdiTextRenderer() {}

 private:
  // TextRenderer overrides:
  virtual void OnThemeChanged() {
    // delete old fonts
    for (size_t i = 0; i < SIZE_OF_FONT_TYPE; ++i) {
      if (!render_info_[i].font.IsNull()) {
        render_info_[i].font.DeleteObject();
      }
    }

    for (size_t i = 0; i < SIZE_OF_FONT_TYPE; ++i) {
      const auto font_type = static_cast<FONT_TYPE>(i);
      const auto &log_font = GetLogFont(font_type);
      render_info_[i].style = GetGdiDrawTextStyle(font_type);
      render_info_[i].font.CreateFontIndirectW(&log_font);
      render_info_[i].color = GetTextColor(font_type);
    }
  }

  virtual Size MeasureString(FONT_TYPE font_type,
                             const std::wstring &str) const {
    const auto previous_font = mem_dc_.SelectFont(render_info_[font_type].font);
    CRect rect;
    mem_dc_.DrawTextW(str.c_str(), str.length(), &rect,
                      DT_NOPREFIX | DT_LEFT | DT_SINGLELINE | DT_CALCRECT);
    mem_dc_.SelectFont(previous_font);
    return Size(rect.Width(), rect.Height());
  }

  virtual Size MeasureStringMultiLine(FONT_TYPE font_type,
                                      const std::wstring &str,
                                      const int width) const {
    const auto previous_font = mem_dc_.SelectFont(render_info_[font_type].font);
    CRect rect(0, 0, width, 0);
    mem_dc_.DrawTextW(str.c_str(), str.length(), &rect,
                      DT_NOPREFIX | DT_LEFT | DT_WORDBREAK | DT_CALCRECT);
    mem_dc_.SelectFont(previous_font);
    return Size(rect.Width(), rect.Height());
  }

  virtual void RenderText(CDCHandle dc, const std::wstring &text,
                          const Rect &rect, FONT_TYPE font_type) const {
    std::vector<TextRenderingInfo> infolist;
    infolist.push_back(TextRenderingInfo(text, rect));
    RenderTextList(dc, infolist, font_type);
  }

  virtual void RenderTextList(
      CDCHandle dc, const std::vector<TextRenderingInfo> &display_list,
      FONT_TYPE font_type) const {
    const auto &render_info = render_info_[font_type];
    const auto old_font = dc.SelectFont(render_info.font);
    const auto previous_color = dc.SetTextColor(render_info.color);
    for (auto it = display_list.begin(); it != display_list.end(); ++it) {
      const auto &info = *it;
      CRect rect(info.rect.Left(), info.rect.Top(), info.rect.Right(),
                 info.rect.Bottom());
      const auto &text = info.text;
      dc.DrawTextW(text.data(), text.size(), &rect, render_info.style);
    }
    dc.SetTextColor(previous_color);
    dc.SelectFont(old_font);
  }

  struct RenderInfo {
    RenderInfo() : color(0), style(0) {}
    COLORREF color;
    DWORD style;
    CFont font;
  };
  std::unique_ptr<RenderInfo[]> render_info_;
  mutable CDC mem_dc_;

  DISALLOW_COPY_AND_ASSIGN(GdiTextRenderer);
};

class DirectWriteTextRenderer : public TextRenderer {
 public:
  static DirectWriteTextRenderer *Create() {
    HRESULT hr = S_OK;
    CComPtr<ID2D1Factory> d2d_factory;
    hr = ::D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                             __uuidof(ID2D1Factory), nullptr,
                             reinterpret_cast<void **>(&d2d_factory));
    if (FAILED(hr)) {
      return nullptr;
    }
    CComPtr<IDWriteFactory> dwrite_factory;
    hr = ::DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                               __uuidof(IDWriteFactory),
                               reinterpret_cast<IUnknown **>(&dwrite_factory));
    if (FAILED(hr)) {
      return nullptr;
    }
    CComPtr<IDWriteGdiInterop> interop;
    hr = dwrite_factory->GetGdiInterop(&interop);
    if (FAILED(hr)) {
      return nullptr;
    }
    return new DirectWriteTextRenderer(d2d_factory, dwrite_factory, interop);
  }
  virtual ~DirectWriteTextRenderer() {}

 private:
  DirectWriteTextRenderer(ID2D1Factory *d2d2_factory,
                          IDWriteFactory *dwrite_factory,
                          IDWriteGdiInterop *dwrite_interop)
      : d2d2_factory_(d2d2_factory),
        dwrite_factory_(dwrite_factory),
        dwrite_interop_(dwrite_interop) {
    OnThemeChanged();
  }

  // TextRenderer overrides:
  virtual void OnThemeChanged() {
    // delete old fonts
    render_info_.clear();
    render_info_.resize(SIZE_OF_FONT_TYPE);

    for (size_t i = 0; i < SIZE_OF_FONT_TYPE; ++i) {
      const auto font_type = static_cast<FONT_TYPE>(i);
      const auto &log_font = GetLogFont(font_type);
      render_info_[i].color = GetTextColor(font_type);
      render_info_[i].format = CreateFormat(log_font);
      render_info_[i].format_to_render = CreateFormat(log_font);
      const auto style = GetGdiDrawTextStyle(font_type);
      const auto render_font = render_info_[i].format_to_render;
      if ((style & DT_VCENTER) == DT_VCENTER) {
        render_font->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
      }
      if ((style & DT_LEFT) == DT_LEFT) {
        render_font->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
      }
      if ((style & DT_CENTER) == DT_CENTER) {
        render_font->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
      }
      if ((style & DT_LEFT) == DT_RIGHT) {
        render_font->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
      }
    }
  }

  // Retrieves the bounding box for a given string.
  virtual Size MeasureString(FONT_TYPE font_type,
                             const std::wstring &str) const {
    return MeasureStringImpl(font_type, str, 0, false);
  }

  virtual Size MeasureStringMultiLine(FONT_TYPE font_type,
                                      const std::wstring &str,
                                      const int width) const {
    return MeasureStringImpl(font_type, str, width, true);
  }

  virtual void RenderText(CDCHandle dc, const std::wstring &text,
                          const Rect &rect, FONT_TYPE font_type) const {
    std::vector<TextRenderingInfo> infolist;
    infolist.push_back(TextRenderingInfo(text, rect));
    RenderTextList(dc, infolist, font_type);
  }

  virtual void RenderTextList(
      CDCHandle dc, const std::vector<TextRenderingInfo> &display_list,
      FONT_TYPE font_type) const {
    const size_t kMaxTrial = 3;
    size_t trial = 0;
    while (true) {
      CreateRenderTargetIfNecessary();
      if (dc_render_target_ == nullptr) {
        // This is not a recoverable error.
        return;
      }
      const HRESULT hr = RenderTextListImpl(dc, display_list, font_type);
      if (hr == D2DERR_RECREATE_TARGET && trial < kMaxTrial) {
        // This is a recoverable error just by recreating the render target.
        dc_render_target_.Release();
        ++trial;
        continue;
      }
      // For other error codes (including S_OK and S_FALSE), or if we exceed the
      // maximum number of trials, we simply accept the result here.
      return;
    }
  }

  HRESULT RenderTextListImpl(CDCHandle dc,
                             const std::vector<TextRenderingInfo> &display_list,
                             FONT_TYPE font_type) const {
    CRect total_rect;
    for (const auto &item : display_list) {
      const auto &item_rect = ToCRect(item.rect);
      total_rect.right = std::max(total_rect.right, item_rect.right);
      total_rect.bottom = std::max(total_rect.bottom, item_rect.bottom);
    }
    HRESULT hr = S_OK;
    hr = dc_render_target_->BindDC(dc, &total_rect);
    if (FAILED(hr)) {
      return hr;
    }
    CComPtr<ID2D1SolidColorBrush> brush;
    hr = dc_render_target_->CreateSolidColorBrush(
        ToD2DColor(render_info_[font_type].color), &brush);
    if (FAILED(hr)) {
      return hr;
    }
    D2D1_DRAW_TEXT_OPTIONS option = D2D1_DRAW_TEXT_OPTIONS_NONE;
    if (SystemUtil::IsWindows8_1OrLater()) {
      option |= D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT;
    }
    dc_render_target_->BeginDraw();
    dc_render_target_->SetTransform(D2D1::Matrix3x2F::Identity());
    for (size_t i = 0; i < display_list.size(); ++i) {
      const auto &item = display_list[i];
      const D2D1_RECT_F render_rect = {
          static_cast<float>(item.rect.Left()),
          static_cast<float>(item.rect.Top()),
          static_cast<float>(item.rect.Right()),
          static_cast<float>(item.rect.Bottom()),
      };
      dc_render_target_->DrawText(item.text.data(), item.text.size(),
                                  render_info_[font_type].format_to_render,
                                  render_rect, brush, option);
    }
    return dc_render_target_->EndDraw();
  }

  Size MeasureStringImpl(FONT_TYPE font_type, const std::wstring &str,
                         const int width, bool use_width) const {
    HRESULT hr = S_OK;
    const FLOAT kLayoutLimit = 100000.0f;
    CComPtr<IDWriteTextLayout> layout;
    hr = dwrite_factory_->CreateTextLayout(
        str.data(), str.size(), render_info_[font_type].format,
        (use_width ? width : kLayoutLimit), kLayoutLimit, &layout);
    if (FAILED(hr)) {
      return Size();
    }
    DWRITE_TEXT_METRICS metrix = {};
    hr = layout->GetMetrics(&metrix);
    if (FAILED(hr)) {
      return Size();
    }
    return Size(ceilf(metrix.widthIncludingTrailingWhitespace),
                ceilf(metrix.height));
  }

  static D2D1_COLOR_F ToD2DColor(COLORREF color_ref) {
    D2D1_COLOR_F color;
    color.a = 1.0;
    color.r = GetRValue(color_ref) / 255.0f;
    color.g = GetGValue(color_ref) / 255.0f;
    color.b = GetBValue(color_ref) / 255.0f;
    return color;
  }

  CComPtr<IDWriteTextFormat> CreateFormat(CLogFont logfont) {
    HRESULT hr = S_OK;
    CComPtr<IDWriteFont> font;
    hr = dwrite_interop_->CreateFontFromLOGFONT(&logfont, &font);
    if (FAILED(hr)) {
      return nullptr;
    }
    CComPtr<IDWriteFontFamily> font_family;
    hr = font->GetFontFamily(&font_family);
    if (FAILED(hr)) {
      return nullptr;
    }
    CComPtr<IDWriteLocalizedStrings> localized_family_names;
    hr = font_family->GetFamilyNames(&localized_family_names);
    if (FAILED(hr)) {
      return nullptr;
    }
    UINT32 length = 0;
    hr = localized_family_names->GetStringLength(0, &length);
    if (FAILED(hr)) {
      return nullptr;
    }
    length += 1;  // for NUL.
    std::unique_ptr<wchar_t[]> family_name(new wchar_t[length]);
    hr = localized_family_names->GetString(0, family_name.get(), length);
    if (FAILED(hr)) {
      return nullptr;
    }
    auto font_size = logfont.lfHeight;
    if (font_size < 0) {
      font_size = -font_size;
    } else {
      DWRITE_FONT_METRICS font_metrix = {};
      font->GetMetrics(&font_metrix);
      const auto cell_height =
          static_cast<float>(font_metrix.ascent + font_metrix.descent) /
          font_metrix.designUnitsPerEm;
      font_size /= cell_height;
    }

    wchar_t locale_name[LOCALE_NAME_MAX_LENGTH] = {};
    if (::GetUserDefaultLocaleName(locale_name, arraysize(locale_name)) == 0) {
      return nullptr;
    }

    CComPtr<IDWriteTextFormat> format;
    hr = dwrite_factory_->CreateTextFormat(
        family_name.get(), nullptr, font->GetWeight(), font->GetStyle(),
        font->GetStretch(), font_size, locale_name, &format);
    return format;
  }

  void CreateRenderTargetIfNecessary() const {
    if (dc_render_target_) {
      return;
    }
    const auto property = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE),
        0, 0, D2D1_RENDER_TARGET_USAGE_NONE, D2D1_FEATURE_LEVEL_DEFAULT);
    d2d2_factory_->CreateDCRenderTarget(&property, &dc_render_target_);
  }

  struct RenderInfo {
    RenderInfo() : color(0) {}
    COLORREF color;
    CComPtr<IDWriteTextFormat> format;
    CComPtr<IDWriteTextFormat> format_to_render;
  };

  CComPtr<ID2D1Factory> d2d2_factory_;
  CComPtr<IDWriteFactory> dwrite_factory_;
  mutable CComPtr<ID2D1DCRenderTarget> dc_render_target_;
  CComPtr<IDWriteGdiInterop> dwrite_interop_;
  std::vector<RenderInfo> render_info_;

  DISALLOW_COPY_AND_ASSIGN(DirectWriteTextRenderer);
};

}  // namespace

TextRenderer::~TextRenderer() {}

// static
TextRenderer *TextRenderer::Create() {
  // In some environments, DirectWrite cannot render characters in the
  // candidate window or even worse may cause crash.  As a workaround,
  // we try to use DirectWrite only on Windows 8.1 and later.
  // TODO(yukawa): Reactivate the following code for older OSes when
  // the root cause of b/23803925 is identified.
  if (SystemUtil::IsWindows8_1OrLater()) {
    auto *dwrite_text_renderer = DirectWriteTextRenderer::Create();
    if (dwrite_text_renderer != nullptr) {
      return dwrite_text_renderer;
    }
  }
  return new GdiTextRenderer();
}

}  // namespace win32
}  // namespace renderer
}  // namespace mozc
