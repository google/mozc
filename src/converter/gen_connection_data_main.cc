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

#include <string>
#include <vector>

#include "base/base.h"
#include "base/codegen_bytearray_stream.h"
#include "base/file_stream.h"
#include "base/mmap.h"
#include "base/util.h"
#include "converter/sparse_connector.h"
#include "converter/sparse_connector_builder.h"

DEFINE_string(input, "", "input text file");
DEFINE_string(output, "", "output binary file");
DEFINE_bool(make_header, false, "make header mode");

int main(int argc, char **argv) {
  InitGoogle(argv[0], &argc, &argv, false);
  string output = FLAGS_output;

  if (FLAGS_make_header) {
    output += ".tmp";
  }

  vector<string> files;
  mozc::Util::SplitStringUsing(FLAGS_input, " ", &files);
  CHECK_EQ(3, files.size());

  mozc::converter::SparseConnectorBuilder::Compile(
      files[0],  // connection.txt
      files[1],  // id.def
      files[2],  // special_pos.def
      output);

  if (FLAGS_make_header) {
    mozc::Mmap<char> mmap;
    CHECK(mmap.Open(output.c_str()));

    mozc::OutputFileStream ofs(FLAGS_output.c_str());
    mozc::CodeGenByteArrayOutputStream codegen_stream(
        &ofs, mozc::codegenstream::NOT_OWN_STREAM);
    codegen_stream.OpenVarDef("ConnectionData");
    codegen_stream.write(mmap.begin(), mmap.Size());
    codegen_stream.CloseVarDef();
    ofs.close();

    mozc::Util::Unlink(output);
  }

  return 0;
}
