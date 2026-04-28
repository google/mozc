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

#ifndef MOZC_REWRITER_USAGE_REWRITER_H_
#define MOZC_REWRITER_USAGE_REWRITER_H_

#ifndef NO_USAGE_REWRITER
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <new>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "base/bits.h"
#include "base/container/serialized_string_array.h"
#include "converter/candidate.h"
#include "converter/segments.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/pos_matcher.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"

namespace mozc {

class UsageRewriter : public RewriterInterface {
 public:
  UsageRewriter(absl::string_view base_conjugation_suffix_data,
                absl::string_view conjugation_suffix_data,
                absl::string_view conjugation_index_data,
                absl::string_view usage_items_data,
                absl::string_view string_array_data,
                const dictionary::DictionaryInterface& dictionary,
                dictionary::PosMatcher pos_matcher);
  ~UsageRewriter() override = default;
  bool Rewrite(const ConversionRequest& request,
               Segments* segments) const override;

  // better to show usage when user type "tab" key.
  int capability(const ConversionRequest& request) const override {
    return CONVERSION | PREDICTION;
  }

 private:
  friend class UsageRewriterTestPeer;

  struct UsageDictItem {
    uint32_t usage_id = 0;
    uint32_t key_index = 0;
    uint32_t value_index = 0;
    uint32_t conjugation_id = 0;
    uint32_t meaning_index = 0;
  } ABSL_ATTRIBUTE_PACKED;

  static_assert(sizeof(UsageDictItem) == 20);
  ASSERT_ALIGNED(UsageDictItem, usage_id);
  ASSERT_ALIGNED(UsageDictItem, key_index);
  ASSERT_ALIGNED(UsageDictItem, value_index);
  ASSERT_ALIGNED(UsageDictItem, conjugation_id);
  ASSERT_ALIGNED(UsageDictItem, meaning_index);

  using StrPair = std::pair<std::string, std::string>;
  static std::string GetKanjiPrefixAndOneHiragana(absl::string_view word);

  const UsageDictItem* LookupUnmatchedUsageHeuristically(
      const converter::Candidate& candidate) const;
  const UsageDictItem* LookupUsage(const converter::Candidate& candidate) const;

  absl::flat_hash_map<StrPair, const UsageDictItem*> key_value_usageitem_map_;
  const dictionary::DictionaryInterface& dictionary_;
  const dictionary::PosMatcher pos_matcher_;
  const uint32_t* base_conjugation_suffix_ = nullptr;
  SerializedStringArray string_array_;

 private:
  friend class UsageRewriterPeer;
};

}  // namespace mozc

#endif  // NO_USAGE_REWRITER

#endif  // MOZC_REWRITER_USAGE_REWRITER_H_
