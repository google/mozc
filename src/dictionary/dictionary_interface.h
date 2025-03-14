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

#ifndef MOZC_DICTIONARY_DICTIONARY_INTERFACE_H_
#define MOZC_DICTIONARY_DICTIONARY_INTERFACE_H_

#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "dictionary/dictionary_token.h"
#include "protocol/user_dictionary_storage.pb.h"
#include "request/conversion_request.h"

namespace mozc {
namespace dictionary {

// DictionaryInterface only defines pure immutable lookup operations.
// mutable operations, e.g., Reload, Load are defined
// in the subclass UserDictionaryInterface.
class DictionaryInterface {
 public:
  // Callback interface for dictionary traversal (currently implemented only for
  // prefix and exact search). Each method is called in the following manner:
  //
  // for (each key found) {
  //   OnKey(key);
  //   OnActualKey(key, actual_key, key != actual_key);
  //   for (each token in the token array for the key) {
  //     OnToken(key, actual_key, token);
  //   }
  // }
  //
  // Using the return value of each call of the three methods, you can tell the
  // traverser how to proceed. The meanings of the four values are as follows:
  //   1) TRAVERSE_DONE
  //       Quit the traversal, i.e., no more callbacks for keys and/or tokens.
  //   2) TRAVERSE_NEXT_KEY
  //       Finish the traversal for the current key and search for the next key.
  //       If returned from OnToken(), the remaining tokens are discarded.
  //   3) TRAVERSE_CULL
  //       Similar to TRAVERSE_NEXT_KEY, finish the traversal for the current
  //       key but search for the next key by using search culling. Namely,
  //       traversal of the subtree starting with the current key is skipped,
  //       which is the difference from TRAVERSE_NEXT_KEY.
  //   4) TRAVERSE_CONTINUE
  //       Continue the traversal for the current key or tokens, namely:
  //         - If returned from OnKey(), OnActualKey() will be called back.
  //         - If returned from OnActualKey(), a series of OnToken()'s will be
  //           called back.
  //         - If returned from OnToken(), OnToken() will be called back again
  //           with the next token, provided that it exists. Proceed to the next
  //           key if there's no more token.
  class Callback {
   public:
    enum ResultType {
      TRAVERSE_DONE,
      TRAVERSE_NEXT_KEY,
      TRAVERSE_CULL,
      TRAVERSE_CONTINUE,
    };

    virtual ~Callback() = default;

    // Called back when key is found.
    virtual ResultType OnKey(absl::string_view key) {
      return TRAVERSE_CONTINUE;
    }

    // Called back when actual key is decoded. `num_expanded` is the number
    // of different characters between key and actual_key.
    virtual ResultType OnActualKey(absl::string_view key,
                                   absl::string_view actual_key,
                                   int num_expanded) {
      return TRAVERSE_CONTINUE;
    }

    // Called back when a token is decoded.
    virtual ResultType OnToken(absl::string_view key,
                               absl::string_view expanded_key,
                               const Token &token_info) {
      return TRAVERSE_CONTINUE;
    }

   protected:
    Callback() = default;
  };

  virtual ~DictionaryInterface() = default;

  // Returns true if the dictionary has an entry for the given key.
  virtual bool HasKey(absl::string_view key) const = 0;

  // Returns true if the dictionary has an entry for the given value.
  virtual bool HasValue(absl::string_view value) const = 0;

  // Looks up values whose keys start from the key.
  // (e.g. key = "abc" -> {"abc": "ABC", "abcd": "ABCD"})
  virtual void LookupPredictive(absl::string_view key,
                                const ConversionRequest &conversion_request,
                                Callback *callback) const = 0;

  // Looks up values whose keys are prefixes of the key.
  // (e.g. key = "abc" -> {"abc": "ABC", "a": "A"})
  virtual void LookupPrefix(absl::string_view key,
                            const ConversionRequest &conversion_request,
                            Callback *callback) const = 0;

  // Looks up values whose keys are same with the key.
  // (e.g. key = "abc" -> {"abc": "ABC"})
  virtual void LookupExact(absl::string_view key,
                           const ConversionRequest &conversion_request,
                           Callback *callback) const = 0;

  // For reverse lookup, the reading is stored in Token::value and the word
  // is stored in Token::key.
  virtual void LookupReverse(absl::string_view str,
                             const ConversionRequest &conversion_request,
                             Callback *callback) const = 0;

  // Looks up a user comment from a pair of key and value.  When (key, value)
  // doesn't exist in this dictionary or user comment is empty, bool is
  // returned and string is kept as-is.
  virtual bool LookupComment(absl::string_view key, absl::string_view value,
                             const ConversionRequest &conversion_request,
                             std::string *comment) const {
    return false;
  }

  // Populates cache for LookupReverse().
  // TODO(noriyukit): These cache initialize/finalize mechanism shouldn't be a
  // part of the interface.
  virtual void PopulateReverseLookupCache(absl::string_view str) const {}
  virtual void ClearReverseLookupCache() const {}

 protected:
  // Do not allow instantiation
  DictionaryInterface() = default;
};

class UserDictionaryInterface : public DictionaryInterface {
 public:
  virtual ~UserDictionaryInterface() = default;

  // Waits until reloader finishes
  virtual void WaitForReloader() = 0;

  // Gets the user POS list.
  virtual std::vector<std::string> GetPosList() const = 0;

  // Loads dictionary from UserDictionaryStorage.
  // mainly for unit testing
  virtual bool Load(const user_dictionary::UserDictionaryStorage &storage) = 0;

  // Suppress `key` and `value` with suppression dictionary.
  // Suppression entries are defined in the user dictionary with a special
  // POS.
  virtual bool IsSuppressedEntry(absl::string_view key,
                                 absl::string_view value) const = 0;

  // Return true if the dictionary has at least one suppression entry.
  virtual bool HasSuppressedEntries() const = 0;

  // Reload dictionary data from local disk.
  virtual bool Reload() { return true; }
};

}  // namespace dictionary
}  // namespace mozc

#endif  // MOZC_DICTIONARY_DICTIONARY_INTERFACE_H_
