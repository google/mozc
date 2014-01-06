// Copyright 2010-2014, Google Inc.
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

#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
#define _ATL_NO_HOSTING
// Workaround against KB813540
#include <atlbase_mozc.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlstr.h>

#include <memory>

#include "base/logging.h"
#include "base/port.h"
#include "base/system_util.h"
#include "base/util.h"
#include "base/win_util.h"

namespace mozc {
namespace win32 {
namespace {

using ATL::CWindow;
using ATL::CStringW;
using std::unique_ptr;

wstring SafeGetWindowText(HWND window_handle) {
  if (!::IsWindow(window_handle)) {
    return wstring();
  }

  const int text_len_without_null = GetWindowTextLength(window_handle);
  if (text_len_without_null <= 0) {
    return wstring();
  }

  const size_t buffer_len = text_len_without_null + 1;
  unique_ptr<wchar_t[]> buffer(new wchar_t[buffer_len]);

  const int copied_len_without_null = GetWindowText(
      window_handle, buffer.get(), buffer_len);

  if (copied_len_without_null <= 0) {
    return wstring();
  }

  return wstring(buffer.get(), copied_len_without_null);
}

}  // namespace

wstring WindowUtil::GetWindowClassName(HWND window_handle) {
  // Maximum length of WindowClass is assumed to be 256.
  // http://msdn.microsoft.com/en-us/library/ms633576.aspx
  wchar_t buffer[256 + 1] = {};
  const size_t num_copied_without_null = ::GetClassNameW(
      window_handle, buffer, arraysize(buffer));
  if (num_copied_without_null + 1 >= arraysize(buffer)) {
    return L"";
  }
  return wstring(buffer, num_copied_without_null);
}

// static
bool WindowUtil::IsSuppressSuggestionWindow(HWND window_handle) {
  return (IsInChromeOmnibox(window_handle) ||
          IsInGoogleSearchBox(window_handle));
}

// static
bool WindowUtil::IsInChromeOmnibox(HWND window_handle) {
  if (!::IsWindow(window_handle)) {
    return false;
  }

  const wstring &class_name = GetWindowClassName(window_handle);
  if (class_name.empty()) {
    return false;
  }

  // b/7508193
  const wchar_t kChromeOmnibox[] = L"Chrome_OmniboxView";
  if (class_name == kChromeOmnibox) {
    return true;
  }
  return false;
}

// static
bool WindowUtil::IsInGoogleSearchBox(HWND window_handle) {
  if (!::IsWindow(window_handle)) {
    return false;
  }
  const HWND root_window_handle = ::GetAncestor(window_handle, GA_ROOT);
  if (!::IsWindow(root_window_handle)) {
    return false;
  }
  const wstring &class_name = GetWindowClassName(window_handle);
  if (class_name.empty()) {
    return false;
  }

  // b/7098463
  const wchar_t *kBrowserWindowClasses[] = {
      L"Chrome_RenderWidgetHostHWND",
      L"MozillaWindowClass",
      L"Internet Explorer_Server",
      L"OperaWindowClass",
  };

  bool is_browser = false;
  for (size_t i = 0; i < arraysize(kBrowserWindowClasses); ++i) {
    if (kBrowserWindowClasses[i] == class_name) {
      is_browser = true;
      break;
    }
  }
  if (!is_browser) {
    return false;
  }
  string root_title;
  mozc::Util::WideToUTF8(SafeGetWindowText(root_window_handle),
                         &root_title);

  const char kGoogleSearchJa[] = "- Google \xE6\xA4\x9C\xE7\xB4\xA2 -";
  const char kGoogleSearchEn[] = "- Google Search -";
  const char kGoogleSearchPrefix[] = "Google - ";
  if (root_title.find(kGoogleSearchJa) != string::npos) {
    return true;
  }
  if (root_title.find(kGoogleSearchEn) != string::npos) {
    return true;
  }
  if (mozc::Util::StartsWith(root_title, kGoogleSearchPrefix)) {
    return true;
  }
  return false;
}

// static
bool WindowUtil::ChangeMessageFilter(HWND window_handle, UINT message) {
  typedef BOOL (WINAPI *FPChangeWindowMessageFilter)(UINT, DWORD);
  typedef BOOL (WINAPI *FPChangeWindowMessageFilterEx)(
      HWND, UINT, DWORD, LPVOID);

  // Following constants are not available unless we change the WINVER
  // higher enough.
  const int kMessageFilterAdd = 1;    // MSGFLT_ADD    (WINVER >=0x0600)
  const int kMessageFilterAllow = 1;  // MSGFLT_ALLOW  (WINVER >=0x0601)

  // Skip windows XP.
  // ChangeWindowMessageFilter is only available on Windows Vista or Later
  if (!SystemUtil::IsVistaOrLater()) {
    LOG(ERROR) << "Skip ChangeWindowMessageFilter on Windows XP";
    return true;
  }

  const HMODULE lib = WinUtil::GetSystemModuleHandle(L"user32.dll");
  if (lib == nullptr) {
    LOG(ERROR) << L"GetModuleHandle for user32.dll failed.";
    return false;
  }

  // Windows 7+
  // http://msdn.microsoft.com/en-us/library/dd388202.aspx
  FPChangeWindowMessageFilterEx change_window_message_filter_ex
      = reinterpret_cast<FPChangeWindowMessageFilterEx>(
          ::GetProcAddress(lib, "ChangeWindowMessageFilterEx"));
  if (change_window_message_filter_ex != nullptr) {
    // Windows 7+ only
    if (!(*change_window_message_filter_ex)(
            window_handle, message, kMessageFilterAllow, nullptr)) {
      // Note: this actually fails in Internet Explorer 10 on Windows 8
      // with ERROR_ACCESS_DENIED (0x5).
      const int error = ::GetLastError();
      VLOG(1) << L"ChangeWindowMessageFilterEx failed. error = " << error;
      return false;
    }
    return true;
  }

  // Windows Vista
  FPChangeWindowMessageFilter change_window_message_filter
      = reinterpret_cast<FPChangeWindowMessageFilter>(
          ::GetProcAddress(lib, "ChangeWindowMessageFilter"));
  if (change_window_message_filter == nullptr) {
    const int error = ::GetLastError();
    LOG(ERROR) << L"GetProcAddress failed. error = " << error;
    return false;
  }

  DCHECK(change_window_message_filter != nullptr);
  if (!(*change_window_message_filter)(message, kMessageFilterAdd)) {
    const int error = ::GetLastError();
    LOG(ERROR) << L"ChangeWindowMessageFilter failed. error = " << error;
    return false;
  }

  return true;
}

}  // namespace win32
}  // namespace mozc
