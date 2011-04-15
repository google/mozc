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

#include <string>
#include <vector>
#include "base/base.h"
#include "base/file_stream.h"
#include "base/text_converter.h"
#include "third_party/darts/v0_32/darts.h"

DEFINE_string(input, "", "input");
DEFINE_string(output, "", "output");

namespace mozc {

// files = "file_name1:name1,file_name2,name2 ... "
// Load Suikyo Format
static void Compile(const string &files,
                    const string &header_filename) {
  OutputFileStream ofs(header_filename.c_str());
  CHECK(ofs);

  ofs << "namespace {" << endl;

  vector<pair<string, string> > rules;
  {
    vector<string> patterns;
    vector<string> col;
    mozc::Util::SplitStringUsing(files, ",", &patterns);
    for (size_t i = 0; i < patterns.size(); ++i) {
      col.clear();
      mozc::Util::SplitStringUsing(patterns[i], ":", &col);
      CHECK_GE(col.size(), 2);
      rules.push_back(make_pair(col[0], col[1]));
    }
  }

  CHECK_GE(rules.size(), 1);

  for (size_t i = 0; i < rules.size(); ++i) {
    const string &filename = rules[i].first;
    const string &name = rules[i].second;
    InputFileStream ifs(filename.c_str());
    CHECK(ifs);
    vector<string> col;
    string line, output;
    vector<pair<string, int> > dic;
    while (getline(ifs, line)) {
      col.clear();
      mozc::Util::SplitStringUsing(line, "\t", &col);
      CHECK_GE(col.size(), 2) << "format error: " << line;
      const string &key = col[0];
      const string &value = col[1];
      size_t rewind_len = 0;
      if (col.size() > 2) {
        rewind_len = col[2].size();
      }
      CHECK_LT(rewind_len, 256) << "rewind length must be < 256";
      CHECK_GT(key.size(), rewind_len) << "rewind_length < key.size()";
      dic.push_back(pair<string, int>
                    (key,
                     static_cast<int>(output.size())));
      output += value;
      output += '\0';
      output += static_cast<uint8>(rewind_len);
    }
    sort(dic.begin(), dic.end());
    vector<char *> ary;
    vector<int> values;
    for (size_t k = 0; k < dic.size(); ++k) {
      ary.push_back(const_cast<char *>(dic[k].first.c_str()));
      values.push_back(dic[k].second);
    }

    Darts::DoubleArray da;
    CHECK_EQ(0, da.build(dic.size(), const_cast<const char **>(&ary[0]), 0, &values[0]));

    string escaped;
    Util::Escape(output, &escaped);
    ofs << "static const char " << name
        << "_table[] = \"" << escaped << "\";" << endl;

    const TextConverter::DoubleArray *array =
        reinterpret_cast<const TextConverter::DoubleArray *>(da.array());
    ofs << "static const mozc::TextConverter::DoubleArray "
        << name << "_da[] = {";
    for (size_t k = 0; k < da.size(); ++k) {
      if (k != 0) ofs << ",";
      ofs << "{" << array[k].base << "," << array[k].check << "}";
    }
    ofs << "};" << endl;
  }
  ofs << "}" << endl;
}

}  // namespace mozc

int main(int argc, char **argv) {
  InitGoogle(argv[0], &argc, &argv, false);
  mozc::Compile(FLAGS_input, FLAGS_output);
  return 0;
}
