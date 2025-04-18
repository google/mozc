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

#include "win32/base/imm_reconvert_string.h"

#include <safeint.h>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "absl/algorithm/container.h"
#include "absl/log/check.h"
#include "base/util.h"

namespace mozc {
namespace win32 {
namespace {

using ::mozc::win32::reconvert_string_internal::Deleter;
using ::mozc::win32::reconvert_string_internal::kAlignment;
using ::msl::utilities::SafeAdd;
using ::msl::utilities::SafeCast;
using ::msl::utilities::SafeMultiply;

bool CheckAddressSpace(const void *ptr, size_t size) {
  const uintptr_t addr = std::bit_cast<uintptr_t>(ptr);
  uintptr_t addr_last = 0;
  // buffer exceeds process address space if overflows.
  return SafeAdd(addr, size, addr_last);
}

// TODO(yukawa): Make a mechanism to generate this code from UnicodeData.txt.
bool IsControlCode(char32_t c) {
  // Based on UnicodeData.txt (5.2.0).
  // [U+0000 (NUL), U+001F (INFORMATION SEPARATOR ONE)]
  // [U+007F (DELETE), U+009F (APPLICATION PROGRAM COMMAND)]
  return (0x0000 <= c && c <= 0x001F) || (0x007F <= c && c <= 0x009F);
}

// TODO(yukawa): Move this to util.cc.
char32_t SurrogatePairToCodepoint(wchar_t high, wchar_t low) {
  return (((high - 0xD800) & 0x3FF) << 10) + ((low - 0xDC00) & 0x3FF) + 0x10000;
}

template <typename T, typename U>
bool SafeAdds(const T init, std::initializer_list<U> args, T &result) {
  result = init;
  for (T arg : args) {
    if (!SafeAdd(result, arg, result)) {
      return false;
    }
  }
  return true;
}

UniqueReconvertString Allocate(size_t size) {
  auto *ptr =
      reinterpret_cast<ReconvertString *>(::operator new(size, kAlignment));
  new (ptr) ReconvertString;
  return UniqueReconvertString(ptr, &Deleter);
}

// C++20 constructor for std::wstring_view.
std::wstring_view WStringView(const wchar_t *begin, const wchar_t *end) {
  return std::wstring_view(begin, end - begin);
}

}  // namespace

UniqueReconvertString ReconvertString::Request(const HWND hwnd) {
  UniqueReconvertString result(nullptr, &Deleter);
  LPARAM l_result = SendMessageW(hwnd, WM_IME_REQUEST, IMR_RECONVERTSTRING, 0);
  if (l_result == 0) {
    // IMR_RECONVERTSTRING is not supported.
    return result;
  }
  size_t buffer_size;
  if (!SafeCast(l_result, buffer_size)) {
    return result;
  }
  result = Allocate(buffer_size);
  result->dwSize = buffer_size;
  result->dwVersion = 0;
  l_result = SendMessageW(hwnd, WM_IME_REQUEST, IMR_RECONVERTSTRING,
                          reinterpret_cast<LPARAM>(result.get()));
  if (l_result == 0) {
    result.release();
  }
  return result;
}

UniqueReconvertString ReconvertString::Compose(const Strings ss) {
  DWORD total_chars = 0;
  UniqueReconvertString result(nullptr, &Deleter);
  if (!SafeAdds<DWORD, size_t>(
          0,
          {ss.preceding_text.size(), ss.preceding_composition.size(),
           ss.target.size(), ss.following_composition.size(),
           ss.following_text.size()},
          total_chars)) {
    return result;
  }
  DWORD buf_size = 0;
  if (!SafeMultiply(total_chars, sizeof(wchar_t), buf_size)) {
    return result;
  }
  if (!SafeAdd(buf_size, sizeof(ReconvertString), buf_size)) {
    return result;
  }
  if (buf_size > std::numeric_limits<DWORD>::max() ||
      !CheckAddressSpace(result.get(), buf_size)) {
    return result;
  }
  result = Allocate(buf_size);
  result->dwSize = buf_size;

  // |dwVersion| is fixed to 0.
  // http://msdn.microsoft.com/en-us/library/dd319107.aspx
  result->dwVersion = 0;

  result->dwStrOffset = sizeof(ReconvertString);
  result->dwStrLen = total_chars;
  result->dwTargetStrLen = ss.target.size();
  result->dwCompStrLen = ss.preceding_composition.size() + ss.target.size() +
                         ss.following_composition.size();
  result->dwCompStrOffset = ss.preceding_text.size() * sizeof(wchar_t);
  result->dwTargetStrOffset = result->dwCompStrOffset +
                              ss.preceding_composition.size() * sizeof(wchar_t);

  // concatenate |preceding_text|, |preceding_composition|, |target|,
  // |following_composition|, and |following_text| into |string_buffer|.

  wchar_t *buf_ptr = result->StringBuffer();
  buf_ptr = absl::c_copy(ss.preceding_text, buf_ptr);
  buf_ptr = absl::c_copy(ss.preceding_composition, buf_ptr);
  buf_ptr = absl::c_copy(ss.target, buf_ptr);
  buf_ptr = absl::c_copy(ss.following_composition, buf_ptr);
  absl::c_copy(ss.following_text, buf_ptr);

  return result;
}

bool ReconvertString::Validate() const {
  if (dwSize < sizeof(RECONVERTSTRING)) {
    // |dwSize| must be equal to or greater than sizeof(RECONVERTSTRING).
    return false;
  }
  if (dwVersion != 0) {
    // |dwVersion| must be 0.
    return false;
  }
  if (!CheckAddressSpace(this, dwSize)) {
    return false;
  }
  if (dwStrOffset > dwSize) {
    // |dwStrOffset| must be inside of the buffer.
    return false;
  }
  const size_t buffer_size_bytes = dwSize - dwStrOffset;
  size_t string_size_bytes;
  if (!SafeMultiply<size_t>(dwStrLen, sizeof(wchar_t), string_size_bytes)) {
    return false;
  }
  if (string_size_bytes > buffer_size_bytes) {
    // |dwStrLen| must be inside of the string buffer.
    return false;
  }
  if (!ValidateOffsetSize(buffer_size_bytes, dwCompStrOffset, dwCompStrLen) ||
      !ValidateOffsetSize(buffer_size_bytes, dwTargetStrOffset,
                          dwTargetStrLen)) {
    return false;
  }
  const DWORD composition_offset_chars = dwCompStrOffset / sizeof(wchar_t);
  const DWORD target_offset_chars = dwTargetStrOffset / sizeof(wchar_t);

  return dwCompStrOffset <= dwTargetStrOffset &&
         target_offset_chars + dwTargetStrLen <=
             composition_offset_chars + dwCompStrLen &&
         composition_offset_chars + dwCompStrLen <= dwStrLen;
}

std::optional<ReconvertString::Strings> ReconvertString::Decompose() const {
  if (!Validate()) {
    return std::nullopt;
  }

  const wchar_t *string_begin = StringBuffer();
  const wchar_t *composition_begin =
      string_begin + dwCompStrOffset / sizeof(wchar_t);
  const wchar_t *target_begin =
      string_begin + dwTargetStrOffset / sizeof(wchar_t);
  const wchar_t *target_end = target_begin + dwTargetStrLen;
  const wchar_t *composition_end = composition_begin + dwCompStrLen;
  const wchar_t *string_end = string_begin + dwStrLen;

  Strings ss;
  ss.preceding_text = WStringView(string_begin, composition_begin);
  ss.preceding_composition = WStringView(composition_begin, target_begin);
  ss.target = WStringView(target_begin, target_end);
  ss.following_composition = WStringView(target_end, composition_end);
  ss.following_text = WStringView(composition_end, string_end);
  return ss;
}

bool ReconvertString::EnsureCompositionIsNotEmpty() {
  Strings ss;
  if (std::optional<Strings> optional_ss = Decompose();
      optional_ss.has_value()) {
    ss = *std::move(optional_ss);
  } else {
    return false;
  }

  if (dwCompStrLen > 0) {
    // If the composition range is not empty, given |reconvert_string| is
    // acceptable.
    return true;
  }

  DCHECK_EQ(0, dwCompStrLen);
  DCHECK_EQ(0, dwTargetStrLen);
  DCHECK(ss.preceding_composition.empty());
  DCHECK(ss.target.empty());
  DCHECK(ss.following_composition.empty());

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
  // To avoid unexpected situation, assume characters categorized into
  // UNKNOWN_SCRIPT never compose a segment.

  Util::ScriptType script_type = Util::SCRIPT_TYPE_SIZE;
  size_t involved_following_len = 0;
  size_t involved_preceding_len = 0;

  // Check if the cursor is splitting a surrogate pair.
  if (!ss.following_text.empty() && !ss.preceding_text.empty() &&
      IS_SURROGATE_PAIR(ss.preceding_text.back(), ss.following_text.front())) {
    ++involved_following_len;
    ++involved_preceding_len;
    const char32_t unichar = SurrogatePairToCodepoint(
        ss.preceding_text.back(), ss.following_text.front());
    script_type = Util::GetScriptType(unichar);
  }

  while (involved_following_len < ss.following_text.size()) {
    // Stop searching when the previous character is UNKNOWN_SCRIPT.
    if (script_type == Util::UNKNOWN_SCRIPT) {
      break;
    }
    char32_t unichar = ss.following_text[involved_following_len];
    size_t num_wchar = 1;
    // Check if this |unichar| is the high part of a surrogate-pair.
    if (IS_HIGH_SURROGATE(unichar) &&
        (involved_following_len + 1 < ss.following_text.size()) &&
        IS_LOW_SURROGATE(ss.following_text[involved_following_len + 1])) {
      const char32_t high_surrogate = unichar;
      const char32_t low_surrogate =
          ss.following_text[involved_following_len + 1];
      unichar = SurrogatePairToCodepoint(high_surrogate, low_surrogate);
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

  while (involved_preceding_len < ss.preceding_text.size()) {
    // Stop searching when the previous character is UNKNOWN_SCRIPT.
    if (script_type == Util::UNKNOWN_SCRIPT) {
      break;
    }
    const size_t index = ss.preceding_text.size() - involved_preceding_len - 1;
    char32_t unichar = ss.preceding_text[index];
    size_t num_wchar = 1;
    // Check if this |unichar| is the low part of a surrogate-pair.
    if (IS_LOW_SURROGATE(unichar) &&
        (involved_preceding_len + 1 < ss.preceding_text.size()) &&
        IS_HIGH_SURROGATE(ss.preceding_text[index - 1])) {
      const char32_t high_surrogate = ss.preceding_text[index - 1];
      const char32_t low_surrogate = unichar;
      unichar = SurrogatePairToCodepoint(high_surrogate, low_surrogate);
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
      ss.preceding_text.size() - involved_preceding_len;

  const DWORD new_composition_len =
      involved_preceding_len + involved_following_len;

  if (new_composition_len == 0) {
    return false;
  }

  dwCompStrOffset = new_preceding_len * sizeof(wchar_t);
  dwTargetStrOffset = new_preceding_len * sizeof(wchar_t);

  dwCompStrLen = new_composition_len;
  dwTargetStrLen = new_composition_len;

  return true;
}

bool ReconvertString::ValidateOffsetSize(const DWORD buf_size,
                                         const DWORD offset,
                                         const DWORD size) const {
  return offset <= buf_size && size <= dwStrLen &&
         offset % sizeof(wchar_t) == 0;
}

}  // namespace win32
}  // namespace mozc
