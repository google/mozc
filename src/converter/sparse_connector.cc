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

#include "converter/sparse_connector.h"

#include <algorithm>
#include <string>
#include <vector>
#include "base/base.h"
#include "base/file_stream.h"
#include "base/util.h"
#include "storage/sparse_array_image.h"

namespace mozc {

SparseConnector::SparseConnector(const char *ptr, size_t size)
    : default_cost_(NULL) {
  // |magic(2bytes)|resolution(2bytes)|lsize(2bytes)|rsize(2bytes)
  // |default_cost..|sparse_image
  const size_t kHeaderSize = 8;
  CHECK_GT(size, kHeaderSize);
  const uint16 *image = reinterpret_cast<const uint16*>(ptr);
  resolution_ = image[1];
  const uint16 lsize = image[2];
  const uint16 rsize = image[3];
  CHECK_EQ(lsize, rsize);
  ptr += kHeaderSize;

  default_cost_ = reinterpret_cast<const int16 *>(ptr);
  const size_t default_cost_size = sizeof(default_cost_[0]) * lsize;

  ptr += default_cost_size;

  CHECK_GT(size, default_cost_size + kHeaderSize);

  const size_t array_image_size = size - kHeaderSize - default_cost_size;
  array_image_.reset(new SparseArrayImage(ptr, array_image_size));
}

SparseConnector::~SparseConnector() {}

int SparseConnector::GetTransitionCost(uint16 rid, uint16 lid) const {
  const int pos = array_image_->Peek(EncodeKey(rid, lid));
  if (pos == SparseArrayImage::kInvalidValueIndex) {
    return default_cost_[rid];
  }
  const int cost = array_image_->GetValue(pos);
  if (resolution_ > 1 && cost == kInvalid1ByteCostValue) {
    return ConnectorInterface::kInvalidCost;
  }
  return cost * resolution_;
}

int SparseConnector::GetResolution() const {
  return resolution_;
}
}  // namespace mozc
