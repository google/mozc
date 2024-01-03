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

#ifndef MOZC_RENDERER_WIN32_CANDIDATE_WINDOW_H_
#define MOZC_RENDERER_WIN32_CANDIDATE_WINDOW_H_

// clang-format off
#include <windows.h>
#include <atlbase.h>
#include <atltypes.h>
#include <atlwin.h>
#include <atlapp.h>
#include <atlcrack.h>
#include <atlmisc.h>
#include <atlgdi.h>
// clang-format on

#include <memory>

#include "base/const.h"
#include "base/coordinates.h"
#include "client/client_interface.h"
#include "protocol/candidates.pb.h"
#include "protocol/commands.pb.h"
#include "renderer/table_layout.h"
#include "renderer/win32/text_renderer.h"

namespace mozc {
namespace renderer {
namespace win32 {

// As Discussed in b/2317702, UI windows are disabled by default because it is
// hard for a user to find out what caused the problem than finding that the
// operations seems to be disabled on the UI window when
// SPI_GETACTIVEWINDOWTRACKING is enabled.
// TODO(yukawa): Support mouse operations before we add a GUI feature which
//   requires UI interaction by mouse and/or touch. (b/2954874)
typedef ATL::CWinTraits<WS_POPUP | WS_DISABLED,
                        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE>
    CandidateWindowTraits;

// a class which implements an IME candidate window for Windows. This class
// is derived from an ATL CWindowImpl<T> class, which provides methods for
// creating a window and handling windows messages.
class CandidateWindow : public ATL::CWindowImpl<CandidateWindow, ATL::CWindow,
                                                CandidateWindowTraits> {
 public:
  DECLARE_WND_CLASS_EX(kCandidateWindowClassName, CS_SAVEBITS | CS_DROPSHADOW,
                       COLOR_WINDOW);

  BEGIN_MSG_MAP_EX(CandidateWindow)
  MSG_WM_CREATE(OnCreate)
  MSG_WM_DESTROY(OnDestroy)
  MSG_WM_DPICHANGED(OnDpiChanged)
  MSG_WM_ERASEBKGND(OnEraseBkgnd)
  MSG_WM_GETMINMAXINFO(OnGetMinMaxInfo)
  MSG_WM_LBUTTONDOWN(OnLButtonDown)
  MSG_WM_LBUTTONUP(OnLButtonUp)
  MSG_WM_MOUSEMOVE(OnMouseMove)
  MSG_WM_SETTINGCHANGE(OnSettingChange)
  MSG_WM_PAINT(OnPaint)
  MSG_WM_PRINTCLIENT(OnPrintClient)
  END_MSG_MAP()

  CandidateWindow();
  CandidateWindow(const CandidateWindow &) = delete;
  CandidateWindow &operator=(const CandidateWindow &) = delete;
  ~CandidateWindow();
  LRESULT OnCreate(LPCREATESTRUCT create_struct);
  void OnDestroy();
  void OnDpiChanged(UINT dpiX, UINT dpiY, RECT *rect);
  BOOL OnEraseBkgnd(WTL::CDCHandle dc);
  void OnGetMinMaxInfo(MINMAXINFO *min_max_info);
  void OnLButtonDown(UINT nFlags, CPoint point);
  void OnLButtonUp(UINT nFlags, CPoint point);
  void OnMouseMove(UINT nFlags, CPoint point);
  void OnPaint(WTL::CDCHandle dc);
  void OnPrintClient(WTL::CDCHandle dc, UINT uFlags);
  void OnSettingChange(UINT uFlags, LPCTSTR lpszSection);

  void set_mouse_moving(bool moving);

  void UpdateLayout(const commands::Candidates &candidates);
  void SetSendCommandInterface(
      client::SendCommandInterface *send_command_interface);

  // Layout information for the WindowManager class.
  Size GetLayoutSize() const;
  Rect GetSelectionRectInScreenCord() const;
  Rect GetCandidateColumnInClientCord() const;
  Rect GetFirstRowInClientCord() const;

 private:
  void DoPaint(WTL::CDCHandle dc);

  void DrawCells(WTL::CDCHandle dc);
  void DrawVScrollBar(WTL::CDCHandle dc);
  void DrawShortcutBackground(WTL::CDCHandle dc);
  void DrawFooter(WTL::CDCHandle dc);
  void DrawSelectedRect(WTL::CDCHandle dc);
  void DrawInformationIcon(WTL::CDCHandle dc);
  void DrawBackground(WTL::CDCHandle dc);
  void DrawFrame(WTL::CDCHandle dc);

  // Handles candidate selection by mouse.
  void HandleMouseEvent(UINT nFlags, const CPoint &point,
                        bool close_candidatewindow);

  // Even though the candidate window supports limited mouse operations, we
  // accept them when and only when SPI_GETACTIVEWINDOWTRACKING is disabled
  // to avoid problematic side effect as discussed in b/2317702.
  void EnableOrDisableWindowForWorkaround();

  std::unique_ptr<commands::Candidates> candidates_;
  WTL::CBitmap footer_logo_;
  Size footer_logo_display_size_;
  client::SendCommandInterface *send_command_interface_;
  std::unique_ptr<TableLayout> table_layout_;
  std::unique_ptr<TextRenderer> text_renderer_;
  int indicator_width_;
  bool metrics_changed_;
  bool mouse_moving_;
};

}  // namespace win32
}  // namespace renderer
}  // namespace mozc

#endif  // MOZC_RENDERER_WIN32_CANDIDATE_WINDOW_H_
