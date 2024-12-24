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

#include "converter/segmenter.h"

#include <cstddef>
#include <cstdint>
#include <memory>

#include "absl/log/check.h"
#include "absl/types/span.h"
#include "base/container/bitarray.h"
#include "converter/node.h"
#include "data_manager/data_manager.h"

namespace mozc {

std::unique_ptr<Segmenter> Segmenter::CreateFromDataManager(
    const DataManager &data_manager) {
  size_t l_num_elements = 0;
  size_t r_num_elements = 0;
  absl::Span<const uint16_t> l_table, r_table, boundary_data;
  absl::Span<const char> bitarray_data;
  data_manager.GetSegmenterData(&l_num_elements, &r_num_elements, &l_table,
                                &r_table, &bitarray_data, &boundary_data);
  return std::make_unique<Segmenter>(l_num_elements, r_num_elements, l_table,
                                     r_table, bitarray_data, boundary_data);
}

Segmenter::Segmenter(size_t l_num_elements, size_t r_num_elements,
                     absl::Span<const uint16_t> l_table,
                     absl::Span<const uint16_t> r_table,
                     absl::Span<const char> bitarray_data,
                     absl::Span<const uint16_t> boundary_data)
    : l_num_elements_(l_num_elements),
      r_num_elements_(r_num_elements),
      l_table_(l_table),
      r_table_(r_table),
      bitarray_data_(bitarray_data),
      boundary_data_(boundary_data) {
  CHECK_LE(l_num_elements_ * r_num_elements_, bitarray_data_.size() * 8);
}

bool Segmenter::IsBoundary(const Node &lnode, const Node &rnode,
                           bool is_single_segment) const {
  if (lnode.node_type == Node::BOS_NODE || rnode.node_type == Node::EOS_NODE) {
    return true;
  }

  // Always return false in prediction mode.
  // This implies that converter always returns single-segment-result
  // in prediction mode.
  if (is_single_segment) {
    return false;
  }

  // Concatenate particle and content word into one segment,
  // if lnode locates at the beginning of user input.
  // This hack is for handling ambiguous bunsetsu segmentation.
  // e.g. "かみ|にかく" => "紙|に書く" or "紙二角".
  // If we segment "に書く" into two segments, "二角" is never be shown.
  // There exits some implicit assumpution that user expects that their input
  // becomes one bunsetu. So, it would be better to keep "二角" even after "紙".
  if (lnode.attributes & Node::STARTS_WITH_PARTICLE) {
    return false;
  }

  return IsBoundary(lnode.rid, rnode.lid);
}

bool Segmenter::IsBoundary(uint16_t rid, uint16_t lid) const {
  const uint32_t bitarray_index =
      l_table_[rid] + l_num_elements_ * r_table_[lid];
  return BitArray::GetValue(bitarray_data_.data(), bitarray_index);
}

int32_t Segmenter::GetPrefixPenalty(uint16_t lid) const {
  return boundary_data_[2 * lid];
}

int32_t Segmenter::GetSuffixPenalty(uint16_t rid) const {
  return boundary_data_[2 * rid + 1];
}

}  // namespace mozc
