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

// Dictioneray data generator:
// (1) Making transition matrix
// % gen_dictionary_data_main
//   --mode=con --input=connection.txt --output=matrix.bin
// (2) Making binary dictionary
// % gen_dictionary_data_main
//   --mode=dic --input=dictionary0.txt,dictionary1.txt
//   --output=dic.bin
#include <string>
#include <vector>
#include "base/base.h"
#include "base/util.h"
#include "converter/converter_compiler.h"

DEFINE_string(input, "", "input text file");
DEFINE_string(output, "", "output binary file");
DEFINE_string(mode, "", "mode");

DEFINE_string(name, "", "name of variable name");
DEFINE_bool(make_header, false, "make header mode");

namespace mozc {
namespace {
// convert space delimtered text to CSV
string CreateInputFileName(const string ssv_filename) {
  vector<string> tokens;
  string output;
  Util::SplitStringUsing(ssv_filename, " ", &tokens);
  Util::JoinStrings(tokens, ",", &output);
  return output;
}
}  // namespace
}  // mozc

int main(int argc, char **argv) {
  InitGoogle(argv[0], &argc, &argv, false);

  const string input = mozc::CreateInputFileName(FLAGS_input);
  string output = FLAGS_output;

  if (FLAGS_make_header) {
    output += ".tmp";
  }

  if (FLAGS_mode == "dic") {
    mozc::ConverterCompiler::CompileDictionary(input, output);
  } else if (FLAGS_mode == "con") {
    mozc::ConverterCompiler::CompileConnectionTable(input, output);
  } else {
    LOG(FATAL) << "Unknown mode: " << FLAGS_mode;
    return -1;
  }

  if (FLAGS_make_header) {
    CHECK(!FLAGS_name.empty());
    mozc::ConverterCompiler::MakeHeaderFile(FLAGS_name,
                                            output,
                                            FLAGS_output);
    mozc::Util::Unlink(output);
  }

  return 0;
}
