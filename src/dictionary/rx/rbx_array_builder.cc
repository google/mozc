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

#include "dictionary/rx/rbx_array_builder.h"

#include "base/base.h"
#include "base/file_stream.h"
#include "third_party/rx/v1_0rc2/rx.h"

namespace mozc {
namespace rx {
RbxArrayBuilder::RbxArrayBuilder() : rbx_builder_(rbx_builder_create()) {}

RbxArrayBuilder::~RbxArrayBuilder() {
  rbx_builder_release(rbx_builder_);
}

void RbxArrayBuilder::SetLengthCoding(int min_element_length,
                                      int element_length_step) {
  rbx_builder_set_length_coding(rbx_builder_,
                                min_element_length,
                                element_length_step);
}

void RbxArrayBuilder::Push(const string &value) {
  rbx_builder_push(rbx_builder_, value.data(), value.size());
}

void RbxArrayBuilder::Build() {
  rbx_builder_build(rbx_builder_);
}

void RbxArrayBuilder::WriteImage(OutputFileStream *ofs) const {
  DCHECK(ofs);
  const unsigned char *image = rbx_builder_get_image(rbx_builder_);
  const int size = rbx_builder_get_size(rbx_builder_);
  ofs->write(reinterpret_cast<const char *>(image), size);
}
}  // namespace rx
}  // namespace mozc
