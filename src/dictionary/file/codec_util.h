// Copyright 2010-2020, Google Inc.
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

#ifndef MOZC_DICTIONARY_FILE_CODEC_UTIL_H_
#define MOZC_DICTIONARY_FILE_CODEC_UTIL_H_

#include <iosfwd>

#include "base/port.h"

namespace mozc {
namespace dictionary {
namespace filecodec_util {

// Writes a raw memory representation of 32-bit integer to a stream.  Therefore,
// the written byte sequence depends on the byte order of the architecture on
// which the code is executed.
void WriteInt32(int32 value, std::ostream *ofs);

// Reads an int32 value written by the above WriteInt32() from the memory block
// starting at |ptr|.  Therefore, 1) |ptr| must be aligned at 32-bit boundary,
// and 2) at least 4 bytes are accessible memory region.  After the value is
// read, |ptr| is advanced by sizeof(int32) == 4 bytes.
int32 ReadInt32ThenAdvance(const char **ptr);

// Rounds up |length| to the least upper bound of multiple of 4.  E.g.,
// RoundUp4(30) == 32.
int32 RoundUp4(int32 length);

// Given a stream for which |length| bytes were already written, adds a
// necessary padding byte(s) so that next writing starts at 4-byte boundary.
void Pad4(int length, std::ostream *ofs);

}  // namespace filecodec_util
}  // namespace dictionary
}  // namespace mozc

#endif  // MOZC_DICTIONARY_FILE_CODEC_UTIL_H_
