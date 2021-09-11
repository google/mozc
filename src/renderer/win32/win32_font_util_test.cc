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

#include <windows.h>

#include <string>

#include "base/util.h"
#include "protocol/renderer_command.pb.h"
#include "renderer/win32/win32_font_util.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace win32 {
TEST(FontUtilTest, ToLOGFONTWithTooLongFaceName) {
  LOGFONT log_font = {18,
                      11,
                      2700,
                      1800,
                      FW_NORMAL,
                      TRUE,
                      FALSE,
                      TRUE,
                      SYMBOL_CHARSET,
                      OUT_DEFAULT_PRECIS,
                      CLIP_DEFAULT_PRECIS,
                      ANTIALIASED_QUALITY,
                      FF_SCRIPT,
                      L""};

  // make a string which is not null-terminated.
  for (size_t i = 0; i < std::size(log_font.lfFaceName); ++i) {
    log_font.lfFaceName[i] = L' ';
  }

  // Conversion should fail.
  mozc::commands::RendererCommand::WinLogFont win_log_font;
  EXPECT_FALSE(FontUtil::ToWinLogFont(log_font, &win_log_font));
}

TEST(FontUtilTest, ToWinLogFontWithTooLongFaceName) {
  mozc::commands::RendererCommand::WinLogFont win_log_font;
  win_log_font.set_height(18);
  win_log_font.set_width(11);
  win_log_font.set_escapement(2700);
  win_log_font.set_orientation(1800);
  win_log_font.set_weight(FW_NORMAL);
  win_log_font.set_italic(true);
  win_log_font.set_underline(false);
  win_log_font.set_strike_out(true);
  win_log_font.set_char_set(SYMBOL_CHARSET);
  win_log_font.set_out_precision(OUT_DEFAULT_PRECIS);
  win_log_font.set_clip_precision(CLIP_DEFAULT_PRECIS);
  win_log_font.set_quality(ANTIALIASED_QUALITY);
  win_log_font.set_pitch_and_family(FF_SCRIPT);

  LOGFONT log_font = {};
  std::wstring too_long_face_name(L' ', std::size(log_font.lfFaceName));
  std::string face_name;
  mozc::Util::WideToUtf8(too_long_face_name, &face_name);
  win_log_font.set_face_name(face_name);

  // Conversion should fail.
  EXPECT_FALSE(FontUtil::ToLOGFONT(win_log_font, &log_font));
}

TEST(FontUtilTest, RoundtripToWinLogFont) {
  const LOGFONT original = {18,
                            11,
                            2700,
                            1800,
                            FW_NORMAL,
                            TRUE,
                            FALSE,
                            TRUE,
                            SYMBOL_CHARSET,
                            OUT_DEFAULT_PRECIS,
                            CLIP_DEFAULT_PRECIS,
                            ANTIALIASED_QUALITY,
                            FF_SCRIPT,
                            L"MS Sans Serif"};

  mozc::commands::RendererCommand::WinLogFont win_log_font;
  EXPECT_TRUE(FontUtil::ToWinLogFont(original, &win_log_font));

  LOGFONT result = {};
  EXPECT_TRUE(FontUtil::ToLOGFONT(win_log_font, &result));

  EXPECT_EQ(original.lfHeight, result.lfHeight);
  EXPECT_EQ(original.lfWidth, result.lfWidth);
  EXPECT_EQ(original.lfEscapement, result.lfEscapement);
  EXPECT_EQ(original.lfOrientation, result.lfOrientation);
  EXPECT_EQ(original.lfWeight, result.lfWeight);
  EXPECT_EQ(original.lfItalic, result.lfItalic);
  EXPECT_EQ(original.lfUnderline, result.lfUnderline);
  EXPECT_EQ(original.lfStrikeOut, result.lfStrikeOut);
  EXPECT_EQ(original.lfCharSet, result.lfCharSet);
  EXPECT_EQ(original.lfOutPrecision, result.lfOutPrecision);
  EXPECT_EQ(original.lfClipPrecision, result.lfClipPrecision);
  EXPECT_EQ(original.lfQuality, result.lfQuality);
  EXPECT_EQ(original.lfPitchAndFamily, result.lfPitchAndFamily);
  EXPECT_STREQ(original.lfFaceName, result.lfFaceName);
}

TEST(FontUtilTest, RoundtripToLOGFONT) {
  mozc::commands::RendererCommand::WinLogFont original;
  original.set_height(18);
  original.set_width(11);
  original.set_escapement(2700);
  original.set_orientation(1800);
  original.set_weight(FW_NORMAL);
  original.set_italic(true);
  original.set_underline(false);
  original.set_strike_out(true);
  original.set_char_set(SYMBOL_CHARSET);
  original.set_out_precision(OUT_DEFAULT_PRECIS);
  original.set_clip_precision(CLIP_DEFAULT_PRECIS);
  original.set_quality(ANTIALIASED_QUALITY);
  original.set_pitch_and_family(FF_SCRIPT);
  std::string face_name;
  mozc::Util::WideToUtf8(L"MS Sans Serif", &face_name);
  original.set_face_name(face_name);

  LOGFONT log_font = {};
  EXPECT_TRUE(FontUtil::ToLOGFONT(original, &log_font));

  mozc::commands::RendererCommand::WinLogFont result;
  EXPECT_TRUE(FontUtil::ToWinLogFont(log_font, &result));

  EXPECT_EQ(original.height(), result.height());
  EXPECT_EQ(original.width(), result.width());
  EXPECT_EQ(original.escapement(), result.escapement());
  EXPECT_EQ(original.orientation(), result.orientation());
  EXPECT_EQ(original.weight(), result.weight());
  EXPECT_EQ(original.italic(), result.italic());
  EXPECT_EQ(original.underline(), result.underline());
  EXPECT_EQ(original.strike_out(), result.strike_out());
  EXPECT_EQ(original.char_set(), result.char_set());
  EXPECT_EQ(original.out_precision(), result.out_precision());
  EXPECT_EQ(original.clip_precision(), result.clip_precision());
  EXPECT_EQ(original.quality(), result.quality());
  EXPECT_EQ(original.pitch_and_family(), result.pitch_and_family());
  EXPECT_EQ(original.face_name(), result.face_name());
}
}  // namespace win32
}  // namespace mozc
