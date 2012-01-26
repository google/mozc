// Copyright 2010-2012, Google Inc.
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

#ifdef OS_WINDOWS
#include <windows.h>
#endif
#include <algorithm>
#include <string>

#include "base/base.h"
#include "base/util.h"
#include "base/version.h"

namespace mozc {

bool UpdateUtil::WriteActiveUsageInfo() {
#ifndef GOOGLE_JAPANESE_INPUT_BUILD
  return false;
#endif  // !GOOGLE_JAPANESE_INPUT_BUILD

#ifdef OS_WINDOWS
  const wchar_t kOmahaUsageKey[] = L"Software\\Google\\Update\\ClientState\\"
                                   L"{DDCCD2A9-025E-4142-BCEB-F467B88CF830}";
  HKEY key;
  LONG result = RegCreateKeyExW(HKEY_CURRENT_USER,
                                kOmahaUsageKey,
                                0,
                                NULL,
                                0,
                                KEY_WRITE,
                                NULL,
                                &key,
                                NULL);
  if (ERROR_SUCCESS != result) {
    return false;
  }
  const wchar_t kDidRunName[] = L"dr";
  const wchar_t kDidRunValue[] = L"1";
  result = RegSetValueExW(key,
                          kDidRunName,
                          0,
                          REG_SZ,
                          reinterpret_cast<const BYTE *>(kDidRunValue),
                          sizeof(kDidRunValue));
  RegCloseKey(key);
  return ERROR_SUCCESS == result;
#else
  // TODO(mazda): Implement Mac version
  return true;
#endif
}

string UpdateUtil::GetAvailableVersion() {
#ifndef GOOGLE_JAPANESE_INPUT_BUILD
  return "";
#endif  // !GOOGLE_JAPANESE_INPUT_BUILD

#ifdef OS_WINDOWS
  const wchar_t kOmahaClientsKey[] = L"Software\\Google\\Update\\Clients\\"
                                     L"{DDCCD2A9-025E-4142-BCEB-F467B88CF830}";
  HKEY key;
  LONG result = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                              kOmahaClientsKey,
                              0,
                              KEY_QUERY_VALUE,
                              &key);
  if (ERROR_SUCCESS != result) {
    return "";
  }
  const wchar_t kProductVersionName[] = L"pv";
  DWORD reg_type;
  BYTE buf[512];
  DWORD buf_size = 512;
  result = RegQueryValueEx(key,
                           kProductVersionName,
                           NULL,
                           &reg_type,
                           buf,
                           &buf_size);
  RegCloseKey(key);
  if (ERROR_SUCCESS != result || reg_type != REG_SZ) {
    return "";
  }

  wstring tmp(reinterpret_cast<wchar_t*>(buf), buf_size / sizeof(wchar_t));
  string ret;
  Util::WideToUTF8(tmp.c_str(), &ret);
  return ret;
#else
  // TODO(mazda): Implement Mac version
  return "Unknown";
#endif
}

string UpdateUtil::GetCurrentVersion() {
  return Version::GetMozcVersion();
}

bool UpdateUtil::CompareVersion(const string &lhs, const string &rhs) {
  return Version::CompareVersion(lhs, rhs);
}

bool UpdateUtil::IsNewVersionAvailable() {
#ifndef GOOGLE_JAPANESE_INPUT_BUILD
  return false;
#endif  // !GOOGLE_JAPANESE_INPUT_BUILD

#ifdef OS_WINDOWS
  const string available_version = GetAvailableVersion();
  if (available_version.empty()) {
    return false;
  }
  return CompareVersion(GetCurrentVersion(), available_version);
#else
  // TODO(mazda): Implement Mac version
  return false;
#endif
}

}  // namespace mozc
