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

#ifndef MOZC_RENDERER_UNIX_PANGO_WRAPPER_INTERFACE_H_
#define MOZC_RENDERER_UNIX_PANGO_WRAPPER_INTERFACE_H_

#include <gtk/gtk.h>
#include <string>

namespace mozc {

struct Size;

namespace renderer {
namespace gtk {

class PangoLayoutWrapperInterface {
 public:
  PangoLayoutWrapperInterface() {}
  virtual ~PangoLayoutWrapperInterface() {}

  virtual void SetText(const string &text) = 0;
  virtual void SetAlignment(PangoAlignment align) = 0;
  virtual void SetAttributes(PangoAttrList *attr) = 0;
  virtual void SetFontDescription(
      const PangoFontDescription *font_description) = 0;
  virtual void SetWidth(int width) = 0;
  virtual void SetHeight(int height) = 0;
  virtual Size GetPixelSize() const = 0;
  virtual PangoLayout *GetPangoLayout() = 0;
};

class PangoWrapperInterface {
 public:
  PangoWrapperInterface() {}
  virtual ~PangoWrapperInterface() {}
  virtual void RendererDrawLayout(PangoLayoutWrapperInterface *layout,
                                  int x, int y) = 0;
  virtual PangoAttrList *CopyAttributes(PangoAttrList *attr) = 0;
  virtual void AttributesUnref(PangoAttrList *attr) = 0;
  virtual PangoContext *GetContext() = 0;
};
}  // namespace gtk
}  // namespace renderer
}  // namespace mozc

#endif  // MOZC_RENDERER_UNIX_PANGO_WRAPPER_INTERFACE_H_
