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

// Functions to manipulate bits and bytes.

#ifndef MOZC_BASE_BITS_H_
#define MOZC_BASE_BITS_H_

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <memory>
#include <type_traits>

#include "absl/base/config.h"

#if __has_include(<bits>)
#include <bits>
#endif  // __has_include(<bits>)

#ifdef _MSC_VER
#include <stdlib.h>  // for _byteswap_
#endif               // _MSC_VER

#ifdef _MSC_VER
// MSVC's _byteswap_ functions are not constexpr.
#define MOZC_BITS_BYTESWAP_CONSTEXPR
#else  // _MSC_VER
#define MOZC_BITS_BYTESWAP_CONSTEXPR constexpr
#endif  // !_MSC_VER

namespace mozc {
namespace bits_internal {
template <typename T, typename Iterator>
inline constexpr void Advance(Iterator &iter);
#ifndef __cpp_lib_byteswap
inline MOZC_BITS_BYTESWAP_CONSTEXPR uint16_t ByteSwap16(uint16_t n);
inline MOZC_BITS_BYTESWAP_CONSTEXPR uint32_t ByteSwap32(uint32_t n);
inline MOZC_BITS_BYTESWAP_CONSTEXPR uint64_t ByteSwap64(uint64_t n);
#endif  // !__cpp_lib_byteswap
}  // namespace bits_internal

// Endian is a replicate of C++23 std::endian.
enum class Endian : int8_t {
  kLittle = 0,
  kBig = 1,
#ifdef ABSL_IS_LITTLE_ENDIAN
  kNative = kLittle,
#else   // ABSL_IS_LITTLE_ENDIAN
  kNative = kBig,
#endif  // !ABSL_IS_LITTLE_ENDIAN
};

// byteswap is a limited implementation of std::byteswap in C++23.
// Reverses the byte order of the given integer value.
// Unlike std::byteswap, it only supports 8-, 16-, 32-, and 64-bit integer
// types. It's not constexpr in MSVC, either.
// If this file is compiled for C++23, it'll be an alias of std::byteswap.
#ifndef __cpp_lib_byteswap
template <typename T>
  requires(std::integral<T>)
inline MOZC_BITS_BYTESWAP_CONSTEXPR T byteswap(T n) {
  static_assert(
      sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8,
      "Only 8-, 16-, 32-, and 64-bit integer types are supported.");
  if constexpr (sizeof(T) == sizeof(uint8_t)) {
    return n;
  } else if constexpr (sizeof(T) == sizeof(uint16_t)) {
    return bits_internal::ByteSwap16(n);
  } else if constexpr (sizeof(T) == sizeof(uint32_t)) {
    return bits_internal::ByteSwap32(n);
  } else if constexpr (sizeof(T) == sizeof(uint64_t)) {
    return bits_internal::ByteSwap64(n);
  }
}
#else   // !__cpp_lib_byteswap
using std::byteswap;
#endif  // __cpp_lib_byteswap

// Loads a value of type T from std::to_address(iter) with the native byte
// order. Use this function instead of reinterpret_cast to read a multi-byte
// type from a byte array. Returns the result.
//
// REQUIRES: Iterator points to a contiguous memory region. Specifically,
// std::deque<T> doesn't satisfy this constraint.
//
// Example:
//  std::string_view buf = /* ... */;
//
//  uint32_t a = LoadUnaligned<uint32_t>(buf.data());
//  uint32_t b = LoadUnaligned<uint32_t>(buf.data() + 4);
template <typename T, typename Iterator>
  requires(std::contiguous_iterator<Iterator> && std::integral<T>)
inline T LoadUnaligned(Iterator iter) {
  T result;
  memcpy(&result, std::to_address(iter), sizeof(T));
  return result;
}

// LoadUnalignedAdvance() advances the iterator in addition to LoadUnaligned()
// to point to the element immediately after the loaded value.
//
// Example:
//
//  absl::Span<const char> buf = /* ... */;
//  std::vector<uint32_t> values;
//  for (auto iter = buf.begin(); iter != buf.end();) {
//    values.push_back(LoadUnalignedAdvance<uint32_t>(iter));
//  }
template <typename T, typename Iterator>
inline T LoadUnalignedAdvance(Iterator &iter) {
  const T result = LoadUnaligned<T>(iter);
  bits_internal::Advance<T>(iter);
  return result;
}

// Stores a value of sizeof(T) to std::to_address(it) with the native byte
// order. Use this function instead of reinterpret_cast to store a multi-byte
// type to a byte array. Returns a iterator pointing the element immediately
// after the stored value.
//
// REQUIRES: Iterator points to a contiguous memory region. Specifically,
// std::deque<T> doesn't satisfy this constraint.
//
// Example:
//
//  std::vector<char> WriteToBuffer(absl::Span<const uint32_t> values) {
//    std::vector<char> result((values.size() + 1) * sizeof(uint32_t));
//
//    auto iter = StoreUnaligned<uint32_t>(
//                  static_cast<uint32_t>(values.size()), result.begin());
//    for (auto &value : values) {
//      iter = StoreUnaligned<uint32_t>(value);
//    }
//    return result;
//  }
template <typename T, typename Iterator>
  requires(std::contiguous_iterator<Iterator> && std::integral<T>)
inline Iterator StoreUnaligned(const T value, Iterator iter) {
  memcpy(std::to_address(iter), std::addressof(value), sizeof(T));
  bits_internal::Advance<T>(iter);
  return iter;
}

// HostToNet changes the host byte order value to the network byte order.
template <typename T>
  requires(std::integral<T>)
inline T HostToNet(const T n) {
  if constexpr (Endian::kNative == Endian::kLittle) {
    return byteswap(n);
  } else {
    return n;
  }
}

// NetToHost changes the network byte order value to the host byte order.
template <typename T>
  requires(std::integral<T>)
inline T NetToHost(const T n) {
  return HostToNet(n);
}

// HostToLittle changes the host byte order value to the little endian order.
template <typename T>
  requires(std::integral<T>)
inline T HostToLittle(const T n) {
  if constexpr (Endian::kNative == Endian::kLittle) {
    return n;
  } else {
    return byteswap(n);
  }
}

// LittleToHost changes the little endian byte order to the host byte order.
template <typename T>
  requires(std::integral<T>)
inline T LittleToHost(const T n) {
  return HostToLittle(n);
}

namespace bits_internal {

template <typename T, typename Iterator>
inline constexpr void Advance(Iterator &iter) {
  using IterValue = typename std::iterator_traits<Iterator>::value_type;
  constexpr size_t iterator_value_size = sizeof(IterValue);

  static_assert(std::is_integral_v<IterValue>,
                "Iterator value must be an itegral type.");
  static_assert(sizeof(T) % iterator_value_size == 0,
                "sizeof(T) must be a multiple of sizeof(iterator value).");
  static_assert(
      sizeof(T) >= iterator_value_size,
      "sizeof(T) must be greater than or equal to sizeof(iterator value).");

  return std::advance(iter, sizeof(T) / sizeof(IterValue));
}

#ifndef __cpp_lib_byteswap

inline MOZC_BITS_BYTESWAP_CONSTEXPR uint16_t ByteSwap16(const uint16_t n) {
#if ABSL_HAVE_BUILTIN(__builtin_bswap16) || defined(__GNUC__)
  return __builtin_bswap16(n);
#elif defined(_MSC_VER)
  return _byteswap_ushort(n);
#else  // clang, gcc, or msvc
#error unsuppoorted compiler
#endif  // not clang, gcc, or msvc
}

inline MOZC_BITS_BYTESWAP_CONSTEXPR uint32_t ByteSwap32(const uint32_t n) {
#if ABSL_HAVE_BUILTIN(__builtin_bswap32) || defined(__GNUC__)
  return __builtin_bswap32(n);
#elif defined(_MSC_VER)
  return _byteswap_ulong(n);
#else  // clang, gcc, or msvc
#error unsuppoorted compiler
#endif  // not clang, gcc, or msvc
}

inline MOZC_BITS_BYTESWAP_CONSTEXPR uint64_t ByteSwap64(const uint64_t n) {
#if ABSL_HAVE_BUILTIN(__builtin_bswap64) || defined(__GNUC__)
  return __builtin_bswap64(n);
#elif defined(_MSC_VER)
  return _byteswap_uint64(n);
#else  // clang, gcc, or msvc
#error unsuppoorted compiler
#endif  // not clang, gcc, or msvc
}

#endif  // !__cpp_lib_byteswap

#undef MOZC_BITS_BYTESWAP_CONSTEXPR

}  // namespace bits_internal
}  // namespace mozc

#endif  // MOZC_BASE_BITS_H_
