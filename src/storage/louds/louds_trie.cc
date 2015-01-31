// Copyright 2010-2015, Google Inc.
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

#include "base/logging.h"
#include "base/port.h"
#include "storage/louds/louds.h"
#include "storage/louds/simple_succinct_bit_vector_index.h"

namespace mozc {
namespace storage {
namespace louds {

namespace {
inline int32 ReadInt32(const uint8 *data) {
  // TODO(noriyukit): static assertion for the endian.
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
  const int louds_size = ReadInt32(image);
  const int terminal_size = ReadInt32(image + 4);
  const int num_character_bits = ReadInt32(image + 8);
  const int edge_character_size = ReadInt32(image + 12);
  CHECK_EQ(num_character_bits, 8);
  CHECK_GT(edge_character_size, 0);

  const uint8 *louds_image = image + 16;
  const uint8 *terminal_image = louds_image + louds_size;
  const uint8 *edge_character = terminal_image + terminal_size;

  louds_.Init(louds_image, louds_size);
  terminal_bit_vector_.Init(terminal_image, terminal_size);
  edge_character_ = reinterpret_cast<const char*>(edge_character);

  return true;
}

void LoudsTrie::Close() {
  louds_.Reset();
  terminal_bit_vector_.Reset();
  edge_character_ = nullptr;
}

bool LoudsTrie::MoveToChildByLabel(char label, Node *node) const {
  MoveToFirstChild(node);
  while (IsValidNode(*node)) {
    if (GetEdgeLabelToParentNode(*node) == label) {
      return true;
    }
    MoveToNextSibling(node);
  }
  return false;
}

bool LoudsTrie::Traverse(StringPiece key, Node *node) const {
  for (auto iter = key.begin(); iter != key.end(); ++iter) {
    if (!MoveToChildByLabel(*iter, node)) {
      return false;
    }
  }
  return true;
}

int LoudsTrie::ExactSearch(StringPiece key) const {
  Node node;  // Root
  if (Traverse(key, &node) && IsTerminalNode(node)) {
    return GetKeyIdOfTerminalNode(node);
  }
  return -1;
}

namespace {

// Traverses the subtree rooted at |node| in DFS order and runs |callback| at
// each terminal node.  Returns true if traversal should be stopped.
bool TraverseSubTree(
    const LoudsTrie &trie,
    const KeyExpansionTable &key_expansion_table,
    LoudsTrie::Callback *callback,
    LoudsTrie::Node node,
    char *key_buffer,
    StringPiece::size_type key_len) {
  if (trie.IsTerminalNode(node)) {
    const LoudsTrie::Callback::ResultType result =
        callback->Run(key_buffer, key_len, trie.GetKeyIdOfTerminalNode(node));
    switch (result) {
      case LoudsTrie::Callback::SEARCH_DONE:
        return true;
      case LoudsTrie::Callback::SEARCH_CULL:
        return false;
      default:
        break;
    }
  }

  for (trie.MoveToFirstChild(&node); trie.IsValidNode(node);
       trie.MoveToNextSibling(&node)) {
    key_buffer[key_len] = trie.GetEdgeLabelToParentNode(node);
    if (TraverseSubTree(trie, key_expansion_table, callback, node,
                        key_buffer, key_len + 1)) {
      return true;
    }
  }
  return false;
}

// Recursively traverses the trie in DFS order until terminal nodes for each
// expanded key is found.  Then TraverseSubTree() is called at each terminal
// node.  Returns true if traversal should be stopped.
bool PredictiveSearchWithKeyExpansionImpl(
    const LoudsTrie &trie,
    StringPiece key,
    const KeyExpansionTable &key_expansion_table,
    LoudsTrie::Callback *callback,
    LoudsTrie::Node node,
    char *key_buffer,
    StringPiece::size_type key_len) {
  if (key_len == key.size()) {
    return TraverseSubTree(trie, key_expansion_table, callback, node,
                           key_buffer, key_len);
  }

  const char target_char = key[key_len];
  const ExpandedKey &chars = key_expansion_table.ExpandKey(target_char);
  for (trie.MoveToFirstChild(&node); trie.IsValidNode(node);
       trie.MoveToNextSibling(&node)) {
    const char c = trie.GetEdgeLabelToParentNode(node);
    if (chars.IsHit(c)) {
      key_buffer[key_len] = c;
      if (PredictiveSearchWithKeyExpansionImpl(trie, key, key_expansion_table,
                                               callback, node,
                                               key_buffer, key_len + 1)) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace

void LoudsTrie::PredictiveSearchWithKeyExpansion(
    StringPiece key, const KeyExpansionTable &key_expansion_table,
    Callback *callback) const {
  char key_buffer[kMaxDepth + 1];
  PredictiveSearchWithKeyExpansionImpl(
      *this, key, key_expansion_table, callback,
      Node(),  // Root
      key_buffer, 0);
}

StringPiece LoudsTrie::RestoreKeyString(int key_id, char *buf) const {
  // TODO(noriyukit): Check if it's really necessary to handle this case.
  if (key_id < 0) {
    return StringPiece();
  }

  // Ensure the returned StringPiece is null-terminated.
  char *const buf_end = buf + kMaxDepth;
  *buf_end = '\0';

  // Climb up the trie to the root and fill |buf| backward.
  char *ptr = buf_end;
  Node node;
  GetTerminalNodeFromKeyId(key_id, &node);
  for (; !louds_.IsRoot(node); louds_.MoveToParent(&node)) {
    *--ptr = GetEdgeLabelToParentNode(node);
  }
  return StringPiece(ptr, buf_end - ptr);
}

}  // namespace louds
}  // namespace storage
}  // namespace mozc
