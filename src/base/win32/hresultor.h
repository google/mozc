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

#ifndef MOZC_BASE_WIN32_HRESULTOR_H_
#define MOZC_BASE_WIN32_HRESULTOR_H_

#include <objbase.h>

#include <initializer_list>
#include <optional>
#include <type_traits>
#include <utility>

#include "base/logging.h"
#include "absl/base/attributes.h"

namespace mozc::win32 {

template <class T>
class HResultOr;

// Helper function to construct HResultOr when T is convertible to HRESULT.
// e.g.
// HResultOr<int> result = HResultOk(42);
template <typename U, typename T>
constexpr HResultOr<T> HResultOk(U&& value);

// HResultOr is a simple absl::StatusOr<T>-like implementation for HRESULT.
// The templates used for constructor and assignment operators largely follow
// absl::StatusOr<T>.
// HRESULT is set to S_OK if HResultOr is initialized or assigned with a value.
// Notable limitations compared to absl::StatusOr:
//  - No constructor, assignment and comparison operator for HResultOr<U> where
//  U is assignable/constructible to T.
//  - No implicit constructors or assignment operators defined.
//
// Note: If T is convertible to HRESULT (e.g. DWORD), HResult<T>(value)
// considers value as an HRESULT error code. Use HResultOk() or
// HResult<T>(std::in_place, value) to construct HResult with a valid value.
template <class T>
class HResultOr {
 public:
  using value_type = T;
  using error_type = HRESULT;

  // Default constructor. HResultOr is initialized with S_OK and value is set to
  // T().
  constexpr HResultOr() : HResultOr(T()) {}

  // Constructible from HRESULT. If hr is successful, value is set to T().
  constexpr explicit HResultOr(const HRESULT hr) : hr_(hr) {
    if (ok()) {
      value_ = T();
    }
  }

  // Constructible from T. This constructor is disabled if T is convertible to
  // U. Use the HResultOk function or the in_place overload to construct HResult
  // with a valid value.
  template <typename U = T,
            typename = std::enable_if_t<std::conjunction_v<
                std::negation<std::is_same<std::decay_t<U>, std::in_place_t>>,
                std::negation<std::is_same<std::decay_t<U>, HResultOr>>,
                std::negation<std::is_convertible<U, HRESULT>>,
                std::is_constructible<T, U>>>>
  constexpr explicit HResultOr(U&& value)
      : hr_(S_OK), value_(std::forward<U>(value)) {}

  // Copyable if T is copyable.
  constexpr HResultOr(const HResultOr&) = default;
  constexpr HResultOr& operator=(const HResultOr&) = default;

  // Movable if T is movable. noexcept if T has a noexcept move constructor.
  constexpr HResultOr(HResultOr&&) = default;
  constexpr HResultOr& operator=(HResultOr&&) = default;

  // Assignable from T. HRESULT is set to S_OK if !ok().
  template <typename U = T,
            typename = std::enable_if_t<std::conjunction_v<
                std::negation<std::is_same<std::decay_t<U>, HResultOr>>,
                std::is_constructible<T, U>, std::is_assignable<T&, U>>>>
  constexpr HResultOr& operator=(U&& value) {
    Assign(std::forward<U>(value));
    return *this;
  }

  // In-place construction of T.
  template <typename... Args>
  constexpr explicit HResultOr(std::in_place_t, Args&&... args)
      : hr_(S_OK), value_(std::in_place, std::forward<Args>(args)...) {}
  template <typename U, typename... Args>
  constexpr explicit HResultOr(std::in_place_t, std::initializer_list<U> ilist,
                               Args&&... args)
      : hr_(S_OK), value_(std::in_place, ilist, std::forward<Args>(args)...) {}

  // ok() checks SUCCEEDED(hr).
  constexpr ABSL_MUST_USE_RESULT bool ok() const { return SUCCEEDED(hr_); }
  // Returns HRESULT.
  constexpr HRESULT hr() const { return hr_; }

  // value() functions test ok() with `CHECK()`. operator*() functions don't.
  constexpr const T& value() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    CHECK(ok());
    return value_.value();
  }
  constexpr T& value() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    CHECK(ok());
    return value_.value();
  }
  constexpr const T&& value() const&& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    CHECK(ok());
    return std::move(value_).value();
  }
  constexpr T& value() && ABSL_ATTRIBUTE_LIFETIME_BOUND {
    CHECK(ok());
    return value_.value();
  }
  constexpr const T& operator*() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return *value_;
  }
  constexpr T& operator*() & ABSL_ATTRIBUTE_LIFETIME_BOUND { return *value_; }
  constexpr const T&& operator*() const&& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return *std::move(value_);
  }
  constexpr T&& operator*() && ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return *std::move(value_);
  }
  constexpr const T* operator->() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return value_.operator->();
  }
  constexpr T* operator->() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return value_.operator->();
  }

  template <typename U>
  friend void swap(HResultOr& a,
                   HResultOr<U>& b) noexcept(noexcept(std::swap(a.value_,
                                                                b.value_))) {
    std::swap(a.hr_, b.hr_);
    std::swap(a.value_, b.value_);
  }

  template <typename U, typename V>
  friend constexpr HResultOr<V> HResultOk(U&& value);

 private:
  // Construct with explicit HRESULT and value. This overload is for
  // HResultOk().
  template <typename U>
  constexpr HResultOr(const HRESULT hr, U&& value)
      : hr_(hr), value_(std::forward<U>(value)) {}

  template <typename U>
  constexpr void Assign(U&& value) {
    if (!ok()) {
      hr_ = S_OK;
    }
    value_ = std::forward<U>(value);
  }

  HRESULT hr_;
  std::optional<T> value_;
};

template <typename T, typename U>
constexpr bool operator==(const HResultOr<T>& a, const HResultOr<U>& b) {
  if (a.ok() && b.ok()) {
    return *a == *b;
  }
  return a.hr() == b.hr();
}

template <typename T, typename U>
constexpr bool operator!=(const HResultOr<T>& a, const HResultOr<U>& b) {
  return !(a == b);
}

template <typename T, typename U>
constexpr bool operator==(const HResultOr<T>& a, const U& b) {
  if (!a.ok()) {
    return false;
  }
  return *a == b;
}

template <typename T, typename U>
constexpr bool operator==(const U& a, const HResultOr<T>& b) {
  return b == a;
}

template <typename T, typename U>
constexpr bool operator!=(const HResultOr<T>& a, const U& b) {
  return !(a == b);
}

template <typename T, typename U>
constexpr bool operator!=(const U& a, const HResultOr<T>& b) {
  return !(b == a);
}

template <typename U, typename T = std::decay_t<U>>
constexpr HResultOr<T> HResultOk(U&& value) {
  return HResultOr<T>(S_OK, std::forward<U>(value));
}

}  // namespace mozc::win32

#endif  // MOZC_BASE_WIN32_HRESULTOR_H_
