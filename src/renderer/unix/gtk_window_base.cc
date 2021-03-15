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

#include "renderer/unix/gtk_window_base.h"

#include "base/logging.h"
#include "renderer/unix/gtk_wrapper.h"

namespace mozc {
namespace renderer {
namespace gtk {

GtkWindowBase::GtkWindowBase(GtkWrapperInterface *gtk)
    : gtk_(gtk),
      send_command_interface_(nullptr),
      window_(gtk_->GtkWindowNew(GTK_WINDOW_POPUP)),
      canvas_(gtk_->GtkDrawingAreaNew()) {
  gtk_->GSignalConnect(window_, "destroy", G_CALLBACK(OnDestroyThunk), this);
  gtk_->GtkWidgetAddEvents(window_, GDK_BUTTON_PRESS_MASK);
  gtk_->GSignalConnect(window_, "button-press-event",
                       G_CALLBACK(OnMouseDownThunk), this);

  gtk_->GtkWidgetAddEvents(window_, GDK_BUTTON_RELEASE_MASK);
  gtk_->GSignalConnect(window_, "button-release-event",
                       G_CALLBACK(OnMouseUpThunk), this);

  gtk_->GSignalConnect(canvas_, "expose-event", G_CALLBACK(OnPaintThunk), this);
  gtk_->GtkContainerAdd(window_, canvas_);
  gtk_->GtkWidgetRealize(window_);
  gtk_->GdkWindowSetTypeHint(window_, GDK_WINDOW_TYPE_HINT_POPUP_MENU);
}

GtkWindowBase::~GtkWindowBase() {}

void GtkWindowBase::ShowWindow() { gtk_->GtkWidgetShowAll(window_); }

void GtkWindowBase::HideWindow() { gtk_->GtkWidgetHideAll(window_); }

GtkWidget *GtkWindowBase::GetWindowWidget() { return window_; }

GtkWidget *GtkWindowBase::GetCanvasWidget() { return canvas_; }

Point GtkWindowBase::GetWindowPos() {
  Point origin;
  gtk_->GtkWindowGetPosition(window_, &origin.x, &origin.y);
  return origin;
}

Size GtkWindowBase::GetWindowSize() {
  Size size;
  gtk_->GtkWindowGetSize(window_, &size.width, &size.height);
  return size;
}

Rect GtkWindowBase::GetWindowRect() {
  return Rect(GetWindowPos(), GetWindowSize());
}

bool GtkWindowBase::IsActive() { return gtk_->GtkWindowIsActive(window_); }

bool GtkWindowBase::DestroyWindow() {
  // TODO(nona): Implement this
  return false;
}

void GtkWindowBase::Move(const Point &pos) {
  gtk_->GtkWindowMove(window_, pos.x, pos.y);
}

void GtkWindowBase::Resize(const Size &size) {
  gtk_->GtkWindowResize(window_, size.width, size.height);
}

void GtkWindowBase::Redraw() {
  Size size = GetWindowSize();
  gtk_->GtkWidgetQueueDrawArea(window_, 0, 0, size.width, size.height);
}

// Callbacks
bool GtkWindowBase::OnDestroy(GtkWidget *widget) {
  gtk_->GtkMainQuit();
  return true;
}

bool GtkWindowBase::OnPaint(GtkWidget *widget, GdkEventExpose *event) {
  return true;
}

void GtkWindowBase::Initialize() {
  // Do nothing, there are no initialization on GtkWindowBase class.
}

Size GtkWindowBase::Update(const commands::Candidates &candidates) {
  // Do nothing, because this method should be overridden.
  DLOG(FATAL) << "Unexpected function call.";
  return Size(0, 0);
}

Rect GtkWindowBase::GetCandidateColumnInClientCord() const {
  // Do nothing, this method should be overridden and only used by candidate
  // window.
  return Rect(0, 0, 0, 0);
}

gboolean GtkWindowBase::OnMouseDown(GtkWidget *widget, GdkEventButton *event) {
  // GdkEventButton::x and y is defined as double, but they seems having only
  // integral part.
  Point pos(static_cast<int>(event->x), static_cast<int>(event->y));
  if (event->state & GDK_BUTTON1_MASK) {  // Mouse left click mask
    OnMouseLeftDown(pos);
  } else if (event->state & GDK_BUTTON3_MASK) {  // Mouse right click mask
    OnMouseRightDown(pos);
  }
  // TODO(nona): Change the interface design if we need to control whether the
  //             event is consumed here or not.
  return TRUE;  // event consumed.
}

gboolean GtkWindowBase::OnMouseUp(GtkWidget *widget, GdkEventButton *event) {
  Point pos(static_cast<int>(event->x), static_cast<int>(event->y));
  if (event->state & GDK_BUTTON1_MASK) {
    OnMouseLeftUp(pos);
  } else if (event->state & GDK_BUTTON3_MASK) {
    OnMouseRightUp(pos);
  }
  // TODO(nona): Change the interface design if we need to control whether the
  //             event is consumed here or not.
  return TRUE;  // event consumed.
}

void GtkWindowBase::OnMouseLeftUp(const Point &pos) {}

void GtkWindowBase::OnMouseLeftDown(const Point &pos) {}

void GtkWindowBase::OnMouseRightUp(const Point &pos) {}

void GtkWindowBase::OnMouseRightDown(const Point &pos) {}

bool GtkWindowBase::SetSendCommandInterface(
    client::SendCommandInterface *send_command_interface) {
  send_command_interface_ = send_command_interface;
  return true;
}

void GtkWindowBase::ReloadFontConfig(const std::string &font_description) {
  // do nothing.
}

}  // namespace gtk
}  // namespace renderer
}  // namespace mozc
