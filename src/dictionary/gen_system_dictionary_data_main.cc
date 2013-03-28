// Copyright 2010-2013, Google Inc.
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
//  --input="dictionary0.txt dictionary1.txt"
//  --output="output.h"
//  --make_header

#include <string>
#include <vector>

#include "base/base.h"
#include "base/codegen_bytearray_stream.h"
#include "base/file_stream.h"
#include "base/util.h"
#include "data_manager/user_pos_manager.h"
#include "dictionary/system/system_dictionary_builder.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/text_dictionary_loader.h"

DEFINE_string(input, "", "space separated input text files");
DEFINE_string(output, "", "output binary file");
DEFINE_bool(make_header, false, "make header mode");


namespace mozc {
namespace {

// 10 dictionary files are passed to this program with --input flag.
// reading_correction.tsv is also passed to this program with --input flag
// in the same manner. This program checks the file name pattern and change
// the algorithm for handling dictionaries. Ideally, we want to use different
// flags for dictionary and reading correction, but due to the limitation
// of internal build system, it turned out that the description of the rules
// will become much complicated, if we use two flags.
const char kReadingCorrectionFile[] = "reading_correction.tsv";

// convert space delimtered text to CSV
void GetInputFileName(const string &input_file,
                      string *system_dictionary_input,
                      string *reading_correction_input) {
  CHECK(system_dictionary_input);
  CHECK(reading_correction_input);
  system_dictionary_input->clear();
  reading_correction_input->clear();
  const StringPiece kDelimiter(", ", 1);
  for (SplitIterator<SingleDelimiter> iter(input_file, " ");
       !iter.Done(); iter.Next()) {
    const StringPiece &input_file = iter.Get();
    if (Util::EndsWith(input_file, kReadingCorrectionFile)) {
      Util::AppendStringWithDelimiter(kDelimiter, input_file,
                                      reading_correction_input);
    } else {
      Util::AppendStringWithDelimiter(kDelimiter, input_file,
                                      system_dictionary_input);
    }
  }
}

}  // namespace
}  // namespace mozc

int main(int argc, char **argv) {
  InitGoogle(argv[0], &argc, &argv, false);

  string system_dictionary_input, reading_correction_input;
  mozc::GetInputFileName(FLAGS_input,
                         &system_dictionary_input,
                         &reading_correction_input);

  const mozc::POSMatcher *pos_matcher =
      mozc::UserPosManager::GetUserPosManager()->GetPOSMatcher();
  CHECK(pos_matcher);

  mozc::TextDictionaryLoader loader(*pos_matcher);
  loader.Load(system_dictionary_input, reading_correction_input);

  mozc::dictionary::SystemDictionaryBuilder builder;
  builder.BuildFromTokens(loader.tokens());

  scoped_ptr<ostream> output_stream(
      new mozc::OutputFileStream(FLAGS_output.c_str(),
                                 FLAGS_make_header
                                 ? ios::out
                                 : ios::out | ios::binary));
  if (FLAGS_make_header) {
    mozc::CodeGenByteArrayOutputStream *codegen_stream;
    output_stream.reset(
        codegen_stream = new mozc::CodeGenByteArrayOutputStream(
            output_stream.release(),
            mozc::codegenstream::OWN_STREAM));
    codegen_stream->OpenVarDef("DictionaryData");
  }

  builder.WriteToStream(FLAGS_output, output_stream.get());

  return 0;
}
