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

#include "renderer/win32/win32_dpi_util.h"

#include <windows.h>

#include "protocol/renderer_style.pb.h"
#include "renderer/renderer_style_handler.h"

namespace mozc {
namespace renderer {
namespace win32 {
namespace {

constexpr int kDefaultDPI = 96;

void ScaleTextStyle(RendererStyle::TextStyle* text_style,
                    double scale_factor) {
  text_style->set_font_size(text_style->font_size() * scale_factor);
  text_style->set_left_padding(text_style->left_padding() * scale_factor);
  text_style->set_right_padding(text_style->right_padding() * scale_factor);
}

}  // namespace

double GetDPIScalingFactor() {
  const UINT dpi = ::GetDpiForSystem();
  return static_cast<double>(dpi) / kDefaultDPI;
}

void GetScaledRendererStyle(::mozc::renderer::RendererStyle* style) {
  const double scale_factor = GetDPIScalingFactor();

  RendererStyleHandler::GetDefaultRendererStyle(style);

  // style->window_border is non-scalable.
  style->set_scrollbar_width(style->scrollbar_width() * scale_factor);
  style->set_row_rect_padding(style->row_rect_padding() * scale_factor);

  for (RendererStyle::TextStyle& text_style : *style->mutable_text_styles()) {
    ScaleTextStyle(&text_style, scale_factor);
  }

  ScaleTextStyle(style->mutable_footer_style(), scale_factor);
  ScaleTextStyle(style->mutable_footer_sub_label_style(), scale_factor);

  RendererStyle::InfolistStyle* info_style = style->mutable_infolist_style();
  // info_style->window_border and info_style->caption_padding are non-scalable.
  info_style->set_caption_height(info_style->caption_height() * scale_factor);
  info_style->set_row_rect_padding(info_style->row_rect_padding() *
                                   scale_factor);
  info_style->set_window_width(info_style->window_width() * scale_factor);

  ScaleTextStyle(info_style->mutable_caption_style(), scale_factor);
  ScaleTextStyle(info_style->mutable_title_style(), scale_factor);
  ScaleTextStyle(info_style->mutable_description_style(), scale_factor);
}

}  // namespace win32
}  // namespace renderer
}  // namespace mozc
