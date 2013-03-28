// Copyright 2010-2013, Google Inc.
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

#include "win32/base/imm_reconvert_string.h"

#include <safeint.h>

#include "base/logging.h"
#include "base/util.h"

namespace mozc {
namespace win32 {
namespace {

using msl::utilities::SafeAdd;
using msl::utilities::SafeCast;
using msl::utilities::SafeMultiply;
using msl::utilities::SafeSubtract;

template <typename T>
bool CheckAddressSpace(const T *ptr) {
#if defined(_M_X64)
  const DWORD64 addr = reinterpret_cast<DWORD64>(ptr);
  DWORD64 addr_last = 0;
#elif defined(_M_IX86)
  const DWORD addr = reinterpret_cast<DWORD>(ptr);
  DWORD addr_last = 0;
#endif
  if (!SafeAdd(addr, ptr->dwSize, addr_last)) {
    // buffer exceeds process address space.
    return false;
  }
  return true;
}

// TODO(yukawa): Make a mechanism to generate this code from UnicodeData.txt.
bool IsControlCode(wchar_t c) {
  // Based on UnicodeData.txt (5.2.0).
  // [U+0000 (NUL), U+001F (INFORMATION SEPARATOR ONE)]
  // [U+007F (DELETE), U+009F (APPLICATION PROGRAM COMMAND)]
  return (0x0000 <= c && c <= 0x001F) || (0x007F <= c && c <= 0x009F);
}

// TODO(yukawa): Move this to util.cc.
char32 SurrogatePairToUCS4(wchar_t high, wchar_t low) {
  return (((high - 0xD800) & 0x3FF) << 10) +
         ((low - 0xDC00) & 0x3FF) + 0x10000;
}
}  // anonymous namespace

bool ReconvertString::Compose(const wstring &preceding_text,
                              const wstring &preceding_composition,
                              const wstring &target,
                              const wstring &following_composition,
                              const wstring &following_text,
                              RECONVERTSTRING *reconvert_string) {
  if (reconvert_string == nullptr) {
    return false;
  }

  if (!CheckAddressSpace(reconvert_string)) {
    return false;
  }

  DWORD preceding_text_len = 0;
  if (!SafeCast(preceding_text.size(), preceding_text_len)) {
    return false;
  }
  DWORD preceding_composition_len = 0;
  if (!SafeCast(preceding_composition.size(), preceding_composition_len)) {
    return false;
  }
  DWORD target_len = 0;
  if (!SafeCast(target.size(), target_len)) {
    return false;
  }
  DWORD following_composition_len = 0;
  if (!SafeCast(following_composition.size(), following_composition_len)) {
    return false;
  }
  DWORD following_text_len = 0;
  if (!SafeCast(following_text.size(), following_text_len)) {
    return false;
  }

  DWORD total_chars = 0;
  if (!SafeAdd(total_chars, preceding_text_len, total_chars)) {
    return false;
  }
  if (!SafeAdd(total_chars, preceding_composition_len, total_chars)) {
    return false;
  }
  if (!SafeAdd(total_chars, target_len, total_chars)) {
    return false;
  }
  if (!SafeAdd(total_chars, following_composition_len, total_chars)) {
    return false;
  }
  if (!SafeAdd(total_chars, following_text_len, total_chars)) {
    return false;
  }

  DWORD total_buffer_size = 0;
  if (!SafeMultiply(total_chars, sizeof(wchar_t), total_buffer_size)) {
    return false;
  }

  DWORD minimum_dw_size = 0;
  if (!SafeAdd(total_buffer_size, sizeof(RECONVERTSTRING), minimum_dw_size)) {
    return false;
  }

  if (minimum_dw_size > reconvert_string->dwSize) {
    // |dwSize| is too small.
    return false;
  }

  // |dwVersion| is fixed to 0.
  // http://msdn.microsoft.com/en-us/library/dd319107.aspx
  reconvert_string->dwVersion = 0;

  reconvert_string->dwStrOffset = sizeof(RECONVERTSTRING);
  reconvert_string->dwStrLen = total_chars;
  reconvert_string->dwTargetStrLen = target_len;

  if (!SafeAdd(preceding_composition_len, target_len,
               reconvert_string->dwCompStrLen)) {
    return false;
  }
  if (!SafeAdd(reconvert_string->dwCompStrLen, following_composition_len,
               reconvert_string->dwCompStrLen)) {
    return false;
  }

  if (!SafeMultiply(preceding_text_len, sizeof(wchar_t),
                    reconvert_string->dwCompStrOffset)) {
    return false;
  }

  DWORD target_offset_chars = 0;
  if (!SafeAdd(preceding_text_len, preceding_composition_len,
               target_offset_chars)) {
    return false;
  }
  if (!SafeMultiply(target_offset_chars, sizeof(wchar_t),
                    reconvert_string->dwTargetStrOffset)) {
    return false;
  }

  wchar_t *string_buffer = reinterpret_cast<wchar_t *>(
      reinterpret_cast<BYTE *>(reconvert_string) +
      reconvert_string->dwStrOffset);

  // concatenate |preceding_text|, |preceding_composition|, |target|,
  // |following_composition|, and |following_text| into |string_buffer|.
  {
    size_t index = 0;
    for (size_t i = 0; i < preceding_text.size(); ++i) {
      string_buffer[index] = preceding_text[i];
      ++index;
    }
    for (size_t i = 0; i < preceding_composition.size(); ++i) {
      string_buffer[index] = preceding_composition[i];
      ++index;
    }
    for (size_t i = 0; i < target.size(); ++i) {
      string_buffer[index] = target[i];
      ++index;
    }
    for (size_t i = 0; i < following_composition.size(); ++i) {
      string_buffer[index] = following_composition[i];
      ++index;
    }
    for (size_t i = 0; i < following_text.size(); ++i) {
      string_buffer[index] = following_text[i];
      ++index;
    }
  }

  return true;
}

bool ReconvertString::Decompose(const RECONVERTSTRING *reconvert_string,
                                wstring *preceding_text,
                                wstring *preceding_composition,
                                wstring *target,
                                wstring *following_composition,
                                wstring *following_text) {
  if (reconvert_string == nullptr) {
    return false;
  }

  if (reconvert_string->dwSize < sizeof(RECONVERTSTRING)) {
    // |dwSize| must be equal to or greater than sizeof(RECONVERTSTRING).
    return false;
  }

  if (reconvert_string->dwVersion != 0) {
    // |dwVersion| must be 0.
    return false;
  }

  if (!CheckAddressSpace(reconvert_string)) {
    return false;
  }

  if (reconvert_string->dwStrOffset > reconvert_string->dwSize) {
    // |dwStrOffset| must be inside of the buffer.
    return false;
  }

  const wchar_t *string_buffer = reinterpret_cast<const wchar_t *>(
      reinterpret_cast<const BYTE *>(reconvert_string) +
      reconvert_string->dwStrOffset);

  DWORD buffer_size_in_byte = 0;
  {
    // This must be always S_OK because |dwStrOffset <= dwSize|.
    if (!SafeSubtract(reconvert_string->dwSize,
                      reconvert_string->dwStrOffset,
                      buffer_size_in_byte)) {
      return false;
    }
  }

  DWORD string_size_in_byte = 0;
  {
    if (!SafeMultiply(reconvert_string->dwStrLen,
                      sizeof(wchar_t),
                      string_size_in_byte)) {
      return false;
    }
  }

  if (string_size_in_byte > buffer_size_in_byte) {
    // |dwStrLen| must be inside of the string buffer.
    return false;
  }

  if (reconvert_string->dwCompStrOffset > buffer_size_in_byte) {
    // |dwStrOffset| must be inside of the string buffer.
    return false;
  }

  if (reconvert_string->dwTargetStrOffset > buffer_size_in_byte) {
    // |dwStrOffset| must be inside of the string buffer.
    return false;
  }

  if ((reconvert_string->dwCompStrOffset % sizeof(wchar_t)) == 1) {
    // |dwCompStrOffset| must be a multiple of sizeof(wchar_t).
    return false;
  }
  const DWORD composition_begin_in_chars =
      reconvert_string->dwCompStrOffset / sizeof(wchar_t);
  DWORD composition_end_in_chars = 0;
  {
    if (!SafeAdd(composition_begin_in_chars,
                 reconvert_string->dwCompStrLen,
                 composition_end_in_chars)) {
      return false;
    }
  }

  if ((reconvert_string->dwTargetStrOffset % sizeof(wchar_t)) == 1) {
    // |dwCompStrOffset| must be a multiple of sizeof(wchar_t).
    return false;
  }
  const DWORD target_begin_in_chars =
      reconvert_string->dwTargetStrOffset / sizeof(wchar_t);
  DWORD target_end_in_chars = 0;
  {
    if (!SafeAdd(target_begin_in_chars,
                 reconvert_string->dwTargetStrLen,
                 target_end_in_chars)) {
      return false;
    }
  }

  const bool incluion_check =
      (composition_begin_in_chars <= target_begin_in_chars) &&
      (target_end_in_chars <= composition_end_in_chars) &&
      (composition_end_in_chars <= reconvert_string->dwStrLen);
  if (!incluion_check) {
    return false;
  }

  if (preceding_text != nullptr) {
    preceding_text->assign(
        string_buffer,
        string_buffer + composition_begin_in_chars);
  }
  if (preceding_composition != nullptr) {
    preceding_composition->assign(
        string_buffer + composition_begin_in_chars,
        string_buffer + target_begin_in_chars);
  }
  if (target != nullptr) {
    target->assign(
        string_buffer + target_begin_in_chars,
        string_buffer + target_end_in_chars);
  }
  if (following_composition != nullptr) {
    following_composition->assign(
        string_buffer + target_end_in_chars,
        string_buffer + composition_end_in_chars);
  }
  if (following_text != nullptr) {
    following_text->assign(
        string_buffer + composition_end_in_chars,
        string_buffer + reconvert_string->dwStrLen);
  }

  return true;
}

bool ReconvertString::Validate(const RECONVERTSTRING *reconvert_string) {
  return Decompose(reconvert_string, nullptr, nullptr, nullptr, nullptr,
                   nullptr);
}

bool ReconvertString::EnsureCompositionIsNotEmpty(
    RECONVERTSTRING *reconvert_string) {
  wstring preceding_text;
  wstring preceding_composition;
  wstring target;
  wstring following_composition;
  wstring following_text;
  if (!ReconvertString::Decompose(
          reconvert_string, &preceding_text, &preceding_composition,
          &target, &following_composition, &following_text)) {
    return false;
  }

  if (reconvert_string->dwCompStrLen > 0) {
    // If the composition range is not empty, given |reconvert_string| is
    // acceptable.
    return true;
  }

  DCHECK_EQ(0, reconvert_string->dwCompStrLen);
  DCHECK_EQ(0, reconvert_string->dwTargetStrLen);
  DCHECK(preceding_composition.empty());
  DCHECK(target.empty());
  DCHECK(following_composition.empty());

  // Here, there is no text selection and |reconvert_string->dwTargetStrLen|
  // represents the cursor position. In this case, the given surrounding text
  // is divided into |following_text| and |preceding_text| at the cursor
  // position. For example, if the text is "SN1[Cursor]987A", |preceding_text|
  // and |following_text| contain "SN1" and "987A", respectively.
  // In this case, existing Japanese IMEs seem to make a composition range
  // which consists of a minimum segment. Since text segmentation command has
  // not been supported by the Mozc server, here Util::ScriptType is used to
  // implement naive segmentation. This works as follows.
  // 1) Like other Japanese IMEs, the character just after the cursor is
  //    checked first. For example, if the text is "SN1[Cursor]987A",
  //    '9' is picked up. If there is no character just after the cursor,
  //    the character just before the cursor is picked up.
  // 2) Check the script type of the character picked up. If the character is
  //    '9', |script_type| is NUMBER.
  // 3) Make a text range greedily by using the |script_type| from the cursor
  //    position. If the text is "SN1[Cursor]987A", "1987" is picked up by
  //    using the script type NUMBER.
  // To avoid unexpected situation, assume characters categolized into
  // UNKNOWN_SCRIPT never compose a segment.

  Util::ScriptType script_type = Util::SCRIPT_TYPE_SIZE;
  size_t involved_following_len = 0;
  size_t involved_preceding_len = 0;

  // Check if the cursor is splitting a surrogate pair.
  if ((following_text.size() >= 1) && (preceding_text.size()) >=1 &&
      IS_SURROGATE_PAIR(*preceding_text.rbegin(), *following_text.begin())) {
    ++involved_following_len;
    ++involved_preceding_len;
    const char32 unichar =
        SurrogatePairToUCS4(*preceding_text.rbegin(), *following_text.begin());
    script_type = Util::GetScriptType(unichar);
  }

  while (involved_following_len < following_text.size()) {
    // Stop searching when the previous character is UNKNOWN_SCRIPT.
    if (script_type == Util::UNKNOWN_SCRIPT) {
      break;
    }
    char32 unichar = following_text[involved_following_len];
    size_t num_wchar = 1;
    // Check if this |unichar| is the high part of a surrogate-pair.
    if (IS_HIGH_SURROGATE(unichar) &&
        (involved_following_len + 1 < following_text.size()) &&
        IS_LOW_SURROGATE(following_text[involved_following_len+1])) {
      const char32 high_surrogate = unichar;
      const char32 low_surrogate = following_text[involved_following_len+1];
      unichar = SurrogatePairToUCS4(high_surrogate, low_surrogate);
      num_wchar = 2;
    }
    // Stop searching when any control code is found.
    if (IsControlCode(unichar)) {
      break;
    }
    const Util::ScriptType type = Util::GetScriptType(unichar);
    if (script_type == Util::SCRIPT_TYPE_SIZE) {
      // This is the first character found so store its script type for later
      // use.
      script_type = type;
    } else if (script_type != type) {
      // Different script type of character found.
      break;
    }
    involved_following_len += num_wchar;
  }

  while (involved_preceding_len < preceding_text.size()) {
    // Stop searching when the previous character is UNKNOWN_SCRIPT.
    if (script_type == Util::UNKNOWN_SCRIPT) {
      break;
    }
    const size_t index = preceding_text.size() - involved_preceding_len - 1;
    char32 unichar = preceding_text[index];
    size_t num_wchar = 1;
    // Check if this |unichar| is the low part of a surrogate-pair.
    if (IS_LOW_SURROGATE(unichar) &&
        (involved_preceding_len + 1 < preceding_text.size()) &&
        IS_HIGH_SURROGATE(preceding_text[index-1])) {
      const char32 high_surrogate = preceding_text[index-1];
      const char32 low_surrogate = unichar;
      unichar = SurrogatePairToUCS4(high_surrogate, low_surrogate);
      num_wchar = 2;
    }
    // Stop searching when any control code is found.
    if (IsControlCode(unichar)) {
      break;
    }
    const Util::ScriptType type = Util::GetScriptType(unichar);
    if (script_type == Util::SCRIPT_TYPE_SIZE) {
      // This is the first character found so store its script type for later
      // use.
      script_type = type;
    } else if (script_type != type) {
      // Different script type of character found.
      break;
    }
    involved_preceding_len += num_wchar;
  }

  const size_t new_preceding_len =
      preceding_text.size() - involved_preceding_len;

  const DWORD new_composition_len =
      involved_preceding_len + involved_following_len;

  if (new_composition_len == 0) {
    return false;
  }

  reconvert_string->dwCompStrOffset = new_preceding_len * sizeof(wchar_t);
  reconvert_string->dwTargetStrOffset = new_preceding_len * sizeof(wchar_t);

  reconvert_string->dwCompStrLen = new_composition_len;
  reconvert_string->dwTargetStrLen = new_composition_len;

  return true;
}
}  // namespace win32
}  // namespace mozc
