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

#include "dictionary/dictionary.h"

#include <cstring>
#include <list>
#include <set>
#include <map>
#include <string>

#include "base/base.h"
#include "converter/node.h"
#include "dictionary/system/system_dictionary.h"
#include "dictionary/user_dictionary.h"

namespace mozc {

Dictionary::Dictionary() {}

// TODO(keni) decide how and when to call destoructors of dictionaries
// that are generated through Dictionary::Add().
Dictionary::~Dictionary() {}

DictionaryInterface *Dictionary::Add(DictionaryType type) {
  DictionaryInterface *dic = NULL;
  switch (type) {
    case SYSTEM:
      dic = SystemDictionary::GetSystemDictionary();
      break;
    case USER:
      dic = UserDictionary::GetUserDictionary();
      break;
    default:
      break;
  }
  if (dic != NULL) {
    dics_.push_back(dic);
  }
  return dic;
}

Node *Dictionary::LookupPredictive(const char *str, int size,
                                   ConverterData *data) const {
  return LookupInternal(str, size, PREDICTIVE, data);
}

Node *Dictionary::LookupExact(const char *str, int size,
                              ConverterData *data) const {
  return LookupInternal(str, size, EXACT, data);
}

Node *Dictionary::LookupPrefix(const char *str, int size,
                               ConverterData *data) const {
  return LookupInternal(str, size, PREFIX, data);
}

Node *Dictionary::LookupReverse(const char *str, int size,
                                ConverterData *data) const {
  return LookupInternal(str, size, REVERSE, data);
}

Node *Dictionary::LookupInternal(const char *str, int size,
                                 LookupType type,
                                 ConverterData *data) const {
  Node *head = NULL;
  for (int i = 0; i < dics_.size(); ++i) {
    Node *nodes = NULL;
    switch (type) {
      case PREDICTIVE:
        nodes = dics_[i]->LookupPredictive(str, size, data);
        break;
      case EXACT:
        nodes = dics_[i]->LookupExact(str, size, data);
        break;
      case PREFIX:
        nodes = dics_[i]->LookupPrefix(str, size, data);
        break;
      case REVERSE:
        nodes = dics_[i]->LookupReverse(str, size, data);
        break;
    }
    if (head == NULL) {
      head = nodes;
    } else if (nodes != NULL) {
      Node *last = NULL;
      // TODO(taku)  this is O(n^2) algorithm. can be fixed.
      for (last = nodes; last->bnext != NULL; last = last->bnext) {}
      last->bnext = head;
      head = nodes;
    }
  }
  return head;
}
}  // namespace mozc
