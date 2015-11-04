// Copyright 2010-2015, Google Inc.
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

#ifndef MOZC_PREDICTION_ZERO_QUERY_LIST_H_
#define MOZC_PREDICTION_ZERO_QUERY_LIST_H_

#include "base/port.h"

namespace mozc {
enum ZeroQueryType {
  ZERO_QUERY_NONE = 0,
  ZERO_QUERY_NUMBER_SUFFIX,
  ZERO_QUERY_EMOTICON,
  ZERO_QUERY_EMOJI,
};

// bit fields
enum ZeroQueryEmojiType {
  EMOJI_NONE = 0,
  EMOJI_UNICODE = 1,
  EMOJI_DOCOMO = 2,
  EMOJI_SOFTBANK = 4,
  EMOJI_KDDI = 8,
};

struct ZeroQueryEntry {
  ZeroQueryType type;
  const char *value;
  uint8 emoji_type;  // ZeroQueryEmojiType
  // The carrier dependent emoji code point on Android.
  uint32 emoji_android_pua;
};

struct ZeroQueryList {
  const char *key;
  const ZeroQueryEntry *entries;
  const size_t entries_size;
};
}  // namespace mozc

#endif  // MOZC_PREDICTION_ZERO_QUERY_LIST_H_
