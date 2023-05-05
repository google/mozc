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

#ifndef MOZC_RENDERER_UNIX_CONST_H_
#define MOZC_RENDERER_UNIX_CONST_H_

#include <cstdint>

#include "base/port.h"

namespace mozc {
namespace renderer {
namespace gtk {

struct RGBA {
  // The range of following variables are [0x00, 0xFF].
  uint8_t red, green, blue, alpha;
};

// Following colors are used for window rendereing.
inline constexpr RGBA kFrameColor = {0x96, 0x96, 0x96, 0xFF};
inline constexpr RGBA kShortcutBackgroundColor = {0xF3, 0xF4, 0xFF, 0xFF};
inline constexpr RGBA kSelectedRowBackgroundColor = {0xD1, 0xEA, 0xFF, 0xFF};
inline constexpr RGBA kDefaultBackgroundColor = {0xFF, 0xFF, 0xFF, 0xFF};
inline constexpr RGBA kSelectedRowFrameColor = {0x7F, 0xAC, 0xDD, 0xFF};
inline constexpr RGBA kIndicatorBackgroundColor = {0xE0, 0xE0, 0xE0, 0xFF};
inline constexpr RGBA kIndicatorColor = {0x75, 0x90, 0xB8, 0xFF};
inline constexpr RGBA kFooterTopColor = {0xFF, 0xFF, 0xFF, 0xFF};
inline constexpr RGBA kFooterBottomColor = {0xEE, 0xEE, 0xEE, 0xFF};

// Following colors are used for font renderering.
inline constexpr RGBA kShortcutColor = {0x61, 0x61, 0x61, 0xFF};
inline constexpr RGBA kDefaultColor = {0x00, 0x00, 0x00, 0xFF};
inline constexpr RGBA kDescriptionColor = {0x88, 0x88, 0x88, 0xFF};
inline constexpr RGBA kFooterIndexColor = {0x4C, 0x4C, 0x4C, 0xFF};
inline constexpr RGBA kFooterLabelColor = {0x4C, 0x4C, 0x4C, 0xFF};
inline constexpr RGBA kFooterSubLabelColor = {0xA7, 0xA7, 0xA7, 0xFF};

inline constexpr RGBA kWhite = {0xFF, 0xFF, 0xFF, 0xFF};

inline constexpr int kWindowBorder = 1;
inline constexpr int kFooterSeparatorHeight = 1;
inline constexpr int kRowRectPadding = 1;
inline constexpr int kIndicatorWidthInDefaultDPI = 4;

inline constexpr char kMinimumCandidateAndDescriptionWidthAsString[] =
    "そのほかの文字種";

// The pango font description allows us to set multiple font names.
// See the detail in
// http://developer.gnome.org/pango/stable/pango-Fonts.html#pango-font-description-from-string
inline constexpr char kDefaultFontDescription[] = "IPAゴシック,SansSerif 11";

}  // namespace gtk
}  // namespace renderer
}  // namespace mozc

#endif  // MOZC_RENDERER_UNIX_CONST_H_
