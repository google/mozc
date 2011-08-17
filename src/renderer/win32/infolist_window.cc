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

#include "renderer/win32/infolist_window.h"

#include <windows.h>

#include <sstream>

#include "base/util.h"
#include "client/client_interface.h"
#include "renderer/coordinates.h"
#include "renderer/renderer_style.pb.h"
#include "renderer/renderer_style_handler.h"
#include "renderer/table_layout.h"
#include "renderer/win32/text_renderer.h"
#include "renderer/win_resource.h"
#include "session/commands.pb.h"

namespace mozc {
namespace renderer {
namespace win32 {

using WTL::CBitmap;
using WTL::CDC;
using WTL::CDCHandle;
using WTL::CMemoryDC;
using WTL::CPaintDC;
using WTL::CPenHandle;
using WTL::CPoint;
using WTL::CRect;
using WTL::CSize;

using mozc::commands::Candidates;
using mozc::commands::Information;
using mozc::commands::InformationList;
using mozc::commands::Output;
using mozc::commands::SessionCommand;
using mozc::renderer::RendererStyle;
using mozc::renderer::RendererStyleHandler;

namespace {
const COLORREF kDefaultBackgroundColor = RGB(0xff, 0xff, 0xff);
const UINT_PTR kIdDelayShowHideTimer = 100;
}


// ------------------------------------------------------------------------
// InfolistWindow
// ------------------------------------------------------------------------

InfolistWindow::InfolistWindow()
    : candidates_(new commands::Candidates),
      metrics_changed_(false),
      text_renderer_(new TextRenderer),
      style_(new RendererStyle),
      visible_(false) {
  text_renderer_->Init();
  mozc::renderer::RendererStyleHandler::GetRendererStyle(style_.get());
}

InfolistWindow::~InfolistWindow() {}

void InfolistWindow::OnDestroy() {
  // PostQuitMessage may stop the message loop even though other
  // windows are not closed. WindowManager should close these windows
  // before process termination.
  ::PostQuitMessage(0);
}

BOOL InfolistWindow::OnEraseBkgnd(CDCHandle dc) {
  // We do not have to erase background
  // because all pixels in client area will be drawn in the DoPaint method.
  return TRUE;
}

void InfolistWindow::OnGetMinMaxInfo(MINMAXINFO *min_max_info) {
  // Do not restrict the window size in case the candidate window must be
  // very small size.
  min_max_info->ptMinTrackSize.x = 1;
  min_max_info->ptMinTrackSize.y = 1;
  SetMsgHandled(TRUE);
}

void InfolistWindow::OnPaint(CDCHandle dc) {
  CRect client_rect;
  this->GetClientRect(&client_rect);

  if (dc != NULL) {
    CMemoryDC memdc(dc, client_rect);
    DoPaint(memdc.m_hDC);
  } else  {
    CPaintDC paint_dc(this->m_hWnd);
    { // Create a copy of |paint_dc| and render the candidate strings in it.
      // The image rendered to this |memdc| is to be copied into the original
      // |paint_dc| in its destructor. So, we don't have to explicitly call
      // any functions that copy this |memdc| to the |paint_dc| but putting
      // the following code into a local block.
      CMemoryDC memdc(paint_dc, client_rect);
      DoPaint(memdc.m_hDC);
    }
  }
}

void InfolistWindow::OnPrintClient(CDCHandle dc, UINT uFlags) {
  OnPaint(dc);
}

Size InfolistWindow::DoPaint(CDCHandle dc) {
  if (dc.m_hDC != NULL) {
    dc.SetBkMode(TRANSPARENT);
  }
  const RendererStyle::InfolistStyle &infostyle = style_->infolist_style();
  const InformationList &usages = candidates_->usages();

  int ypos = infostyle.window_border();

  if ((dc.m_hDC != NULL) && infostyle.has_caption_string()) {
    const RendererStyle::TextStyle &caption_style =
      infostyle.caption_style();
    const int caption_height = infostyle.caption_height();
    const Rect backgrounnd_rect(infostyle.window_border(), ypos,
      infostyle.window_width() - infostyle.window_border() * 2,
      caption_height);
    const CRect background_crect = backgrounnd_rect.ToCRect();

    dc.FillSolidRect(&background_crect,
        RGB(infostyle.caption_background_color().r(),
            infostyle.caption_background_color().g(),
            infostyle.caption_background_color().b()));

    wstring caption_str;
    const Rect caption_rect(
      infostyle.window_border() + infostyle.caption_padding()
      + caption_style.left_padding(),
      ypos + infostyle.caption_padding(),
      infostyle.window_width() - infostyle.window_border() * 2,
      caption_height);
    mozc::Util::UTF8ToWide(infostyle.caption_string().c_str(),
                           &caption_str);

    text_renderer_->RenderText(dc,
                               caption_str,
                               caption_rect,
                               TextRenderer::FONTSET_INFOLIST_CAPTION);
  }
  ypos += infostyle.caption_height();

  for (int i = 0; i < usages.information_size(); ++i) {
    Size size = DoPaintRow(dc, i, ypos);
    ypos += size.height;
  }
  ypos += infostyle.window_border();

  if (dc.m_hDC != NULL) {
    const CRect rect(0, 0, infostyle.window_width(), ypos);
    dc.SetDCBrushColor(
        RGB(infostyle.border_color().r(),
            infostyle.border_color().g(),
            infostyle.border_color().b()));
    dc.FrameRect(&rect,
                 static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
  }


  return Size(style_->infolist_style().window_width(), ypos);
}

Size InfolistWindow::DoPaintRow(CDCHandle dc, int row, int ypos) {
  const RendererStyle::InfolistStyle &infostyle = style_->infolist_style();
  const InformationList &usages = candidates_->usages();
  const RendererStyle::TextStyle &title_style = infostyle.title_style();
  const RendererStyle::TextStyle &desc_style = infostyle.description_style();
  const int title_width = infostyle.window_width() -
      title_style.left_padding() - title_style.right_padding() -
      infostyle.window_border() * 2 -
      infostyle.row_rect_padding() * 2;
  const int desc_width = infostyle.window_width() -
      desc_style.left_padding() - desc_style.right_padding() -
      infostyle.window_border() * 2 -
      infostyle.row_rect_padding() * 2;
  const Information &info = usages.information(row);

  wstring title_str;
  mozc::Util::UTF8ToWide(info.title().c_str(), &title_str);
  const Size title_size = text_renderer_->MeasureStringMultiLine(
      TextRenderer::FONTSET_INFOLIST_TITLE, title_str, title_width);

  wstring desc_str;
  mozc::Util::UTF8ToWide(info.description().c_str(), &desc_str);
  const Size desc_size = text_renderer_->MeasureStringMultiLine(
      TextRenderer::FONTSET_INFOLIST_DESCRIPTION, desc_str, desc_width);

  int row_height = title_size.height + desc_size.height +
                   infostyle.row_rect_padding() * 2;

  if (dc.m_hDC == NULL) {
    return Size(0, row_height);
  }
  const Rect title_rect(
      infostyle.window_border() + infostyle.row_rect_padding() +
      title_style.left_padding(),
      ypos + infostyle.row_rect_padding(),
      title_width, title_size.height);
  const Rect desc_rect(
      infostyle.window_border() + infostyle.row_rect_padding() +
      desc_style.left_padding(),
      ypos + infostyle.row_rect_padding() + title_rect.size.height,
      desc_width, desc_size.height);

  const CRect title_back_crect(infostyle.window_border(), ypos,
      infostyle.window_width() - infostyle.window_border(),
      ypos + title_rect.size.height + infostyle.row_rect_padding());

  const CRect desc_back_crect(infostyle.window_border(),
      ypos + title_rect.size.height + infostyle.row_rect_padding(),
      infostyle.window_width() - infostyle.window_border(),
      ypos + title_rect.size.height + infostyle.row_rect_padding() +
      desc_rect.size.height + infostyle.row_rect_padding());

  if (usages.has_focused_index() && (row == usages.focused_index())) {
    const CRect selected_rect(infostyle.window_border(), ypos,
        infostyle.window_width() - infostyle.window_border(),
        ypos + title_rect.size.height + desc_rect.size.height
         + infostyle.row_rect_padding() * 2);
    dc.FillSolidRect(&selected_rect,
        RGB(infostyle.focused_background_color().r(),
            infostyle.focused_background_color().g(),
            infostyle.focused_background_color().b()));
    dc.SetDCBrushColor(
        RGB(infostyle.focused_border_color().r(),
            infostyle.focused_border_color().g(),
            infostyle.focused_border_color().b()));
    dc.FrameRect(&selected_rect,
        static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
  } else {
    if (title_style.has_background_color()) {
      dc.FillSolidRect(&title_back_crect,
          RGB(title_style.background_color().r(),
              title_style.background_color().g(),
              title_style.background_color().b()));
    } else {
      dc.FillSolidRect(&title_back_crect,
          RGB(255, 255, 255));
    }
    if (desc_style.has_background_color()) {
      dc.FillSolidRect(&desc_back_crect,
          RGB(title_style.background_color().r(),
              title_style.background_color().g(),
              title_style.background_color().b()));
    } else {
      dc.FillSolidRect(&desc_back_crect,
          RGB(255, 255, 255));
    }
  }

  text_renderer_->RenderText(dc,  title_str,  title_rect,
      TextRenderer::FONTSET_INFOLIST_TITLE);
  text_renderer_->RenderText(dc, desc_str, desc_rect,
      TextRenderer::FONTSET_INFOLIST_DESCRIPTION);
  return Size(0, row_height);
}

void InfolistWindow::OnSettingChange(UINT uFlags, LPCTSTR /*lpszSection*/) {
  // Since TextRenderer uses dialog font to render,
  // we monitor font-related parameters to know when the font style is changed.
  switch (uFlags) {
    case 0x1049:  // = SPI_SETCLEARTYPE
    case SPI_SETFONTSMOOTHING:
    case SPI_SETFONTSMOOTHINGCONTRAST:
    case SPI_SETFONTSMOOTHINGORIENTATION:
    case SPI_SETFONTSMOOTHINGTYPE:
    case SPI_SETNONCLIENTMETRICS:
      metrics_changed_ = true;
      break;
    default:
      // We ignore other changes.
      break;
  }
}
void InfolistWindow::OnTimer(UINT_PTR nIDEvent) {
  if (nIDEvent != kIdDelayShowHideTimer) {
    return;
  }
  if (visible_) {
    DelayShow(0);
  } else {
    DelayHide(0);
  }
}

void InfolistWindow::DelayShow(UINT mseconds) {
  visible_ = true;
  KillTimer(kIdDelayShowHideTimer);
  if (mseconds <= 0) {
    SetWindowPos(HWND_TOPMOST, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    SendMessageW(WM_NCACTIVATE, FALSE);
  } else {
    SetTimer(kIdDelayShowHideTimer, mseconds, NULL);
  }
}

void InfolistWindow::DelayHide(UINT mseconds) {
  visible_ = false;
  KillTimer(kIdDelayShowHideTimer);
  if (mseconds <= 0) {
    ShowWindow(SW_HIDE);
  } else {
    SetTimer(kIdDelayShowHideTimer, mseconds, NULL);
  }
}

void InfolistWindow::UpdateLayout(const commands::Candidates &candidates) {
  candidates_->CopyFrom(candidates);

  // If we detect any change of font parameters, update text renderer
  if (metrics_changed_) {
    text_renderer_->Init();
    metrics_changed_ = false;
  }
}

Size InfolistWindow::GetLayoutSize() {
  CDCHandle dmyDc(NULL);
  return DoPaint(dmyDc);
}
}  // namespace win32
}  // namespace renderer
}  // namespace mozc
