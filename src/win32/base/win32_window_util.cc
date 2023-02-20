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

#include "win32/base/win32_window_util.h"

// clang-format off
#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlstr.h>
// clang-format on

#include <memory>

#include "base/logging.h"
#include "base/port.h"
#include "base/win32/win_util.h"

namespace mozc {
namespace win32 {
namespace {

using ATL::CStringW;
using ATL::CWindow;

std::wstring SafeGetWindowText(HWND window_handle) {
  if (!::IsWindow(window_handle)) {
    return std::wstring();
  }

  const int text_len_without_null = GetWindowTextLength(window_handle);
  if (text_len_without_null <= 0) {
    return std::wstring();
  }

  const size_t buffer_len = text_len_without_null + 1;
  std::unique_ptr<wchar_t[]> buffer(new wchar_t[buffer_len]);

  const int copied_len_without_null =
      GetWindowText(window_handle, buffer.get(), buffer_len);

  if (copied_len_without_null <= 0) {
    return std::wstring();
  }

  return std::wstring(buffer.get(), copied_len_without_null);
}

}  // namespace

std::wstring WindowUtil::GetWindowClassName(HWND window_handle) {
  // Maximum length of WindowClass is assumed to be 256.
  // http://msdn.microsoft.com/en-us/library/ms633576.aspx
  wchar_t buffer[256 + 1] = {};
  const size_t num_copied_without_null =
      ::GetClassNameW(window_handle, buffer, std::size(buffer));
  if (num_copied_without_null + 1 >= std::size(buffer)) {
    return L"";
  }
  return std::wstring(buffer, num_copied_without_null);
}

// static
bool WindowUtil::ChangeMessageFilter(HWND window_handle, UINT message) {
  typedef BOOL(WINAPI * FPChangeWindowMessageFilterEx)(HWND, UINT, DWORD,
                                                       LPVOID);

  // The following constant is not available unless we change the WINVER
  // higher enough.
  constexpr int kMessageFilterAllow = 1;  // MSGFLT_ALLOW  (WINVER >=0x0601)

  const HMODULE lib = WinUtil::GetSystemModuleHandle(L"user32.dll");
  if (lib == nullptr) {
    LOG(ERROR) << L"GetModuleHandle for user32.dll failed.";
    return false;
  }

  // Windows 7+
  // http://msdn.microsoft.com/en-us/library/dd388202.aspx
  FPChangeWindowMessageFilterEx change_window_message_filter_ex =
      reinterpret_cast<FPChangeWindowMessageFilterEx>(
          ::GetProcAddress(lib, "ChangeWindowMessageFilterEx"));
  if (change_window_message_filter_ex != nullptr) {
    // Windows 7+ only
    if (!(*change_window_message_filter_ex)(window_handle, message,
                                            kMessageFilterAllow, nullptr)) {
      // Note: this actually fails in Internet Explorer 10 on Windows 8
      // with ERROR_ACCESS_DENIED (0x5).
      const int error = ::GetLastError();
      VLOG(1) << L"ChangeWindowMessageFilterEx failed. error = " << error;
      return false;
    }
    return true;
  }

  // Windows Vista
  if (!::ChangeWindowMessageFilter(message, MSGFLT_ADD)) {
    const int error = ::GetLastError();
    LOG(ERROR) << L"ChangeWindowMessageFilter failed. error = " << error;
    return false;
  }

  return true;
}

}  // namespace win32
}  // namespace mozc
