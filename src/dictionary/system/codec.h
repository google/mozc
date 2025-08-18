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

#ifndef MOZC_DICTIONARY_SYSTEM_CODEC_H_
#define MOZC_DICTIONARY_SYSTEM_CODEC_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "dictionary/system/codec_interface.h"

namespace mozc {
namespace dictionary {

class SystemDictionaryCodec : public SystemDictionaryCodecInterface {
 public:
  // Return section name for key trie
  std::string GetSectionNameForKey() const override;

  // Return section name for value trie
  std::string GetSectionNameForValue() const override;

  // Return section name for tokens array
  std::string GetSectionNameForTokens() const override;

  // Return section name for frequent pos map
  std::string GetSectionNameForPos() const override;

  // Compresses key string into small bytes.
  void EncodeKey(absl::string_view src, std::string* dst) const override;

  // Decompress key string
  void DecodeKey(absl::string_view src, std::string* dst) const override;

  // Returns the length of encoded key string.
  size_t GetEncodedKeyLength(absl::string_view src) const override;

  // Returns the length of decoded key string.
  size_t GetDecodedKeyLength(absl::string_view src) const override;

  // Compresses value string into small bytes.
  void EncodeValue(absl::string_view src, std::string* dst) const override;

  // Decompress value string
  void DecodeValue(absl::string_view src, std::string* dst) const override;

  // Compress tokens
  void EncodeTokens(absl::Span<const TokenInfo> tokens,
                    std::string* output) const override;

  // Decompress tokens
  void DecodeTokens(const uint8_t* ptr,
                    std::vector<TokenInfo>* tokens) const override;

  // Decompress a token.
  bool DecodeToken(const uint8_t* ptr, TokenInfo* token_info,
                   int* read_bytes) const override;

  // Read a token for reverse lookup
  // If the token have value id, assign it to |id_in_value_trie|
  // otherwise assign -1
  // Return false if a token is the last token for a certain key
  bool ReadTokenForReverseLookup(const uint8_t* ptr, int* value_id,
                                 int* read_bytes) const override;

  uint8_t GetTokensTerminationFlag() const override;

 private:
  void EncodeToken(absl::Span<const TokenInfo> tokens, int index,
                   std::string* output) const;
};

}  // namespace dictionary
}  // namespace mozc

#endif  // MOZC_DICTIONARY_SYSTEM_CODEC_H_
