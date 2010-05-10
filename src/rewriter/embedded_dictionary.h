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

#ifndef MOZC_REWRITER_EMBEDDED_DICTIONARY_H_
#define MOZC_REWRITER_EMBEDDED_DICTIONARY_H_

#include <string>
#include "base/base.h"

namespace mozc {

// EmbeddedDictionary is used for
// - SingleKanji dictionary
// - Symbol dictionary
class EmbeddedDictionary {
 public:
  struct Value {
    const char *value;
    const char *description;
    uint16 lid;
    uint16 rid;
    int16  cost;
  };

  struct Token {
    const  char *key;
    const  Value *value;
    size_t value_size;
  };

  // Initialize dictionary with a constant token table
  // generated with Compile method
  EmbeddedDictionary(const Token *token, size_t size);
  virtual ~EmbeddedDictionary();

  // Lookup key. Return NULL if no key is found.
  // Usage:
  // token = dic.Lookup(key);
  // if (value != token) {
  //   for (size_t i = 0; i < token->value_size; ++i) {
  //     cout << token->value[i].value;
  //   }
  // }
  const Token *Lookup(const string &key) const;

  // Given mozc-dictionary tsv file (e.g, data/dictionary/dic.txt)
  // output *.h file that contains array of token.
  //
  // static const EmbeddedDictionary::Token k$(NAME)_token_data[] and
  // static const size_t k$(NAME)_token_size;
  static void Compile(const string &name,
                      const string &input,
                      const string &output);

 private:
  const Token *token_;
  const size_t size_;
};
}  // mozc

#endif  // MOZC_REWRITER_EMBEDDED_DICTIONARY_H_
