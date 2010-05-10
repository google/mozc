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

#include "storage/sparse_array_image.h"

#include "base/base.h"

using mozc::sparse_array_image::BitArray;
using mozc::sparse_array_image::BitTreeNode;
using mozc::sparse_array_image::ByteStream;

namespace mozc {
namespace sparse_array_image {
// Trie node. Used to build sparse array image.
struct BitTreeNode {
  ~BitTreeNode();
  vector<struct BitTreeNode *> children;
  int mask;
};

BitTreeNode::~BitTreeNode() {
  for (int i = 0; i < children.size(); ++i) {
    delete children[i];
  }
}

// Bit array with rank operation.
class BitArray {
 public:
  BitArray(const char *image, int size);
  ~BitArray();
  int Rank(int n);
  char GetByte(int idx);

 private:
  int PopCount(unsigned int p);

  const char *image_;
  int size_;
  // Pre computed rank value for each 4 bytes.
  int *rank_array_;
};


BitArray::BitArray(const char *image, int size) : image_(image), size_(size) {
  int rank_array_len = (size + 3) / 4;
  rank_array_ = new int[rank_array_len];
  int r = 0;
  const unsigned int *p = reinterpret_cast<const unsigned int *>(image_);
  for (int i = 0; i < rank_array_len; ++i) {
    rank_array_[i] = r;
    r += PopCount(*p);
    ++p;
  }
}

BitArray::~BitArray() {
  delete [] rank_array_;
}

int BitArray::Rank(int n) {
  int idx = n / 32;
  int rem = n % 32;
  const unsigned int *p = reinterpret_cast<const unsigned int *>(image_);
  int rank = rank_array_[idx];
  if (rem) {
    rank += PopCount(p[idx] << (32 - rem));
  }
  return rank;
}

char BitArray::GetByte(int idx) {
  return image_[idx];
}

int BitArray::PopCount(unsigned int x) {
  x = ((x & 0xaaaaaaaa) >> 1) + (x & 0x55555555);
  x = ((x & 0xcccccccc) >> 2) + (x & 0x33333333);
  x = ((x >> 4) + x) & 0x0f0f0f0f;
  x = (x >> 8) + x;
  x = ((x >> 16) + x) & 0x3f;
  return x;
}

// Byte stream to store partially built sparse array image.
class ByteStream {
 public:
  ByteStream();

  void PushByte(uint8 b);
  void PushInt(int n);
  void PushString(const string &s);
  void PushPadding(int pad);
  int GetSize() const;
  const char *GetImage() const;
  const string &GetString() const;

 private:
  string str_;
};

ByteStream::ByteStream() {
}

void ByteStream::PushByte(uint8 b) {
  str_ += static_cast<char>(b);
}

void ByteStream::PushInt(int n) {
  for (int i = 0; i < 4; ++i) {
    PushByte(n & 255);
    n >>= 8;
  }
}

void ByteStream::PushString(const string &s) {
  str_ += s;
}

void ByteStream::PushPadding(int pad) {
  int rem = pad - (str_.size() % pad);
  if (rem != pad) {
    for (int i = 0; i < rem; ++i) {
      PushByte(0);
    }
  }
}

int ByteStream::GetSize() const {
  return str_.size();
}

const char *ByteStream::GetImage() const {
  return str_.data();
}

const string &ByteStream::GetString() const {
  return str_;
}
}  // namespace sparse_array_image

SparseArrayBuilder::SparseArrayBuilder() : root_node_(NULL),
                                           value_stream_(new ByteStream),
                                           main_stream_(new ByteStream) {
  num_bits_per_node_ = (1 << kNumBitsPerLevel);
  tree_depth_ = 32 / kNumBitsPerLevel;
  if (32 % kNumBitsPerLevel) {
    ++tree_depth_;
  }
}

SparseArrayBuilder::~SparseArrayBuilder() {
  delete root_node_;
  for (size_t i = 0; i < streams_.size(); ++i) {
    delete streams_[i];
  }
}

void SparseArrayBuilder::AddValue(uint32 key, int val) {
  values_[key] = val;
}

void SparseArrayBuilder::Build() {
  CHECK_EQ(main_stream_->GetSize(), 0)
      << "sparse array was already built.";
  LOG(INFO) << "Building sparse array with "
            << values_.size() << " values";
  root_node_ = AllocNode();
  num_nodes_ = 0;
  for (map<uint32, int>::const_iterator it = values_.begin();
       it != values_.end(); ++it) {
    AddNode(it->first);
    value_stream_->PushByte(it->second & 0xff);
    value_stream_->PushByte((it->second >> 8) & 0xff);
  }
  LOG(INFO) << "allocated " << num_nodes_ << " nodes";
  Serialize();
  Concatenate();
  LOG(INFO) << "image size=" << main_stream_->GetSize() << "bytes";
  float ratio =
      static_cast<float>(main_stream_->GetSize() - (values_.size() * 2))
      / values_.size();
  LOG(INFO) << "trie over head per each value=" << ratio << "bytes";
}

int SparseArrayBuilder::GetSize() const {
  return main_stream_->GetSize();
}

const char *SparseArrayBuilder::GetImage() const {
  return main_stream_->GetImage();
}

void SparseArrayBuilder::AddNode(uint32 key) {
  BitTreeNode *current_node = root_node_;
  for (int i = 0; i < tree_depth_; ++i) {
    const int shift_count = kNumBitsPerLevel * (tree_depth_ - i - 1);
    const int idx = (key >> shift_count) % (1 << kNumBitsPerLevel);
    if (!current_node->children[idx]) {
      current_node->children[idx] = AllocNode();
      current_node->mask |= (1 << idx);
    }
    current_node = current_node->children[idx];
  }
}

BitTreeNode *SparseArrayBuilder::AllocNode() {
  BitTreeNode *node = new BitTreeNode;
  node->children.resize(num_bits_per_node_);
  for (int i = 0; i < num_bits_per_node_; ++i) {
    node->children[i] = NULL;
  }
  node->mask = 0;
  ++num_nodes_;
  return node;
}

void SparseArrayBuilder::Serialize() {
  vector<BitTreeNode *> queue;
  vector<BitTreeNode *> child_queue;
  queue.push_back(root_node_);
  for (int level = 0; level < 11; ++level) {
    ByteStream *stream = new ByteStream;
    streams_.push_back(stream);
    child_queue.clear();
    for (size_t i = 0; i < queue.size(); ++i) {
      BitTreeNode *node = queue[i];
      for (size_t j = 0; j < node->children.size(); ++j) {
        if (node->children[j]) {
          child_queue.push_back(node->children[j]);
        }
      }
      stream->PushByte(node->mask);
    }
    queue = child_queue;
  }
  for (size_t i = 0; i < streams_.size(); ++i) {
    ByteStream *stream = streams_[i];
    stream->PushPadding(4);
  }
}

void SparseArrayBuilder::Concatenate() {
  // num bits per level.
  main_stream_->PushInt(kNumBitsPerLevel);
  main_stream_->PushInt(value_stream_->GetSize());
  // write streams.
  for (size_t i = 0; i < streams_.size(); ++i) {
    ByteStream *stream = streams_[i];
    main_stream_->PushInt(stream->GetSize());
  }
  for (size_t i = 0; i < streams_.size(); ++i) {
    ByteStream *stream = streams_[i];
    main_stream_->PushString(stream->GetString());
  }
  main_stream_->PushString(value_stream_->GetString());
  // trailer for debug
  main_stream_->PushInt(kTrailerMagic);
}

SparseArrayImage::SparseArrayImage(const char *image, int size)
    : image_(image), size_(size) {
  DCHECK(image) << "got empty message";
  const char *p = image_;
  num_bits_per_level_ = ReadInt(p);
  p += 4;
  values_size_ = ReadInt(p);
  p += 4;
  num_levels_ = 32 / num_bits_per_level_;
  if (32 % num_bits_per_level_) {
    ++num_levels_;
  }
  const char *bytes = p + (num_levels_ * 4);
  for (int i = 0; i < num_levels_; ++i) {
    size = ReadInt(p);
    p += 4;
    BitArray *array = new BitArray(bytes, size);
    bytes += size;
    arrays_.push_back(array);
  }
  values_ = bytes;
  bytes += values_size_;
  CHECK(ReadInt(bytes) == SparseArrayBuilder::kTrailerMagic)
      << "trailer magic mismatch";
  LOG(INFO) << "SparseArrayImage: "
            << values_size_ / 2 << " values";
}

SparseArrayImage::~SparseArrayImage() {
  for (int i = 0; i < arrays_.size(); ++i) {
    delete arrays_[i];
  }
}

int SparseArrayImage::ReadInt(const char *p) const {
  const int *n = reinterpret_cast<const int *>(p);
  return *n;
}

int SparseArrayImage::Peek(uint32 index) const {
  int byte_offset = 0;
  for (int level = 0; level < num_levels_; ++level) {
    int shift_count = num_bits_per_level_ * (num_levels_ - level - 1);
    int idx = (index >> shift_count) % (1 << num_bits_per_level_);
    BitArray *array = arrays_[level];
    // TODO(tabata): fix more than 8 bits.
    char mask = array->GetByte(byte_offset);
    if (!(mask & (1 << idx))) {
      return kInvalidValueIndex;
    }
    byte_offset = array->Rank(byte_offset * 8 + idx);
  }
  return byte_offset;
}

int SparseArrayImage::GetValue(int nth) const {
  const uint16 *v = reinterpret_cast<const uint16 *>(&values_[nth * 2]);
  return *v;
}
}  // namespace mozc
