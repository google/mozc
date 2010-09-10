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

#include <string.h>
#include <string>
#include <vector>
#include <algorithm>
#include <map>
#include "base/file_stream.h"
#include "rewriter/embedded_dictionary.h"

namespace mozc {

namespace {

struct CompilerToken {
  string key;
  string value;
  string description;
  string additional_description;
  uint16 lid;
  uint16 rid;
  int16 cost;
};

struct TokenCompare {
  bool operator()(const EmbeddedDictionary::Token &s1,
                  const EmbeddedDictionary::Token &s2) {
    return (strcmp(s1.key, s2.key) < 0);
  }
};

struct CompareByCost {
  bool operator()(const CompilerToken &t1, const CompilerToken &t2) {
    return (t1.cost < t2.cost);
  }
};
}  // namespace

EmbeddedDictionary::EmbeddedDictionary(const EmbeddedDictionary::Token *token,
                                       size_t size)
    : token_(token), size_(size) {
  CHECK(token_);
  CHECK_GT(size_, 0);
}

EmbeddedDictionary::~EmbeddedDictionary() {}

// do binary-search
const EmbeddedDictionary::Token*
EmbeddedDictionary::Lookup(const string &key) const {
  Token key_token;
  key_token.key = key.c_str();
  key_token.value = NULL;
  key_token.value_size = 0;
  const Token *result = lower_bound(token_, token_ + size_,
                                    key_token, TokenCompare());
  if (result == (token_ + size_) || key != result->key) {
    return NULL;
  }
  return result;
}

const EmbeddedDictionary::Token *
EmbeddedDictionary::AllToken() const {
  // |token_ + size| has a placeholder for the all tokens
  return token_ + size_;
}

void EmbeddedDictionary::Compile(const string &name,
                                 const string &input,
                                 const string &output) {
  InputFileStream ifs(input.c_str());
  CHECK(ifs);

  OutputFileStream ofs(output.c_str());
  CHECK(ofs);

  string line;
  vector<string> fields;
  map<string, vector<CompilerToken> > dic;
  while (getline(ifs, line)) {
    fields.clear();
    Util::SplitStringUsing(line, "\t", &fields);
    CHECK_GE(fields.size(), 4);
    CompilerToken token;
    const string &key = fields[0];
    token.value = fields[4];
    token.lid   = atoi32(fields[1].c_str());
    token.rid   = atoi32(fields[2].c_str());
    token.cost  = atoi32(fields[3].c_str());
    token.description = (fields.size() > 5) ? fields[5] : "";
    token.additional_description = (fields.size() > 6) ? fields[6] : "";
    dic[key].push_back(token);
  }

  ofs << "static const mozc::EmbeddedDictionary::Value k" << name
      << "_value[] = {" << endl;

  size_t value_size = 0;
  for (map<string, vector<CompilerToken> >::iterator it = dic.begin();
       it != dic.end(); ++it) {
    vector<CompilerToken> &vec = it->second;
    sort(vec.begin(), vec.end(), CompareByCost());
    for (size_t i = 0; i < vec.size(); ++i) {
      string escaped;
      Util::Escape(vec[i].value, &escaped);
      ofs << "  { \"" << escaped << "\", ";
      if (vec[i].description.empty()) {
        ofs << "NULL, ";
      } else {
        Util::Escape(vec[i].description, &escaped);
        ofs << " \"" << escaped << "\", ";
      }
      if (vec[i].additional_description.empty()) {
        ofs << "NULL, ";
      } else {
        Util::Escape(vec[i].additional_description, &escaped);
        ofs << " \"" << escaped << "\", ";
      }
      ofs << vec[i].lid << ", " << vec[i].rid << ", " << vec[i].cost << " },";
      ofs << endl;
      ++value_size;
    }
  }
  ofs << "  { NULL, NULL, NULL, 0, 0, 0 }" << endl;
  ofs << "};" << endl;

  ofs << "static const size_t k" << name << "_token_size = "
      << dic.size() << ";" << endl;

  ofs << "static const mozc::EmbeddedDictionary::Token k" << name
      << "_token_data[] = {" << endl;

  size_t offset = 0;
  for (map<string, vector<CompilerToken> >::const_iterator it = dic.begin();
       it != dic.end(); ++it) {
    string escaped;
    Util::Escape(it->first, &escaped);
    ofs << "  { \"" << escaped << "\", k" << name << "_value + "
        << offset << ", " << it->second.size() << "}," << endl;
    offset += it->second.size();
  }
  ofs << "  { NULL, " << "k" << name << "_value, " << value_size << " }" << endl;

  ofs << "};" << endl;
};
}  // mozc
