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
    const UserDictionaryInterface &user_dictionary,
    const PosMatcher &pos_matcher)
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
  return absl::c_any_of(dics_, [&key](const DictionaryInterface *dic) {
    return dic->HasKey(key);
  });
}

bool DictionaryImpl::HasValue(absl::string_view value) const {
  return absl::c_any_of(dics_, [&value](const DictionaryInterface *dic) {
    return dic->HasKey(value);
  });
}

namespace {

class CallbackWithFilter : public DictionaryInterface::Callback {
 public:
  CallbackWithFilter(const config::Config &config,
                     const PosMatcher &pos_matcher,
                     const UserDictionaryInterface &user_dictionary,
                     DictionaryInterface::Callback *callback)
      : config_(config),
        pos_matcher_(pos_matcher),
        user_dictionary_(user_dictionary),
        callback_(callback) {}

  ResultType OnKey(absl::string_view key) override {
    return callback_->OnKey(key);
  }

  ResultType OnActualKey(absl::string_view key, absl::string_view actual_key,
                         int num_expanded) override {
    return callback_->OnActualKey(key, actual_key, num_expanded);
  }

  ResultType OnToken(absl::string_view key, absl::string_view actual_key,
                     const Token &token) override {
    if (!(token.attributes & Token::USER_DICTIONARY)) {
      if (!config_.use_spelling_correction() &&
          (token.attributes & Token::SPELLING_CORRECTION)) {
        return TRAVERSE_CONTINUE;
      }
      if (!config_.use_zip_code_conversion() &&
          pos_matcher_.IsZipcode(token.lid)) {
        return TRAVERSE_CONTINUE;
      }
      if (!config_.use_t13n_conversion() &&
          Util::IsEnglishTransliteration(token.value)) {
        return TRAVERSE_CONTINUE;
      }
    }
    if (user_dictionary_.IsSuppressedEntry(token.key, token.value)) {
      return TRAVERSE_CONTINUE;
    }
    return callback_->OnToken(key, actual_key, token);
  }

 private:
  const config::Config &config_;
  const PosMatcher &pos_matcher_;
  const UserDictionaryInterface &user_dictionary_;
  DictionaryInterface::Callback *callback_;
};

}  // namespace

void DictionaryImpl::LookupPredictive(
    absl::string_view key, const ConversionRequest &conversion_request,
    Callback *callback) const {
  CallbackWithFilter callback_with_filter(
      conversion_request.config(), pos_matcher_, user_dictionary_, callback);
  for (const DictionaryInterface *dic : dics_) {
    dic->LookupPredictive(key, conversion_request, &callback_with_filter);
  }
}

void DictionaryImpl::LookupPrefix(absl::string_view key,
                                  const ConversionRequest &conversion_request,
                                  Callback *callback) const {
  CallbackWithFilter callback_with_filter(
      conversion_request.config(), pos_matcher_, user_dictionary_, callback);
  for (const DictionaryInterface *dic : dics_) {
    dic->LookupPrefix(key, conversion_request, &callback_with_filter);
  }
}

void DictionaryImpl::LookupExact(absl::string_view key,
                                 const ConversionRequest &conversion_request,
                                 Callback *callback) const {
  CallbackWithFilter callback_with_filter(
      conversion_request.config(), pos_matcher_, user_dictionary_, callback);
  for (const DictionaryInterface *dic : dics_) {
    dic->LookupExact(key, conversion_request, &callback_with_filter);
  }
}

void DictionaryImpl::LookupReverse(absl::string_view str,
                                   const ConversionRequest &conversion_request,
                                   Callback *callback) const {
  CallbackWithFilter callback_with_filter(
      conversion_request.config(), pos_matcher_, user_dictionary_, callback);
  for (const DictionaryInterface *dic : dics_) {
    dic->LookupReverse(str, conversion_request, &callback_with_filter);
  }
}

bool DictionaryImpl::LookupComment(absl::string_view key,
                                   absl::string_view value,
                                   const ConversionRequest &conversion_request,
                                   std::string *comment) const {
  // Access dics_ in reverse order to prefer UserDictionary
  return std::any_of(
      dics_.rbegin(), dics_.rend(), [&](const DictionaryInterface *dic) {
        return dic->LookupComment(key, value, conversion_request, comment);
      });
}

void DictionaryImpl::PopulateReverseLookupCache(absl::string_view str) const {
  for (const DictionaryInterface *dic : dics_) {
    dic->PopulateReverseLookupCache(str);
  }
}

void DictionaryImpl::ClearReverseLookupCache() const {
  for (const DictionaryInterface *dic : dics_) {
    dic->ClearReverseLookupCache();
  }
}

}  // namespace dictionary
}  // namespace mozc
