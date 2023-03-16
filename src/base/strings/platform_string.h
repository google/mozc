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

#ifndef MOZC_BASE_STRINGS_PLATFORM_STRING_H_
#define MOZC_BASE_STRINGS_PLATFORM_STRING_H_

#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"

#ifdef _WIN32
#include <wchar.h>

#include "base/win32/wide_char.h"
#endif  // _WIN32

namespace mozc {

// BasicPlatformStringView<CharT> holds a string_view like class that points to
// a null-terminated string. It provides the c_str() function that returns the
// pointer of the string. std::basic_string_view doesn't guarantee null
// termination, but this wrapper adds the guarantee by limiting the constructor
// and mutable operations.
//
// Use operator*() and operator->() to access the underlying string_view.
//
// Implicit conversions:
//  Allowed:
//   - std::basic_string<CharT> -> BasicPlatformStringView<CharT>
//   - const CharT * -> BasicPlatformStringView<CharT>
//   - BasicPlatformStringView<CharT> -> StringViewT
//  Not allowed:
//   - StringViewT -> BasicPlatformStringView<CharT>
//
// Note: Although implicit constructor from std::basic_string<CharT> is allowed,
// explicitly use the ToPlatformString function when passing a std::string to
// functions that needs a wchar_t pointer on Windows.
template <typename CharT, typename StringViewT = std::basic_string_view<CharT>,
          std::enable_if_t<std::is_same_v<CharT, char> ||
                               std::is_same_v<CharT, wchar_t>,
                           bool> = true>
class BasicPlatformStringView {
 public:
  using pointer = CharT *;
  using const_pointer = const CharT *;
  using size_type = std::size_t;
  using element_type = StringViewT;

  // Default constructor initializes BasicPlatformStringView with nullptr.
  constexpr BasicPlatformStringView() noexcept = default;

  // Implicit constructor from const CharT * (C-style null-terminated string).
  constexpr BasicPlatformStringView(  // NOLINT(runtime/explicit)
      const const_pointer p)
      // This std::basic_string_view constructor overload is not noexcept.
      : sv_(p) {}

  // Disallow nullptr.
  constexpr BasicPlatformStringView(std::nullptr_t) = delete;

  // Implicit constructor from std::basic_string. Use ToPlatformString when
  // passing a std::string to functions that accept PlatformStringView. This
  // implicit cast is necessary to accept PlatformString as PlatformStringView
  // because we can't add an operator to std::basic_string.
  constexpr BasicPlatformStringView(  // NOLINT(runtime/explicit)
      const std::basic_string<CharT> &str
          ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      // Implicitly converts to std::basic_string::operator basic_string_view(),
      // which is noexcept. operator basic_string_view() is equivalent to
      // std::basic_string_view(data(), size()) so it's guaranteed to be
      // null-terminated.
      : sv_(str) {}

  // Allow copies.
  constexpr BasicPlatformStringView(const BasicPlatformStringView &) noexcept =
      default;
  constexpr BasicPlatformStringView &operator=(
      const BasicPlatformStringView &) noexcept = default;

  // Access the underlying StringViewT through the pointer operators.
  // PlatformStringView doesn't now implement a non-const operator->() to not
  // allow operations that could break the null-termination guarantee.
  constexpr element_type operator*() const noexcept { return sv_; }
  constexpr const element_type *operator->() const noexcept { return &sv_; }

  // Allow implicit conversion to StringViewT.
  constexpr operator StringViewT() const noexcept  // NOLINT(runtime/explicit)
  {
    return sv_;
  }

  // c_str() and data() return a pointer to the null-terminated StringViewT.
  // The returned pointer is guaranteed to be null-terminated because this class
  // only constructs from null-terminated strings.
  // Use these functions instead of the underlying StringViewT when calling
  // C functions.
  constexpr const_pointer c_str() const noexcept { return sv_.data(); }
  constexpr const_pointer data() const noexcept { return sv_.data(); }

  // Googletest matchers need empty().
  constexpr bool empty() const noexcept { return sv_.empty(); }

  // The std::basic_string's constructor overload from StringViewLike needs
  // size().
  constexpr size_type size() const noexcept { return sv_.size(); }
  constexpr size_type length() const noexcept { return sv_.length(); }

  constexpr void swap(BasicPlatformStringView &other) noexcept {
    std::swap(sv_, other.sv_);
  }

  template <typename H>
  friend H AbslHashValue(H state, const BasicPlatformStringView s) {
    return H::combine(std::move(state), s.sv_);
  }

 private:
  StringViewT sv_;
};

// Comparison operators for BasicPlatformStringView. Currently only ==, !=, <
// are implemented because the other comparisons are not very common.
// TODO(yuryu): use operator <=> when C++20 is allowed.
template <typename CharT, typename StringViewT>
constexpr bool operator==(
    const BasicPlatformStringView<CharT, StringViewT> lhs,
    const BasicPlatformStringView<CharT, StringViewT> rhs) noexcept {
  return *lhs == *rhs;
}

template <typename CharT, typename StringViewT>
constexpr bool operator!=(
    const BasicPlatformStringView<CharT, StringViewT> lhs,
    const BasicPlatformStringView<CharT, StringViewT> rhs) noexcept {
  return *lhs != *rhs;
}

template <typename CharT, typename StringViewT>
constexpr bool operator<(
    const BasicPlatformStringView<CharT, StringViewT> lhs,
    const BasicPlatformStringView<CharT, StringViewT> rhs) noexcept {
  return *lhs < *rhs;
}

// Comparison operators for BasicPlatformStringView and anything that converts
// to StringViewT.
// TODO(yuryu): use operator <=> when C++20 is allowed.
template <typename CharT, typename StringViewT, typename StringViewLike>
constexpr bool operator==(const BasicPlatformStringView<CharT, StringViewT> lhs,
                          const StringViewLike &rhs) {
  return *lhs == rhs;
}

template <typename CharT, typename StringViewT, typename StringViewLike>
constexpr bool operator==(
    const StringViewLike &lhs,
    const BasicPlatformStringView<CharT, StringViewT> rhs) {
  return lhs == *rhs;
}

template <typename CharT, typename StringViewT, typename StringViewLike>
constexpr bool operator!=(const BasicPlatformStringView<CharT, StringViewT> lhs,
                          const StringViewLike &rhs) {
  return *lhs != rhs;
}

template <typename CharT, typename StringViewT, typename StringViewLike>
constexpr bool operator!=(
    const StringViewLike &lhs,
    const BasicPlatformStringView<CharT, StringViewT> rhs) {
  return lhs != *rhs;
}

template <typename CharT, typename StringViewT, typename StringViewLike>
constexpr bool operator<(const BasicPlatformStringView<CharT, StringViewT> lhs,
                         const StringViewLike &rhs) {
  return *lhs < rhs;
}

template <typename CharT, typename StringViewT, typename StringViewLike>
constexpr bool operator<(
    const StringViewLike &lhs,
    const BasicPlatformStringView<CharT, StringViewT> rhs) {
  return lhs < *rhs;
}

// Outputs the value of the underlying StringViewT.
template <typename CharT, typename StringViewT>
std::basic_ostream<CharT> &operator<<(
    std::basic_ostream<CharT> &os,
    const BasicPlatformStringView<CharT, StringViewT> str) {
  os << *str;
  return os;
}

#ifdef _WIN32
using PlatformChar = wchar_t;
using PlatformStringView = BasicPlatformStringView<wchar_t>;
#else   // _WIN32
using PlatformChar = char;
using PlatformStringView = BasicPlatformStringView<char, absl::string_view>;
#endif  // !_WIN32

using PlatformString = std::basic_string<PlatformChar>;

// ToPlatformString converts a utf-8 string str to PlatformString.
// On Windows, it converts the string to utf-16. Otherwise, it's equivalent to
// std::string(str).
inline PlatformString ToPlatformString(const std::string &str) {
#ifdef _WIN32
  return win32::Utf8ToWide(str);
#else   // _WIN32
  return str;
#endif  // !_WIN32
}

inline PlatformString ToPlatformString(std::string &&str) {
#ifdef _WIN32
  return win32::Utf8ToWide(str);
#else   // _WIN32
  return std::move(str);
#endif  // !_WIN32
}

inline PlatformString ToPlatformString(const absl::string_view str) {
#ifdef _WIN32
  return win32::Utf8ToWide(str);
#else   // _WIN32
  return PlatformString(str);
#endif  // !_WIN32
}

// ToString converts a PlatformStringView str to a utf-8 std::string.
// On Windows, it converts the string to from utf-16. Otherwise, it's equivalent
// to std::string(str).
inline std::string ToString(const PlatformStringView str) {
#ifdef _WIN32
  return win32::WideToUtf8(str);
#else   // _WIN32
  return std::string(str);
#endif  // !_WIN32
}

inline std::string ToString(const PlatformString &str) {
#ifdef _WIN32
  return win32::WideToUtf8(str);
#else   // _WIN32
  return str;
#endif  // !_WIN32
}

inline std::string ToString(PlatformString &&str) {
#ifdef _WIN32
  return win32::WideToUtf8(str);
#else   // _WIN32
  return std::move(str);
#endif  // !_WIN32
}

// PLATFORM_STRING("literal") defines a PlatformString literal.
// It's essentially the same as Microsoft's _T().
#define PLATFORM_STRING(s) PLATFORM_STRING_INTERNAL(s)
#ifdef _WIN32
#define PLATFORM_STRING_INTERNAL(s) L##s
#else  // _WIN32
#define PLATFORM_STRING_INTERNAL(s) s
#endif  // !_WIN32

}  // namespace mozc

#endif  // MOZC_BASE_STRINGS_PLATFORM_STRING_H_
