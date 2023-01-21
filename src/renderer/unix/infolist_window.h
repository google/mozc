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

// TODO(nona): Implement delay rendering.

#ifndef MOZC_RENDERER_UNIX_INFOLIST_WINDOW_H_
#define MOZC_RENDERER_UNIX_INFOLIST_WINDOW_H_

#include <gtk/gtk.h>

#include <memory>

#include "base/coordinates.h"
#include "protocol/renderer_command.pb.h"
#include "protocol/renderer_style.pb.h"
#include "renderer/unix/cairo_factory_interface.h"
#include "renderer/unix/draw_tool_interface.h"
#include "renderer/unix/gtk_window_base.h"
#include "renderer/unix/text_renderer_interface.h"
#include "testing/gunit_prod.h"

namespace mozc {
namespace renderer {
namespace gtk {

class InfolistWindow : public GtkWindowBase {
 public:
  // GtkWindowBase(InfolistWindow) has ownership of GtkWrapperInterface *gtk;
  explicit InfolistWindow(TextRendererInterface *text_renderer,
                          DrawToolInterface *draw_tool,
                          GtkWrapperInterface *gtk,
                          CairoFactoryInterface *cairo_factory);
  InfolistWindow(const InfolistWindow &) = delete;
  InfolistWindow &operator=(const InfolistWindow &) = delete;
  virtual ~InfolistWindow() {}

  virtual Size Update(const commands::Candidates &candidates);
  virtual void Initialize();
  // This function is not used in infolist window.
  virtual Rect GetCandidateColumnInClientCord() const;
  virtual void ReloadFontConfig(const std::string &font_description);

 protected:
  virtual bool OnPaint(GtkWidget *widget, GdkEventExpose *event);

 private:
  struct RenderingRowRects {
    Rect title_rect;
    Rect title_back_rect;
    Rect desc_rect;
    Rect desc_back_rect;
    Rect whole_rect;
  };

  void Draw();

  // Draws specified description row and returns its height.
  int DrawRow(int row, int ypos);

  // Gets target rendering rects.
  RenderingRowRects GetRowRects(int row, int ypos);

  // Draws caption string and returns its height.
  int DrawCaption();

  // Draws infolist window frame line.
  void DrawFrame(int height);

  // Calculate background and text displaying area based on style, text font and
  // top position.
  void GetRenderingRects(const renderer::RendererStyle::TextStyle &style,
                         const std::string &text,
                         FontSpecInterface::FONT_TYPE font_type, int top,
                         Rect *background_rect, Rect *textarea_rect);

  friend class InfolistWindowTest;
  FRIEND_TEST(InfolistWindowTest, DrawFrameTest);
  FRIEND_TEST(InfolistWindowTest, DrawRowTest);
  FRIEND_TEST(InfolistWindowTest, GetRowRectsTest);
  FRIEND_TEST(InfolistWindowTest, DrawCaptionTest);
  FRIEND_TEST(InfolistWindowTest, GetRenderingRectsTest);

  commands::Candidates candidates_;
  std::unique_ptr<TextRendererInterface> text_renderer_;
  std::unique_ptr<RendererStyle> style_;
  std::unique_ptr<DrawToolInterface> draw_tool_;
  std::unique_ptr<CairoFactoryInterface> cairo_factory_;
};

}  // namespace gtk
}  // namespace renderer
}  // namespace mozc
#endif  // MOZC_RENDERER_UNIX_INFOLIST_WINDOW_H_
