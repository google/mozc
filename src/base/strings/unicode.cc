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

#include "base/strings/unicode.h"

#include <string>
#include <string_view>

#include "base/strings/internal/utf8_internal.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace strings {

bool IsValidUtf8(const absl::string_view sv) {
  const char *const last = sv.data() + sv.size();
  for (const char *ptr = sv.data(); ptr != last;) {
    const utf8_internal::DecodeResult dr = utf8_internal::Decode(ptr, last);
    if (!dr.ok()) {
      return false;
    }
    ptr += dr.bytes_seen();
  }
  return true;
}

std::u32string Utf8ToUtf32(const absl::string_view sv) {
  const Utf8AsChars32 c32s{sv};
  // Most strings in Mozc are fairly short, so it's faster to depend on
  // automatic growth rather than calling reserve(CharsLen()).
  return std::u32string(c32s.begin(), c32s.end());
}

std::string Utf32ToUtf8(const std::u32string_view sv) {
  std::string result;
  // Same, most strings are fairly short, so it's faster to just append.
  for (auto it = sv.begin(); it != sv.end(); ++it) {
    StrAppendChar32(&result, *it);
  }
  return result;
}

absl::string_view Utf8Substring(absl::string_view sv, size_t pos) {
  while (pos > 0) {
    sv.remove_prefix(OneCharLen(sv.front()));
    --pos;
  }
  return sv;
}

absl::string_view Utf8Substring(absl::string_view sv, const size_t pos,
                                size_t count) {
  sv = Utf8Substring(sv, pos);
  size_t i = 0;
  while (i < sv.size() && count > 0) {
    i += OneCharLen(sv[i]);
    --count;
  }
  return sv.substr(0, i);
}

}  // namespace strings
}  // namespace mozc
