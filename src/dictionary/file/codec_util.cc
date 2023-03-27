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

#include "dictionary/file/codec_util.h"

#include <cstdint>
#include <cstring>
#include <ostream>

#include "base/logging.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"

namespace mozc {
namespace dictionary {
namespace filecodec_util {

void WriteInt32(int32_t value, std::ostream *ofs) {
  DCHECK(ofs);
  ofs->write(reinterpret_cast<const char *>(&value), sizeof(value));
}

int32_t ReadInt32ThenAdvance(const char **ptr) {
  DCHECK(ptr);
  DCHECK(*ptr);
  int32_t value;
  std::memcpy(&value, *ptr, sizeof(int32_t));
  *ptr += sizeof(int32_t);
  return value;
}

int RoundUp4(int length) {
  const int rem = (length % 4);
  return length + ((4 - rem) % 4);
}

void Pad4(int length, std::ostream *ofs) {
  DCHECK(ofs);
  for (int i = length; (i % 4) != 0; ++i) {
    (*ofs) << '\0';
  }
}

}  // namespace filecodec_util
}  // namespace dictionary
}  // namespace mozc
