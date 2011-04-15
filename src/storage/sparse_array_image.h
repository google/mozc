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

// Storage for sparse array. This encodes set of indexes into a tree.
// SparseArray assumes distribution of connection is not uniform,
// and dense in some part of the matrix. Other wise the matrix image
// will be bloated.

// SparseArray uses trie tree for bit slices of key and uses
// rank operation of bit array to reach next level in the trie.
//
// level 0   ..
//              ..
//                \ ..
// level N  : .... 0101
//                  |  \ ..
// level N+1:      0000 1101 each node corresponds to 1st '1' and 2nd '1'.
//  ..
// level MAX: 0010 0110 .. rank of each '1' is the index of value array.

#ifndef MOZC_STORAGE_SPARS_ARRAY_IMAGE_H_
#define MOZC_STORAGE_SPARS_ARRAY_IMAGE_H_

#include <map>
#include <string>
#include <vector>

#include "base/base.h"

namespace mozc {
namespace sparse_array_image {
class BitArray;
struct BitTreeNode;
class ByteStream;
}  // namespace sparse_array_image

class SparseArrayBuilder {
 public:
  // Last 4 bytes of sparse array image. Used for sanity check.
  static const int kTrailerMagic = 0x12345678;

  SparseArrayBuilder();
  ~SparseArrayBuilder();
  void AddValue(uint32 key, int val);
  void SetUse1ByteValue(bool use_1byte_value);
  // Build sparse array image of added values.
  void Build();
  int GetSize() const;
  const char *GetImage() const;

 private:
  const static int kNumBitsPerLevel = 3;

  void AddNode(uint32 key);
  sparse_array_image::BitTreeNode *AllocNode();
  void Serialize();
  void Concatenate();

  // Encoded key to value.
  map<uint32, int> values_;
  bool use_1byte_value_;
  // Root node of trie tree.
  sparse_array_image::BitTreeNode *root_node_;
  int num_nodes_;

  vector<sparse_array_image::ByteStream *> streams_;
  scoped_ptr<sparse_array_image::ByteStream> value_stream_;
  scoped_ptr<sparse_array_image::ByteStream> main_stream_;

  // Currently, this is not configureable. Fixed with
  // kNumBitsPerLevel = 3.
  int num_bits_per_node_;
  int tree_depth_;
};

class SparseArrayImage {
 public:
  static const int kInvalidValueIndex = -1;

  SparseArrayImage(const char *image, int size);
  ~SparseArrayImage();

  // Returns index in values. kInvalidValueIndex will be returned
  // if the value is not available.
  int Peek(uint32 index) const;
  // Returns nth value.
  int GetValue(int nth) const;

 private:
  int ReadInt(const char *p) const;

  const char *image_;
  int size_;

  int num_bits_per_level_;
  bool use_1byte_value_;
  int num_levels_;
  vector<sparse_array_image::BitArray *> arrays_;

  int values_size_;
  const char *values_;
};
}  // namespace mozc

#endif  // MOZC_STORAGE_SPARS_ARRAY_IMAGE_H_
