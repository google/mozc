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

// Single Kanji dictionary generator:
// % gen_symbol_ewriter_dictionary_main
//    --sorting_table=sorting_table_file
//    --ordering_rule=ordering_rule_file
//    --input=input.tsv
//    --user_pos_manager_data=user_pos_manager.data
//    --output_token_array=output_token_file
//    --output_string_array=output_array_file

#include <climits>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/file/temp_dir.h"
#include "base/file_stream.h"
#include "base/init_mozc.h"
#include "base/strings/japanese.h"
#include "base/util.h"
#include "base/vlog.h"
#include "data_manager/data_manager.h"
#include "data_manager/serialized_dictionary.h"
#include "rewriter/dictionary_generator.h"

ABSL_FLAG(std::string, sorting_table, "", "sorting table file");
ABSL_FLAG(std::string, ordering_rule, "", "sorting order file");
ABSL_FLAG(std::string, input, "", "symbol dictionary file");
ABSL_FLAG(std::string, user_pos_manager_data, "", "user pos manager data file");
ABSL_FLAG(std::string, output_token_array, "",
          "output token array binary file");
ABSL_FLAG(std::string, output_string_array, "",
          "output string array binary file");

namespace mozc {
namespace {

using SortingKeyMap = absl::flat_hash_map<std::string, uint16_t>;

SortingKeyMap CreateSortingKeyMap(const std::string& auto_file,
                                  const std::string& rule_file) {
  SortingKeyMap sorting_keys;
  std::string line;
  int sorting_key = 0;
  InputFileStream rule_ifs(rule_file);
  CHECK(rule_ifs.good());
  while (!std::getline(rule_ifs, line).fail()) {
    if (line.empty() ||
        // Support comment and single '#'
        (line[0] == '#' && line.size() > 1)) {
      continue;
    }
    sorting_keys.emplace(std::move(line), sorting_key);
    line.clear();
    ++sorting_key;
  }

  InputFileStream auto_ifs(auto_file);
  CHECK(auto_ifs.good());

  while (!std::getline(auto_ifs, line).fail()) {
    if (line.empty() || line[0] == '#') {
      continue;
    }
    const std::vector<absl::string_view> fields =
        absl::StrSplit(line, absl::ByAnyChar("\t "), absl::SkipEmpty());
    CHECK_GE(fields.size(), 2);
    uint32_t codepoint = 0;
    CHECK(absl::SimpleHexAtoi(fields[0], &codepoint));
    const std::string utf8 = Util::CodepointToUtf8(codepoint);
    if (sorting_keys.contains(utf8)) {
      // ordered by rule
      continue;
    }
    sorting_keys.emplace(std::move(utf8), sorting_key);
    ++sorting_key;
  }
  return sorting_keys;
}

void AddSymbolToDictionary(const absl::string_view pos,
                           const absl::string_view value,
                           const absl::Span<const std::string> keys,
                           const absl::string_view description,
                           const absl::string_view additional_description,
                           const SortingKeyMap& sorting_keys,
                           rewriter::DictionaryGenerator& dictionary) {
  // use first char of value as sorting key.
  const absl::string_view first_value = Util::Utf8SubString(value, 0, 1);
  const auto it = sorting_keys.find(first_value);
  uint16_t sorting_key = 0;
  if (it == sorting_keys.end()) {
    DLOG(WARNING) << first_value << " is not defined in sorting map.";
    // If the character is platform-dependent, put the character at the last.
    if (!Util::IsJisX0208(value)) {
      sorting_key = USHRT_MAX;
    }
  } else {
    sorting_key = it->second;
  }

  for (const std::string& key : keys) {
    const rewriter::Token& token = dictionary.AddToken(
        {sorting_key, key, std::string(value), std::string(pos),
         std::string(description), std::string(additional_description)});

    std::string fw_key = japanese::HalfWidthAsciiToFullWidthAscii(token.key);
    if (fw_key != key) {
      rewriter::Token fw_token = token;
      fw_token.key = std::move(fw_key);
      dictionary.AddToken(std::move(fw_token));
    }
  }
}

// Read dic:
void MakeDictionary(const std::string& symbol_dictionary_file,
                    const std::string& sorting_map_file,
                    const std::string& ordering_rule_file,
                    rewriter::DictionaryGenerator& dictionary) {
  absl::flat_hash_set<std::string> seen;
  SortingKeyMap sorting_keys =
      CreateSortingKeyMap(sorting_map_file, ordering_rule_file);

  InputFileStream ifs(symbol_dictionary_file);
  CHECK(ifs.good());

  std::string line;
  CHECK(!std::getline(ifs, line).fail());  // get first line

  while (!std::getline(ifs, line).fail()) {
    // Format:
    // POS <tab> value <tab> readings(space delimited) <tab>
    // description <tab> memo
    std::vector<absl::string_view> fields =
        absl::StrSplit(line, '\t', absl::AllowEmpty());
    if (fields.size() < 3 || (fields[1].empty() && fields[2].empty())) {
      MOZC_VLOG(3) << "invalid format. skip line: " << line;
      continue;
    }
    std::string pos(fields[0]);
    Util::UpperString(&pos);
    const absl::string_view value = fields[1];
    if (seen.contains(value)) {
      LOG(WARNING) << "already inserted: " << value;
      continue;
    } else {
      seen.emplace(value);
    }

    std::vector<std::string> keys;
    for (const absl::string_view key :
         absl::StrSplit(fields[2], ' ', absl::SkipEmpty())) {
      keys.push_back(
          absl::StrReplaceAll(key, {{"　", " "}}));  // Full-width space
    }
    const absl::string_view description = fields.size() > 3 ? fields[3] : "";
    const absl::string_view additional_description =
        fields.size() > 4 ? fields[4] : "";
    AddSymbolToDictionary(pos, value, keys, description, additional_description,
                          sorting_keys, dictionary);
  }
  // Add space as a symbol
  AddSymbolToDictionary("記号", " ", {" "}, "空白", "", sorting_keys,
                        dictionary);
}
}  // namespace
}  // namespace mozc

int main(int argc, char** argv) {
  mozc::InitMozc(argv[0], &argc, &argv);

  if ((absl::GetFlag(FLAGS_input).empty() ||
       absl::GetFlag(FLAGS_sorting_table).empty() ||
       absl::GetFlag(FLAGS_ordering_rule).empty()) &&
      argc > 3) {
    absl::SetFlag(&FLAGS_input, argv[1]);
    absl::SetFlag(&FLAGS_sorting_table, argv[2]);
    absl::SetFlag(&FLAGS_ordering_rule, argv[3]);
  }

  absl::StatusOr<mozc::TempFile> tmp_text_file =
      mozc::TempDirectory::Default().CreateTempFile();
  CHECK_OK(tmp_text_file);

  // User pos manager data for build tools has no magic number.
  constexpr absl::string_view kMagicNumber = "";
  absl::StatusOr<std::unique_ptr<const mozc::DataManager>> data_manager =
      mozc::DataManager::CreateUserPosManagerDataFromFile(
          absl::GetFlag(FLAGS_user_pos_manager_data), kMagicNumber);
  CHECK_OK(data_manager);

  mozc::rewriter::DictionaryGenerator dictionary(*data_manager.value());
  mozc::MakeDictionary(absl::GetFlag(FLAGS_input),
                       absl::GetFlag(FLAGS_sorting_table),
                       absl::GetFlag(FLAGS_ordering_rule), dictionary);
  {
    mozc::OutputFileStream ofs(tmp_text_file->path());
    if (!ofs) {
      LOG(ERROR) << "Failed to open: " << tmp_text_file->path();
      return 1;
    }
    dictionary.Output(ofs);
    CHECK(ofs.good());
  }
  mozc::SerializedDictionary::CompileToFiles(
      tmp_text_file->path(), absl::GetFlag(FLAGS_output_token_array),
      absl::GetFlag(FLAGS_output_string_array));

  return 0;
}
