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

#include "renderer/unix/gtk_wrapper.h"

namespace mozc {
namespace renderer {
namespace gtk {

GtkWidget *GtkWrapper::GtkWindowNew(GtkWindowType type) {
  return gtk_window_new(type);
}

GtkWidget *GtkWrapper::GtkDrawingAreaNew() {
  return gtk_drawing_area_new();
}

void GtkWrapper::GtkContainerAdd(GtkWidget *container, GtkWidget *widget) {
  gtk_container_add(GTK_CONTAINER(container), widget);
}

void GtkWrapper::GSignalConnect(
    gpointer instance, const gchar *signal, GCallback handler, gpointer data) {
  g_signal_connect(instance, signal, handler, data);
}

void GtkWrapper::GtkWidgetHideAll(GtkWidget *widget) {
  gtk_widget_hide_all(widget);
}

void GtkWrapper::GtkWidgetShowAll(GtkWidget *widget) {
  gtk_widget_show_all(widget);
}

void GtkWrapper::GtkWindowGetSize(GtkWidget *window, int *width, int *height) {
  gtk_window_get_size(GTK_WINDOW(window), width, height);
}

void GtkWrapper::GtkWindowGetPosition(GtkWidget *window, int *x, int *y) {
  gtk_window_get_position(GTK_WINDOW(window), x, y);
}

void GtkWrapper::GtkWindowMove(GtkWidget *window, int x, int y) {
  gtk_window_move(GTK_WINDOW(window), x, y);
}

void GtkWrapper::GtkWindowResize(GtkWidget *window, int width, int height) {
  gtk_window_resize(GTK_WINDOW(window), width, height);
}

void GtkWrapper::GtkMainQuit() {
  gtk_main_quit();
}

void GtkWrapper::GtkWidgetQueueDrawArea(GtkWidget *widget, int x, int y,
                                        int width, int height) {
  gtk_widget_queue_draw_area(widget, x, y, width, height);
}

void GtkWrapper::GObjectUnref(gpointer object) {
  g_object_unref(object);
}

void GtkWrapper::GSourceAttach(GSource *source, GMainContext *context) {
  g_source_attach(source, context);
}

GSource *GtkWrapper::GSourceNew(GSourceFuncs *source_funcs, guint struct_size) {
  return g_source_new(source_funcs, struct_size);
}

void GtkWrapper::GSourceSetCallback(GSource *source,
                                    GSourceFunc func,
                                    gpointer data,
                                    GDestroyNotify notify) {
  g_source_set_callback(source, func, data, notify);
}

void GtkWrapper::GSourceSetCanRecurse(GSource *source, gboolean can_recurse) {
  g_source_set_can_recurse(source, can_recurse);
}

void GtkWrapper::GtkMain() {
  gtk_main();
}

GdkScreen *GtkWrapper::GtkWindowGetScreen(GtkWidget *window) {
  return gtk_window_get_screen(GTK_WINDOW(window));
}

void GtkWrapper::GSourceAddPoll(GSource *source, GPollFD *fd) {
  g_source_add_poll(source, fd);
}

void GtkWrapper::GdkThreadsEnter() {
  gdk_threads_enter();
}

void GtkWrapper::GdkThreadsLeave() {
  gdk_threads_leave();
}

guint GtkWrapper::GdkScreenGetWidth(GdkScreen *screen) {
  return gdk_screen_get_width(screen);
}

guint GtkWrapper::GdkScreenGetHeight(GdkScreen *screen) {
  return gdk_screen_get_height(screen);
}

bool GtkWrapper::GtkWindowIsActive(GtkWidget *window) {
  return gtk_window_is_active(GTK_WINDOW(window));
}

}  // namespace gtk
}  // namespace renderer
}  // namespace mozc
