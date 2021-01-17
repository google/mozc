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

#ifndef MOZC_STORAGE_LOUDS_BIT_STREAM_H_
#define MOZC_STORAGE_LOUDS_BIT_STREAM_H_

#include <string>

#include "base/port.h"

namespace mozc {
namespace storage {
namespace louds {

// Stream taking 'bits', especially designed for building a LOUDS trie.
class BitStream {
 public:
  BitStream();

  const std::string &image() const { return image_; }
  size_t num_bits() const { return num_bits_; }
  size_t ByteSize() const { return image_.length(); }

  void PushBit(int bit);

  // Fills the padding (0-bit) until the size is aligned to 32bit boundary.
  void FillPadding32();

 private:
  std::string image_;
  size_t num_bits_;

  DISALLOW_COPY_AND_ASSIGN(BitStream);
};

}  // namespace louds
}  // namespace storage
}  // namespace mozc

#endif  // MOZC_STORAGE_LOUDS_BIT_STREAM_H_
