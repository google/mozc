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

#include "storage/louds/louds_trie.h"

#include <cstddef>
#include <cstdint>

#include "absl/strings/string_view.h"
#include "base/bits.h"
#include "base/logging.h"
#include "storage/louds/louds.h"
#include "storage/louds/simple_succinct_bit_vector_index.h"

namespace mozc {
namespace storage {
namespace louds {

bool LoudsTrie::Open(const uint8_t *image, size_t louds_lb0_cache_size,
                     size_t louds_lb1_cache_size,
                     size_t louds_select0_cache_size,
                     size_t louds_select1_cache_size,
                     size_t termvec_lb1_cache_size) {
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
  const int louds_size = LoadUnalignedAdvance<uint32_t>(image);
  const int terminal_size = LoadUnalignedAdvance<uint32_t>(image);
  const int num_character_bits = LoadUnalignedAdvance<uint32_t>(image);
  const int edge_character_size = LoadUnalignedAdvance<uint32_t>(image);
  CHECK_EQ(num_character_bits, 8);
  CHECK_GT(edge_character_size, 0);

  const uint8_t *louds_image = image;
  const uint8_t *terminal_image = louds_image + louds_size;
  const uint8_t *edge_character = terminal_image + terminal_size;

  louds_.Init(louds_image, louds_size, louds_lb0_cache_size,
              louds_lb1_cache_size, louds_select0_cache_size,
              louds_select1_cache_size);
  terminal_bit_vector_.Init(terminal_image, terminal_size,
                            0,  // Select0 is not carried out.
                            termvec_lb1_cache_size);
  edge_character_ = reinterpret_cast<const char *>(edge_character);

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

bool LoudsTrie::Traverse(absl::string_view key, Node *node) const {
  for (auto iter = key.begin(); iter != key.end(); ++iter) {
    if (!MoveToChildByLabel(*iter, node)) {
      return false;
    }
  }
  return true;
}

int LoudsTrie::ExactSearch(absl::string_view key) const {
  Node node;  // Root
  if (Traverse(key, &node) && IsTerminalNode(node)) {
    return GetKeyIdOfTerminalNode(node);
  }
  return -1;
}

absl::string_view LoudsTrie::RestoreKeyString(Node node, char *buf) const {
  // Ensure the returned string view is null-terminated.
  char *const buf_end = buf + kMaxDepth;
  *buf_end = '\0';

  // Climb up the trie to the root and fill |buf| backward.
  char *ptr = buf_end;
  for (; !louds_.IsRoot(node); louds_.MoveToParent(&node)) {
    *--ptr = GetEdgeLabelToParentNode(node);
  }
  return absl::string_view(ptr, buf_end - ptr);
}

}  // namespace louds
}  // namespace storage
}  // namespace mozc
