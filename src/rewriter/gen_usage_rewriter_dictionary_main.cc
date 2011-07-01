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

// Usage dictionary generator:
// % gen_usage_rewriter_dictionary_main
//    --usage_data_file=usage_data.txt
//    --cforms_file=cforms.def
//    --output=output_header

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "base/base.h"
#include "base/file_stream.h"
#include "base/util.h"

DEFINE_string(usage_data_file, "", "usage data file");
DEFINE_string(cforms_file, "", "cforms file");
DEFINE_string(output, "", "output header file");

namespace mozc {
namespace {
struct ConjugationType {
  string form;
  string value_suffix;
  string key_suffix;
};

struct UsageItem {
  string key;
  string value;
  string conjugation;
  int conjugation_id;
  string meaning;
};

bool UsageItemKeynameCmp(const UsageItem& l, const UsageItem& r) {
  return l.key < r.key;
}

// Load cforms_file
void LoadConjugation(const string &filename,
                     map<string, vector<ConjugationType> > *output,
                     map<string, ConjugationType> *baseform_map) {
  mozc::InputFileStream ifs(filename.c_str());
  CHECK(ifs);

  string line;
  vector<string> fields;
  while (getline(ifs, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }
    fields.clear();
    Util::SplitStringUsing(line, "\t ", &fields);
    CHECK_GE(fields.size(), 4)  << "format error: " << line;

    ConjugationType tmp;
    tmp.form = fields[1];
    tmp.value_suffix = ((fields[2] == "*") ? "" : fields[2]);
    tmp.key_suffix   = ((fields[3] == "*") ? "" : fields[3]);
    (*output)[fields[0]].push_back(tmp);   // insert

    if (tmp.form == "\xE5\x9F\xBA\xE6\x9C\xAC\xE5\xBD\xA2") {  // 基本形
      (*baseform_map)[fields[0]] = tmp;
    }
  }
}

// Load usage_data_file
void LoadUsage(const string &filename,
               vector<UsageItem> *usage_entries,
               vector<string> *conjugation_list) {
  mozc::InputFileStream ifs(filename.c_str());
  CHECK(ifs);

  string line;
  vector<string> fields;
  map<string, int> conjugation_id_map;

  int conjugation_id = 0;
  while (getline(ifs, line)) {
    if (line.empty() || line[0] == '#') {
      // starting with '#' is a comment line.
      continue;
    }
    fields.clear();
    Util::SplitStringAllowEmpty(line, "\t", &fields);
    CHECK_GE(fields.size(), 4) << "format error: " << line;

    UsageItem item;
    item.key = ((fields[0] == "*") ? "" : fields[0]);
    item.value = ((fields[1] == "*") ? "" : fields[1]);
    item.conjugation = ((fields[2] == "*") ? "" : fields[2]);
    item.meaning = ((fields[3] == "*") ? "" : fields[3]);

    map<string, int>::iterator it = conjugation_id_map.find(item.conjugation);
    if (it == conjugation_id_map.end()) {
      conjugation_id_map.insert(
        pair<string, int>(item.conjugation, conjugation_id));
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
  const map<string, ConjugationType> &baseform_map,
  vector<UsageItem> *usage_entries) {
  for (vector<UsageItem>::iterator usage_itr = usage_entries->begin();
      usage_itr != usage_entries->end(); ++usage_itr) {
    const map<string, ConjugationType>::const_iterator baseform_itr =
      baseform_map.find(usage_itr->conjugation);
    if (baseform_itr == baseform_map.end()) {
      continue;
    }
    const ConjugationType &type = baseform_itr->second;

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

    usage_itr->key = usage_itr->key.substr(0,
      usage_itr->key.length() - type.key_suffix.length());
    usage_itr->value = usage_itr->value.substr(0,
      usage_itr->value.length() - type.value_suffix.length());
  }
}

void Convert() {
  // Load cforms_file
  map<string, vector<ConjugationType> > inflection_map;
  map<string, ConjugationType> baseform_map;
  LoadConjugation(FLAGS_cforms_file, &inflection_map, &baseform_map);

  // Load usage_data_file
  vector<UsageItem> usage_entries;
  vector<string> conjugation_list;
  LoadUsage(FLAGS_usage_data_file, &usage_entries, &conjugation_list);

  ostream *ofs = &cout;
  if (!FLAGS_output.empty()) {
    ofs = new mozc::OutputFileStream(FLAGS_output.c_str());
  }

  *ofs << "// This header file is generated by "
       << "gen_usage_rewriter_dictionary_main."
       << endl;

  // Output kConjugationNum
  *ofs << "static const int kConjugationNum = " <<
          conjugation_list.size() << ";" << endl;

  // Output kBaseConjugationSuffix
  *ofs << "static const ConjugationSuffix kBaseConjugationSuffix[] = {" << endl;
  for (size_t i = 0; i < conjugation_list.size(); ++i) {
    string value_suffix, key_suffix;
    Util::Escape(baseform_map[conjugation_list[i]].value_suffix, &value_suffix);
    Util::Escape(baseform_map[conjugation_list[i]].key_suffix, &key_suffix);
      *ofs << "  {\"" << value_suffix << "\", \"" << key_suffix << "\"},  "
        << "// " << conjugation_list[i] << endl;
  }
  *ofs << "};" << endl;

  // Output kConjugationSuffixData
  vector<int> conjugation_index(conjugation_list.size() + 1);
  *ofs << "static const ConjugationSuffix kConjugationSuffixData[] = {" << endl;
  int out_count = 0;
  for (size_t i = 0; i < conjugation_list.size(); ++i) {
    vector<ConjugationType> conjugations = inflection_map[conjugation_list[i]];
    conjugation_index[i] = out_count;
    if (conjugations.size() == 0) {
      *ofs << "  // " << i << ": (" << out_count << "-" << out_count
           << "): no conjugations" << endl;
      *ofs << "  {\"\",\"\"}," << endl;
      ++out_count;
    } else {
      typedef pair<string, string> StrPair;
      set<StrPair> key_and_value_suffix_set;
      for (size_t j = 0; j < conjugations.size(); ++j) {
        StrPair key_and_value_suffix(conjugations[j].value_suffix,
                                    conjugations[j].key_suffix);
        key_and_value_suffix_set.insert(key_and_value_suffix);
      }
      *ofs << "  // " << i << ": (" << out_count << "-"
           << (out_count + key_and_value_suffix_set.size()-1)
           << "): " << conjugation_list[i] << endl << " ";
      set<StrPair>::iterator itr;
      for (itr = key_and_value_suffix_set.begin();
          itr != key_and_value_suffix_set.end(); ++itr) {
        string value_suffix, key_suffix;
        Util::Escape(itr->first, &value_suffix);
        Util::Escape(itr->second, &key_suffix);
        *ofs << " {\"" << value_suffix <<
                "\", \"" << key_suffix << "\"},";
        ++out_count;
      }
      *ofs << endl;
    }
  }
  *ofs << "};" << endl;
  conjugation_index[conjugation_list.size()] = out_count;

  // Output kConjugationSuffixDataIndex
  *ofs << "static const int kConjugationSuffixDataIndex[] = {";
  for (size_t i = 0; i < conjugation_index.size(); ++i) {
    if (i != 0) {
      *ofs << ", ";
    }
    *ofs << conjugation_index[i];
  }
  *ofs << "};" << endl;

  RemoveBaseformConjugationSuffix(baseform_map, &usage_entries);
  sort(usage_entries.begin(), usage_entries.end(), UsageItemKeynameCmp);

  // Output kUsageDataSize
  *ofs << "static const size_t kUsageDataSize = "
       << usage_entries.size() << ";" << endl;

  // Output kUsageData_value
  *ofs << "static const UsageDictItem kUsageData_value[] = {" << endl;
  int32 usage_id = 0;
  for (vector<UsageItem>::iterator i = usage_entries.begin();
      i != usage_entries.end(); i++) {
    string key, value, meaning;
    Util::Escape(i->key, &key);
    Util::Escape(i->value, &value);
    Util::Escape(i->meaning, &meaning);
    *ofs <<  "  {" << usage_id << ", \"" << key << "\", "
        << "\"" << value << "\", "
        << "" << i->conjugation_id << ", "
        << "\"" << meaning << "\"}, // "
        << i->value << "(" << i->key << ")" << endl;
    ++usage_id;
  }
  *ofs << "  { NULL, NULL, NULL, NULL, NULL }" << endl;
  *ofs << "};" << endl;

  if (ofs != &cout) {
    delete ofs;
  }
}

}  // namespace
}  // namespace mozc

int main(int argc, char **argv) {
  InitGoogle(argv[0], &argc, &argv, true);
  mozc::Convert();
  return 0;
}
