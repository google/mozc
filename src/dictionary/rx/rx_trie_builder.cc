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

#include "dictionary/rx/rx_trie_builder.h"

#include "base/base.h"
#include "base/file_stream.h"
#include "third_party/rx/v1_0rc2/rx.h"

namespace mozc {
namespace rx {
RxTrieBuilder::RxTrieBuilder() : rx_builder_(rx_builder_create()) {}

RxTrieBuilder::~RxTrieBuilder() {
  rx_builder_release(rx_builder_);
}

void RxTrieBuilder::AddKey(const string &key) {
  rx_builder_add(rx_builder_, key.c_str());
}

void RxTrieBuilder::Build() {
  rx_builder_build(rx_builder_);
}

int RxTrieBuilder::GetIdFromKey(const string &key) const {
  return rx_builder_get_key_index(rx_builder_, key.c_str());
}

const char *RxTrieBuilder::GetImageBody() const {
  return reinterpret_cast<const char *>(rx_builder_get_image(rx_builder_));
}

int RxTrieBuilder::GetImageSize() const {
  return rx_builder_get_size(rx_builder_);
}

void RxTrieBuilder::WriteImage(OutputFileStream *ofs) const {
  DCHECK(ofs);
  const unsigned char *image = rx_builder_get_image(rx_builder_);
  const int size = rx_builder_get_size(rx_builder_);
  ofs->write(reinterpret_cast<const char *>(image), size);
}
}  // namespace rx
}  // namespace mozc
