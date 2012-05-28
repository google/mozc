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

#ifndef MOZC_CONVERTER_SPARSE_CONNECTOR_BUILDER_H_
#define MOZC_CONVERTER_SPARSE_CONNECTOR_BUILDER_H_

#include <vector>

#include "base/base.h"

namespace mozc {

class SparseArrayBuilder;

namespace converter {

class SparseConnectorBuilder {
 public:
  SparseConnectorBuilder();
  ~SparseConnectorBuilder();

  void SetIdFile(const string &id_file);
  void SetSpecialPosFile(const string &special_pos_file);

  uint16 MatrixSize() const { return id_size_ + special_pos_size_; }

  void Build(const string &text_connection_file);
  void WriteTo(const string& output_file) const;

  static void Compile(const string &text_connection_file,
                      const string &id_file,
                      const string &special_pos_file,
                      const string &output_file);
 private:
  void BuildInternal(const string &text_connection_file,
                     vector<int16> *output_matrix);
  void ParseTextConnectionFile(const string &text_connection_file,
                               vector<int16> *matrix) const;
  void FillDefaultCost(const vector<int16> &matrix);

  size_t id_size_;
  size_t special_pos_size_;

  vector<int16> default_cost_;
  scoped_ptr<SparseArrayBuilder> builder_;

  DISALLOW_COPY_AND_ASSIGN(SparseConnectorBuilder);
};
}  // namespace converter
}  // namespace mozc

#endif  // MOZC_CONVERTER_SPARSE_CONNECTOR_BUILDER_H_
