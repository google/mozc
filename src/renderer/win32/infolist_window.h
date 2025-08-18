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

#ifndef MOZC_RENDERER_WIN32_INFOLIST_WINDOW_H_
#define MOZC_RENDERER_WIN32_INFOLIST_WINDOW_H_

#include <atlbase.h>
#include <atltypes.h>
#include <atlwin.h>
#include <windows.h>

#include <memory>
#include <string>

#include "base/const.h"
#include "base/coordinates.h"
#include "client/client_interface.h"
#include "protocol/renderer_command.pb.h"
#include "protocol/renderer_style.pb.h"
#include "renderer/win32/text_renderer.h"

namespace mozc {
namespace renderer {
namespace win32 {

typedef ATL::CWinTraits<WS_POPUP | WS_DISABLED,
                        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE>
    InfolistWindowTraits;

// a class which implements an IME infolist window for Windows. This class
// is derived from an ATL CWindowImpl<T> class, which provides methods for
// creating a window and handling windows messages.
class InfolistWindow : public ATL::CWindowImpl<InfolistWindow, ATL::CWindow,
                                               InfolistWindowTraits> {
 public:
  DECLARE_WND_CLASS_EX(kInfolistWindowClassName, CS_SAVEBITS | CS_DROPSHADOW,
                       COLOR_WINDOW);

  BEGIN_MSG_MAP(InfolistWindow)
  MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
  MESSAGE_HANDLER(WM_DPICHANGED, OnDpiChanged)
  MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
  MESSAGE_HANDLER(WM_GETMINMAXINFO, OnGetMinMaxInfo)
  MESSAGE_HANDLER(WM_SETTINGCHANGE, OnSettingChange)
  MESSAGE_HANDLER(WM_PAINT, OnPaint)
  MESSAGE_HANDLER(WM_PRINTCLIENT, OnPrintClient)
  MESSAGE_HANDLER(WM_TIMER, OnTimer)
  END_MSG_MAP()

  InfolistWindow();
  InfolistWindow(const InfolistWindow&) = delete;
  InfolistWindow& operator=(const InfolistWindow&) = delete;
  ~InfolistWindow();
  void OnDestroy();
  void OnDpiChanged(UINT dpiX, UINT dpiY, RECT* rect);
  BOOL OnEraseBkgnd(HDC dc);
  void OnGetMinMaxInfo(MINMAXINFO* min_max_info);
  void OnPaint(HDC dc);
  void OnPrintClient(HDC dc, UINT uFlags);
  void OnSettingChange(UINT uFlags, LPCTSTR lpszSection);
  void OnTimer(UINT_PTR nIDEvent);

  void UpdateLayout(const commands::CandidateWindow& candidates);
  void SetSendCommandInterface(
      client::SendCommandInterface* send_command_interface);

  // Layout information for the WindowManager class.
  Size GetLayoutSize();

  void DelayShow(UINT mseconds);
  void DelayHide(UINT mseconds);

 private:
  Size DoPaint(HDC dc);
  Size DoPaintRow(HDC dc, int row, int ypos);

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
  inline LRESULT OnTimer(UINT msg_id, WPARAM wparam, LPARAM lparam,
                         BOOL& handled) {
    OnTimer(static_cast<UINT_PTR>(wparam));
    return 0;
  }

  client::SendCommandInterface* send_command_interface_;
  std::unique_ptr<commands::CandidateWindow> candidate_window_;
  std::unique_ptr<TextRenderer> text_renderer_;
  std::unique_ptr<renderer::RendererStyle> style_;
  bool metrics_changed_;
  bool visible_;
};

}  // namespace win32
}  // namespace renderer
}  // namespace mozc

#endif  // MOZC_RENDERER_WIN32_INFOLIST_WINDOW_H_
