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

#ifndef MOZC_RENDERER_UNIX_GTK_WRAPPER_H_
#define MOZC_RENDERER_UNIX_GTK_WRAPPER_H_

#include "renderer/unix/gtk_wrapper_interface.h"

namespace mozc {
namespace renderer {
namespace gtk {

class GtkWrapper : public GtkWrapperInterface {
 public:
  virtual GSource *GSourceNew(GSourceFuncs *source_funcs, guint struct_size);
  virtual GdkScreen *GtkWindowGetScreen(GtkWidget *window);
  virtual GtkWidget *GtkDrawingAreaNew();
  virtual GtkWidget *GtkWindowNew(GtkWindowType type);
  virtual guint GdkScreenGetHeight(GdkScreen *screen);
  virtual guint GdkScreenGetWidth(GdkScreen *screen);
  virtual void GObjectUnref(gpointer object);
  virtual void GSignalConnect(gpointer instance,
                              const gchar *detailed_signal,
                              GCallback c_handler,
                              gpointer data);
  virtual void GSourceAddPoll(GSource *source, GPollFD *fd);
  virtual void GSourceAttach(GSource *source, GMainContext *context);
  virtual void GSourceSetCallback(GSource *source,
                                  GSourceFunc func,
                                  gpointer data,
                                  GDestroyNotify notify);
  virtual void GSourceSetCanRecurse(GSource *source, gboolean can_recurse);
  virtual void GdkThreadsEnter();
  virtual void GdkThreadsLeave();
  virtual void GtkContainerAdd(GtkWidget *container, GtkWidget *widget);
  virtual void GtkMain();
  virtual void GtkMainQuit();
  virtual void GtkWidgetHideAll(GtkWidget *widget);
  virtual void GtkWidgetQueueDrawArea(GtkWidget *widget,
                                      int x,
                                      int y,
                                      int width,
                                      int height);
  virtual void GtkWidgetShowAll(GtkWidget *widget);
  virtual void GtkWindowGetPosition(GtkWidget *window, int *x, int *y);
  virtual void GtkWindowGetSize(GtkWidget *window, int *width, int *height);
  virtual bool GtkWindowIsActive(GtkWidget *window);
  virtual void GtkWindowMove(GtkWidget *window, int x, int y);
  virtual void GtkWindowResize(GtkWidget *window, int width, int height);
};

}  // namespace gtk
}  // namespace renderer
}  // namespace mozc

#endif  // MOZC_RENDERER_UNIX_GTK_WRAPPER_H_
