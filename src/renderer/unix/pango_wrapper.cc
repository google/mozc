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

#include "renderer/unix/pango_wrapper.h"

#include "base/coordinates.h"

namespace mozc {
namespace renderer {
namespace gtk {

PangoLayoutWrapper::PangoLayoutWrapper(PangoContext *context)
    : layout_(pango_layout_new(context)) {}

PangoLayoutWrapper::~PangoLayoutWrapper() { g_object_unref(layout_); }

void PangoLayoutWrapper::SetText(const std::string &text) {
  pango_layout_set_text(layout_, text.c_str(), text.length());
}

void PangoLayoutWrapper::SetAlignment(PangoAlignment align) {
  pango_layout_set_alignment(layout_, align);
}

void PangoLayoutWrapper::SetAttributes(PangoAttrList *attribute) {
  pango_layout_set_attributes(layout_, attribute);
}

void PangoLayoutWrapper::SetFontDescription(
    const PangoFontDescription *font_description) {
  pango_layout_set_font_description(layout_, font_description);
}

void PangoLayoutWrapper::SetWidth(int width) {
  pango_layout_set_width(layout_, width);
}

void PangoLayoutWrapper::SetHeight(int height) {
  pango_layout_set_height(layout_, height);
}

Size PangoLayoutWrapper::GetPixelSize() const {
  Size actual_size;
  pango_layout_get_pixel_size(layout_, &actual_size.width, &actual_size.height);
  return actual_size;
}

PangoLayout *PangoLayoutWrapper::GetPangoLayout() { return layout_; }

void PangoWrapper::RendererDrawLayout(PangoLayoutWrapperInterface *layout,
                                      int x, int y) {
  pango_renderer_draw_layout(renderer_, layout->GetPangoLayout(), x, y);
}

PangoAttrList *PangoWrapper::CopyAttributes(PangoAttrList *attribute) {
  return pango_attr_list_copy(attribute);
}

void PangoWrapper::AttributesUnref(PangoAttrList *attribute) {
  pango_attr_list_unref(attribute);
}

PangoContext *PangoWrapper::GetContext() { return context_; }

PangoWrapper::PangoWrapper(GdkDrawable *drawable) : gc_(gdk_gc_new(drawable)) {
  GdkScreen *screen = gdk_drawable_get_screen(drawable);
  renderer_ = gdk_pango_renderer_new(screen);
  gdk_pango_renderer_set_drawable(GDK_PANGO_RENDERER(renderer_), drawable);
  gdk_pango_renderer_set_gc(GDK_PANGO_RENDERER(renderer_), gc_);
  context_ = gdk_pango_context_get();
}

PangoWrapper::~PangoWrapper() {
  gdk_pango_renderer_set_override_color(GDK_PANGO_RENDERER(renderer_),
                                        PANGO_RENDER_PART_FOREGROUND, nullptr);
  gdk_pango_renderer_set_drawable(GDK_PANGO_RENDERER(renderer_), nullptr);
  gdk_pango_renderer_set_gc(GDK_PANGO_RENDERER(renderer_), nullptr);
  g_object_unref(gc_);
  g_object_unref(context_);
}

}  // namespace gtk
}  // namespace renderer
}  // namespace mozc
