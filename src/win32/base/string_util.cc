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

#include "win32/base/string_util.h"

#include "base/base.h"
#include "base/util.h"
#include "session/commands.pb.h"

namespace mozc {
namespace win32 {

wstring StringUtil::KeyToReading(const string &key) {
  string katakana;
  Util::HiraganaToKatakana(key.c_str(), &katakana);

  DWORD lcid = MAKELCID(MAKELANGID(LANG_JAPANESE, SUBLANG_DEFAULT),
                        SORT_JAPANESE_XJIS);
  string sjis;
  mozc::Util::UTF8ToSJIS(katakana.c_str(), &sjis);

  // Convert "\x81\x65" (backquote in SJIFT-JIS) to ` by myself since
  // LCMapStringA converts it to ' for some reason.
  string sjis2;
  mozc::Util::StringReplace(sjis, "\x81\x65", "`", true, &sjis2);

  char hankaku[512];
  size_t len = LCMapStringA(lcid, LCMAP_HALFWIDTH, sjis2.c_str(), -1, hankaku,
                            sizeof(hankaku));
  if (len == 0) {
    return L"";
  }

  const UINT cp_sjis = 932;  // ANSI/OEM - Japanese, Shift-JIS
  const int output_length = MultiByteToWideChar(cp_sjis, 0, hankaku, -1, NULL,
                                                0);
  if (output_length == 0) {
    return L"";
  }

  scoped_array<wchar_t> input_wide(new wchar_t[output_length + 1]);
  const int result = MultiByteToWideChar(cp_sjis, 0, hankaku, -1,
                                         input_wide.get(), output_length + 1);
  if (result == 0) {
    return L"";
  }
  return wstring(input_wide.get());
}


string StringUtil::KeyToReadingA(const string &key) {
  string ret;
  mozc::Util::WideToUTF8(KeyToReading(key).c_str(), &ret);
  return ret;
}


wstring StringUtil::ComposePreeditText(const commands::Preedit &preedit) {
  wstring value;
  for (int i = 0; i < preedit.segment_size(); ++i) {
    wstring segment_value;
    mozc::Util::UTF8ToWide(preedit.segment(i).value(), &segment_value);
    value.append(segment_value);
  }
  return value;
}

}  // namespace win32
}  // namespace mozc
