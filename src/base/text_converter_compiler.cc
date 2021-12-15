// Copyright 2010-2021, Google Inc.
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

// # Usage
// base/text_converter_compiler
// --output=base/japanese_util_rule.cc
// --input=
//   "data/preedit/hiragana-katakana.tsv:hiragana_to_katakana,
//    data/preedit/hiragana-romanji.tsv:hiragana_to_romanji,
//    data/preedit/katakana-hiragana.tsv:katakana_to_hiragana,
//    data/preedit/romanji-hiragana.tsv:romanji_to_hiragana,
//    data/preedit/fullwidthkatakana-halfwidthkatakana.tsv:fullwidthkatakana_to_halfwidthkatakana,
//    data/preedit/halfwidthkatakana-fullwidthkatakana.tsv:halfwidthkatakana_to_fullwidthkatakana,
//    data/preedit/halfwidthascii-fullwidthascii.tsv:halfwidthascii_to_fullwidthascii,
//    data/preedit/fullwidthascii-halfwidthascii.tsv:fullwidthascii_to_halfwidthascii,
//    data/preedit/normalize-voiced-sound.tsv:normalize_voiced_sound,
//    data/preedit/kanjinumber-arabicnumber.tsv:kanjinumber_to_arabicnumber"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "base/double_array.h"
#include "base/file_stream.h"
#include "base/init_mozc.h"
#include "base/logging.h"
#include "base/util.h"
#include "absl/flags/flag.h"
#include "absl/strings/str_split.h"
#include "third_party/darts/v0_32/darts.h"

ABSL_FLAG(std::string, input, "", "input");
ABSL_FLAG(std::string, output, "", "output");

namespace mozc {

// files = "file_name1:name1,file_name2,name2 ... "
// Load Suikyo Format
static void Compile(const std::string &files,
                    const std::string &header_filename) {
  OutputFileStream ofs(header_filename.c_str());
  CHECK(ofs.good());

  ofs << "#include \"base/japanese_util_rule.h\"\n"
      << "\n"
      << "namespace mozc {\n"
      << "namespace japanese_util_rule {\n"
      << std::endl;

  std::vector<std::pair<std::string, std::string>> rules;
  {
    for (absl::string_view pattern :
         absl::StrSplit(files, ',', absl::SkipEmpty())) {
      std::vector<std::string> col =
          absl::StrSplit(pattern, ':', absl::SkipEmpty());
      CHECK_GE(col.size(), 2);
      rules.emplace_back(std::move(col[0]), std::move(col[1]));
    }
  }

  CHECK_GE(rules.size(), 1);

  for (size_t i = 0; i < rules.size(); ++i) {
    const std::string &filename = rules[i].first;
    const std::string &name = rules[i].second;
    InputFileStream ifs(filename.c_str());
    CHECK(ifs.good());
    std::string line, output;
    std::vector<std::pair<std::string, int>> dic;
    while (!std::getline(ifs, line).fail()) {
      std::vector<std::string> col =
          absl::StrSplit(line, '\t', absl::SkipEmpty());
      CHECK_GE(col.size(), 2) << "format error: " << line;
      const std::string &key = col[0];
      const std::string &value = col[1];
      size_t rewind_len = 0;
      if (col.size() > 2) {
        rewind_len = col[2].size();
      }
      CHECK_LT(rewind_len, 256) << "rewind length must be < 256";
      CHECK_GT(key.size(), rewind_len) << "rewind_length < key.size()";
      dic.emplace_back(key, static_cast<int>(output.size()));
      output += value;
      output += '\0';
      output += static_cast<uint8_t>(rewind_len);
    }
    std::sort(dic.begin(), dic.end());
    std::vector<char *> ary;
    std::vector<int> values;
    for (size_t k = 0; k < dic.size(); ++k) {
      ary.push_back(const_cast<char *>(dic[k].first.c_str()));
      values.push_back(dic[k].second);
    }

    Darts::DoubleArray da;
    const int result = da.build(dic.size(), const_cast<const char **>(&ary[0]),
                                nullptr, &values[0]);
    CHECK_EQ(0, result);

    std::string escaped;
    Util::Escape(output, &escaped);
    ofs << "const char " << name << "_table[] = \"" << escaped << "\";"
        << std::endl;

    const japanese_util_rule::DoubleArray *array =
        reinterpret_cast<const japanese_util_rule::DoubleArray *>(da.array());
    ofs << "const DoubleArray " << name << "_da[] = {";
    for (size_t k = 0; k < da.size(); ++k) {
      if (k != 0) ofs << ",";
      ofs << "{" << array[k].base << "," << array[k].check << "}";
    }
    ofs << "};" << std::endl;
  }

  ofs << "\n"
      << "}  // namespace japanese_util_rule\n"
      << "}  // namespace mozc" << std::endl;
}

}  // namespace mozc

int main(int argc, char **argv) {
  mozc::InitMozc(argv[0], &argc, &argv);
  mozc::Compile(absl::GetFlag(FLAGS_input), absl::GetFlag(FLAGS_output));
  return 0;
}
