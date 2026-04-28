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

#ifndef MOZC_BASE_CONTAINER_TUPLE_H_
#define MOZC_BASE_CONTAINER_TUPLE_H_

#include <memory>
#include <tuple>
#include <type_traits>

namespace mozc {
// Constructs an object of type T by unpacking and flattening multiple
// tuple-like values (std::tuple, std::pair, std::array) or individual values.
//
// Example:
// struct Hero {
//   Hero(std::string n, int lv, int h, int m);
// };
//
// auto basic_info = std::make_tuple("Aris", 1);
// auto stats = std::make_pair(100, 50);
// std::array<int, 3> stats2 = {20, 30, 40};
//
// Hero h1 = make_from_tuples<Hero>(basic_info, stats);  // "Aris", 1, 100, 50
// Hero h2 = make_from_tuples<Hero>("Bob", 10, stats);   // "Bob", 10, 100, 50
// Hero h3 = make_from_tuples<Hero>("Charlie", 99, 999, 999);
// Hero h4 = make_from_tuples<Hero>("Taro", stats2);  // "Taro", 20, 30, 40
//
// std::unique_ptr<Hero> h5 =
//          make_unique_from_tuples<Hero>("Ken", 10, 10, 10);
// auto h6 = apply_from_tuples(Hero::Create, basic_info, 20, 30);

namespace internal {

template <typename T, typename = void>
struct is_tuple_like : std::false_type {};

template <typename T>
struct is_tuple_like<T, std::void_t<decltype(std::tuple_size<T>::value)>>
    : std::true_type {};

template <typename T>
decltype(auto) ensure_tuple(T&& t) {
  if constexpr (is_tuple_like<
                    std::remove_cv_t<std::remove_reference_t<T>>>::value) {
    return std::forward<T>(t);
  } else {
    return std::forward_as_tuple(std::forward<T>(t));
  }
}
}  // namespace internal

// This is a variadic extension of std::make_from_tuple. It concatenates all
// provided tuple-like objects and raw values into a single argument list for
// T's constructor.
template <typename T, typename... Args>
T make_from_tuples(Args&&... args) {
  auto flattened =
      std::tuple_cat(internal::ensure_tuple(std::forward<Args>(args))...);
  return std::apply(
      [](auto&&... unpacked_args) -> T {
        return T(std::forward<decltype(unpacked_args)>(unpacked_args)...);
      },
      std::move(flattened));
}

// Similar to make_from_tuples, but returns std::unique_ptr<T>
template <typename T, typename... Args>
std::unique_ptr<T> make_unique_from_tuples(Args&&... args) {
  auto flattened =
      std::tuple_cat(internal::ensure_tuple(std::forward<Args>(args))...);
  return std::apply(
      [](auto&&... unpacked_args) {
        return std::make_unique<T>(
            std::forward<decltype(unpacked_args)>(unpacked_args)...);
      },
      std::move(flattened));
}

// Calls a callable object by unpacking and flattening multiple tuples or
// individual values as its arguments.
template <typename F, typename... Args>
decltype(auto) apply_from_tuples(F&& func, Args&&... args) {
  auto flattened =
      std::tuple_cat(internal::ensure_tuple(std::forward<Args>(args))...);
  return std::apply(std::forward<F>(func), std::move(flattened));
}
}  // namespace mozc

#endif  // MOZC_BASE_CONTAINER_TUPLE_H_
