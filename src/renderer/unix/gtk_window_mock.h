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

#ifndef MOZC_RENDERER_UNIX_GTK_WINDOW_MOCK_H_
#define MOZC_RENDERER_UNIX_GTK_WINDOW_MOCK_H_

#include <gtk/gtk.h>

#include "renderer/unix/gtk_window_interface.h"
#include "testing/gmock.h"

namespace mozc {
namespace renderer {
namespace gtk {

class GtkWindowMock : public GtkWindowInterface {
 public:
  GtkWindowMock() {}
  virtual ~GtkWindowMock() {}

  MOCK_METHOD(void, ShowWindow, ());
  MOCK_METHOD(void, HideWindow, ());
  MOCK_METHOD(GtkWidget *, GetWindowWidget, ());
  MOCK_METHOD(GtkWidget *, GetCanvasWidget, ());
  MOCK_METHOD(Rect, GetWindowRect, ());
  MOCK_METHOD(Point, GetWindowPos, ());
  MOCK_METHOD(Size, GetWindowSize, ());
  MOCK_METHOD(bool, IsActive, ());
  MOCK_METHOD(bool, DestroyWindow, ());
  MOCK_METHOD(void, Move, (const Point &pos));
  MOCK_METHOD(void, Resize, (const Size &size));
  MOCK_METHOD(void, Initialize, ());
  MOCK_METHOD(void, Redraw, ());
  MOCK_METHOD(Size, Update, (const commands::Candidates &candidates));
  MOCK_METHOD(Rect, GetCandidateColumnInClientCord, (), (const));
  MOCK_METHOD(bool, SetSendCommandInterface,
              (client::SendCommandInterface* send_command_interface));
  MOCK_METHOD(void, ReloadFontConfig, (const std::string &font_description));
};

}  // namespace gtk
}  // namespace renderer
}  // namespace mozc

#endif  // MOZC_RENDERER_UNIX_GTK_WINDOW_MOCK_H_
