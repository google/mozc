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

#include <vector>
#include "base/file_stream.h"
#include "base/mmap.h"
#include "converter/connector.h"

DEFINE_bool(use_sparse_connector, true, "uses SparseConnector.");
DEFINE_bool(verify_connector, true, "verify output of connector");
DEFINE_bool(use_symmetric_matrix, true, "use symmetric matrix");

namespace mozc {
void ConnectorInterface::Compile(const char *text_file,
                                 const char *binary_file) {
  InputFileStream ifs(text_file);
  CHECK(ifs);
  string line;
  vector<string> fields;
  getline(ifs, line);
  Util::SplitStringUsing(line, "\t ", &fields);
  CHECK_GE(fields.size(), 2);

  // workaround:
  // prepare for invalid POS
  const uint16 lsize = atoi32(fields[0].c_str()) + 1;
  const uint16 rsize = atoi32(fields[1].c_str()) + 1;

  LOG(INFO) << "Making " << lsize << " x " << rsize << " matrix";
  vector<int16> matrix(lsize * rsize);
  fill(matrix.begin(), matrix.end(), 0);

  if (FLAGS_use_symmetric_matrix) {
    CHECK_EQ(lsize, rsize);
  }

  while (getline(ifs, line)) {
    fields.clear();
    Util::SplitStringUsing(line, "\t ", &fields);
    CHECK_GE(fields.size(), 3);
    const int l = atoi32(fields[0].c_str());
    const int r = atoi32(fields[1].c_str());
    int       c = atoi32(fields[2].c_str());
    CHECK(l < lsize && r < rsize) << "index values are out of range";

    // BOS->EOS connection cost is always 0
    if (l == 0 && r == 0) {
      c = 0;
    }
    matrix[(l + lsize * r)] = static_cast<int16>(c);
  }

  // cost for invalid POS
  // TODO(toshiyuki): remove this after defining new POS.
  for (int l = 0; l < lsize; ++l) {
    const int index = l + lsize * (rsize - 1);
    if (l == 0) {
      matrix[index] = 0;
    } else {
      matrix[index] = ConnectorInterface::kInvalidCost;
    }
  }
  for (int r = 0; r < rsize; ++r) {
    const int index = ((lsize - 1) + lsize * r);
    if (r == 0) {
      matrix[index] = 0;
    } else {
      matrix[index] = ConnectorInterface::kInvalidCost;
    }
  }

  if (FLAGS_use_sparse_connector) {
    SparseConnector::CompileImage(&matrix[0], lsize, rsize,
                                  binary_file);
  } else {
    DenseConnector::CompileImage(&matrix[0], lsize, rsize,
                                 binary_file);
  }

  if (FLAGS_verify_connector) {
    Mmap<char> mmap;
    CHECK(mmap.Open(binary_file, "r"));

    scoped_ptr<ConnectorInterface> connector(
        ConnectorInterface::OpenFromArray(mmap.begin(), mmap.GetFileSize()));
    CHECK(connector.get());

    for (int rid = 0; rid < rsize; ++rid) {
      for (int lid = 0; lid < lsize; ++lid) {
        CHECK_EQ(connector->GetTransitionCost(lid, rid),
                 matrix[lid + lsize * rid]);
      }
    }
  }

  return;
}
}  // namespace mozc
