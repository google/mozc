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
#include <unknwn.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include <new>
#include <type_traits>
#include <utility>

#include "base/win32/hresultor.h"
#include "absl/base/casts.h"

namespace mozc::win32 {

// ComRawPtr returns a raw pointer of the COM object. It returns the parameter
// as it is if it's already a raw pointer. If it's a ComPtr<T>, it calls
// ComPtr<T>::Get().
template <typename T>
T *ComRawPtr(T *p) {
  return p;
}

template <typename T>
T *ComRawPtr(const Microsoft::WRL::ComPtr<T> &p) {
  return p.Get();
}

// MakeComPtr is like std::make_unique but for ComPtr. Returns nullptr if new
// fails.
template <typename T, typename... Args>
Microsoft::WRL::ComPtr<T> MakeComPtr(Args &&...args) {
  static_assert(std::is_base_of_v<IUnknown, T>, "T must implement IUnknown.");

  return Microsoft::WRL::ComPtr<T>(new (std::nothrow)
                                       T(std::forward<Args>(args)...));
}

// Calls CoCreateInstance and returns the result as ComPtr<Interface>.
template <typename Interface>
Microsoft::WRL::ComPtr<Interface> ComCreateInstance(REFCLSID clsid) {
  Microsoft::WRL::ComPtr<Interface> result;
  if (SUCCEEDED(CoCreateInstance(clsid, nullptr, CLSCTX_ALL,
                                 IID_PPV_ARGS(&result)))) {
    return result;
  }
  return nullptr;
}

// Calls CoCreateInstance and returns the result as ComPtr<Interface> with the
// CLSID from T.
template <typename Interface, typename T>
Microsoft::WRL::ComPtr<Interface> ComCreateInstance() {
  return ComCreateInstance<Interface>(__uuidof(T));
}

// Returns the result of QueryInterface as HResultOr<ComPtr<T>>.
template <typename T, typename U>
HResultOr<Microsoft::WRL::ComPtr<T>> ComQueryHR(U &&source) {
  using ReturnType = HResultOr<Microsoft::WRL::ComPtr<T>>;

  auto ptr = ComRawPtr(std::forward<U>(source));
  // If source is convertible to T, casting is faster than calling
  // QueryInterface.
  if constexpr (std::is_convertible_v<decltype(ptr), T *>) {
    // ComPtr will call AddRef here.
    return ReturnType(absl::implicit_cast<T *>(ptr));
  }
  Microsoft::WRL::ComPtr<T> result;
  const HRESULT hr = ptr->QueryInterface(IID_PPV_ARGS(&result));
  if (SUCCEEDED(hr)) {
    return ReturnType(std::move(result));
  }
  return ReturnType(hr);
}

// Returns the result of QueryInterface as ComPtr<T>.
template <typename T, typename U>
Microsoft::WRL::ComPtr<T> ComQuery(U &&source) {
  return std::move(ComQueryHR<T>(std::forward<U>(source))).value_or(nullptr);
}

// Similar to ComQuery but returns nullptr if source is nullptr.
template <typename T, typename U>
Microsoft::WRL::ComPtr<T> ComCopy(U &&source) {
  auto ptr = ComRawPtr(std::forward<U>(source));
  if (ptr) {
    return ComQuery<T>(ptr);
  }
  return nullptr;
}

}  // namespace mozc::win32

#endif  // MOZC_BASE_WIN32_COM_H_
