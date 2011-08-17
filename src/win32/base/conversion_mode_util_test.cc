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

#if defined(OS_WINDOWS)
#include <windows.h>
#include <imm.h>
#endif  // OS_WINDOWS

#include "session/commands.pb.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "win32/base/conversion_mode_util.h"

namespace mozc {
namespace win32 {
#if !defined(OS_WINDOWS)
// Use the same naming convention to emulate imm32.h.
const uint32 IME_CMODE_ALPHANUMERIC = 0x0;
const uint32 IME_CMODE_NATIVE = 0x1;
const uint32 IME_CMODE_KATAKANA = 0x2;
const uint32 IME_CMODE_LANGUAGE = 0x3;
const uint32 IME_CMODE_FULLSHAPE = 0x8;
const uint32 IME_CMODE_ROMAN = 0x10;
const uint32 IME_CMODE_CHARCODE = 0x20;
const uint32 IME_CMODE_HANJACONVERT = 0x40;
const uint32 IME_CMODE_SOFTKBD = 0x80;
const uint32 IME_CMODE_NOCONVERSION = 0x100;
const uint32 IME_CMODE_EUDC = 0x200;
const uint32 IME_CMODE_SYMBOL = 0x400;
const uint32 IME_CMODE_SYMBOL = 0x800;
#endif  // !OS_WINDOWS

TEST(ConversionModeUtilTest, ToNativeMode) {
  uint32 native_code = 0;

  native_code = 0;
  EXPECT_TRUE(ConversionModeUtil::ToNativeMode(
      mozc::commands::DIRECT, false, &native_code));
  EXPECT_EQ(IME_CMODE_ALPHANUMERIC | IME_CMODE_ROMAN, native_code);

  native_code = 0;
  EXPECT_TRUE(ConversionModeUtil::ToNativeMode(
      mozc::commands::HIRAGANA, false, &native_code));
  EXPECT_EQ(
      IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN, native_code);

  native_code = 0;
  EXPECT_TRUE(ConversionModeUtil::ToNativeMode(
      mozc::commands::HALF_KATAKANA, false, &native_code));
  EXPECT_EQ(
      IME_CMODE_NATIVE | IME_CMODE_KATAKANA | IME_CMODE_ROMAN, native_code);

  native_code = 0;
  EXPECT_TRUE(ConversionModeUtil::ToNativeMode(
      mozc::commands::HALF_ASCII, false, &native_code));
  EXPECT_EQ(IME_CMODE_ALPHANUMERIC | IME_CMODE_ROMAN, native_code);

  native_code = 0;
  EXPECT_TRUE(ConversionModeUtil::ToNativeMode(
      mozc::commands::FULL_ASCII, false, &native_code));
  EXPECT_EQ(IME_CMODE_ALPHANUMERIC | IME_CMODE_FULLSHAPE |
            IME_CMODE_ROMAN, native_code);

  native_code = 0;
  EXPECT_TRUE(ConversionModeUtil::ToNativeMode(
      mozc::commands::FULL_KATAKANA, false, &native_code));
  EXPECT_EQ(IME_CMODE_NATIVE | IME_CMODE_KATAKANA |
            IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN, native_code);
}

TEST(ConversionModeUtilTest, ToNativeModeWithKanaLocked) {
  uint32 native_code = 0;

  native_code = 0;
  EXPECT_TRUE(ConversionModeUtil::ToNativeMode(
      mozc::commands::DIRECT, true, &native_code));
  EXPECT_EQ(IME_CMODE_ALPHANUMERIC, native_code);

  native_code = 0;
  EXPECT_TRUE(ConversionModeUtil::ToNativeMode(
      mozc::commands::HIRAGANA, true, &native_code));
  EXPECT_EQ(IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE, native_code);

  native_code = 0;
  EXPECT_TRUE(ConversionModeUtil::ToNativeMode(
      mozc::commands::HALF_KATAKANA, true, &native_code));
  EXPECT_EQ(IME_CMODE_NATIVE | IME_CMODE_KATAKANA, native_code);

  native_code = 0;
  EXPECT_TRUE(ConversionModeUtil::ToNativeMode(
      mozc::commands::HALF_ASCII, true, &native_code));
  EXPECT_EQ(IME_CMODE_ALPHANUMERIC, native_code);

  native_code = 0;
  EXPECT_TRUE(ConversionModeUtil::ToNativeMode(
      mozc::commands::FULL_ASCII, true, &native_code));
  EXPECT_EQ(IME_CMODE_ALPHANUMERIC | IME_CMODE_FULLSHAPE, native_code);

  native_code = 0;
  EXPECT_TRUE(ConversionModeUtil::ToNativeMode(
      mozc::commands::FULL_KATAKANA, true, &native_code));
  EXPECT_EQ(IME_CMODE_NATIVE | IME_CMODE_KATAKANA |
            IME_CMODE_FULLSHAPE, native_code);
}

TEST(ConversionModeUtilTest, ToMozcMode) {
  mozc::commands::CompositionMode mode = mozc::commands::DIRECT;

  // Check for HALF_ASCII
  // NOTE: IME_CMODE_ALPHANUMERIC will be converted to HALF_ASCII, not DIRECT.
  mode = mozc::commands::DIRECT;
  EXPECT_TRUE(ConversionModeUtil::ToMozcMode(IME_CMODE_ALPHANUMERIC, &mode));
  EXPECT_EQ(mozc::commands::HALF_ASCII, mode);

  // IME_CMODE_ROMAN has no effect in this conversion.
  mode = mozc::commands::DIRECT;
  EXPECT_TRUE(ConversionModeUtil::ToMozcMode(
      IME_CMODE_ALPHANUMERIC | IME_CMODE_ROMAN, &mode));
  EXPECT_EQ(mozc::commands::HALF_ASCII, mode);

  // Check for FULL_ASCII
  mode = mozc::commands::DIRECT;
  EXPECT_TRUE(ConversionModeUtil::ToMozcMode(
      IME_CMODE_ALPHANUMERIC | IME_CMODE_FULLSHAPE, &mode));
  EXPECT_EQ(mozc::commands::FULL_ASCII, mode);

  // IME_CMODE_ROMAN has no effect in this conversion.
  mode = mozc::commands::DIRECT;
  EXPECT_TRUE(ConversionModeUtil::ToMozcMode(
      IME_CMODE_ALPHANUMERIC | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN, &mode));
  EXPECT_EQ(mozc::commands::FULL_ASCII, mode);

  // Check for HIRAGANA
  mode = mozc::commands::DIRECT;
  EXPECT_TRUE(ConversionModeUtil::ToMozcMode(
      IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE, &mode));
  EXPECT_EQ(mozc::commands::HIRAGANA, mode);

  // IME_CMODE_ROMAN has no effect in this conversion.
  mode = mozc::commands::DIRECT;
  EXPECT_TRUE(ConversionModeUtil::ToMozcMode(
      IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN, &mode));
  EXPECT_EQ(mozc::commands::HIRAGANA, mode);

  // There is no "HALF_HIRAGANA"
  mode = mozc::commands::DIRECT;
  EXPECT_FALSE(ConversionModeUtil::ToMozcMode(
      IME_CMODE_NATIVE | IME_CMODE_ROMAN, &mode));

  // IME_CMODE_ROMAN has no effect in this conversion.
  mode = mozc::commands::DIRECT;
  EXPECT_FALSE(ConversionModeUtil::ToMozcMode(
      IME_CMODE_NATIVE, &mode));

  // Check for FULL_KATAKANA
  mode = mozc::commands::DIRECT;
  EXPECT_TRUE(ConversionModeUtil::ToMozcMode(
      IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_KATAKANA, &mode));
  EXPECT_EQ(mozc::commands::FULL_KATAKANA, mode);

  // IME_CMODE_ROMAN has no effect in this conversion.
  mode = mozc::commands::DIRECT;
  EXPECT_TRUE(ConversionModeUtil::ToMozcMode(
      IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE |
      IME_CMODE_KATAKANA | IME_CMODE_ROMAN, &mode));
  EXPECT_EQ(mozc::commands::FULL_KATAKANA, mode);

  // Check for HALF_KATAKANA
  mode = mozc::commands::DIRECT;
  EXPECT_TRUE(ConversionModeUtil::ToMozcMode(
      IME_CMODE_NATIVE | IME_CMODE_KATAKANA, &mode));
  EXPECT_EQ(mozc::commands::HALF_KATAKANA, mode);

  // IME_CMODE_ROMAN has no effect in this conversion.
  mode = mozc::commands::DIRECT;
  EXPECT_TRUE(ConversionModeUtil::ToMozcMode(
      IME_CMODE_NATIVE | IME_CMODE_KATAKANA | IME_CMODE_ROMAN, &mode));
  EXPECT_EQ(mozc::commands::HALF_KATAKANA, mode);
}

TEST(ConversionModeUtilTest, ToMozcModeUnsupportedModes) {
  mozc::commands::CompositionMode mode = mozc::commands::DIRECT;

  // IME_CMODE_KATAKANA should be used with IME_CMODE_NATIVE.
  mode = mozc::commands::DIRECT;
  EXPECT_FALSE(ConversionModeUtil::ToMozcMode(IME_CMODE_KATAKANA, &mode));

  // IME_CMODE_CHARCODE is not supported yet.
  mode = mozc::commands::DIRECT;
  EXPECT_FALSE(ConversionModeUtil::ToMozcMode(IME_CMODE_CHARCODE, &mode));

  // IME_CMODE_HANJACONVERT is not supported yet.
  mode = mozc::commands::DIRECT;
  EXPECT_FALSE(ConversionModeUtil::ToMozcMode(IME_CMODE_HANJACONVERT, &mode));

  // IME_CMODE_SOFTKBD is not supported yet.
  mode = mozc::commands::DIRECT;
  EXPECT_FALSE(ConversionModeUtil::ToMozcMode(IME_CMODE_SOFTKBD, &mode));

  // IME_CMODE_NOCONVERSION is not supported yet.
  mode = mozc::commands::DIRECT;
  EXPECT_FALSE(ConversionModeUtil::ToMozcMode(IME_CMODE_NOCONVERSION, &mode));

  // IME_CMODE_EUDC is not supported yet.
  mode = mozc::commands::DIRECT;
  EXPECT_FALSE(ConversionModeUtil::ToMozcMode(IME_CMODE_EUDC, &mode));

  // IME_CMODE_SYMBOL is not supported yet.
  mode = mozc::commands::DIRECT;
  EXPECT_FALSE(ConversionModeUtil::ToMozcMode(IME_CMODE_SYMBOL, &mode));
}

TEST(ConversionModeUtilTest, ConvertStatusFromMozcToNative) {
  bool is_open = false;
  DWORD native_mode = 0;
  mozc::commands::Status status;

  // Should succeeds only when |status| has both |activated| and |mode|.
  is_open = false;
  native_mode = 0;
  status.Clear();
  EXPECT_FALSE(ConversionModeUtil::ConvertStatusFromMozcToNative(
      status, false, &is_open, &native_mode));

  // Should succeeds only when |status| has both |activated| and |mode|.
  is_open = false;
  native_mode = 0;
  status.Clear();
  status.set_activated(true);
  EXPECT_FALSE(ConversionModeUtil::ConvertStatusFromMozcToNative(
      status, false, &is_open, &native_mode));

  // Should succeeds only when |status| has both |activated| and |mode|.
  is_open = false;
  native_mode = 0;
  status.Clear();
  status.set_mode(commands::HIRAGANA);
  EXPECT_FALSE(ConversionModeUtil::ConvertStatusFromMozcToNative(
      status, false, &is_open, &native_mode));

  // commands::DIRECT should not be used in |status|.
  is_open = false;
  native_mode = 0;
  status.Clear();
  status.set_activated(false);
  status.set_mode(commands::DIRECT);
  EXPECT_FALSE(ConversionModeUtil::ConvertStatusFromMozcToNative(
      status, false, &is_open, &native_mode));

  // The mode conversion should always be done regardless of open/close status.
  is_open = false;
  native_mode = 0;
  status.Clear();
  status.set_activated(false);
  status.set_mode(commands::HIRAGANA);
  EXPECT_TRUE(ConversionModeUtil::ConvertStatusFromMozcToNative(
      status, false, &is_open, &native_mode));
  EXPECT_EQ(IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN,
            native_mode);

  // The mode conversion should always be done regardless of open/close status.
  is_open = false;
  native_mode = 0;
  status.Clear();
  status.set_activated(true);
  status.set_mode(commands::HIRAGANA);
  EXPECT_TRUE(ConversionModeUtil::ConvertStatusFromMozcToNative(
      status, false, &is_open, &native_mode));
  EXPECT_EQ(IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN,
            native_mode);
}

TEST(ConversionModeUtilTest, ConvertStatusFromNativeToMozc) {
  mozc::commands::Status status;

  // The mode conversion should always be done regardless of open/close status,
  // that is, we no longer rely on |mozc::commands::DIRECT|.
  status.Clear();
  EXPECT_TRUE(ConversionModeUtil::ConvertStatusFromNativeToMozc(
      false, IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN,
      &status));
  EXPECT_TRUE(status.has_activated());
  EXPECT_FALSE(status.activated());
  EXPECT_TRUE(status.has_mode());
  EXPECT_EQ(commands::HIRAGANA, status.mode());

  // The mode conversion should always be done regardless of open/close status,
  // that is, we no longer rely on |mozc::commands::DIRECT|.
  status.Clear();
  EXPECT_TRUE(ConversionModeUtil::ConvertStatusFromNativeToMozc(
      true, IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN,
      &status));
  EXPECT_TRUE(status.has_activated());
  EXPECT_TRUE(status.activated());
  EXPECT_TRUE(status.has_mode());
  EXPECT_EQ(commands::HIRAGANA, status.mode());
}
}  // namespace win32
}  // namespace mozc
