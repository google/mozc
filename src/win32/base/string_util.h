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

// Windows specific string utility functions.

#ifndef MOZC_WIN32_BASE_STRING_UTIL_H_
#define MOZC_WIN32_BASE_STRING_UTIL_H_

#include <windows.h>

#include <string>

#include "absl/strings/string_view.h"

namespace mozc {
namespace commands {
class Preedit;
}  // namespace commands

namespace win32 {

class StringUtil {
 public:
  StringUtil() = delete;
  ~StringUtil() = delete;

  // Converts |key| to a reading string used for a value of GUID_PROP_READING.
  // http://msdn.microsoft.com/en-us/library/ms629017(VS.85).aspx
  // This function only supports conversion of Japanese characters (characters
  // covered by code page 932).
  static std::wstring KeyToReading(absl::string_view key);

  // Returns a UTF8 string converted from the result of KeyToReading.
  // This function is mainly for unittest.
  static std::string KeyToReadingA(absl::string_view key);

  // Joins all segment strings in |preedit| and returns it.
  static std::wstring ComposePreeditText(
      const mozc::commands::Preedit &preedit);
};

}  // namespace win32
}  // namespace mozc

#endif  // MOZC_WIN32_BASE_STRING_UTIL_H_
