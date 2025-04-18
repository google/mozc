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

#include <concepts>
#include <ostream>
#include <string>
#include <utility>

namespace mozc::win32 {

// HResult is a wrapper for HRESULT. Prefer this class to a raw HRESULT value as
// HResult disallows implicit conversions to other scalar types than HRESULT.
//
// To construct a HResult value, either use the explicit constructor or one of
// the predefined functions for well-known error codes.
//
// e.g.
//   HResult hr = HResultOk();
//   hr = HResultFail();
//   hr = HResult(kCustomErrorCode);
//
// You can get a human-readable string with the ToString() member function. You
// can also use operator<<() to stream the error string.
//
// Example:
//  if(HResult hr = SomeFunc(); !hr.Succeeded()) {
//    LOG(ERROR) << hr;
//  }
// Returned HResult values must not be discarded.
class [[nodiscard]] HResult {
 public:
  // Default construction is disallowed. Explicitly call HResultOk to construct
  // HResult for S_OK.
  HResult() = delete;

  // Constructs HResult with the error code hr.
  constexpr explicit HResult(const HRESULT hr) noexcept : hr_(hr) {}

  // Assigns an HRESULT value.
  HResult& operator=(const HRESULT hr) noexcept {
    hr_ = hr;
    return *this;
  }

  // Implicitly converts to HRESULT. This implicit conversion is necessary for
  // the RETURN_IF_FAILED_HRESULT macro to work both HRESULT and HResultOr<T>
  // return types.
  constexpr operator HRESULT() const noexcept  // NOLINT(runtime/explicit)
  {
    return hr_;
  }

  // Disallow implicit conversions to other scalar types.
  // This especially prevents accidental implicit conversions to bool.
  //
  //  bool Func() {
  //    HResult hr = HResultFail();
  //    // ...
  //    // return hr;  // compile error
  //    return hr.Succeeded();
  // }
  template <typename T>
    requires(!std::same_as<T, HRESULT> && std::convertible_to<T, bool>)
  operator T() = delete;

  // Returns the result of the SUCCEEDED macro.
  [[nodiscard]] constexpr bool Succeeded() const noexcept {
    return SUCCEEDED(hr_);
  }

  // Returns the result of the FAILED macro.
  [[nodiscard]] constexpr bool Failed() const noexcept { return FAILED(hr_); }

  // Returns the HRESULT value.
  constexpr HRESULT hr() const noexcept { return hr_; }

  inline void swap(HResult& other) noexcept {
    using std::swap;
    swap(hr_, other.hr_);
  }

  // Converts the error code to a human-readable string.
  inline std::string ToString() const;

 private:
  std::string ToStringSlow() const;

  HRESULT hr_;
};

// Returns a human-readable string of HRESULT.
//
// For well-known codes (defined as the HResultXX() functions), it returns
// strings like "Success: S_OK" and "Failure: E_FAIL'. For other successful
// values, it returns the value in hex ("Success: 0x00000002"). For other
// failure codes, it calls the Windows FormatMessage API and returns the result.
inline std::string HResult::ToString() const {
  if (hr_ == S_OK) {
    return "Success: S_OK";
  }
  return ToStringSlow();
}

constexpr bool operator==(const HResult& a, const HResult& b) {
  return a.hr() == b.hr();
}

constexpr bool operator!=(const HResult& a, const HResult& b) {
  return !(a == b);
}

inline std::ostream& operator<<(std::ostream& os, const HResult& hr) {
  os << hr.ToString();
  return os;
}

// Common values for HResult.
// https://learn.microsoft.com/en-us/windows/win32/com/error-handling-strategies
constexpr HResult HResultOk() { return HResult(S_OK); }
constexpr HResult HResultFalse() { return HResult(S_FALSE); }
constexpr HResult HResultAbort() { return HResult(E_ABORT); }
constexpr HResult HResultAccessDenied() { return HResult(E_ACCESSDENIED); }
constexpr HResult HResultFail() { return HResult(E_FAIL); }
constexpr HResult HResultHandle() { return HResult(E_HANDLE); }
constexpr HResult HResultInvalidArg() { return HResult(E_INVALIDARG); }
constexpr HResult HResultNoInterface() { return HResult(E_NOINTERFACE); }
constexpr HResult HResultNotImpl() { return HResult(E_NOTIMPL); }
constexpr HResult HResultOutOfMemory() { return HResult(E_OUTOFMEMORY); }
constexpr HResult HResultPending() { return HResult(E_PENDING); }
constexpr HResult HResultPointer() { return HResult(E_POINTER); }
constexpr HResult HResultUnexpected() { return HResult(E_UNEXPECTED); }

// Creates a HRESULT code from Windows error codes.
// https://learn.microsoft.com/en-us/windows/win32/com/structure-of-com-error-codes
constexpr HResult HResultWin32(const DWORD code) {
  return HResult(HRESULT_FROM_WIN32(code));
}

// RETURN_IF_FAILED_HRESULT Runs the statement and returns from the current
// function if FAILED(statement) is true.
#define RETURN_IF_FAILED_HRESULT(...)                            \
  do {                                                           \
    const HResult hresultor_macro_impl_tmp_hr(__VA_ARGS__);      \
    if (!hresultor_macro_impl_tmp_hr.Succeeded()) [[unlikely]] { \
      return hresultor_macro_impl_tmp_hr;                        \
    }                                                            \
  } while (0)

}  // namespace mozc::win32

#endif  // MOZC_BASE_WIN32_HRESULT_H_
