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
#include <limits>
#include <memory>
#include <tuple>

#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "base/container/bitarray.h"
#include "converter/node.h"

namespace mozc {

// static
absl::StatusOr<std::unique_ptr<Segmenter>> Segmenter::Create(
    size_t l_num_elements, size_t r_num_elements,
    absl::Span<const uint16_t> l_table, absl::Span<const uint16_t> r_table,
    absl::Span<const char> bitarray_data,
    absl::Span<const uint16_t> boundary_data) {
  if (boundary_data.size() % 2 != 0) {
    return absl::FailedPreconditionError(
        "segmenter.cc: boundary_data size must be even");
  }
  // BitArray::GetValue indexes bits by uint32_t, so the compressed matrix can
  // have at most 2^32 bits (`kMaxBitArrayElements`).
  constexpr uint64_t kMaxBitArrayElements =
      uint64_t{std::numeric_limits<uint32_t>::max()} + 1;
  const uint64_t l_size = l_num_elements;
  const uint64_t r_size = r_num_elements;
  if (l_size > 0 && r_size > 0) {
    const bool exceeds_bitarray_index_limit =
        l_size > kMaxBitArrayElements || r_size > kMaxBitArrayElements / l_size;
    if (exceeds_bitarray_index_limit) {
      return absl::FailedPreconditionError(
          "segmenter.cc: compressed dimensions exceed BitArray limit");
    }
    const uint64_t required_bits = l_size * r_size;
    const uint64_t required_bytes = (required_bits + 7) / 8;
    if (required_bytes > bitarray_data.size()) {
      return absl::FailedPreconditionError(
          "segmenter.cc: bitarray_data is too small for compressed dimensions");
    }
  }
  for (const uint16_t l_id : l_table) {
    if (l_id >= l_num_elements) {
      return absl::FailedPreconditionError(
          "segmenter.cc: l_table entry exceeds l_num_elements");
    }
  }
  for (const uint16_t r_id : r_table) {
    if (r_id >= r_num_elements) {
      return absl::FailedPreconditionError(
          "segmenter.cc: r_table entry exceeds r_num_elements");
    }
  }
  return absl::WrapUnique(new Segmenter(l_num_elements, r_num_elements, l_table,
                                        r_table, bitarray_data, boundary_data));
}

// static
absl::StatusOr<std::unique_ptr<Segmenter>> Segmenter::Create(
    std::tuple<size_t, size_t, absl::Span<const uint16_t>,
               absl::Span<const uint16_t>, absl::Span<const char>,
               absl::Span<const uint16_t>>
        data) {
  return Create(std::get<0>(data), std::get<1>(data), std::get<2>(data),
                std::get<3>(data), std::get<4>(data), std::get<5>(data));
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
      boundary_data_(boundary_data) {}

bool Segmenter::IsBoundary(const Node& lnode, const Node& rnode,
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
  // rid and lid come from dictionary tokens rather than segmenter data, so
  // they are not covered by Segmenter::Create's load-time validation.
  if (rid >= l_table_.size() || lid >= r_table_.size()) [[unlikely]] {
    return true;
  }
  const uint32_t bitarray_index =
      l_table_[rid] + static_cast<uint32_t>(l_num_elements_) * r_table_[lid];
  return BitArray::GetValue(bitarray_data_.data(), bitarray_index);
}

int32_t Segmenter::GetPrefixPenalty(uint16_t lid) const {
  const size_t index = static_cast<size_t>(lid) * 2;
  if (index >= boundary_data_.size()) [[unlikely]] {
    return 0;
  }
  return boundary_data_[index];
}

int32_t Segmenter::GetSuffixPenalty(uint16_t rid) const {
  const size_t index = static_cast<size_t>(rid) * 2 + 1;
  if (index >= boundary_data_.size()) [[unlikely]] {
    return 0;
  }
  return boundary_data_[index];
}

}  // namespace mozc
