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

// Single Kanji dictionary generator:
// % gen_emoticon_reewriter_dictionary_main
//    --input=input.tsv --output=output_header

#include <cmath>
#include <vector>
#include <string>
#include "base/base.h"
#include "base/file_stream.h"
#include "base/util.h"
#include "rewriter/dictionary_generator.h"
#include "rewriter/embedded_dictionary.h"

DEFINE_string(input, "", "emoticon dictionary file");
DEFINE_string(output, "", "output header file");

namespace mozc {
namespace {

// Read dic:
void ConvertEmoticonTsvToTextDictionary(
    const string &emoticon_dictionary_file,
    const string &output_file) {
  InputFileStream ifs(emoticon_dictionary_file.c_str());
  OutputFileStream ofs(output_file.c_str());

  CHECK(ifs);
  CHECK(ofs);

  string line;
  CHECK(getline(ifs, line));  // get first line
  const uint32 base_freq = 100000000;

  while (getline(ifs, line)) {
    // Format:
    // value <tab> readings(space delimitered) <tab> freq
    vector<string> fields;
    Util::SplitStringAllowEmpty(line, "\t", &fields);
    CHECK_GE(fields.size(), 2) << "format error: " << line;
    if (fields.size() < 3) {
      continue;
    }
    string keys_str;
    // \xE3\x80\x80 is full width space
    Util::StringReplace(fields[1], "\xE3\x80\x80", " ", true, &keys_str);
    vector<string> keys;
    Util::SplitStringUsing(keys_str, " ", &keys);

    const string &value = fields[0];

    const int32 freq = atoi(fields[2].c_str());
    CHECK_LE(freq, base_freq);
    CHECK_GT(freq, 0);
    const int32 cost = static_cast<int>(-500 * log(1.0 *  freq / base_freq));
    CHECK_GE(cost, 0);

    for (size_t i = 0; i < keys.size(); ++i) {
      ofs << keys[i] << "\t0\t0\t" << cost
          << "\t" << value << "\t" << value << "\t*" << endl;
    }
  }
}
}  // namespace
}  // mozc

int main(int argc, char **argv) {
  InitGoogle(argv[0], &argc, &argv, true);

  const string tmp_text_file = FLAGS_output + ".txt";
  const char kHeaderName[] = "EmoticonData";

  mozc::ConvertEmoticonTsvToTextDictionary(FLAGS_input,
                                           tmp_text_file);

  mozc::EmbeddedDictionary::Compile(kHeaderName,
                                    tmp_text_file,
                                    FLAGS_output);

  mozc::Util::Unlink(tmp_text_file);

  return 0;
}
