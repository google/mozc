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

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "base/file_stream.h"
#include "base/init_mozc.h"
#include "data_manager/serialized_dictionary.h"

ABSL_FLAG(std::string, input, "", "Emoticon dictionary file");
ABSL_FLAG(std::string, output_token_array, "", "Output token array");
ABSL_FLAG(std::string, output_string_array, "", "Output string array");

namespace mozc {
namespace {

using KeyList = std::vector<std::string>;
using CompilerToken = SerializedDictionary::CompilerToken;
using TokenList = SerializedDictionary::TokenList;

int LookupCount(const absl::flat_hash_map<std::string, int>& key_count,
                const absl::string_view key) {
  const auto iter = key_count.find(key);
  return (iter == key_count.end()) ? 0 : iter->second;
}

std::string GetDescription(
    const KeyList& key_list,
    const absl::flat_hash_map<std::string, int>& key_count) {
  if (key_list.size() == 1) {
    return key_list[0];
  }
  KeyList sorted_key_list(key_list);
  std::sort(sorted_key_list.begin(), sorted_key_list.end(),
            [&key_count](const absl::string_view x, const absl::string_view y) {
              const int x_count = LookupCount(key_count, x);
              const int y_count = LookupCount(key_count, y);
              if (x_count == y_count) {
                return x < y;
              }
              return x_count < y_count;
            });
  return absl::StrFormat("%s %s", sorted_key_list.back(),
                         sorted_key_list.front());
}

std::map<std::string, TokenList> ReadEmoticonTsv(const std::string& path) {
  InputFileStream ifs(path);

  std::string line;
  std::getline(ifs, line);  // Skip header

  std::vector<std::pair<std::string, KeyList>> data;
  absl::flat_hash_map<std::string, int> key_count;
  while (std::getline(ifs, line)) {
    const std::vector<absl::string_view> field_list =
        absl::StrSplit(line, '\t', absl::SkipEmpty());
    CHECK_GE(field_list.size(), 2) << "Format error: " << line;
    LOG_IF(WARNING, field_list.size() > 3) << "Ignore extra columns: " << line;

    std::string replaced =
        absl::StrReplaceAll(field_list[1], {{"　", " "}});  // Full-width space
    KeyList key_list = absl::StrSplit(field_list[1], ' ', absl::SkipEmpty());

    for (const auto& key : key_list) {
      ++key_count[key];
    }
    data.emplace_back(field_list[0], std::move(key_list));
  }

  std::map<std::string, TokenList> input_data;
  int16_t cost = 10;
  for (const auto& kv : data) {
    const std::string& value = kv.first;
    const KeyList& key_list = kv.second;
    const std::string& description = GetDescription(key_list, key_count);
    for (const std::string& key : key_list) {
      std::unique_ptr<CompilerToken> token = std::make_unique<CompilerToken>();
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

int main(int argc, char** argv) {
  mozc::InitMozc(argv[0], &argc, &argv);
  const auto& input_data = mozc::ReadEmoticonTsv(absl::GetFlag(FLAGS_input));
  mozc::SerializedDictionary::CompileToFiles(
      input_data, absl::GetFlag(FLAGS_output_token_array),
      absl::GetFlag(FLAGS_output_string_array));
  return 0;
}
