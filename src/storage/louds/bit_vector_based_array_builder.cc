// Copyright 2010-2014, Google Inc.
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

#include "storage/louds/bit_vector_based_array_builder.h"

#include "base/logging.h"
#include "storage/louds/bit_stream.h"

namespace mozc {
namespace storage {
namespace louds {

// Initial values of base_length_ and step_length_ is (4, 1) bytes.
BitVectorBasedArrayBuilder::BitVectorBasedArrayBuilder()
    : built_(false), base_length_(4), step_length_(1) {
}

BitVectorBasedArrayBuilder::~BitVectorBasedArrayBuilder() {
}

void BitVectorBasedArrayBuilder::Add(const string &element) {
  CHECK(!built_);
  elements_.push_back(element);
}

void BitVectorBasedArrayBuilder::SetSize(
    size_t base_length, size_t step_length) {
  CHECK(!built_);
  base_length_ = base_length;
  step_length_ = step_length;
}

namespace {
void PushInt32(int value, string *image) {
  CHECK_EQ(value & ~0xFFFFFFFF, 0);

  // Output from LSB to MSB.
  image->push_back(static_cast<char>(value & 0xFF));
  image->push_back(static_cast<char>((value >> 8) & 0xFF));
  image->push_back(static_cast<char>((value >> 16) & 0xFF));
  image->push_back(static_cast<char>((value >> 24) & 0xFF));
}
}  // namespace

void BitVectorBasedArrayBuilder::Build() {
  CHECK(!built_);

  BitStream bit_stream;
  string data;

  // Output to the bit_stream and the data.
  for (size_t i = 0; i < elements_.size(); ++i) {
    const string &element = elements_[i];

    // Counts how many steps is needed.
    int num_steps = 0;
    for (size_t length = element.length(); length > base_length_;
         length -= step_length_) {
      ++num_steps;
    }

    // Output '0' as a beginning bit, followed by the num_steps of '1'-bits.
    size_t output_length = base_length_ + num_steps * step_length_;
    DCHECK_GE(output_length, element.length());
    bit_stream.PushBit(0);
    for (int j = 0; j < num_steps; ++j) {
      bit_stream.PushBit(1);
    }

    // Output word data (excluding '\0' termination) and then padding by '\0'
    // to align to the output length.
    data.append(element);
    data.append(output_length - element.length(), '\0');
  }

  // Output a sentinel, and align to 32 bits.
  bit_stream.PushBit(0);
  bit_stream.FillPadding32();

  // Output the image.
  PushInt32(bit_stream.ByteSize(), &image_);
  PushInt32(base_length_, &image_);
  PushInt32(step_length_, &image_);
  PushInt32(0, &image_);

  image_.append(bit_stream.image());
  image_.append(data);

  built_ = true;
}

const string &BitVectorBasedArrayBuilder::image() const {
  CHECK(built_);
  return image_;
}

}  // namespace louds
}  // namespace storage
}  // namespace mozc
