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
#include <limits>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/bits.h"
#include "storage/louds/louds.h"
#include "storage/louds/simple_succinct_bit_vector_index.h"

namespace mozc {
namespace storage {
namespace louds {
namespace {

alignas(uint32_t) constexpr uint8_t kEmptyTerminalImage[4] = {0, 0, 0, 0};

}  // namespace

bool LoudsTrie::Open(absl::Span<const uint8_t> image,
                     size_t louds_lb0_cache_size, size_t louds_lb1_cache_size,
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
  Close();
  constexpr size_t kHeaderSize = 4 * sizeof(uint32_t);
  if (image.size() < kHeaderSize) {
    return false;
  }
  const uint8_t* ptr = image.data();
  const uint32_t louds_size = LoadUnalignedAdvance<uint32_t>(ptr);
  const uint32_t terminal_size = LoadUnalignedAdvance<uint32_t>(ptr);
  const uint32_t num_character_bits = LoadUnalignedAdvance<uint32_t>(ptr);
  const uint32_t edge_character_size = LoadUnalignedAdvance<uint32_t>(ptr);

  // SimpleSuccinctBitVectorIndex computes bit counts as `8 * length_` in `int`,
  // reads bit vectors in 4-byte words, and evaluates `chunk_size_ * 8 *
  // chunk_length` (`256 * ceil(length / 32)`) in `int` at its sentinel chunk.
  // Aligning the limit down to a 32-byte chunk boundary keeps both computations
  // strictly within `int`.
  constexpr uint32_t kMaxBitVectorByteLength =
      static_cast<uint32_t>((std::numeric_limits<int>::max() / 8) & ~int{31});
  const auto is_valid_bit_vector_size = [](uint32_t byte_size) {
    return byte_size > 0 && byte_size <= kMaxBitVectorByteLength &&
           (byte_size % sizeof(uint32_t) == 0);
  };
  if (!is_valid_bit_vector_size(louds_size) ||
      !is_valid_bit_vector_size(terminal_size) || num_character_bits != 8) {
    return false;
  }
  const uint64_t required_payload_size =
      static_cast<uint64_t>(louds_size) + static_cast<uint64_t>(terminal_size) +
      static_cast<uint64_t>(edge_character_size);
  if (required_payload_size > image.size() - kHeaderSize) {
    return false;
  }

  const uint8_t* louds_image = ptr;
  const uint8_t* terminal_image = louds_image + louds_size;
  const uint8_t* edge_character = terminal_image + terminal_size;

  louds_.Init(louds_image, static_cast<int>(louds_size), louds_lb0_cache_size,
              louds_lb1_cache_size, louds_select0_cache_size,
              louds_select1_cache_size);
  const int num_nodes = louds_.num_nodes();
  if (!louds_.IsValidStructure(kMaxDepth) ||
      static_cast<uint64_t>(terminal_size) * 8 <
          static_cast<uint64_t>(num_nodes) ||
      edge_character_size < static_cast<uint32_t>(num_nodes)) {
    Close();
    return false;
  }
  terminal_bit_vector_.Init(terminal_image, static_cast<int>(terminal_size),
                            0,  // Select0 is not carried out.
                            termvec_lb1_cache_size);
  if (terminal_bit_vector_.Rank1(num_nodes) !=
      terminal_bit_vector_.GetNum1Bits()) {
    Close();
    return false;
  }
  edge_character_ = reinterpret_cast<const char*>(edge_character);

  return true;
}

void LoudsTrie::Close() {
  louds_.Reset();
  terminal_bit_vector_.Init(kEmptyTerminalImage, sizeof(kEmptyTerminalImage));
  edge_character_ = reinterpret_cast<const char*>(kEmptyTerminalImage);
}

bool LoudsTrie::MoveToChildByLabel(char label, Node* node) const {
  MoveToFirstChild(node);
  while (IsValidNode(*node)) {
    if (GetEdgeLabelToParentNode(*node) == label) {
      return true;
    }
    MoveToNextSibling(node);
  }
  return false;
}

bool LoudsTrie::Traverse(absl::string_view key, Node* node) const {
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

absl::string_view LoudsTrie::RestoreKeyString(Node node, char* buf) const {
  // Ensure the returned string view is null-terminated.
  char* const buf_end = buf + kMaxDepth;
  *buf_end = '\0';

  // Climb up the trie to the root and fill |buf| backward.
  char* ptr = buf_end;
  if (IsValidNode(node)) {
    for (; !louds_.IsRoot(node); louds_.MoveToParent(&node)) {
      *--ptr = GetEdgeLabelToParentNode(node);
    }
  }
  return absl::string_view(ptr, buf_end - ptr);
}

}  // namespace louds
}  // namespace storage
}  // namespace mozc
