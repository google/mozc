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

#include <winerror.h>

#include <concepts>
#include <type_traits>

#include "base/win32/hresult.h"

namespace mozc::win32 {

template <typename T>
class HResultOr;

namespace hresultor_internal {

// Tests if T is constructible or convertible from HResultOr<U>.
template <typename T, typename U>
inline constexpr bool IsConstructibleOrConvertibleFromHResultOrV =
    std::constructible_from<T, HResultOr<U>&> ||
    std::constructible_from<T, const HResultOr<U>&> ||
    std::constructible_from<T, HResultOr<U>&&> ||
    std::constructible_from<T, const HResultOr<U>&&> ||
    std::convertible_to<HResultOr<U>&, T> ||
    std::convertible_to<const HResultOr<U>&, T> ||
    std::convertible_to<HResultOr<U>&&, T> ||
    std::convertible_to<const HResultOr<U>&&, T>;

// Tests if T is assignable from HResultOr<U>.
template <typename T, typename U>
inline constexpr bool IsAssignableFromHResultOrV =
    std::is_assignable_v<T&, HResultOr<U>&> ||
    std::is_assignable_v<T&, const HResultOr<U>&> ||
    std::is_assignable_v<T&, HResultOr<U>&&> ||
    std::is_assignable_v<T&, const HResultOr<U>&&>;

// Tests if T is convertible to HResult or HRESULT.
template <typename T>
inline constexpr bool IsConvertibleToHResultLikeV =
    std::convertible_to<T, HResult> || std::convertible_to<T, HRESULT>;

}  // namespace hresultor_internal
}  // namespace mozc::win32

#endif  // MOZC_BASE_WIN32_HRESULTOR_INTERNAL_H_
