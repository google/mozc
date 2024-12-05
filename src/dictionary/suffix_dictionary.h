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

#ifndef MOZC_DICTIONARY_SUFFIX_DICTIONARY_H_
#define MOZC_DICTIONARY_SUFFIX_DICTIONARY_H_

#include <cstdint>

#include "absl/strings/string_view.h"
#include "base/container/serialized_string_array.h"
#include "dictionary/dictionary_interface.h"
#include "request/conversion_request.h"

namespace mozc {
namespace dictionary {

// SuffixDictionary is a special dictionary which handles
// Japanese bunsetsu suffixes.
//
// Japanese bunsetsu consists of two parts.
// Content words: ("自立語") and Functional words ("付属語")
// Japanese Bunsets = (Content word){1,1}(Functional words){1,}
//
// Suffix dictionary contains sequences of functional words
// frequently appear on the web.
// When user inputs a content word, we can predict an appropriate
// functional word with this dictionary.
class SuffixDictionary : public DictionaryInterface {
 public:
  SuffixDictionary(absl::string_view key_array_data,
                   absl::string_view value_array_data,
                   absl::Span<const uint32_t> token_array);
  SuffixDictionary(const SuffixDictionary &) = delete;
  SuffixDictionary &operator=(const SuffixDictionary &) = delete;
  ~SuffixDictionary() override = default;

  bool HasKey(absl::string_view key) const override;
  bool HasValue(absl::string_view value) const override;

  // Kana modifier insensitive lookup is not supported.
  void LookupPredictive(absl::string_view key,
                        const ConversionRequest &conversion_request,
                        Callback *callback) const override;

  // SuffixDictionary doesn't support Prefix/Revese/Exact Lookup.
  void LookupPrefix(absl::string_view key,
                    const ConversionRequest &conversion_request,
                    Callback *callback) const override {}

  void LookupExact(absl::string_view key,
                   const ConversionRequest &conversion_request,
                   Callback *callback) const override {}

  void LookupReverse(absl::string_view key,
                     const ConversionRequest &conversion_request,
                     Callback *callback) const override {}

 private:
  SerializedStringArray key_array_;
  SerializedStringArray value_array_;
  const absl::Span<const uint32_t> token_array_;
};

}  // namespace dictionary
}  // namespace mozc

#endif  // MOZC_DICTIONARY_SUFFIX_DICTIONARY_H_
