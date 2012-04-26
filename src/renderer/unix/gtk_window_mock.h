// Copyright 2010-2012, Google Inc.
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

#ifndef MOZC_RENDERER_UNIX_GTK_WINDOW_MOCK_H_
#define MOZC_RENDERER_UNIX_GTK_WINDOW_MOCK_H_

#include <gtk/gtk.h>

#include "renderer/unix/gtk_window_interface.h"
#include "testing/base/public/gmock.h"

namespace mozc {
namespace renderer {
namespace gtk {

class GtkWindowMock : public GtkWindowInterface {
 public:
  GtkWindowMock() {}
  virtual ~GtkWindowMock() {}

  MOCK_METHOD0(ShowWindow, void());
  MOCK_METHOD0(HideWindow, void());
  MOCK_METHOD0(GetWindowWidget, GtkWidget *());
  MOCK_METHOD0(GetCanvasWidget, GtkWidget *());
  MOCK_METHOD0(GetWindowRect, Rect());
  MOCK_METHOD0(GetWindowPos, Point());
  MOCK_METHOD0(GetWindowSize, Size());
  MOCK_METHOD0(IsActive, bool());
  MOCK_METHOD0(DestroyWindow, bool());
  MOCK_METHOD1(Move, void(const Point &pos));
  MOCK_METHOD1(Resize, void(const Size &size));
  MOCK_METHOD0(Initialize, void());
  MOCK_METHOD0(Redraw, void());
  MOCK_METHOD1(Update, Size(const commands::Candidates &candidates));
  MOCK_CONST_METHOD0(GetCandidateColumnInClientCord, Rect());
  MOCK_METHOD1(SetSendCommandInterface,
               bool(client::SendCommandInterface *send_command_interface));
  MOCK_METHOD1(ReloadFontConfig, void(const string &font_description));
};

}  // namespace gtk
}  // namespace renderer
}  // namespace mozc

#endif  // MOZC_RENDERER_UNIX_GTK_WINDOW_INTERFACE_H_
