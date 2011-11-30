// Copyright 2010-2011, Google Inc.
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

#if defined(OS_WINDOWS)
#include "base/util.h"
#endif

#include "sync/oauth2_token_util.h"

namespace mozc {
namespace gui {
namespace {

#if defined(OS_WINDOWS)
BOOL CALLBACK EnumWindowsProc(HWND window_handle, LPARAM lparam) {
  wchar_t buffer[512] = {};
  const int copied_len_without_null =
      ::GetWindowText(window_handle, buffer, ARRAYSIZE(buffer));
  if (copied_len_without_null == 0) {
    // Failed. Go to next window.
    return TRUE;
  }

  if (copied_len_without_null > ARRAYSIZE(buffer)) {
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
#endif  // OS_WINDOWS

}  // namespace

AuthCodeDetector::AuthCodeDetector() {
}

void AuthCodeDetector::startFetchingAuthCode() {
  // TODO(mukai): write platform dependent code here.
  // when finished, emit setAuthCode(code) to tell the result back to
  // the dialog.
#if defined(OS_WINDOWS)
  const string &auth_code = GetAuthCodeForWindows();
  if (!auth_code.empty()) {
    emit setAuthCode(auth_code.c_str());
  }
#endif  // OS_WINDOWS
}

}  // namespace mozc::gui
}  // namespace mozc
