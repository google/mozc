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

#ifndef MOZC_BASE_WIN32_HRESULTOR_INTERNAL_H_
#define MOZC_BASE_WIN32_HRESULTOR_INTERNAL_H_

#include <windows.h>

#include <memory>
#include <type_traits>
#include <utility>

#include "base/win32/hresult.h"
#include "absl/base/attributes.h"

namespace mozc::win32 {

template <typename T>
class HResultOr;

namespace hresultor_internal {

// Tests if T is constructible from HResultOr<U>.
template <typename T, typename U>
using IsConstructibleFromHResultOr =
    std::disjunction<std::is_constructible<T, HResultOr<U>&>,
                     std::is_constructible<T, const HResultOr<U>&>,
                     std::is_constructible<T, HResultOr<U>&&>,
                     std::is_constructible<T, const HResultOr<U>&&>>;

// Tests if HResultOr<U> is convertible to T.
template <typename T, typename U>
using IsConvertibleFromHResultOr =
    std::disjunction<std::is_convertible<HResultOr<U>&, T>,
                     std::is_convertible<const HResultOr<U>&, T>,
                     std::is_convertible<HResultOr<U>&&, T>,
                     std::is_convertible<const HResultOr<U>&&, T>>;

// Tests if T is constructible or convertible from HResultOr<U>.
template <typename T, typename U>
using IsConstructibleOrConvertibleFromHResultOr =
    std::disjunction<IsConstructibleFromHResultOr<T, U>,
                     IsConvertibleFromHResultOr<T, U>>;

template <typename T, typename U>
inline constexpr bool IsConstructibleOrConvertibleFromHResultOrV =
    IsConstructibleOrConvertibleFromHResultOr<T, U>::value;

// Tests if T is assignable from HResultOr<U>.
template <typename T, typename U>
using IsAssignableFromHResultOr =
    std::disjunction<std::is_assignable<T&, HResultOr<U>&>,
                     std::is_assignable<T&, const HResultOr<U>&>,
                     std::is_assignable<T&, HResultOr<U>&&>,
                     std::is_assignable<T&, const HResultOr<U>&&>>;

template <typename T, typename U>
inline constexpr bool IsAssignableFromHResultOrV =
    IsAssignableFromHResultOr<T, U>::value;

// Tests if T is convertible to HResult or HRESULT.
template <typename T>
using IsConvertibleToHResultLike =
    std::disjunction<std::is_convertible<T, HResult>,
                     std::is_convertible<T, HRESULT>>;

template <typename T>
inline constexpr bool IsConvertibleToHResultLikeV =
    IsConvertibleToHResultLike<T>::value;

struct hresult_tag_t {};
inline constexpr hresult_tag_t hresult_tag;

template <typename T, bool = std::is_trivially_destructible_v<T>>
class HResultOrStorageBase;

template <typename T>
class HResultOrStorageBase<T, true /* trivially destructible */> {
 public:
  constexpr HResultOrStorageBase() noexcept : dummy_(), hr_(E_UNEXPECTED) {}
  template <typename... Args>
  constexpr explicit HResultOrStorageBase(std::in_place_t, Args&&... args)
      : value_(std::forward<Args>(args)...), hr_(S_OK) {}
  constexpr HResultOrStorageBase(hresult_tag_t, HRESULT hr)
      : dummy_(), hr_(hr) {}

  constexpr bool has_value() const noexcept { return SUCCEEDED(hr_); }
  ABSL_ATTRIBUTE_REINITIALIZES constexpr void AssignHResult(
      HRESULT hr) noexcept {
    hr_ = hr;
  }

 protected:
  struct Dummy {};

  union {
    Dummy dummy_;
    T value_;
  };
  HRESULT hr_;
};

template <typename T>
class HResultOrStorageBase<T, false /* not trivially destructible */> {
 public:
  constexpr HResultOrStorageBase() noexcept : dummy_(), hr_(E_UNEXPECTED) {}
  template <typename... Args>
  constexpr explicit HResultOrStorageBase(std::in_place_t, Args&&... args)
      : value_(std::forward<Args>(args)...), hr_(S_OK) {}
  constexpr HResultOrStorageBase(hresult_tag_t, HRESULT hr)
      : dummy_(), hr_(hr) {}

  ~HResultOrStorageBase() {
    if (has_value()) {
      value_.~T();
    }
  }

  constexpr bool has_value() const noexcept { return SUCCEEDED(hr_); }
  ABSL_ATTRIBUTE_REINITIALIZES constexpr void AssignHResult(
      HRESULT hr) noexcept {
    if (has_value()) {
      value_.~T();
    }
    hr_ = hr;
  }

 protected:
  struct Dummy {};

  union {
    Dummy dummy_;
    T value_;
  };
  HRESULT hr_;
};

// Common member functions to implement construction and assignment helpers.
template <typename T>
class HResultOrImpl : public HResultOrStorageBase<T> {
  using Base = HResultOrStorageBase<T>;

 public:
  using value_type = T;
  using error_type = HResult;
  using Base::Base;

  // error() returns the error code as HRESULT.
  constexpr HResult error() const noexcept { return HResult(hr_); }

  // Same as HResultOr<T>::operator *() but defined here to use internally too.
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

  template <typename... Args>
  constexpr void ConstructValue(Args&&... args) {
    // TODO(yuryu): use std::construct_at() when C++20 is ready.
    ::new (std::addressof(dummy_)) T(std::forward<Args>(args)...);
    hr_ = S_OK;
  }

  template <typename U>
  constexpr void Assign(U&& other) {
    if (this->has_value() == other.has_value()) {
      if (this->has_value()) {
        this->value_ = *std::forward<U>(other);
      } else {
        this->AssignHResult(other.error());
      }
    } else {
      if (other.has_value()) {
        this->ConstructValue(*std::forward<U>(other));
      } else {
        this->AssignHResult(other.error());
      }
    }
  }
};

template <typename T, bool = std::is_trivially_copy_constructible_v<T>>
struct HResultOrCopyBase : public HResultOrImpl<T> {
  using HResultOrImpl<T>::HResultOrImpl;
};

template <typename T>
struct HResultOrCopyBase<T, false /* not trivially copy constructible */>
    : public HResultOrImpl<T> {
  using HResultOrImpl<T>::HResultOrImpl;

  HResultOrCopyBase() = default;
  constexpr HResultOrCopyBase(const HResultOrCopyBase& other) {
    this->Assign(other);
  }
  HResultOrCopyBase& operator=(const HResultOrCopyBase& other) = default;
  HResultOrCopyBase(HResultOrCopyBase&& other) = default;
  HResultOrCopyBase& operator=(HResultOrCopyBase&& other) = default;
};

template <typename T, bool = std::is_trivially_move_constructible_v<T>>
struct HResultOrMoveBase : public HResultOrCopyBase<T> {
  using HResultOrCopyBase<T>::HResultOrCopyBase;
};

template <typename T>
struct HResultOrMoveBase<T, false /* not trivially move constructible */>
    : public HResultOrCopyBase<T> {
  using HResultOrCopyBase<T>::HResultOrCopyBase;

  HResultOrMoveBase() = default;
  HResultOrMoveBase(const HResultOrMoveBase& other) = default;
  HResultOrMoveBase& operator=(const HResultOrMoveBase& other) = default;
  constexpr HResultOrMoveBase(HResultOrMoveBase&& other) noexcept(
      std::is_nothrow_move_constructible_v<T>) {
    this->Assign(std::move(other));
  }
  HResultOrMoveBase& operator=(HResultOrMoveBase&& other) = default;
};

template <typename T, bool = std::is_trivially_destructible_v<T> &&
                             std::is_trivially_copy_constructible_v<T> &&
                             std::is_trivially_copy_assignable_v<T>>
struct HResultOrCopyAssignBase : public HResultOrMoveBase<T> {
  using HResultOrMoveBase<T>::HResultOrMoveBase;
};

template <typename T>
struct HResultOrCopyAssignBase<T, false /* not trivially copy assignable */>
    : public HResultOrMoveBase<T> {
  using HResultOrMoveBase<T>::HResultOrMoveBase;

  HResultOrCopyAssignBase() = default;
  HResultOrCopyAssignBase(const HResultOrCopyAssignBase& other) = default;
  constexpr HResultOrCopyAssignBase& operator=(
      const HResultOrCopyAssignBase& other) {
    this->Assign(other);
    return *this;
  }
  HResultOrCopyAssignBase(HResultOrCopyAssignBase&& other) = default;
  HResultOrCopyAssignBase& operator=(HResultOrCopyAssignBase&& other) = default;
};

template <typename T, bool = std::is_trivially_destructible_v<T> &&
                             std::is_trivially_move_constructible_v<T> &&
                             std::is_trivially_move_assignable_v<T>>
struct HResultOrMoveAssignBase : public HResultOrCopyAssignBase<T> {
  using HResultOrCopyAssignBase<T>::HResultOrCopyAssignBase;
};

template <typename T>
struct HResultOrMoveAssignBase<T, false /* not trivially move assignable */>
    : public HResultOrCopyAssignBase<T> {
 public:
  using HResultOrCopyAssignBase<T>::HResultOrCopyAssignBase;

  HResultOrMoveAssignBase() = default;
  HResultOrMoveAssignBase(const HResultOrMoveAssignBase& other) = default;
  HResultOrMoveAssignBase& operator=(const HResultOrMoveAssignBase& other) =
      default;
  HResultOrMoveAssignBase(HResultOrMoveAssignBase&& other) = default;
  constexpr HResultOrMoveAssignBase&
  operator=(HResultOrMoveAssignBase&& other) noexcept(
      std::is_nothrow_move_constructible_v<T> &&
      std::is_nothrow_move_assignable_v<T>) {
    this->Assign(std::move(other));
    return *this;
  }
};

template <typename T, bool = std::is_copy_constructible_v<T>>
struct CopyCtorBase {};

template <typename T>
struct CopyCtorBase<T, false> {
  CopyCtorBase() = default;
  CopyCtorBase(const CopyCtorBase&) = delete;
  CopyCtorBase(CopyCtorBase&&) = default;
  CopyCtorBase& operator=(const CopyCtorBase&) = default;
  CopyCtorBase& operator=(CopyCtorBase&&) = default;
};

template <typename T, bool = std::is_move_constructible_v<T>>
struct MoveCtorBase {};

template <typename T>
struct MoveCtorBase<T, false> {
  MoveCtorBase() = default;
  MoveCtorBase(const MoveCtorBase&) = default;
  MoveCtorBase(MoveCtorBase&&) = delete;
  MoveCtorBase& operator=(const MoveCtorBase&) = default;
  MoveCtorBase& operator=(MoveCtorBase&&) = default;
};

template <typename T, bool = std::is_copy_constructible_v<T> &&
                             std::is_copy_assignable_v<T>>
struct CopyAssignBase {};

template <typename T>
struct CopyAssignBase<T, false> {
  CopyAssignBase() = default;
  CopyAssignBase(const CopyAssignBase&) = default;
  CopyAssignBase(CopyAssignBase&&) = default;
  CopyAssignBase& operator=(const CopyAssignBase&) = delete;
  CopyAssignBase& operator=(CopyAssignBase&&) = default;
};

template <typename T, bool = std::is_move_constructible_v<T> &&
                             std::is_move_assignable_v<T>>
struct MoveAssignBase {};

template <typename T>
struct MoveAssignBase<T, false> {
  MoveAssignBase() = default;
  MoveAssignBase(const MoveAssignBase&) = default;
  MoveAssignBase(MoveAssignBase&&) = default;
  MoveAssignBase& operator=(const MoveAssignBase&) = default;
  MoveAssignBase& operator=(MoveAssignBase&&) = delete;
};

}  // namespace hresultor_internal
}  // namespace mozc::win32

#endif  // MOZC_BASE_WIN32_HRESULTOR_INTERNAL_H_
