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

#ifndef MOZC_RENDERER_UNIX_GTK_WRAPPER_INTERFACE_H_
#define MOZC_RENDERER_UNIX_GTK_WRAPPER_INTERFACE_H_

#include <gtk/gtk.h>

namespace mozc {
namespace renderer {
namespace gtk {

// This class contains simple wrapper functions which has just same argument and
// call actual function. This class is used for testing and we do not intend
// making general GTK C++ wrapper class.
class GtkWrapperInterface {
 public:
  explicit GtkWrapperInterface() {}
  virtual ~GtkWrapperInterface() {}
  virtual GSource *GSourceNew(GSourceFuncs *source_funcs,
                              guint struct_size) = 0;
  virtual GdkScreen *GtkWindowGetScreen(GtkWidget *window) = 0;
  virtual GtkWidget *GtkDrawingAreaNew() = 0;
  virtual GtkWidget *GtkWindowNew(GtkWindowType type) = 0;
  virtual guint GdkScreenGetHeight(GdkScreen *screen) = 0;
  virtual guint GdkScreenGetWidth(GdkScreen *screen) = 0;
  virtual void GObjectUnref(gpointer object) = 0;
  virtual void GSignalConnect(gpointer instance,
                              const gchar *signal,
                              GCallback handler,
                              gpointer data) = 0;
  virtual void GSourceAddPoll(GSource *source, GPollFD *fd) = 0;
  virtual void GSourceAttach(GSource *source, GMainContext *context) = 0;
  virtual void GSourceSetCallback(GSource *source,
                                  GSourceFunc func,
                                  gpointer data,
                                  GDestroyNotify notify) = 0;
  virtual void GSourceSetCanRecurse(GSource *source, gboolean can_recurse) = 0;
  virtual void GdkThreadsEnter() = 0;
  virtual void GdkThreadsLeave() = 0;
  virtual void GtkContainerAdd(GtkWidget *container, GtkWidget *widget) = 0;
  virtual void GtkMain() = 0;
  virtual void GtkMainQuit() = 0;
  virtual void GtkWidgetHideAll(GtkWidget *widget) = 0;
  virtual void GtkWidgetQueueDrawArea(GtkWidget *widget,
                                      int x,
                                      int y,
                                      int width,
                                      int height) = 0;
  virtual void GtkWidgetShowAll(GtkWidget *widget) = 0;
  virtual void GtkWindowGetPosition(GtkWidget *window, int *x, int *y) = 0;
  virtual void GtkWindowGetSize(GtkWidget *window, int *width, int *height) = 0;
  virtual bool GtkWindowIsActive(GtkWidget *window) = 0;
  virtual void GtkWindowMove(GtkWidget *window, int x, int y) = 0;
  virtual void GtkWindowResize(GtkWidget *window, int width, int height) = 0;
  virtual void GtkWidgetAddEvents(GtkWidget *widget, gint events) = 0;
};

}  // namespace gtk
}  // namespace renderer
}  // namespace mozc

#endif  // MOZC_RENDERER_UNIX_GTK_WRAPPER_INTERFACE_H_
