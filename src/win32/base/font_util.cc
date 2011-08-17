// Copyright 2010-2011, Google Inc.
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

#include "win32/base/font_util.h"

#include "base/base.h"
#include "base/util.h"

namespace mozc {
namespace win32 {

bool FontUtil::ToWinLogFont(
    const LOGFONTW &log_font,
    mozc::commands::RendererCommand::WinLogFont *win_log_font) {
  if (win_log_font == NULL) {
    return false;
  }

  win_log_font->Clear();
  win_log_font->set_height(log_font.lfHeight);
  win_log_font->set_width(log_font.lfWidth);
  win_log_font->set_escapement(log_font.lfEscapement);
  win_log_font->set_orientation(log_font.lfOrientation);
  win_log_font->set_weight(log_font.lfWeight);
  win_log_font->set_italic(log_font.lfItalic != FALSE);
  win_log_font->set_underline(log_font.lfUnderline != FALSE);
  win_log_font->set_strike_out(log_font.lfStrikeOut != FALSE);
  win_log_font->set_char_set(log_font.lfCharSet);
  win_log_font->set_out_precision(log_font.lfOutPrecision);
  win_log_font->set_clip_precision(log_font.lfClipPrecision);
  win_log_font->set_quality(log_font.lfQuality);
  win_log_font->set_pitch_and_family(log_font.lfPitchAndFamily);

  // make sure |log_font.lfFaceName| is properly null-terminated.
  bool null_terminated = false;
  for (size_t i = 0; i < arraysize(log_font.lfFaceName); ++i) {
    if (log_font.lfFaceName[i] == L'\0') {
      null_terminated = true;
      break;
    }
  }

  if (!null_terminated) {
    return false;
  }

  string name;
  mozc::Util::WideToUTF8(log_font.lfFaceName, &name);
  win_log_font->set_face_name(name);

  return true;
}

bool FontUtil::ToLOGFONT(
    const mozc::commands::RendererCommand::WinLogFont &win_log_font,
    LOGFONTW *log_font) {
  if (log_font == NULL) {
    return false;
  }
  if (!win_log_font.IsInitialized()) {
    return false;
  }

  ::ZeroMemory(log_font, sizeof(log_font));

  log_font->lfHeight = win_log_font.height();
  log_font->lfWidth = win_log_font.width();
  log_font->lfEscapement = win_log_font.escapement();
  log_font->lfOrientation = win_log_font.orientation();
  log_font->lfWeight = win_log_font.weight();
  log_font->lfItalic = win_log_font.italic() ? TRUE : FALSE;
  log_font->lfUnderline = win_log_font.underline() ? TRUE : FALSE;
  log_font->lfStrikeOut = win_log_font.strike_out() ? TRUE : FALSE;
  log_font->lfCharSet = win_log_font.char_set();
  log_font->lfOutPrecision = win_log_font.out_precision();
  log_font->lfClipPrecision = win_log_font.clip_precision();
  log_font->lfQuality = win_log_font.quality();
  log_font->lfPitchAndFamily = win_log_font.pitch_and_family();

  wstring face_name;
  mozc::Util::UTF8ToWide(win_log_font.face_name(), &face_name);

  // Although the result of mozc::Util::UTF8ToWide never contains any null
  // character, make sure it again just for the safety.
  face_name += L'\0';
  face_name = face_name.substr(0, face_name.find(L'\0') + 1);

  // +1 is for NULL.
  if (face_name.size() + 1 > arraysize(log_font->lfFaceName)) {
    return false;
  }

  for (size_t i = 0; i < arraysize(log_font->lfFaceName); ++i) {
    if (i < face_name.size()) {
      log_font->lfFaceName[i] = face_name[i];
    } else {
      log_font->lfFaceName[i] = L'\0';
    }
  }

  return true;
}

}  // namespace win32
}  // namespace mozc
