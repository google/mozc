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

#include "win32/base/string_util.h"

#include <memory>
#include <string>

#include "absl/strings/str_replace.h"
#include "absl/strings/string_view.h"
#include "base/japanese_util.h"
#include "base/win32/wide_char.h"
#include "protocol/commands.pb.h"

namespace mozc {
namespace win32 {
namespace {

constexpr size_t kMaxReadingChars = 512;

void Utf8ToSjis(absl::string_view input, std::string *output) {
  const std::wstring utf16 = Utf8ToWide(input);
  if (utf16.empty()) {
    output->clear();
    return;
  }

  constexpr int kCodePageShiftJIS = 932;

  const int output_length_without_null =
      ::WideCharToMultiByte(kCodePageShiftJIS, 0, utf16.data(), utf16.size(),
                            nullptr, 0, nullptr, nullptr);
  if (output_length_without_null == 0) {
    output->clear();
    return;
  }

  std::unique_ptr<char[]> sjis(new char[output_length_without_null]);
  const int actual_output_length_without_null = ::WideCharToMultiByte(
      kCodePageShiftJIS, 0, utf16.data(), utf16.size(), sjis.get(),
      output_length_without_null, nullptr, nullptr);
  if (output_length_without_null != actual_output_length_without_null) {
    output->clear();
    return;
  }

  output->assign(sjis.get(), actual_output_length_without_null);
}

}  // namespace

std::wstring StringUtil::KeyToReading(absl::string_view key) {
  std::string katakana;
  japanese_util::HiraganaToKatakana(key, &katakana);

  DWORD lcid =
      MAKELCID(MAKELANGID(LANG_JAPANESE, SUBLANG_DEFAULT), SORT_JAPANESE_XJIS);
  std::string sjis;
  Utf8ToSjis(katakana, &sjis);

  // Convert "\x81\x65" (backquote in SJIFT-JIS) to ` by myself since
  // LCMapStringA converts it to ' for some reason.
  absl::StrReplaceAll({{"\x81\x65", "`"}}, &sjis);

  const size_t halfwidth_len_without_null = ::LCMapStringA(
      lcid, LCMAP_HALFWIDTH, sjis.c_str(), sjis.size(), nullptr, 0);
  if (halfwidth_len_without_null == 0) {
    return L"";
  }

  if (halfwidth_len_without_null >= kMaxReadingChars) {
    return L"";
  }

  std::unique_ptr<char[]> halfwidth_chars(new char[halfwidth_len_without_null]);
  const size_t actual_halfwidth_len_without_null =
      ::LCMapStringA(lcid, LCMAP_HALFWIDTH, sjis.c_str(), sjis.size(),
                     halfwidth_chars.get(), halfwidth_len_without_null);
  if (halfwidth_len_without_null != actual_halfwidth_len_without_null) {
    return L"";
  }
  const UINT cp_sjis = 932;  // ANSI/OEM - Japanese, Shift-JIS
  const int output_length_without_null =
      ::MultiByteToWideChar(cp_sjis, 0, halfwidth_chars.get(),
                            actual_halfwidth_len_without_null, nullptr, 0);
  if (output_length_without_null == 0) {
    return L"";
  }

  std::unique_ptr<wchar_t[]> wide_output(
      new wchar_t[output_length_without_null]);
  const int actual_output_length_without_null = ::MultiByteToWideChar(
      cp_sjis, 0, halfwidth_chars.get(), actual_halfwidth_len_without_null,
      wide_output.get(), output_length_without_null);
  if (output_length_without_null != actual_output_length_without_null) {
    return L"";
  }
  return std::wstring(wide_output.get(), actual_output_length_without_null);
}

std::string StringUtil::KeyToReadingA(absl::string_view key) {
  return WideToUtf8(KeyToReading(key));
}

std::wstring StringUtil::ComposePreeditText(const commands::Preedit &preedit) {
  std::wstring value;
  for (int i = 0; i < preedit.segment_size(); ++i) {
    const std::wstring segment_value = Utf8ToWide(preedit.segment(i).value());
    value.append(segment_value);
  }
  return value;
}

}  // namespace win32
}  // namespace mozc
