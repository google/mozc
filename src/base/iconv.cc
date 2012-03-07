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

#ifndef OS_WINDOWS
#include <iconv.h>
#else
#include <windows.h>
#endif

#include <string>
#include "base/base.h"
#include "base/util.h"

namespace {

#ifndef OS_WINDOWS
bool IconvHelper(iconv_t ic, const string &input, string *output) {
  size_t ilen = input.size();
  size_t olen = ilen * 4;
  string tmp;
  tmp.reserve(olen);
  char *ibuf = const_cast<char *>(input.data());
  char *obuf_org = const_cast<char *>(tmp.data());
  char *obuf = obuf_org;
  fill(obuf, obuf + olen, 0);
  size_t olen_org = olen;
  iconv(ic, 0, &ilen, 0, &olen);  // reset iconv state
  while (ilen != 0) {
    if (iconv(ic, reinterpret_cast<char **>(&ibuf), &ilen, &obuf, &olen)
        == static_cast<size_t>(-1)) {
      return false;
    }
  }
  output->assign(obuf_org, olen_org - olen);
  return true;
}

inline bool Convert(const char *from, const char *to,
                    const string &input, string *output) {
  iconv_t ic = iconv_open(to, from);   // note the order
  if (ic == reinterpret_cast<iconv_t>(-1)) {
    LOG(WARNING) << "iconv_open failed";
    *output = input;
    return false;
  }
  bool result = IconvHelper(ic, input, output);
  iconv_close(ic);
  return result;
}
#else
// Returns the code-page identifier for the specified encoding string.
// This function scans a list of mappings from an encoding name to a
// code-page identifier of Windows according to the encoding name.
// If the given encoding string does not have any matching code-page
// identifiers, this function returns 0.
// To add a mapping from an encoding name to its code-page identifier:
// 1. Read the list of code-page identifiers supported by Windows (*1), and;
// 2. Find a code-page identifier matching to the encoding name:
// (*1) "http://msdn.microsoft.com/en-us/library/ms776446(VS.85).aspx".
static int GetCodepage(const char* name) {
  static const struct {
    const char* name;
    int codepage;
  } kCodePageMap[] = {
    { "UTF8",         CP_UTF8 },  // Unicode UTF-8
    { "UTF-8",        CP_UTF8 },  // Unicode UTF-8
    { "SJIS",         932     },  // ANSI/OEM - Japanese, Shift-JIS
    { "EUC-JP-MS",    51932   },  // EUC - Japanese
    { "JIS",          20932   },  // JIS X 0208-1990 & 0212-1990
    { "ISO8859-1",    28591   },  // ISO8859-1
    { "ISO8859-2",    28592   },  // ISO8859-2
    { "ISO8859-3",    28593   },  // ISO8859-3
    { "ISO8859-4",    28594   },  // ISO8859-4
    { "ISO8859-5",    28595   },  // ISO8859-5
    { "ISO8859-6",    28596   },  // ISO8859-6
    { "ISO8859-7",    28597   },  // ISO8859-7
    { "ISO8859-8",    28598   },  // ISO8859-8
    { "ISO8859-9",    28599   },  // ISO8859-9
    { "ISO8859-13",   28603   },  // ISO8859-13
    { "ISO8859-15",   28605   },  // ISO8859-15
    { "KOI8-R",       20866   },  // KOI8-R
    { "windows-1251", 1251    },  // windows-1251
  };

  for (size_t i = 0; i < arraysize(kCodePageMap); i++) {
    if (strcmp(kCodePageMap[i].name, name) == 0) {
      return kCodePageMap[i].codepage;
    }
  }
  return 0;
}

// Converts the encoding of the specified string.
// This function firstly converts the source string to create a temporary
// UTF-16 string, and encodes the UTF-16 string with the destination encoding.
inline bool Convert(const char *from, const char *to,
                    const string &input, string *output) {
  const int codepage_from = GetCodepage(from);
  const int codepage_to = GetCodepage(to);
  if (codepage_from == 0 || codepage_to == 0) {
    return false;
  }

  const int wide_length = MultiByteToWideChar(codepage_from, 0, input.c_str(),
                                              -1, NULL, 0);
  if (wide_length == 0) {
    return false;
  }

  scoped_array<wchar_t> wide(new wchar_t[wide_length + 1]);
  if (wide.get() == NULL) {
    return false;
  }

  if (MultiByteToWideChar(codepage_from, 0, input.c_str(), -1,
                          wide.get(), wide_length + 1) == 0)
    return false;

  const int output_length = WideCharToMultiByte(codepage_to, 0, wide.get(), -1,
                                                NULL, 0, NULL, NULL);
  if (output_length == 0) {
    return false;
  }

  scoped_array<char> multibyte(new char[output_length + 1]);
  if (multibyte.get() == NULL) {
    return false;
  }

  const int result = WideCharToMultiByte(codepage_to, 0, wide.get(),
                                         wide_length, multibyte.get(),
                                         output_length + 1, NULL, NULL);
  if (result == 0) {
    return false;
  }

  output->assign(multibyte.get());
  return true;
}

#endif
}   // namespace

namespace mozc {

#ifndef OS_WINDOWS
// The following functions don't work on Windows
void Util::UTF8ToEUC(const string &input, string *output) {
  Convert("UTF8", "EUC-JP-MS", input, output);
}

void Util::EUCToUTF8(const string &input, string *output) {
  Convert("EUC-JP-MS", "UTF8", input, output);
}
#endif

void Util::UTF8ToSJIS(const string &input, string *output) {
  Convert("UTF8", "SJIS", input, output);
}

void Util::SJISToUTF8(const string &input, string *output) {
  Convert("SJIS", "UTF8", input, output);
}

bool Util::ToUTF8(const char *from, const string &input, string *output) {
  return Convert(from, "UTF8", input, output);
}
}  // namespace mozc
