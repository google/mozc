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

#include "renderer/unix/infolist_window.h"

#include "base/logging.h"
#include "base/mutex.h"
#include "renderer/renderer_command.pb.h"
#include "renderer/renderer_style.pb.h"
#include "renderer/renderer_style_handler.h"
#include "renderer/unix/cairo_wrapper.h"
#include "renderer/unix/const.h"
#include "renderer/unix/draw_tool.h"
#include "renderer/unix/font_spec.h"
#include "renderer/unix/text_renderer.h"

namespace mozc {
namespace renderer {
namespace gtk {

using mozc::commands::Candidates;
using mozc::commands::Information;
using mozc::commands::InformationList;
using mozc::commands::Output;
using mozc::commands::SessionCommand;
using mozc::renderer::RendererStyle;
using mozc::renderer::RendererStyleHandler;

namespace {
RGBA StyleColorToRGBA(const RendererStyle::RGBAColor &rgbacolor) {
  const RGBA rgba = { static_cast<uint8>(rgbacolor.r()),
                      static_cast<uint8>(rgbacolor.g()),
                      static_cast<uint8>(rgbacolor.b()),
                      0xFF };
  return rgba;
}
}  // namespace

InfolistWindow::InfolistWindow(
    TextRendererInterface *text_renderer,
    DrawToolInterface *draw_tool,
    GtkWrapperInterface *gtk,
    CairoFactoryInterface *cairo_factory)
    : GtkWindowBase(gtk),
      text_renderer_(text_renderer),
      style_(new RendererStyle()),
      draw_tool_(draw_tool),
      cairo_factory_(cairo_factory) {
  mozc::renderer::RendererStyleHandler::GetRendererStyle(style_.get());
}

Size InfolistWindow::Update(const commands::Candidates &candidates) {
  candidates_.CopyFrom(candidates);

  const RendererStyle::InfolistStyle &infostyle = style_->infolist_style();
  const InformationList &usages = candidates_.usages();

  int ypos = infostyle.window_border();
  ypos += infostyle.caption_height();

  for (int i = 0; i < usages.information_size(); ++i) {
    ypos += GetRowRects(i, ypos).whole_rect.Height();
  }
  ypos += infostyle.window_border();

  const Size result_size(style_->infolist_style().window_width(), ypos);
  Resize(result_size);
  Redraw();
  return result_size;
}

// We do not use this function. So following code makes no sense.
Rect InfolistWindow::GetCandidateColumnInClientCord() const {
  DCHECK(false) << "Do not call this function without CandidateWindow.";
  return Rect(0, 0, 0, 0);
}

bool InfolistWindow::OnPaint(GtkWidget *widget, GdkEventExpose *event) {
  draw_tool_->Reset(cairo_factory_->CreateCairoInstance(
      GetCanvasWidget()->window));
  Draw();
  return true;
}

int InfolistWindow::DrawCaption() {
  const RendererStyle::InfolistStyle &infostyle = style_->infolist_style();
  if (!infostyle.has_caption_string()) {
    return 0;
  }
  const RendererStyle::TextStyle &caption_style = infostyle.caption_style();
  const int caption_height = infostyle.caption_height();
  const Rect background_rect(
      infostyle.window_border(),
      infostyle.window_border(),
      infostyle.window_width() - infostyle.window_border() * 2,
      caption_height);

  const RGBA bgcolor = StyleColorToRGBA(infostyle.caption_background_color());
  draw_tool_->FillRect(background_rect, bgcolor);

  const Rect caption_rect(
      background_rect.Left()
          + infostyle.caption_padding() + caption_style.left_padding(),
      background_rect.Top() + infostyle.caption_padding(),
      background_rect.Width()
          - infostyle.caption_padding() - caption_style.left_padding(),
      caption_height - infostyle.caption_padding());
  text_renderer_->RenderText(infostyle.caption_string(), caption_rect,
                             FontSpec::FONTSET_INFOLIST_CAPTION);
  return infostyle.caption_height();
}

void InfolistWindow::DrawFrame(int height) {
  const RendererStyle::InfolistStyle &infostyle = style_->infolist_style();
  const Rect rect(0, 0, infostyle.window_width(), height);
  const RGBA framecolor = StyleColorToRGBA(infostyle.border_color());
  draw_tool_->FrameRect(rect, framecolor, 1);
}

void InfolistWindow::Draw() {
  const RendererStyle::InfolistStyle &infostyle = style_->infolist_style();
  const InformationList &usages = candidates_.usages();
  int ypos = infostyle.window_border();
  ypos += DrawCaption();
  for (int i = 0; i < usages.information_size(); ++i) {
    ypos += DrawRow(i, ypos);
  }
  DrawFrame(ypos);
}

void InfolistWindow::GetRenderingRects(
    const renderer::RendererStyle::TextStyle &style,
    const string &text,
    FontSpec::FONT_TYPE font_type,
    int top,
    Rect *background_rect,
    Rect *textarea_rect) {
  const RendererStyle::InfolistStyle &infostyle = style_->infolist_style();
  const int text_width = infostyle.window_width()
      - style.left_padding() - style.right_padding()
      - infostyle.window_border() * 2 - infostyle.row_rect_padding() * 2;

  const Size text_size = text_renderer_->GetMultiLinePixelSize(font_type,
                                                               text,
                                                               text_width);

  background_rect->origin.x = infostyle.window_border();
  background_rect->origin.y = top;
  background_rect->size.width =
      infostyle.window_width() - infostyle.window_border() * 2;
  background_rect->size.height =
      text_size.height + infostyle.row_rect_padding() * 2;

  *textarea_rect = *background_rect;

  textarea_rect->origin.x +=
      infostyle.row_rect_padding() + style.left_padding();
  textarea_rect->origin.y += infostyle.row_rect_padding();
  textarea_rect->size.width = text_width;
  textarea_rect->size.height = text_size.height;
}

InfolistWindow::RenderingRowRects InfolistWindow::GetRowRects(int row,
                                                              int ypos) {
  const RendererStyle::InfolistStyle &infostyle = style_->infolist_style();
  const InformationList &usages = candidates_.usages();
  const RendererStyle::TextStyle &title_style = infostyle.title_style();
  const RendererStyle::TextStyle &desc_style = infostyle.description_style();
  DCHECK_GT(usages.information_size(), row);
  const Information &info = usages.information(row);

  RenderingRowRects result;

  GetRenderingRects(title_style,
                    info.title(),
                    FontSpec::FONTSET_INFOLIST_TITLE,
                    ypos,
                    &result.title_back_rect,
                    &result.title_rect);
  ypos += result.title_back_rect.size.height;
  GetRenderingRects(desc_style,
                    info.description(),
                    FontSpec::FONTSET_INFOLIST_DESCRIPTION,
                    ypos,
                    &result.desc_back_rect,
                    &result.desc_rect);
  result.whole_rect = result.title_back_rect;
  result.whole_rect.size.height += result.desc_back_rect.Height();
  return result;
}

int InfolistWindow::DrawRow(int row, int ypos) {
  const RendererStyle::InfolistStyle &infostyle = style_->infolist_style();
  const InformationList &usages = candidates_.usages();
  const RendererStyle::TextStyle &title_style = infostyle.title_style();
  const RendererStyle::TextStyle &desc_style = infostyle.description_style();
  DCHECK_GT(usages.information_size(), row);
  const Information &info = usages.information(row);
  const RenderingRowRects row_rects = GetRowRects(row, ypos);

  if (usages.has_focused_index() && (row == usages.focused_index())) {
    const RGBA bgcol = StyleColorToRGBA(infostyle.focused_background_color());
    draw_tool_->FillRect(row_rects.whole_rect, bgcol);

    const RGBA frcol = StyleColorToRGBA(infostyle.focused_border_color());
    draw_tool_->FrameRect(row_rects.whole_rect, frcol, 1);
  } else {
    if (title_style.has_background_color()) {
      draw_tool_->FillRect(row_rects.title_back_rect,
                           StyleColorToRGBA(title_style.background_color()));
    } else {
      draw_tool_->FillRect(row_rects.title_back_rect, kWhite);
    }
    if (desc_style.has_background_color()) {
      draw_tool_->FillRect(row_rects.desc_back_rect,
                           StyleColorToRGBA(desc_style.background_color()));
    } else {
      draw_tool_->FillRect(row_rects.desc_back_rect, kWhite);
    }
  }

  text_renderer_->RenderText(info.title(), row_rects.title_rect,
                             FontSpec::FONTSET_INFOLIST_TITLE);
  text_renderer_->RenderText(info.description(), row_rects.desc_rect,
                             FontSpec::FONTSET_INFOLIST_DESCRIPTION);
  return row_rects.whole_rect.Height();
}

void InfolistWindow::Initialize() {
  text_renderer_->Initialize(GetCanvasWidget()->window);
}

void InfolistWindow::ReloadFontConfig(const string &font_description) {
  text_renderer_->ReloadFontConfig(font_description);
}

}  // namespace gtk
}  // namespace renderer
}  // namespace mozc
