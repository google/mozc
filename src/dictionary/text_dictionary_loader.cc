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

#include "base/base.h"
#include "base/file_stream.h"
#include "base/util.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/pos_matcher.h"

namespace mozc {

TextDictionaryLoader::TextDictionaryLoader() {
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
    token->lid = POSMatcher::GetZipcodeId();
    token->rid = POSMatcher::GetZipcodeId();
    return true;
  }

  return false;
}

bool TextDictionaryLoader::Open(const char *filename) {
  Close();
  return OpenWithLineLimit(filename, -1);
}

bool TextDictionaryLoader::OpenWithLineLimit(const char *filename,
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

void TextDictionaryLoader::Close() {
  for (vector<Token *>::iterator it = tokens_.begin();
       it != tokens_.end(); ++it) {
    delete *it;
  }
  tokens_.clear();
}

void TextDictionaryLoader::CollectTokens(vector<Token *> *res) {
  for (vector<Token *>::const_iterator it = tokens_.begin();
       it != tokens_.end(); ++it) {
    res->push_back(*it);
  }
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
  token->lid = atoi(fields[1].c_str());
  token->rid = atoi(fields[2].c_str());
  token->cost = atoi(fields[3].c_str());
  Util::NormalizeVoicedSoundMark(fields[4], &token->value);

  const string label = fields.size() > 5 ? fields[5] : "";
  CHECK(RewriteSpecialToken(token, label))
      << "invalid label: " << line;

  tokens_.push_back(token);
}
}  // namespace mozc
