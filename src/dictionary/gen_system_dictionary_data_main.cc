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

// Generates system dictionary header file.
//
// gen_system_dictionary_data_main
//  --input="dictionary0.txt dictionary1.txt zip_code_seed.txt"
//  --output="output.h"
//  --make_header
//
// Special input files are distinguished by prefixes of filenames
//   zip_code*            : zip code seed dictionary
//   spelling_correction* : spelling correction dictionary

#include <string>
#include <vector>
#include "base/base.h"
#include "base/util.h"
#include "dictionary/spelling_correction_dictionary_builder.h"
#include "dictionary/system/system_dictionary_builder.h"
#include "dictionary/zip_code_dictionary_builder.h"

DEFINE_string(input, "", "space separated input text files");
DEFINE_string(output, "", "output binary file");
DEFINE_bool(make_header, false, "make header mode");

namespace mozc {
namespace {
// In gyp build, file names will be given in unix style(separated by '/')
// even for windows.
// So we can not use Util::Basename().
// Here we assume that |file_path| contains '/' or '\' only for separater.
string GetBasenameWithAnyStyles(const string &file_path) {
  // First we try with unix style
  const string::size_type unix_style_separated_pos =
      file_path.find_last_of('/');
  if (unix_style_separated_pos != string::npos) {
    return file_path.substr(unix_style_separated_pos + 1);
  }
  // Try with windows style
  const string::size_type win_style_separated_pos =
      file_path.find_last_of('\\');
  if (win_style_separated_pos != string::npos) {
    return file_path.substr(win_style_separated_pos + 1);
  }
  // Return as-is |file_path| for the file in the current directory.
  return file_path;
}

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

  string zip_code_input;
  string spelling_correction_input;
  for (int i = 0; i < input_files.size(); ++i) {
    const string &file_path = input_files[i];
    const string file_name = mozc::GetBasenameWithAnyStyles(file_path);

    if (file_name.find("zip_code") == 0) {
      CHECK(zip_code_input.empty())
          << "Multiple zip code seed dictionaries are not supported";
      zip_code_input = file_path;
      input_files.erase(input_files.begin() + i--);
      LOG(INFO) << "zip code seed dictionary: " << zip_code_input;
    } else if (file_name.find("spelling_correction") == 0) {
      CHECK(spelling_correction_input.empty())
          << "Multiple spelling correction dictionaries are not supported";
      spelling_correction_input = file_path;
      input_files.erase(input_files.begin() + i--);
      LOG(INFO) << "spelling correction dictionary: "
                << spelling_correction_input;
    }
  }

  string zip_code_text_dictionary;
  if (!zip_code_input.empty()) {
    zip_code_text_dictionary = FLAGS_output + ".zip_code_tmp";
    mozc::ZipCodeDictionaryBuilder zip_code_builder(zip_code_input,
                                                    zip_code_text_dictionary);
    zip_code_builder.Build();
    input_files.push_back(zip_code_text_dictionary);
  }

  string spelling_correction_converted_dictionary;
  if (!spelling_correction_input.empty()) {
    spelling_correction_converted_dictionary
        = FLAGS_output + ".spelling_correction_tmp";
    mozc::SpellingCorrectionDictionaryBuilder
        spelling_correction_builder(spelling_correction_input,
                                    spelling_correction_converted_dictionary);
    spelling_correction_builder.Build();
    input_files.push_back(spelling_correction_converted_dictionary);
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
  if (!spelling_correction_converted_dictionary.empty()) {
    mozc::Util::Unlink(spelling_correction_converted_dictionary);
  }

  return 0;
}
