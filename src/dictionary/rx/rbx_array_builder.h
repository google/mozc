// Copyright 2010-2012, Google Inc.
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

#ifndef MOZC_DICTIONARY_RX_RBX_ARRAY_BUILDER_H_
#define MOZC_DICTIONARY_RX_RBX_ARRAY_BUILDER_H_

#include "base/base.h"

struct rbx_builder;

namespace mozc {
class OutputFileStream;
namespace rx {

class RbxArrayBuilder {
 public:
  RbxArrayBuilder();
  virtual ~RbxArrayBuilder();

  void SetLengthCoding(int min_element_length,
                       int element_length_step);

  // Push data
  void Push(const string &value);

  // Build array
  void Build();

  // Write image of array
  void WriteImage(OutputFileStream *ofs) const;

 private:
  struct rbx_builder *rbx_builder_;

  DISALLOW_COPY_AND_ASSIGN(RbxArrayBuilder);
};

}  // namespace rx
}  // namespace mozc
#endif  // MOZC_DICTIONARY_RX_RBX_ARRAY_BUILDER_H_
