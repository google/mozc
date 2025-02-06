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

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "base/container/serialized_string_array.h"
#include "converter/segments.h"
#include "data_manager/data_manager.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/pos_matcher.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"
#include "testing/friend_test.h"

namespace mozc {

class UsageRewriter : public RewriterInterface {
 public:
  UsageRewriter(const DataManager &data_manager,
                const dictionary::DictionaryInterface &dictionary);
  ~UsageRewriter() override = default;
  bool Rewrite(const ConversionRequest &request,
               Segments *segments) const override;

  // better to show usage when user type "tab" key.
  int capability(const ConversionRequest &request) const override {
    return CONVERSION | PREDICTION;
  }

 private:
  FRIEND_TEST(UsageRewriterTest, GetKanjiPrefixAndOneHiragana);

  static constexpr size_t kUsageItemSize = 5;

  class UsageDictItemIterator {
   public:
    using iterator_cateogry = std::input_iterator_tag;
    using value_type = size_t;

    UsageDictItemIterator() : ptr_(nullptr) {}
    explicit UsageDictItemIterator(const char *ptr)
        : ptr_(std::launder(reinterpret_cast<const uint32_t *>(ptr))) {}

    size_t usage_id() const { return *ptr_; }
    size_t key_index() const { return *(ptr_ + 1); }
    size_t value_index() const { return *(ptr_ + 2); }
    size_t conjugation_id() const { return *(ptr_ + 3); }
    size_t meaning_index() const { return *(ptr_ + 4); }

    UsageDictItemIterator &operator++() {
      ptr_ += kUsageItemSize;
      return *this;
    }

    bool IsValid() const { return ptr_ != nullptr; }

    friend bool operator==(UsageDictItemIterator x, UsageDictItemIterator y) {
      return x.ptr_ == y.ptr_;
    }

    friend bool operator!=(UsageDictItemIterator x, UsageDictItemIterator y) {
      return x.ptr_ != y.ptr_;
    }

   private:
    const uint32_t *ptr_;
  };

  using StrPair = std::pair<std::string, std::string>;
  static std::string GetKanjiPrefixAndOneHiragana(absl::string_view word);

  UsageDictItemIterator LookupUnmatchedUsageHeuristically(
      const Segment::Candidate &candidate) const;
  UsageDictItemIterator LookupUsage(const Segment::Candidate &candidate) const;

  absl::flat_hash_map<StrPair, UsageDictItemIterator> key_value_usageitem_map_;
  const dictionary::PosMatcher pos_matcher_;
  const dictionary::DictionaryInterface *dictionary_;
  const uint32_t *base_conjugation_suffix_;
  SerializedStringArray string_array_;

 private:
  friend class UsageRewriterPeer;
};

}  // namespace mozc

#endif  // NO_USAGE_REWRITER

#endif  // MOZC_REWRITER_USAGE_REWRITER_H_
