// Copyright 2010-2013, Google Inc.
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

#include "base/port.h"
#include "base/util.h"

namespace mozc {
namespace win32 {

using ATL::CWindow;
using ATL::CStringW;

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
  const CWindow root_window(::GetAncestor(window_handle, GA_ROOT));
  if (!root_window.IsWindow()) {
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
  const CWindow root_window(::GetAncestor(window_handle, GA_ROOT));
  if (!root_window.IsWindow()) {
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
  {
    CStringW wide_str;
    if (!root_window.GetWindowTextW(wide_str)) {
      return false;
    }
    mozc::Util::WideToUTF8(wide_str.GetBuffer(), &root_title);
  }

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

}  // namespace win32
}  // namespace mozc
