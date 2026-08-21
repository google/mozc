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

#include "prediction/zero_query_decoder.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/japanese_util.h"
#include "base/number_util.h"
#include "base/util.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_token.h"
#include "engine/modules.h"
#include "prediction/decoder_util.h"
#include "prediction/result.h"
#include "prediction/zero_query_dict.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"
#include "request/request_util.h"
#include "transliteration/transliteration.h"

namespace mozc::prediction {
namespace {

using ::mozc::dictionary::DictionaryInterface;
using ::mozc::dictionary::Token;

bool IsZeroQuerySuffixPredictionDisabled(const ConversionRequest& request) {
  return request.request()
      .decoder_experiment_params()
      .disable_zero_query_suffix_prediction();
}

// Returns the normalized number history if `request` contains it.
// Note:
//  Now this function supports arabic number candidates only and
//  we don't support kanji number candidates for now.
//  This is because We have several kanji number styles, for example,
//  "一二", "十二", "壱拾弐", etc for 12.
//  If the history in `request` is empty, it fallback to `preceding_text`.
// TODO(toshiyuki): Define the spec and support Kanji.
std::optional<std::string> GetNumberHistory(const ConversionRequest& request) {
  absl::string_view history_value = request.converter_history_value(1);
  if (history_value.empty()) {
    // Note: Full width number is not supported in `preceding_text`.
    history_value = request.context().preceding_text();
    const auto it =
        std::find_if(history_value.rbegin(), history_value.rend(),
                     [](char c) { return !absl::ascii_isdigit(c); });
    history_value = history_value.substr(it.base() - history_value.begin());
  }
  if (history_value.empty() || !NumberUtil::IsArabicNumber(history_value)) {
    return std::nullopt;
  }
  return japanese_util::FullWidthToHalfWidth(history_value);
}

bool IsEmailPrefix(absl::string_view str) {
  return str.ends_with('@') && mozc::Util::IsAscii(str);
}

class SuffixLookupCallback : public DictionaryInterface::Callback {
 public:
  SuffixLookupCallback(PredictionTypes types, size_t limit, int zip_code_id,
                       int unknown_id, std::vector<Result>* results)
      : types_(types),
        limit_(limit),
        zip_code_id_(zip_code_id),
        unknown_id_(unknown_id),
        results_(results) {}

  ResultType OnToken(absl::string_view key, absl::string_view expanded_new_key,
                     const Token& token) override {
    if (token.lid == zip_code_id_ || token.lid == unknown_id_) {
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
  const PredictionTypes types_;
  const size_t limit_;
  const int zip_code_id_;
  const int unknown_id_;
  std::vector<Result>* results_;
};

}  // namespace

ZeroQueryDecoder::ZeroQueryDecoder(const engine::Modules& modules)
    : modules_(modules),
      zero_query_dict_(modules.GetZeroQueryDict()),
      zero_query_number_dict_(modules.GetZeroQueryNumberDict()),
      suffix_dictionary_(modules.GetSuffixDictionary()),
      counter_suffix_word_id_(modules.GetPosMatcher().GetCounterSuffixWordId()),
      zip_code_id_(modules.GetPosMatcher().GetZipcodeId()),
      unknown_id_(modules.GetPosMatcher().GetUnknownId()) {}

std::vector<Result> ZeroQueryDecoder::Decode(
    const ConversionRequest& request) const {
  // There are 4 sources in zero query suggestion.

  absl::string_view history_value = request.converter_history_value(1);
  absl::string_view history_key = request.converter_history_key(1);

  if (history_value.empty() && history_key.empty()) {
    // (b/475682454): Use the preceding text as the history value and key.
    // We may want to tokenize the preceding text to increase the coverage.
    history_value = request.context().preceding_text();
    history_key = history_value;
  }

  if (history_key.empty() || history_value.empty()) {
    return {};
  }

  std::vector<Result> results;

  // 1. Supplemental model.
  modules_.GetSupplementalModel().Predict(request, results);

  // 2. Zero query number dictionary(data / zero_query / zero_query_number.def)
  // "30" -> "年"
  // TOOD(taku): Consider to aggregate other candidates.
  if (AggregateNumberZeroQuery(request, &results)) {
    return results;
  }

  // 3. Zero query dictionary (data/zero_query/zero_query.def)
  // "あけまして" -> "おめでとうございます”
  constexpr uint16_t kId = 0;  // EOS
  GetZeroQueryCandidatesForKey(request, history_value, zero_query_dict_, kId,
                               kId, &results);

  // Special treatment for email address.
  // "user@" -> "google.com"
  if (IsEmailPrefix(history_key) && (history_key == history_value)) {
    GetZeroQueryCandidatesForKey(request, "@", zero_query_dict_, kId, kId,
                                 &results);
  }

  // 4. English decoder.
  modules_.GetSupplementalModel().DecodeEnglish(request, results);

  // We do not want zero query results from suffix dictionary for Latin
  // input mode. For example, we do not need "です", "。" just after "when".
  if (request_util::IsLatinInputMode(request)) {
    return results;
  }

  // We do not want zero query results from suffix dictionary if the request
  // does not have the history POS information.
  // (b/475682454): Context does not have POS information. Suffix dictionary
  // may generate noisy predictions.
  if (request.converter_history_rid() == 0) {
    return results;
  }

  // 5. Zero query suffix dictionary.
  //    "東京" -> "は"
  if (results.empty() || !IsZeroQuerySuffixPredictionDisabled(request) ||
      request_util::IsHandwriting(request)) {
    // Uses larger cutoff (kPredictionMaxResultsSize) in order to consider
    // all suffix entries.
    const auto [base, expanded] = request.composer().GetQueriesForPrediction();
    if (expanded.empty()) {
      SuffixLookupCallback callback(SUFFIX, kPredictionMaxResultsSize,
                                    zip_code_id_, unknown_id_, &results);
      suffix_dictionary_.LookupPredictive(base, request.options(), &callback);
    } else {
      for (absl::string_view expanded_char : expanded) {
        const std::string request_key = absl::StrCat(base, expanded_char);
        SuffixLookupCallback callback(SUFFIX, kPredictionMaxResultsSize,
                                      zip_code_id_, unknown_id_, &results);
        suffix_dictionary_.LookupPredictive(request_key, request.options(),
                                            &callback);
      }
    }
  }

  return results;
}

// Returns true if we add zero query result.
bool ZeroQueryDecoder::AggregateNumberZeroQuery(
    const ConversionRequest& request, std::vector<Result>* results) const {
  DCHECK(results);

  auto number_key_opt = GetNumberHistory(request);
  if (!number_key_opt) return false;

  const std::string number_key = std::move(number_key_opt.value());

  GetZeroQueryCandidatesForKey(request, number_key, zero_query_number_dict_,
                               counter_suffix_word_id_, counter_suffix_word_id_,
                               results);

  GetZeroQueryCandidatesForKey(request, "default", zero_query_number_dict_,
                               counter_suffix_word_id_, counter_suffix_word_id_,
                               results);

  return true;
}

void ZeroQueryDecoder::GetZeroQueryCandidatesForKey(
    const ConversionRequest& request, absl::string_view key,
    const ZeroQueryDict& dict, uint16_t lid, uint16_t rid,
    std::vector<Result>* results) const {
  DCHECK(results);

  absl::Span<const ZeroQueryEntry> entries = dict.equal_range(key);
  if (entries.empty()) {
    return;
  }

  const bool is_key_one_char_and_not_kanji =
      Util::CharsLen(key) == 1 && !Util::ContainsScriptType(key, Util::KANJI);

  int cost = 0;
  constexpr int kSuffixPenalty = 10;

  auto add_entry = [&](const ZeroQueryEntry& entry) {
    Result result;
    result.SetTypesAndTokenAttributes(SUFFIX, Token::NONE);
    result.key = dict.value(entry);
    result.value = dict.value(entry);
    result.wcost = cost;
    result.lid = lid;
    result.rid = rid;
    results->emplace_back(std::move(result));
    cost += kSuffixPenalty;
  };

  for (const ZeroQueryEntry& entry : entries) {
    if (entry.type != ZERO_QUERY_EMOJI) {
      add_entry(entry);
      continue;
    }

    // Emoji should not be suggested for single Hiragana / Katakana input,
    // because they tend to be too much aggressive.
    if (is_key_one_char_and_not_kanji) {
      continue;
    }

    add_entry(entry);
  }
}

}  // namespace mozc::prediction
