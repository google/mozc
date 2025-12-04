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
#include "converter/node_list_builder.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/pos_matcher.h"
#include "engine/modules.h"
#include "engine/supplemental_model_interface.h"
#include "prediction/number_decoder.h"
#include "prediction/realtime_decoder.h"
#include "prediction/result.h"
#include "prediction/result_filter.h"
#include "prediction/single_kanji_decoder.h"
#include "prediction/zero_query_dict.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"
#include "request/request_util.h"
#include "transliteration/transliteration.h"

namespace mozc::prediction {
namespace {

using ::mozc::commands::Request;
using ::mozc::composer::TypeCorrectedQuery;
using ::mozc::dictionary::DictionaryInterface;
using ::mozc::dictionary::Token;

// Note that PREDICTION mode is much slower than SUGGESTION.
// Number of prediction calls should be minimized.
constexpr size_t kSuggestionMaxResultsSize = 256;
constexpr size_t kPredictionMaxResultsSize = 100000;

// Returns true if the input mode is Latin-character-input mode, regardless
// of the actual keyboard layout.
bool IsLatinInputMode(const ConversionRequest& request) {
  return request.composer().GetInputMode() == transliteration::HALF_ASCII ||
         request.composer().GetInputMode() == transliteration::FULL_ASCII;
}

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

bool IsZeroQuerySuffixPredictionDisabled(const ConversionRequest& request) {
  return request.request()
      .decoder_experiment_params()
      .disable_zero_query_suffix_prediction();
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

// Returns true if `request` contains number history.
// Normalized number will be set to |number_key|
// Note:
//  Now this function supports arabic number candidates only and
//  we don't support kanji number candidates for now.
//  This is because We have several kanji number styles, for example,
//  "一二", "十二", "壱拾弐", etc for 12.
// TODO(toshiyuki): Define the spec and support Kanji.
std::optional<std::string> GetNumberHistory(const ConversionRequest& request) {
  absl::string_view history_value = request.converter_history_value(1);
  if (history_value.empty() || !NumberUtil::IsArabicNumber(history_value)) {
    return std::nullopt;
  }
  return japanese_util::FullWidthToHalfWidth(history_value);
}

bool IsEmailPrefix(absl::string_view str) {
  return str.ends_with("@") && mozc::Util::IsAscii(str);
}

class PredictiveLookupCallback : public DictionaryInterface::Callback {
 public:
  PredictiveLookupCallback(PredictionTypes types, size_t limit,
                           size_t original_key_len,
                           const absl::btree_set<std::string>& subsequent_chars,
                           int zip_code_id, int unknown_id,
                           std::vector<Result>* results)
      : penalty_(0),
        types_(types),
        limit_(limit),
        original_key_len_(original_key_len),
        subsequent_chars_(subsequent_chars),
        zip_code_id_(zip_code_id),
        unknown_id_(unknown_id),
        results_(results) {}

  PredictiveLookupCallback(const PredictiveLookupCallback&) = delete;
  PredictiveLookupCallback& operator=(const PredictiveLookupCallback&) = delete;

  virtual void RewriteResult(Result& result) const {}

  ResultType OnKey(absl::string_view key) override {
    if (subsequent_chars_.empty()) {
      return TRAVERSE_CONTINUE;
    }
    // If |subsequent_chars_| was provided, check if the substring of |key|
    // obtained by removing the original lookup key starts with a string in the
    // set.  For example, if original key is "he" and "hello" was found,
    // continue traversing only when one of "l", "ll", or "llo" is in
    // |subsequent_chars_|.
    // Implementation note: Although starts_with() is called at most N times
    // where N = subsequent_chars_.size(), N is very small in practice, less
    // than 10.  Thus, this linear order algorithm is fast enough.
    // Theoretically, we can construct a trie of strings in |subsequent_chars_|
    // to get more performance but it's overkill here.
    // TODO(noriyukit): std::vector<string> would be better than set<string>.
    // To this end, we need to fix Comopser as well.
    const absl::string_view rest = absl::ClippedSubstr(key, original_key_len_);
    for (absl::string_view chr : subsequent_chars_) {
      if (rest.starts_with(chr)) {
        return TRAVERSE_CONTINUE;
      }
    }
    return TRAVERSE_NEXT_KEY;
  }

  ResultType OnActualKey(absl::string_view key, absl::string_view actual_key,
                         int num_expanded) override {
    penalty_ = GetSpatialCostPenalty(num_expanded);
    return TRAVERSE_CONTINUE;
  }

  ResultType OnToken(absl::string_view key, absl::string_view actual_key,
                     const Token& token) override {
    // If the token is from user dictionary and its POS is unknown, it is
    // suggest-only words.  Such words are looked up only when their keys
    // exactly match |key|.  Otherwise, unigram suggestion can be annoying.  For
    // example, suppose a user registers their email address as める.  Then,
    // we don't want to show the email address from め but exactly from める.
    //
    // We also want to show ZIP_CODE entries only for the exact input key.
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
    result.wcost += penalty_;
    if (penalty_ > 0) result.types |= KEY_EXPANDED_IN_DICTIONARY;
    RewriteResult(result);
    results_->emplace_back(std::move(result));
    return (results_->size() < limit_) ? TRAVERSE_CONTINUE : TRAVERSE_DONE;
  }

 protected:
  int32_t penalty_;
  const PredictionTypes types_;
  const size_t limit_;
  const size_t original_key_len_;
  const absl::btree_set<std::string>& subsequent_chars_;
  const int zip_code_id_;
  const int unknown_id_;
  std::vector<Result>* results_ = nullptr;

 private:
  // When the key is number, number token will be noisy if
  // - the key predicts number ("十月[10がつ]" for the key, "1")
  // - the value predicts number ("12時" for the key, "1")
  // - the value contains long suffix ("101匹わんちゃん" for the key, "101")
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
};

class PredictiveBigramLookupCallback : public PredictiveLookupCallback {
 public:
  PredictiveBigramLookupCallback(
      PredictionTypes types, size_t limit, size_t original_key_len,
      const absl::btree_set<std::string>& subsequent_chars,
      absl::string_view history_key, absl::string_view history_value,
      int zip_code_id, int unknown_id, std::vector<Result>* results)
      : PredictiveLookupCallback(types, limit, original_key_len,
                                 subsequent_chars, zip_code_id, unknown_id,
                                 results),
        history_key_(history_key),
        history_value_(history_value) {}

  PredictiveBigramLookupCallback(const PredictiveBigramLookupCallback&) =
      delete;
  PredictiveBigramLookupCallback& operator=(
      const PredictiveBigramLookupCallback&) = delete;

  ResultType OnToken(absl::string_view key, absl::string_view expanded_key,
                     const Token& token) override {
    // Skip the token if its value doesn't start with the previous user input,
    // |history_value_|.
    if (!token.value.starts_with(history_value_) ||
        token.value.size() <= history_value_.size()) {
      return TRAVERSE_CONTINUE;
    }
    ResultType result_type =
        PredictiveLookupCallback::OnToken(key, expanded_key, token);
    return result_type;
  }

  // Removes the history key/values in the result.
  void RewriteResult(Result& result) const override {
    result.key.erase(0, history_key_.size());
    result.value.erase(0, history_value_.size());
  }

 private:
  absl::string_view history_key_;
  absl::string_view history_value_;
};

class PrefixLookupCallback : public DictionaryInterface::Callback {
 public:
  PrefixLookupCallback(size_t limit, int kanji_number_id, int unknown_id,
                       int min_value_chars_len, int request_key_len,
                       std::vector<Result>* results)
      : limit_(limit),
        kanji_number_id_(kanji_number_id),
        unknown_id_(unknown_id),
        min_value_chars_len_(min_value_chars_len),
        request_key_len_(request_key_len),
        results_(results) {}

  PrefixLookupCallback(const PrefixLookupCallback&) = delete;
  PrefixLookupCallback& operator=(const PrefixLookupCallback&) = delete;

  ResultType OnToken(absl::string_view key, absl::string_view actual_key,
                     const Token& token) override {
    if ((token.attributes & Token::USER_DICTIONARY) != 0 &&
        token.lid == unknown_id_) {
      // No suggest-only words as prefix candidates
      return TRAVERSE_CONTINUE;
    }
    // Avoid noisy script type nodes.
    if (token.lid == kanji_number_id_ && token.rid == kanji_number_id_) {
      // Kanji number entry can be looked up with the special reading and will
      // be expanded for the number variants, so we want to suppress them here.
      // For example, for the input "ろっぽんぎ", "六" can be looked up for
      // the prefix reading "ろ" or "ろっ", and then be expanded with "6", "Ⅵ",
      // etc.
      return TRAVERSE_CONTINUE;
    }
    const Util::ScriptType script_type = Util::GetScriptType(token.value);
    if (script_type == Util::NUMBER || script_type == Util::ALPHABET ||
        script_type == Util::EMOJI) {
      return TRAVERSE_CONTINUE;
    }
    if (Util::CharsLen(token.value) < min_value_chars_len_) {
      return TRAVERSE_CONTINUE;
    }
    Result result;
    result.InitializeByTokenAndTypes(token, PREFIX);
    if (key != actual_key) {
      result.candidate_attributes |= converter::Attribute::TYPING_CORRECTION;
    }
    const int key_len = Util::CharsLen(key);
    if (key_len < request_key_len_) {
      result.candidate_attributes |=
          converter::Attribute::PARTIALLY_KEY_CONSUMED;
      result.consumed_key_size = key_len;
    }
    results_->emplace_back(std::move(result));
    return (results_->size() < limit_) ? TRAVERSE_CONTINUE : TRAVERSE_DONE;
  }

 private:
  const size_t limit_;
  const int kanji_number_id_;
  const int unknown_id_;
  const int min_value_chars_len_;
  const int request_key_len_;
  std::vector<Result>* results_ = nullptr;
};

class HandwritingLookupCallback : public DictionaryInterface::Callback {
 public:
  HandwritingLookupCallback(size_t limit, int penalty,
                            std::vector<std::string> constraints,
                            std::vector<Result>* results)
      : limit_(limit),
        penalty_(penalty),
        constraints_(std::move(constraints)),
        results_(results) {}

  HandwritingLookupCallback(const HandwritingLookupCallback&) = delete;
  HandwritingLookupCallback& operator=(const HandwritingLookupCallback&) =
      delete;

  ResultType OnToken(absl::string_view key, absl::string_view actual_key,
                     const Token& token) override {
    size_t next_pos = 0;
    for (absl::string_view constraint : constraints_) {
      const size_t pos = token.value.find(constraint, next_pos);
      if (pos == std::string::npos) {
        return TRAVERSE_CONTINUE;
      }
      next_pos = pos + 1;
    }

    Result result;
    result.InitializeByTokenAndTypes(token, UNIGRAM);
    result.wcost += penalty_;
    results_->emplace_back(std::move(result));
    return (results_->size() < limit_) ? TRAVERSE_CONTINUE : TRAVERSE_DONE;
  }

 private:
  const size_t limit_;  // The maximum number of results token size.
  const int penalty_;   // Cost penalty for result tokens.
  const std::vector<std::string> constraints_;
  std::vector<Result>* results_ = nullptr;
};

class FindValueCallback : public DictionaryInterface::Callback {
 public:
  FindValueCallback(const FindValueCallback&) = delete;
  FindValueCallback& operator=(const FindValueCallback&) = delete;
  explicit FindValueCallback(absl::string_view target_value)
      : target_value_(target_value), found_(false) {}

  ResultType OnToken(absl::string_view,  // key
                     absl::string_view,  // actual_key
                     const Token& token) override {
    if (token.value != target_value_) {
      return TRAVERSE_CONTINUE;
    }
    found_ = true;
    token_ = token;
    return TRAVERSE_DONE;
  }

  bool found() const { return found_; }

  const Token& token() const { return token_; }

 private:
  absl::string_view target_value_;
  bool found_;
  Token token_;
};

}  // namespace

// RAII class to adjust the result size to be `cutoff_threshold`.
class ResultsSizeAdjuster {
 public:
  ResultsSizeAdjuster(const ConversionRequest& request,
                      std::vector<Result>* results)
      : cutoff_threshold_(
            DictionaryPredictionAggregator::GetCandidateCutoffThreshold(
                request.request_type())),
        results_(results),
        prev_size_(results_->size()) {}

  ~ResultsSizeAdjuster() { AdjustSize(); }

  void AdjustSize() const {
    // If size reaches max_results_size (== cutoff_threshold).
    // we don't show the candidates, since disambiguation from
    // 256 candidates is hard. (It may exceed max_results_size, because this is
    // just a limit for each backend, so total number may be larger)
    const size_t added_size = results_->size() - prev_size_;
    if (added_size >= cutoff_threshold_) {
      results_->resize(prev_size_);
    }
  }

  // Returns the span of newly added results.
  absl::Span<Result> GetAddedResults() const {
    return absl::Span<Result>(*results_).subspan(prev_size_);
  }

  size_t cutoff_threshold() const { return cutoff_threshold_; }

 private:
  const size_t cutoff_threshold_ = 0;
  std::vector<Result>* results_ = nullptr;
  const size_t prev_size_ = 0;
};

DictionaryPredictionAggregator::DictionaryPredictionAggregator(
    const engine::Modules& modules, const RealtimeDecoder& decoder)
    : modules_(modules),
      decoder_(decoder),
      dictionary_(modules.GetDictionary()),
      suffix_dictionary_(modules.GetSuffixDictionary()),
      counter_suffix_word_id_(modules.GetPosMatcher().GetCounterSuffixWordId()),
      kanji_number_id_(modules.GetPosMatcher().GetKanjiNumberId()),
      zip_code_id_(modules.GetPosMatcher().GetZipcodeId()),
      unknown_id_(modules.GetPosMatcher().GetUnknownId()),
      zero_query_dict_(modules.GetZeroQueryDict()),
      zero_query_number_dict_(modules.GetZeroQueryNumberDict()) {}

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
  AggregateRealtime(request, GetRealtimeCandidateMaxSize(request),
                    request.use_actual_converter_for_realtime_conversion(),
                    &results);

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
  if (IsLanguageAwareInputEnabled(request) && !IsLatinInputMode(request) &&
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
    AggregateRealtime(request, GetRealtimeCandidateMaxSize(request),
                      request.use_actual_converter_for_realtime_conversion(),
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

  const bool is_mixed_conversion = IsMixedConversionEnabled(request);

  using AggregateUnigramFn = void (DictionaryPredictionAggregator::*)(
      const ConversionRequest& request, std::vector<Result>* results) const;

  int min_key_len = 1;
  AggregateUnigramFn unigram_fn = nullptr;

  // User switches to Latin input mode type Latin characters or English words.
  // No need to perform Japanese decoding.
  if (IsLatinInputMode(request)) {
    // For SUGGESTION request in Desktop, We don't look up English words when
    // key length is one.
    min_key_len = (is_mixed_conversion ||
                   request.request_type() == ConversionRequest::PREDICTION)
                      ? 1
                      : 2;
    unigram_fn = &DictionaryPredictionAggregator::AggregateEnglish;
  } else if (request_util::IsHandwriting(request)) {
    min_key_len = 1;
    unigram_fn =
        &DictionaryPredictionAggregator::AggregateUnigramForHandwriting;
  } else if (is_mixed_conversion) {
    // In mixed conversion mode, we want to show unigram candidates even for
    // short keys to emulate PREDICTION mode.
    min_key_len = 1;
    unigram_fn =
        &DictionaryPredictionAggregator::AggregateUnigramForMixedConversion;
  } else {
    // Use the standard dictionary-based prediction by default.
    min_key_len =
        (request.request_type() == ConversionRequest::PREDICTION) ? 1 : 3;
    unigram_fn = &DictionaryPredictionAggregator::AggregateUnigramForDictionary;
  }

  DCHECK(unigram_fn);

  *min_unigram_key_len = min_key_len;
  if (key_len >= min_key_len) {
    (this->*unigram_fn)(request, results);
  }
}

void DictionaryPredictionAggregator::AggregateZeroQuery(
    const ConversionRequest& request, std::vector<Result>* results) const {
  DCHECK(results);

  // There are 4 sources in zero query suggestion.

  absl::string_view history_value = request.converter_history_value(1);
  absl::string_view history_key = request.converter_history_key(1);

  if (history_key.empty() || history_value.empty()) {
    return;
  }

  // 1. Supplemental model.
  modules_.GetSupplementalModel().Predict(request, *results);

  // 2. Zero query number dictionary(data / zero_query / zero_query_number.def)
  // "30" -> "年"
  // TOOD(taku): Consider to aggregate other candidates.
  if (AggregateNumberZeroQuery(request, results)) {
    return;
  }

  // 3. Zero query dictionary (data/zero_query/zero_query.def)
  // "あけまして" -> "おめでとうございます”
  constexpr uint16_t kId = 0;  // EOS
  GetZeroQueryCandidatesForKey(request, history_value, zero_query_dict_, kId,
                               kId, results);
  // Special treatment for email address.
  // "user@" -> "google.com"
  if (IsEmailPrefix(history_key) && (history_key == history_value)) {
    GetZeroQueryCandidatesForKey(request, "@", zero_query_dict_, kId, kId,
                                 results);
  }

  // 4. English decoder.
  modules_.GetSupplementalModel().DecodeEnglish(request, *results);

  // We do not want zero query results from suffix dictionary for Latin
  // input mode. For example, we do not need "です", "。" just after "when".
  if (IsLatinInputMode(request)) {
    return;
  }

  // 5. Zero query suffix dictionary.
  //    "東京" -> "は"
  if (results->empty() || !IsZeroQuerySuffixPredictionDisabled(request) ||
      request_util::IsHandwriting(request)) {
    // Uses larger cutoff (kPredictionMaxResultsSize) in order to consider
    // all suffix entries.
    GetPredictiveResultsForUnigram(suffix_dictionary_, request, SUFFIX,
                                   kPredictionMaxResultsSize, results);
  }
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

void DictionaryPredictionAggregator::AggregateUnigramForDictionary(
    const ConversionRequest& request, std::vector<Result>* results) const {
  DCHECK(results);
  DCHECK(request.request_type() == ConversionRequest::PREDICTION ||
         request.request_type() == ConversionRequest::SUGGESTION);

  const ResultsSizeAdjuster adjuster(request, results);

  GetPredictiveResultsForUnigram(dictionary_, request, UNIGRAM,
                                 adjuster.cutoff_threshold(), results);
}

void DictionaryPredictionAggregator::AggregateUnigramForHandwriting(
    const ConversionRequest& request, std::vector<Result>* results) const {
  DCHECK(results);
  DCHECK(request.request_type() == ConversionRequest::PREDICTION ||
         request.request_type() == ConversionRequest::SUGGESTION);

  const ResultsSizeAdjuster adjuster(request, results);

  const commands::DecoderExperimentParams& param =
      request.request().decoder_experiment_params();
  const int handwriting_cost_offset =
      param.handwriting_conversion_candidate_cost_offset();

  int processed_count = 0;
  const int size_to_process = param.max_composition_event_to_process();
  absl::Span<const commands::SessionCommand::CompositionEvent>
      composition_events = request.composer().GetHandwritingCompositions();
  for (size_t i = 0; i < composition_events.size(); ++i) {
    const commands::SessionCommand::CompositionEvent& elm =
        composition_events[i];
    if (elm.probability() <= 0.0) {
      continue;
    }
    const int recognition_cost = -500.0 * log(elm.probability());
    constexpr int kAsisCostOffset = 3453;  // 500 * log(1000) = ~3453
    Result asis_result = {
        .key = elm.composition_string(),
        .value = elm.composition_string(),
        .types = UNIGRAM,
        // Set small cost for the top recognition result.
        .wcost = (i == 0) ? 0 : kAsisCostOffset + recognition_cost,
        .candidate_attributes = (converter::Attribute::NO_VARIANTS_EXPANSION |
                                 converter::Attribute::NO_EXTRA_DESCRIPTION |
                                 converter::Attribute::NO_MODIFICATION),
    };

    const std::optional<DictionaryPredictionAggregator::HandwritingQueryInfo>
        query_info = processed_count < size_to_process
                         ? GenerateQueryForHandwriting(request, elm)
                         : std::nullopt;
    if (query_info.has_value()) {
      ++processed_count;

      // Populate |results| with the look up result.
      HandwritingLookupCallback callback(
          adjuster.cutoff_threshold(),
          handwriting_cost_offset + recognition_cost, query_info->constraints,
          results);
      dictionary_.LookupExact(query_info->query, request, &callback);
      // Rewrite key with the look-up query.
      asis_result.key = query_info->query;
    }
    results->emplace_back(std::move(asis_result));
  }
}

void DictionaryPredictionAggregator::AggregateUnigramForMixedConversion(
    const ConversionRequest& request, std::vector<Result>* results) const {
  DCHECK(results);
  DCHECK(request.request_type() == ConversionRequest::PREDICTION ||
         request.request_type() == ConversionRequest::SUGGESTION);

  std::vector<Result> raw_result;
  GetPredictiveResultsForUnigram(dictionary_, request, UNIGRAM,
                                 kPredictionMaxResultsSize, &raw_result);

  filter::RemoveRedundantResults(&raw_result);

  absl::c_move(raw_result, std::back_inserter(*results));
}

void DictionaryPredictionAggregator::AggregateBigram(
    const ConversionRequest& request, std::vector<Result>* results) const {
  DCHECK(results);

  // Disables bigram zero query just in case.
  // This check must be done outside of this method.
  if (request.IsZeroQuerySuggestion()) {
    return;
  }

  // TODO(toshiyuki): Support suggestion from the last 2 histories.
  //  ex) "六本木"+"ヒルズ"->"レジデンス"
  absl::string_view history_key = request.converter_history_key(1);
  absl::string_view history_value = request.converter_history_value(1);
  if (history_key.empty() || history_value.empty()) {
    return;
  }

  // Check that history_key/history_value are in the dictionary.
  FindValueCallback find_history_callback(history_value);
  dictionary_.LookupPrefix(history_key, request, &find_history_callback);

  // History value is not found in the dictionary.
  // User may create this the history candidate from T13N or segment
  // expand/shrinkg operations.
  if (!find_history_callback.found()) {
    return;
  }

  const ResultsSizeAdjuster adjuster(request, results);
  GetPredictiveResultsForBigram(dictionary_, history_key, history_value,
                                request, BIGRAM, adjuster.cutoff_threshold(),
                                results);
  adjuster.AdjustSize();

  const Util::ScriptType history_ctype = Util::GetScriptType(history_value);
  const int history_value_size = Util::CharsLen(history_value);
  const Util::ScriptType last_history_ctype = Util::GetScriptType(
      Util::Utf8SubString(history_value, history_value_size - 1, 1));
  for (Result& result : adjuster.GetAddedResults()) {
    CheckBigramResult(find_history_callback.token(), history_ctype,
                      last_history_ctype, request, &result);
  }
}

// Returns true if we add zero query result.
bool DictionaryPredictionAggregator::AggregateNumberZeroQuery(
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

void DictionaryPredictionAggregator::AggregateEnglish(
    const ConversionRequest& request, std::vector<Result>* results) const {
  DCHECK(results);

  const ResultsSizeAdjuster adjuster(request, results);

  GetPredictiveResultsForEnglishKey(dictionary_, request, request.key(),
                                    ENGLISH, adjuster.cutoff_threshold(),
                                    results);

  modules_.GetSupplementalModel().DecodeEnglish(request, *results);
}

void DictionaryPredictionAggregator::AggregateEnglishUsingRawInput(
    const ConversionRequest& request, std::vector<Result>* results) const {
  DCHECK(results);

  const ResultsSizeAdjuster adjuster(request, results);

  GetPredictiveResultsForEnglishKey(dictionary_, request,
                                    request.composer().GetRawString(), ENGLISH,
                                    adjuster.cutoff_threshold(), results);

  modules_.GetSupplementalModel().DecodeEnglish(request, *results);
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
  DCHECK(results);

  absl::string_view request_key = request.key();
  const size_t request_key_len = Util::CharsLen(request_key);
  if (request_key_len <= 1) {
    return;
  }

  // Excludes exact match nodes.
  absl::string_view lookup_key =
      Util::Utf8SubString(request_key, 0, request_key_len - 1);

  constexpr int kMinValueCharsLen = 2;
  PrefixLookupCallback callback(
      GetCandidateCutoffThreshold(request.request_type()), kanji_number_id_,
      unknown_id_, kMinValueCharsLen, request_key_len, results);
  dictionary_.LookupPrefix(lookup_key, request, &callback);
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

void DictionaryPredictionAggregator::GetPredictiveResultsForUnigram(
    const DictionaryInterface& dictionary, const ConversionRequest& request,
    PredictionTypes types, size_t lookup_limit,
    std::vector<Result>* results) const {
  const absl::btree_set<std::string> empty_expanded;
  if (request.use_already_typing_corrected_key()) {
    PredictiveLookupCallback callback(types, lookup_limit, request.key().size(),
                                      empty_expanded, zip_code_id_, unknown_id_,
                                      results);
    dictionary.LookupPredictive(request.key(), request, &callback);
    return;
  }

  // If we have ambiguity for the input, get expanded key.
  // Example1 roman input: for "あk", we will get |base|, "あ" and |expanded|,
  // "か", "き", etc
  // Example2 kana input: for "あか", we will get |base|, "あ" and |expanded|,
  // "か", and "が".
  // auto = std::pair<std::string, absl::btree_set<std::string>>
  const auto [base, expanded] = request.composer().GetQueriesForPrediction();
  if (expanded.empty()) {
    PredictiveLookupCallback callback(types, lookup_limit, base.size(),
                                      expanded, zip_code_id_, unknown_id_,
                                      results);
    dictionary.LookupPredictive(base, request, &callback);
    return;
  }

  // |expanded| is a very small set, so calling LookupPredictive multiple
  // times is not so expensive.  Also, the number of lookup results is limited
  // by |lookup_limit|.
  for (absl::string_view expanded_char : expanded) {
    const std::string request_key = absl::StrCat(base, expanded_char);
    PredictiveLookupCallback callback(types, lookup_limit, request_key.size(),
                                      empty_expanded, zip_code_id_, unknown_id_,
                                      results);
    dictionary.LookupPredictive(request_key, request, &callback);
  }
}

void DictionaryPredictionAggregator::GetPredictiveResultsForBigram(
    const DictionaryInterface& dictionary, const absl::string_view history_key,
    const absl::string_view history_value, const ConversionRequest& request,
    PredictionTypes types, size_t lookup_limit,
    std::vector<Result>* results) const {
  absl::btree_set<std::string> expanded;

  if (request.use_already_typing_corrected_key()) {
    const std::string request_key = absl::StrCat(history_key, request.key());
    PredictiveBigramLookupCallback callback(
        types, lookup_limit, request_key.size(), expanded, history_key,
        history_value, zip_code_id_, unknown_id_, results);
    dictionary.LookupPredictive(request_key, request, &callback);
    return;
  }

  // If we have ambiguity for the input, get expanded key.
  // Example1 roman input: for "あk", we will get |base|, "あ" and |expanded|,
  // "か", "き", etc
  // Example2 kana input: for "あか", we will get |base|, "あ" and |expanded|,
  // "か", and "が".
  // auto = std::pair<std::string, absl::btree_set<std::string>>
  std::string base;
  std::tie(base, expanded) = request.composer().GetQueriesForPrediction();
  const std::string request_key = absl::StrCat(history_key, base);

  PredictiveBigramLookupCallback callback(
      types, lookup_limit, request_key.size(), expanded, history_key,
      history_value, zip_code_id_, unknown_id_, results);
  dictionary.LookupPredictive(request_key, request, &callback);
}

void DictionaryPredictionAggregator::GetPredictiveResultsForEnglishKey(
    const DictionaryInterface& dictionary, const ConversionRequest& request,
    const absl::string_view request_key, PredictionTypes types,
    size_t lookup_limit, std::vector<Result>* results) const {
  const size_t prev_results_size = results->size();
  const absl::btree_set<std::string> empty_expanded;
  if (Util::IsUpperAscii(request_key)) {
    // For upper case key, look up its lower case version and then transform
    // the results to upper case.
    std::string key(request_key);
    Util::LowerString(&key);
    PredictiveLookupCallback callback(types, lookup_limit, key.size(),
                                      empty_expanded, zip_code_id_, unknown_id_,
                                      results);
    dictionary.LookupPredictive(key, request, &callback);
    for (size_t i = prev_results_size; i < results->size(); ++i) {
      Util::UpperString(&(*results)[i].value);
    }
  } else if (Util::IsCapitalizedAscii(request_key)) {
    // For capitalized key, look up its lower case version and then transform
    // the results to capital.
    std::string key(request_key);
    Util::LowerString(&key);
    PredictiveLookupCallback callback(types, lookup_limit, key.size(),
                                      empty_expanded, zip_code_id_, unknown_id_,
                                      results);
    dictionary.LookupPredictive(key, request, &callback);
    for (size_t i = prev_results_size; i < results->size(); ++i) {
      Util::CapitalizeString(&(*results)[i].value);
    }
  } else {
    // For other cases (lower and as-is), just look up directly.
    PredictiveLookupCallback callback(types, lookup_limit, request_key.size(),
                                      empty_expanded, zip_code_id_, unknown_id_,
                                      results);
    dictionary.LookupPredictive(request_key, request, &callback);
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

void DictionaryPredictionAggregator::GetZeroQueryCandidatesForKey(
    const ConversionRequest& request, absl::string_view key,
    const ZeroQueryDict& dict, uint16_t lid, uint16_t rid,
    std::vector<Result>* results) const {
  DCHECK(results);

  using ZeroQueryResult = std::pair<std::string, ZeroQueryType>;
  std::vector<ZeroQueryResult> zero_query_results;

  auto range = dict.equal_range(key);
  if (range.first == range.second) {
    return;
  }

  const bool is_key_one_char_and_not_kanji =
      Util::CharsLen(key) == 1 && !Util::ContainsScriptType(key, Util::KANJI);
  for (; range.first != range.second; ++range.first) {
    const auto& entry = range.first;
    if (entry.type() != ZERO_QUERY_EMOJI) {
      zero_query_results.emplace_back(entry.value(), entry.type());
      continue;
    }

    // Emoji should not be suggested for single Hiragana / Katakana input,
    // because they tend to be too much aggressive.
    if (is_key_one_char_and_not_kanji) {
      continue;
    }

    zero_query_results.emplace_back(entry.value(), entry.type());
  }

  int cost = 0;
  for (const auto& [value, type] : zero_query_results) {
    // Increment cost to show the candidates in order.
    constexpr int kSuffixPenalty = 10;

    Result result;
    result.SetTypesAndTokenAttributes(SUFFIX, Token::NONE);
    result.key = value;
    result.value = value;
    result.wcost = cost;
    result.lid = lid;
    result.rid = rid;
    results->emplace_back(std::move(result));

    cost += kSuffixPenalty;
  }
}

size_t DictionaryPredictionAggregator::GetCandidateCutoffThreshold(
    ConversionRequest::RequestType request_type) {
  DCHECK(request_type == ConversionRequest::PREDICTION ||
         request_type == ConversionRequest::SUGGESTION);
  if (request_type == ConversionRequest::PREDICTION) {
    // If PREDICTION, many candidates are needed than SUGGESTION.
    return kPredictionMaxResultsSize;
  }
  return kSuggestionMaxResultsSize;
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

  const size_t size_limit = request.max_dictionary_prediction_candidates_size();

  // Set the initial values to max_size and default_size.
  size_t max_size = size_limit;
  if (request.create_partial_candidates()) {
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

std::optional<DictionaryPredictionAggregator::HandwritingQueryInfo>
DictionaryPredictionAggregator::GenerateQueryForHandwriting(
    const ConversionRequest& request,
    const commands::SessionCommand::CompositionEvent& composition_event) const {
  if (composition_event.probability() < 0.0001) {
    // Skip generating the query info for unconfident composition,
    // since running reverse conversion is slow.
    return std::nullopt;
  }
  if (absl::StrContains(composition_event.composition_string(), " ")) {
    // Skip providing converted candidates for queries including white space.
    return std::nullopt;
  }
  if (!Util::ContainsScriptType(composition_event.composition_string(),
                                Util::HIRAGANA)) {
    // Skip providing converted candidates for queries not including Hiragana.
    return std::nullopt;
  }

  const ConversionRequest request_for_realtime =
      ConversionRequestBuilder()
          .SetConversionRequestView(request)
          .SetRequestType(ConversionRequest::REVERSE_CONVERSION)
          .SetKey(composition_event.composition_string())
          .Build();

  HandwritingQueryInfo info;
  std::vector<Result> results = decoder_.ReverseDecode(request_for_realtime);
  if (results.empty()) return info;

  Result& result = results.front();
  info.query = std::move(result.value);

  // b/324976556:
  // We have to use the segment key instead of the candidate key.
  // candidate key does not always match segment key for T13N chars.
  std::string utf8_str;
  const Utf8AsChars original_chars(result.key);
  for (const absl::string_view c : original_chars) {
    if (Util::GetScriptType(c) != Util::HIRAGANA) {
      absl::StrAppend(&utf8_str, c);
    } else if (!utf8_str.empty()) {
      info.constraints.emplace_back(utf8_str);
      utf8_str.clear();
    }
  }
  if (!utf8_str.empty()) {
    info.constraints.emplace_back(utf8_str);
  }

  return info;
}

// Filter out irrelevant bigrams. For example, we don't want to
// suggest "リカ" from the history "アメ".
void DictionaryPredictionAggregator::CheckBigramResult(
    const Token& history_token, const Util::ScriptType history_ctype,
    const Util::ScriptType last_history_ctype, const ConversionRequest& request,
    Result* result) const {
  DCHECK(result);

  absl::string_view key = result->key;
  absl::string_view value = result->value;

  // Don't suggest 0-length key/value.
  if (key.empty() || value.empty()) {
    result->removed = true;
    MOZC_WORD_LOG(*result, "Removed. key, value or both are empty.");
    return;
  }

  const Util::ScriptType ctype =
      Util::GetScriptType(Util::Utf8SubString(value, 0, 1));

  if (history_ctype == Util::KANJI && ctype == Util::KATAKANA) {
    // Do not filter "六本木ヒルズ"
    MOZC_WORD_LOG(*result, "Valid bigram. Kanji + Katakana pattern.");
    return;
  }

  // If freq("アメ") < freq("アメリカ"), we don't
  // need to suggest it. As "アメリカ" should already be
  // suggested when user type "アメ".
  // Note that wcost = -500 * log(prob).
  if (ctype != Util::KANJI && history_token.cost > result->wcost) {
    result->removed = true;
    MOZC_WORD_LOG(*result,
                  "Removed. The prefix's score is lower than the whole.");
    return;
  }

  // If character type doesn't change, this boundary might NOT
  // be a word boundary. Only use iif the entire key is reasonably long.
  const size_t key_len =
      Util::CharsLen(result->key) + Util::CharsLen(history_token.key);
  if (ctype == last_history_ctype &&
      ((ctype == Util::HIRAGANA && key_len <= 9) ||
       (ctype == Util::KATAKANA && key_len <= 5))) {
    result->removed = true;
    MOZC_WORD_LOG(*result, "Removed. Short Hiragana (<= 9) or Katakana (<= 5)");
    return;
  }

  // The suggested key/value pair must exist in the dictionary.
  // For example, we don't want to suggest "ターネット" from
  // the history "イン".
  // If character type is Kanji and the suggestion is not a
  // zero_query_suggestion, we relax this condition, as there are
  // many Kanji-compounds which may not in the dictionary. For example,
  // we want to suggest "霊長類研究所" from the history "京都大学".
  if (ctype == Util::KANJI && Util::CharsLen(value) >= 2) {
    // Do not filter this.
    // TODO(toshiyuki): one-length kanji prediction may be annoying other than
    // some exceptions, "駅", "口", etc
    MOZC_WORD_LOG(*result, "Valid bigram. Kanji suffix (>= 2).");
    return;
  }

  // Check if the word is in the dictionary or not.
  // For Hiragana words, check if that word is in a key of values.
  // This is for a situation that
  // ありがとうございました is not in the dictionary, but
  // ありがとう御座いました is in the dictionary.
  if (ctype == Util::HIRAGANA) {
    if (!dictionary_.HasKey(key)) {
      result->removed = true;
      MOZC_WORD_LOG(*result, "Removed. No keys are found.");
      return;
    }
  } else {
    FindValueCallback callback(value);
    dictionary_.LookupPrefix(key, request, &callback);
    if (!callback.found()) {
      result->removed = true;
      MOZC_WORD_LOG(*result, "Removed. No prefix found.");
      return;
    }
  }

  MOZC_WORD_LOG(*result, "Valid bigram.");
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
