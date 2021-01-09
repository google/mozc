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

#ifndef MOZC_RENDERER_UNIX_FONT_SPEC_H_
#define MOZC_RENDERER_UNIX_FONT_SPEC_H_

#include <memory>
#include <vector>

#include "base/port.h"
#include "renderer/unix/font_spec_interface.h"
#include "renderer/unix/gtk_wrapper_interface.h"

namespace mozc {
namespace renderer {
namespace gtk {

struct FontInfo;

class FontSpec : public FontSpecInterface {
 public:
  explicit FontSpec(GtkWrapperInterface *gtk);
  virtual ~FontSpec();

  virtual void Reload(const string &font_description);
  virtual PangoAlignment GetFontAlignment(FONT_TYPE font_type) const;
  virtual PangoAttrList *GetFontAttributes(FONT_TYPE font_type) const;
  virtual const PangoFontDescription *GetFontDescription(
      FONT_TYPE font_type) const;

 protected:
  void LoadFontSpec(const string &font_description);
  void ReleaseFontSpec();
  std::vector<FontInfo> fonts_;
  bool is_initialized_;
  std::unique_ptr<GtkWrapperInterface> gtk_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FontSpec);
};

}  // namespace gtk
}  // namespace renderer
}  // namespace mozc
#endif  // MOZC_RENDERER_UNIX_FONT_SPEC_H_
