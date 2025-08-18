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

#ifndef MOZC_BASE_WIN32_WIDE_CHAR_H_
#define MOZC_BASE_WIN32_WIDE_CHAR_H_

#include <concepts>
#include <cstddef>
#include <initializer_list>
#include <string>
#include <string_view>

#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"

namespace mozc::win32 {

// Returns how many wide characters are necessary in UTF-16 to represent the
// UTF-8 input string. Note that the result of this method becomes greater
// than that of Util::CharsLen if |src| contains surrogate pairs in UTF-16.
size_t WideCharsLen(absl::string_view input);

// Converts the UTF-8 input string to a UTF-16 wide string. This function uses
// the Win32 MultiByteToWideChar API internally. Invalid characters are replaced
// with U+FFFD.
std::wstring Utf8ToWide(absl::string_view input);

// Converts the UTF-16 wide string input to UTF-8. This function uses the Win32
// WideCharToMultiByte API internally. Invalid characters are replaced with
// U+FFFD.

std::string WideToUtf8(std::wstring_view input);

// The following functions are simpler, limited versions of Abseil string
// functions. They don't behave exactly like the counterparts in Abseil, hence
// the "W" suffix in the function names. Notably, these implementations only
// take string-like objects (anything that you can construct a std::wstring_view
// from). It covers most of the use cases in Mozc though.

namespace wide_char_internal {
// These helper functions explicitly take std::wstring_view to prevent object
// size bloat. Also it's explicitly marked noinline because otherwise it'll get
// inlined into StrAppendW.
template <typename... Ts>
  requires(... && std::same_as<Ts, std::wstring_view>)
ABSL_ATTRIBUTE_NOINLINE void StrAppendWInternal(std::wstring* dest,
                                                const Ts... args) {
  dest->reserve(dest->size() + (0 + ... + args.size()));
  (dest->append(args), ...);
}

void StrAppendWInternal(std::wstring* dest,
                        std::initializer_list<std::wstring_view> slist);
}  // namespace wide_char_internal

// StrAppendW is a simplified version of absl::StrAppend() for wchar_t.
// It's more efficient and readable than chaining += or std::wstring::append().
// This implementation is, however, still slower than absl::StrAppend() because
// each append() call checks the current size and capacity. Abseil's
// implementation uses memcpy after __append_default_init (only available in
// libc++).
// There is no portable way to efficiently append multiple strings in C++ yet:
// http://wg21.link/P1072R10
template <typename... Ts>
  requires(... && std::constructible_from<std::wstring_view, Ts&>)
void StrAppendW(std::wstring* dest, const Ts&... args) {
  if constexpr (sizeof...(Ts) == 1) {
    // No need to call reserve() for one string.
    dest->append(args...);
  } else if constexpr (sizeof...(Ts) <= 4) {
    // Use the template version for small numbers of strings.
    wide_char_internal::StrAppendWInternal(dest, std::wstring_view(args)...);
  } else {
    // Use the initializer_list overload if there are more than four strings.
    // This threshold is the same as absl::StrAppend().
    wide_char_internal::StrAppendWInternal(dest, {std::wstring_view(args)...});
  }
}

inline void StrAppendW(std::wstring* dest) {}

// Simplified absl::StrCat. Same restrictions apply as StrAppendW.
template <typename... Ts>
  requires(... && std::constructible_from<std::wstring_view, Ts&>)
[[nodiscard]] std::wstring StrCatW(const Ts&... args) {
  if constexpr (sizeof...(Ts) <= 1) {
    return std::wstring(args...);
  } else {
    std::wstring result;
    StrAppendW(&result, args...);
    return result;
  }
}

}  // namespace mozc::win32

#endif  // MOZC_BASE_WIN32_WIDE_CHAR_H_
