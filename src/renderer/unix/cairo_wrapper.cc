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

#include "renderer/unix/cairo_wrapper.h"

namespace mozc {
namespace renderer {
namespace gtk {

CairoWrapper::CairoWrapper(GdkWindow *window)
    : cairo_context_(gdk_cairo_create(window)) {}

CairoWrapper::~CairoWrapper() { cairo_destroy(cairo_context_); }

void CairoWrapper::Save() { cairo_save(cairo_context_); }

void CairoWrapper::Restore() { cairo_restore(cairo_context_); }

void CairoWrapper::SetSourceRGBA(double r, double g, double b, double a) {
  cairo_set_source_rgba(cairo_context_, r, g, b, a);
}

void CairoWrapper::Rectangle(double x, double y, double width, double height) {
  cairo_rectangle(cairo_context_, x, y, width, height);
}

void CairoWrapper::Fill() { cairo_fill(cairo_context_); }

void CairoWrapper::SetLineWidth(double width) {
  cairo_set_line_width(cairo_context_, width);
}

void CairoWrapper::Stroke() { cairo_stroke(cairo_context_); }

void CairoWrapper::MoveTo(double x, double y) {
  cairo_move_to(cairo_context_, x, y);
}

void CairoWrapper::LineTo(double x, double y) {
  cairo_line_to(cairo_context_, x, y);
}

}  // namespace gtk
}  // namespace renderer
}  // namespace mozc
