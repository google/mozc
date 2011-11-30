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

#include "win32/base/omaha_util.h"

// Omaha is the code name of Google Update, which is not used in
// OSS version of Mozc.
#if !defined(GOOGLE_JAPANESE_INPUT_BUILD)
#error OmahaUtil must be used with Google Japanese Input, not OSS Mozc
#endif  // !GOOGLE_JAPANESE_INPUT_BUILD

#include <windows.h>

#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
#define _ATL_NO_HOSTING
// Workaround against KB813540
#include <atlbase_mozc.h>

#include <string>

#include "base/util.h"

namespace mozc {
namespace win32 {
namespace {
using ATL::CRegKey;

const wchar_t kClientStateKey[] = L"Software\\Google\\Update\\ClientState\\"
                                  L"{DDCCD2A9-025E-4142-BCEB-F467B88CF830}";
const wchar_t kChannelKeyName[] = L"ap";

LONG OpenClientStateKey(CRegKey *key, REGSAM base_sam) {
  const REGSAM sam_desired = base_sam |
      (Util::IsWindowsX64() ? KEY_WOW64_32KEY : 0);
  return key->Create(HKEY_LOCAL_MACHINE, kClientStateKey,
                     REG_NONE, REG_OPTION_NON_VOLATILE,
                     sam_desired);
}
}  // anonymous namespace

// Writes a REG_SZ channel name into "ap" in Mozc's client state key.
bool OmahaUtil::WriteChannel(const wstring &value) {
  CRegKey key;
  LONG result = OpenClientStateKey(&key, KEY_READ | KEY_WRITE);
  if (ERROR_SUCCESS != result) {
    return false;
  }
  result = key.SetStringValue(kChannelKeyName, value.c_str());
  if (ERROR_SUCCESS != result) {
    return false;
  }
  return true;
}

// Reads a REG_SZ channel name from "ap" in Mozc's client state key.
wstring OmahaUtil::ReadChannel() {
  CRegKey key;
  LONG result = OpenClientStateKey(&key, KEY_READ);
  if (ERROR_SUCCESS != result) {
    return L"";
  }
  wchar_t buf[512];
  ULONG buf_size = arraysize(buf);
  result = key.QueryStringValue(kChannelKeyName, buf, &buf_size);
  if (ERROR_SUCCESS != result) {
    return L"";
  }
  return wstring(buf);
}

bool OmahaUtil::ClearOmahaError() {
  CRegKey key;
  LONG result = OpenClientStateKey(&key, KEY_READ | KEY_WRITE);
  if (ERROR_SUCCESS != result) {
    return false;
  }
  result = key.SetDWORDValue(L"InstallerResult", 0);
  if (ERROR_SUCCESS != result) {
    return false;
  }
  result = key.SetStringValue(L"InstallerResultUIString", L"");
  if (ERROR_SUCCESS != result) {
    return false;
  }
  return true;
}

bool OmahaUtil::WriteOmahaError(const wstring &ui_message,
                                const wstring &header) {
  CRegKey key;
  LONG result = OpenClientStateKey(&key, KEY_READ | KEY_WRITE);
  if (ERROR_SUCCESS != result) {
    return false;
  }
  result = key.SetDWORDValue(L"InstallerResult", 1);
  if (ERROR_SUCCESS != result) {
    return false;
  }

  // Leaves Mozc version in addition to UI message for customer support.
  const wstring &message = header.length() > 0 ? header + L"\r\n" + ui_message
                                               : ui_message;

  // This message will be displayed by Omaha meta installer in the error
  // dialog.
  result = key.SetStringValue(L"InstallerResultUIString", message.c_str());
  if (ERROR_SUCCESS != result) {
    return false;
  }
  return true;
}

bool OmahaUtil::ClearChannel() {
  CRegKey key;
  LONG result = OpenClientStateKey(&key, KEY_READ | KEY_WRITE);
  if (ERROR_SUCCESS != result) {
    // The parent key is already deleted. It's OK.
    return true;
  }
  if (ReadChannel().empty()) {
    // The value does not exist. It's OK.
    return true;
  }
  result = key.DeleteValue(kChannelKeyName);
  if (ERROR_SUCCESS != result) {
    return false;
  }
  return true;
}
}  // namespace win32
}  // namespace mozc
