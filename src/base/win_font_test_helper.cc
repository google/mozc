// Copyright 2010-2020, Google Inc.
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

#include "base/win_font_test_helper.h"

// skip all unless OS_WIN
#ifdef OS_WIN
#include <Windows.h>
#include <shlwapi.h>

#include "base/logging.h"
#include "base/mmap.h"
#include "base/util.h"

namespace mozc {
namespace {

HANDLE g_ipa_gothic_ = nullptr;
HANDLE g_ipa_mincho_ = nullptr;

HANDLE LoadPrivateFont(const wchar_t *font_name) {
  wchar_t w_path[MAX_PATH] = {};
  const DWORD char_size =
      ::GetModuleFileNameW(nullptr, w_path, ARRAYSIZE(w_path));
  const DWORD get_module_file_name_error = ::GetLastError();
  if (char_size == 0) {
    LOG(ERROR) << "GetModuleFileNameW failed.  error = "
               << get_module_file_name_error;
    return nullptr;
  } else if (char_size == ARRAYSIZE(w_path)) {
    LOG(ERROR) << "The result of GetModuleFileNameW was truncated.";
    return nullptr;
  }
  if (!::PathRemoveFileSpec(w_path)) {
    LOG(ERROR) << "PathRemoveFileSpec failed.";
    return nullptr;
  }
  if (!::PathAppend(w_path, font_name)) {
    LOG(ERROR) << "PathAppend failed.";
    return nullptr;
  }
  string path;
  Util::WideToUTF8(w_path, &path);

  Mmap mmap;
  if (!mmap.Open(path.c_str())) {
    LOG(ERROR) << "Mmap::Open failed.";
    return nullptr;
  }

  DWORD num_font = 0;
  const HANDLE handle =
      ::AddFontMemResourceEx(mmap.begin(), mmap.size(), nullptr, &num_font);
  if (handle == nullptr) {
    const int error = ::GetLastError();
    LOG(ERROR) << "AddFontMemResourceEx failed. error = " << error;
    return nullptr;
  }
  return handle;
}

}  // namespace

// static
bool WinFontTestHelper::Initialize() {
  if (g_ipa_gothic_ == nullptr) {
    g_ipa_gothic_ = LoadPrivateFont(L"data\\ipaexg.ttf");
  }
  if (g_ipa_mincho_ == nullptr) {
    g_ipa_mincho_ = LoadPrivateFont(L"data\\ipaexm.ttf");
  }
  if (g_ipa_gothic_ == nullptr || g_ipa_mincho_ == nullptr) {
    Uninitialize();
    return false;
  }
  return true;
}

// static
void WinFontTestHelper::Uninitialize() {
  if (g_ipa_gothic_ != nullptr) {
    ::RemoveFontMemResourceEx(g_ipa_gothic_);
    g_ipa_gothic_ = nullptr;
  }
  if (g_ipa_mincho_ != nullptr) {
    ::RemoveFontMemResourceEx(g_ipa_mincho_);
    g_ipa_mincho_ = nullptr;
  }
}

// static
string WinFontTestHelper::GetIPAexGothicFontName() {
  // "IPAexゴシック"
  return "IPAex"
         "\343\202\264\343\202\267\343\203\203\343\202\257";
}

// static
string WinFontTestHelper::GetIPAexMinchoFontName() {
  // "IPAex明朝"
  return "IPAex"
         "\346\230\216\346\234\235";
}

}  // namespace mozc

#endif  // OS_WIN
