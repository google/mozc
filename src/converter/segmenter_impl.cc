// Copyright 2010-2011, Google Inc.
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

#include "converter/segmenter_impl.h"

#include "base/base.h"
#include "base/bitarray.h"
#include "converter/node.h"

namespace mozc {
namespace {

#include "converter/boundary_data.h"
#include "converter/segmenter_data.h"
}  // namespace

SegmenterImpl::SegmenterImpl() {}

SegmenterImpl::~SegmenterImpl() {}

bool SegmenterImpl::IsBoundary(const Node *lnode, const Node *rnode,
                               bool is_single_segment) const {
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

  const bool is_boundary = IsBoundary(lnode->rid, rnode->lid);

  // Concatenate particle and content word into one segment,
  // if lnode locates at the beginning of user input.
  // This hack is for handling ambiguous bunsetsu segmentation.
  // e.g. "かみ|にかく" => "紙|に書く" or "紙二角".
  // If we segment "に書く" into two segments, "二角" is never be shown.
  // There exits some implicit assumpution that user expects that his/her input
  // becomes one bunsetu. So, it would be better to keep "二角" even after "紙".
  if (is_boundary && (lnode->attributes & Node::STARTS_WITH_PARTICLE)) {
    return false;
  }

  return is_boundary;
}

// use only for gen_segmenter_bitarrray.
bool SegmenterImpl::IsBoundary(uint16 rid, uint16 lid) const {
  //return IsBoundaryInternal(rid, lid);
  return BitArray::GetValue(reinterpret_cast<const char*>
                            (kSegmenterBitArrayData_data),
                            kCompressedLIDTable[rid] +
                            kCompressedLSize *
                            kCompressedRIDTable[lid]);
}

// static
int32 SegmenterImpl::GetPrefixPenalty(uint16 lid) const {
  return kBoundaryData[lid].prefix_penalty;
}

// static
int32 SegmenterImpl::GetSuffixPenalty(uint16 rid) const {
  return kBoundaryData[rid].suffix_penalty;
}
}  // namespace mozc
