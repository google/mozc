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

#include "dictionary/dictionary_impl.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "absl/algorithm/container.h"
#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/util.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/pos_matcher.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"

namespace mozc {
namespace dictionary {

DictionaryImpl::DictionaryImpl(
    std::unique_ptr<const DictionaryInterface> system_dictionary,
    std::unique_ptr<const DictionaryInterface> value_dictionary,
    const UserDictionaryInterface& user_dictionary,
    const PosMatcher& pos_matcher)
    : pos_matcher_(pos_matcher),
      system_dictionary_(std::move(system_dictionary)),
      value_dictionary_(std::move(value_dictionary)),
      user_dictionary_(user_dictionary) {
  CHECK(system_dictionary_.get());
  CHECK(value_dictionary_.get());
  dics_.push_back(system_dictionary_.get());
  dics_.push_back(value_dictionary_.get());
  dics_.push_back(&user_dictionary_);
}

DictionaryImpl::~DictionaryImpl() { dics_.clear(); }

bool DictionaryImpl::HasKey(absl::string_view key) const {
  return absl::c_any_of(dics_, [&key](const DictionaryInterface* dic) {
    return dic->HasKey(key);
  });
}

bool DictionaryImpl::HasValue(absl::string_view value) const {
  return absl::c_any_of(dics_, [&value](const DictionaryInterface* dic) {
    return dic->HasValue(value);
  });
}

namespace {

class CallbackWithFilter : public DictionaryInterface::Callback {
 public:
  CallbackWithFilter(const ConversionRequest& request,
                     const PosMatcher& pos_matcher,
                     const UserDictionaryInterface& user_dictionary,
                     DictionaryInterface::Callback* callback)
      : request_(request),
        pos_matcher_(pos_matcher),
        user_dictionary_(user_dictionary),
        has_suppressed_entries_(user_dictionary_.HasSuppressedEntries()),
        callback_(callback) {}

  ResultType OnKey(absl::string_view key) override {
    return callback_->OnKey(key);
  }

  ResultType OnActualKey(absl::string_view key, absl::string_view actual_key,
                         int num_expanded) override {
    return callback_->OnActualKey(key, actual_key, num_expanded);
  }

  ResultType OnToken(absl::string_view key, absl::string_view actual_key,
                     const Token& token) override {
    if (!(token.attributes & Token::USER_DICTIONARY)) {
      if (!request_.config().use_spelling_correction() &&
          (token.attributes & Token::SPELLING_CORRECTION)) {
        return TRAVERSE_CONTINUE;
      }
      if (!request_.config().use_zip_code_conversion() &&
          pos_matcher_.IsZipcode(token.lid)) {
        return TRAVERSE_CONTINUE;
      }
      if (!request_.config().use_t13n_conversion() &&
          Util::IsEnglishTransliteration(token.value)) {
        return TRAVERSE_CONTINUE;
      }
    }
    if (has_suppressed_entries_ &&
        user_dictionary_.IsSuppressedEntry(token.key, token.value)) {
      return TRAVERSE_CONTINUE;
    }
    return callback_->OnToken(key, actual_key, token);
  }

  bool IsKanaModifierInsensitiveConversion() const override {
    return request_.IsKanaModifierInsensitiveConversion();
  }

 private:
  const ConversionRequest& request_;
  const PosMatcher& pos_matcher_;
  const UserDictionaryInterface& user_dictionary_;
  // cache the result of HasSuppressedEntries, because calling
  // IsSuppressedEntry() has some latency because of mutex lock even when
  // the entry is empty.
  const bool has_suppressed_entries_ = false;
  DictionaryInterface::Callback* callback_ = nullptr;
};

}  // namespace

void DictionaryImpl::LookupPredictive(
    absl::string_view key, const ConversionRequest& conversion_request,
    Callback* callback) const {
  CallbackWithFilter callback_with_filter(conversion_request, pos_matcher_,
                                          user_dictionary_, callback);
  for (const DictionaryInterface* dic :
       GetDictionaries(conversion_request.incognito_mode())) {
    dic->LookupPredictive(key, &callback_with_filter);
  }
}

void DictionaryImpl::LookupPrefix(absl::string_view key,
                                  const ConversionRequest& conversion_request,
                                  Callback* callback) const {
  CallbackWithFilter callback_with_filter(conversion_request, pos_matcher_,
                                          user_dictionary_, callback);
  for (const DictionaryInterface* dic :
       GetDictionaries(conversion_request.incognito_mode())) {
    dic->LookupPrefix(key, &callback_with_filter);
  }
}

void DictionaryImpl::LookupExact(absl::string_view key,
                                 const ConversionRequest& conversion_request,
                                 Callback* callback) const {
  CallbackWithFilter callback_with_filter(conversion_request, pos_matcher_,
                                          user_dictionary_, callback);
  for (const DictionaryInterface* dic :
       GetDictionaries(conversion_request.incognito_mode())) {
    dic->LookupExact(key, &callback_with_filter);
  }
}

void DictionaryImpl::LookupReverse(absl::string_view str,
                                   const ConversionRequest& conversion_request,
                                   Callback* callback) const {
  CallbackWithFilter callback_with_filter(conversion_request, pos_matcher_,
                                          user_dictionary_, callback);
  for (const DictionaryInterface* dic :
       GetDictionaries(conversion_request.incognito_mode())) {
    dic->LookupReverse(str, &callback_with_filter);
  }
}

bool DictionaryImpl::LookupComment(absl::string_view key,
                                   absl::string_view value,
                                   const ConversionRequest& conversion_request,
                                   std::string* comment) const {
  auto dics = GetDictionaries(conversion_request.incognito_mode());
  // Access dics in reverse order to prefer UserDictionary (if not in incognito)
  return std::any_of(dics.rbegin(), dics.rend(),
                     [&](const DictionaryInterface* dic) {
                       return dic->LookupComment(key, value, comment);
                     });
}

void DictionaryImpl::LookupPredictive(absl::string_view key,
                                      Callback* callback) const {
  for (const DictionaryInterface* dic : dics_) {
    dic->LookupPredictive(key, callback);
  }
}

void DictionaryImpl::LookupPrefix(absl::string_view key,
                                  Callback* callback) const {
  for (const DictionaryInterface* dic : dics_) {
    dic->LookupPrefix(key, callback);
  }
}

void DictionaryImpl::LookupExact(absl::string_view key,
                                 Callback* callback) const {
  for (const DictionaryInterface* dic : dics_) {
    dic->LookupExact(key, callback);
  }
}

void DictionaryImpl::LookupReverse(absl::string_view str,
                                   Callback* callback) const {
  for (const DictionaryInterface* dic : dics_) {
    dic->LookupReverse(str, callback);
  }
}

bool DictionaryImpl::LookupComment(absl::string_view key,
                                   absl::string_view value,
                                   std::string* comment) const {
  // Access dics_ in reverse order to prefer UserDictionary
  return std::any_of(dics_.rbegin(), dics_.rend(),
                     [&](const DictionaryInterface* dic) {
                       return dic->LookupComment(key, value, comment);
                     });
}

void DictionaryImpl::PopulateReverseLookupCache(absl::string_view str) const {
  for (const DictionaryInterface* dic : dics_) {
    dic->PopulateReverseLookupCache(str);
  }
}

void DictionaryImpl::ClearReverseLookupCache() const {
  for (const DictionaryInterface* dic : dics_) {
    dic->ClearReverseLookupCache();
  }
}

absl::Span<const DictionaryInterface* const> DictionaryImpl::GetDictionaries(
    bool incognito_mode) const {
  // Removes the last user dictionary when incognito_mode.
  DCHECK_EQ(dics_.back(), &user_dictionary_);
  const int size = (incognito_mode && !dics_.empty()) ? 1 : 0;
  return absl::MakeConstSpan(dics_).first(dics_.size() - size);
}

}  // namespace dictionary
}  // namespace mozc
