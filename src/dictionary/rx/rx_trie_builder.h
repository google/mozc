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

#ifndef MOZC_DICTIONARY_RX_RX_TRIE_BUILDER_H_
#define MOZC_DICTIONARY_RX_RX_TRIE_BUILDER_H_

#include <string>

#include "base/base.h"

struct rx_builder;

namespace mozc {
class OutputFileStream;
namespace rx {

class RxTrieBuilder {
 public:
  RxTrieBuilder();
  virtual ~RxTrieBuilder();

  // Add key string
  void AddKey(const string &key);

  // Build trie
  void Build();

  // Get id from key string.
  // Return -1 if key is not found or rx trie is not built yet.
  int GetIdFromKey(const string &key) const;

  // Returns a byte array of the image.
  // The instance owns the returned object.
  const char *GetImageBody() const;

  // Returns the size of the image.
  int GetImageSize() const;

  // Write image of trie
  void WriteImage(OutputFileStream *ofs) const;

 private:
  struct rx_builder *rx_builder_;

  DISALLOW_COPY_AND_ASSIGN(RxTrieBuilder);
};

}  // namespace rx
}  // namespace mozc

#endif  // MOZC_DICTIONARY_RX_RX_TRIE_BUILDER_H_
