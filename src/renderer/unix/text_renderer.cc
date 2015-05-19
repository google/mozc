// Copyright 2010-2014, Google Inc.
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

#include "base/coordinates.h"
#include "renderer/unix/font_spec.h"
#include "renderer/unix/pango_wrapper.h"

namespace mozc {
namespace renderer {
namespace gtk {

TextRenderer::TextRenderer(FontSpecInterface *font_spec)
  : font_spec_(font_spec),
    pango_(nullptr) {
}

void TextRenderer::Initialize(GdkDrawable *drawable) {
  pango_.reset(new PangoWrapper(drawable));
}

void TextRenderer::SetUpPangoLayout(const string &str,
                                    FontSpecInterface::FONT_TYPE font_type,
                                    PangoLayoutWrapperInterface *layout) {
  PangoAttrList *attributes = pango_->CopyAttributes(
      font_spec_->GetFontAttributes(font_type));
  layout->SetText(str.c_str());
  layout->SetAlignment(font_spec_->GetFontAlignment(font_type));
  layout->SetAttributes(attributes);
  layout->SetFontDescription(font_spec_->GetFontDescription(font_type));
  pango_->AttributesUnref(attributes);
}

Size TextRenderer::GetPixelSize(FontSpecInterface::FONT_TYPE font_type,
                                const string &str) {
  PangoLayoutWrapper layout(pango_->GetContext());
  return GetPixelSizeInternal(font_type, str, &layout);
}

Size TextRenderer::GetPixelSizeInternal(FontSpecInterface::FONT_TYPE font_type,
                                        const string &str,
                                        PangoLayoutWrapperInterface *layout) {
  SetUpPangoLayout(str, font_type, layout);
  return layout->GetPixelSize();
}

Size TextRenderer::GetMultiLinePixelSize(FontSpecInterface::FONT_TYPE font_type,
                                         const string &str,
                                         const int width) {
  PangoLayoutWrapper layout(pango_->GetContext());
  return GetMultiLinePixelSizeInternal(font_type, str, width, &layout);
}

Size TextRenderer::GetMultiLinePixelSizeInternal(
    FontSpecInterface::FONT_TYPE font_type,
    const string &str,
    const int width,
    PangoLayoutWrapperInterface *layout) {
  SetUpPangoLayout(str, font_type, layout);
  layout->SetWidth(width * PANGO_SCALE);
  return layout->GetPixelSize();
}

void TextRenderer::RenderText(const string &text,
                              const Rect &rect,
                              FontSpecInterface::FONT_TYPE font_type) {
  PangoLayoutWrapper layout(pango_->GetContext());
  RenderTextInternal(text, rect, font_type, &layout);
}

void TextRenderer::RenderTextInternal(const string& text,
                                      const Rect &rect,
                                      FontSpecInterface::FONT_TYPE font_type,
                                      PangoLayoutWrapperInterface *layout) {
  SetUpPangoLayout(text, font_type, layout);
  layout->SetWidth(rect.size.width * PANGO_SCALE);
  layout->SetHeight(rect.size.height * PANGO_SCALE);

  // Vertical centering.
  Size actual_size = layout->GetPixelSize();
  const int delta_y = (rect.size.height - actual_size.height) / 2;

  pango_->RendererDrawLayout(layout, rect.origin.x * PANGO_SCALE,
                             (rect.origin.y + delta_y) * PANGO_SCALE);
}

void TextRenderer::ReloadFontConfig(const string &font_description) {
  font_spec_->Reload(font_description);
}
}  // namespace gtk
}  // namespace renderer
}  // namespace mozc
