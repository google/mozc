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

#include <algorithm>
#include <string>
#include <vector>
#include "base/base.h"
#include "base/util.h"
#include "base/file_stream.h"
#include "converter/pos_util.h"

namespace mozc {

void POSUtil::Open(const string &id_file) {
  ids_.clear();
  InputFileStream ifs(id_file.c_str());
  CHECK(ifs);
  string line;
  vector<string> fields;
  while (getline(ifs, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }
    fields.clear();
    Util::SplitStringUsing(line, "\t ", &fields);
    CHECK_GE(fields.size(), 2);
    const int id = atoi32(fields[0].c_str());
    ids_.push_back(make_pair(fields[1], static_cast<uint16>(id)));
  }

  //  const char kNumberPOS[] = "名詞,数";
  const char kNumberPOS[] = "\xE5\x90\x8D\xE8\xA9\x9E,\xE6\x95\xB0";
  CHECK(ids(kNumberPOS, &number_ids_));
  sort(number_ids_.begin(), number_ids_.end());

  // "助詞"
  const char kParticlePOS[] = "\xe5\x8a\xa9\xe8\xa9\x9e";
  // "助動詞"
  const char kAuxVerbPOS[] = "\xe5\x8a\xa9\xe5\x8b\x95\xe8\xa9\x9e";
  // "記号"
  const char kSymbolPOS[] = "\xe8\xa8\x98\xe5\x8f\xb7";
  // "動詞,非自立"
  const char kVerbDependentPOS[] =
      "\xE5\x8B\x95\xE8\xA9\x9E,\xE9\x9D\x9E\xE8\x87\xAA\xE7\xAB\x8B";
  // "名詞,非自立"
  const char kNounDependentPOS[] =
      "\xE5\x90\x8D\xE8\xA9\x9E,\xE9\x9D\x9E\xE8\x87\xAA\xE7\xAB\x8B";
  // "形容詞,非自立"
  const char kAdjectiveDependentPOS[] =
      "\xE5\xBD\xA2\xE5\xAE\xB9\xE8\xA9\x9E,\xE9\x9D\x9E\xE8\x87\xAA\xE7\xAB\x8B";
  // "動詞,接尾"
  const char kVerbSuffixPOS[] =
      "\xE5\x8B\x95\xE8\xA9\x9E,\xE6\x8E\xA5\xE5\xB0\xBE";
  // "名詞,接尾"
  const char kNounSuffixPOS[] =
      "\xE5\x90\x8D\xE8\xA9\x9E,\xE6\x8E\xA5\xE5\xB0\xBE";
  // "形容詞,接尾"
  const char kAdjectiveSuffixPOS[] =
      "\xE5\xBD\xA2\xE5\xAE\xB9\xE8\xA9\x9E,\xE6\x8E\xA5\xE5\xB0\xBE";

  CHECK(ids(kParticlePOS, &functional_word_ids_));
  CHECK(ids(kAuxVerbPOS, &functional_word_ids_));
  CHECK(ids(kSymbolPOS, &functional_word_ids_));
  CHECK(ids(kVerbDependentPOS, &functional_word_ids_));
  CHECK(ids(kNounDependentPOS, &functional_word_ids_));
  CHECK(ids(kAdjectiveDependentPOS, &functional_word_ids_));
  CHECK(ids(kVerbSuffixPOS, &functional_word_ids_));
  CHECK(ids(kNounSuffixPOS, &functional_word_ids_));
  CHECK(ids(kAdjectiveSuffixPOS, &functional_word_ids_));

  sort(functional_word_ids_.begin(), functional_word_ids_.end());
}

uint16 POSUtil::id(const string &feature) const {
  CHECK(!feature.empty());
  for (size_t i = 0; i < ids_.size(); ++i) {
    if (ids_[i].first.find(feature) == 0) {
      return ids_[i].second;
    }
  }
  LOG(ERROR) << "Cannot find the POS for: " << feature;
  return 0;
}

bool POSUtil::ids(const string &feature,
                  vector<uint16> *ids) const {
  CHECK(ids);
  CHECK(!feature.empty());
  bool found = false;
  for (size_t i = 0; i < ids_.size(); ++i) {
    if (ids_[i].first.find(feature) == 0) {
      ids->push_back(ids_[i].second);
      found = true;
    }
  }
  LOG_IF(ERROR, !found) << "Cannot find the POS for: "
                        << feature;
  return found;
}
}  // namespace mozc
