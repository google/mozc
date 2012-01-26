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

#ifndef MOZC_RENDERER_WIN32_TEXT_RENDERER_H_
#define MOZC_RENDERER_WIN32_TEXT_RENDERER_H_

#include <windows.h>
#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
// Workaround against KB813540
#include <atlbase_mozc.h>
#include <atlapp.h>
#include <atlmisc.h>

#include <string>
#include <vector>

#include "base/base.h"
#include "base/coordinates.h"
#include "base/scoped_ptr.h"

namespace mozc {
namespace renderer {
namespace win32 {
using WTL::CDC;
using WTL::CDCHandle;
using WTL::CFont;
using WTL::CFontHandle;

struct FontInfo;

// text-rect pair for batch text rendering.
struct TextRenderingInfo {
  wstring text;
  Rect rect;
  TextRenderingInfo(const wstring &str, const Rect &r) :
    text(str), rect(r) {
  }
};

// A class which manages text rendering for Windows. This class
// is currently implemented with Win32 GDI functions.
class TextRenderer {
 public:
  // Text rendering styles for a candidate window
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

  TextRenderer();
  ~TextRenderer();
  void Init();
  // Retrieves the font handle
  CFontHandle GetFont(FONT_TYPE font_type) const;
  // Retrieves the font color
  COLORREF GetFontColor(FONT_TYPE font_type) const;
  // Retrieves the font style
  DWORD GetFontStyle(FONT_TYPE font_type) const;
  // Retrieves the bounding box for a given string.
  Size MeasureString(FONT_TYPE font_type, const wstring &str) const;
  Size MeasureStringMultiLine(FONT_TYPE font_type, const wstring &str,
      const int width) const;
  void RenderText(CDCHandle dc, const wstring &text, const Rect &rect,
                  FONT_TYPE font_type) const;
  void RenderText(CDCHandle dc, const vector<TextRenderingInfo> &display_list,
                  FONT_TYPE font_type) const;
 private:
  scoped_array<FontInfo> fonts_;
  mutable CDC mem_dc_;

  DISALLOW_COPY_AND_ASSIGN(TextRenderer);
};
}  // namespace win32
}  // namespace renderer
}  // namespace mozc
#endif  // MOZC_RENDERER_WIN32_TEXT_RENDERER_H_
