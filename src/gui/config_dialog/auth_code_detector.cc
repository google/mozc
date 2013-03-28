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

#include "gui/config_dialog/auth_code_detector.h"

#if defined(OS_WIN)
#include <Windows.h>
#endif  // OS_WIN

#if defined(OS_MACOSX)
#include <Carbon/Carbon.h>
#endif  // OS_MACOSX

#if defined(OS_WIN)
#include "base/util.h"
#endif  // OS_WIN
#include "sync/oauth2_token_util.h"

namespace mozc {
namespace gui {
namespace {

#if defined(OS_WIN)
BOOL CALLBACK EnumWindowsProc(HWND window_handle, LPARAM lparam) {
  wchar_t buffer[512] = {};
  const int copied_len_without_null =
      ::GetWindowText(window_handle, buffer, arraysize(buffer));
  if (copied_len_without_null == 0) {
    // Failed. Go to next window.
    return TRUE;
  }

  if (copied_len_without_null > arraysize(buffer)) {
    // Result is too big. Go to next window.
    return TRUE;
  }

  const wstring title(buffer, buffer + copied_len_without_null);
  string title_utf8;
  mozc::Util::WideToUTF8(title, &title_utf8);

  const string &auth_code =
      sync::OAuth2TokenUtil::ParseAuthCodeFromWindowTitleForWindows(
          title_utf8);
  if (auth_code.empty()) {
    // Not found. go to next window.
    return TRUE;
  }

  // Target window found. Stop iteration.
  *reinterpret_cast<string *>(lparam) = auth_code;
  return FALSE;
}

// Returns auth code by enumerating all the top-level windows and parsing
// their window titles. See the comment of ParseAuthCodeFromWindowTitle about
// how the auth code is embedded into window titles by typical browsers.
string GetAuthCodeForWindows() {
  string auth_code;
  ::EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&auth_code));
  return auth_code;
}

#elif defined(OS_MACOSX)
string GetAuthCodeForMac() {
  // Carbon API allows a user process to obtain the information of all
  // windows in the desktop.  See:
  // http://developer.apple.com/library/mac/#documentation
  //   /Carbon/Reference/CGWindow_Reference/Reference/Functions.html
  CFArrayRef window_list = ::CGWindowListCopyWindowInfo(
      kCGWindowListOptionOnScreenOnly, kCGNullWindowID);
  // window_list may be NULL if it's running outside of GUI (like SSH).
  if (window_list == NULL) {
    return "";
  }
  for (int i = 0; i < CFArrayGetCount(window_list); ++i) {
    CFDictionaryRef window_data = static_cast<CFDictionaryRef>(
        CFArrayGetValueAtIndex(window_list, i));
    CFStringRef window_name = static_cast<CFStringRef>(
        CFDictionaryGetValue(window_data, kCGWindowName));
    if (window_name == NULL) {
      continue;
    }

    // CFStringGetLength returns the number of UTF-16 code points,
    // so the number of bytes in UTF-8 would be 3x larger at most.
    CFIndex buffer_size = CFStringGetLength(window_name) * 3 + 1;
    scoped_array<char> buffer(new char[buffer_size]);
    if (!CFStringGetCString(
            window_name, buffer.get(), buffer_size, kCFStringEncodingUTF8)) {
      continue;
    }

    const string &auth_code =
        sync::OAuth2TokenUtil::ParseAuthCodeFromWindowTitleForMac(buffer.get());
    if (!auth_code.empty()) {
      return auth_code;
    }
  }

  return "";
}
#endif  // OS_WIN | OS_MACOSX

}  // namespace

AuthCodeDetector::AuthCodeDetector() {
}

void AuthCodeDetector::startFetchingAuthCode() {
  // when finished, emit setAuthCode(code) to tell the result back to
  // the dialog.
#if defined(OS_WIN)
  const string &auth_code = GetAuthCodeForWindows();
  if (!auth_code.empty()) {
    emit setAuthCode(auth_code.c_str());
  }
#elif defined(OS_MACOSX)
  const string &auth_code = GetAuthCodeForMac();
  if (!auth_code.empty()) {
    emit setAuthCode(auth_code.c_str());
  }
#endif  // OS_WIN | OS_MACOSX
}

}  // namespace mozc::gui
}  // namespace mozc
