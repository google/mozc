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

#include "base/strings/internal/double_array.h"

#include <string>

#include "base/strings/unicode.h"
#include "absl/strings/string_view.h"

namespace mozc::japanese::internal {
namespace {

using ::mozc::strings::OneCharLen;

int LookupDoubleArray(const DoubleArray *array, const char *key, int len,
                      int *result) {
  int seekto = 0;
  int n = 0;
  int b = array[0].base;
  uint32_t p = 0;
  *result = -1;
  uint32_t num = 0;

  for (int i = 0; i < len; ++i) {
    p = b;
    n = array[p].base;
    if (static_cast<uint32_t>(b) == array[p].check && n < 0) {
      seekto = i;
      *result = -n - 1;
      ++num;
    }
    p = b + static_cast<uint8_t>(key[i]) + 1;
    if (static_cast<uint32_t>(b) == array[p].check) {
      b = array[p].base;
    } else {
      return seekto;
    }
  }
  p = b;
  n = array[p].base;
  if (static_cast<uint32_t>(b) == array[p].check && n < 0) {
    seekto = len;
    *result = -n - 1;
  }

  return seekto;
}

}  // namespace

void ConvertUsingDoubleArray(const DoubleArray *da, const char *ctable,
                             absl::string_view input, std::string *output) {
  output->clear();
  const char *begin = input.data();
  const char *const end = input.data() + input.size();
  while (begin < end) {
    int result = 0;
    int mblen =
        LookupDoubleArray(da, begin, static_cast<int>(end - begin), &result);
    if (mblen > 0) {
      const char *p = &ctable[result];
      const size_t len = strlen(p);
      output->append(p, len);
      mblen -= static_cast<int32_t>(p[len + 1]);
      begin += mblen;
    } else {
      mblen = OneCharLen(*begin);
      output->append(begin, mblen);
      begin += mblen;
    }
  }
}

}  // namespace mozc::japanese::internal
