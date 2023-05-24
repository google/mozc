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
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"

namespace mozc::japanese::internal {
namespace {

using ::mozc::strings::OneCharLen;

struct LookupResult {
  int seekto;
  int index;
};

LookupResult LookupDoubleArray(const DoubleArray *array,
                               const absl::string_view key) {
  int n = 0;
  int b = array[0].base;
  uint32_t p = 0;
  uint32_t num = 0;
  LookupResult result = {0};

  for (size_t i = 0; i < key.size(); ++i) {
    p = b;
    n = array[p].base;
    if (static_cast<uint32_t>(b) == array[p].check && n < 0) {
      result.seekto = i;
      result.index = -n - 1;
      ++num;
    }
    p = b + static_cast<uint8_t>(key[i]) + 1;
    if (static_cast<uint32_t>(b) == array[p].check) {
      b = array[p].base;
    } else {
      return result;
    }
  }
  p = b;
  n = array[p].base;
  if (static_cast<uint32_t>(b) == array[p].check && n < 0) {
    result.seekto = key.size();
    result.index = -n - 1;
  }

  return result;
}

inline int AdvanceInputBy(const char *ctable, const LookupResult result,
                          const int len) {
  return result.seekto - ctable[result.index + len + 1];
}

}  // namespace

std::string ConvertUsingDoubleArray(const DoubleArray *da, const char *ctable,
                                    const absl::string_view input) {
  int mblen = 0;
  std::string output;
  for (size_t i = 0; i < input.size(); i += mblen) {
    const LookupResult result = LookupDoubleArray(da, input.substr(i));
    if (result.seekto > 0) {
      // Each entry in ctable consists of:
      // - null-terminated string
      // - one byte offset to rewind the input
      const absl::string_view s(ctable + result.index);
      absl::StrAppend(&output, s);
      mblen = AdvanceInputBy(ctable, result, s.size());
    } else {
      // Not found in the table. Copy from input.
      mblen = OneCharLen(input[i]);
      absl::StrAppend(&output, input.substr(i, mblen));
    }
  }
  return output;
}

}  // namespace mozc::japanese::internal
