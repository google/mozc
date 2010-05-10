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

#include <vector>
#include <string>
#include "converter/pos_mock.h"

namespace mozc {

bool POSMockHandler::IsValidPOS(const string &pos) const {
  return pos == "noun" || pos == "verb";
}

namespace {

void PushBackToken(const string &key,
                   const string &value,
                   uint16 id,
                   vector<POS::Token> *tokens) {
  tokens->resize(tokens->size() + 1);
  POS::Token *t = &tokens->back();
  t->key = key;
  t->value = value;
  t->id = id;
  t->cost = 0;
}

}  // namespace

bool POSMockHandler::GetTokens(const string &key,
                               const string &value,
                               const string &pos,
                               POS::CostType cost_type,
                               vector<POS::Token> *tokens) const {
  if (key.empty() ||
      value.empty() ||
      pos.empty() ||
      tokens == NULL) {
    return false;
  }

  tokens->clear();
  if (pos == "noun") {
    PushBackToken(key, value, 100, tokens);
    return true;
  } else if (pos == "verb") {
    PushBackToken(key, value, 200, tokens);
    PushBackToken(key + "ed", value + "ed", 210, tokens);
    PushBackToken(key + "ing", value + "ing", 220, tokens);
    return true;
  } else {
    return false;
  }
}

}  // namespace mozc
