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

#ifndef MOZC_DICTIONARY_DICTIONARY_TOKEN_H_
#define MOZC_DICTIONARY_DICTIONARY_TOKEN_H_

#include <string>

#include "base/port.h"

namespace mozc {

struct Token {
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
    USER_DICTIONARY = 1 << 7,
  };

  Token() : cost(0), lid(0), rid(0), attributes(NONE) {}

  string key;
  string value;
  int    cost;
  int    lid;
  int    rid;
  uint8 attributes;  // Bit field of Attributes.
};

}  // namespace mozc

#endif  // MOZC_DICTIONARY_DICTIONARY_TOKEN_H_
