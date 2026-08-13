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

#include "prediction/english_decoder.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "base/japanese_util.h"
#include "base/number_util.h"
#include "base/util.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_token.h"
#include "engine/modules.h"
#include "prediction/result.h"
#include "request/conversion_request.h"
#include "transliteration/transliteration.h"

namespace mozc::prediction {
namespace {

using ::mozc::dictionary::DictionaryInterface;
using ::mozc::dictionary::Token;

constexpr size_t kSuggestionMaxResultsSize = 256;
constexpr size_t kPredictionMaxResultsSize = 100000;

// TODO(taku): Move shared prediction cutoff threshold to a common library.
size_t GetCandidateCutoffThreshold(
    ConversionRequest::RequestType request_type) {
  DCHECK(request_type == ConversionRequest::PREDICTION ||
         request_type == ConversionRequest::SUGGESTION ||
         request_type == ConversionRequest::PARTIAL_PREDICTION ||
         request_type == ConversionRequest::PARTIAL_SUGGESTION);
  if (request_type == ConversionRequest::PREDICTION ||
      request_type == ConversionRequest::PARTIAL_PREDICTION) {
    return kPredictionMaxResultsSize;
  }
  return kSuggestionMaxResultsSize;
}

// TODO(taku): Move shared callback/lookup helper to a common prediction
// library.
class PredictiveLookupCallback : public DictionaryInterface::Callback {
 public:
  PredictiveLookupCallback(PredictionTypes types, size_t limit,
                           size_t original_key_len, int zip_code_id,
                           int unknown_id, std::vector<Result>* results)
      : types_(types),
        limit_(limit),
        original_key_len_(original_key_len),
        zip_code_id_(zip_code_id),
        unknown_id_(unknown_id),
        results_(results) {}

  ResultType OnToken(absl::string_view key, absl::string_view expanded_new_key,
                     const Token& token) override {
    if (((token.attributes & Token::USER_DICTIONARY) != 0 &&
         token.lid == unknown_id_) ||
        token.lid == zip_code_id_) {
      const auto orig_key = absl::ClippedSubstr(key, 0, original_key_len_);
      if (token.key != orig_key) {
        return TRAVERSE_CONTINUE;
      }
    }
    if (IsNoisyNumberToken(key, token)) {
      return TRAVERSE_CONTINUE;
    }

    Result result;
    result.InitializeByTokenAndTypes(token, types_);
    results_->emplace_back(std::move(result));
    if (results_->size() >= limit_) {
      return TRAVERSE_DONE;
    }
    return TRAVERSE_CONTINUE;
  }

 private:
  bool IsNoisyNumberToken(absl::string_view key, const Token& token) const {
    const auto orig_key = absl::ClippedSubstr(key, 0, original_key_len_);
    if (!NumberUtil::IsArabicNumber(orig_key)) {
      return false;
    }
    const absl::string_view key_suffix(token.key.data() + orig_key.size(),
                                       token.key.size() - orig_key.size());
    if (key_suffix.empty()) {
      return false;
    }
    if (Util::GetFirstScriptType(key_suffix) == Util::NUMBER) {
      return true;
    }

    if (!token.value.starts_with(orig_key)) {
      return false;
    }

    const absl::string_view value_suffix(token.value.data() + orig_key.size(),
                                         token.value.size() - orig_key.size());
    if (value_suffix.empty()) {
      return false;
    }
    if (Util::GetFirstScriptType(value_suffix) == Util::NUMBER) {
      return true;
    }
    return Util::CharsLen(value_suffix) >= 3;
  }

  const PredictionTypes types_;
  const size_t limit_;
  const size_t original_key_len_;
  const int zip_code_id_;
  const int unknown_id_;
  std::vector<Result>* results_;
};

}  // namespace

EnglishDecoder::EnglishDecoder(const engine::Modules& modules)
    : modules_(modules),
      dictionary_(modules.GetDictionary()),
      zip_code_id_(modules.GetPosMatcher().GetZipcodeId()),
      unknown_id_(modules.GetPosMatcher().GetUnknownId()) {}

std::vector<Result> EnglishDecoder::Decode(
    const ConversionRequest& request) const {
  if (request.request_type() == ConversionRequest::CONVERSION ||
      request.key().empty()) {
    return {};
  }
  std::vector<Result> results;
  const size_t cutoff = GetCandidateCutoffThreshold(request.request_type());
  GetPredictiveResultsForEnglishKey(request, request.key(), cutoff, &results);
  modules_.GetSupplementalModel().DecodeEnglish(request, results);
  return results;
}

std::vector<Result> EnglishDecoder::DecodeUsingRawInput(
    const ConversionRequest& request) const {
  if (request.request_type() == ConversionRequest::CONVERSION ||
      request.composer().GetRawString().empty()) {
    return {};
  }
  std::vector<Result> results;
  const size_t cutoff = GetCandidateCutoffThreshold(request.request_type());
  GetPredictiveResultsForEnglishKey(request, request.composer().GetRawString(),
                                    cutoff, &results);
  modules_.GetSupplementalModel().DecodeEnglish(request, results);
  return results;
}

void EnglishDecoder::GetPredictiveResultsForEnglishKey(
    const ConversionRequest& request, const absl::string_view request_key,
    size_t lookup_limit, std::vector<Result>* results) const {
  DCHECK(results);
  const size_t prev_results_size = results->size();
  if (Util::IsUpperAscii(request_key)) {
    // For upper case key, look up its lower case version and then transform
    // the results to upper case.
    std::string key(request_key);
    Util::LowerString(&key);
    PredictiveLookupCallback callback(ENGLISH, lookup_limit, key.size(),
                                      zip_code_id_, unknown_id_, results);
    dictionary_.LookupPredictive(key, request.options(), &callback);
    for (size_t i = prev_results_size; i < results->size(); ++i) {
      Util::UpperString(&(*results)[i].value);
    }
  } else if (Util::IsCapitalizedAscii(request_key)) {
    // For capitalized key, look up its lower case version and then transform
    // the results to capital.
    std::string key(request_key);
    Util::LowerString(&key);
    PredictiveLookupCallback callback(ENGLISH, lookup_limit, key.size(),
                                      zip_code_id_, unknown_id_, results);
    dictionary_.LookupPredictive(key, request.options(), &callback);
    for (size_t i = prev_results_size; i < results->size(); ++i) {
      Util::CapitalizeString(&(*results)[i].value);
    }
  } else {
    // For other cases (lower and as-is), just look up directly.
    PredictiveLookupCallback callback(ENGLISH, lookup_limit, request_key.size(),
                                      zip_code_id_, unknown_id_, results);
    dictionary_.LookupPredictive(request_key, request.options(), &callback);
  }
  // If input mode is FULL_ASCII, then convert the results to full-width.
  if (request.composer().GetInputMode() == transliteration::FULL_ASCII) {
    std::string tmp;
    for (size_t i = prev_results_size; i < results->size(); ++i) {
      tmp.assign((*results)[i].value);
      (*results)[i].value = japanese_util::HalfWidthAsciiToFullWidthAscii(tmp);
    }
  }
}

}  // namespace mozc::prediction
