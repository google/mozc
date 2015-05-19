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

#ifndef MOZC_DICTIONARY_DICTIONARY_INTERFACE_H_
#define MOZC_DICTIONARY_DICTIONARY_INTERFACE_H_

#include <string>
#include <vector>

#include "base/port.h"
#include "base/string_piece.h"
#include "base/trie.h"

namespace mozc {

class NodeAllocatorInterface;  // converter/node.h
struct Node;                   // converter/node.h
struct Token;                  // dictionary/dictionary_token.h

// TODO(noriyukit): Move this interface into dictionary namespace.
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

    virtual ~Callback() {}

    // Called back when key is found.
    virtual ResultType OnKey(StringPiece key) {
      return TRAVERSE_CONTINUE;
    }

    // Called back when actual key is decoded. The third argument is guaranteed
    // to be (key != actual_key) but computed in an efficient way.
    virtual ResultType OnActualKey(
        StringPiece key, StringPiece actual_key, bool is_expanded) {
      return TRAVERSE_CONTINUE;
    }

    // Called back when a token is decoded.
    virtual ResultType OnToken(StringPiece key,
                               StringPiece expanded_key,
                               const Token &token_info) {
      return TRAVERSE_CONTINUE;
    }

   protected:
    Callback() {}
  };

  // limitation for LookupPrefixWithLimit
  struct Limit {
    // For prefix lookup, to reduce redundant lookup for making lattice,
    // returns only nodes which key length is >= this limit.
    int key_len_lower_limit;
    // For predictive lookup,
    // returns only nodes which predicted key starts with the string in the trie
    // Example:
    //  predictive lookup key: 'conv'
    //  begin with list: 'er', 'in'
    //  In this case, we will get 'convert', converter', 'convince', etc.
    //  We will not get 'convention', etc.
    // This does not have the ownership
    const Trie<string> *begin_with_trie;

    // For predictive and prefix lookup, enables ambiguous search.
    bool kana_modifier_insensitive_lookup_enabled;
    Limit() :
        key_len_lower_limit(0),
        begin_with_trie(NULL),
        kana_modifier_insensitive_lookup_enabled(false) {
    }
  };

  virtual ~DictionaryInterface() {}

  // Returns true if the dictionary has an entry for the given value.
  virtual bool HasValue(const StringPiece value) const = 0;

  virtual void LookupPredictiveWithCallback(
      StringPiece key, bool use_kana_modifier_insensitive_lookup,
      Callback *callback) const {
    // TODO(noriyukit): Remove the default implementation after removing
    // LookupPredictiveWithLimit and LookupPredictive.
  }

  // For Lookup methods, dictionary does not manage the ownerships of the
  // returned Node objects.
  // If the |allocator| is specified, we will use it to make Node objects.

  virtual Node *LookupPredictiveWithLimit(
      const char *str, int size, const Limit &limit,
      NodeAllocatorInterface *allocator) const = 0;

  virtual Node *LookupPredictive(const char *str, int size,
                                 NodeAllocatorInterface *allocator) const = 0;

  virtual void LookupPrefix(
      StringPiece key, bool use_kana_modifier_insensitive_lookup,
      Callback *callback) const = 0;

  virtual void LookupExact(StringPiece key, Callback *callback) const = 0;

  // For reverse lookup, the reading is stored in Node::value and the word
  // is stored in Node::key.
  virtual Node *LookupReverse(const char *str, int size,
                              NodeAllocatorInterface *allocator) const = 0;

  // Looks up a user comment from a pair of key and value.  When (key, value)
  // doesn't exist in this dictionary or user comment is empty, bool is
  // returned and string is kept as-is.
  virtual bool LookupComment(StringPiece key, StringPiece value,
                             string *comment) const { return false; }

  virtual void PopulateReverseLookupCache(
      const char *str, int size, NodeAllocatorInterface *allocator) const {}
  virtual void ClearReverseLookupCache(
      NodeAllocatorInterface *allocator) const {}

  // Sync mutable dictionary data into local disk.
  virtual bool Sync() { return true; }

  // Reload dictionary data from local disk.
  virtual bool Reload() { return true; }

 protected:
  // Do not allow instantiation
  DictionaryInterface() {}
};

}  // namespace mozc

#endif  // MOZC_DICTIONARY_DICTIONARY_INTERFACE_H_
