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

#ifndef MOZC_BASE_STRINGS_ZSTRING_VIEW_H_
#define MOZC_BASE_STRINGS_ZSTRING_VIEW_H_

#include <cstddef>
#include <ostream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "base/strings/pfchar.h"
#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"

namespace mozc {

// basic_zstring_view<CharT> holds a string_view like class that points to
// a null-terminated string. It provides the c_str() function that returns the
// pointer of the string. std::basic_string_view doesn't guarantee null
// termination, but this wrapper adds the guarantee by limiting the constructor
// and mutable operations.
//
// Use operator*() and operator->() to access the underlying string_view.
//
// Implicit conversions:
//  Allowed:
//   - std::basic_string<CharT> -> basic_zstring_view<CharT>
//   - const CharT * -> basic_zstring_view<CharT>
//   - basic_zstring_view<CharT> -> StringViewT
//  Not allowed:
//   - StringViewT -> basic_zstring_view<CharT>
//
// Note: Although implicit constructor from std::basic_string<CharT> is allowed,
// explicitly use the to_pfstring function when passing a std::string to
// functions that needs a wchar_t pointer on Windows.
template <typename CharT, typename StringViewT>
class basic_zstring_view {
 public:
  using pointer = CharT *;
  using const_pointer = const CharT *;
  using size_type = std::size_t;
  using element_type = StringViewT;

  // Default constructor initializes basic_zstring_view with nullptr.
  constexpr basic_zstring_view() noexcept = default;

  // Implicit constructor from const CharT * (C-style null-terminated string).
  // Unlike absl::string_view (when ABSL_OPTION_USE_STD_STRING_VIEW is set to
  // 0), passing nullptr is an undefined behavior. Note that absl::string_view
  // is just an alias of std::string_view in most cases, so passing a nullptr is
  // probably a bad idea anyway.
  constexpr basic_zstring_view(  // NOLINT(runtime/explicit)
      const const_pointer p)
      // This std::basic_string_view constructor overload is not noexcept.
      : sv_(p) {}

  // Disallow nullptr.
  constexpr basic_zstring_view(std::nullptr_t) = delete;

  // Implicit constructor from std::basic_string. Use to_pfstring when passing a
  // std::string to functions that accept zstring_view. This implicit cast
  // is necessary to accept basic_string as zstring_view because we can't add
  // an operator to std::basic_string.
  constexpr basic_zstring_view(  // NOLINT(runtime/explicit)
      const std::basic_string<CharT> &str
          ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      // Implicitly converts to std::basic_string::operator basic_string_view(),
      // which is noexcept. operator basic_string_view() is equivalent to
      // std::basic_string_view(data(), size()) so it's guaranteed to be
      // null-terminated.
      : sv_(str) {}

  // Allow copies.
  constexpr basic_zstring_view(const basic_zstring_view &) noexcept = default;
  constexpr basic_zstring_view &operator=(const basic_zstring_view &) noexcept =
      default;

  // Access the underlying StringViewT through the pointer operators.
  // basic_zstring_view doesn't now implement a non-const operator->() to not
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

  constexpr void swap(basic_zstring_view &other) noexcept {
    std::swap(sv_, other.sv_);
  }

  template <typename H>
  friend H AbslHashValue(H state, const basic_zstring_view s) {
    return H::combine(std::move(state), s.sv_);
  }

  template <typename Sink>
  friend void AbslStringify(Sink &sink, const basic_zstring_view s) {
    // This AbslStringify() implementation doesn't support implicit encoding
    // conversions between char and wchar_t because doing so would allow
    // zwstring_view in absl::StrAppend() and absl::StrCat(), which would result
    // in two MultiByteToWideChar() API calls and a temporary object allocation
    // for each string to concatenate. Use to_string() explicitly to avoid the
    // overhead.
    static_assert(std::is_same_v<CharT, char>,
                  "Implicit encoding conversion is not allowed. Use "
                  "to_string() explicitly.");
    sink.Append(s.sv_);
  }

 private:
  StringViewT sv_;
};

// Comparison operators for basic_zstring_view. Currently only ==, !=, <
// are implemented because the other comparisons are not very common.
// TODO(yuryu): use operator <=> when C++20 is allowed.
template <typename CharT, typename StringViewT>
constexpr bool operator==(
    const basic_zstring_view<CharT, StringViewT> lhs,
    const basic_zstring_view<CharT, StringViewT> rhs) noexcept {
  return *lhs == *rhs;
}

template <typename CharT, typename StringViewT>
constexpr bool operator!=(
    const basic_zstring_view<CharT, StringViewT> lhs,
    const basic_zstring_view<CharT, StringViewT> rhs) noexcept {
  return *lhs != *rhs;
}

template <typename CharT, typename StringViewT>
constexpr bool operator<(
    const basic_zstring_view<CharT, StringViewT> lhs,
    const basic_zstring_view<CharT, StringViewT> rhs) noexcept {
  return *lhs < *rhs;
}

// Comparison operators for basic_zstring_view and anything that converts
// to StringViewT.
// TODO(yuryu): use operator <=> when C++20 is allowed.
template <typename CharT, typename StringViewT, typename StringViewLike>
constexpr bool operator==(const basic_zstring_view<CharT, StringViewT> lhs,
                          const StringViewLike &rhs) {
  return *lhs == rhs;
}

template <typename CharT, typename StringViewT, typename StringViewLike>
constexpr bool operator==(const StringViewLike &lhs,
                          const basic_zstring_view<CharT, StringViewT> rhs) {
  return lhs == *rhs;
}

template <typename CharT, typename StringViewT, typename StringViewLike>
constexpr bool operator!=(const basic_zstring_view<CharT, StringViewT> lhs,
                          const StringViewLike &rhs) {
  return *lhs != rhs;
}

template <typename CharT, typename StringViewT, typename StringViewLike>
constexpr bool operator!=(const StringViewLike &lhs,
                          const basic_zstring_view<CharT, StringViewT> rhs) {
  return lhs != *rhs;
}

template <typename CharT, typename StringViewT, typename StringViewLike>
constexpr bool operator<(const basic_zstring_view<CharT, StringViewT> lhs,
                         const StringViewLike &rhs) {
  return *lhs < rhs;
}

template <typename CharT, typename StringViewT, typename StringViewLike>
constexpr bool operator<(const StringViewLike &lhs,
                         const basic_zstring_view<CharT, StringViewT> rhs) {
  return lhs < *rhs;
}

// Outputs the value of the underlying StringViewT.
template <typename CharT, typename StringViewT>
std::basic_ostream<CharT> &operator<<(
    std::basic_ostream<CharT> &os,
    const basic_zstring_view<CharT, StringViewT> str) {
  os << *str;
  return os;
}

using zstring_view = basic_zstring_view<char, absl::string_view>;
using zwstring_view = basic_zstring_view<wchar_t, std::wstring_view>;
using zpfstring_view = basic_zstring_view<pfchar_t, pfstring_view>;

static_assert(std::is_convertible_v<zstring_view, absl::string_view>);

}  // namespace mozc

#endif  // MOZC_BASE_STRINGS_ZSTRING_VIEW_H_
