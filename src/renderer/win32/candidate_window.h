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

#include <atlbase.h>
#include <atltypes.h>
#include <atlwin.h>
#include <wil/resource.h>
#include <windows.h>

#include <memory>

#include "base/const.h"
#include "base/coordinates.h"
#include "client/client_interface.h"
#include "protocol/candidate_window.pb.h"
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

  BEGIN_MSG_MAP(CandidateWindow)
  MESSAGE_HANDLER(WM_CREATE, OnCreate)
  MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
  MESSAGE_HANDLER(WM_DPICHANGED, OnDpiChanged)
  MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
  MESSAGE_HANDLER(WM_GETMINMAXINFO, OnGetMinMaxInfo)
  MESSAGE_HANDLER(WM_LBUTTONDOWN, OnLButtonDown)
  MESSAGE_HANDLER(WM_LBUTTONUP, OnLButtonUp)
  MESSAGE_HANDLER(WM_MOUSEMOVE, OnMouseMove)
  MESSAGE_HANDLER(WM_SETTINGCHANGE, OnSettingChange)
  MESSAGE_HANDLER(WM_PAINT, OnPaint)
  MESSAGE_HANDLER(WM_PRINTCLIENT, OnPrintClient)
  END_MSG_MAP()

  CandidateWindow();
  CandidateWindow(const CandidateWindow&) = delete;
  CandidateWindow& operator=(const CandidateWindow&) = delete;
  ~CandidateWindow();
  LRESULT OnCreate(LPCREATESTRUCT create_struct);
  void OnDestroy();
  void OnDpiChanged(UINT dpiX, UINT dpiY, RECT* rect);
  BOOL OnEraseBkgnd(HDC dc);
  void OnGetMinMaxInfo(MINMAXINFO* min_max_info);
  void OnLButtonDown(UINT nFlags, CPoint point);
  void OnLButtonUp(UINT nFlags, CPoint point);
  void OnMouseMove(UINT nFlags, CPoint point);
  void OnPaint(HDC dc);
  void OnPrintClient(HDC dc, UINT uFlags);
  void OnSettingChange(UINT uFlags, LPCTSTR lpszSection);

  void set_mouse_moving(bool moving);

  void UpdateLayout(const commands::CandidateWindow& candidate_window);
  void SetSendCommandInterface(
      client::SendCommandInterface* send_command_interface);

  // Layout information for the WindowManager class.
  Size GetLayoutSize() const;
  Rect GetSelectionRectInScreenCord() const;
  Rect GetCandidateColumnInClientCord() const;
  Rect GetFirstRowInClientCord() const;

 private:
  void DoPaint(HDC dc);

  void DrawCells(HDC dc);
  void DrawVScrollBar(HDC dc);
  void DrawShortcutBackground(HDC dc);
  void DrawFooter(HDC dc);
  void DrawSelectedRect(HDC dc);
  void DrawInformationIcon(HDC dc);
  void DrawBackground(HDC dc);
  void DrawFrame(HDC dc);

  // Handles candidate selection by mouse.
  void HandleMouseEvent(UINT nFlags, const CPoint& point,
                        bool close_candidatewindow);

  // Even though the candidate window supports limited mouse operations, we
  // accept them when and only when SPI_GETACTIVEWINDOWTRACKING is disabled
  // to avoid problematic side effect as discussed in b/2317702.
  void EnableOrDisableWindowForWorkaround();

  inline LRESULT OnCreate(UINT msg_id, WPARAM wparam, LPARAM lparam,
                          BOOL& handled) {
    return static_cast<LRESULT>(
        OnCreate(reinterpret_cast<LPCREATESTRUCT>(lparam)));
  }
  inline LRESULT OnDestroy(UINT msg_id, WPARAM wparam, LPARAM lparam,
                           BOOL& handled) {
    OnDestroy();
    return 0;
  }
  inline LRESULT OnDpiChanged(UINT msg_id, WPARAM wparam, LPARAM lparam,
                              BOOL& handled) {
    OnDpiChanged(static_cast<UINT>(LOWORD(wparam)),
                 static_cast<UINT>(HIWORD(wparam)),
                 reinterpret_cast<RECT*>(lparam));
    return 0;
  }
  inline LRESULT OnEraseBkgnd(UINT msg_id, WPARAM wparam, LPARAM lparam,
                              BOOL& handled) {
    return static_cast<LRESULT>(OnEraseBkgnd(reinterpret_cast<HDC>(wparam)));
  }
  inline LRESULT OnGetMinMaxInfo(UINT msg_id, WPARAM wparam, LPARAM lparam,
                                 BOOL& handled) {
    OnGetMinMaxInfo(reinterpret_cast<MINMAXINFO*>(lparam));
    return 0;
  }
  inline LRESULT OnLButtonDown(UINT msg_id, WPARAM wparam, LPARAM lparam,
                               BOOL& handled) {
    OnLButtonDown(static_cast<UINT>(wparam),
                  CPoint(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)));
    return 0;
  }
  inline LRESULT OnLButtonUp(UINT msg_id, WPARAM wparam, LPARAM lparam,
                             BOOL& handled) {
    OnLButtonUp(static_cast<UINT>(wparam),
                CPoint(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)));
    return 0;
  }
  inline LRESULT OnMouseMove(UINT msg_id, WPARAM wparam, LPARAM lparam,
                             BOOL& handled) {
    OnMouseMove(static_cast<UINT>(wparam),
                CPoint(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)));
    return 0;
  }
  inline LRESULT OnSettingChange(UINT msg_id, WPARAM wparam, LPARAM lparam,
                                 BOOL& handled) {
    OnSettingChange(static_cast<UINT>(wparam),
                    reinterpret_cast<LPCTSTR>(lparam));
    return 0;
  }
  inline LRESULT OnPaint(UINT msg_id, WPARAM wparam, LPARAM lparam,
                         BOOL& handled) {
    OnPaint(reinterpret_cast<HDC>(wparam));
    return 0;
  }
  inline LRESULT OnPrintClient(UINT msg_id, WPARAM wparam, LPARAM lparam,
                               BOOL& handled) {
    OnPrintClient(reinterpret_cast<HDC>(wparam), static_cast<UINT>(lparam));
    return 0;
  }

  std::unique_ptr<commands::CandidateWindow> candidate_window_;
  wil::unique_hbitmap footer_logo_;
  Size footer_logo_display_size_;
  client::SendCommandInterface* send_command_interface_;
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
