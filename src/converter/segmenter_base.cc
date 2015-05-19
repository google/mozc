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

#include "converter/segmenter_base.h"

#include "base/base.h"
#include "base/bitarray.h"
#include "base/logging.h"
#include "converter/boundary_struct.h"
#include "converter/node.h"
#include "data_manager/data_manager_interface.h"

namespace mozc {

SegmenterBase *SegmenterBase::CreateFromDataManager(
    const DataManagerInterface &data_manager) {
  size_t l_num_elements = 0;
  size_t r_num_elements = 0;
  const uint16 *l_table = NULL;
  const uint16 *r_table = NULL;
  size_t bitarray_num_bytes = 0;
  const char *bitarray_data = NULL;
  const BoundaryData *boundary_data = NULL;
  data_manager.GetSegmenterData(&l_num_elements, &r_num_elements,
                                &l_table, &r_table,
                                &bitarray_num_bytes, &bitarray_data,
                                &boundary_data);
  return new SegmenterBase(l_num_elements, r_num_elements,
                           l_table, r_table,
                           bitarray_num_bytes, bitarray_data,
                           boundary_data);
}

SegmenterBase::SegmenterBase(
    size_t l_num_elements, size_t r_num_elements, const uint16 *l_table,
    const uint16 *r_table, size_t bitarray_num_bytes,
    const char *bitarray_data, const BoundaryData *boundary_data)
    : l_num_elements_(l_num_elements), r_num_elements_(r_num_elements),
      l_table_(l_table), r_table_(r_table),
      bitarray_num_bytes_(bitarray_num_bytes),
      bitarray_data_(bitarray_data), boundary_data_(boundary_data) {
  DCHECK(l_table_);
  DCHECK(r_table_);
  DCHECK(bitarray_data_);
  DCHECK(boundary_data_);
  CHECK_LE(l_num_elements_ * r_num_elements_, bitarray_num_bytes_ * 8);
}

SegmenterBase::~SegmenterBase() {}

bool SegmenterBase::IsBoundary(const Node *lnode, const Node *rnode,
                               bool is_single_segment) const {
  DCHECK(lnode);
  DCHECK(rnode);
  if (lnode->node_type == Node::BOS_NODE ||
      rnode->node_type == Node::EOS_NODE) {
    return true;
  }

  // return always false in prediction mode.
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
  // There exits some implicit assumpution that user expects that his/her input
  // becomes one bunsetu. So, it would be better to keep "二角" even after "紙".
  if (lnode->attributes & Node::STARTS_WITH_PARTICLE) {
    return false;
  }

  return IsBoundary(lnode->rid, rnode->lid);
}

bool SegmenterBase::IsBoundary(uint16 rid, uint16 lid) const {
  const uint32 bitarray_index = l_table_[rid] + l_num_elements_ * r_table_[lid];
  return BitArray::GetValue(reinterpret_cast<const char*>(bitarray_data_),
                            bitarray_index);
}

int32 SegmenterBase::GetPrefixPenalty(uint16 lid) const {
  return boundary_data_[lid].prefix_penalty;
}

int32 SegmenterBase::GetSuffixPenalty(uint16 rid) const {
  return boundary_data_[rid].suffix_penalty;
}
}  // namespace mozc
