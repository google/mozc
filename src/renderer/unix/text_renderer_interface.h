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

#ifndef MOZC_RENDERER_UNIX_TEXT_RENDERER_INTERFACE_H_
#define MOZC_RENDERER_UNIX_TEXT_RENDERER_INTERFACE_H_

#include <gtk/gtk.h>

#include "base/coordinates.h"
#include "renderer/unix/font_spec_interface.h"

namespace mozc {
namespace renderer {
namespace gtk {

class TextRendererInterface {
 public:
  TextRendererInterface() {}
  virtual ~TextRendererInterface() {}

  virtual void Initialize(GdkDrawable *drawable) = 0;
  // Returns boundary rectangle size of actual rendered text.
  virtual Size GetPixelSize(FontSpecInterface::FONT_TYPE font_type,
                            const string &str) = 0;
  virtual Size GetMultiLinePixelSize(FontSpecInterface::FONT_TYPE font_type,
                                     const string &str, const int width) = 0;

  virtual void RenderText(const string &text, const Rect &rect,
                          FontSpecInterface::FONT_TYPE font_type) = 0;

  virtual void ReloadFontConfig(const string &font_description) = 0;
};

}  // namespace gtk
}  // namespace renderer
}  // namespace mozc
#endif  // MOZC_RENDERER_UNIX_TEXT_RENDERER_INTERFACE_H_
