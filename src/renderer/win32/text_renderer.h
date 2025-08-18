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

#ifndef MOZC_RENDERER_WIN32_TEXT_RENDERER_H_
#define MOZC_RENDERER_WIN32_TEXT_RENDERER_H_

#include <windows.h>

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "absl/types/span.h"
#include "base/coordinates.h"

namespace mozc {
namespace renderer {
namespace win32 {

// text-rect pair for batch text rendering.
struct TextRenderingInfo {
  TextRenderingInfo(std::wstring str, Rect r)
      : text(std::move(str)), rect(std::move(r)) {}

  std::wstring text;
  Rect rect;
};

// An interface which manages text rendering for Windows. This class
// is currently implemented with Win32 GDI and Direct2D.
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

  TextRenderer() = default;
  TextRenderer(TextRenderer&&) = default;
  TextRenderer& operator=(TextRenderer&&) = default;
  virtual ~TextRenderer() = default;

  // Returns an instance of TextRenderer.
  static std::unique_ptr<TextRenderer> Create();

  // Updates font cache.
  virtual void OnThemeChanged() = 0;

  // Retrieves the bounding box for a given string.
  virtual Size MeasureString(FONT_TYPE font_type,
                             std::wstring_view str) const = 0;
  virtual Size MeasureStringMultiLine(FONT_TYPE font_type,
                                      std::wstring_view str,
                                      int width) const = 0;
  // Renders the given |text|.
  virtual void RenderText(HDC dc, std::wstring_view text, const Rect& rect,
                          FONT_TYPE font_type) const = 0;
  virtual void RenderTextList(HDC dc,
                              absl::Span<const TextRenderingInfo> display_list,
                              FONT_TYPE font_type) const = 0;
};

}  // namespace win32
}  // namespace renderer
}  // namespace mozc
#endif  // MOZC_RENDERER_WIN32_TEXT_RENDERER_H_
