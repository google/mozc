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

#include "storage/louds/louds.h"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace mozc {
namespace storage {
namespace louds {
namespace {

alignas(uint32_t) constexpr uint8_t kEmptyLoudsImage[4] = {0, 0, 0, 0};

}  // namespace

void Louds::Init(const uint8_t* image, int length, size_t bitvec_lb0_cache_size,
                 size_t bitvec_lb1_cache_size, size_t select0_cache_size,
                 size_t select1_cache_size) {
  index_.Init(image, length, bitvec_lb0_cache_size, bitvec_lb1_cache_size);

  // Cap the cache sizes.
  if (select0_cache_size > index_.GetNum0Bits()) {
    select0_cache_size = index_.GetNum0Bits();
  }
  if (select1_cache_size > index_.GetNum1Bits()) {
    select1_cache_size = index_.GetNum1Bits();
  }

  // Initialize Select0 and Select1 cache for speed.  In LOUDS traversal, nodes
  // close to the root are frequently accessed.  Thus, we precompute select0 and
  // select1 values for such nodes.  Since node IDs are assigned in BFS order,
  // the nodes close to the root are assigned smaller IDs.  Hence, a simple
  // array can be used for the mapping from ID to cached value.
  select0_cache_size_ = select0_cache_size;
  select1_cache_size_ = select1_cache_size;
  const size_t cache_size = select0_cache_size + select1_cache_size;
  if (cache_size == 0) {
    return;
  }
  select_cache_ = std::make_unique<int[]>(cache_size);

  if (select0_cache_size > 0) {
    // Precompute Select0(i) + 1 for i in (0, select0_cache_size).
    select_cache_[0] = 0;
    for (size_t i = 1; i < select0_cache_size; ++i) {
      select_cache_[i] = index_.Select0(i) + 1;
    }
  }

  if (select1_cache_size > 0) {
    // Precompute Select1(i) for i in (0, select1_cache_size).
    select1_cache_ptr_ = select_cache_.get() + select0_cache_size;
    select1_cache_ptr_[0] = 0;
    for (size_t i = 1; i < select1_cache_size; ++i) {
      select1_cache_ptr_[i] = index_.Select1(i);
    }
  }
}

void Louds::Reset() {
  index_.Init(kEmptyLoudsImage, sizeof(kEmptyLoudsImage));
  select_cache_.reset();
  select0_cache_size_ = 0;
  select1_cache_size_ = 0;
}

bool Louds::IsValidStructure(size_t max_depth) const {
  const int num_1bits = index_.GetNum1Bits();
  const int num_0bits = index_.GetNum0Bits();
  if (num_0bits <= num_1bits) {
    return false;
  }
  // Super-root must be encoded as "10" (single root node with node_id = 1).
  if (index_.Get(0) != 1 || index_.Get(1) != 0) {
    return false;
  }

  int level_end_node = 1;  // Last node_id at the current BFS `depth`.
  size_t depth = 0;

  // All nodes in `(prev_level_end, level_end_node]` already appeared as 1-bits
  // in earlier BFS levels, so `nodes_seen >= level_end_node >= parent_id` holds
  // throughout the level. Jumping to the `(level_end_node + 1)`-th 0-bit (where
  // bit 1 is the super-root's 0-bit) advances one full BFS level per Select0
  // query, enforcing `Select1(k) < Select0(k)` for all k in [1, num_1bits] and
  // ensuring no 1-bits remain in trailing padding.
  while (level_end_node < num_1bits) {
    const int nodes_seen = index_.Select0(level_end_node + 1) - level_end_node;
    if (nodes_seen == level_end_node) {
      return false;
    }
    ++depth;
    if (depth > max_depth) {
      return false;
    }
    level_end_node = nodes_seen;
  }
  return true;
}

}  // namespace louds
}  // namespace storage
}  // namespace mozc
