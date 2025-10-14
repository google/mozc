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

#ifndef MOZC_DICTIONARY_DICTIONARY_TOKEN_H_
#define MOZC_DICTIONARY_DICTIONARY_TOKEN_H_

#include <cstdint>
#include <string>

#include "absl/strings/string_view.h"

namespace mozc {
namespace dictionary {

struct Token {
  using AttributesBitfield = uint8_t;

  enum Attribute {
    NONE = 0,
    SPELLING_CORRECTION = 1,
    LABEL_SIZE = 2,
    // * CAUTION *
    // If you are going to add new attributes, make sure that they have larger
    // values than LABEL_SIZE!! The attributes having less values than it are
    // tightly integrated with the system dictionary codec.

    // The following attribute is not stored in the system dictionary but is
    // added by dictionary modules when looking up from user dictionary.
    SUFFIX_DICTIONARY = 1 << 6,
    USER_DICTIONARY = 1 << 7,
  };

  Token() = default;
  Token(absl::string_view k, absl::string_view v) : key(k), value(v) {}
  Token(absl::string_view k, absl::string_view v, int c, uint16_t l, uint16_t r,
        AttributesBitfield a)
      : key(k), value(v), cost(c), lid(l), rid(r), attributes(a) {}

  std::string key;
  std::string value;
  int cost = 0;
  uint16_t lid = 0;
  uint16_t rid = 0;
  AttributesBitfield attributes = NONE;
};

}  // namespace dictionary
}  // namespace mozc

#endif  // MOZC_DICTIONARY_DICTIONARY_TOKEN_H_
