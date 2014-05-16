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

// A mock class for testing.
//
// Note: Entries added by a method cannot be retrieved by other methods.
//       For example, even if you add an entry with a key "きょうと" and value
//       "京都" by AddLookupPrefix, you will get no results by calling
//       LookupReverse() with the key "京都" or by calling LookupPredictive()
//       with the key "きょう".
//       If you want results from a lookup method, you have to call the
//       corresponding Add...() method beforehand.
//       LookupPredictive() doesn't return results by any other strings other
//       than the one used in AddLookupPredictive(); If you have called
//       AddLookupPredictive("きょう", "きょうと", "京都"), you will get no
//       results by calling, for example, LookupPredictive("き", ...).
//       On the other hand, LookupPrefix() and LookupReverse() actually performs
//       prefix searches. If you have called
//       AddLookupPrefix("きょうと", "きょうと", "京都"), you will get a result
//       calling LookupPrefix("きょうとだいがく", ...).
//       Tokens looked up by these methods all have the "lid" and "rid" with the
//       value 1.
// TODO(manabe): An interface to change "lid"/"rid".

#ifndef MOZC_DICTIONARY_DICTIONARY_MOCK_H_
#define MOZC_DICTIONARY_DICTIONARY_MOCK_H_

#include <list>
#include <map>
#include <string>
#include <vector>

#include "base/port.h"
#include "base/string_piece.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_token.h"

namespace mozc {

// These structures are defined in converter
class NodeAllocatorInterface;

class DictionaryMock : public DictionaryInterface {
 public:
  DictionaryMock();
  virtual ~DictionaryMock();

  virtual bool HasValue(StringPiece value) const;

  // DictionaryMock doesn't support a limitation.  Note also that only the
  // tokens whose keys exactly match the registered key are looked up; see the
  // comment of AddLookupPredictive.
  virtual void LookupPredictive(
      StringPiece key, bool use_kana_modifier_insensitive_lookup,
      Callback *callback) const;

  virtual void LookupPrefix(
      StringPiece key, bool use_kana_modifier_insensitive_lookup,
      Callback *callback) const;

  virtual void LookupExact(StringPiece key, Callback *callback) const;

  // For reverse lookup, the reading is stored in Token::value and the word
  // is stored in Token::key. This mock method doesn't use |*allocator|.
  virtual void LookupReverse(StringPiece str, NodeAllocatorInterface *allocator,
                             Callback *callback) const;

  // Adds a string-result pair to the predictive search result.
  // LookupPrefix will return the result only when the search key exactly
  // matches the string registered by this function.
  // Note that str and key are not necessarily the same, as in the case of the
  // spelling correction. (This applies to other types of searches.)
  void AddLookupPredictive(const string &str,
                           const string &key, const string &value,
                           Token::AttributesBitfield token_attributes);

  // Adds a string-token pair to the prefix search result.
  // LookupPrefix will return the result when the left part of the search key
  // partially matches the string registered by this function.
  void AddLookupPrefix(const string &str,
                       const string &key, const string &value,
                       Token::AttributesBitfield token_attributes);

  // Adds a string-token pair to the reverse search result.
  // Same as AddLookupPrefix, but they have different dictionaries
  // internally.
  void AddLookupReverse(const string &str,
                        const string &key, const string &value,
                        Token::AttributesBitfield token_attributes);

  // Adds a string-token pair to the exact search result.
  // Same as AddLookupPrefix, but they have different dictionaries
  // internally.
  void AddLookupExact(const string &str,
                      const string &key, const string &value,
                      Token::AttributesBitfield token_attributes);

 private:
  map<string, vector<Token *> > reverse_dictionary_;
  map<string, vector<Token *> > prefix_dictionary_;
  map<string, vector<Token *> > exact_dictionary_;
  map<string, vector<Token *> > predictive_dictionary_;

  DISALLOW_COPY_AND_ASSIGN(DictionaryMock);
};
}  // namespace mozc

#endif  // MOZC_DICTIONARY_DICTIONARY_MOCK_H_
