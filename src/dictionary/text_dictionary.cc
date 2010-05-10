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

// NOTE(tabata): This code is used mainly by rx_builder to build
// dictionary. Please check error handling, if you want to include
// this to run within client.

#include "dictionary/text_dictionary.h"

#include <string.h>

#include <climits>

#include "base/base.h"
#include "base/file_stream.h"
#include "base/util.h"

DEFINE_int32(maximum_cost_threshold, -1,
             "Maximum Cost threshold used for Text Dictionary");

namespace mozc {

TextDictionary::TextDictionary() {
}

TextDictionary::~TextDictionary() {
  vector<Token *>::iterator it;
  for (it = tokens_.begin(); it != tokens_.end(); it++) {
    delete *it;
  }
}

bool TextDictionary::Open(const char *filename) {
  return OpenWithLineLimit(filename, -1);
}

bool TextDictionary::OpenWithLineLimit(const char *filename, int limit) {
  if (limit < 0) {
    limit = INT_MAX;
  }

  vector<string> filenames;
  Util::SplitStringUsing(filename, ",", &filenames);
  if (filenames.empty()) {
    LOG(ERROR) << "filename is empty";
    return false;
  }

  for (size_t i = 0; i < filenames.size(); ++i) {
    LOG(INFO) << "Loading: " << filenames[i];
    InputFileStream ifs(filenames[i].c_str());
    if (!ifs) {
      LOG(INFO) << "cannot open: " << filenames[i];
      return false;
    }
    string line;
    while (limit > 0 && getline(ifs, line)) {
      Util::ChopReturns(&line);
      if (strchr(line.c_str(), '\t')) {
        ParseTSV(line);
      } else {
        ParseCSV(line);
      }
      --limit;
    }
  }

  LOG(INFO) << tokens_.size() << " tokens from " << filename << "\n";

  return true;
}

void TextDictionary::Close() {
}

Node *TextDictionary::LookupPredictive(const char *str, int size,
                                       ConverterData *data) const {
  return NULL;
}

Node *TextDictionary::LookupExact(const char *str, int size,
                                  ConverterData *data) const {
  return NULL;
}

Node *TextDictionary::LookupPrefix(const char *str, int size,
                                   ConverterData *data) const {
  return NULL;
}

Node *TextDictionary::LookupReverse(const char *str, int size,
                                    ConverterData *data) const {
  return NULL;
}

void TextDictionary::CollectTokens(vector<Token *>* res) {
  vector<Token *>::iterator it;
  for (it = tokens_.begin(); it != tokens_.end(); it++) {
    res->push_back(*it);
  }
}

void TextDictionary::ParseCSV(const string &line) {
  vector<string> fields;
  Util::SplitStringUsing(line, ",", &fields);
  const int cost = atoi(fields[3].c_str());
  if (FLAGS_maximum_cost_threshold > 0 &&
      FLAGS_maximum_cost_threshold <= cost) {
    return;
  }
  Token *t = new Token();
  Util::NormalizeVoicedSoundMark(fields[0], &t->value);
  t->value = fields[0];
  t->left_id = atoi(fields[1].c_str());
  t->right_id = atoi(fields[2].c_str());
  t->cost = cost;
  Util::NormalizeVoicedSoundMark(fields[11], &t->key);
  tokens_.push_back(t);
}

void TextDictionary::ParseTSV(const string &line) {
  const int kNumFields = 5;
  vector<string> fields;
  fields.reserve(kNumFields);
  Util::SplitStringUsing(line, "\t", &fields);
  CHECK_GE(fields.size(), kNumFields) << "malformed line in dictionary:"
                                      << line;
  const int cost = atoi(fields[3].c_str());
  if (FLAGS_maximum_cost_threshold > 0 &&
      FLAGS_maximum_cost_threshold <= cost) {
    return;
  }
  Token *t = new Token();
  Util::NormalizeVoicedSoundMark(fields[0], &t->key);
  t->left_id = atoi(fields[1].c_str());
  t->right_id = atoi(fields[2].c_str());
  t->cost = cost;
  Util::NormalizeVoicedSoundMark(fields[4], &t->value);
  tokens_.push_back(t);
}
}  // namespace mozc
