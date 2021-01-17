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

// clang-format off
#include <windows.h>
#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
#include <atlbase.h>
#include <atlwin.h>
#include <atlapp.h>
#include <atlcrack.h>
#include <atlmisc.h>
#include <atlgdi.h>
// clang-format on

#include <memory>
#include <string>

#include "base/const.h"
#include "base/coordinates.h"
#include "base/port.h"

namespace mozc {

namespace client {
class SendCommandInterface;
}  // namespace client

namespace commands {
class Candidates;
}  // namespace commands

namespace renderer {
class RendererStyle;
namespace win32 {

class TextRenderer;

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

  BEGIN_MSG_MAP_EX(InfolistWindow)
  MSG_WM_DESTROY(OnDestroy)
  MSG_WM_ERASEBKGND(OnEraseBkgnd)
  MSG_WM_GETMINMAXINFO(OnGetMinMaxInfo)
  MSG_WM_SETTINGCHANGE(OnSettingChange)
  MSG_WM_PAINT(OnPaint)
  MSG_WM_PRINTCLIENT(OnPrintClient)
  MSG_WM_TIMER(OnTimer)
  END_MSG_MAP()

  InfolistWindow();
  ~InfolistWindow();
  void OnDestroy();
  BOOL OnEraseBkgnd(WTL::CDCHandle dc);
  void OnGetMinMaxInfo(MINMAXINFO *min_max_info);
  void OnPaint(WTL::CDCHandle dc);
  void OnPrintClient(WTL::CDCHandle dc, UINT uFlags);
  void OnSettingChange(UINT uFlags, LPCTSTR lpszSection);
  void OnTimer(UINT_PTR nIDEvent);

  void UpdateLayout(const commands::Candidates &candidates);
  void SetSendCommandInterface(
      client::SendCommandInterface *send_command_interface);

  // Layout information for the WindowManager class.
  Size GetLayoutSize();

  void DelayShow(UINT mseconds);
  void DelayHide(UINT mseconds);

 private:
  Size DoPaint(WTL::CDCHandle dc);
  Size DoPaintRow(WTL::CDCHandle dc, int row, int ypos);

  client::SendCommandInterface *send_command_interface_;
  std::unique_ptr<commands::Candidates> candidates_;
  std::unique_ptr<TextRenderer> text_renderer_;
  std::unique_ptr<renderer::RendererStyle> style_;
  bool metrics_changed_;
  bool visible_;

  DISALLOW_COPY_AND_ASSIGN(InfolistWindow);
};

}  // namespace win32
}  // namespace renderer
}  // namespace mozc

#endif  // MOZC_RENDERER_WIN32_INFOLIST_WINDOW_H_
