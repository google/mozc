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

#ifndef MOZC_RENDERER_UNIX_CANDIDATE_WINDOW_H_
#define MOZC_RENDERER_UNIX_CANDIDATE_WINDOW_H_

#include <memory>

#include "renderer/table_layout_interface.h"
#include "renderer/unix/cairo_factory_interface.h"
#include "renderer/unix/const.h"
#include "renderer/unix/draw_tool_interface.h"
#include "renderer/unix/gtk_window_base.h"
#include "renderer/unix/text_renderer_interface.h"
#include "testing/base/public/gunit_prod.h"

namespace mozc {
namespace client {
class SendCommandInterface;
}  // namespace client
namespace renderer {
namespace gtk {

class CandidateWindow : public GtkWindowBase {
 public:
  // GtkWindowBase (and super class) has ownership of all arguments.
  CandidateWindow(TableLayoutInterface *table_layout,
                  TextRendererInterface *text_renderer,
                  DrawToolInterface *draw_tool, GtkWrapperInterface *gtk,
                  CairoFactoryInterface *cairo_factory);
  virtual ~CandidateWindow() {}

  virtual Size Update(const commands::Candidates &candidates);
  virtual Rect GetCandidateColumnInClientCord() const;
  virtual void Initialize();

  // usage type for each column.
  enum COLUMN_TYPE {
    COLUMN_SHORTCUT = 0,  // show shortcut key
    COLUMN_GAP1,          // padding region
    COLUMN_CANDIDATE,     // show candidate string
    COLUMN_GAP2,          // padding region
    COLUMN_DESCRIPTION,   // show description message
    NUMBER_OF_COLUMNS,    // number of columns. (this item should be last)
  };

  virtual bool SetSendCommandInterface(
      client::SendCommandInterface *send_command_interface);
  virtual void ReloadFontConfig(const string &font_description);

 protected:
  // Callbacks
  bool OnPaint(GtkWidget *widget, GdkEventExpose *event);

  void OnMouseLeftUp(const Point &pos);

  // Returns zero oriented row index which covers the argument position.
  // If this function returns negative value, it means there are no row to
  // cover the argument position.
  // The reason for virtual function is for testing.
  virtual int GetSelectedRowIndex(const Point &pos) const;

 private:
  void DrawBackground();
  void DrawShortcutBackground();
  void DrawSelectedRect();
  void DrawCells();
  void DrawInformationIcon();
  void DrawVScrollBar();
  void DrawFooter();
  void DrawFrame();

  // Draws footer separator and updates footer content area rectangle.
  void DrawFooterSeparator(Rect *footer_content_area);

  // Draws footer index into specified rectangle and updates content area to
  // rest one.
  void DrawFooterIndex(Rect *footer_content_rect);

  // Draws logo into specified rectangle and updates content area to rest one.
  void DrawLogo(Rect *footer_content_rect);

  // Draws footer label.
  void DrawFooterLabel(const Rect &footer_content_rect);

  // Make strings to be displayed based on candidates. If there is no string to
  // be displayed, string will be empty.
  static void GetDisplayString(const commands::Candidates::Candidate &candidate,
                               string *shortcut, string *value,
                               string *description);

  void UpdateScrollBarSize();
  void UpdateFooterSize();
  void UpdateGap1Size();

  void UpdateCandidatesSize(bool *has_description);
  void UpdateGap2Size(bool has_description);

  // TODO(nona): Remove FRIEND_TEST
  FRIEND_TEST(CandidateWindowTest, DrawBackgroundTest);
  FRIEND_TEST(CandidateWindowTest, DrawShortcutBackgroundTest);
  FRIEND_TEST(CandidateWindowTest, DrawSelectedRectTest);
  FRIEND_TEST(CandidateWindowTest, DrawCellsTest);
  FRIEND_TEST(CandidateWindowTest, DrawInformationIconTest);
  FRIEND_TEST(CandidateWindowTest, DrawVScrollBarTest);
  FRIEND_TEST(CandidateWindowTest, DrawFooterTest);
  FRIEND_TEST(CandidateWindowTest, DrawFrameTest);
  FRIEND_TEST(CandidateWindowTest, GetDisplayStringTest);
  FRIEND_TEST(CandidateWindowTest, DrawFooterSeparatorTest);
  FRIEND_TEST(CandidateWindowTest, DrawFooterIndexTest);
  FRIEND_TEST(CandidateWindowTest, DrawLogoTest);
  FRIEND_TEST(CandidateWindowTest, DrawFooterLabelTest);
  FRIEND_TEST(CandidateWindowTest, UpdateScroolBarSizeTest);
  FRIEND_TEST(CandidateWindowTest, UpdateFooterSizeTest);
  FRIEND_TEST(CandidateWindowTest, UpdateGap1SizeTest);
  FRIEND_TEST(CandidateWindowTest, UpdateCandidatesSizeTest);
  FRIEND_TEST(CandidateWindowTest, UpdateGap2SizeTest);
  FRIEND_TEST(CandidateWindowTest, OnMouseLeftUpTest);
  FRIEND_TEST(CandidateWindowTest, GetSelectedRowIndexTest);

  commands::Candidates candidates_;
  std::unique_ptr<TableLayoutInterface> table_layout_;
  std::unique_ptr<TextRendererInterface> text_renderer_;
  std::unique_ptr<DrawToolInterface> draw_tool_;
  std::unique_ptr<CairoFactoryInterface> cairo_factory_;
  client::SendCommandInterface *send_command_interface_;
  DISALLOW_COPY_AND_ASSIGN(CandidateWindow);
};

}  // namespace gtk
}  // namespace renderer
}  // namespace mozc
#endif  // MOZC_RENDERER_UNIX_CANDIDATE_WINDOW_H_
