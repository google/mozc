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

// Generates system dictionary header file.
//
// gen_system_dictionary_data_main
//  --input="dictionary0.txt dictionary1.txt zip_code_seed.txt"
//  --output="output.h"
//  --include_zip_code
//  --make_header

#include <string>
#include <vector>
#include "base/base.h"
#include "base/util.h"
#include "dictionary/system/system_dictionary_builder.h"
#include "dictionary/zip_code_dictionary_builder.h"

DEFINE_string(input, "", "space separated input text files");
DEFINE_string(output, "", "output binary file");
DEFINE_bool(include_zip_code, false,
            "include zip code dictionary by specifying seed dictionary"
            " at the end of --input");
DEFINE_bool(make_header, false, "make header mode");

namespace mozc {
namespace {
// convert space delimtered text to CSV
string GetInputFileName(const vector<string> filenames) {
  string output;
  Util::JoinStrings(filenames, ",", &output);
  return output;
}
}  // namespace
}  // mozc

int main(int argc, char **argv) {
  InitGoogle(argv[0], &argc, &argv, false);

  vector<string> input_files;
  mozc::Util::SplitStringUsing(FLAGS_input, " ", &input_files);
  string output = FLAGS_output;

  if (FLAGS_make_header) {
    output += ".tmp";
  }

  string zip_code_text_dictionary;
  if (FLAGS_include_zip_code) {
    zip_code_text_dictionary = FLAGS_output + ".zip_code_tmp";
    const string zip_code_input = input_files.back();
    input_files.pop_back();
    mozc::ZipCodeDictionaryBuilder zip_code_builder(zip_code_input,
                                                    zip_code_text_dictionary);
    zip_code_builder.Build();
    input_files.push_back(zip_code_text_dictionary);
  }

  const string input = mozc::GetInputFileName(input_files);

  mozc::SystemDictionaryBuilder::Compile(input.c_str(),
                                         output.c_str());

  if (FLAGS_make_header) {
    const char kName[] = "DictionaryData";
    mozc::Util::MakeByteArrayFile(kName,
                                  output,
                                  FLAGS_output);
    mozc::Util::Unlink(output);
  }

  if (!zip_code_text_dictionary.empty()) {
    mozc::Util::Unlink(zip_code_text_dictionary);
  }

  return 0;
}
