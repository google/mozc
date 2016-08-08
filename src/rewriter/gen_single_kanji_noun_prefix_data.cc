// Copyright 2010-2016, Google Inc.
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

#include <map>
#include <string>

#include "base/flags.h"
#include "base/init_mozc.h"
#include "base/port.h"
#include "data_manager/serialized_dictionary.h"

DEFINE_string(output_token_array, "",
              "Output token array of noun prefix dictionary");
DEFINE_string(output_string_array, "",
              "Output string array of noun prefix dictionary");

namespace {

struct NounPrefix {
  const char *key;
  const char *value;
  int16 rank;
} kNounPrefixList[] = {
    // "お", "お"
    {"\xe3\x81\x8a", "\xe3\x81\x8a", 1},
    // "ご", "ご"
    {"\xe3\x81\x94", "\xe3\x81\x94", 1},
    // {"ご", "誤"},    // don't register it as 誤 isn't in the ipadic.
    // {"み", "み"},    // seems to be rare.
    // "もと", "もと"
    {"\xe3\x82\x82\xe3\x81\xa8", "\xe3\x82\x82\xe3\x81\xa8", 1},
    // "だい", "代"
    {"\xe3\x81\xa0\xe3\x81\x84", "\xe4\xbb\xa3", 1},
    // "てい", "低"
    {"\xe3\x81\xa6\xe3\x81\x84", "\xe4\xbd\x8e", 0},
    // "もと", "元"
    {"\xe3\x82\x82\xe3\x81\xa8", "\xe5\x85\x83", 1},
    // "ぜん", "全"
    {"\xe3\x81\x9c\xe3\x82\x93", "\xe5\x85\xa8", 0},
    // "さい", "再"
    {"\xe3\x81\x95\xe3\x81\x84", "\xe5\x86\x8d", 0},
    // "しょ", "初"
    {"\xe3\x81\x97\xe3\x82\x87", "\xe5\x88\x9d", 1},
    // "はつ", "初"
    {"\xe3\x81\xaf\xe3\x81\xa4", "\xe5\x88\x9d", 0},
    // "ぜん", "前"
    {"\xe3\x81\x9c\xe3\x82\x93", "\xe5\x89\x8d", 1},
    // "かく", "各"
    {"\xe3\x81\x8b\xe3\x81\x8f", "\xe5\x90\x84", 1},
    // "どう", "同"
    {"\xe3\x81\xa9\xe3\x81\x86", "\xe5\x90\x8c", 1},
    // "だい", "大"
    {"\xe3\x81\xa0\xe3\x81\x84", "\xe5\xa4\xa7", 1},
    // "おお", "大"
    {"\xe3\x81\x8a\xe3\x81\x8a", "\xe5\xa4\xa7", 1},
    // "とう", "当"
    {"\xe3\x81\xa8\xe3\x81\x86", "\xe5\xbd\x93", 1},
    // "ご", "御"
    {"\xe3\x81\x94", "\xe5\xbe\xa1", 1},
    // "お", "御"
    {"\xe3\x81\x8a", "\xe5\xbe\xa1", 1},
    // "しん", "新"
    {"\xe3\x81\x97\xe3\x82\x93", "\xe6\x96\xb0", 1},
    // "さい", "最"
    {"\xe3\x81\x95\xe3\x81\x84", "\xe6\x9c\x80", 1},
    // "み", "未"
    {"\xe3\x81\xbf", "\xe6\x9c\xaa", 0},
    // "ほん", "本"
    {"\xe3\x81\xbb\xe3\x82\x93", "\xe6\x9c\xac", 1},
    // "む", "無"
    {"\xe3\x82\x80", "\xe7\x84\xa1", 0},
    // "だい", "第"
    {"\xe3\x81\xa0\xe3\x81\x84", "\xe7\xac\xac", 1},
    // "とう", "等"
    {"\xe3\x81\xa8\xe3\x81\x86", "\xe7\xad\x89", 1},
    // "やく", "約"
    {"\xe3\x82\x84\xe3\x81\x8f", "\xe7\xb4\x84", 1},
    // "ひ", "被"
    {"\xe3\x81\xb2", "\xe8\xa2\xab", 1},
    // "ちょう", "超"
    {"\xe3\x81\xa1\xe3\x82\x87\xe3\x81\x86", "\xe8\xb6\x85", 1},
    // "ちょう", "長"
    {"\xe3\x81\xa1\xe3\x82\x87\xe3\x81\x86", "\xe9\x95\xb7", 1},
    // "なが", "長"
    {"\xe3\x81\xaa\xe3\x81\x8c", "\xe9\x95\xb7", 1},
    // "ひ", "非"
    {"\xe3\x81\xb2", "\xe9\x9d\x9e", 1},
    // "こう", "高"
    {"\xe3\x81\x93\xe3\x81\x86", "\xe9\xab\x98", 1},
};

}  // namespace

int main(int argc, char **argv) {
  mozc::InitMozc(argv[0], &argc, &argv, true);

  map<string, mozc::SerializedDictionary::TokenList> tokens;
  for (const NounPrefix &entry : kNounPrefixList) {
    std::unique_ptr<mozc::SerializedDictionary::CompilerToken> token(
        new mozc::SerializedDictionary::CompilerToken);
    token->value = entry.value;
    token->lid = 0;
    token->rid = 0;
    token->cost = entry.rank;
    tokens[entry.key].emplace_back(std::move(token));
  }
  mozc::SerializedDictionary::CompileToFiles(tokens,
                                             FLAGS_output_token_array,
                                             FLAGS_output_string_array);
  return 0;
}
