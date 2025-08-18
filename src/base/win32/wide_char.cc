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

#include "base/win32/wide_char.h"

#include <windows.h>

#include <cstddef>
#include <initializer_list>
#include <string>
#include <string_view>  // for wstring_view

#include "absl/base/casts.h"
#include "absl/log/check.h"
#include "absl/strings/string_view.h"

namespace mozc::win32 {
namespace wide_char_internal {

void StrAppendWInternal(std::wstring* dest,
                        const std::initializer_list<std::wstring_view> slist) {
  size_t size = 0;
  for (const std::wstring_view view : slist) {
    size += view.size();
  }
  dest->reserve(size);
  for (const std::wstring_view view : slist) {
    dest->append(view);
  }
}

}  // namespace wide_char_internal

size_t WideCharsLen(const absl::string_view input) {
  // This API call should always succeed as long as the codepage (CP_UTF8) and
  // flag (0) are valid.
  const int wchar_count =
      ::MultiByteToWideChar(CP_UTF8, 0, input.data(), input.size(), nullptr, 0);
  CHECK_GE(wchar_count, 0);
  return absl::implicit_cast<size_t>(wchar_count);
}

std::wstring Utf8ToWide(const absl::string_view input) {
  if (input.empty()) {
    return std::wstring();
  }

  size_t wchar_count = WideCharsLen(input);
  std::wstring result;
  // MultiByteToWideChar doesn't null-terminate the output string when the
  // length is explicitly specified.
  result.resize(wchar_count);

  // This API call should always succeed as long as the codepage (CP_UTF8) and
  // flag (0) are valid.
  const int copied_wchars = ::MultiByteToWideChar(
      CP_UTF8, 0, input.data(), input.size(), result.data(), result.size());
  CHECK_GE(copied_wchars, 0);
  DCHECK_EQ(wchar_count, copied_wchars);
  return result;
}

std::string WideToUtf8(const std::wstring_view input) {
  if (input.empty()) {
    return std::string();
  }

  // These API calls should always succeed as long as the codepage (CP_UTF8) and
  // flag (0) are valid.
  const int char_count = ::WideCharToMultiByte(
      CP_UTF8, 0, input.data(), input.size(), nullptr, 0, nullptr, nullptr);
  if (char_count < 0) {
    return std::string();
  }

  std::string result;
  // WideCharToMultiByte doesn't null-terminate the output string when the
  // length is explicitly specified.
  result.resize(char_count);
  const int copied_chars =
      ::WideCharToMultiByte(CP_UTF8, 0, input.data(), input.size(),
                            result.data(), result.size(), nullptr, nullptr);

  CHECK_GE(copied_chars, 0);
  DCHECK_EQ(char_count, copied_chars);
  return result;
}

}  // namespace mozc::win32
