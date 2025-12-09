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

#include <windows.h>

#include <cstddef>
#include <string>

#include "base/vlog.h"

namespace mozc {
namespace win32 {
namespace {

std::wstring SafeGetWindowText(HWND window_handle) {
  if (!::IsWindow(window_handle)) {
    return std::wstring();
  }

  const int text_len_without_null = GetWindowTextLengthW(window_handle);
  if (text_len_without_null <= 0) {
    return std::wstring();
  }

  std::wstring buffer(text_len_without_null, 0);

  const int copied_len_without_null =
      GetWindowTextW(window_handle, buffer.data(), buffer.size() + 1);

  if (copied_len_without_null <= 0) {
    return std::wstring();
  }

  return buffer;
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
  // http://msdn.microsoft.com/en-us/library/dd388202.aspx
  if (!ChangeWindowMessageFilterEx(window_handle, message, MSGFLT_ALLOW,
                                   nullptr)) {
    // Note: this actually fails in Internet Explorer 10 on Windows 8
    // with ERROR_ACCESS_DENIED (0x5).
    const int error = ::GetLastError();
    MOZC_VLOG(1) << "ChangeWindowMessageFilterEx failed. error = " << error;
    return false;
  }
  return true;
}

}  // namespace win32
}  // namespace mozc
