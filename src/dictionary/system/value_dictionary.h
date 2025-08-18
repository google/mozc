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

// This dictionary supports system dicitonary that will be looked up from
// value, rather than key.
// Value dictionary is mainly used to lookup the English words from
// user-typed literal sequences. (English has no 'reading' representation.)
// Token's key, cost, ids will not be looked up due to performance concern

#ifndef MOZC_DICTIONARY_SYSTEM_VALUE_DICTIONARY_H_
#define MOZC_DICTIONARY_SYSTEM_VALUE_DICTIONARY_H_

#include <cstdint>

#include "absl/strings/string_view.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/system/codec_interface.h"
#include "request/conversion_request.h"
#include "storage/louds/louds_trie.h"

namespace mozc {
namespace dictionary {

class ValueDictionary : public DictionaryInterface {
 public:
  // This class doesn't take the ownership of |value_trie|.
  ValueDictionary(const PosMatcher& pos_matcher,
                  const storage::louds::LoudsTrie* value_trie);

  ValueDictionary(const ValueDictionary&) = delete;
  ValueDictionary& operator=(const ValueDictionary&) = delete;

  // Implementation of DictionaryInterface
  bool HasKey(absl::string_view key) const override;
  bool HasValue(absl::string_view value) const override;
  void LookupPredictive(absl::string_view key,
                        const ConversionRequest& conversion_request,
                        Callback* callback) const override;
  void LookupPrefix(absl::string_view key,
                    const ConversionRequest& conversion_request,
                    Callback* callback) const override;
  void LookupExact(absl::string_view key,
                   const ConversionRequest& conversion_request,
                   Callback* callback) const override;
  void LookupReverse(absl::string_view str,
                     const ConversionRequest& conversion_request,
                     Callback* callback) const override;

 private:
  const storage::louds::LoudsTrie* value_trie_;
  const SystemDictionaryCodecInterface* codec_;
  const uint16_t suggestion_only_word_id_;
};

}  // namespace dictionary
}  // namespace mozc

#endif  // MOZC_DICTIONARY_SYSTEM_VALUE_DICTIONARY_H_
