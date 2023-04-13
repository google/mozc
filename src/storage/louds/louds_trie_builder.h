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

#ifndef MOZC_STORAGE_LOUDS_LOUDS_TRIE_BUILDER_H_
#define MOZC_STORAGE_LOUDS_LOUDS_TRIE_BUILDER_H_

#include <string>
#include <vector>

#include "base/port.h"

namespace mozc {
namespace storage {
namespace louds {

class LoudsTrieBuilder {
 public:
  LoudsTrieBuilder();

  LoudsTrieBuilder(const LoudsTrieBuilder &) = delete;
  LoudsTrieBuilder &operator=(const LoudsTrieBuilder &) = delete;

  LoudsTrieBuilder(LoudsTrieBuilder &&) = default;
  LoudsTrieBuilder &operator=(LoudsTrieBuilder &&) = default;

  ~LoudsTrieBuilder() = default;

  // Adds the word to the builder. It is necessary to call this method,
  // before Build invocation.
  void Add(const std::string &word);

  // Builds the trie image.
  void Build();

  // Returns the binary image of the trie.
  const std::string &image() const;

  // Returns the key_id for the word (-1 if not found).
  // Note: in Mozc, the key_id will be used to build additional data
  // related to the built LoudsTrie.
  int GetId(const std::string &word) const;

 private:
  bool built_;

  std::vector<std::string> word_list_;
  std::vector<int> id_list_;
  std::string image_;
};

}  // namespace louds
}  // namespace storage
}  // namespace mozc

#endif  // MOZC_STORAGE_LOUDS_LOUDS_TRIE_BUILDER_H_
