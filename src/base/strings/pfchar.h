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

#ifndef MOZC_BASE_STRINGS_PFCHAR_H_
#define MOZC_BASE_STRINGS_PFCHAR_H_

#include <concepts>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"

#ifdef _WIN32
#include <string_view>

#include "base/win32/wide_char.h"
#endif  // _WIN32

namespace mozc {

// pfchar_t is a typedef for the platform-native character type. It's wchar_t on
// Windows, and char everywhere else. pfstring and pfstring_view are also
// aliases for the corresponding string and string_view classes.
#ifdef _WIN32
using pfchar_t = wchar_t;
using pfstring = std::wstring;
using pfstring_view = std::wstring_view;
#else   // _WIN32
using pfchar_t = char;
using pfstring = std::string;
using pfstring_view = absl::string_view;
#endif  // !_WIN32

// to_pfstring converts a utf-8 string string to pfstring.
// On Windows, it converts the string to utf-16. On other platforms, it passes
// through std::string as a reference, or creates a new std::string object from
// absl::string_view.
inline pfstring to_pfstring(std::string&& str) {
#ifdef _WIN32
  return win32::Utf8ToWide(str);
#else   // _WIN32
  return std::move(str);
#endif  // !_WIN32
}

// Zero overhead overload for cases where pfstring == std::string.
template <typename T = pfstring>
  requires(std::same_as<T, std::string>)
inline const pfstring& to_pfstring(const std::string& str) {
  return static_cast<const T&>(str);
}

inline pfstring to_pfstring(const absl::string_view str) {
#ifdef _WIN32
  return win32::Utf8ToWide(str);
#else   // _WIN32
  return pfstring(str);
#endif  // !_WIN32
}

// ToString converts a pfchar_t strings to a utf-8 std::string.
// On Windows, it converts the string from utf-16. On other platforms, it passes
// through std::string as a reference, or creates a new std::string object from
// absl::string_view.
inline std::string to_string(pfstring&& str) {
#ifdef _WIN32
  return win32::WideToUtf8(str);
#else   // _WIN32
  return std::move(str);
#endif  // !_WIN32
}

// Zero overhead overload for cases where pfstring == std::string.
template <typename T = pfstring>
  requires(std::same_as<T, std::string>)
inline const std::string& to_string(const T& str) {
  return str;
}

inline std::string to_string(const pfstring_view str) {
#ifdef _WIN32
  return win32::WideToUtf8(str);
#else   // _WIN32
  return std::string(str);
#endif  // !_WIN32
}

// PF_STRING("literal") defines a pfstring literal.
// It's essentially the same as Microsoft's _T().
#define PF_STRING(s) PF_STRING_INTERNAL(s)
#ifdef _WIN32
#define PF_STRING_INTERNAL(s) L##s
#else  // _WIN32
#define PF_STRING_INTERNAL(s) s
#endif  // !_WIN32

}  // namespace mozc

#endif  // MOZC_BASE_STRINGS_PFCHAR_H_
