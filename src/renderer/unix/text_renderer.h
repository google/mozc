// Copyright 2010-2013, Google Inc.
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

#ifndef MOZC_RENDERER_UNIX_TEXT_RENDERER_H_
#define MOZC_RENDERER_UNIX_TEXT_RENDERER_H_

#include <gtk/gtk.h>

#include "renderer/unix/font_spec_interface.h"
#include "renderer/unix/pango_wrapper_interface.h"
#include "renderer/unix/text_renderer_interface.h"
#include "testing/base/public/gunit_prod.h"

namespace mozc {
namespace renderer {
namespace gtk {

class TextRendererTest;

class TextRenderer : public TextRendererInterface {
 public:
  explicit TextRenderer(FontSpecInterface *font_spec);
  virtual ~TextRenderer() {}

  virtual void Initialize(GdkDrawable *drawable);
  virtual Size GetPixelSize(FontSpecInterface::FONT_TYPE font_type,
                            const string &str);
  virtual Size GetMultiLinePixelSize(FontSpecInterface::FONT_TYPE font_type,
                                     const string &str,
                                     const int width);
  virtual void RenderText(const string &text,
                          const Rect &rect,
                          FontSpecInterface::FONT_TYPE font_type);
  virtual void ReloadFontConfig(const string &font_description);

 private:
  friend class TextRendererTest;
  FRIEND_TEST(TextRendererTest, GetPixelSizeTest);
  FRIEND_TEST(TextRendererTest, GetMultilinePixelSizeTest);
  FRIEND_TEST(TextRendererTest, RenderTextTest);

  void SetUpPangoLayout(const string &str,
                        FontSpecInterface::FONT_TYPE font_type,
                        PangoLayoutWrapperInterface *layout);
  void RenderTextInternal(const string& text,
                          const Rect &rect,
                          FontSpecInterface::FONT_TYPE font_type,
                          PangoLayoutWrapperInterface *layout);
  Size GetPixelSizeInternal(FontSpecInterface::FONT_TYPE font_type,
                            const string &str,
                            PangoLayoutWrapperInterface *layout);
  Size GetMultiLinePixelSizeInternal(FontSpecInterface::FONT_TYPE font_type,
                                     const string &str,
                                     const int width,
                                     PangoLayoutWrapperInterface *layout);
  scoped_ptr<FontSpecInterface> font_spec_;
  scoped_ptr<PangoWrapperInterface> pango_;
};
}  // namespace gtk
}  // namespace renderer
}  // namespace mozc
#endif  // MOZC_RENDERER_UNIX_TEXT_RENDERER_H_
