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

#ifndef MOZC_RENDERER_UNIX_GTK_WINDOW_BASE_H_
#define MOZC_RENDERER_UNIX_GTK_WINDOW_BASE_H_

#include <gtk/gtk.h>

#include "base/base.h"
#include "base/coordinates.h"
#include "renderer/unix/gtk_window_interface.h"
#include "renderer/unix/gtk_wrapper_interface.h"

#define MOZC_GTK_VIRTUAL_CALLBACK_0(CLASS, RETURN, METHOD)                \
    static RETURN METHOD ## Thunk(GtkWidget *widget, gpointer userdata) { \
      return reinterpret_cast<CLASS*>(userdata)->METHOD(widget);          \
    }                                                                     \
    virtual RETURN METHOD(GtkWidget *widget);

#define MOZC_GTK_VIRTUAL_CALLBACK_1(CLASS, RETURN, METHOD, ARG1)       \
    static RETURN METHOD ## Thunk(GtkWidget *widget, ARG1 one,         \
                                  gpointer userdata) {                 \
      return reinterpret_cast<CLASS*>(userdata)->METHOD(widget, one);  \
    }                                                                  \
    virtual RETURN METHOD(GtkWidget *widget, ARG1 one);

namespace mozc {
namespace renderer {
namespace gtk {

class GtkWindowBaseTest;
class CandidateWindowTest;

class GtkWindowBase : public GtkWindowInterface {
 public:
  // GtkWindowBase takes ownership of GtkWrapperInterface.
  explicit GtkWindowBase(GtkWrapperInterface *gtk);
  virtual ~GtkWindowBase();

  virtual void ShowWindow();
  virtual void HideWindow();
  virtual GtkWidget *GetWindowWidget();
  virtual GtkWidget *GetCanvasWidget();

  virtual Rect GetWindowRect();
  virtual Size GetWindowSize();
  virtual Point GetWindowPos();

  virtual bool IsActive();
  virtual bool DestroyWindow();
  virtual void Move(const Point &pos);
  virtual void Resize(const Size &size);
  virtual void Redraw();

  virtual void Initialize();
  virtual Size Update(const commands::Candidates &candidates);
  virtual Rect GetCandidateColumnInClientCord() const;

  virtual void OnMouseLeftUp(const Point &pos);
  virtual void OnMouseLeftDown(const Point &pos);
  virtual void OnMouseRightUp(const Point &pos);
  virtual void OnMouseRightDown(const Point &pos);
  virtual void ReloadFontConfig(const string &font_description);

  virtual bool SetSendCommandInterface(
      client::SendCommandInterface *send_command_interface);

  // Callbacks
  MOZC_GTK_VIRTUAL_CALLBACK_0(GtkWindowBase, bool, OnDestroy)
  MOZC_GTK_VIRTUAL_CALLBACK_1(GtkWindowBase, bool, OnPaint, GdkEventExpose *)
  MOZC_GTK_VIRTUAL_CALLBACK_1(GtkWindowBase, gboolean, OnMouseDown,
                              GdkEventButton *)
  MOZC_GTK_VIRTUAL_CALLBACK_1(GtkWindowBase, gboolean, OnMouseUp,
                              GdkEventButton *)

  scoped_ptr<GtkWrapperInterface> gtk_;

 protected:
  client::SendCommandInterface *send_command_interface_;

 private:
  GtkWidget *window_;
  GtkWidget *canvas_;

  friend class GtkWindowBaseTest;
  friend class CandidateWindowTest;

  DISALLOW_COPY_AND_ASSIGN(GtkWindowBase);
};

}  // namespace gtk
}  // namespace renderer
}  // namespace mozc

#undef MOZC_GTK_VIRTUAL_CALLBACK_0
#undef MOZC_GTK_VIRTUAL_CALLBACK_1

#endif  // MOZC_RENDERER_UNIX_GTK_WINDOW_BASE_H_
