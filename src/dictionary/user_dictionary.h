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

#ifndef MOZC_DICTIONARY_USER_DICTIONARY_H_
#define MOZC_DICTIONARY_USER_DICTIONARY_H_

#include <string>
#include <vector>
#include "base/port.h"
#include "base/scoped_ptr.h"
#include "base/string_piece.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/user_dictionary_storage.pb.h"

namespace mozc {

class POSMatcher;
class ReaderWriterMutex;
class SuppressionDictionary;
class TokensIndex;   // defined in user_dictionary.cc
class UserDictionaryReloader;
class UserDictionaryStorage;
class UserPOSInterface;

class UserDictionary : public DictionaryInterface {
 public:
  UserDictionary(const UserPOSInterface *user_pos,
                 const POSMatcher *pos_matcher,
                 SuppressionDictionary *suppression_dictionary);
  virtual ~UserDictionary();

  virtual bool HasValue(const StringPiece value) const;
  virtual Node *LookupPredictiveWithLimit(
      const char *str, int size, const Limit &limit,
      NodeAllocatorInterface *allocator) const;
  virtual Node *LookupPredictive(const char *str, int size,
                                 NodeAllocatorInterface *allocator) const;
  // Kana modifier insensitive lookup is not supported, meaning that
  // Callback::OnActualKey() is never called.
  virtual void LookupPrefix(
      StringPiece key, bool use_kana_modifier_insensitive_lookup,
      Callback *callback) const;
  virtual void LookupExact(StringPiece key, Callback *callback) const;
  virtual Node *LookupReverse(const char *str, int size,
                              NodeAllocatorInterface *allocator) const;

  // Looks up a user comment from a pair of key and value.  When (key, value)
  // doesn't exist in this dictionary or user comment is empty, bool is
  // returned and string is kept as-is.
  virtual bool LookupComment(StringPiece key, StringPiece value,
                             string *comment) const;

  // Load dictionary from UserDictionaryStorage.
  // mainly for unittesting
  bool Load(const user_dictionary::UserDictionaryStorage &storage);

  // Reload dictionary asynchronously
  bool Reload();

  // Wait until reloader finishes
  void WaitForReloader();

  // Adds new word to auto registered dictionary and reload asynchronously.
  // Note that this method will not guarantee that
  // new word is added successfully, since the actual
  // dictionary modification is done by other thread.
  // Also, this method should be called by the main converter thread which
  // is executed synchronously with user input.
  bool AddToAutoRegisteredDictionary(
      const string &key, const string &value,
      user_dictionary::UserDictionary::PosType pos);

  // Set user dicitonary filename for unittesting
  static void SetUserDictionaryName(const string &filename);

 private:
  // Swap internal tokens index to |new_tokens|.
  void Swap(TokensIndex *new_tokens);

  friend class UserDictionaryTest;

  scoped_ptr<UserDictionaryReloader> reloader_;
  scoped_ptr<const UserPOSInterface> user_pos_;
  const POSMatcher *pos_matcher_;
  SuppressionDictionary *suppression_dictionary_;
  const Limit empty_limit_;
  TokensIndex *tokens_;
  mutable scoped_ptr<ReaderWriterMutex> mutex_;

  DISALLOW_COPY_AND_ASSIGN(UserDictionary);
};

}  // namespace mozc

#endif  // MOZC_DICTIONARY_USER_DICTIONARY_H_
