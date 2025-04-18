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

#include <winerror.h>

#include <cstddef>
#include <initializer_list>
#include <memory>
#include <ostream>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/log/check.h"
#include "base/port.h"
#include "base/win32/hresult.h"
#include "base/win32/hresultor_internal.h"

namespace mozc::win32 {

// HResultOr<T> is a HRESULT version of absl::StatusOr<T>.
// The templates used for constructor and assignment operators largely follow
// absl::StatusOr<T>.
//
// HRESULT is set to S_OK if HResultOr is initialized or assigned with a value.
// If you also need to return different HRESULT codes, use std::pair<HRESULT, T>
// instead.
//
// HResultOr<T> return values must not be discarded.
//
// Note: If T is convertible to HRESULT (e.g. DWORD), HResult<T>(value) deletes
// the in-place constructors and value assignment operators. Use HResultOk() or
// HResult<T>(std::in_place, value) instead. This limitation is to disallow
// confusing statements like
//
//  HResult<int> return;
//  // many lines
//  return = E_FAIL;  // Error. E_FAIL is convertible to int.
template <class T>
class [[nodiscard]] HResultOr {
 public:
  static_assert(!std::is_void_v<std::remove_cvref_t<T>>,
                "void is not allowed as the type of the value.");
  static_assert(!std::is_reference_v<T>, "reference type is not supported.");

  using value_type = T;
  using error_type = HRESULT;

  // Delete the default constructor. Use HResultOk<T>() instead.
  HResultOr() = delete;

  // Copyable if T is copyable. HResultOr<T> is trivially copyable if T is
  // trivially copyable.
  constexpr HResultOr(const HResultOr&) = default;
  constexpr HResultOr(const HResultOr& other) noexcept(
      std::is_nothrow_copy_constructible_v<T>)
    requires(std::is_copy_constructible_v<T> &&
             !std::is_trivially_copy_constructible_v<T>)
      : HResultOr(std::in_place, other.value_) {}
  constexpr HResultOr& operator=(const HResultOr&) = default;
  constexpr HResultOr& operator=(const HResultOr& other) noexcept(
      std::is_nothrow_copy_constructible_v<T> &&
      std::is_nothrow_copy_assignable_v<T>)
    requires(std::is_copy_assignable_v<T> &&
             !(std::is_trivially_destructible_v<T> &&
               std::is_trivially_copy_constructible_v<T> &&
               std::is_trivially_copy_assignable_v<T>))
  {
    Assign(other);
    return *this;
  }

  // Movable if T is movable. HResultOr<T> is
  //  - noexcept movable if T has a noexcept move constructor.
  //  - trivially movable if T is trivially movable.
  constexpr HResultOr(HResultOr&&) = default;
  constexpr HResultOr(HResultOr&& other) noexcept(
      std::is_nothrow_move_constructible_v<T>)
    requires(std::is_move_constructible_v<T> &&
             !std::is_trivially_move_constructible_v<T>)
      : HResultOr(std::in_place, std::move(other.value_)) {}
  constexpr HResultOr& operator=(HResultOr&&) = default;
  constexpr HResultOr& operator=(HResultOr&& other) noexcept(
      std::is_nothrow_move_constructible_v<T> &&
      std::is_nothrow_move_assignable_v<T>)
    requires(std::is_move_assignable_v<T> &&
             !(std::is_trivially_destructible_v<T> &&
               std::is_trivially_move_constructible_v<T> &&
               std::is_trivially_move_assignable_v<T>))
  {
    Assign(std::move(other));
    return *this;
  }

  // Converting copy and move constructors from HResultOk<U> where
  //  1. U is not T,
  //  2. T is constructible from U (const U& for copy and U for move), and
  //  3. HResultOk<U> is not directly constructible or convertible to T.
  //
  // This constructor is implicit if std::is_convertible_v<U> is true.
  template <typename U>
    requires(
        !std::same_as<T, U> && std::constructible_from<T, U> &&
        !hresultor_internal::IsConstructibleOrConvertibleFromHResultOrV<T, U>)
  constexpr explicit(!std::is_convertible_v<const U&, T>)
      HResultOr(const HResultOr<U>& other) {
    Assign(other);
  }

  template <typename U>
    requires(
        !std::same_as<T, U> && std::constructible_from<T, U> &&
        !hresultor_internal::IsConstructibleOrConvertibleFromHResultOrV<T, U>)
  constexpr explicit(!std::is_convertible_v<U&&, T>)
      HResultOr(HResultOr<U>&& other) {
    Assign(std::move(other));
  }

  // Converting assignment operators from HResultOr<U>.
  //
  // These overloads participate in resolution if
  //  1. T is not U,
  //  2. T is constructible and assignable from U, and
  //  3. HResultOr<T> is not directly constructible, convertible, or assignable
  //     from HResult<U>.
  //
  // These operators are called in a case like these:
  //  HResultOk<int64_t> foo;
  //  // ...
  //  foo = HResultOk(42);
  template <typename U>
    requires(!std::same_as<T, U> && std::constructible_from<T, const U&> &&
             std::assignable_from<T&, const U&> &&
             !(hresultor_internal::IsConstructibleOrConvertibleFromHResultOrV<
                   T, U> ||
               hresultor_internal::IsAssignableFromHResultOrV<T, U>))
  constexpr HResultOr& operator=(const HResultOr<U>& other) {
    Assign(other);
    return *this;
  }

  template <typename U>
    requires(!std::same_as<T, U> && std::constructible_from<T, U> &&
             std::assignable_from<T&, U> &&
             !(hresultor_internal::IsConstructibleOrConvertibleFromHResultOrV<
                   T, U> ||
               hresultor_internal::IsAssignableFromHResultOrV<T, U>))
  constexpr HResultOr& operator=(HResultOr<U>&& other) {
    Assign(std::move(other));
    return *this;
  }

  // Direct-initializing constructors.
  // Like absl::StatusOr<T>, these constructors participate in overload
  // resolution if
  //  1. T is constructible from U,
  //  2. remove_cvref_t<U> is not HResultOr<T>, HResult, or std::in_place,
  //  3. U is not convertible to HResultOr<T>, and
  //  4. U is not convertible to HRESULT or HResult.
  //
  // This constructor is explicit if U is not convertible to T.
  //
  // The implicit constructor is necessary to write
  //
  // HResultOr<T> Foo() {
  //   T value;
  //   // ...
  //   return value;
  // }
  //
  // Elsewhere in the code, the recommended way of constructing a valid
  // HResultOr<T> is to use the HResultOk<T>() function.
  //
  // Direct-initialization is disallowed if U is convertible to HRESULT.
  // Use HResult() and HResultOk() explicitly to remove ambiguity.
  //
  // For example, write
  //   HResultOr<int> i(HResultOk(42));
  // instead of
  //   HResultOr<int> i(42);
  template <typename U>
    requires(std::constructible_from<T, U> &&
             !std::same_as<std::remove_cvref_t<U>, std::in_place_t> &&
             !std::same_as<std::remove_cvref_t<U>, HResultOr> &&
             !std::same_as<std::remove_cvref_t<U>, HResult> &&
             !hresultor_internal::IsConvertibleToHResultLikeV<U>)
  constexpr explicit(!std::is_convertible_v<U, T>) HResultOr(U&& value)
      : HResultOr(std::in_place, std::forward<U>(value)) {}

  // Perfect-forwarding assignment operator for value.
  // This operator participates in overload resolution if
  //  1. remove_cvref<U> is not HResultOr<T>,
  //  2. T is constructible and assignable from U, and
  //  3. U is not convertible to HResult or HRESULT.
  template <typename U>
    requires(!std::same_as<std::remove_cvref_t<U>, HResultOr> &&
             std::convertible_to<T, U> && std::assignable_from<T&, U> &&
             !hresultor_internal::IsConvertibleToHResultLikeV<T>)
  constexpr HResultOr& operator=(U&& value) {
    if (has_value()) {
      value_ = std::forward<U>(value);
    } else {
      ConstructValue(std::forward<U>(value));
    }
    return *this;
  }

  // In-place construction of T.
  template <typename... Args>
  constexpr explicit HResultOr(std::in_place_t, Args&&... args)
      : value_(std::forward<Args>(args)...), hr_(S_OK) {}
  template <typename U, typename... Args>
  constexpr explicit HResultOr(std::in_place_t, std::initializer_list<U> ilist,
                               Args&&... args)
      : value_(ilist, std::forward<Args>(args)...), hr_(S_OK) {}

  // Constructions and assignments from a non-successful HResult.
  //
  // These functions perform DCHECK(FAILED(hr). Use HResultOk() to construct a
  // successful value.
  //
  // The assignment operator destroys the current value.
  constexpr HResultOr(const HResult hr)  // NOLINT(runtime/explicit)
      noexcept
      : placeholder_(), hr_(hr) {
    DCHECK(!has_value());
  }

  HResultOr& operator=(const HResult hr) noexcept {
    AssignHResult(hr);
    DCHECK(!has_value());
    return *this;
  }

  ~HResultOr() = default;
  ~HResultOr()
    requires(!std::is_trivially_destructible_v<T>)
  {
    DestroyValue();
  }

  // has_value() returns true if HResultOr<T> has a valid value.
  constexpr bool has_value() const noexcept { return SUCCEEDED(hr_); }

  // Returns error code as HResult.
  constexpr HResult error() const noexcept { return HResult(hr_); }

  // value() tests has_value() with `CHECK()` and returns the value.
  constexpr T& value() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    CHECK(has_value());
    return value_;
  }
  constexpr const T& value() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    CHECK(has_value());
    return value_;
  }
  constexpr T&& value() && ABSL_ATTRIBUTE_LIFETIME_BOUND {
    CHECK(has_value());
    return std::move(value_);
  }
  constexpr const T&& value() const&& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    CHECK(has_value());
    return std::move(value_);
  }

  // operator*() returns the value.
  //
  // REQUIRES: has_value() == true.
  constexpr T& operator*() & noexcept ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return value_;
  }
  constexpr const T& operator*() const& noexcept ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return value_;
  }
  constexpr T&& operator*() && noexcept ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::move(value_);
  }
  constexpr const T&& operator*() const&& noexcept
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::move(value_);
  }

  // operator->() returns a pointer to the value.
  //
  // REQUIRES: has_value() == true.
  constexpr T* operator->() noexcept ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::addressof(value_);
  }
  constexpr const T* operator->() const noexcept ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::addressof(value_);
  }

  // value_or() returns a value if has_value() == true, otherwise returns
  // default_value. Unlike value() or operator*(), value_or() returns a value
  // (not a reference). To avoid unnecessary copies, use std::move() to call the
  // rvalue overload.
  //
  // Example:
  //  T value = std::move(hresultor).value_or(default_value);
  template <typename U>
  constexpr T value_or(U&& default_value) const& {
    if (has_value()) {
      return value_;
    }
    return std::forward<U>(default_value);
  }
  template <typename U>
  constexpr T value_or(U&& default_value) && {
    if (has_value()) {
      return std::move(value_);
    }
    return std::forward<U>(default_value);
  }

  void swap(HResultOr& other) noexcept(
      std::is_nothrow_swappable_v<T> &&
      std::is_nothrow_move_constructible_v<T>) {
    if (has_value() && other.has_value()) {
      using std::swap;
      swap(value_, other.value_);
    } else {
      if (has_value()) {
        const HRESULT other_hr = other.error();
        other.ConstructValue(std::move(value_));
        AssignHResult(other_hr);
      } else {
        const HRESULT this_hr = error();
        ConstructValue(std::move(other.value_));
        other.AssignHResult(this_hr);
      }
    }
  }

  friend void swap(HResultOr& lhs,
                   HResultOr& rhs) noexcept(noexcept(lhs.swap(rhs))) {
    lhs.swap(rhs);
  }

 private:
  struct Placeholder {};

  template <typename... Args>
  ABSL_ATTRIBUTE_REINITIALIZES constexpr void ConstructValue(Args&&... args) {
    std::construct_at(std::addressof(value_), std::forward<Args>(args)...);
    hr_ = S_OK;
  }

  constexpr void DestroyValue() noexcept {
    if constexpr (!std::is_trivially_destructible_v<T>) {
      if (has_value()) {
        std::destroy_at(std::addressof(value_));
      }
    }
  }

  template <typename U>
  constexpr void Assign(U&& other) {
    if (has_value() == other.has_value()) {
      if (has_value()) {
        value_ = *std::forward<U>(other);
      } else {
        AssignHResult(other.error());
      }
    } else {
      if (other.has_value()) {
        ConstructValue(*std::forward<U>(other));
      } else {
        AssignHResult(other.error());
      }
    }
  }

  ABSL_ATTRIBUTE_REINITIALIZES constexpr void AssignHResult(
      const HRESULT hr) noexcept {
    DestroyValue();
    hr_ = hr;
  }

  union {
    MOZC_NO_UNIQUE_ADDRESS_ATTRIBUTE Placeholder placeholder_;
    T value_;
  };
  HRESULT hr_ = E_UNEXPECTED;
};

// Comparison operators between HResult.
template <typename T, typename U>
constexpr bool operator==(const HResultOr<T>& a, const HResultOr<U>& b) {
  if (a.has_value() && b.has_value()) {
    return *a == *b;
  }
  return a.error() == b.error();
}

// Comparison operators between HResultOr<T> and a value.
template <typename T, typename U>
constexpr bool operator==(const HResultOr<T>& a, const U& b) {
  if (!a.has_value()) {
    return false;
  }
  return *a == b;
}

// Comparison operators between HResultOr<T> and HResult.
template <typename T>
constexpr bool operator==(const HResultOr<T>& a, const HResult b) {
  return a.error() == b;
}

// HResultOk() overloads defined here return a HResultOr<T> with a valid value.
// Similar to std::make_optional().
// Examples:
//
//  Type value;
//  HResultOr<Type> result = HResultOk(value);
//  auto result2 = HResultOk<Type>(constructor, argments);
//  auto result3 = HResultOk<std::vector<std::string>>({"initializer", "list"});
template <typename U, typename T = std::decay_t<U>>
constexpr HResultOr<T> HResultOk(U&& value) {
  return HResultOr<T>(std::in_place, std::forward<U>(value));
}

template <typename T, typename... Args>
constexpr HResultOr<T> HResultOk(Args&&... args) {
  return HResultOr<T>(std::in_place, std::forward<Args>(args)...);
}

template <typename T, typename... Args>
constexpr HResultOr<T> HResultOk(std::initializer_list<T> ilist,
                                 Args&&... args) {
  return HResultOk<T>(std::in_place, ilist, std::forward<Args>(args)...);
}

// operator<<() outputs the underlying HRESULT code.
template <typename T>
std::ostream& operator<<(std::ostream& os, const HResultOr<T>& v) {
  return operator<<(os, v.error());
}

#define HRESULTOR_MACRO_IMPL_CONCAT_INTERNAL_(x, y) x##y
#define HRESULTOR_MACRO_IMPL_CONCAT_(x, y) \
  HRESULTOR_MACRO_IMPL_CONCAT_INTERNAL_(x, y)

#define HRESULTOR_MACRO_IMPL_ASSIGN_OR_RETURN_HRESULT_(tmp, lhs, expr) \
  auto tmp = expr;                                                     \
  if (!tmp.has_value()) [[unlikely]] {                                 \
    return tmp.error();                                                \
  }                                                                    \
  lhs = *std::move(tmp)

// Helper macros for HResultOr.
// ASSIGN_OR_RETURN_HRESULT Assigns expr to lhs if HResultOr::has_value() is
// true, otherwise returns HResultOr::error() and exits the function.
//
// ASSIGN_OR_RETURN_HRESULT(std::wstring str, foo->Bar());
// ASSIGN_OR_RETURN_HRESULT(auto i, ComQueryHR<IInterface>(p));
//
// Limitation: This macro doesn't work if lhs has a "," inside.
#define ASSIGN_OR_RETURN_HRESULT(lhs, ...)                                    \
  HRESULTOR_MACRO_IMPL_ASSIGN_OR_RETURN_HRESULT_(                             \
      HRESULTOR_MACRO_IMPL_CONCAT_(hresultor_assign_or_return_tmp, __LINE__), \
      lhs, (__VA_ARGS__))

}  // namespace mozc::win32

#endif  // MOZC_BASE_WIN32_HRESULTOR_H_
