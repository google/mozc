// Copyright 2010-2014, Google Inc.
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

// Trie table for Romaji (or Kana) conversion

#ifndef MOZC_COMPOSER_TABLE_H_
#define MOZC_COMPOSER_TABLE_H_

#include <map>
#include <set>
#include <string>
#include <vector>
#include "base/port.h"
#include "base/scoped_ptr.h"
#include "base/trie.h"

namespace mozc {

// For friendship.
class DictionaryPredictorTest;

namespace commands {
class Request;
}  // namespace commands

namespace config {
class Config;
}  // namespace config
namespace composer {

class TypingModel;

// This is a bitmap representing Entry's additional attributes.
enum TableAttribute {
  NO_TABLE_ATTRIBUTE = 0,
  // When the typing is beginning, the rule with this attribute is
  // executed even if the previous characters can be a part of other
  // rules.
  NEW_CHUNK = 1,
  // This flag suppresses any transliteration performed in CharChunk
  // and treated as an as-is key event.
  NO_TRANSLITERATION = 2,
  // This flag indicates that the composition should be ended and commited.
  DIRECT_INPUT = 4,

  // This flag treats the next typing as a new input.  This flag is
  // used with the NEW_CHUNK flag.
  END_CHUNK = 8,
};
typedef uint32 TableAttributes;

class Entry {
 public:
  Entry(const string &input,
        const string &result,
        const string &pending,
        TableAttributes attributes);
  virtual ~Entry() {}
  const string &input() const { return input_; }
  const string &result() const { return result_; }
  const string &pending() const { return pending_; }
  TableAttributes attributes() const { return attributes_; }
 private:
  const string input_;
  const string result_;
  const string pending_;
  TableAttributes attributes_;
};

class Table {
 public:
  Table();
  virtual ~Table();

  bool InitializeWithRequestAndConfig(const commands::Request &request,
                                      const config::Config &config);


  // Return true if adding the input-pending pair makes a loop of
  // conversion rules.
  bool IsLoopingEntry(const string &input, const string &pending) const;
  const Entry *AddRule(const string &input,
                       const string &output,
                       const string &pending);

  const Entry *AddRuleWithAttributes(const string &input,
                                     const string &output,
                                     const string &pending,
                                     TableAttributes attributes);

  void DeleteRule(const string &input);

  bool LoadFromString(const string &str);
  bool LoadFromFile(const char *filepath);

  const Entry *LookUp(const string &input) const;
  const Entry *LookUpPrefix(const string &input,
                            size_t *key_length,
                            bool *fixed) const;
  void LookUpPredictiveAll(const string &input,
                           vector<const Entry *> *results) const;
  // TODO(komatsu): Delete this function.
  bool HasSubRules(const string &input) const;

  bool HasNewChunkEntry(const string &input) const;

  bool case_sensitive() const;
  void set_case_sensitive(bool case_sensitive);

  const TypingModel* typing_model() const;

  // Parse special key strings escaped with the pair of "{" and "}"
  // and return the parsed string.
  static string ParseSpecialKey(const string &input);

  // Delete invisible special keys wrapped with ("\x0F", "\x0E") and
  // return the trimmed visible string.
  static string DeleteSpecialKey(const string &input);

 private:
  friend class mozc::DictionaryPredictorTest;
  friend class TypingCorrectorTest;
  friend class TypingCorrectionTest;

  bool LoadFromStream(istream *is);
  void DeleteEntry(const Entry *entry);
  void ResetEntrySet();

  typedef Trie<const Entry*> EntryTrie;
  scoped_ptr<EntryTrie> entries_;
  typedef set<const Entry*> EntrySet;
  EntrySet entry_set_;

  // If false, input alphabet characters are normalized to lower
  // characters.  The default value is false.
  bool case_sensitive_;

  // Typing model. NULL if no corresponding model is available.
  const TypingModel* typing_model_;

  DISALLOW_COPY_AND_ASSIGN(Table);
};

class TableManager {
 public:
  TableManager();
  ~TableManager();
  // Return Table for the request and the config
  // TableManager has ownership of the return value;
  const Table *GetTable(const commands::Request &request,
                        const config::Config &config);

 private:
  // Table caches.
  // Key uint32 is calculated hash and unique for
  //  commands::Request::SpecialRomanjiTable
  //  config::Config::PreeditMethod
  //  config::Config::PunctuationMethod
  //  config::Config::SymbolMethod
  map<uint32, const Table*> table_map_;
  // Fingerprint for Config::custom_roman_table;
  uint32 custom_roman_table_fingerprint_;
};

}  // namespace composer
}  // namespace mozc

#endif  // MOZC_COMPOSER_TABLE_H_
