// Copyright 2010, Google Inc.
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
// % gen_single_kanji_rewriter_dictionary_main
//    --input=input.tsv --output=output_header

#include <map>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <set>
#include "converter/pos.h"
#include "base/base.h"
#include "base/file_stream.h"
#include "base/port.h"
#include "base/util.h"
#include "rewriter/symbol_rewriter.h"
#include "rewriter/embedded_dictionary.h"

DEFINE_string(input, "", "single kanji dictionary file");
DEFINE_string(output, "", "output header file");
DEFINE_double(min_prob, 0.1, "minimum prob threshold");

namespace mozc {
namespace {

struct SingleKanjiEntry {
  double freq;
  string value;
  string desc;
  SingleKanjiEntry (const double f, const string &v, const string &d) :
      freq(f), value(v), desc(d) {}
};

struct GreaterEntry {
  bool operator() (const SingleKanjiEntry &lhs, const SingleKanjiEntry &rhs) {
    if (lhs.freq != rhs.freq) {
      return lhs.freq > rhs.freq;
    }
    if (lhs.value != rhs.value) {
      return lhs.value > rhs.value;
    }
    return lhs.desc > rhs.desc;
  }
};

bool ReplaceFirst(const string &src, const string &dst,
                  string *output) {
  if (output->find(src) == 0 && Util::CharsLen(*output) >= 2) {
    *output = dst + output->substr(src.size(),
                                   output->size() - src.size());
    return true;
  }
  return false;
}

string NormalizeRendaku(const string &input) {
  static const char* const kRendakuSrc[] = {
    "\343\201\214", "\343\201\216", "\343\201\220",
    "\343\201\222", "\343\201\224",
    "\343\201\226", "\343\201\230", "\343\201\232",
    "\343\201\234", "\343\201\236",
    "\343\201\240", "\343\201\242", "\343\201\245",
    "\343\201\247", "\343\201\251",
    "\343\201\260", "\343\201\263", "\343\201\266",
    "\343\201\271", "\343\201\274"
  };

  static const char* const kRendakuDst[] = {
    "\343\201\213", "\343\201\215", "\343\201\217",
    "\343\201\221", "\343\201\223",
    "\343\201\225", "\343\201\227", "\343\201\231",
    "\343\201\233", "\343\201\235",
    "\343\201\237", "\343\201\241", "\343\201\244",
    "\343\201\246", "\343\201\250",
    "\343\201\257", "\343\201\262", "\343\201\265",
    "\343\201\270", "\343\201\273"
  };

//   static const char* const kRendakuSrc[] = {
//     "が", "ぎ", "ぐ", "げ", "ご",
//     "ざ", "じ", "ず", "ぜ", "ぞ",
//     "だ", "ぢ", "づ", "で", "ど",
//     "ば", "び", "ぶ", "べ", "ぼ"
//   };
//   static const char* const kRendakuDst[] = {
//     "か", "き", "く", "け", "こ",
//     "さ", "し", "す", "せ", "そ",
//     "た", "ち", "つ", "て", "と",
//     "は", "ひ", "ふ", "へ", "ほ"
//   };

  COMPILE_ASSERT(arraysize(kRendakuSrc) == arraysize(kRendakuDst),
                 checks_rendaku_src_dst_size);

  string str = input;
  for (size_t i = 0; i < arraysize(kRendakuDst); ++i) {
    if (ReplaceFirst(kRendakuSrc[i], kRendakuDst[i], &str)) {
      return str;
    }
  }
  return str;
}

// Read dic:
void MakeDictioanry(const string &single_kanji_dictionary_file,
                    const string &output_file) {
  mozc::InputFileStream ifs(single_kanji_dictionary_file.c_str());
  mozc::OutputFileStream ofs(output_file.c_str());
  CHECK(ifs);
  CHECK(ofs);

  uint16 id = 0;

  // assume POS of single-kanji is NOUN
  //  static const char kPOS[] = "名詞";
  static const char kPOS[] = "\345\220\215\350\251\236";
  CHECK(POS::GetPOSIDs(kPOS, &id));

  map<string, map<string, pair<double, string> > > rdic;
  set<string> seen;
  vector<string> fields;
  string line;
  while (getline(ifs, line)) {
    fields.clear();
    Util::SplitStringUsing(line, "\t", &fields);
    CHECK_GE(fields.size(), 4);
    const string &value = fields[0];
    const string &key = fields[1];
    const double prob = strtod(fields[2].c_str(), NULL);
    const double freq = strtod(fields[3].c_str(), NULL);
    const string desc = (fields.size() >= 7)? fields[6] : "";
    // filter kanji with minor reading
    if (prob > FLAGS_min_prob && freq > 0.0) {
      rdic[value][key] = make_pair(freq, desc);
      seen.insert(value + "\t" + key);
    }
  }

  // Rendaku Normalization
  for (map<string, map<string, pair<double, string> > >::iterator it = rdic.begin();
       it != rdic.end(); ++it) {
    for (map<string, pair<double, string> >::iterator it2 = it->second.begin();
         it2 != it->second.end(); ++it2) {
      CHECK_GT(it2->second.first, 0.0);
      const string normalized = NormalizeRendaku(it2->first);
      static const double kRendakuDemotionFactor = 0.01;
      if (normalized != it2->first &&
          seen.find(it->first + "\t" + normalized) != seen.end()) {
        it2->second.first *= kRendakuDemotionFactor;
        CHECK_GT(it2->second.first, 0.0);
      }
    }
  }

  map<string, vector<SingleKanjiEntry > > dic;
  double sum = 0.0;
  for (map<string, map<string, pair<double, string> > >::const_iterator it = rdic.begin();
       it != rdic.end(); ++it) {
    for (map<string, pair<double, string> >::const_iterator it2 = it->second.begin();
         it2 != it->second.end(); ++it2) {
      const string &key = it2->first;
      const string &value = it->first;
      const double freq = it2->second.first;
      const string desc = it2->second.second;
      CHECK_GT(freq, 0.0);
      sum += freq;
      dic[key].push_back(SingleKanjiEntry(freq, value, desc));
    }
  }

  CHECK_GT(sum, 0.0);

  for (map<string, vector<SingleKanjiEntry> >::iterator it = dic.begin();
       it != dic.end(); ++it) {
    vector<SingleKanjiEntry> &vec = it->second;
    sort(vec.begin(), vec.end(), GreaterEntry());
    for (size_t i = 0; i < vec.size(); ++i) {
      const int cost = min(static_cast<int>(-500.0 *
                                            log(1.0 * vec[i].freq / sum)),
                           32765);
      CHECK_GT(cost, 0);
      // Output in mozc dictionary format
      ofs << it->first << '\t'
          << id << '\t' << id << '\t' << cost << '\t'
          << vec[i].value << '\t' << vec[i].desc << endl;
    }
  }
}
}  // namespace
}  // mozc

int main(int argc, char **argv) {
  InitGoogle(argv[0], &argc, &argv, false);

  const string tmp_text_file = FLAGS_output + ".txt";
  static const char kHeaderName[] = "SingleKanjiData";

  mozc::MakeDictioanry(FLAGS_input, tmp_text_file);
  mozc::EmbeddedDictionary::Compile(kHeaderName,
                                    tmp_text_file,
                                    FLAGS_output);
  //  mozc::Util::Unlink(tmp_text_file);

  return 0;
}
