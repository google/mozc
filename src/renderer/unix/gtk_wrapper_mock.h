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

#ifndef MOZC_RENDERER_UNIX_GTK_WRAPPER_MOCK_H_
#define MOZC_RENDERER_UNIX_GTK_WRAPPER_MOCK_H_

#include "renderer/unix/gtk_wrapper_interface.h"
#include "testing/base/public/gmock.h"

namespace mozc {
namespace renderer {
namespace gtk {

class GtkWrapperMock : public GtkWrapperInterface {
 public:
  MOCK_METHOD2(GSourceNew, GSource *(GSourceFuncs *source_funcs,
                                     guint struct_size));
  MOCK_METHOD1(GtkWindowGetScreen, GdkScreen *(GtkWidget *window));
  MOCK_METHOD0(GtkDrawingAreaNew, GtkWidget *());
  MOCK_METHOD1(GtkWindowNew, GtkWidget *(GtkWindowType type));
  MOCK_METHOD1(GdkScreenGetHeight, guint(GdkScreen *screen));
  MOCK_METHOD1(GdkScreenGetWidth, guint(GdkScreen *screen));
  MOCK_METHOD1(GObjectUnref, void(gpointer object));
  MOCK_METHOD4(GSignalConnect, void(gpointer instance,
                                    const gchar *signal,
                                    GCallback handler,
                                    gpointer data));
  MOCK_METHOD2(GSourceAddPoll, void(GSource *source, GPollFD *fd));
  MOCK_METHOD2(GSourceAttach, void(GSource *source, GMainContext *context));
  MOCK_METHOD4(GSourceSetCallback, void(GSource *source,
                                        GSourceFunc func,
                                        gpointer data,
                                        GDestroyNotify notify));
  MOCK_METHOD2(GSourceSetCanRecurse, void(GSource *source,
                                          gboolean can_recurse));
  MOCK_METHOD0(GdkThreadsEnter, void());
  MOCK_METHOD0(GdkThreadsLeave, void());
  MOCK_METHOD2(GtkContainerAdd, void(GtkWidget *container, GtkWidget *widget));
  MOCK_METHOD0(GtkMain, void());
  MOCK_METHOD0(GtkMainQuit, void());
  MOCK_METHOD1(GtkWidgetHideAll, void(GtkWidget *widget));
  MOCK_METHOD5(GtkWidgetQueueDrawArea, void(GtkWidget *widget,
                                            int x,
                                            int y,
                                            int width,
                                            int height));
  MOCK_METHOD1(GtkWidgetShowAll, void(GtkWidget *widget));
  MOCK_METHOD3(GtkWindowGetPosition, void(GtkWidget *window, int *x, int *y));
  MOCK_METHOD3(GtkWindowGetSize, void(GtkWidget *window, int *width,
                                      int *height));
  MOCK_METHOD1(GtkWindowIsActive, bool(GtkWidget *window));
  MOCK_METHOD3(GtkWindowMove, void(GtkWidget *window, int x, int y));
  MOCK_METHOD3(GtkWindowResize, void(GtkWidget *window, int width, int height));
};

}  // namespace gtk
}  // namespace renderer
}  // namespace mozc

#endif  // MOZC_RENDERER_UNIX_GTK_WRAPPER_MOCK_H_
