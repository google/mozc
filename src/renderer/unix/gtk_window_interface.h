// Copyright 2010-2020, Google Inc.
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

#ifndef MOZC_RENDERER_UNIX_GTK_WINDOW_INTERFACE_H_
#define MOZC_RENDERER_UNIX_GTK_WINDOW_INTERFACE_H_

#include <gtk/gtk.h>

#include "base/coordinates.h"
#include "protocol/renderer_command.pb.h"

namespace mozc {
namespace client {
class SendCommandInterface;
}  // namespace client
namespace renderer {
namespace gtk {

// This class derive necessary logics for window in mozc, so this class does not
// support fully generic GTK+ logic and also contains it's advanced code in it.
class GtkWindowInterface {
 public:
  GtkWindowInterface() {}
  virtual ~GtkWindowInterface() {}

  virtual void ShowWindow() = 0;
  virtual void HideWindow() = 0;
  virtual GtkWidget *GetWindowWidget() = 0;
  virtual GtkWidget *GetCanvasWidget() = 0;
  virtual Rect GetWindowRect() = 0;
  virtual Point GetWindowPos() = 0;
  virtual Size GetWindowSize() = 0;
  virtual bool IsActive() = 0;
  virtual bool DestroyWindow() = 0;
  virtual void Move(const Point &pos) = 0;
  virtual void Resize(const Size &size) = 0;
  virtual void Initialize() = 0;
  virtual void Redraw() = 0;
  virtual void ReloadFontConfig(const string &font_description) = 0;

  virtual Size Update(const commands::Candidates &candidates) = 0;

  // This function is only avaialbe for CandidateWindow.
  virtual Rect GetCandidateColumnInClientCord() const = 0;

  virtual bool SetSendCommandInterface(
      client::SendCommandInterface *send_command_interface) = 0;
};

}  // namespace gtk
}  // namespace renderer
}  // namespace mozc

#endif  // MOZC_RENDERER_UNIX_GTK_WINDOW_INTERFACE_H_
