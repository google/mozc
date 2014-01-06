// Copyright 2010-2014, Google Inc.
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

// Windows specific conversion mode utility functions.

#ifndef MOZC_WIN32_BASE_CONVERSION_MODE_UTIL_H_
#define MOZC_WIN32_BASE_CONVERSION_MODE_UTIL_H_

#include <Windows.h>  // for DWORD

#include "base/port.h"
#include "session/commands.pb.h"

namespace mozc {
namespace win32 {
class ConversionModeUtil {
 public:
  // Converts a |mode| to a corresponding combination of TF_CONVERSIONMODE_*.
  // As a rule described blow, |*flag| may contain TF_CONVERSIONMODE_ROMAN
  // flag.  We should leave this flag because ATOK checks it and changes its
  // input mode to meet the specified mode.  See http://b/2189944
  // for details.  On the other hand, MS-IME 98 or later ignores
  // TF_CONVERSIONMODE_ROMAN. http://support.microsoft.com/kb/419357
  // We will set TF_CONVERSIONMODE_ROMAN if is allowed combination and
  // |kana_lock_enabled_in_hiragana_mode| is false.  Note that
  // InputModeConversionFlagToCompositionMode ignores TF_CONVERSIONMODE_ROMAN
  // flag.
  static bool ToNativeMode(
      mozc::commands::CompositionMode mode,
      bool kana_lock_enabled_in_hiragana_mode,
      uint32 *flag);

  // Converts combination of TF_CONVERSIONMODE_* to corresponding |mode|.
  // This function ignores TF_CONVERSIONMODE_ROMAN flag.
  // This his behavior is the same to that of MS-IME 98 or later.
  // http://support.microsoft.com/kb/419357
  static bool ToMozcMode(
      uint32 flag, mozc::commands::CompositionMode *mode);

  // A variant of ToNativeMode but takes mozc::commands::Status.
  // |logical_imm32_mode| is the conversion mode that should be reported to
  // the input method framework (IMM32/TSF).
  // |visible_imm32_mode| is the conversion mode that should be visible from
  // the user.
  static bool ConvertStatusFromMozcToNative(
      const mozc::commands::Status &status,
      bool kana_lock_enabled_in_hiragana_mode,
      bool *is_open,
      DWORD *logical_imm32_mode,
      DWORD *visible_imm32_mode);

  // A variant of ToMozcMode but returns mozc::commands::CompositionMode.
  static bool GetMozcModeFromNativeMode(
      DWORD imm32_mode,
      mozc::commands::CompositionMode *mozc_mode);
};
}  // namespace win32
}  // namespace mozc

#endif  // MOZC_WIN32_BASE_CONVERSION_MODE_UTIL_H_
