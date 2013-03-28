// Copyright 2010-2013, Google Inc.
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

#ifndef MOZC_STORAGE_LOUDS_LOUDS_TRIE_H_
#define MOZC_STORAGE_LOUDS_LOUDS_TRIE_H_

#include "base/port.h"
#include "base/string_piece.h"
#include "storage/louds/key_expansion_table.h"
#include "storage/louds/louds.h"
#include "storage/louds/simple_succinct_bit_vector_index.h"

namespace mozc {
namespace storage {
namespace louds {

// Implementation of a trie, based on the LOUDS (Level-Ordered Unary Degree
// Sequence) data structure.
// The "string" this class can handle as a key is c-style string
// (i.e. '\0'-terminated char array).
// TODO(hidehiko): Parametrize succinct bit vector implementation.
class LoudsTrie {
 public:
  // The max depth of the trie.
  static const size_t kMaxDepth = 256;

  // Interface which is called back when the result is found.
  class Callback {
   public:
    enum ResultType {
      // Finishes the current (prefix or predictive) search.
      SEARCH_DONE,

      // Continues the current (prefix or predictive) search.
      SEARCH_CONTINUE,

      // Finished the (prefix or predictive) search of the current edge,
      // but still continues to search for other edges.
      SEARCH_CULL,
    };

    virtual ~Callback() {
    }

    // The callback will be invoked when a target word is found.
    // "s" may not be null-terminated.
    virtual ResultType Run(const char *s, size_t len, int key_id) = 0;

   protected:
    Callback() {}
  };

  LoudsTrie() : edge_character_(NULL) {
  }
  ~LoudsTrie() {
  }

  // Opens the binary image, and constructs the data structure.
  // This method doesn't own the "data", so it is caller's reponsibility
  // to keep the data alive until Close is invoked.
  // See .cc file for the detailed format of the binary image.
  bool Open(const uint8 *data);

  // Destructs the internal data structure.
  void Close();

  // Searches the trie for the key that exactly matches the given key. Returns
  // -1 if the key doesn't exist.
  // TODO(noriyukit): Implement a callback style method if necessary.
  int ExactSearch(const StringPiece key) const;

  // Searches the trie structure, and invokes callback->Run when for each word
  // which is a prefix of the key is found.
  void PrefixSearch(const char *key, Callback *callback) const {
    PrefixSearchWithKeyExpansion(
        key, KeyExpansionTable::GetDefaultInstance(), callback);
  }
  void PrefixSearchWithKeyExpansion(
      const char *key, const KeyExpansionTable &key_expansion_table,
      Callback *callback) const;


  // Searches the trie structure, and invokes callback->Run when for each word
  // which begins with key is found.
  void PredictiveSearch(const char *key, Callback *callback) const {
    PredictiveSearchWithKeyExpansion(
        key, KeyExpansionTable::GetDefaultInstance(), callback);
  }
  void PredictiveSearchWithKeyExpansion(
      const char *key, const KeyExpansionTable &key_expansion_table,
      Callback *callback) const;


  // Traverses the trie from leaf to root and store the characters annotated to
  // the edges. The size of the buffer should be larger than kMaxDepth.  Returns
  // the pointer to the first character.
  const char *Reverse(int key_id, char *buf) const;

 private:
  // Tree-structure represented in LOUDS.
  Louds trie_;

  // Bit-vector to represent whether each node in LOUDS tree is terminal.
  // This bit vector doesn't include "super root" in the LOUDS.
  // In other words, id=1 in trie_ corresponds to id=0 in terminal_bit_vector_,
  // id=10 in trie_ corresponds to id=9 in terminal_bit_vector_, and so on.
  // TODO(hidehiko): Simplify the id-mapping by introducing a bit for the
  // super root in this bit vector.
  SimpleSuccinctBitVectorIndex terminal_bit_vector_;

  // A sequence of characters, annotated to each edge.
  // This array also doesn't have an entry for super root.
  // In other words, id=2 in trie_ corresponds to edge_character_[1].
  const char *edge_character_;

  DISALLOW_COPY_AND_ASSIGN(LoudsTrie);
};

}  // namespace louds
}  // namespace storage
}  // namespace mozc

#endif  // MOZC_STORAGE_LOUDS_LOUDS_TRIE_H_
