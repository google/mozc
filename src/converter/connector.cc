// Copyright 2010, Google Inc.
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

#include <algorithm>
#include <string>
#include <vector>
#include "base/base.h"
#include "base/file_stream.h"
#include "base/util.h"
#include "converter/connector.h"
#include "storage/sparse_array_image.h"

namespace mozc {

ConnectorInterface *ConnectorInterface::OpenFromArray(const char *ptr,
                                                      size_t size) {
  const int16 *image = reinterpret_cast<const int16*>(ptr);
  if (image[0] == kDenseConnectorMagic) {
    VLOG(1) << "DenseConector";
    return new DenseConnector(ptr, size);
  } else if (image[0] == kSparseConnectorMagic) {
    VLOG(1) << "SparseConector";
    return new SparseConnector(ptr, size);
  }
  LOG(FATAL) << "unsuported Connector image";
  return NULL;
}

///////////////////////////////////////////////////////////////////////
// DenseConnector implementations
DenseConnector::DenseConnector(const char *ptr, size_t size)
    : matrix_(NULL), lsize_(0), rsize_(0) {
  matrix_ = reinterpret_cast<const int16*>(ptr);
  lsize_  = static_cast<int>(matrix_[2]);
  rsize_  = static_cast<int>(matrix_[3]);
  CHECK(static_cast<int>(lsize_ * rsize_ + 4) ==
        size / sizeof(matrix_[0]))
      << "connection table is broken";
  matrix_ += 4;
}

DenseConnector::~DenseConnector() {}

void DenseConnector::CompileImage(const int16 *matrix,
                                  uint16 lsize, uint16 rsize,
                                  const char *binary_file) {
  OutputFileStream ofs(binary_file, ios::binary|ios::out);
  CHECK(ofs) << "permission denied: " << binary_file;
  uint16 magic = kDenseConnectorMagic;
  ofs.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
  // write one more value to align matrix image in 4 bytes.
  magic = 0;
  ofs.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
  ofs.write(reinterpret_cast<const char*>(&lsize), sizeof(lsize));
  ofs.write(reinterpret_cast<const char*>(&rsize), sizeof(rsize));
  ofs.write(reinterpret_cast<const char*>(&matrix[0]),
            sizeof(matrix[0]) * lsize * rsize);
  ofs.close();
}

///////////////////////////////////////////////////////////////////////
// SparseConnector implementations
SparseConnector::SparseConnector(const char *ptr, size_t size)
    : default_cost_(NULL) {
  // |magic(2byte)|null(2byte)|lsize(2byte)|rsize(2byte)
  // |default_cost..|sparse_image
  const size_t kHeaderSize = 8;
  CHECK_GT(size, kHeaderSize);
  const uint16 *image = reinterpret_cast<const uint16*>(ptr);
  const uint16 lsize = image[2];
  ptr += kHeaderSize;

  default_cost_ = reinterpret_cast<const int16 *>(ptr);
  const size_t default_cost_size = sizeof(default_cost_[0]) * lsize;

  ptr += default_cost_size;

  CHECK_GT(size, default_cost_size + kHeaderSize);

  const size_t array_image_size= size - kHeaderSize - default_cost_size;
  array_image_.reset(new SparseArrayImage(ptr, array_image_size));
}

SparseConnector::~SparseConnector() {}

int SparseConnector::GetTransitionCost(uint16 rid, uint16 lid) const {
  const int pos = array_image_->Peek(EncodeKey(rid, lid));
  if (pos == SparseArrayImage::kInvalidValueIndex) {
    return default_cost_[rid];
  }
  return array_image_->GetValue(pos);
}

void SparseConnector::CompileImage(const int16 *matrix,
                                   uint16 lsize, uint16 rsize,
                                   const char *binary_file) {
  LOG(INFO) << "compiling matrix with " << lsize * rsize;

  SparseArrayBuilder builder;
  vector<int16> default_cost(lsize);
  fill(default_cost.begin(), default_cost.end(), static_cast<int16>(0));

  for (int lid = 0; lid < lsize; ++lid) {
    for (int rid = 0; rid < rsize; ++rid) {
      const int16 c = matrix[lid + lsize * rid];
      if (ConnectorInterface::kInvalidCost != c) {
        default_cost[lid] = max(default_cost[lid], c);  // save default cost
      }
    }
  }

  for (int lid = 0; lid < lsize; ++lid) {
    for (int rid = 0; rid < rsize; ++rid) {
      const int16 c = matrix[lid + lsize * rid];
      if (c != default_cost[lid]) {
        builder.AddValue(EncodeKey(lid, rid), c);
      }
    }
  }

  builder.Build();

  OutputFileStream ofs(binary_file, ios::binary|ios::out);
  CHECK(ofs) << "permission denied: " << binary_file;

  CHECK_EQ(lsize, default_cost.size());

  uint16 magic = kSparseConnectorMagic;
  ofs.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
  // write one more value to align matrix image in 4 bytes.
  magic = 0;
  ofs.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
  ofs.write(reinterpret_cast<const char*>(&lsize), sizeof(lsize));
  ofs.write(reinterpret_cast<const char*>(&rsize), sizeof(rsize));
  ofs.write(reinterpret_cast<const char*>(&default_cost[0]),
            sizeof(default_cost[0]) * default_cost.size());
  ofs.write(builder.GetImage(), builder.GetSize());
  ofs.close();
}
}  // namespace mozc
