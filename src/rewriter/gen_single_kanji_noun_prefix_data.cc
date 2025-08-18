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

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "absl/flags/flag.h"
#include "base/init_mozc.h"
#include "data_manager/serialized_dictionary.h"

ABSL_FLAG(std::string, output_token_array, "",
          "Output token array of noun prefix dictionary");
ABSL_FLAG(std::string, output_string_array, "",
          "Output string array of noun prefix dictionary");

namespace {

struct NounPrefix {
  const char* key;
  const char* value;
  int16_t rank;
} kNounPrefixList[] = {
    {"お", "お", 1},
    {"ご", "ご", 1},
    // {"ご", "誤"},    // don't register it as 誤 isn't in the ipadic.
    // {"み", "み"},    // seems to be rare.
    {"もと", "もと", 1},
    {"だい", "代", 1},
    {"てい", "低", 0},
    {"もと", "元", 1},
    {"ぜん", "全", 0},
    {"さい", "再", 0},
    {"しょ", "初", 1},
    {"はつ", "初", 0},
    {"ぜん", "前", 1},
    {"かく", "各", 1},
    {"どう", "同", 1},
    {"だい", "大", 1},
    {"おお", "大", 1},
    {"とう", "当", 1},
    {"ご", "御", 1},
    {"お", "御", 1},
    {"しん", "新", 1},
    {"さい", "最", 1},
    {"み", "未", 0},
    {"ほん", "本", 1},
    {"む", "無", 0},
    {"だい", "第", 1},
    {"とう", "等", 1},
    {"やく", "約", 1},
    {"ひ", "被", 1},
    {"ちょう", "超", 1},
    {"ちょう", "長", 1},
    {"なが", "長", 1},
    {"ひ", "非", 1},
    {"こう", "高", 1},
};

}  // namespace

int main(int argc, char** argv) {
  mozc::InitMozc(argv[0], &argc, &argv);

  std::map<std::string, mozc::SerializedDictionary::TokenList> tokens;
  for (const NounPrefix& entry : kNounPrefixList) {
    std::unique_ptr<mozc::SerializedDictionary::CompilerToken> token(
        new mozc::SerializedDictionary::CompilerToken);
    token->value = entry.value;
    token->lid = 0;
    token->rid = 0;
    token->cost = entry.rank;
    tokens[entry.key].emplace_back(std::move(token));
  }
  mozc::SerializedDictionary::CompileToFiles(
      tokens, absl::GetFlag(FLAGS_output_token_array),
      absl::GetFlag(FLAGS_output_string_array));
  return 0;
}
