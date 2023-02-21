// Copyright 2010-2021, Google Inc.
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

#include <cstdint>
#include <istream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/container/trie.h"
#include "base/port.h"
#include "composer/internal/typing_model.h"
#include "data_manager/data_manager_interface.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "absl/container/flat_hash_set.h"

namespace mozc {
namespace composer {

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
  // This flag indicates that the composition should be ended and committed.
  DIRECT_INPUT = 4,

  // This flag treats the next typing as a new input.  This flag is
  // used with the NEW_CHUNK flag.
  END_CHUNK = 8,
};
typedef uint32_t TableAttributes;

class Entry {
 public:
  Entry(const std::string &input, const std::string &result,
        const std::string &pending, TableAttributes attributes);
  virtual ~Entry() {}
  const std::string &input() const { return input_; }
  const std::string &result() const { return result_; }
  const std::string &pending() const { return pending_; }
  TableAttributes attributes() const { return attributes_; }

 private:
  const std::string input_;
  const std::string result_;
  const std::string pending_;
  TableAttributes attributes_;
};

class Table {
 public:
  Table();
  Table(const Table &) = delete;
  Table &operator=(const Table &) = delete;
  virtual ~Table();

  bool InitializeWithRequestAndConfig(const commands::Request &request,
                                      const config::Config &config,
                                      const DataManagerInterface &data_manager);

  // Return true if adding the input-pending pair makes a loop of
  // conversion rules.
  bool IsLoopingEntry(const std::string &input,
                      const std::string &pending) const;
  const Entry *AddRule(const std::string &input, const std::string &output,
                       const std::string &pending);

  const Entry *AddRuleWithAttributes(const std::string &input,
                                     const std::string &output,
                                     const std::string &pending,
                                     TableAttributes attributes);

  void DeleteRule(const std::string &input);

  bool LoadFromString(const std::string &str);
  bool LoadFromFile(const char *filepath);

  const Entry *LookUp(const std::string &input) const;
  const Entry *LookUpPrefix(const std::string &input, size_t *key_length,
                            bool *fixed) const;
  void LookUpPredictiveAll(const std::string &input,
                           std::vector<const Entry *> *results) const;
  // TODO(komatsu): Delete this function.
  bool HasSubRules(const std::string &input) const;

  bool HasNewChunkEntry(const std::string &input) const;

  bool case_sensitive() const;
  void set_case_sensitive(bool case_sensitive);

  const TypingModel *typing_model() const;

  // Parse special key strings escaped with the pair of "{" and "}"
  // and register them to be used by ParseSpecialKey().
  // Also returns the parsed string.
  std::string RegisterSpecialKey(const std::string &input);

  // Parse special key strings escaped with the pair of "{" and "}"
  // and return the parsed string.
  std::string ParseSpecialKey(const std::string &input) const;

  // Delete invisible special keys wrapped with ("\x0F", "\x0E") and
  // return the trimmed visible string.
  static std::string DeleteSpecialKey(const std::string &input);

  // If the string starts with a special key wrapped with ("\x0F", "\x0E"),
  // erases it and returns true. Otherwise returns false.
  static bool TrimLeadingSpecialKey(std::string *input);

  // Return the default table.
  static const Table &GetDefaultTable();

  void SetTypingModelForTesting(
      std::unique_ptr<const TypingModel> typing_model) {
    typing_model_ = std::move(typing_model);
  }

 private:
  friend class TypingCorrectorTest;
  friend class TypingCorrectionTest;

  bool LoadFromStream(std::istream *is);
  void DeleteEntry(const Entry *entry);
  void ResetEntrySet();

  using EntryTrie = Trie<const Entry *>;
  std::unique_ptr<EntryTrie> entries_;
  using EntrySet = absl::flat_hash_set<const Entry *>;
  EntrySet entry_set_;

  using SpecialKeyMap = absl::flat_hash_map<std::string, std::string>;
  SpecialKeyMap special_key_map_;

  // If false, input alphabet characters are normalized to lower
  // characters.  The default value is false.
  bool case_sensitive_;

  // Typing model. nullptr if no corresponding model is available.
  std::unique_ptr<const TypingModel> typing_model_;
};

class TableManager {
 public:
  TableManager();
  ~TableManager();
  // Return Table for the request and the config
  // TableManager has ownership of the return value;
  const Table *GetTable(const commands::Request &request,
                        const config::Config &config,
                        const DataManagerInterface &data_manager);

  void ClearCaches();

 private:
  // Table caches.
  // Key uint32_t is calculated hash and unique for
  //  commands::Request::SpecialRomanjiTable
  //  config::Config::PreeditMethod
  //  config::Config::PunctuationMethod
  //  config::Config::SymbolMethod
  std::map<uint32_t, std::unique_ptr<const Table>> table_map_;
  // Fingerprint for Config::custom_roman_table;
  uint32_t custom_roman_table_fingerprint_;
};

}  // namespace composer
}  // namespace mozc

#endif  // MOZC_COMPOSER_TABLE_H_
