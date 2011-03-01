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
// % gen_emoticon_rewriter_dictionary_main
//    --input=input.tsv --output=output_header

#include <algorithm>
#include <cmath>
#include <map>
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

// Generate a description from readings. We simply add
// 1) the most general reading and 2) the most specific reading.
// 1) and 2) are simply approximated by checking the frequency
// of the readings.
string GetDescription(const vector<string> &keys,
                      const map<string, int> &key_count) {
  vector<pair<int, string> > freq;
  for (size_t i = 0; i < keys.size(); ++i) {
    const map<string, int>::const_iterator it = key_count.find(keys[i]);
    if (it != key_count.end()) {
      freq.push_back(make_pair(it->second, keys[i]));
    }
  }
  CHECK(!freq.empty());

  // use stable sort just in case.
  stable_sort(freq.begin(), freq.end());

  if (freq.size() >= 2) {
    return freq.back().second + " " + freq.front().second;
  } else {
    return freq.back().second;
  }

  return "";
}

// Read dic:
void ConvertEmoticonTsvToTextDictionary(
    const string &emoticon_dictionary_file,
    const string &output_file) {
  vector<pair<string, vector<string> > > emoticon_data;
  map<string, int> key_count;

  // Load data
  {
    InputFileStream ifs(emoticon_dictionary_file.c_str());
    CHECK(ifs);

    string line;
    CHECK(getline(ifs, line));  // get first line

    while (getline(ifs, line)) {
      // Format:
      // value <tab> readings(space delimitered)
      vector<string> fields;
      Util::SplitStringAllowEmpty(line, "\t", &fields);
      CHECK_GE(fields.size(), 2) << "format error: " << line;
      if (fields.size() > 3) {
        LOG(WARNING) << "ignore extra columns: " << line;
      }

      string keys_str;
      // \xE3\x80\x80 is full width space
      Util::StringReplace(fields[1], "\xE3\x80\x80", " ", true, &keys_str);
      vector<string> keys;
      Util::SplitStringUsing(keys_str, " ", &keys);
      const string &value = fields[0];
      emoticon_data.push_back(make_pair(value, keys));
      for (size_t j = 0; j < keys.size(); ++j) {
        key_count[keys[j]]++;
      }
    }
  }

  // Output dictionary
  {
    OutputFileStream ofs(output_file.c_str());
    CHECK(ofs);
    int cost = 10;
    for (size_t i = 0; i < emoticon_data.size(); ++i) {
      const string &value = emoticon_data[i].first;
      const vector<string> &keys = emoticon_data[i].second;
      const string description = GetDescription(keys, key_count);
      for (size_t i = 0; i < keys.size(); ++i) {
        ofs << keys[i] << "\t0\t0\t" << cost << "\t" << value;
        if (!description.empty()) {
          ofs << '\t'  << description;
        }
        ofs << endl;
      }
      cost += 10;
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
