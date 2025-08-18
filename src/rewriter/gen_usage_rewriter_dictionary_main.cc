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

// * Usage
// % gen_usage_rewriter_dictionary_main
//    --usage_data_file=usage_data.txt
//    --cforms_file=cforms.def
//    --output_base_conjugation_suffix=base_conj_suffix.data
//    --output_conjugation_suffix=conj_suffix.data
//    --output_conjugation_index=conj_index.data
//    --output_usage_item_array=usage_item_array.data
//    --output_string_array=string_array.data
//
// * Prerequisite
// Little endian is assumed.
//
// * Output file format
// The output data consists of five files:
//
// ** String array
// All the strings (e.g., usage of word) are stored in this array and are
// referenced by index to this array in oterh data.  The array is serialized by
// SerializedStringArray.
//
// ** Base conjugation suffix
// Array of uint32_t indices to the string array for base forms of conjugation
// suffixes.  Value and key suffixes are stored as follows:
//
// | value_suffix[0] | key_suffix[0] | value_suffix[1] | key_suffix[1] |...
//
// So, this array has 2*N elements, where N is the number of base suffixes.
// Suffix strings can be retrieved from the string array using these indices.
//
// ** Conjugation suffix
// This data has the same format as the base conjugation suffix above, but it
// stores suffix indices for all the conjugation suffixes.
//
// ** Conjugation suffix index
//
// Array of uint32_t indices sorted in ascending order.  This array represents a
// partition of the conjugation suffix, where the range [array[i], array[i + 1])
// of conjugation suffix data stores the suffix information of i-th conjugation
// type.
//
// ** Usage item array

// This is an array of usage dictionary entries.  Each entry consists of 5
// uint32_t values and has the following layout:
//
// +=============================+
// | Usage ID (4 byte)           |
// +-----------------------------+
// | Value index (4 byte)        |
// +-----------------------------+
// | Key index (4 byte)          |
// +-----------------------------+
// | Conjugation index (4 byte)  |
// +-----------------------------+
// | Meaning index (4 byte)      |
// +=============================+
//
// Thus, the total byte length of usage item array is 2 * M, where M is the
// number of usage dictionary entries.  Here, value, key and meaning are indices
// to the string array.  Usage ID is the unique ID of this entry.  Conjugation
// index is the conjugation type of this key value pair, and its conjugation
// suffix types are retrieved using conjugation suffix index and conjugation
// suffix array.

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <ios>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/container/serialized_string_array.h"
#include "base/file_stream.h"
#include "base/init_mozc.h"

ABSL_FLAG(std::string, usage_data_file, "", "usage data file");
ABSL_FLAG(std::string, cforms_file, "", "cforms file");
ABSL_FLAG(std::string, output_base_conjugation_suffix, "",
          "output base conjugation suffix array");
ABSL_FLAG(std::string, output_conjugation_suffix, "",
          "output conjugation suffix array");
ABSL_FLAG(std::string, output_conjugation_index, "",
          "output conjugation index array");
ABSL_FLAG(std::string, output_usage_item_array, "",
          "output array of usage items");
ABSL_FLAG(std::string, output_string_array, "", "output string array");

namespace mozc {
namespace {
struct ConjugationType {
  std::string form;
  std::string value_suffix;
  std::string key_suffix;
};

struct UsageItem {
  std::string key;
  std::string value;
  std::string conjugation;
  int conjugation_id;
  std::string meaning;
};

bool UsageItemKeynameCmp(const UsageItem& l, const UsageItem& r) {
  return l.key < r.key;
}

// Load cforms_file
void LoadConjugation(
    const std::string& filename,
    absl::btree_map<std::string, std::vector<ConjugationType>>* output,
    std::map<std::string, ConjugationType>* baseform_map) {
  InputFileStream ifs(filename);
  CHECK(ifs.good());

  std::string line;
  while (!std::getline(ifs, line).fail()) {
    if (line.empty() || line[0] == '#') {
      continue;
    }
    const std::vector<std::string> fields =
        absl::StrSplit(line, absl::ByAnyChar("\t "), absl::SkipEmpty());
    CHECK_GE(fields.size(), 4) << "format error: " << line;

    ConjugationType tmp;
    tmp.form = fields[1];
    tmp.value_suffix = ((fields[2] == "*") ? "" : fields[2]);
    tmp.key_suffix = ((fields[3] == "*") ? "" : fields[3]);
    (*output)[fields[0]].push_back(tmp);  // insert

    if (tmp.form == "基本形") {
      (*baseform_map)[fields[0]] = tmp;
    }
  }
}

// Load usage_data_file
void LoadUsage(const std::string& filename,
               std::vector<UsageItem>* usage_entries,
               std::vector<std::string>* conjugation_list) {
  InputFileStream ifs(filename);

  if (!ifs.good()) {
    LOG(WARNING) << "Can't open file:" << filename;
    return;
  }

  std::string line;
  absl::btree_map<std::string, int> conjugation_id_map;

  int conjugation_id = 0;
  while (!std::getline(ifs, line).fail()) {
    if (line.empty() || line[0] == '#') {
      // starting with '#' is a comment line.
      continue;
    }
    const std::vector<std::string> fields =
        absl::StrSplit(line, '\t', absl::AllowEmpty());
    CHECK_GE(fields.size(), 4) << "format error: " << line;

    UsageItem item;
    item.key = ((fields[0] == "*") ? "" : fields[0]);
    item.value = ((fields[1] == "*") ? "" : fields[1]);
    item.conjugation = ((fields[2] == "*") ? "" : fields[2]);
    item.meaning = absl::StrReplaceAll((fields[3] == "*") ? "" : fields[3],
                                       {{"\\n", "\n"}});

    absl::btree_map<std::string, int>::iterator it =
        conjugation_id_map.find(item.conjugation);
    if (it == conjugation_id_map.end()) {
      conjugation_id_map.insert(
          std::pair<std::string, int>(item.conjugation, conjugation_id));
      item.conjugation_id = conjugation_id;
      conjugation_list->push_back(item.conjugation);
      ++conjugation_id;
    } else {
      item.conjugation_id = it->second;
    }
    usage_entries->push_back(item);
  }
}

// remove "基本形"'s conjugation suffix
void RemoveBaseformConjugationSuffix(
    const std::map<std::string, ConjugationType>& baseform_map,
    std::vector<UsageItem>* usage_entries) {
  for (std::vector<UsageItem>::iterator usage_itr = usage_entries->begin();
       usage_itr != usage_entries->end(); ++usage_itr) {
    const std::map<std::string, ConjugationType>::const_iterator baseform_itr =
        baseform_map.find(usage_itr->conjugation);
    if (baseform_itr == baseform_map.end()) {
      continue;
    }
    const ConjugationType& type = baseform_itr->second;

    if (usage_itr->key.length() <= type.key_suffix.length()) {
      LOG(WARNING) << "key:[" << usage_itr->key << "] is not longer then "
                   << "baseform.key_suffix  of \"" << usage_itr->conjugation
                   << "\" : [" << type.key_suffix << "]";
    }
    if (usage_itr->value.length() <= type.value_suffix.length()) {
      LOG(WARNING) << "value:[" << usage_itr->value << "] is not longer then "
                   << "baseform.value_suffix  of \"" << usage_itr->conjugation
                   << "\" : [" << type.value_suffix << "]";
    }

    usage_itr->key.erase(usage_itr->key.length() - type.key_suffix.length());
    usage_itr->value.erase(usage_itr->value.length() -
                           type.value_suffix.length());
  }
}

uint32_t Lookup(const absl::btree_map<std::string, uint32_t>& m,
                const absl::string_view key) {
  const auto iter = m.find(key);
  CHECK(iter != m.end()) << "Cannot find key=" << key;
  return iter->second;
}

void Convert() {
  static_assert(std::endian::native == std::endian::little);

  // Load cforms_file
  absl::btree_map<std::string, std::vector<ConjugationType>> inflection_map;
  std::map<std::string, ConjugationType> baseform_map;
  LoadConjugation(absl::GetFlag(FLAGS_cforms_file), &inflection_map,
                  &baseform_map);

  // Load usage_data_file
  std::vector<UsageItem> usage_entries;
  std::vector<std::string> conjugation_list;
  LoadUsage(absl::GetFlag(FLAGS_usage_data_file), &usage_entries,
            &conjugation_list);
  RemoveBaseformConjugationSuffix(baseform_map, &usage_entries);
  std::stable_sort(usage_entries.begin(), usage_entries.end(),
                   UsageItemKeynameCmp);

  // Assign unique index to every string data.  The same string share the same
  // index, so the data is slightly compressed.
  absl::btree_map<std::string, uint32_t> string_index;
  {
    // Collect all the strings while assigning temporary index 0.
    string_index[""] = 0;
    for (const auto& kv : baseform_map) {
      string_index[kv.second.value_suffix] = 0;
      string_index[kv.second.key_suffix] = 0;
    }
    for (const auto& kv : inflection_map) {
      for (const auto& conj_type : kv.second) {
        string_index[conj_type.value_suffix] = 0;
        string_index[conj_type.key_suffix] = 0;
      }
    }
    for (const auto& item : usage_entries) {
      string_index[item.key] = 0;
      string_index[item.value] = 0;
      string_index[item.meaning] = 0;
    }
    // Assign index.
    uint32_t index = 0;
    for (auto& kv : string_index) {
      kv.second = index++;
    }
  }

  // Output base conjugation suffix data.
  {
    OutputFileStream ostream(
        absl::GetFlag(FLAGS_output_base_conjugation_suffix),
        std::ios_base::out | std::ios_base::binary);
    for (const auto& conj : conjugation_list) {
      const uint32_t key_suffix_index =
          Lookup(string_index, baseform_map[conj].key_suffix);
      const uint32_t value_suffix_index =
          Lookup(string_index, baseform_map[conj].value_suffix);
      ostream.write(reinterpret_cast<const char*>(&key_suffix_index), 4);
      ostream.write(reinterpret_cast<const char*>(&value_suffix_index), 4);
    }
  }

  // Output conjugation suffix data.
  std::vector<int> conjugation_index(conjugation_list.size() + 1);
  {
    OutputFileStream ostream(absl::GetFlag(FLAGS_output_conjugation_suffix),
                             std::ios_base::out | std::ios_base::binary);
    int out_count = 0;
    for (size_t i = 0; i < conjugation_list.size(); ++i) {
      absl::Span<const ConjugationType> conjugations =
          inflection_map[conjugation_list[i]];
      conjugation_index[i] = out_count;
      if (conjugations.empty()) {
        const uint32_t index = Lookup(string_index, "");
        ostream.write(reinterpret_cast<const char*>(&index), 4);
        ostream.write(reinterpret_cast<const char*>(&index), 4);
        ++out_count;
      } else {
        using StrPair = std::pair<std::string, std::string>;
        std::set<StrPair> key_and_value_suffix_set;
        for (const ConjugationType& ctype : conjugations) {
          key_and_value_suffix_set.emplace(ctype.value_suffix,
                                           ctype.key_suffix);
        }
        for (const auto& kv : key_and_value_suffix_set) {
          const uint32_t value_suffix_index = Lookup(string_index, kv.first);
          const uint32_t key_suffix_index = Lookup(string_index, kv.second);
          ostream.write(reinterpret_cast<const char*>(&value_suffix_index), 4);
          ostream.write(reinterpret_cast<const char*>(&key_suffix_index), 4);
          ++out_count;
        }
      }
    }
    conjugation_index[conjugation_list.size()] = out_count;
  }

  // Output conjugation suffix data index.
  {
    OutputFileStream ostream(absl::GetFlag(FLAGS_output_conjugation_index),
                             std::ios_base::out | std::ios_base::binary);
    ostream.write(reinterpret_cast<const char*>(conjugation_index.data()),
                  4 * conjugation_index.size());
  }

  // Output usage data.
  {
    OutputFileStream ostream(absl::GetFlag(FLAGS_output_usage_item_array),
                             std::ios_base::out | std::ios_base::binary);
    int32_t usage_id = 0;
    for (const UsageItem& item : usage_entries) {
      const uint32_t key_index = Lookup(string_index, item.key);
      const uint32_t value_index = Lookup(string_index, item.value);
      const uint32_t meaning_index = Lookup(string_index, item.meaning);
      ostream.write(reinterpret_cast<const char*>(&usage_id), 4);
      ostream.write(reinterpret_cast<const char*>(&key_index), 4);
      ostream.write(reinterpret_cast<const char*>(&value_index), 4);
      ostream.write(reinterpret_cast<const char*>(&item.conjugation_id), 4);
      ostream.write(reinterpret_cast<const char*>(&meaning_index), 4);
      ++usage_id;
    }
  }

  // Output string array.
  {
    std::vector<absl::string_view> strs;
    for (const auto& kv : string_index) {
      // Check if the string is placed at its index in the string array.
      CHECK_EQ(strs.size(), kv.second);
      strs.emplace_back(kv.first);
    }
    SerializedStringArray::SerializeToFile(
        strs, absl::GetFlag(FLAGS_output_string_array));
  }
}

}  // namespace
}  // namespace mozc

int main(int argc, char** argv) {
  mozc::InitMozc(argv[0], &argc, &argv);
  mozc::Convert();
  return 0;
}
