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

#include "prediction/dictionary_prediction_aggregator.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/btree_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/japanese_util.h"
#include "base/number_util.h"
#include "base/strings/unicode.h"
#include "base/util.h"
#include "composer/query.h"
#include "config/character_form_manager.h"
#include "converter/attribute.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/pos_matcher.h"
#include "engine/modules.h"
#include "engine/supplemental_model_interface.h"
#include "prediction/decoder_util.h"
#include "prediction/number_decoder.h"
#include "prediction/realtime_decoder.h"
#include "prediction/result.h"
#include "prediction/result_filter.h"
#include "prediction/single_kanji_decoder.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"
#include "request/request_util.h"
#include "transliteration/transliteration.h"

namespace mozc::prediction {
namespace {

using ::mozc::commands::Request;
using ::mozc::composer::TypeCorrectedQuery;
using ::mozc::converter::Attribute;
using ::mozc::dictionary::DictionaryInterface;
using ::mozc::dictionary::Token;

// Return true if the current keyboard is capable to type Latin characters
// regardless of actual input mode. QWERTY keyboard is the typical case.
bool IsQwertyMobileTable(const ConversionRequest& request) {
  const auto table = request.request().special_romanji_table();
  return (table == Request::QWERTY_MOBILE_TO_HIRAGANA ||
          table == Request::QWERTY_MOBILE_TO_HALFWIDTHASCII);
}

bool IsLanguageAwareInputEnabled(const ConversionRequest& request) {
  const auto lang_aware = request.request().language_aware_input();
  return lang_aware == Request::LANGUAGE_AWARE_SUGGESTION;
}

bool IsZeroQueryEnabled(const ConversionRequest& request) {
  return request.request().zero_query_suggestion();
}

bool IsMixedConversionEnabled(const ConversionRequest& request) {
  return request.request().mixed_conversion();
}

bool HasHistoryKeyLongerThanOrEqualTo(const ConversionRequest& request,
                                      size_t utf8_len) {
  return Util::CharsLen(request.converter_history_key(1)) >= utf8_len;
}

bool IsLongKeyForRealtimeCandidates(const ConversionRequest& request) {
  constexpr int kFewResultThreshold = 8;
  return Util::CharsLen(request.key()) >= kFewResultThreshold;
}

}  // namespace

DictionaryPredictionAggregator::DictionaryPredictionAggregator(
    const engine::Modules& modules, const RealtimeDecoder& decoder)
    : modules_(modules),
      decoder_(decoder),
      dictionary_decoder_(modules),
      handwriting_decoder_(modules, decoder),
      zero_query_decoder_(modules),
      english_decoder_(modules) {}

std::vector<Result> DictionaryPredictionAggregator::AggregateResultsForTesting(
    const ConversionRequest& request) const {
  return IsMixedConversionEnabled(request)
             ? AggregateResultsForMixedConversion(request)
             : AggregateResultsForDesktop(request);
}

std::vector<Result>
DictionaryPredictionAggregator::AggregateResultsForMixedConversion(
    const ConversionRequest& request) const {
  DCHECK(IsMixedConversionEnabled(request));

  std::vector<Result> results;
  absl::string_view key = request.key();

  // Zero query prediction.
  if (request.IsZeroQuerySuggestion()) {
    if (IsZeroQueryEnabled(request)) {
      AggregateZeroQuery(request, &results);
    }
    return results;
  }

  if (request.request_type() == ConversionRequest::SUGGESTION &&
      (!request.config().use_dictionary_suggest() || IsZipCodeRequest(key))) {
    return results;
  }

  // Always aggregate realtime results when mixed conversion mode.
  AggregateRealtime(
      request, GetRealtimeCandidateMaxSize(request),
      request.options().use_actual_converter_for_realtime_conversion, &results);

  // In partial suggestion or prediction, only realtime candidates are used.
  if (request.request_type() == ConversionRequest::PARTIAL_SUGGESTION ||
      request.request_type() == ConversionRequest::PARTIAL_PREDICTION) {
    return results;
  }

  // TODO(taku): Removes the dependency to `min_unigram_key_len`.
  // This variable is only used in this method.
  int min_unigram_key_len = 0;
  AggregateUnigram(request, &results, &min_unigram_key_len);

  if (IsNotExceedingCutoffThreshold(request, results)) {
    AggregateNumber(request, &results);
  }

  constexpr int kMinHistoryKeyLen = 3;
  if (HasHistoryKeyLongerThanOrEqualTo(request, kMinHistoryKeyLen) &&
      !request.IsZeroQuerySuggestion()) {
    AggregateBigram(request, &results);
  }

  // `min_unigram_key_len` is only used here.
  const size_t key_len = Util::CharsLen(key);
  if (IsLanguageAwareInputEnabled(request) &&
      !request_util::IsLatinInputMode(request) &&
      IsQwertyMobileTable(request) && key_len >= min_unigram_key_len) {
    // QWERTY-Romaji mode to type Japanese. Handle the ごおgぇ -> Google.
    AggregateEnglishUsingRawInput(request, &results);
  }

  if (request_util::IsAutoPartialSuggestionEnabled(request) &&
      IsNotExceedingCutoffThreshold(request, results)) {
    AggregatePrefix(request, &results);
  }

  // Always aggregate single kanji results when mixed conversion mode.
  AggregateSingleKanji(request, &results);

  MaybePopulateTypingCorrectionPenalty(request, &results);

  return results;
}

std::vector<Result> DictionaryPredictionAggregator::AggregateResultsForDesktop(
    const ConversionRequest& request) const {
  DCHECK(!IsMixedConversionEnabled(request));

  std::vector<Result> results;

  absl::string_view key = request.key();

  if (request.request_type() == ConversionRequest::SUGGESTION &&
      (!request.config().use_dictionary_suggest() || IsZipCodeRequest(key))) {
    return results;
  }

  if (ShouldAggregateRealTimeConversionResults(request)) {
    AggregateRealtime(
        request, GetRealtimeCandidateMaxSize(request),
        request.options().use_actual_converter_for_realtime_conversion,
        &results);
  }

  // Desktop mode never sets PARTIAL mode, so we may use DCHECK after the
  // refactoring.
  if (request.request_type() == ConversionRequest::PARTIAL_SUGGESTION ||
      request.request_type() == ConversionRequest::PARTIAL_PREDICTION) {
    return results;
  }

  int min_unigram_key_len = 0;
  AggregateUnigram(request, &results, &min_unigram_key_len);

  if (IsNotExceedingCutoffThreshold(request, results)) {
    AggregateNumber(request, &results);
  }

  constexpr int kMinHistoryKeyLen = 3;
  if (HasHistoryKeyLongerThanOrEqualTo(request, kMinHistoryKeyLen)) {
    AggregateBigram(request, &results);
  }

  return results;
}

std::vector<Result> DictionaryPredictionAggregator::
    AggregateTypingCorrectedResultsForMixedConversion(
        const ConversionRequest& request) const {
  const std::optional<std::vector<TypeCorrectedQuery>> corrected =
      modules_.GetSupplementalModel().CorrectComposition(request);
  if (!corrected) {
    return {};
  }

  std::vector<Result> results;

  bool number_added = false;

  for (const auto& query : corrected.value()) {
    absl::string_view key = query.correction;

    // Make ConversionRequest that uses conversion_segment(0).key() as typing
    // corrected key instead of ComposerData to avoid the original key from
    // being used during the candidate aggregation.
    // Kana modifier insensitive dictionary lookup is also disabled as
    // composition spellchecker has already fixed them.
    ConversionRequest::Options options = request.options();
    options.kana_modifier_insensitive_conversion = false;
    options.use_already_typing_corrected_key = true;

    // Populates all information, e.g., history segments, from `request`,
    // and overrides the options and key.
    const ConversionRequest corrected_request =
        ConversionRequestBuilder()
            .SetConversionRequestView(request)
            .SetOptions(std::move(options))
            .SetKey(key)
            .Build();

    std::vector<Result> corrected_results;

    // Since COMPLETION query already performs predictive lookup,
    // no need to run UNIGRAM and BIGRAM lookup.
    const bool is_realtime_only =
        (query.type & TypeCorrectedQuery::COMPLETION ||
         request.request_type() == ConversionRequest::PARTIAL_SUGGESTION ||
         request.request_type() == ConversionRequest::PARTIAL_PREDICTION);

    if (is_realtime_only) {
      constexpr int kRealtimeSize = 1;
      AggregateRealtime(corrected_request, kRealtimeSize,
                        /* insert_realtime_top_from_actual_converter= */ false,
                        &corrected_results);
    } else {
      int min_unigram_key_len = 0;
      AggregateUnigram(corrected_request, &corrected_results,
                       &min_unigram_key_len);

      constexpr int kRealtimeSize = 2;
      AggregateRealtime(corrected_request, kRealtimeSize,
                        /* insert_realtime_top_from_actual_converter= */ false,
                        &corrected_results);

      if (!request.IsZeroQuerySuggestion()) {
        AggregateBigram(corrected_request, &corrected_results);
      }

      if (!number_added) {
        const int prev_size = corrected_results.size();
        AggregateNumber(corrected_request, &corrected_results);
        number_added |= corrected_results.size() > prev_size;
      }
    }

    const auto* manager =
        config::CharacterFormManager::GetCharacterFormManager();

    // Appends the result with TYPING_CORRECTION attribute.
    for (Result& result : corrected_results) {
      PopulateTypeCorrectedQuery(query, &result);
      result.value = manager->ConvertConversionString(result.value);
      results.emplace_back(std::move(result));
    }
  }

  return results;
}

void DictionaryPredictionAggregator::AggregateUnigram(
    const ConversionRequest& request, std::vector<Result>* results,
    int* min_unigram_key_len) const {
  DCHECK(results);
  DCHECK(min_unigram_key_len);
  *min_unigram_key_len = 0;

  const size_t key_len = Util::CharsLen(request.key());
  if (key_len == 0) {
    return;
  }

  // User switches to Latin input mode type Latin characters or English words.
  // No need to perform Japanese decoding.
  if (request_util::IsLatinInputMode(request)) {
    // For SUGGESTION request in Desktop, We don't look up English words when
    // key length is one.
    const bool is_mixed_conversion = IsMixedConversionEnabled(request);
    const int min_key_len =
        (is_mixed_conversion ||
         request.request_type() == ConversionRequest::PREDICTION)
            ? 1
            : 2;
    *min_unigram_key_len = min_key_len;
    if (key_len >= min_key_len) {
      AggregateEnglish(request, results);
    }
    return;
  }

  if (request_util::IsHandwriting(request)) {
    const int min_key_len = 1;
    *min_unigram_key_len = min_key_len;
    if (key_len >= min_key_len) {
      AggregateUnigramForHandwriting(request, results);
    }
    return;
  }

  dictionary_decoder_.AggregateUnigram(request, results, min_unigram_key_len);
}

void DictionaryPredictionAggregator::AggregateZeroQuery(
    const ConversionRequest& request, std::vector<Result>* results) const {
  DCHECK(results);
  std::vector<Result> zero_query_results = zero_query_decoder_.Decode(request);
  absl::c_move(zero_query_results, std::back_inserter(*results));
}

void DictionaryPredictionAggregator::AggregateRealtime(
    const ConversionRequest& request, size_t realtime_candidates_size,
    bool insert_realtime_top_from_actual_converter,
    std::vector<Result>* results) const {
  DCHECK(results);

  ConversionRequest::Options options = request.options();
  options.max_conversion_candidates_size = realtime_candidates_size;
  options.use_actual_converter_for_realtime_conversion =
      insert_realtime_top_from_actual_converter;

  const ConversionRequest request_for_realtime =
      ConversionRequestBuilder()
          .SetConversionRequestView(request)
          .SetOptions(std::move(options))
          .Build();

  std::vector<Result> realtime_results = decoder_.Decode(request_for_realtime);
  absl::c_move(realtime_results, std::back_inserter(*results));
}

void DictionaryPredictionAggregator::AggregateUnigramForHandwriting(
    const ConversionRequest& request, std::vector<Result>* results) const {
  DCHECK(results);
  std::vector<Result> handwriting_results =
      handwriting_decoder_.Decode(request);
  absl::c_move(handwriting_results, std::back_inserter(*results));
}

void DictionaryPredictionAggregator::AggregateBigram(
    const ConversionRequest& request, std::vector<Result>* results) const {
  dictionary_decoder_.AggregateBigram(request, results);
}

void DictionaryPredictionAggregator::AggregateEnglish(
    const ConversionRequest& request, std::vector<Result>* results) const {
  DCHECK(results);
  std::vector<Result> english_results = english_decoder_.Decode(request);
  absl::c_move(english_results, std::back_inserter(*results));
}

void DictionaryPredictionAggregator::AggregateEnglishUsingRawInput(
    const ConversionRequest& request, std::vector<Result>* results) const {
  DCHECK(results);
  std::vector<Result> english_results =
      english_decoder_.DecodeUsingRawInput(request);
  absl::c_move(english_results, std::back_inserter(*results));
}

void DictionaryPredictionAggregator::AggregateNumber(
    const ConversionRequest& request, std::vector<Result>* results) const {
  DCHECK(results);
  std::vector<Result> number_results =
      NumberDecoder(modules_.GetPosMatcher()).Decode(request);
  absl::c_move(number_results, std::back_inserter(*results));
}

void DictionaryPredictionAggregator::AggregatePrefix(
    const ConversionRequest& request, std::vector<Result>* results) const {
  dictionary_decoder_.AggregatePrefix(request, results);
}

void DictionaryPredictionAggregator::AggregateSingleKanji(
    const ConversionRequest& request, std::vector<Result>* results) const {
  DCHECK(results);

  const std::vector<Result> single_kaji_results =
      SingleKanjiDecoder(modules_.GetPosMatcher(),
                         modules_.GetSingleKanjiDictionary())
          .Decode(request);
  absl::c_move(single_kaji_results, std::back_inserter(*results));
}

size_t DictionaryPredictionAggregator::GetCandidateCutoffThreshold(
    ConversionRequest::RequestType request_type) {
  return mozc::prediction::GetCandidateCutoffThreshold(request_type);
}

size_t DictionaryPredictionAggregator::GetRealtimeCandidateMaxSize(
    const ConversionRequest& request) {
  const ConversionRequest::RequestType request_type = request.request_type();
  DCHECK(request_type == ConversionRequest::PREDICTION ||
         request_type == ConversionRequest::SUGGESTION ||
         request_type == ConversionRequest::PARTIAL_PREDICTION ||
         request_type == ConversionRequest::PARTIAL_SUGGESTION);
  if (request.key().empty()) {
    return 0;
  }
  if (request_util::IsHandwriting(request)) {
    constexpr size_t kRealtimeCandidatesSizeForHandwriting = 3;
    return kRealtimeCandidatesSizeForHandwriting;
  }

  const size_t size_limit =
      request.options().max_dictionary_prediction_candidates_size;

  // Set the initial values to max_size and default_size.
  size_t max_size = size_limit;
  if (request.options().create_partial_candidates) {
    max_size = 20;
  }
  size_t default_size = 10;

  // Reduce the number of candidates for long key.
  if (IsLongKeyForRealtimeCandidates(request)) {
    max_size = 8;
    default_size = 5;
  }

  // Cap the numbers of candidates to the size limit.
  max_size = std::min(max_size, size_limit);
  default_size = std::min(default_size, size_limit);

  const bool mixed_conversion = IsMixedConversionEnabled(request);
  switch (request_type) {
    case ConversionRequest::PREDICTION:
      return mixed_conversion ? max_size : default_size;
    case ConversionRequest::SUGGESTION:
      // Fewer candidates are needed basically.
      // But on mixed_conversion mode we should behave like as conversion
      // mode.
      return mixed_conversion ? default_size : 1;
    case ConversionRequest::PARTIAL_PREDICTION:
      // This is kind of prediction so richer result than PARTIAL_SUGGESTION
      // is needed.
      return max_size;
    case ConversionRequest::PARTIAL_SUGGESTION:
      // PARTIAL_SUGGESTION works like as conversion mode so returning
      // some candidates is needed.
      return default_size;
    default:
      DLOG(FATAL) << "Unexpected request type: " << request_type;
      return 0;
  }
}

bool DictionaryPredictionAggregator::ShouldAggregateRealTimeConversionResults(
    const ConversionRequest& request) {
  constexpr size_t kMaxRealtimeKeySize = 300;  // 300 bytes in UTF8
  absl::string_view key = request.key();
  if (key.empty() || key.size() >= kMaxRealtimeKeySize) {
    // 1) If key is empty, realtime conversion doesn't work.
    // 2) If the key is too long, we'll hit a performance issue.
    return false;
  }

  return (request.request_type() == ConversionRequest::PARTIAL_SUGGESTION ||
          request.config().use_realtime_conversion() ||
          IsMixedConversionEnabled(request));
}

bool DictionaryPredictionAggregator::IsZipCodeRequest(
    const absl::string_view key) {
  if (key.empty()) {
    return false;
  }

  int num_chars = 0;
  for (ConstChar32Iterator iter(key); !iter.Done(); iter.Next()) {
    const char32_t c = iter.Get();
    if (!('0' <= c && c <= '9') && (c != '-')) {
      return false;
    }
    ++num_chars;
  }

  return num_chars < 6;
}

void DictionaryPredictionAggregator::MaybePopulateTypingCorrectionPenalty(
    const ConversionRequest& request, std::vector<Result>* results) const {
  modules_.GetSupplementalModel().PopulateTypeCorrectedQuery(
      request, absl::Span<Result>(*results));
}

}  // namespace mozc::prediction
