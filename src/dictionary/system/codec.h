// Copyright 2010-2012, Google Inc.
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

#ifndef MOZC_DICTIONARY_SYSTEM_CODEC_H_
#define MOZC_DICTIONARY_SYSTEM_CODEC_H_

#include "dictionary/system/codec_interface.h"

#include <string>
#include <vector>

#include "base/base.h"
#include "dictionary/system/words_info.h"

namespace mozc {
namespace dictionary {

class SystemDictionaryCodec : public SystemDictionaryCodecInterface {
 public:
  SystemDictionaryCodec();

  virtual ~SystemDictionaryCodec();

  // Return section name for key trie
  virtual const string GetSectionNameForKey() const;

  // Return section name for value trie
  virtual const string GetSectionNameForValue() const;

  // Return section name for tokens array
  virtual const string GetSectionNameForTokens() const;

  // Return section name for frequent pos map
  virtual const string GetSectionNameForPos() const;

  // Compresses key string into small bytes.
  virtual void EncodeKey(const string &src, string *dst) const;

  // Decompress key string
  virtual void DecodeKey(const string &src, string *dst) const;

  // Compresses value string into small bytes.
  virtual void EncodeValue(const string &src, string *dst) const;

  // Decompress value string
  virtual void DecodeValue(const string &src, string *dst) const;

  // Compress tokens
  virtual void EncodeTokens(
      const vector<TokenInfo> &tokens, string *output) const;

  // Decompress tokens
  virtual void DecodeTokens(const uint8 *ptr, vector<TokenInfo> *tokens) const;

  // Decompress a token.
  virtual bool DecodeToken(
      const uint8 *ptr, TokenInfo *token_info, int *read_bytes) const;

  // Read a token for reverse lookup
  // If the token have value id, assign it to |id_in_value_trie|
  // otherwise assign -1
  // Return false if a token is the last token for a certain key
  virtual bool ReadTokenForReverseLookup(
      const uint8 *ptr, int *value_id, int *read_bytes) const;

  virtual uint8 GetTokensTerminationFlag() const;

 private:
  void EncodeToken(
      const vector<TokenInfo> &tokens, int index, string *output) const;

  DISALLOW_COPY_AND_ASSIGN(SystemDictionaryCodec);
};
}  // namespace dictionary
}  // namespace mozc

#endif  // MOZC_DICTIONARY_SYSTEM_CODEC_H_
