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
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <type_traits>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "base/strings/pfchar.h"

namespace mozc {

// basic_zstring_view<StringViewT> holds a string_view like class that points to
// a null-terminated string. It provides the c_str() function that returns the
// pointer of the string. std::basic_string_view doesn't guarantee null
// termination, but this wrapper adds the guarantee by limiting the constructor
// and mutable operations.
//
// Use operator->() and view() to access the underlying string_view. Notably,
// t's required to explicitly convert to absl::string_view when using
// absl::StrCat() and absl::StrAppend(). AbslStringify() is intentionally not
// implemented because it creates a copy of the string.
//
// Implicit conversions (both types must have the same value_type):
//  Allowed:
//   - std::basic_string -> basic_zstring_view
//   - const_pointer -> basic_zstring_view
//   - basic_zstring_view -> StringViewT
//  Not allowed:
//   - StringViewT -> basic_zstring_view
//
// Note: Although implicit constructor from std::basic_string is allowed,
// explicitly use the to_pfstring function when passing a std::string to
// functions that needs a wchar_t pointer on Windows.
template <typename StringViewT>
class basic_zstring_view {
 public:
  using traits_type = typename StringViewT::traits_type;
  using value_type = typename StringViewT::value_type;
  using reference = typename StringViewT::reference;
  using const_reference = typename StringViewT::const_reference;
  using pointer = typename StringViewT::pointer;
  using const_pointer = typename StringViewT::const_pointer;
  using iterator = typename StringViewT::iterator;
  using const_iterator = typename StringViewT::const_iterator;
  using size_type = typename StringViewT::size_type;
  using difference_type = typename StringViewT::difference_type;

  // Default constructor initializes basic_zstring_view with nullptr.
  constexpr basic_zstring_view() noexcept = default;

  // Implicit constructor from const CharT * (C-style null-terminated string).
  // Unlike absl::string_view (when ABSL_OPTION_USE_STD_STRING_VIEW is set to
  // 0), passing nullptr is an undefined behavior. Note that absl::string_view
  // is just an alias of std::string_view in most cases, so passing a nullptr is
  // probably a bad idea anyway.
  //
  // NOLINTNEXTLINE(runtime/explicit)
  constexpr basic_zstring_view(
      const absl::Nonnull<const_pointer> p ABSL_ATTRIBUTE_LIFETIME_BOUND)
      // This std::basic_string_view constructor overload is not noexcept.
      : sv_(p) {}

  // Constructs basic_zstring_view from a const CharT * and length of the
  // string, not including the null-terminated character.
  //
  // REQUIRES: p[n] is the null-terminated character.
  constexpr basic_zstring_view(const absl::Nonnull<const_pointer> p
                                   ABSL_ATTRIBUTE_LIFETIME_BOUND,
                               const size_type n)
      : sv_(p, n) {
    DCHECK_EQ(p[n], 0);
  }

  // Implicit constructor from std::basic_string. Use to_pfstring when passing a
  // std::string to functions that accept zstring_view. This implicit cast
  // is necessary to accept basic_string as zstring_view because we can't add
  // an operator to std::basic_string.
  //
  // NOLINTNEXTLINE(runtime/explicit)
  constexpr basic_zstring_view(const std::basic_string<value_type> &str
                                   ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      // Implicitly converts to std::basic_string::operator basic_string_view(),
      // which is noexcept. operator basic_string_view() is equivalent to
      // std::basic_string_view(data(), size()) so it's guaranteed to be
      // null-terminated.
      : sv_(str) {}

  // Disallow nullptr.
  constexpr basic_zstring_view(std::nullptr_t) = delete;

  // Allow copies.
  constexpr basic_zstring_view(const basic_zstring_view &) noexcept = default;
  constexpr basic_zstring_view &operator=(const basic_zstring_view &) noexcept =
      default;

  // Access the underlying StringViewT through the arrow operator.
  // basic_zstring_view doesn't now implement a non-const operator->() to not
  // allow operations that could break the null-termination guarantee.
  constexpr const StringViewT *operator->() const noexcept {
    return std::addressof(sv_);
  }

  // Returns the underlying string_view.
  constexpr const StringViewT &view() const noexcept { return sv_; }

  // Allow implicit conversion to StringViewT.
  // NOLINTNEXTLINE(runtime/explicit)
  constexpr operator StringViewT() const noexcept { return sv_; }

  // Allow implicit conversion to absl::AlphaNum to use with absl::StrCat().
  // This method only works for zstring_view (not zwstring_view) because
  // absl::AlphaNum doesn't support wide characters.
  // NOLINTNEXTLINE(runtime/explicit)
  operator absl::AlphaNum() const noexcept { return sv_; }

  // c_str() and data() return a pointer to the null-terminated StringViewT.
  // The returned pointer is guaranteed to be null-terminated because this class
  // only constructs from null-terminated strings.
  // Use these functions instead of the underlying StringViewT when calling
  // C functions.
  constexpr absl::Nonnull<const_pointer> c_str() const noexcept {
    return sv_.data();
  }
  constexpr absl::Nonnull<const_pointer> data() const noexcept {
    return sv_.data();
  }

  // Returns true if the string is empty.
  constexpr bool empty() const noexcept { return sv_.empty(); }

  // Returns the number of characters in value_type.
  constexpr size_type size() const noexcept { return sv_.size(); }
  constexpr size_type length() const noexcept { return sv_.length(); }

  // Returns StringViewT::max_size().
  constexpr size_type max_size() const noexcept { return sv_.max_size(); }

  // Iterators.
  constexpr const_iterator begin() const noexcept { return sv_.begin(); }
  constexpr const_iterator cbegin() const noexcept { return begin(); }
  constexpr const_iterator end() const noexcept { return sv_.end(); }
  constexpr const_iterator cend() const noexcept { return end(); }

  constexpr void swap(basic_zstring_view &other) noexcept {
    sv_.swap(other.sv_);
  }

  // Comparison operators for basic_zstring_view. Currently only ==, !=, <
  // are implemented because the other comparisons are not very common.
  // TODO(yuryu): use operator <=> when C++20 is allowed.
  friend constexpr bool operator==(const basic_zstring_view lhs,
                                   const basic_zstring_view rhs) noexcept {
    return lhs.sv_ == rhs.sv_;
  }
  friend constexpr bool operator!=(const basic_zstring_view lhs,
                                   const basic_zstring_view rhs) noexcept {
    return lhs.sv_ != rhs.sv_;
  }
  friend constexpr bool operator<(const basic_zstring_view lhs,
                                  const basic_zstring_view rhs) noexcept {
    return lhs.sv_ < rhs.sv_;
  }

  template <typename H>
  friend H AbslHashValue(H state, const basic_zstring_view s) {
    return H::combine(std::move(state), s.sv_);
  }

 private:
  StringViewT sv_;
};

// Comparison operators for basic_zstring_view and anything that converts
// to StringViewT.
// TODO(yuryu): use operator <=> when C++20 is allowed.
template <typename StringViewT, typename StringViewLike>
constexpr bool operator==(const basic_zstring_view<StringViewT> lhs,
                          const StringViewLike &rhs) {
  return lhs.view() == rhs;
}

template <typename StringViewT, typename StringViewLike>
constexpr bool operator==(const StringViewLike &lhs,
                          const basic_zstring_view<StringViewT> rhs) {
  return lhs == rhs.view();
}

template <typename StringViewT, typename StringViewLike>
constexpr bool operator!=(const basic_zstring_view<StringViewT> lhs,
                          const StringViewLike &rhs) {
  return lhs.view() != rhs;
}

template <typename StringViewT, typename StringViewLike>
constexpr bool operator!=(const StringViewLike &lhs,
                          const basic_zstring_view<StringViewT> rhs) {
  return lhs != rhs.view();
}

template <typename StringViewT, typename StringViewLike>
constexpr bool operator<(const basic_zstring_view<StringViewT> lhs,
                         const StringViewLike &rhs) {
  return lhs.view() < rhs;
}

template <typename StringViewT, typename StringViewLike>
constexpr bool operator<(const StringViewLike &lhs,
                         const basic_zstring_view<StringViewT> rhs) {
  return lhs < rhs.view();
}

// Outputs the value of the underlying StringViewT.
template <typename StringViewT>
std::basic_ostream<typename basic_zstring_view<StringViewT>::value_type> &
operator<<(
    std::basic_ostream<typename basic_zstring_view<StringViewT>::value_type>
        &os,
    const basic_zstring_view<StringViewT> str) {
  os << str.view();
  return os;
}

using zstring_view = basic_zstring_view<absl::string_view>;
using zwstring_view = basic_zstring_view<std::wstring_view>;
using zpfstring_view = basic_zstring_view<pfstring_view>;

static_assert(std::is_convertible_v<zstring_view, absl::string_view>);

}  // namespace mozc

#endif  // MOZC_BASE_STRINGS_ZSTRING_VIEW_H_
