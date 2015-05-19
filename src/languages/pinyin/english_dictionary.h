// Copyright 2010-2013, Google Inc.
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

// English dictionary class to suggest english words by prefix match. This class
// supports user dictionary to learn new words or reorder suggested words.
// This class is NOT thread-safe or process-safe.

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

class StringStorageInterface;

namespace louds {
class LoudsTrie;
}  // namespace louds

}  // namespace storage

namespace pinyin {
namespace english {

typedef map<string, uint32> UserDictionary;

class EnglishDictionary : public EnglishDictionaryInterface {
 public:
  EnglishDictionary();
  virtual ~EnglishDictionary();

  // Gets english words starting with |prefix| from system / user dictionary,
  // and sets it into |output|. Entries of |output| are ordered by priority
  // based on appearance frequency, and consist of lower-case characters.
  virtual void GetSuggestions(const string &prefix,
                              vector<string> *output) const;

  // Boosts the priority of a word. if it is a unknown word, It will be added on
  // user dictionary. Return false if failed.
  virtual bool LearnWord(const string &word);

 private:
  friend class EnglishDictionaryTest;

  // Loads system / user dictionary data. Don't call this method twice.
  void Init();

  // Discards user dictionary data and reloads it from storage.
  bool ReloadUserDictionary();

  // Reads user dictionary data from storage, merges it, and writes it to a
  // storage.
  bool Sync();

  // Returns the path to the user dictionary file.
  // For initialization or unittest use only.
  static string user_dictionary_file_path();

  // System dictionary trie data.
  scoped_ptr<mozc::storage::louds::LoudsTrie> word_trie_;
  // Maps an ID of trie entries to their priority.
  vector<float> priority_table_;
  // It maps words to their frequency.
  UserDictionary user_dictionary_;
  // Multiplier to convert from frequency to priority.
  float learning_multiplier_;
  // Storage instance to manage user dictionary.
  scoped_ptr<mozc::storage::StringStorageInterface> storage_;

  DISALLOW_COPY_AND_ASSIGN(EnglishDictionary);
};

}  // namespace english
}  // namespace pinyin
}  // namespace mozc

#endif  // MOZC_LANGUAGES_PINYIN_ENGLISH_DICTIONARY_H_
