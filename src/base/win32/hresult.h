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

#ifndef MOZC_BASE_WIN32_HRESULT_H_
#define MOZC_BASE_WIN32_HRESULT_H_

#include <windows.h>

#include <utility>

#include "absl/base/attributes.h"

namespace mozc::win32 {

// HResult is a conversion class from HRESULT to HResultOr<T>.
// Constructing HResultOr<T> for error codes directly requires the type name for
// T every time, so you can instead write HResult(E_FAIL) and it gets implicitly
// converted to HResultOr<T>.
class HResult {
 public:
  HResult() = delete;
  constexpr explicit HResult(const HRESULT hr) noexcept : hr_(hr) {}

  // Implicitly converts to HRESULT. This implicit conversion is necessary for
  // the RETURN_IF_FAILED_HRESULT macro to work both HRESULT and HResultOr<T>
  // return types.
  constexpr operator HRESULT() const noexcept  // NOLINT(runtime/explicit)
  {
    return hr_;
  }

  ABSL_MUST_USE_RESULT constexpr bool ok() const noexcept {
    return SUCCEEDED(hr_);
  }
  constexpr HRESULT hr() const noexcept { return hr_; }

  friend void swap(HResult& a, HResult& b) noexcept {
    // std::swap will support the constexpr version from C++20.
    std::swap(a.hr_, b.hr_);
  }

 private:
  HRESULT hr_;
};

// RETURN_IF_FAILED_HRESULT Runs the statement and returns from the current
// function if FAILED(statement) is true.
#define RETURN_IF_FAILED_HRESULT(...)                            \
  do {                                                           \
    const HResult hresultor_macro_impl_tmp_hr(__VA_ARGS__);      \
    if (ABSL_PREDICT_FALSE(!hresultor_macro_impl_tmp_hr.ok())) { \
      return hresultor_macro_impl_tmp_hr;                        \
    }                                                            \
  } while (0)

}  // namespace mozc::win32

#endif  // MOZC_BASE_WIN32_HRESULT_H_
