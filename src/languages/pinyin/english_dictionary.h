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

#ifndef MOZC_LANGUAGES_PINYIN_ENGLISH_DICTIONARY_H_
#define MOZC_LANGUAGES_PINYIN_ENGLISH_DICTIONARY_H_

#include <map>
#include <string>
#include <vector>

#include "base/port.h"
#include "base/scoped_ptr.h"
#include "languages/pinyin/english_dictionary_interface.h"

namespace mozc {

namespace storage {
class EncryptedStringStorage;
}  // namespace storage

namespace rx {
class RxTrie;
}  // namespace rx

namespace pinyin {
namespace english {

typedef map<string, float> UserDictionary;

class EnglishDictionary : public EnglishDictionaryInterface {
 public:
  EnglishDictionary();
  virtual ~EnglishDictionary();

  bool GetSuggestions(const string &prefix, vector<string> *output) const;
  void LearnWord(const string &word);

 private:
  friend class EnglishDictionaryTest;

  void Init();
  // Discards user dictionary data and reload it from a storage.
  bool ReloadUserDictionary();
  // Read user dicitonary data from a storage, merges it, and writes it to a
  // storage. Currently some data are not merged correctly.
  bool Sync();

  // For Initialization or unittest use only.
  static string file_name();

  scoped_ptr<rx::RxTrie> word_trie_;
  vector<float> priority_table_;
  UserDictionary user_dictionary_;
  float learning_multiplier_;
  scoped_ptr<storage::EncryptedStringStorage> storage_;

  DISALLOW_COPY_AND_ASSIGN(EnglishDictionary);
};

}  // namespace english
}  // namespace pinyin
}  // namespace mozc

// MOZC_LANGUAGES_PINYIN_ENGLISH_DICTIONARY_H_
#endif
