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

#include "prediction/decoder_util.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/btree_set.h"
#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "base/number_util.h"
#include "base/util.h"
#include "converter/attribute.h"
#include "converter/node_list_builder.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_token.h"
#include "prediction/result.h"
#include "request/conversion_request.h"

namespace mozc::prediction {

using ::mozc::converter::Attribute;
using ::mozc::dictionary::DictionaryInterface;
using ::mozc::dictionary::Token;

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

bool IsNoisyNumberToken(absl::string_view key, size_t original_key_len,
                        const Token& token) {
  const auto orig_key = absl::ClippedSubstr(key, 0, original_key_len);
  if (!NumberUtil::IsArabicNumber(orig_key)) {
    return false;
  }
  const absl::string_view key_suffix(token.key.data() + orig_key.size(),
                                     token.key.size() - orig_key.size());
  if (key_suffix.empty()) {
    return false;
  }
  if (Util::GetFirstScriptType(key_suffix) == Util::NUMBER) {
    // If the key is "1", the token "10がつ" is noisy because the suffix
    // starts from a number. i.e. "0がつ".
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

ResultsSizeAdjuster::ResultsSizeAdjuster(const ConversionRequest& request,
                                         std::vector<Result>* results)
    : cutoff_threshold_(GetCandidateCutoffThreshold(request.request_type())),
      results_(results),
      prev_size_(results_->size()) {}

void ResultsSizeAdjuster::AdjustSize() const {
  const size_t added_size = results_->size() - prev_size_;
  if (added_size >= cutoff_threshold_) {
    results_->resize(prev_size_);
  }
}

PredictiveLookupCallback::PredictiveLookupCallback(
    PredictionTypes types, size_t limit, size_t original_key_len,
    const absl::btree_set<std::string>& subsequent_chars, int zip_code_id,
    int unknown_id, std::vector<Result>* results)
    : types_(types),
      limit_(limit),
      original_key_len_(original_key_len),
      subsequent_chars_(&subsequent_chars),
      zip_code_id_(zip_code_id),
      unknown_id_(unknown_id),
      results_(results) {}

PredictiveLookupCallback::PredictiveLookupCallback(
    PredictionTypes types, size_t limit, size_t original_key_len,
    int zip_code_id, int unknown_id, std::vector<Result>* results)
    : types_(types),
      limit_(limit),
      original_key_len_(original_key_len),
      subsequent_chars_(nullptr),
      zip_code_id_(zip_code_id),
      unknown_id_(unknown_id),
      results_(results) {}

DictionaryInterface::Callback::ResultType PredictiveLookupCallback::OnKey(
    absl::string_view key) {
  if (!subsequent_chars_ || subsequent_chars_->empty()) {
    return TRAVERSE_CONTINUE;
  }
  const absl::string_view rest = absl::ClippedSubstr(key, original_key_len_);
  for (absl::string_view chr : *subsequent_chars_) {
    if (rest.starts_with(chr)) {
      return TRAVERSE_CONTINUE;
    }
  }
  return TRAVERSE_NEXT_KEY;
}

DictionaryInterface::Callback::ResultType
PredictiveLookupCallback::OnActualKey(absl::string_view key,
                                      absl::string_view actual_key,
                                      int num_expanded) {
  penalty_ = GetSpatialCostPenalty(num_expanded);
  return TRAVERSE_CONTINUE;
}

DictionaryInterface::Callback::ResultType PredictiveLookupCallback::OnToken(
    absl::string_view key, absl::string_view actual_key, const Token& token) {
  if (((token.attributes & Token::USER_DICTIONARY) != 0 &&
       token.lid == unknown_id_) ||
      token.lid == zip_code_id_) {
    const auto orig_key = absl::ClippedSubstr(key, 0, original_key_len_);
    if (token.key != orig_key) {
      return TRAVERSE_CONTINUE;
    }
  }
  if (IsNoisyNumberToken(key, original_key_len_, token)) {
    return TRAVERSE_CONTINUE;
  }

  Result result;
  result.InitializeByTokenAndTypes(token, types_);
  result.wcost += penalty_;
  if (penalty_ > 0) result.attributes |= Attribute::KEY_EXPANDED_IN_DICTIONARY;
  RewriteResult(result);
  results_->emplace_back(std::move(result));
  return (results_->size() < limit_) ? TRAVERSE_CONTINUE : TRAVERSE_DONE;
}

}  // namespace mozc::prediction
