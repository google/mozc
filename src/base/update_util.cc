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

#include "base/update_util.h"

#ifdef _WIN32
#include <windows.h>
#endif  // _WIN32

namespace mozc {

bool UpdateUtil::WriteActiveUsageInfo() {
#ifndef GOOGLE_JAPANESE_INPUT_BUILD
  return false;
#endif  // !GOOGLE_JAPANESE_INPUT_BUILD

#ifdef _WIN32
  const wchar_t kOmahaUsageKey[] =
      L"Software\\Google\\Update\\ClientState\\"
      L"{DDCCD2A9-025E-4142-BCEB-F467B88CF830}";
  HKEY key;
  LONG result = RegCreateKeyExW(HKEY_CURRENT_USER, kOmahaUsageKey, 0, nullptr,
                                0, KEY_WRITE, nullptr, &key, nullptr);
  if (ERROR_SUCCESS != result) {
    return false;
  }
  const wchar_t kDidRunName[] = L"dr";
  const wchar_t kDidRunValue[] = L"1";
  result = RegSetValueExW(key, kDidRunName, 0, REG_SZ,
                          reinterpret_cast<const BYTE*>(kDidRunValue),
                          sizeof(kDidRunValue));
  RegCloseKey(key);
  return ERROR_SUCCESS == result;
#else   // _WIN32
  // TODO(mazda): Implement Mac version
  return false;
#endif  // _WIN32
}

}  // namespace mozc
