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

#include "renderer/win32/text_renderer.h"

#include <atlbase.h>
#include <atlcom.h>
#include <atltypes.h>
#include <d2d1.h>
#include <dwrite.h>
#include <objbase.h>
#include <wil/com.h>
#include <wil/resource.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "absl/types/span.h"
#include "base/coordinates.h"
#include "protocol/renderer_style.pb.h"
#include "renderer/renderer_style_handler.h"
#include "renderer/win32/win32_font_util.h"

namespace mozc {
namespace renderer {
namespace win32 {

using ::mozc::renderer::RendererStyle;
using ::mozc::renderer::RendererStyleHandler;

namespace {

CRect ToCRect(const Rect& rect) {
  return CRect(rect.Left(), rect.Top(), rect.Right(), rect.Bottom());
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
    default:
      break;
  }

  // TODO(horo): Not only infolist fonts but also candidate fonts
  //             should be created from RendererStyle
  RendererStyle style;
  RendererStyleHandler::GetRendererStyle(&style);
  const auto& infostyle = style.infolist_style();
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
    default:
      break;
  }

  LOG(DFATAL) << "Unknown type: " << type;
  return RGB(0, 0, 0);
}

LOGFONT GetLogFont(TextRenderer::FONT_TYPE type) {
  LOGFONT font = GetMessageBoxLogFont();

  switch (type) {
    case TextRenderer::FONTSET_SHORTCUT: {
      font.lfHeight += (font.lfHeight > 0 ? 3 : -3);
      font.lfWeight = FW_BOLD;
      return font;
    }
    case TextRenderer::FONTSET_CANDIDATE: {
      font.lfHeight += (font.lfHeight > 0 ? 3 : -3);
      font.lfWeight = FW_NORMAL;
      return font;
    }
    case TextRenderer::FONTSET_DESCRIPTION:
    case TextRenderer::FONTSET_FOOTER_INDEX:
    case TextRenderer::FONTSET_FOOTER_LABEL:
    case TextRenderer::FONTSET_FOOTER_SUBLABEL: {
      font.lfWeight = FW_NORMAL;
      return font;
    }
    default:
      break;
  }

  // TODO(horo): Not only infolist fonts but also candidate fonts
  //             should be created from RendererStyle
  RendererStyle style;
  RendererStyleHandler::GetRendererStyle(&style);
  const auto& infostyle = style.infolist_style();
  switch (type) {
    case TextRenderer::FONTSET_INFOLIST_CAPTION: {
      font.lfHeight = -infostyle.caption_style().font_size();
      return font;
    }
    case TextRenderer::FONTSET_INFOLIST_TITLE: {
      font.lfHeight = -infostyle.title_style().font_size();
      return font;
    }
    case TextRenderer::FONTSET_INFOLIST_DESCRIPTION: {
      font.lfHeight = -infostyle.description_style().font_size();
      return font;
    }
    default:
      break;
  }

  LOG(DFATAL) << "Unknown type: " << type;
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
  GdiTextRenderer()
      : render_info_(SIZE_OF_FONT_TYPE),
        mem_dc_(::CreateCompatibleDC(nullptr)) {
    OnThemeChanged();
  }

 private:
  struct RenderInfo {
    RenderInfo() : color(0), style(0) {}
    COLORREF color;
    DWORD style;
    wil::unique_hfont font;
  };

  // TextRenderer overrides:
  void OnThemeChanged() override {
    // delete old fonts
    for (size_t i = 0; i < SIZE_OF_FONT_TYPE; ++i) {
      if (render_info_[i].font.is_valid()) {
        render_info_[i].font.reset();
      }
    }

    for (size_t i = 0; i < SIZE_OF_FONT_TYPE; ++i) {
      const auto font_type = static_cast<FONT_TYPE>(i);
      const auto& log_font = GetLogFont(font_type);
      render_info_[i].style = GetGdiDrawTextStyle(font_type);
      render_info_[i].font.reset(::CreateFontIndirectW(&log_font));
      render_info_[i].color = GetTextColor(font_type);
    }
  }

  Size MeasureString(FONT_TYPE font_type,
                     const std::wstring_view str) const override {
    CRect rect;
    {
      const auto previous_font =
          wil::SelectObject(mem_dc_.get(), render_info_[font_type].font.get());
      ::DrawTextW(mem_dc_.get(), str.data(), str.length(), &rect,
                  DT_NOPREFIX | DT_LEFT | DT_SINGLELINE | DT_CALCRECT);
    }
    return Size(rect.Width(), rect.Height());
  }

  Size MeasureStringMultiLine(FONT_TYPE font_type, const std::wstring_view str,
                              const int width) const override {
    CRect rect(0, 0, width, 0);
    {
      const auto previous_font =
          wil::SelectObject(mem_dc_.get(), render_info_[font_type].font.get());
      ::DrawTextW(mem_dc_.get(), str.data(), str.length(), &rect,
                  DT_NOPREFIX | DT_LEFT | DT_WORDBREAK | DT_CALCRECT);
    }
    return Size(rect.Width(), rect.Height());
  }

  void RenderText(HDC dc, const std::wstring_view text, const Rect& rect,
                  FONT_TYPE font_type) const override {
    std::vector<TextRenderingInfo> infolist;
    infolist.emplace_back(std::wstring(text), rect);
    RenderTextList(dc, infolist, font_type);
  }

  void RenderTextList(HDC dc,
                      const absl::Span<const TextRenderingInfo> display_list,
                      FONT_TYPE font_type) const override {
    const auto& render_info = render_info_[font_type];
    const auto old_font = wil::SelectObject(dc, render_info.font.get());
    const auto previous_color = ::SetTextColor(dc, render_info.color);
    for (const TextRenderingInfo& info : display_list) {
      CRect rect = ToCRect(info.rect);
      ::DrawTextW(dc, info.text.data(), info.text.size(), &rect,
                  render_info.style);
    }
    ::SetTextColor(dc, previous_color);
  }

  std::vector<RenderInfo> render_info_;
  wil::unique_hdc mem_dc_;
};

class DirectWriteTextRenderer : public TextRenderer {
 public:
  DirectWriteTextRenderer(
      wil::com_ptr_nothrow<ID2D1Factory> d2d2_factory,
      wil::com_ptr_nothrow<IDWriteFactory> dwrite_factory,
      wil::com_ptr_nothrow<IDWriteGdiInterop> dwrite_interop)
      : d2d2_factory_(std::move(d2d2_factory)),
        dwrite_factory_(std::move(dwrite_factory)),
        dwrite_interop_(std::move(dwrite_interop)) {
    OnThemeChanged();
  }

  static std::unique_ptr<DirectWriteTextRenderer> Create() {
    HRESULT hr = S_OK;
    wil::com_ptr_nothrow<ID2D1Factory> d2d_factory;
    hr = ::D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                             __uuidof(ID2D1Factory), nullptr,
                             d2d_factory.put_void());
    if (FAILED(hr)) {
      return nullptr;
    }
    wil::com_ptr_nothrow<IDWriteFactory> dwrite_factory;
    hr = ::DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                               __uuidof(IDWriteFactory),
                               dwrite_factory.put_unknown());
    if (FAILED(hr)) {
      return nullptr;
    }
    wil::com_ptr_nothrow<IDWriteGdiInterop> interop;
    hr = dwrite_factory->GetGdiInterop(interop.put());
    if (FAILED(hr)) {
      return nullptr;
    }
    return std::make_unique<DirectWriteTextRenderer>(
        std::move(d2d_factory), std::move(dwrite_factory), std::move(interop));
  }

 private:
  struct RenderInfo {
    RenderInfo() : color(0) {}
    COLORREF color;
    wil::com_ptr_nothrow<IDWriteTextFormat> format;
    wil::com_ptr_nothrow<IDWriteTextFormat> format_to_render;
  };

  // TextRenderer overrides:
  void OnThemeChanged() override {
    // delete old fonts
    render_info_.clear();
    render_info_.resize(SIZE_OF_FONT_TYPE);

    for (size_t i = 0; i < SIZE_OF_FONT_TYPE; ++i) {
      const auto font_type = static_cast<FONT_TYPE>(i);
      const auto& log_font = GetLogFont(font_type);
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
  Size MeasureString(FONT_TYPE font_type,
                     const std::wstring_view str) const override {
    return MeasureStringImpl(font_type, str, 0, false);
  }

  Size MeasureStringMultiLine(FONT_TYPE font_type, const std::wstring_view str,
                              const int width) const override {
    return MeasureStringImpl(font_type, str, width, true);
  }

  void RenderText(HDC dc, const std::wstring_view text, const Rect& rect,
                  FONT_TYPE font_type) const override {
    std::vector<TextRenderingInfo> infolist;
    infolist.emplace_back(std::wstring(text), rect);
    RenderTextList(dc, infolist, font_type);
  }

  void RenderTextList(HDC dc,
                      const absl::Span<const TextRenderingInfo> display_list,
                      FONT_TYPE font_type) const override {
    constexpr size_t kMaxTrial = 3;
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
        dc_render_target_.reset();
        ++trial;
        continue;
      }
      // For other error codes (including S_OK and S_FALSE), or if we exceed the
      // maximum number of trials, we simply accept the result here.
      return;
    }
  }

  HRESULT RenderTextListImpl(
      HDC dc, const absl::Span<const TextRenderingInfo> display_list,
      FONT_TYPE font_type) const {
    CRect total_rect;
    for (const auto& item : display_list) {
      const auto& item_rect = ToCRect(item.rect);
      total_rect.right = std::max(total_rect.right, item_rect.right);
      total_rect.bottom = std::max(total_rect.bottom, item_rect.bottom);
    }
    HRESULT hr = S_OK;
    hr = dc_render_target_->BindDC(dc, &total_rect);
    if (FAILED(hr)) {
      return hr;
    }
    wil::com_ptr_nothrow<ID2D1SolidColorBrush> brush;
    hr = dc_render_target_->CreateSolidColorBrush(
        ToD2DColor(render_info_[font_type].color), brush.put());
    if (FAILED(hr)) {
      return hr;
    }
    constexpr D2D1_DRAW_TEXT_OPTIONS option =
        D2D1_DRAW_TEXT_OPTIONS_NONE | D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT;
    dc_render_target_->BeginDraw();
    dc_render_target_->SetTransform(D2D1::Matrix3x2F::Identity());
    for (size_t i = 0; i < display_list.size(); ++i) {
      const auto& item = display_list[i];
      const D2D1_RECT_F render_rect = {
          static_cast<float>(item.rect.Left()),
          static_cast<float>(item.rect.Top()),
          static_cast<float>(item.rect.Right()),
          static_cast<float>(item.rect.Bottom()),
      };
      dc_render_target_->DrawText(
          item.text.data(), item.text.size(),
          render_info_[font_type].format_to_render.get(), render_rect,
          brush.get(), option);
    }
    return dc_render_target_->EndDraw();
  }

  Size MeasureStringImpl(FONT_TYPE font_type, const std::wstring_view str,
                         const int width, bool use_width) const {
    HRESULT hr = S_OK;
    constexpr FLOAT kLayoutLimit = 100000.0f;
    wil::com_ptr_nothrow<IDWriteTextLayout> layout;
    hr = dwrite_factory_->CreateTextLayout(
        str.data(), str.size(), render_info_[font_type].format.get(),
        (use_width ? width : kLayoutLimit), kLayoutLimit, layout.put());
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

  wil::com_ptr_nothrow<IDWriteTextFormat> CreateFormat(const LOGFONT& logfont) {
    HRESULT hr = S_OK;
    wil::com_ptr_nothrow<IDWriteFont> font;
    hr = dwrite_interop_->CreateFontFromLOGFONT(&logfont, font.put());
    if (FAILED(hr)) {
      return nullptr;
    }
    wil::com_ptr_nothrow<IDWriteFontFamily> font_family;
    hr = font->GetFontFamily(font_family.put());
    if (FAILED(hr)) {
      return nullptr;
    }
    wil::com_ptr_nothrow<IDWriteLocalizedStrings> localized_family_names;
    hr = font_family->GetFamilyNames(localized_family_names.put());
    if (FAILED(hr)) {
      return nullptr;
    }
    UINT32 length_without_null = 0;
    hr = localized_family_names->GetStringLength(0, &length_without_null);
    if (FAILED(hr)) {
      return nullptr;
    }
    // |IDWriteLocalizedStrings::GetString()| requires the return buffer to be
    // large enough to store a result with the terminating null character.
    // https://learn.microsoft.com/en-us/windows/win32/api/dwrite/nf-dwrite-idwritelocalizedstrings-getstring#parameters
    std::wstring family_name(size_t(length_without_null + 1), L'\0');
    hr = localized_family_names->GetString(0, family_name.data(),
                                           family_name.size());
    if (FAILED(hr)) {
      return nullptr;
    }
    family_name.resize(length_without_null);
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
    if (::GetUserDefaultLocaleName(locale_name, std::size(locale_name)) == 0) {
      return nullptr;
    }

    wil::com_ptr_nothrow<IDWriteTextFormat> format;
    hr = dwrite_factory_->CreateTextFormat(
        family_name.c_str(), nullptr, font->GetWeight(), font->GetStyle(),
        font->GetStretch(), font_size, locale_name, format.put());
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

  wil::com_ptr_nothrow<ID2D1Factory> d2d2_factory_;
  wil::com_ptr_nothrow<IDWriteFactory> dwrite_factory_;
  mutable wil::com_ptr_nothrow<ID2D1DCRenderTarget> dc_render_target_;
  wil::com_ptr_nothrow<IDWriteGdiInterop> dwrite_interop_;
  std::vector<RenderInfo> render_info_;
};

}  // namespace

// static
std::unique_ptr<TextRenderer> TextRenderer::Create() {
  std::unique_ptr<TextRenderer> dwrite_text_renderer =
      DirectWriteTextRenderer::Create();
  if (dwrite_text_renderer != nullptr) {
    return dwrite_text_renderer;
  }
  return std::make_unique<GdiTextRenderer>();
}

}  // namespace win32
}  // namespace renderer
}  // namespace mozc
