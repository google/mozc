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

#ifndef MOZC_RENDERER_UNIX_GTK_WRAPPER_MOCK_H_
#define MOZC_RENDERER_UNIX_GTK_WRAPPER_MOCK_H_

#include "renderer/unix/gtk_wrapper_interface.h"
#include "testing/base/public/gmock.h"

namespace mozc {
namespace renderer {
namespace gtk {

class GtkWrapperMock : public GtkWrapperInterface {
 public:
  MOCK_METHOD(GSource *, GSourceNew,
              (GSourceFuncs* source_funcs, guint struct_size));
  MOCK_METHOD(GdkScreen *, GtkWindowGetScreen, (GtkWidget* window));
  MOCK_METHOD(GtkWidget *, GtkDrawingAreaNew, ());
  MOCK_METHOD(GtkWidget *, GtkWindowNew, (GtkWindowType type));
  MOCK_METHOD(gint, GdkScreenGetMonitorAtPoint,
              (GdkScreen* screen, gint x, gint y));
  MOCK_METHOD(void, GdkScreenGetMonitorGeometry,
              (GdkScreen* screen, gint monitor, GdkRectangle *rectangle));
  MOCK_METHOD(void, GObjectUnref, (gpointer object));
  MOCK_METHOD(void, GSignalConnect,
              (gpointer instance, const gchar *signal, GCallback handler,
               gpointer data));
  MOCK_METHOD(void, GSourceAddPoll, (GSource* source, GPollFD *fd));
  MOCK_METHOD(void, GSourceAttach, (GSource* source, GMainContext *context));
  MOCK_METHOD(void, GSourceSetCallback,
              (GSource* source, GSourceFunc func, gpointer data,
               GDestroyNotify notify));
  MOCK_METHOD(void, GSourceSetCanRecurse,
              (GSource* source, gboolean can_recurse));
  MOCK_METHOD(void, GdkThreadsEnter, ());
  MOCK_METHOD(void, GdkThreadsLeave, ());
  MOCK_METHOD(void, GtkContainerAdd,
              (GtkWidget* container, GtkWidget *widget));
  MOCK_METHOD(void, GtkMain, ());
  MOCK_METHOD(void, GtkMainQuit, ());
  MOCK_METHOD(void, GtkWidgetHideAll, (GtkWidget* widget));
  MOCK_METHOD(void, GtkWidgetQueueDrawArea,
              (GtkWidget* widget, int x, int y, int width, int height));
  MOCK_METHOD(void, GtkWidgetShowAll, (GtkWidget* widget));
  MOCK_METHOD(void, GtkWindowGetPosition, (GtkWidget* window, int *x, int *y));
  MOCK_METHOD(void, GtkWindowGetSize,
              (GtkWidget* window, int *width, int *height));
  MOCK_METHOD(bool, GtkWindowIsActive, (GtkWidget* window));
  MOCK_METHOD(void, GtkWindowMove, (GtkWidget* window, int x, int y));
  MOCK_METHOD(void, GtkWindowResize,
              (GtkWidget* window, int width, int height));
  MOCK_METHOD(void, GtkWidgetAddEvents, (GtkWidget* widget, gint events));
  MOCK_METHOD(void, GtkWidgetRealize, (GtkWidget* widget));
  MOCK_METHOD(void, GdkWindowSetTypeHint,
              (GtkWidget* widget, GdkWindowTypeHint hint));
};

}  // namespace gtk
}  // namespace renderer
}  // namespace mozc

#endif  // MOZC_RENDERER_UNIX_GTK_WRAPPER_MOCK_H_
