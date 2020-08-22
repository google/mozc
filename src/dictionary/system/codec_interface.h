// Copyright 2010-2020, Google Inc.
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

#ifndef MOZC_DICTIONARY_SYSTEM_CODEC_INTERFACE_H_
#define MOZC_DICTIONARY_SYSTEM_CODEC_INTERFACE_H_

#include <string>
#include <vector>

#include "base/port.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace dictionary {

struct TokenInfo;

class SystemDictionaryCodecInterface {
 public:
  virtual ~SystemDictionaryCodecInterface() {}

  // Get section name functions are expected not to be called so often

  // Return section name for key trie
  virtual const std::string GetSectionNameForKey() const = 0;

  // Return section name for value trie
  virtual const std::string GetSectionNameForValue() const = 0;

  // Return section name for tokens array
  virtual const std::string GetSectionNameForTokens() const = 0;

  // Return section name for frequent pos map
  virtual const std::string GetSectionNameForPos() const = 0;

  // Encode value(word) string
  virtual void EncodeValue(const absl::string_view src,
                           std::string *dst) const = 0;

  // Decode value(word) string
  virtual void DecodeValue(const absl::string_view src,
                           std::string *dst) const = 0;

  // Encode key(reading) string
  virtual void EncodeKey(const absl::string_view src,
                         std::string *dst) const = 0;

  // Decode key(reading) string
  virtual void DecodeKey(const absl::string_view src,
                         std::string *dst) const = 0;

  // Returns the length of encoded key string.
  virtual size_t GetEncodedKeyLength(const absl::string_view src) const = 0;

  // Returns the length of decoded key string.
  virtual size_t GetDecodedKeyLength(const absl::string_view src) const = 0;

  // Encode tokens(word info) for a certain key
  virtual void EncodeTokens(const std::vector<TokenInfo> &tokens,
                            std::string *output) const = 0;

  // Decode token(word info) for a certain key
  virtual void DecodeTokens(const uint8 *ptr,
                            std::vector<TokenInfo> *tokens) const = 0;

  // Decode a token. If the token is the last one, returns false,
  // otherwise true.
  virtual bool DecodeToken(const uint8 *ptr, TokenInfo *token_info,
                           int *read_bytes) const = 0;

  // Read a token for reverse lookup
  // If the token have value id, assign it to |value_id|
  // otherwise assign -1
  // Return false if a token is the last token for a certain key
  virtual bool ReadTokenForReverseLookup(const uint8 *ptr, int *value_id,
                                         int *read_bytes) const = 0;

  // Return termination flag for tokens
  virtual uint8 GetTokensTerminationFlag() const = 0;

 protected:
  SystemDictionaryCodecInterface() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemDictionaryCodecInterface);
};

class SystemDictionaryCodecFactory {
 public:
  static SystemDictionaryCodecInterface *GetCodec();
  static void SetCodec(SystemDictionaryCodecInterface *codec);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(SystemDictionaryCodecFactory);
};

}  // namespace dictionary
}  // namespace mozc

#endif  // MOZC_DICTIONARY_SYSTEM_CODEC_INTERFACE_H_
