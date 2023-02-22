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

#ifndef MOZC_STORAGE_LOUDS_BIT_VECTOR_BASED_ARRAY_BUILDER_H_
#define MOZC_STORAGE_LOUDS_BIT_VECTOR_BASED_ARRAY_BUILDER_H_

#include <cstddef>
#include <string>
#include <vector>

namespace mozc {
namespace storage {
namespace louds {

class BitVectorBasedArrayBuilder {
 public:
  BitVectorBasedArrayBuilder();
  BitVectorBasedArrayBuilder(const BitVectorBasedArrayBuilder&) = delete;
  BitVectorBasedArrayBuilder& operator=(const BitVectorBasedArrayBuilder&) =
      delete;
  ~BitVectorBasedArrayBuilder() = default;

  // Adds the element to the builder.
  // The length of the element would be ceiling by padding '\x00' bytes to the
  // end. Please see also comments for SetSize below.
  void Add(const std::string& element);

  // Sets size related parameters. The size of each element will be ceiling
  // (when the image is built) as follows:
  // - if the length of a element is less than or equal to base_length,
  //   the length in the image should be base_length.
  // - otherwise, the length in the image should be
  //   "base_length + N * step_length", where N is a positive integer.
  // For example, let base_length be '4' and step_length be '2';
  // - "" would be stored as "\x00\x00\x00\x00".
  // - "a" would be stored as "a\x00\x00\x00"
  // - "abcd" would be stored as "abcd"
  // - "abcde" would be stored as "abcde\x00"
  // Note that the '\0' terminator should *NOT* be stored in the image.
  void SetSize(size_t base_length, size_t step_length);

  void Build();

  const std::string& image() const;

 private:
  bool built_;
  std::vector<std::string> elements_;
  size_t base_length_;
  size_t step_length_;

  std::string image_;
};

}  // namespace louds
}  // namespace storage
}  // namespace mozc

#endif  // MOZC_STORAGE_LOUDS_BIT_VECTOR_BASED_ARRAY_BUILDER_H_
