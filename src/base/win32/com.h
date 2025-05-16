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

#ifndef MOZC_BASE_WIN32_COM_H_
#define MOZC_BASE_WIN32_COM_H_

#include <objbase.h>
#include <oleauto.h>
#include <unknwn.h>
#include <wil/com.h>
#include <wil/resource.h>

#include <concepts>
#include <new>
#include <string_view>
#include <type_traits>
#include <utility>

#include "base/win32/hresult.h"
#include "base/win32/hresultor.h"

namespace mozc::win32 {

// A concept to check if a type is a COM interface.
template <typename I>
concept ComInterface = std::derived_from<I, IUnknown> && std::is_abstract_v<I>;

// MakeComPtr is like std::make_unique but for COM pointers. Returns nullptr if
// new fails.
template <typename T, typename... Args>
wil::com_ptr_nothrow<T> MakeComPtr(Args &&...args) {
  static_assert(std::is_base_of_v<IUnknown, T>, "T must implement IUnknown.");

  return wil::com_ptr_nothrow<T>(new (std::nothrow)
                                     T(std::forward<Args>(args)...));
}

// Calls CoCreateInstance and returns the result as
// wil::com_ptr_nothrow<Interface>.
template <typename Interface>
wil::com_ptr_nothrow<Interface> ComCreateInstance(REFCLSID clsid) {
  // TODO(yuryu): Restrict CLSCTX.
  return wil::CoCreateInstanceNoThrow<Interface>(clsid, CLSCTX_ALL);
}

// Calls CoCreateInstance and returns the result as
// wil::com_ptr_nothrow<Interface> with the CLSID from T.
template <typename Interface, typename T>
wil::com_ptr_nothrow<Interface> ComCreateInstance() {
  return ComCreateInstance<Interface>(__uuidof(T));
}

// Returns the result of QueryInterface as HResultOr<wil::com_ptr_nothrow<T>>.
template <typename T, typename U>
HResultOr<wil::com_ptr_nothrow<T>> ComQueryHR(U &&source) {
  static_assert(ComInterface<T>, "T must be a COM interface.");

  wil::com_ptr_nothrow<T> result;
  // Workaround as WIL doesn't detect convertible queries with VC++2017.
  auto *ptr = wil::com_raw_ptr(std::forward<U>(source));
  if constexpr (std::derived_from<std::remove_pointer_t<decltype(ptr)>, T>) {
    // The constructor of wil::com_ptr calls AddRef().
    return ptr;
  } else {
    const HRESULT hr = wil::com_query_to_nothrow<T>(std::move(ptr), &result);
    if (SUCCEEDED(hr)) {
      return result;
    }
    return HResult(hr);
  }
}

// Returns the result of QueryInterface as wil::com_ptr_nothrow<T>.
// Use this function instead of wil::try_com_query_nothrow.
template <typename T, typename U>
wil::com_ptr_nothrow<T> ComQuery(U &&source) {
  static_assert(ComInterface<T>, "T must be a COM interface.");

  // Workaround as WIL doesn't detect convertible queries with VC++2017.
  auto *ptr = wil::com_raw_ptr(std::forward<U>(source));
  if constexpr (std::derived_from<std::remove_pointer_t<decltype(ptr)>, T>) {
    // The constructor of wil::com_ptr calls AddRef().
    return ptr;
  } else {
    return wil::try_com_query_nothrow<T>(std::move(ptr));
  }
}

// Similar to ComQuery but returns nullptr if source is nullptr.
// Use this function instead of wil::try_com_copy_nothrow.
template <typename T, typename U>
wil::com_ptr_nothrow<T> ComCopy(U &&source) {
  if (source) {
    return ComQuery<T>(std::forward<U>(source));
  } else {
    return nullptr;
  }
}

// MakeUniqueBSTR allocates a new BSTR and returns as wil::unique_bstr.
// Use this function instead of wil::make_bstr() as it doesn't use
// SysAllocStringLen() for strings with sizes.
inline wil::unique_bstr MakeUniqueBSTR(const std::wstring_view source) {
  return wil::unique_bstr(SysAllocStringLen(source.data(), source.size()));
}

inline wil::unique_bstr MakeUniqueBSTR(const wchar_t *source) {
  return wil::unique_bstr(SysAllocString(source));
}

}  // namespace mozc::win32

#endif  // MOZC_BASE_WIN32_COM_H_
