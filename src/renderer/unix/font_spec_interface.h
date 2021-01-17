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

#ifndef MOZC_RENDERER_UNIX_FONT_SPEC_INTERFACE_H_
#define MOZC_RENDERER_UNIX_FONT_SPEC_INTERFACE_H_

#include <gtk/gtk.h>
#include <string>

namespace mozc {
namespace renderer {
namespace gtk {

class FontSpecInterface {
 public:
  enum FONT_TYPE {
    FONTSET_SHORTCUT = 0,
    FONTSET_CANDIDATE,
    FONTSET_DESCRIPTION,
    FONTSET_FOOTER_INDEX,
    FONTSET_FOOTER_LABEL,
    FONTSET_FOOTER_SUBLABEL,
    FONTSET_INFOLIST_CAPTION,
    FONTSET_INFOLIST_TITLE,
    FONTSET_INFOLIST_DESCRIPTION,
    SIZE_OF_FONT_TYPE,  // DO NOT DELETE THIS
  };

  FontSpecInterface() {}
  virtual ~FontSpecInterface() {}

  virtual void Reload(const string &font_description) = 0;
  virtual PangoAlignment GetFontAlignment(FONT_TYPE font_type) const = 0;
  // The FontSpec takes the ownership of returned PangoAttrList* instance even
  // this function returns non-const value. This is due to API restriction which
  // is pango_attr_list_copy.
  virtual PangoAttrList *GetFontAttributes(FONT_TYPE font_type) const = 0;
  virtual const PangoFontDescription *GetFontDescription(
      FONT_TYPE font_type) const = 0;
};
}  // namespace gtk
}  // namespace renderer
}  // namespace mozc
#endif  // MOZC_RENDERER_UNIX_FONT_SPEC_INTERFACE_H_
