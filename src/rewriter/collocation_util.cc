// Copyright 2010, Google Inc.
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

#include "rewriter/collocation_util.h"

#include "base/util.h"

namespace mozc {
void CollocationUtil::GetNormalizedScript(const string &str, string *output) {
  output->clear();
  string temp;
  size_t pos = 0;
  const char *begin = str.data();
  const char *end = str.data() + str.size();
  size_t mblen = 0;
  while (begin < end) {
    const uint16 w = Util::UTF8ToUCS2(begin, end, &mblen);
    if (((Util::GetScriptType(w) != Util::UNKNOWN_SCRIPT) && !IsNumber(w)) ||
        w == 0x3005 ||  // "々"
        w == 0x0025 || w == 0xFF05 ||  // "%", "％"
        w == 0x3006 ||  // "〆"
        w == 0x301C || w == 0xFF5E) {  // "〜", "～"
      temp += str.substr(pos, mblen);
    }

    begin += mblen;
    pos += mblen;
  }

  string temp2;
  // "％" -> "%"
  Util::StringReplace(temp, "\xef\xbc\x85", "%", true, &temp2);
  // "～" -> "〜"
  Util::StringReplace(temp2, "\xef\xbd\x9e", "\xe3\x80\x9c", true, output);
}

bool CollocationUtil::IsNumber(uint16 wchar) {
  if (Util::GetScriptType(wchar) == Util::NUMBER) {
    return true;
  }

  switch (wchar) {
    case 0x3007:  // "〇"
    case 0x4e00:  // "一"
    case 0x4e8c:  // "二"
    case 0x4e09:  // "三"
    case 0x56db:  // "四"
    case 0x4e94:  // "五"
    case 0x516d:  // "六"
    case 0x4e03:  // "七"
    case 0x516b:  // "八"
    case 0x4e5d:  // "九"
    case 0x5341:  // "十"
    case 0x767e:  // "百"
    case 0x5343:  // "千"
    case 0x4e07:  // "万"
    case 0x5104:  // "億"
    case 0x5146:  // "兆"
      return true;
    default:
      break;
  }
  return false;
}
}  // namespace mozc
