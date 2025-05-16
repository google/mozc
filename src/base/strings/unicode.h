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

#ifndef MOZC_BASE_STRINGS_UNICODE_H_
#define MOZC_BASE_STRINGS_UNICODE_H_

#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/optimization.h"
#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "base/absl_nullability.h"
#include "base/strings/internal/utf8_internal.h"

namespace mozc {
namespace strings {

// The Unicode replacement character (U+FFFD) for ill-formed sequences.
using ::mozc::utf8_internal::kReplacementCharacter;

// Returns the byte length of a single UTF-8 character based on the leading
// byte.
//
// REQUIRES: The UTF-8 character is valid.
using ::mozc::utf8_internal::OneCharLen;

// Returns the byte length of a single UTF-8 character at the iterator. This
// overload participates in resolution only if InputIterator is not convertible
// to char.
//
// REQUIRES: The iterator is valid, points to the leading byte of the UTF-8
// character, and the value type is char.
template <typename InputIterator>
  requires(!std::convertible_to<InputIterator, char>)
constexpr uint8_t OneCharLen(const InputIterator it) {
  static_assert(
      std::same_as<typename std::iterator_traits<InputIterator>::value_type,
                   char>,
      "The iterator value_type must be char.");
  return OneCharLen(*it);
}

// Checks if the string is a valid UTF-8 string.
bool IsValidUtf8(absl::string_view sv);

// Returns the codepoint count of the given UTF-8 string indicated as [first,
// last) or a string_view.
//
// REQUIRES: The UTF-8 string must be valid. This implementation only sees the
// leading byte of each character and doesn't check if it's well-formed.
// Complexity: linear
template <typename InputIterator>
  requires std::input_iterator<InputIterator>
size_t CharsLen(InputIterator first, InputIterator last);
inline size_t CharsLen(const absl::string_view sv) {
  return CharsLen(sv.begin(), sv.end());
}

// Returns the number of Unicode characters between [0, n]. It stops counting at
// n. This is faster than CharsLen if you just want to check the length against
// certain thresholds.
//
// REQUIRES: The UTF-8 string must be valid. Same restrictions as CharsLen
// apply.
// Complexity: linear to min(n, CharsLen())
//
// Example:
//    const size_t len = AtLeastCharsLen(sv, 9);
//    if (len < 5) {
//      // len is shorter than 5
//    } else if (len < 9) {
//      // len is shorter than 9
//    }
template <typename InputIterator>
  requires std::input_iterator<InputIterator>
size_t AtLeastCharsLen(InputIterator first, InputIterator last, size_t n);
inline size_t AtLeastCharsLen(absl::string_view sv, size_t n) {
  return AtLeastCharsLen(sv.begin(), sv.end(), n);
}

// Returns <first char, rest> of the string.
// The result is clipped if the input string isn't long enough.
constexpr std::pair<absl::string_view, absl::string_view> FrontChar(
    absl::string_view s);

// Converts the UTF-8 string to UTF-32. ToUtf32 works correctly with
// ill-formed UTF-8 sequences. Unrecognized encodings are replaced with U+FFFD.
std::u32string Utf8ToUtf32(absl::string_view sv);

// Converts the UTF-32 string to UTF-8. If a code point is outside of the
// valid Unicode range [U+0000, U+10FFFF], it'll be replaced with U+FFFD.
std::string Utf32ToUtf8(std::u32string_view sv);

// Appends a single Unicode character represented by a char32_t code point to
// dest. It ignores a null character.
inline void StrAppendChar32(std::string* absl_nonnull dest, char32_t cp);

// Converts a single Unicode character by a char32_t code point to UTF-8.
inline std::string Char32ToUtf8(const char32_t cp) {
  const utf8_internal::EncodeResult ec = utf8_internal::Encode(cp);
  return std::string(ec.data(), ec.size());
}

// Returns a substring of the UTF-8 string sv [pos, pos + count), or [pos,
// sv.end()) if count is not provided, by the number of Unicode characters. The
// result is clipped if pos + count > [number of Unicode characters in sv].
//
// Note that this function is linear and slower than Utf8AsChars::Substring as
// it needs to traverse through each character. Use Utf8AsChars::Substring if
// you already have the character iterators.
//
// REQUIRES: pos <= [number of Unicode characters in sv].
// Complexity: linear to pos + count, or pos if count it not provided.
absl::string_view Utf8Substring(absl::string_view sv, size_t pos);
absl::string_view Utf8Substring(absl::string_view sv, size_t pos, size_t count);

}  // namespace strings

// Represents both a UTF-8 string and its decoded result.
class UnicodeChar {
 public:
  UnicodeChar(const char* utf8, bool ok, const uint_fast8_t bytes_seen,
              char32_t codepoint)
      : utf8_(utf8), dr_(GetDecodeResult(ok, bytes_seen, codepoint)) {}
  UnicodeChar(const char* utf8, const uint_fast8_t bytes_seen,
              char32_t codepoint)
      : UnicodeChar(utf8, /*ok=*/true, bytes_seen, codepoint) {}

  UnicodeChar(const UnicodeChar&) = default;
  UnicodeChar& operator=(const UnicodeChar&) = default;

  char32_t char32() const { return dr_.code_point(); }
  absl::string_view utf8() const {
    return absl::string_view(utf8_, dr_.bytes_seen());
  }

  // Returns if the current character has a valid UTF-8 encoding.
  //
  // Most of the time, it's not necessary to explicitly call this function as
  // invalid characters are replaced with U+FFFD when the iterator is
  // dereferenced. Use this if you need to distinguish decoding errors from
  // U+FFFD in the source string when processing untrusted UTF-8 inputs.
  bool ok() const { return dr_.ok(); }

 private:
  static utf8_internal::DecodeResult GetDecodeResult(
      bool ok, const uint_fast8_t bytes_seen, char32_t codepoint) {
    if (ok) {
      return utf8_internal::DecodeResult::Continue(codepoint, bytes_seen);
    }
    if (bytes_seen) {
      return utf8_internal::DecodeResult::Error(bytes_seen);
    }
    return utf8_internal::DecodeResult::Sentinel();
  }

  const char* utf8_;
  utf8_internal::DecodeResult dr_;
};

// Utf8CharIterator is an iterator adapter for a string-like iterator to iterate
// over each UTF-8 character.
// Note that the simple dereference returns the underlying StringIterator value.
// Use one of the member functions to access each character.
template <typename BaseIterator, typename ValueType>
  requires std::contiguous_iterator<BaseIterator>
class Utf8CharIterator {
 public:
  using difference_type =
      typename std::iterator_traits<BaseIterator>::difference_type;
  using value_type = ValueType;
  using pointer = const value_type*;
  // The reference type can be non-reference for input iterators.
  using reference = value_type;
  using iterator_category = std::input_iterator_tag;

  // Constructs Utf8CharIterator at it for range [first, last). You also need to
  // pass the valid range of the underlying array to prevent any buffer overruns
  // because both operator++ and operator-- can move the StringIterator multiple
  // times in one call.
  template <typename First, typename Last>
    requires std::contiguous_iterator<First> &&
                 std::sized_sentinel_for<Last, First>
  Utf8CharIterator(First first, Last last) : iter_(first), last_(last) {
    Decode();
  }

  Utf8CharIterator(const Utf8CharIterator&) = default;
  Utf8CharIterator& operator=(const Utf8CharIterator&) = default;

  // Returns the current character.
  reference operator*() const;

  // Moves the iterator to the next Unicode character.
  Utf8CharIterator& operator++() {
    DCHECK(!dr_.IsSentinel());
    iter_ += dr_.bytes_seen();
    Decode();
    return *this;
  }
  Utf8CharIterator operator++(int) {
    Utf8CharIterator tmp(*this);
    ++*this;
    return tmp;
  }

  // Returns the code point of the current character as char32_t.
  char32_t char32() const {
    DCHECK(!dr_.IsSentinel());
    return dr_.code_point();
  }

  // Returns the UTF-8 string of the current character as absl::string_view.
  absl::string_view view() const {
    DCHECK(!dr_.IsSentinel());
    return absl::string_view(std::to_address(iter_), dr_.bytes_seen());
  }

  // Returns the byte length in UTF-8 of the current character.
  uint_fast8_t size() const { return dr_.bytes_seen(); }

  // Returns if the current character has a valid UTF-8 encoding.
  //
  // Most of the time, it's not necessary to explicitly call this function as
  // invalid characters are replaced with U+FFFD when the iterator is
  // dereferenced. Use this if you need to distinguish decoding errors from
  // U+FFFD in the source string when processing untrusted UTF-8 inputs.
  bool ok() const { return dr_.ok(); }

  // Returns a const char pointer to the current iterator position.
  const char* to_address() const { return std::to_address(iter_); }

  // Returns a substring of the original string between this iterator and
  // another iterator last.
  //
  // REQUIRES: last points to the same string object.
  template <typename LastBaseIterator, typename LastValueType>
  absl::string_view SubstringTo(
      const Utf8CharIterator<LastBaseIterator, LastValueType> last) const {
    return absl::string_view(std::to_address(iter_), last.iter_ - iter_);
  }

  template <typename RBaseIterator, typename RValueType>
  constexpr bool operator==(
      const Utf8CharIterator<RBaseIterator, RValueType> rhs) const {
    return iter_ == rhs.iter_;
  }

 private:
  void Decode() {
    dr_ = utf8_internal::Decode(std::to_address(iter_), std::to_address(last_));
  }

  BaseIterator iter_;
  BaseIterator last_;
  utf8_internal::DecodeResult dr_;
};

// Utf8AsCharsBase is a wrapper to iterate over a UTF-8 string as a char32_t
// code point or an absl::string_view substring of each character. Use the
// aliases Utf8AsChars32 and Utf8AsChars.
//
// Note: Utf8AsCharsBase doesn't satisfy all items of the C++ Container
// requirement, but you can still use it with STL algorithms and ranged for
// loops. Specifically, it requires `size() == std::distance(begin(), end())`
// and that size() has constant time complexity. Since UTF-8 is a
// variable-length code, the constructor would need to precompute and store the
// size, however, this class will mostly be used to just iterate over each
// character once, so it'd be inefficient to iterate over the same string twice.
// Therefore, it doesn't have the size() member function.
template <typename ValueType>
class Utf8AsCharsBase {
 public:
  using StringViewT = absl::string_view;
  using CharT = StringViewT::value_type;
  using BaseIterator = typename StringViewT::const_iterator;

  using value_type = ValueType;
  using reference = value_type&;
  using const_reference = const value_type&;
  using iterator = Utf8CharIterator<BaseIterator, ValueType>;
  using const_iterator = iterator;
  using difference_type = StringViewT::difference_type;
  using size_type = StringViewT::size_type;

  // Constructs an empty Utf8AsCharBase.
  Utf8AsCharsBase() = default;

  // Constructs Utf8AsCharBase with a string pointed by string_view.
  //
  // Complexity: constant
  explicit Utf8AsCharsBase(const StringViewT sv) : sv_(sv) {}

  // Constructs Utf8AsCharBase of the first count characters in the array s.
  //
  // Complexity: constant
  Utf8AsCharsBase(const CharT* s ABSL_ATTRIBUTE_LIFETIME_BOUND,
                  const size_type count)
      : sv_(s, count) {}

  // Constructs Utf8AsCharBase for string from a range of [first, last), where
  // both are const_iterator values of Utf8AsCharBase. The AsChar32 parameter
  // can be different among first, last, and the constructed class.
  //
  // Complexity: constant
  template <typename FirstIter, typename FirstType, typename LastIter,
            typename LastType>
  Utf8AsCharsBase(const Utf8CharIterator<FirstIter, FirstType> first,
                  const Utf8CharIterator<LastIter, LastType> last)
      : sv_(first.to_address(), last.to_address() - first.to_address()) {}

  // Construction from a null pointer is disallowed.
  Utf8AsCharsBase(std::nullptr_t) = delete;

  // Copyable.
  Utf8AsCharsBase(const Utf8AsCharsBase&) = default;
  Utf8AsCharsBase& operator=(const Utf8AsCharsBase&) = default;

  // Iterators.
  const_iterator begin() const {
    return const_iterator(sv_.begin(), sv_.end());
  }
  const_iterator end() const { return const_iterator(sv_.end(), sv_.end()); }
  const_iterator cbegin() const { return begin(); }
  const_iterator cend() const { return end(); }

  // Returns true if the string is empty.
  // Complexity: constant
  constexpr bool empty() const { return sv_.empty(); }

  // Returns the largest possible size in bytes.
  // Complexity: constant
  constexpr size_type max_size() const { return sv_.max_size(); }

  // Returns the first character.
  //
  // REQUIRES: !empty().
  // Complexity: constant
  value_type front() const { return *begin(); }

  // Returns the last character.
  //
  // REQUIRES: !empty().
  // Complexity: constant
  value_type back() const;

  // Returns a substring of two Utf8CharIterator [first, last) as StringViewT,
  // where last = end() if omitted. Prefer this function over other substring
  // functions if you already have iterators because this function has constant
  // complexity.
  //
  // Complexity: constant
  template <typename FirstIter, typename FirstType>
  StringViewT Substring(
      const Utf8CharIterator<FirstIter, FirstType> first) const {
    return first.SubstringTo(end());
  }
  template <typename FirstIter, typename FirstType, typename LastIter,
            typename LastType>
  StringViewT Substring(const Utf8CharIterator<FirstIter, FirstType> first,
                        const Utf8CharIterator<LastIter, LastType> last) const {
    return first.SubstringTo(last);
  }

  // Returns StringViewT to the underlying string.
  //
  // Complexity: constant
  constexpr StringViewT view() const { return sv_; }

  constexpr void swap(Utf8AsCharsBase& other) noexcept { sv_.swap(other.sv_); }

  // Bitwise comparison operators that compare two Utf8AsCharBase using the
  // underlying string_view comparators.
  template <typename RValueType>
  constexpr bool operator==(const Utf8AsCharsBase<RValueType> rhs) const {
    return view() == rhs.view();
  }
  template <typename RValueType>
  constexpr std::strong_ordering operator<=>(
      const Utf8AsCharsBase<RValueType> rhs) const {
    // In some environments, absl::string_view isn't an alias of
    // std::string_view and doesn't implement operator <=>.
    return view().compare(rhs.view()) <=> 0;
  }

 private:
  const CharT* EndPtr() const { return std::to_address(sv_.end()); }

  StringViewT sv_;
};

// Utf8AsChars32 is a wrapper to iterator a UTF-8 string over each character as
// char32_t code points.
// Characters with invalid encodings are replaced with U+FFFD.
//
// Example:
//  bool Func(const absl::string_view sv) {
//    for (const char32_t c : Utf8AsChars32(sv)) {
//      ...
//    }
//    ...
//  }
//
// Example:
//  const absl::string_view sv = ...;
//  std::u32string s32 = ...;
//  std::vector<char32_t> v;
//  absl::c_copy(Utf8AsChars32(sv), std::back_inserter(s32));
//  absl::c_copy(Utf8AsChars32(sv), std::back_inserter(v));
using Utf8AsChars32 = Utf8AsCharsBase<char32_t>;

// Utf8AsChars is a wrapper to iterator a UTF-8 string over each character as
// substrings. Characters with invalid encodings are returned as they are.
//
// Example:
//  bool Func(const absl::string_view sv) {
//    for (const absl::string_view c : Utf8AsChars(sv)) {
//      ...
//    }
//    ...
//  }
//
// Example:
// const absl::string_view sv = ...;
// std::vector<absl::string_view> v;
// absl::c_copy(Utf8AsChars(sv), std::back_inserter(v));
using Utf8AsChars = Utf8AsCharsBase<absl::string_view>;

// Utf8AsUnicodeChar is similar to Utf8AsChars32 and Utf8AsChars above, except
// that it can provide both char32_t code points and its UTF-8 sub-strings.
//
// When both aren't needed, Utf8AsChars32 and Utf8AsChars are more efficient.
//
// Example:
//  bool Func(const absl::string_view sv) {
//    for (const UnicodeChar c : Utf8AsUnicodeChar(sv)) {
//      std::cout << c.utf8() >> " --> " << c.char32();
//    }
//    ...
//  }
using Utf8AsUnicodeChar = Utf8AsCharsBase<UnicodeChar>;

// Implementations.
namespace strings {

template <typename InputIterator>
  requires std::input_iterator<InputIterator>
size_t CharsLen(InputIterator first, const InputIterator last) {
  size_t result = 0;
  while (first != last) {
    ++result;
    std::advance(first, OneCharLen(first));
  }
  return result;
}

template <typename InputIterator>
  requires std::input_iterator<InputIterator>
size_t AtLeastCharsLen(InputIterator first, const InputIterator last,
                       const size_t n) {
  size_t i = 0;
  while (first != last && i < n) {
    ++i;
    std::advance(first, OneCharLen(first));
  }
  return i;
}

constexpr std::pair<absl::string_view, absl::string_view> FrontChar(
    absl::string_view s) {
  if (s.empty()) {
    return {};
  }
  const uint8_t len = OneCharLen(s.front());
  return {absl::ClippedSubstr(s, 0, len), absl::ClippedSubstr(s, len)};
}

inline void StrAppendChar32(std::string* absl_nonnull dest, const char32_t cp) {
  if (cp == 0) [[unlikely]] {
    // Do nothing if |cp| is `\0`. Keeping the the legacy behavior of
    // CodepointToUtf8Append as some code may rely on it.
    return;
  }
  const utf8_internal::EncodeResult ec = utf8_internal::Encode(cp);
  // basic_string::append() is faster than absl::StrAppend() here.
  dest->append(ec.data(), ec.size());
}

}  // namespace strings

template <typename BaseIterator, typename ValueType>
  requires std::contiguous_iterator<BaseIterator>
typename Utf8CharIterator<BaseIterator, ValueType>::reference
Utf8CharIterator<BaseIterator, ValueType>::operator*() const {
  DCHECK(!dr_.IsSentinel());
  if constexpr (std::same_as<ValueType, char32_t>) {
    return char32();
  } else if constexpr (std::same_as<ValueType, absl::string_view>) {
    return view();
  } else if constexpr (std::same_as<ValueType, UnicodeChar>) {
    return ValueType{std::to_address(iter_), dr_.ok(), dr_.bytes_seen(),
                     dr_.code_point()};
  }
}

template <typename ValueType>
typename Utf8AsCharsBase<ValueType>::value_type
Utf8AsCharsBase<ValueType>::back() const {
  if (sv_.back() <= 0x7f) {
    // ASCII
    if constexpr (std::same_as<ValueType, char32_t>) {
      return sv_.back();
    } else if constexpr (std::same_as<ValueType, absl::string_view>) {
      return sv_.substr(sv_.size() - 1);
    } else if constexpr (std::same_as<ValueType, UnicodeChar>) {
      return value_type{EndPtr() - 1, 1, sv_.back()};
    }
  }
  // Other patterns. UTF-8 characters are at most four bytes long.
  // Check three bytes first as it's the most common pattern.
  // We still need to check one byte as it handles invalid sequences.
  for (const int size : {3, 2, 4, 1}) {
    if (size <= sv_.size()) {
      auto it = sv_.end() - size;
      const utf8_internal::DecodeResult dr =
          utf8_internal::Decode(std::to_address(it), EndPtr());
      if (dr.bytes_seen() == size) {
        if constexpr (std::same_as<ValueType, char32_t>) {
          return dr.code_point();
        } else if constexpr (std::same_as<ValueType, absl::string_view>) {
          return value_type(std::to_address(it), dr.bytes_seen());
        } else if constexpr (std::same_as<ValueType, UnicodeChar>) {
          return value_type{std::to_address(it), dr.bytes_seen(),
                            dr.code_point()};
        }
      }
    }
  }
  ABSL_UNREACHABLE();
}

}  // namespace mozc

#endif  // MOZC_BASE_STRINGS_UNICODE_H_
