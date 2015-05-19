// Copyright 2010-2012, Google Inc.
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

#include "dictionary/text_dictionary_loader.h"

#include <climits>
#include <cstring>
#include <map>

#include "base/base.h"
#include "base/file_stream.h"
#include "base/logging.h"
#include "base/number_util.h"
#include "base/util.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/pos_matcher.h"

namespace mozc {

TextDictionaryLoader::TextDictionaryLoader(const POSMatcher &pos_matcher)
    : pos_matcher_(&pos_matcher) {
}

TextDictionaryLoader::~TextDictionaryLoader() {
  Close();
}

bool TextDictionaryLoader::RewriteSpecialToken(Token *token,
                                               const string &label) {
  CHECK(token);
  if (label.empty()) {
    return true;
  }
  if (Util::StartsWith(label, "SPELLING_CORRECTION")) {
    token->attributes = Token::SPELLING_CORRECTION;
    return true;
  }
  if (Util::StartsWith(label, "ZIP_CODE")) {
    token->lid = pos_matcher_->GetZipcodeId();
    token->rid = pos_matcher_->GetZipcodeId();
    return true;
  }
  if (Util::StartsWith(label, "ENGLISH")) {
    // TODO(noriyukit): Might be better to use special POS for english words.
    token->lid = pos_matcher_->GetIsolatedWordId();
    token->rid = pos_matcher_->GetIsolatedWordId();
    return true;
  }
  LOG(ERROR) << "Unknown special label: " << label;
  return false;
}

bool TextDictionaryLoader::Open(const string &filename) {
  Close();
  return OpenWithLineLimit(filename, -1);
}

bool TextDictionaryLoader::OpenWithLineLimit(const string &filename,
                                             int limit) {
  Close();
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
      ParseTSV(line);
      --limit;
    }
  }

  LOG(INFO) << tokens_.size() << " tokens from " << filename << "\n";

  return true;
}

namespace {
void UpdateMap(const string &key, const Token *token,
               map<string, const Token *> *target_map) {
  map<string, const Token *>::iterator it = target_map->find(key);
  if (it == target_map->end() || it->second->cost > token->cost) {
    target_map->insert(make_pair(key, token));
  }
}
}   // namespace

bool TextDictionaryLoader::OpenReadingCorrection(const string &filename) {
  map<string, const Token *> value_map, key_value_map;

  // TODO(all): we want to merge them off-line (in dictionary pipeline)
  // in order to reduce the dictionary generation cost and speed.
  // The benefit of merging them here is that we can allow third-party
  // developers to integrate their own reading correction data.
  for (size_t i = 0; i < tokens_.size(); ++i) {
    if (tokens_[i]->attributes != Token::NONE) {
      continue;
    }
    const string &value = tokens_[i]->value;
    const string key_value = tokens_[i]->key + "\t" + tokens_[i]->value;
    UpdateMap(value, tokens_[i], &value_map);
    UpdateMap(key_value, tokens_[i], &key_value_map);
  }

  InputFileStream ifs(filename.c_str());
  if (!ifs) {
    LOG(INFO) << "cannot open: " << filename;
    return false;
  }

  LOG(INFO) << "Loading reading correction: " << filename;
  size_t reading_correction_size = 0;

  string line;
  while (getline(ifs, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }
    // value, error, correct
    vector<string> fields;
    Util::SplitStringUsing(line, "\t", &fields);
    CHECK_GE(fields.size(), 3);
    const string key_value = fields[1] + "\t" + fields[0];
    const string value = fields[0];
    // No need to add this entry to the dictionary,
    // as it is already registered.
    if (key_value_map.find(key_value) != key_value_map.end()) {
      continue;
    }

    // If the value is not in the system dictioanry, ignore this entry,
    // as we can't calculate the emission cost.
    map<string, const Token *>::const_iterator it = value_map.find(value);
    if (it == value_map.end()) {
      continue;
    }

    // The cost is calculated as -log(prob) * 500.
    // We here assume that the wrong reading appear with 1/100 probability
    // of the original (correct) reading.
    const int kCostPenalty = 2302;      // -log(1/100) * 500;
    const Token *org_token = it->second;
    Token *token = new Token;
    token->key   = fields[1];
    token->value = org_token->value;
    token->lid   = org_token->lid;
    token->rid   = org_token->rid;
    token->cost  = org_token->cost + kCostPenalty;
    // We don't set SPELLING_CORRECTION. The entries in reading_correction
    // data are also stored in rewriter/correction_rewriter.cc.
    // reading_correction_rewriter annotates the spelling correction
    // notations.
    token->attributes = Token::NONE;
    tokens_.push_back(token);
    ++reading_correction_size;
  }

  LOG(INFO) << reading_correction_size << " tokens from " << filename << "\n";

  return true;
}

void TextDictionaryLoader::Close() {
  for (vector<Token *>::iterator it = tokens_.begin();
       it != tokens_.end(); ++it) {
    delete *it;
  }
  tokens_.clear();
}

void TextDictionaryLoader::CollectTokens(vector<Token *> *res) {
  DCHECK(res);
  res->reserve(res->size() + tokens_.size());
  res->insert(res->end(), tokens_.begin(), tokens_.end());
}

void TextDictionaryLoader::ParseTSV(const string &line) {
  const int kNumFields = 5;
  vector<string> fields;
  Util::SplitStringUsing(line, "\t", &fields);
  CHECK_GE(fields.size(), kNumFields) << "malformed line in dictionary:"
                                      << line;
  Token *token = new Token;
  CHECK(token);
  Util::NormalizeVoicedSoundMark(fields[0], &token->key);
  uint32 lid, rid, cost;
  CHECK(NumberUtil::SafeStrToUInt32(fields[1], &lid))
      << "wrong lid: " << fields[1];
  CHECK(NumberUtil::SafeStrToUInt32(fields[2], &rid))
      << "wrong rid: " << fields[2];
  CHECK(NumberUtil::SafeStrToUInt32(fields[3], &cost))
      << "wrong cost: " << fields[3];
  token->lid = lid;
  token->rid = rid;
  token->cost = cost;
  Util::NormalizeVoicedSoundMark(fields[4], &token->value);

  const string label = fields.size() > 5 ? fields[5] : "";
  CHECK(RewriteSpecialToken(token, label))
      << "invalid label: " << line;

  tokens_.push_back(token);
}
}  // namespace mozc
