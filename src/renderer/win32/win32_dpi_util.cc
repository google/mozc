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
    text_style.set_font_size(text_style.font_size() * scale_factor);
    text_style.set_left_padding(text_style.left_padding() * scale_factor);
    text_style.set_right_padding(text_style.right_padding() * scale_factor);
  }

  {
    RendererStyle::TextStyle* footer_style = style->mutable_footer_style();
    footer_style->set_font_size(footer_style->font_size() * scale_factor);
    footer_style->set_left_padding(footer_style->left_padding() * scale_factor);
    footer_style->set_right_padding(footer_style->right_padding() *
                                    scale_factor);
  }

  {
    RendererStyle::TextStyle* footer_sub_label_style =
        style->mutable_footer_sub_label_style();
    footer_sub_label_style->set_font_size(footer_sub_label_style->font_size() *
                                          scale_factor);
    footer_sub_label_style->set_left_padding(
        footer_sub_label_style->left_padding() * scale_factor);
    footer_sub_label_style->set_right_padding(
        footer_sub_label_style->right_padding() * scale_factor);
  }

  {
    RendererStyle::InfolistStyle* infostyle = style->mutable_infolist_style();
    // infostyle->window_border and infostyle->caption_padding are non-scalable.
    infostyle->set_caption_height(infostyle->caption_height() * scale_factor);
    infostyle->set_row_rect_padding(infostyle->row_rect_padding() *
                                    scale_factor);
    infostyle->set_window_width(infostyle->window_width() * scale_factor);

    RendererStyle::TextStyle* caption_style = infostyle->mutable_caption_style();
    caption_style->set_font_size(caption_style->font_size() * scale_factor);
    caption_style->set_left_padding(caption_style->left_padding() *
                                    scale_factor);

    RendererStyle::TextStyle* title_style = infostyle->mutable_title_style();
    title_style->set_font_size(title_style->font_size() * scale_factor);
    title_style->set_left_padding(title_style->left_padding() * scale_factor);

    RendererStyle::TextStyle* description_style =
        infostyle->mutable_description_style();
    description_style->set_font_size(description_style->font_size() *
                                     scale_factor);
    description_style->set_left_padding(description_style->left_padding() *
                                        scale_factor);
  }
}

}  // namespace win32
}  // namespace renderer
}  // namespace mozc
