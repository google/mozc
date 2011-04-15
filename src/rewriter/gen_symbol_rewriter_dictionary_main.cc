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

// Single Kanji dictionary generator:
// % gen_symbol_ewriter_dictionary_main
//    --sorting_table=sorting_table_file
//    --ordering_rule=ordering_rule_file
//    --input=input.tsv --output=output_header

#include <climits>
#include <map>
#include <vector>
#include <set>
#include <string>
#include <algorithm>
#include "base/base.h"
#include "base/file_stream.h"
#include "base/util.h"
#include "rewriter/dictionary_generator.h"
#include "rewriter/embedded_dictionary.h"

DEFINE_string(sorting_table, "", "sorting table file");
DEFINE_string(ordering_rule, "", "sorting order file");
DEFINE_string(input, "", "symbol dictionary file");
DEFINE_string(output, "", "output header file");

namespace mozc {
namespace {

void GetSortingMap(const string &auto_file,
                   const string &rule_file,
                   map<string, uint16> *sorting_map) {
  CHECK(sorting_map);
  sorting_map->clear();
  string line;
  int sorting_key = 0;
  mozc::InputFileStream rule_ifs(rule_file.c_str());
  CHECK(rule_ifs);
  while (getline(rule_ifs, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }
    sorting_map->insert(make_pair(line, sorting_key));
    ++sorting_key;
  }

  mozc::InputFileStream auto_ifs(auto_file.c_str());
  CHECK(auto_ifs);

  while (getline(auto_ifs, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }
    vector<string> fields;
    mozc::Util::SplitStringUsing(line, "\t ", &fields);
    CHECK_GE(fields.size(), 2);
    const uint32 ucs2 = strtol(fields[1].c_str(), NULL, 16);
    string utf8;
    mozc::Util::UCS2ToUTF8(ucs2, &utf8);
    if (sorting_map->find(utf8) != sorting_map->end()) {
      // ordered by rule
      continue;
    }
    sorting_map->insert(make_pair(utf8, sorting_key));
    ++sorting_key;
  }
}

void AddSymbolToDictionary(const string &pos,
                           const string &value,
                           const vector<string> &keys,
                           const string &description,
                           const string &additional_description,
                           const map<string, uint16> &sorting_map,
                           rewriter::DictionaryGenerator *dictionary) {
  // use first char of value as sorting key.
  const string first_value = mozc::Util::SubString(value, 0, 1);
  map<string, uint16>::const_iterator itr = sorting_map.find(first_value);
  uint16 sorting_key = 0;
  if (itr == sorting_map.end()) {
    DLOG(WARNING) << first_value << " is not defined in sorting map.";
    // If the character is platform-dependent, put the character at the last.
    const Util::CharacterSet cset = Util::GetCharacterSet(value);
    if (cset >= Util::JISX0212) {
      sorting_key = USHRT_MAX;
    }
  } else {
    sorting_key = itr->second;
  }

  for (size_t i = 0; i < keys.size(); ++i) {
    const string &key = keys[i];

    rewriter::Token token;
    token.set_sorting_key(sorting_key);
    token.set_key(key);
    token.set_value(value);
    token.set_pos(pos);
    token.set_description(description);
    token.set_additional_description(additional_description);
    dictionary->AddToken(token);

    string fw_key;
    mozc::Util::HalfWidthAsciiToFullWidthAscii(key, &fw_key);
    if (fw_key != key) {
      token.set_key(fw_key);
      dictionary->AddToken(token);
    }
  }
}

// Read dic:
void MakeDictionary(const string &symbol_dictionary_file,
                    const string &sorting_map_file,
                    const string &ordering_rule_file,
                    rewriter::DictionaryGenerator *dictionary) {
  set<string> seen;
  map<string, uint16> sorting_map;
  GetSortingMap(sorting_map_file, ordering_rule_file, &sorting_map);

  mozc::InputFileStream ifs(symbol_dictionary_file.c_str());
  CHECK(ifs);

  string line;
  CHECK(getline(ifs, line));  // get first line

  vector<string> fields;
  while (getline(ifs, line)) {
    fields.clear();
    // Format:
    // POS <tab> value <tab> readings(space delimitered) <tab>
    // description <tab> memo
    mozc::Util::SplitStringAllowEmpty(line, "\t", &fields);
    if (fields.size() < 3 ||
        (fields[1].empty() && fields[2].empty())) {
      VLOG(3) << "invalid format. skip line: " << line;
      continue;
    }
    string pos = fields[0];
    mozc::Util::UpperString(&pos);
    const string &value = fields[1];
    if (seen.find(value) != seen.end()) {
      LOG(WARNING) << "already inserted: " << value;
      continue;
    } else {
      seen.insert(value);
    }
    string keys_str;
    // \xE3\x80\x80 is full width space
    mozc::Util::StringReplace(fields[2], "\xE3\x80\x80", " ", true, &keys_str);
    vector<string> keys;
    mozc::Util::SplitStringUsing(keys_str, " ", &keys);
    const string &description = (fields.size()) > 3 ? fields[3] : "";
    const string &additional_description = (fields.size()) > 4 ? fields[4] : "";
    AddSymbolToDictionary(pos, value, keys, description,
                          additional_description,
                          sorting_map, dictionary);
  }
  // Add space as a symbol
  vector<string> keys_space;
  keys_space.push_back(" ");
  // "記号", "空白"
  AddSymbolToDictionary("\xe8\xa8\x98\xe5\x8f\xb7", " ", keys_space,
                        "\xe7\xa9\xba\xe7\x99\xbd", "", sorting_map,
                        dictionary);
}
}  // namespace
}  // mozc

int main(int argc, char **argv) {
  InitGoogle(argv[0], &argc, &argv, true);

  if ((FLAGS_input.empty() ||
       FLAGS_sorting_table.empty() ||
       FLAGS_ordering_rule.empty()) && argc > 3) {
    FLAGS_input = argv[1];
    FLAGS_sorting_table = argv[2];
    FLAGS_ordering_rule = argv[3];
  }

  const string tmp_text_file = FLAGS_output + ".txt";
  static const char kHeaderName[] = "SymbolData";

  mozc::rewriter::DictionaryGenerator dictionary;
  mozc::MakeDictionary(FLAGS_input,
                       FLAGS_sorting_table,
                       FLAGS_ordering_rule,
                       &dictionary);
  dictionary.Output(tmp_text_file);
  mozc::EmbeddedDictionary::Compile(kHeaderName,
                                    tmp_text_file,
                                    FLAGS_output);
  mozc::Util::Unlink(tmp_text_file);

  return 0;
}
