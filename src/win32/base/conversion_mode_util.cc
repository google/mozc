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

#include "win32/base/conversion_mode_util.h"

#if defined(_WIN32)
#include <imm.h>
#include <msctf.h>
#include <windows.h>
#endif  // _WIN32

#include <cstdint>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "base/vlog.h"

namespace {
constexpr uint32_t kAlphaNumeric = 0x0;
constexpr uint32_t kNative = 0x1;
constexpr uint32_t kKatakana = 0x2;
constexpr uint32_t kLanguage = 0x3;
constexpr uint32_t kFullShape = 0x8;
constexpr uint32_t kRoman = 0x10;
constexpr uint32_t kCharCode = 0x20;
constexpr uint32_t kHanjiConvert = 0x40;
constexpr uint32_t kSoftKeyboard = 0x80;
constexpr uint32_t kNoConversion = 0x100;
constexpr uint32_t kEUDC = 0x200;
constexpr uint32_t kSymbol = 0x400;
constexpr uint32_t kFixed = 0x800;

#if defined(_WIN32)
// Check the equality of constans if header files are available.

// kAlphaNumeric
static_assert(kAlphaNumeric == IME_CMODE_ALPHANUMERIC, "Renaming Check");
static_assert(kAlphaNumeric == TF_CONVERSIONMODE_ALPHANUMERIC,
              "Renaming Check");

// kNative
static_assert(kNative == IME_CMODE_NATIVE, "Renaming Check");
static_assert(kNative == TF_CONVERSIONMODE_NATIVE, "Renaming Check");

// kKatakana
static_assert(kKatakana == IME_CMODE_KATAKANA, "Renaming Check");
static_assert(kKatakana == TF_CONVERSIONMODE_KATAKANA, "Renaming Check");

// kLanguage
static_assert(kLanguage == IME_CMODE_LANGUAGE, "Renaming Check");

// kFullShape
static_assert(kFullShape == IME_CMODE_FULLSHAPE, "Renaming Check");
static_assert(kFullShape == TF_CONVERSIONMODE_FULLSHAPE, "Renaming Check");

// kRoman
static_assert(kRoman == IME_CMODE_ROMAN, "Renaming Check");
static_assert(kRoman == TF_CONVERSIONMODE_ROMAN, "Renaming Check");

// kCharCode
static_assert(kCharCode == IME_CMODE_CHARCODE, "Renaming Check");
static_assert(kCharCode == TF_CONVERSIONMODE_CHARCODE, "Renaming Check");
// kHanjiConvert
static_assert(kHanjiConvert == IME_CMODE_HANJACONVERT, "Renaming Check");

// kSoftKeyboard
static_assert(kSoftKeyboard == IME_CMODE_SOFTKBD, "Renaming Check");

// kNoConversion
static_assert(kNoConversion == IME_CMODE_NOCONVERSION, "Renaming Check");
static_assert(kNoConversion == TF_CONVERSIONMODE_NOCONVERSION,
              "Renaming Check");

// kEUDC
static_assert(kEUDC == IME_CMODE_EUDC, "Renaming Check");

// kSymbol
static_assert(kSymbol == IME_CMODE_SYMBOL, "Renaming Check");
static_assert(kSymbol == TF_CONVERSIONMODE_SYMBOL, "Renaming Check");

// kFixed
static_assert(kFixed == IME_CMODE_FIXED, "Renaming Check");
static_assert(kFixed == TF_CONVERSIONMODE_FIXED, "Renaming Check");
#endif  // _WIN32

// Returns true if the specified bits are set in the |flag| with
// unsetting the bits in the |flag|.
bool TestAndClearBits(uint32_t *flag, uint32_t bits) {
  const bool result = ((*flag & bits) != 0);
  *flag &= ~bits;
  return result;
}
}  // namespace

namespace mozc {
namespace win32 {
bool ConversionModeUtil::ToNativeMode(mozc::commands::CompositionMode mode,
                                      bool kana_lock_enabled_in_hiragana_mode,
                                      uint32_t *flag) {
  // b/2189944.
  // Built-in MS-IME and ATOK (as of 22.0.1.0) seem to specify IME_CMODE_ROMAN
  // flag even if the input mode is Half-width Alphanumeric.
  //
  // [Hiragana]
  //   Conversion Mode = 0x00000019
  //   IME_CMODE_NATIVE (CHINESE / HANGUL (HANGEUL) / JAPANESE) (0x00000001)
  //   IME_CMODE_FULLSHAPE (0x00000008)
  //   IME_CMODE_ROMAN (0x00000010)
  //
  // [Full-width Katakana]
  //   Conversion Mode = 0x0000001b
  //   IME_CMODE_NATIVE (CHINESE / HANGUL (HANGEUL) / JAPANESE) (0x00000001)
  //   IME_CMODE_KATAKANA (0x00000002)
  //   IME_CMODE_FULLSHAPE (0x00000008)
  //   IME_CMODE_ROMAN (0x00000010)
  //
  // [Full-width Alphanumeric]
  //   Conversion Mode = 0x00000018
  //   IME_CMODE_FULLSHAPE (0x00000008)
  //   IME_CMODE_ROMAN (0x00000010)
  //
  // [Half-width Katakana]
  //   Conversion Mode = 0x00000013
  //   IME_CMODE_NATIVE (CHINESE / HANGUL (HANGEUL) / JAPANESE) (0x00000001)
  //   IME_CMODE_KATAKANA (0x00000002)
  //   IME_CMODE_ROMAN (0x00000010)
  //
  // [Half-width Alphanumeric]
  //   Conversion Mode = 0x00000010
  //   IME_CMODE_ROMAN (0x00000010)
  const DWORD roman_flag = kana_lock_enabled_in_hiragana_mode ? 0 : kRoman;
  switch (mode) {
    case mozc::commands::DIRECT:
      // We do set |roman_flag|.
      *flag = kAlphaNumeric | roman_flag;
      break;
    case mozc::commands::HIRAGANA:
      *flag = kNative | kFullShape | roman_flag;
      break;
    case mozc::commands::HALF_KATAKANA:
      *flag = kNative | kKatakana | roman_flag;
      break;
    case mozc::commands::HALF_ASCII:
      // We do set |roman_flag|.
      *flag = kAlphaNumeric | roman_flag;
      break;
    case mozc::commands::FULL_ASCII:
      // We do set |roman_flag|.
      *flag = kAlphaNumeric | kFullShape | roman_flag;
      break;
    case mozc::commands::FULL_KATAKANA:
      *flag = kNative | kKatakana | kFullShape | roman_flag;
      break;
    default:
      LOG(ERROR) << "Unknown composition mode: " << mode;
      return false;
  }
  return true;
}

bool ConversionModeUtil::ToMozcMode(uint32_t flag,
                                    mozc::commands::CompositionMode *mode) {
  if (mode == nullptr) {
    LOG(ERROR) << "|mode| is nullptr";
    return false;
  }

  if (TestAndClearBits(&flag, kCharCode)) {
    return false;
  }
  if (TestAndClearBits(&flag, kHanjiConvert)) {
    return false;
  }
  if (TestAndClearBits(&flag, kSoftKeyboard)) {
    return false;
  }
  if (TestAndClearBits(&flag, kNoConversion)) {
    return false;
  }
  if (TestAndClearBits(&flag, kSymbol)) {
    return false;
  }
  if (TestAndClearBits(&flag, kEUDC)) {
    return false;
  }
  if (TestAndClearBits(&flag, kFixed)) {
    return false;
  }

  bool succeeded = false;
  if (TestAndClearBits(&flag, kNative)) {
    // NATIVE mode
    if (TestAndClearBits(&flag, kKatakana)) {
      // KATAKANA mode
      if (TestAndClearBits(&flag, kFullShape)) {
        *mode = mozc::commands::FULL_KATAKANA;
        succeeded = true;
      } else {
        *mode = mozc::commands::HALF_KATAKANA;
        succeeded = true;
      }
    } else {
      // HIRAGANA mode
      if (TestAndClearBits(&flag, kFullShape)) {
        *mode = mozc::commands::HIRAGANA;
        succeeded = true;
      } else {
        LOG(ERROR) << "Half HIRAGANA is not supported";
      }
    }
  } else {
    // ALPHANUMERIC mode
    if (TestAndClearBits(&flag, kKatakana)) {
      // kKatakana should be used with kNative.
      *mode = mozc::commands::HALF_ASCII;
      succeeded = false;
    } else if (TestAndClearBits(&flag, kFullShape)) {
      *mode = mozc::commands::FULL_ASCII;
      succeeded = true;
    } else {
      *mode = mozc::commands::HALF_ASCII;
      succeeded = true;
    }
  }

  // b/2189944.
  // We will intentionally ignore kRoman.
  // This behaviour is the same to that of MS-IME 98 or later.
  // http://support.microsoft.com/kb/419357
  if (TestAndClearBits(&flag, kRoman)) {
    MOZC_VLOG(2) << "kRoman remains";
  }

  // Check remaining flags
  if (TestAndClearBits(&flag, kNative)) {
    MOZC_VLOG(1) << "kNative remains";
  }
  if (TestAndClearBits(&flag, kKatakana)) {
    MOZC_VLOG(1) << "kKatakana remains";
  }
  if (TestAndClearBits(&flag, kFullShape)) {
    MOZC_VLOG(1) << "kFullShape remains";
  }
  if (TestAndClearBits(&flag, kCharCode)) {
    MOZC_VLOG(1) << "kCharCode remains";
  }
  if (TestAndClearBits(&flag, kSoftKeyboard)) {
    MOZC_VLOG(1) << "kSoftKeyboard remains";
  }
  if (TestAndClearBits(&flag, kNoConversion)) {
    MOZC_VLOG(1) << "kNoConversion remains";
  }
  if (TestAndClearBits(&flag, kSymbol)) {
    MOZC_VLOG(1) << "kSymbol remains";
  }
  if (TestAndClearBits(&flag, kEUDC)) {
    MOZC_VLOG(1) << "kEUDC remains";
  }
  if (TestAndClearBits(&flag, kFixed)) {
    MOZC_VLOG(1) << "kFixed remains";
  }

  return succeeded;
}

bool ConversionModeUtil::ConvertStatusFromMozcToNative(
    const mozc::commands::Status &status,
    bool kana_lock_enabled_in_hiragana_mode, bool *is_open,
    DWORD *logical_imm32_mode, DWORD *visible_imm32_mode) {
  if (!status.has_activated() || !status.has_mode() ||
      !status.has_comeback_mode()) {
    return false;
  }

  // We no longer support DIRECT mode in mozc::commands::Status.
  if (status.mode() == commands::DIRECT) {
    return false;
  }

  uint32_t logical_native_mode = 0;
  if (!ConversionModeUtil::ToNativeMode(status.comeback_mode(),
                                        kana_lock_enabled_in_hiragana_mode,
                                        &logical_native_mode)) {
    return false;
  }

  uint32_t visible_native_mode = 0;
  if (!ConversionModeUtil::ToNativeMode(status.mode(),
                                        kana_lock_enabled_in_hiragana_mode,
                                        &visible_native_mode)) {
    return false;
  }

  if (is_open != nullptr) {
    *is_open = status.activated();
  }
  if (logical_imm32_mode != nullptr) {
    *logical_imm32_mode = static_cast<DWORD>(logical_native_mode);
  }
  if (visible_imm32_mode != nullptr) {
    *visible_imm32_mode = static_cast<DWORD>(visible_native_mode);
  }
  return true;
}

bool ConversionModeUtil::GetMozcModeFromNativeMode(
    DWORD imm32_mode, mozc::commands::CompositionMode *mozc_mode) {
  const uint32_t native_mode = static_cast<uint32_t>(imm32_mode);
  *mozc_mode = mozc::commands::HIRAGANA;
  if (!ConversionModeUtil::ToMozcMode(native_mode, mozc_mode)) {
    return false;
  }

  // We never returns DIRECT from ConversionModeUtil::ToMozcMode.
  DCHECK_NE(mozc::commands::DIRECT, *mozc_mode);

  return true;
}
}  // namespace win32
}  // namespace mozc
