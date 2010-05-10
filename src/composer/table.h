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

// Trie table for Romaji (or Kana) conversion

#ifndef MOZC_COMPOSER_TABLE_H_
#define MOZC_COMPOSER_TABLE_H_

#include <set>
#include <string>
#include "base/base.h"
#include "composer/internal/trie.h"

namespace mozc {
namespace composer {

class Entry {
 public:
  Entry(const string &input, const string &result, const string &pending);
  virtual ~Entry() {}
  const string &input() const { return input_; }
  const string &result() const { return result_; }
  const string &pending() const { return pending_; }
 private:
  const string input_;
  const string result_;
  const string pending_;
};

class Table {
 public:
  Table();
  virtual ~Table();

  bool Initialize();
  bool Reload();

  void AddRule(const string &input,
               const string &output,
               const string &pending);
  void DeleteRule(const string &input);

  bool LoadFromString(const string &str);
  bool LoadFromFile(const char *filepath);

  const Entry *LookUp(const string &input) const;
  const Entry *LookUpPrefix(const string &input,
                            size_t *key_length,
                            bool *fixed) const;
  bool HasSubRules(const string &input) const;

  bool case_sensitive() const;
  void set_case_sensitive(bool case_sensitive);

 private:
  bool LoadFromStream(istream *is);
  void DeleteEntry(const Entry *entry);

  typedef Trie<const Entry*> EntryTrie;
  scoped_ptr<EntryTrie> entries_;
  typedef set<const Entry*> EntrySet;
  EntrySet entry_set_;

  // If false, input alphabet characters are normalized to lower
  // characters.  The default value is false.
  bool case_sensitive_;
};

}  // namespace composer
}  // namespace mozc

#endif  // MOZC_COMPOSER_TABLE_H_
