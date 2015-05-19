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

#include "storage/louds/louds_trie.h"

#include <cstring>

#include "base/base.h"
#include "base/logging.h"
#include "storage/louds/louds.h"
#include "storage/louds/simple_succinct_bit_vector_index.h"

namespace mozc {
namespace storage {
namespace louds {

namespace {
inline int32 ReadInt32(const uint8 *data) {
  // TODO(hidehiko): static assertion for the endian.
  return *reinterpret_cast<const int32*>(data);
}
}  // namespace

bool LoudsTrie::Open(const uint8 *image) {
  // Reads a binary image data, which is compatible with rx.
  // The format is as follows:
  // [trie size: little endian 4byte int]
  // [terminal size: little endian 4byte int]
  // [num bits for each character annotated to an edge:
  //  little endian 4 byte int. Currently, this class supports only 8-bits]
  // [edge character image size: little endian 4 byte int]
  // [trie image: "trie size" bytes]
  // [terminal image: "terminal size" bytes]
  // [edge character image: "edge character image size" bytes]
  //
  // Here, "terminal" means "the node is one of the end of a word."
  // For example, if we have a trie for "aa" and "aaa", the trie looks like:
  //         [0]
  //        a/
  //       [1]
  //      a/
  //     [2]
  //    a/
  //   [3]
  // In this case, [0] and [1] are not terminal (as the original words contains
  // neither "" nor "a"), and [2] and [3] are terminal.
  const int trie_size = ReadInt32(image);
  const int terminal_size = ReadInt32(image + 4);
  const int num_character_bits = ReadInt32(image + 8);
  const int edge_character_size = ReadInt32(image + 12);
  CHECK_EQ(num_character_bits, 8);
  CHECK_GT(edge_character_size, 0);

  const uint8 *trie_image = image + 16;
  const uint8 *terminal_image = trie_image + trie_size;
  const uint8 *edge_character = terminal_image + terminal_size;

  trie_.Open(trie_image, trie_size);
  terminal_bit_vector_.Init(terminal_image, terminal_size);
  edge_character_ = reinterpret_cast<const char*>(edge_character);

  return true;
}

void LoudsTrie::Close() {
  trie_.Close();
  terminal_bit_vector_.Reset();
  edge_character_ = NULL;
}

namespace {

// This class is the implementation of prefix search.
class PrefixSearcher {
 public:
  PrefixSearcher(const Louds *trie,
                 const SimpleSuccinctBitVectorIndex *terminal_bit_vector,
                 const char *edge_character,
                 const KeyExpansionTable *key_expansion_table,
                 const char *key,
                 LoudsTrie::Callback *callback)
      : trie_(trie),
        terminal_bit_vector_(terminal_bit_vector),
        edge_character_(edge_character),
        key_expansion_table_(key_expansion_table),
        key_(key),
        callback_(callback) {
  }

  // Returns true if we shouldn't continue to search any more.
  bool Search(size_t key_index, size_t bit_index) {
    const char key_char = key_[key_index];
    if (key_char == '\0') {
      // The end of search key. Do nothing.
      return false;
    }

    if (!trie_->IsEdgeBit(bit_index)) {
      // This is leaf. Do nothing.
      return false;
    }

    // Then traverse the children.
    int child_node_id = trie_->GetChildNodeId(bit_index);
    const ExpandedKey &expanded_key =
        key_expansion_table_->ExpandKey(key_char);
    do {
      const char character = edge_character_[child_node_id - 1];
      if (expanded_key.IsHit(character)) {
        buffer_[key_index] = character;
        // Hit the annotated character to the key char.
        if (terminal_bit_vector_->Get(child_node_id - 1)) {
          // The child node is terminal, so invoke the callback.
          if (callback_->Run(buffer_, key_index + 1,
                             terminal_bit_vector_->Rank1(child_node_id - 1))) {
            // Terminate the search if callback returns true.
            return true;
          }
        }

        // Search to the next child.
        // Note: we use recursive callback, instead of the just simple loop
        // here, in order to support key-expansion (in future).
        const int child_index = trie_->GetFirstEdgeBitIndex(child_node_id);
        if (Search(key_index + 1, child_index)) {
          return true;
        }
      }

      // Note: Because of the representation of LOUDS, the child node id is
      // consecutive. So we don't need to invoke GetChildNodeId.
      ++bit_index;
      ++child_node_id;
    } while (trie_->IsEdgeBit(bit_index));

    return false;
  }

 private:
  const Louds *trie_;
  const SimpleSuccinctBitVectorIndex *terminal_bit_vector_;
  const char *edge_character_;
  const KeyExpansionTable *key_expansion_table_;

  const char *key_;
  char buffer_[LoudsTrie::kMaxDepth + 1];
  LoudsTrie::Callback *callback_;

  DISALLOW_COPY_AND_ASSIGN(PrefixSearcher);
};
}  // namespace

void LoudsTrie::PrefixSearchWithKeyExpansion(
    const char *key, const KeyExpansionTable &key_expansion_table,
    Callback *callback) const {
  PrefixSearcher searcher(
      &trie_, &terminal_bit_vector_, edge_character_, &key_expansion_table,
      key, callback);

  // The bit index of the root node is '2'.
  searcher.Search(0, 2);
}

namespace {
class PredictiveSearcher {
 public:
  PredictiveSearcher(const Louds *trie,
                     const SimpleSuccinctBitVectorIndex *terminal_bit_vector,
                     const char *edge_character,
                     const KeyExpansionTable *key_expansion_table,
                     const char *key,
                     LoudsTrie::Callback *callback)
      : trie_(trie),
        terminal_bit_vector_(terminal_bit_vector),
        edge_character_(edge_character),
        key_expansion_table_(key_expansion_table),
        key_(key),
        callback_(callback) {
  }

  // Returns true if we shouldn't continue to search any more.
  bool Search(size_t key_index, size_t bit_index) {
    const char key_char = key_[key_index];
    if (key_char == '\0') {
      // Hit the end of the key. Start traverse.
      return Traverse(key_index, bit_index);
    }

    if (!trie_->IsEdgeBit(bit_index)) {
      // This is leaf. Do nothing.
      return false;
    }

    // Then traverse the children.
    int child_node_id = trie_->GetChildNodeId(bit_index);
    const ExpandedKey &expanded_key =
        key_expansion_table_->ExpandKey(key_char);
    do {
      const char character = edge_character_[child_node_id - 1];
      if (expanded_key.IsHit(character)) {
        buffer_[key_index] = character;
        const int child_index = trie_->GetFirstEdgeBitIndex(child_node_id);
        if (Search(key_index + 1, child_index)) {
          return true;
        }
      }

      // Note: Because of the representation of LOUDS, the child node id is
      // consecutive. So we don't need to invoke GetChildNodeId.
      ++bit_index;
      ++child_node_id;
    } while (trie_->IsEdgeBit(bit_index));

    return false;
  }

 private:
  const Louds *trie_;
  const SimpleSuccinctBitVectorIndex *terminal_bit_vector_;
  const char *edge_character_;
  const KeyExpansionTable *key_expansion_table_;

  const char *key_;
  char buffer_[LoudsTrie::kMaxDepth + 1];
  LoudsTrie::Callback *callback_;

  bool Traverse(size_t key_index, size_t bit_index) {
    const int node_id = trie_->GetParentNodeId(bit_index);
    if (terminal_bit_vector_->Get(node_id - 1)) {
      // Invoke callback, if the node is terminal.
      if (callback_->Run(buffer_, key_index,
                         terminal_bit_vector_->Rank1(node_id - 1))) {
        return true;
      }
    }

    if (!trie_->IsEdgeBit(bit_index)) {
      return false;
    }

    // Then traverse the children.
    int child_node_id = Louds::GetChildNodeId(node_id, bit_index);
    do {
      buffer_[key_index] = edge_character_[child_node_id - 1];
      // TODO(hidehiko): We can remove GetFirstEdgeBitIndex by using the result
      // of previous Traverse internal calculation.
      // Check the performance and implement it if it's effective.
      if (Traverse(
              key_index + 1, trie_->GetFirstEdgeBitIndex(child_node_id))) {
        return true;
      }
      ++bit_index;
      ++child_node_id;
    } while (trie_->IsEdgeBit(bit_index));

    return false;
  }

  DISALLOW_COPY_AND_ASSIGN(PredictiveSearcher);
};
}  // namespace

void LoudsTrie::PredictiveSearchWithKeyExpansion(
    const char *key, const KeyExpansionTable &key_expansion_table,
    Callback *callback) const {
  PredictiveSearcher searcher(
      &trie_, &terminal_bit_vector_, edge_character_, &key_expansion_table,
      key, callback);

  searcher.Search(0, 2);
}

const char *LoudsTrie::Reverse(int key_id, char *buffer) const {
  if (key_id < 0) {
    // Just for rx compatibility.
    buffer[0] = '\0';
    return buffer;
  }

  // Calculate node_id from key_id.
  int node_id = terminal_bit_vector_.Select1(key_id + 1) + 1;

  // Terminate by '\0'.
  char *ptr = buffer + kMaxDepth;
  *ptr = '\0';

  // Traverse the trie from leaf to root.
  while (node_id > 1) {
    --ptr;
    *ptr = edge_character_[node_id - 1];
    const int bit_index = trie_.GetParentEdgeBitIndex(node_id);
    node_id = trie_.GetParentNodeId(bit_index);
  }
  return ptr;
}

}  // namespace louds
}  // namespace storage
}  // namespace mozc
