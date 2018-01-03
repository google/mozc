// Copyright 2010-2018, Google Inc.
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

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/file_stream.h"
#include "base/flags.h"
#include "base/init_mozc.h"
#include "base/logging.h"
#include "base/mozc_hash_map.h"
#include "base/string_piece.h"
#include "base/util.h"
#include "data_manager/serialized_dictionary.h"

DEFINE_string(input, "", "Emoticon dictionary file");
DEFINE_string(output_token_array, "", "Output token array");
DEFINE_string(output_string_array, "", "Output string array");

namespace mozc {
namespace {

using KeyList = std::vector<string>;
using CompilerToken = SerializedDictionary::CompilerToken;
using TokenList = SerializedDictionary::TokenList;

int LookupCount(const mozc_hash_map<string, int> &key_count,
                const string &key) {
  const auto iter = key_count.find(key);
  return (iter == key_count.end()) ? 0 : iter->second;
}

string GetDescription(const KeyList &key_list,
                      const mozc_hash_map<string, int> &key_count) {
  if (key_list.size() == 1) {
    return key_list[0];
  }
  KeyList sorted_key_list(key_list);
  std::sort(sorted_key_list.begin(), sorted_key_list.end(),
            [&key_count](const string &x, const string &y) {
              const int x_count = LookupCount(key_count, x);
              const int y_count = LookupCount(key_count, y);
              if (x_count == y_count) {
                return x < y;
              }
              return x_count < y_count;
            });
  return Util::StringPrintf("%s %s", sorted_key_list.back().c_str(),
                            sorted_key_list.front().c_str());
}

std::map<string, TokenList> ReadEmoticonTsv(const string &path) {
  InputFileStream ifs(path.c_str());

  string line;
  getline(ifs, line);  // Skip header

  std::vector<std::pair<string, KeyList>> data;
  mozc_hash_map<string, int> key_count;
  while (getline(ifs, line)) {
    std::vector<StringPiece> field_list;
    Util::SplitStringUsing(line, "\t", &field_list);
    CHECK_GE(field_list.size(), 2) << "Format error: " << line;
    LOG_IF(WARNING, field_list.size() > 3) << "Ignore extra columns: " << line;

    string replaced;
    Util::StringReplace(field_list[1], "　",  // Full-width space
                        " ", true, &replaced);
    KeyList key_list;
    Util::SplitStringUsing(field_list[1], " ", &key_list);

    data.emplace_back(field_list[0].as_string(), std::move(key_list));
    for (const auto &key : key_list) {
      ++key_count[key];
    }
  }

  std::map<string, TokenList> input_data;
  int16 cost = 10;
  for (const auto &kv : data) {
    const string &value = kv.first;
    const KeyList &key_list = kv.second;
    const string &description = GetDescription(key_list, key_count);
    for (const string &key : key_list) {
      std::unique_ptr<CompilerToken> token(new CompilerToken());
      token->value = value;
      token->description = description;
      token->lid = 0;
      token->rid = 0;
      token->cost = cost;
      input_data[key].push_back(std::move(token));
      cost += 10;
    }
  }

  return input_data;
}

}  // namespace
}  // namespace mozc

int main(int argc, char **argv) {
  mozc::InitMozc(argv[0], &argc, &argv, true);
  const auto &input_data = mozc::ReadEmoticonTsv(FLAGS_input);
  mozc::SerializedDictionary::CompileToFiles(
      input_data, FLAGS_output_token_array, FLAGS_output_string_array);
  return 0;
}
