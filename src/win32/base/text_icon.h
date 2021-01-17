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

#ifndef MOZC_WIN32_BASE_TEXT_ICON_H_
#define MOZC_WIN32_BASE_TEXT_ICON_H_

#include <stddef.h>
#include <windows.h>

#include "absl/strings/string_view.h"

namespace mozc {
namespace win32 {

class TextIcon {
 public:
  TextIcon() = delete;
  ~TextIcon() = delete;

  // Returns a monochrome icon with rendering |text| by using given |fontname|
  // and |text_color|.
  // The returned icon consists of color bitmap (a.k.a. XOR bitmap) and mask
  // bitmap (a.k.a. AND bitmap). This is mainly because ITfLangBarMgr causes
  // GDI handle leak when ITfLangBarItemButton::GetIcon returns an icon which
  // consists only of mask bitmap (AND bitmap).
  static HICON CreateMonochromeIcon(size_t width, size_t height,
                                    absl::string_view text,
                                    absl::string_view fontname,
                                    COLORREF text_color);
};

}  // namespace win32
}  // namespace mozc

#endif  // MOZC_WIN32_BASE_TEXT_ICON_H_
