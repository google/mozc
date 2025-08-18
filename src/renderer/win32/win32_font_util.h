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

#ifndef MOZC_RENDERER_WIN32_WIN32_FONT_UTIL_H_
#define MOZC_RENDERER_WIN32_WIN32_FONT_UTIL_H_

#include <commctrl.h>  // for CCSIZEOF_STRUCT
#include <windows.h>

namespace mozc {
namespace renderer {
namespace win32 {

inline LOGFONT GetMessageBoxLogFont() {
  NONCLIENTMETRICS info = {};
  info.cbSize = CCSIZEOF_STRUCT(NONCLIENTMETRICS, iPaddedBorderWidth);
  if (::SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(info), &info, 0)) {
    return info.lfMessageFont;
  } else {
    // Fallback font.
    return {
        .lfHeight = -12,
        .lfWidth = 0,
        .lfEscapement = 0,
        .lfOrientation = 0,
        .lfWeight = 400,
        .lfItalic = 0,
        .lfUnderline = 0,
        .lfStrikeOut = 0,
        .lfCharSet = 1,
        .lfOutPrecision = 0,
        .lfClipPrecision = 0,
        .lfQuality = 0,
        .lfPitchAndFamily = 0,
        .lfFaceName = L"Segoe UI",
    };
  }
}

}  // namespace win32
}  // namespace renderer
}  // namespace mozc

#endif  // MOZC_RENDERER_WIN32_WIN32_FONT_UTIL_H_
