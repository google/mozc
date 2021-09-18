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

#include "prediction/dictionary_predictor.h"

#include <algorithm>
#include <cctype>
#include <climits>  // INT_MAX
#include <cmath>
#include <cstdint>
#include <list>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/number_util.h"
#include "base/util.h"
#include "composer/composer.h"
#include "converter/connector.h"
#include "converter/converter_interface.h"
#include "converter/immutable_converter_interface.h"
#include "converter/node_list_builder.h"
#include "converter/segmenter.h"
#include "converter/segments.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/pos_matcher.h"
#include "prediction/predictor_interface.h"
#include "prediction/suggestion_filter.h"
#include "prediction/zero_query_dict.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "usage_stats/usage_stats.h"
#include "absl/container/flat_hash_map.h"
#include "absl/flags/flag.h"
#include "absl/strings/string_view.h"

#ifndef NDEBUG
#define MOZC_DEBUG
#define MOZC_WORD_LOG_MESSAGE(message) \
  absl::StrCat(__FILE__, ":", __LINE__, " ", message, "\n")
#define MOZC_WORD_LOG(result, message) \
  (result).log.append(MOZC_WORD_LOG_MESSAGE(message))

#else  // NDEBUG
#define MOZC_WORD_LOG(result, message) \
  {}

#endif  // NDEBUG

namespace mozc {
namespace {

using ::mozc::commands::Request;
using ::mozc::dictionary::DictionaryInterface;
using ::mozc::dictionary::POSMatcher;
using ::mozc::dictionary::Token;
using ::mozc::usage_stats::UsageStats;

// Used to emulate positive infinity for cost. This value is set for those
// candidates that are thought to be aggressive; thus we can eliminate such
// candidates from suggestion or prediction. Note that for this purpose we don't
// want to use INT_MAX because someone might further add penalty after cost is
// set to INT_MAX, which leads to overflow and consequently aggressive
// candidates would appear in the top results.
constexpr int kInfinity = (2 << 20);

// Note that PREDICTION mode is much slower than SUGGESTION.
// Number of prediction calls should be minimized.
constexpr size_t kSuggestionMaxResultsSize = 256;
constexpr size_t kPredictionMaxResultsSize = 100000;

bool IsSimplifiedRankingEnabled(const ConversionRequest &request) {
  return request.request()
      .decoder_experiment_params()
      .enable_simplified_ranking();
}

// Returns true if the |target| may be reduncant result.
bool MaybeRedundant(const std::string &reference, const std::string &target) {
  return Util::StartsWith(target, reference);
}

bool IsLatinInputMode(const ConversionRequest &request) {
  return (request.has_composer() &&
          (request.composer().GetInputMode() == transliteration::HALF_ASCII ||
           request.composer().GetInputMode() == transliteration::FULL_ASCII));
}

bool IsQwertyMobileTable(const ConversionRequest &request) {
  const auto table = request.request().special_romanji_table();
  return (table == commands::Request::QWERTY_MOBILE_TO_HIRAGANA ||
          table == commands::Request::QWERTY_MOBILE_TO_HALFWIDTHASCII);
}

bool IsLanguageAwareInputEnabled(const ConversionRequest &request) {
  const auto lang_aware = request.request().language_aware_input();
  return lang_aware == commands::Request::LANGUAGE_AWARE_SUGGESTION;
}

// Returns true if |segments| contains number history.
// Normalized number will be set to |number_key|
// Note:
//  Now this function supports arabic number candidates only and
//  we don't support kanji number candidates for now.
//  This is because We have several kanji number styles, for example,
//  "一二", "十二", "壱拾弐", etc for 12.
// TODO(toshiyuki): Define the spec and support Kanji.
bool GetNumberHistory(const Segments &segments, std::string *number_key) {
  DCHECK(number_key);
  const size_t history_size = segments.history_segments_size();
  if (history_size <= 0) {
    return false;
  }

  const Segment &last_segment = segments.history_segment(history_size - 1);
  DCHECK_GT(last_segment.candidates_size(), 0);
  const std::string &history_value = last_segment.candidate(0).value;
  if (!NumberUtil::IsArabicNumber(history_value)) {
    return false;
  }

  Util::FullWidthToHalfWidth(history_value, number_key);
  return true;
}

bool IsMixedConversionEnabled(const commands::Request &request) {
  return request.mixed_conversion();
}

bool IsTypingCorrectionEnabled(const ConversionRequest &request) {
  return request.config().use_typing_correction();
}

bool HasHistoryKeyLongerThanOrEqualTo(const Segments &segments,
                                      size_t utf8_len) {
  const size_t history_segments_size = segments.history_segments_size();
  if (history_segments_size == 0) {
    return false;
  }
  const Segment &history_segment =
      segments.history_segment(history_segments_size - 1);
  if (history_segment.candidates_size() == 0) {
    return false;
  }
  return Util::CharsLen(history_segment.candidate(0).key) >= utf8_len;
}

bool IsLongKeyForRealtimeCandidates(const Segments &segments) {
  constexpr int kFewResultThreshold = 8;
  return (segments.segments_size() > 0 &&
          Util::CharsLen(segments.segment(0).key()) >= kFewResultThreshold);
}

size_t GetMaxSizeForRealtimeCandidates(const Segments &segments,
                                       bool is_long_key) {
  const auto &segment = segments.conversion_segment(0);
  const size_t size =
      (segments.max_prediction_candidates_size() - segment.candidates_size());
  return is_long_key ? std::min(size, static_cast<size_t>(8)) : size;
}

size_t GetDefaultSizeForRealtimeCandidates(bool is_long_key) {
  return is_long_key ? 5 : 10;
}
}  // namespace

class DictionaryPredictor::PredictiveLookupCallback
    : public DictionaryInterface::Callback {
 public:
  PredictiveLookupCallback(DictionaryPredictor::PredictionTypes types,
                           size_t limit, size_t original_key_len,
                           const std::set<std::string> *subsequent_chars,
                           Segment::Candidate::SourceInfo source_info,
                           int unknown_id,
                           std::vector<DictionaryPredictor::Result> *results)
      : penalty_(0),
        types_(types),
        limit_(limit),
        original_key_len_(original_key_len),
        subsequent_chars_(subsequent_chars),
        source_info_(source_info),
        unknown_id_(unknown_id),
        results_(results) {}

  PredictiveLookupCallback(const PredictiveLookupCallback &) = delete;
  PredictiveLookupCallback &operator=(const PredictiveLookupCallback &) =
      delete;

  ResultType OnKey(absl::string_view key) override {
    if (subsequent_chars_ == nullptr) {
      return TRAVERSE_CONTINUE;
    }
    // If |subsequent_chars_| was provided, check if the substring of |key|
    // obtained by removing the original lookup key starts with a string in the
    // set.  For example, if original key is "he" and "hello" was found,
    // continue traversing only when one of "l", "ll", or "llo" is in
    // |subsequent_chars_|.
    // Implementation note: Although Util::StartsWith is called at most N times
    // where N = subsequent_chars_.size(), N is very small in practice, less
    // than 10.  Thus, this linear order algorithm is fast enough.
    // Theoretically, we can construct a trie of strings in |subsequent_chars_|
    // to get more performance but it's overkill here.
    // TODO(noriyukit): std::vector<string> would be better than set<string>.
    // To this end, we need to fix Comopser as well.
    const absl::string_view rest = absl::ClippedSubstr(key, original_key_len_);
    for (const std::string &chr : *subsequent_chars_) {
      if (Util::StartsWith(rest, chr)) {
        return TRAVERSE_CONTINUE;
      }
    }
    return TRAVERSE_NEXT_KEY;
  }

  ResultType OnActualKey(absl::string_view key, absl::string_view actual_key,
                         int num_expanded) override {
    // TODO(taku): Considers to change the penalty depending on `num_expanded`.
    penalty_ = num_expanded > 0 ? kKanaModifierInsensitivePenalty : 0;
    return TRAVERSE_CONTINUE;
  }

  ResultType OnToken(absl::string_view key, absl::string_view actual_key,
                     const Token &token) override {
    // If the token is from user dictionary and its POS is unknown, it is
    // suggest-only words.  Such words are looked up only when their keys
    // exactly match |key|.  Otherwise, unigram suggestion can be annoying.  For
    // example, suppose a user registers their email address as める.  Then,
    // we don't want to show the email address from め but exactly from める.
    if ((token.attributes & Token::USER_DICTIONARY) != 0 &&
        token.lid == unknown_id_) {
      const auto orig_key = absl::ClippedSubstr(key, 0, original_key_len_);
      if (token.key != orig_key) {
        return TRAVERSE_CONTINUE;
      }
    }
    results_->push_back(Result());
    results_->back().InitializeByTokenAndTypes(token, types_);
    results_->back().wcost += penalty_;
    results_->back().source_info |= source_info_;
    return (results_->size() < limit_) ? TRAVERSE_CONTINUE : TRAVERSE_DONE;
  }

 protected:
  int32_t penalty_;
  const DictionaryPredictor::PredictionTypes types_;
  const size_t limit_;
  const size_t original_key_len_;
  const std::set<std::string> *subsequent_chars_;
  const Segment::Candidate::SourceInfo source_info_;
  const int unknown_id_;
  std::vector<DictionaryPredictor::Result> *results_;
};

class DictionaryPredictor::PredictiveBigramLookupCallback
    : public PredictiveLookupCallback {
 public:
  PredictiveBigramLookupCallback(
      DictionaryPredictor::PredictionTypes types, size_t limit,
      size_t original_key_len, const std::set<std::string> *subsequent_chars,
      absl::string_view history_value,
      Segment::Candidate::SourceInfo source_info, int unknown_id,
      std::vector<DictionaryPredictor::Result> *results)
      : PredictiveLookupCallback(types, limit, original_key_len,
                                 subsequent_chars, source_info, unknown_id,
                                 results),
        history_value_(history_value) {}

  PredictiveBigramLookupCallback(const PredictiveBigramLookupCallback &) =
      delete;
  PredictiveBigramLookupCallback &operator=(
      const PredictiveBigramLookupCallback &) = delete;

  ResultType OnToken(absl::string_view key, absl::string_view expanded_key,
                     const Token &token) override {
    // Skip the token if its value doesn't start with the previous user input,
    // |history_value_|.
    if (!Util::StartsWith(token.value, history_value_) ||
        token.value.size() <= history_value_.size()) {
      return TRAVERSE_CONTINUE;
    }
    ResultType result_type =
        PredictiveLookupCallback::OnToken(key, expanded_key, token);
    return result_type;
  }

 private:
  absl::string_view history_value_;
};

// Comparator for sorting prediction candidates.
// If we have words A and AB, for example "六本木" and "六本木ヒルズ",
// assume that cost(A) < cost(AB).
class DictionaryPredictor::ResultWCostLess {
 public:
  bool operator()(const DictionaryPredictor::Result &lhs,
                  const DictionaryPredictor::Result &rhs) const {
    return lhs.wcost < rhs.wcost;
  }
};

class DictionaryPredictor::ResultCostLess {
 public:
  bool operator()(const DictionaryPredictor::Result &lhs,
                  const DictionaryPredictor::Result &rhs) const {
    return lhs.cost > rhs.cost;
  }
};

DictionaryPredictor::DictionaryPredictor(
    const DataManagerInterface &data_manager,
    const ConverterInterface *converter,
    const ImmutableConverterInterface *immutable_converter,
    const DictionaryInterface *dictionary,
    const DictionaryInterface *suffix_dictionary, const Connector *connector,
    const Segmenter *segmenter, const POSMatcher *pos_matcher,
    const SuggestionFilter *suggestion_filter)
    : converter_(converter),
      immutable_converter_(immutable_converter),
      dictionary_(dictionary),
      suffix_dictionary_(suffix_dictionary),
      connector_(connector),
      segmenter_(segmenter),
      suggestion_filter_(suggestion_filter),
      counter_suffix_word_id_(pos_matcher->GetCounterSuffixWordId()),
      general_symbol_id_(pos_matcher->GetGeneralSymbolId()),
      unknown_id_(pos_matcher->GetUnknownId()),
      predictor_name_("DictionaryPredictor") {
  absl::string_view zero_query_token_array_data;
  absl::string_view zero_query_string_array_data;
  absl::string_view zero_query_number_token_array_data;
  absl::string_view zero_query_number_string_array_data;
  data_manager.GetZeroQueryData(&zero_query_token_array_data,
                                &zero_query_string_array_data,
                                &zero_query_number_token_array_data,
                                &zero_query_number_string_array_data);
  zero_query_dict_.Init(zero_query_token_array_data,
                        zero_query_string_array_data);
  zero_query_number_dict_.Init(zero_query_number_token_array_data,
                               zero_query_number_string_array_data);
}

DictionaryPredictor::~DictionaryPredictor() {}

void DictionaryPredictor::Finish(const ConversionRequest &request,
                                 Segments *segments) {
  if (segments->request_type() == Segments::REVERSE_CONVERSION) {
    // Do nothing for REVERSE_CONVERSION.
    return;
  }

  const Segment &segment = segments->conversion_segment(0);
  if (segment.candidates_size() < 1) {
    VLOG(2) << "candidates size < 1";
    return;
  }

  const Segment::Candidate &candidate = segment.candidate(0);
  if (segment.segment_type() != Segment::FIXED_VALUE) {
    VLOG(2) << "segment is not FIXED_VALUE" << candidate.value;
    return;
  }

  MaybeRecordUsageStats(candidate);
}

void DictionaryPredictor::MaybeRecordUsageStats(
    const Segment::Candidate &candidate) const {
  if (candidate.source_info &
      Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_NONE) {
    UsageStats::IncrementCount("CommitDictionaryPredictorZeroQueryTypeNone");
  }

  if (candidate.source_info &
      Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_NUMBER_SUFFIX) {
    UsageStats::IncrementCount(
        "CommitDictionaryPredictorZeroQueryTypeNumberSuffix");
  }

  if (candidate.source_info &
      Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_EMOTICON) {
    UsageStats::IncrementCount(
        "CommitDictionaryPredictorZeroQueryTypeEmoticon");
  }

  if (candidate.source_info &
      Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_EMOJI) {
    UsageStats::IncrementCount("CommitDictionaryPredictorZeroQueryTypeEmoji");
  }

  if (candidate.source_info &
      Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_BIGRAM) {
    UsageStats::IncrementCount("CommitDictionaryPredictorZeroQueryTypeBigram");
  }

  if (candidate.source_info &
      Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_SUFFIX) {
    UsageStats::IncrementCount("CommitDictionaryPredictorZeroQueryTypeSuffix");
  }
}

bool DictionaryPredictor::PredictForRequest(const ConversionRequest &request,
                                            Segments *segments) const {
  if (segments == nullptr) {
    return false;
  }
  if (segments->request_type() == Segments::CONVERSION) {
    VLOG(2) << "request type is CONVERSION";
    return false;
  }
  if (segments->conversion_segments_size() < 1) {
    VLOG(2) << "segment size < 1";
    return false;
  }

  std::vector<Result> results;
  // Mixed conversion is the feature that mixes prediction and
  // conversion, meaning that results may include the candidates whose
  // key is exactly the same as the composition.  This mode is used in mobile.
  const bool is_mixed_conversion = IsMixedConversionEnabled(request.request());
  AggregatePredictionForRequest(request, segments, &results);
  if (results.empty()) {
    return false;
  }

  if (is_mixed_conversion) {
    SetPredictionCostForMixedConversion(*segments, &results);
    ApplyPenaltyForKeyExpansion(*segments, &results);
    // Currently, we don't have spelling correction feature when in
    // the mixed conversion mode, so RemoveMissSpelledCandidates() is
    // not called.
    return AddPredictionToCandidates(
        request,
        true,  // Include exact key result even if it's a bad suggestion.
        segments, &results);
  }

  // Normal prediction.
  SetPredictionCost(*segments, &results);
  ApplyPenaltyForKeyExpansion(*segments, &results);
  const std::string &input_key = segments->conversion_segment(0).key();
  const size_t input_key_len = Util::CharsLen(input_key);
  RemoveMissSpelledCandidates(input_key_len, &results);
  return AddPredictionToCandidates(request, false,  // Remove exact key result.
                                   segments, &results);
}

DictionaryPredictor::PredictionTypes
DictionaryPredictor::AggregatePredictionForRequest(
    const ConversionRequest &request, Segments *segments,
    std::vector<Result> *results) const {
  const bool is_mixed_conversion = IsMixedConversionEnabled(request.request());
  // In mixed conversion mode, the number of real time candidates is increased.
  const size_t realtime_max_size =
      GetRealtimeCandidateMaxSize(*segments, is_mixed_conversion);
  const auto &unigram_config = GetUnigramConfig(request, *segments);

  return AggregatePrediction(request, realtime_max_size, unigram_config,
                             segments, results);
}

DictionaryPredictor::UnigramConfig DictionaryPredictor::GetUnigramConfig(
    const ConversionRequest &request, const Segments &segments) const {
  const bool is_mixed_conversion = IsMixedConversionEnabled(request.request());
  if (IsLatinInputMode(request)) {
    // For SUGGESTION request in Desktop, We don't look up English words when
    // key length is one.
    const size_t min_key_len_for_latin_input =
        (is_mixed_conversion || segments.request_type() == Segments::PREDICTION)
            ? 1
            : 2;
    return {&DictionaryPredictor::AggregateUnigramCandidateForLatinInput,
            min_key_len_for_latin_input};
  }

  if (is_mixed_conversion) {
    // In mixed conversion mode, we want to show unigram candidates even for
    // short keys to emulate PREDICTION mode.
    constexpr size_t kMinUnigramKeyLen = 1;
    return {&DictionaryPredictor::AggregateUnigramCandidateForMixedConversion,
            kMinUnigramKeyLen};
  }

  // Normal prediction.
  const size_t min_unigram_key_len =
      (segments.request_type() == Segments::PREDICTION) ? 1 : 3;
  return {&DictionaryPredictor::AggregateUnigramCandidate, min_unigram_key_len};
}

DictionaryPredictor::PredictionTypes DictionaryPredictor::AggregatePrediction(
    const ConversionRequest &request, size_t realtime_max_size,
    const UnigramConfig &unigram_config, Segments *segments,
    std::vector<Result> *results) const {
  DCHECK(segments);
  DCHECK(results);

  // Zero query prediction.
  if (segments->conversion_segment(0).key().empty()) {
    return AggregatePredictionForZeroQuery(request, segments, results);
  }

  const std::string &key = segments->conversion_segment(0).key();
  const size_t key_len = Util::CharsLen(key);

  // TODO(toshiyuki): Check if we can remove this SUGGESTION check.
  // i.e. can we return NO_PREDICTION here for both of SUGGESTION and
  // PREDICTION?
  if (segments->request_type() == Segments::SUGGESTION) {
    if (!request.config().use_dictionary_suggest()) {
      VLOG(2) << "no_dictionary_suggest";
      return NO_PREDICTION;
    }
    // Never trigger prediction if the key looks like zip code.
    if (DictionaryPredictor::IsZipCodeRequest(key) && key_len < 6) {
      return NO_PREDICTION;
    }
  }

  PredictionTypes selected_types = NO_PREDICTION;
  if (ShouldAggregateRealTimeConversionResults(request, *segments)) {
    AggregateRealtimeConversion(request, realtime_max_size, segments, results);
    selected_types |= REALTIME;
  }
  // In partial suggestion or prediction, only realtime candidates are used.
  if (segments->request_type() == Segments::PARTIAL_SUGGESTION ||
      segments->request_type() == Segments::PARTIAL_PREDICTION) {
    return selected_types;
  }

  // Add unigram candidates.
  const size_t min_unigram_key_len = unigram_config.min_key_len;
  if (key_len >= min_unigram_key_len) {
    const auto &unigram_fn = unigram_config.unigram_fn;
    PredictionType type = (this->*unigram_fn)(request, *segments, results);
    selected_types |= type;
  }

  // Add bigram candidates.
  constexpr int kMinHistoryKeyLen = 3;
  if (HasHistoryKeyLongerThanOrEqualTo(*segments, kMinHistoryKeyLen)) {
    AggregateBigramPrediction(request, *segments,
                              Segment::Candidate::SOURCE_INFO_NONE, results);
    selected_types |= BIGRAM;
  }

  // Add english candidates.
  if (IsLanguageAwareInputEnabled(request) && IsQwertyMobileTable(request) &&
      key_len >= min_unigram_key_len) {
    AggregateEnglishPredictionUsingRawInput(request, *segments, results);
    selected_types |= ENGLISH;
  }

  // Add typing correction candidates.
  constexpr int kMinTypingCorrectionKeyLen = 3;
  if (IsTypingCorrectionEnabled(request) &&
      key_len >= kMinTypingCorrectionKeyLen) {
    AggregateTypeCorrectingPrediction(request, *segments, results);
    selected_types |= TYPING_CORRECTION;
  }

  return selected_types;
}

bool DictionaryPredictor::AddPredictionToCandidates(
    const ConversionRequest &request, bool include_exact_key,
    Segments *segments, std::vector<Result> *results) const {
  DCHECK(segments);
  DCHECK(results);
  const std::string &input_key = segments->conversion_segment(0).key();
  const size_t input_key_len = Util::CharsLen(input_key);

  std::string history_key, history_value;
  GetHistoryKeyAndValue(*segments, &history_key, &history_value);

  // exact_bigram_key does not contain ambiguity expansion, because
  // this is used for exact matching for the key.
  const std::string exact_bigram_key = history_key + input_key;

  Segment *segment = segments->mutable_conversion_segment(0);
  DCHECK(segment);

  // Instead of sorting all the results, we construct a heap.
  // This is done in linear time and
  // we can pop as many results as we need efficiently.
  std::make_heap(results->begin(), results->end(), ResultCostLess());

  const size_t size =
      std::min(segments->max_prediction_candidates_size(), results->size());

  int added = 0;
  std::set<std::string> seen;

  int added_suffix = 0;
  bool cursor_at_tail =
      request.has_composer() &&
      request.composer().GetCursor() == request.composer().GetLength();

  absl::flat_hash_map<std::string, int32_t> merged_types;

#ifndef NDEBUG
  const bool is_debug = true;
#else   // NDEBUG
  // TODO(taku): Sets more advanced debug info depending on the verbose_level.
  const bool is_debug = request.config().verbose_level() >= 1;
#endif  // NDEBUG

  if (is_debug || IsSimplifiedRankingEnabled(request)) {
    for (const auto &result : *results) {
      if (!result.removed) {
        merged_types[result.value] |= result.types;
      }
    }
    if (IsSimplifiedRankingEnabled(request)) {
      for (auto &result : *results) {
        if (!result.removed) {
          result.types = merged_types[result.value];
        }
      }
    }
  }

  auto add_candidate = [&](const Result &result, const std::string &key,
                           const std::string &value,
                           Segment::Candidate *candidate) {
    DCHECK(candidate);

    candidate->Init();
    candidate->content_key = key;
    candidate->content_value = value;
    candidate->key = key;
    candidate->value = value;
    candidate->lid = result.lid;
    candidate->rid = result.rid;
    candidate->wcost = result.wcost;
    candidate->cost = result.cost;
    candidate->attributes = result.candidate_attributes;
    if ((!(candidate->attributes & Segment::Candidate::SPELLING_CORRECTION) &&
         IsLatinInputMode(request)) ||
        (result.types & SUFFIX)) {
      candidate->attributes |= Segment::Candidate::NO_VARIANTS_EXPANSION;
      candidate->attributes |= Segment::Candidate::NO_EXTRA_DESCRIPTION;
    }
    if (candidate->attributes & Segment::Candidate::PARTIALLY_KEY_CONSUMED) {
      candidate->consumed_key_size = result.consumed_key_size;
      // There are two scenarios to reach here.
      // 1. Auto partial suggestion.
      //    e.g. composition わたしのなまえ| -> candidate 私の
      // 2. Partial suggestion.
      //    e.g. composition わたしの|なまえ -> candidate 私の
      // To distinguish auto partial suggestion from (non-auto) partial
      // suggestion, see the cursor position. If the cursor is at the tail
      // of the composition, this is auto partial suggestion.
      if (cursor_at_tail) {
        candidate->attributes |= Segment::Candidate::AUTO_PARTIAL_SUGGESTION;
      }
    }
    candidate->source_info = result.source_info;
    if (result.types & REALTIME) {
      candidate->inner_segment_boundary = result.inner_segment_boundary;
    }
    if (result.types & TYPING_CORRECTION) {
      candidate->attributes |= Segment::Candidate::TYPING_CORRECTION;
    }

    SetDescription(result.types, candidate->attributes,
                   &candidate->description);
    if (is_debug) {
      SetDebugDescription(merged_types[result.value], &candidate->description);
    }
#ifdef MOZC_DEBUG
    candidate->log += "\n" + result.log;
#endif  // MOZC_DEBUG
  };

#ifdef MOZC_DEBUG
  auto add_debug_candidate = [&](Result result, const std::string &log) {
    std::string key, value;
    if (result.types & BIGRAM) {
      // remove the prefix of history key and history value.
      key = result.key.substr(history_key.size(),
                              result.key.size() - history_key.size());
      value = result.value.substr(history_value.size(),
                                  result.value.size() - history_value.size());
    } else {
      key = result.key;
      value = result.value;
    }

    result.log.append(log);
    Segment::Candidate candidate;
    add_candidate(result, key, value, &candidate);
    segment->removed_candidates_for_debug_.push_back(std::move(candidate));
  };
#define MOZC_ADD_DEBUG_CANDIDATE(result, log) \
  add_debug_candidate(result, MOZC_WORD_LOG_MESSAGE(log))

#else  // MOZC_DEBUG
#define MOZC_ADD_DEBUG_CANDIDATE(result, log) \
  {}

#endif  // MOZC_DEBUG

  for (size_t i = 0; i < results->size(); ++i) {
    // Pop a result from a heap. Please pay attention not to use results->at(i).
    std::pop_heap(results->begin(), results->end() - i, ResultCostLess());
    const Result &result = results->at(results->size() - i - 1);

    if (added >= size || result.cost >= kInfinity) {
      break;
    }

    if (result.removed) {
      MOZC_ADD_DEBUG_CANDIDATE(result, "Removed flag is on");
      continue;
    }

    // When |include_exact_key| is true, we don't filter the results
    // which have the exactly same key as the input even if it's a bad
    // suggestion.
    if (!(include_exact_key && (result.key == input_key)) &&
        suggestion_filter_->IsBadSuggestion(result.value)) {
      MOZC_ADD_DEBUG_CANDIDATE(result, "Bad suggestion");
      continue;
    }

    // Don't suggest exactly the same candidate as key.
    // if |include_exact_key| is true, that's not the case.
    if (!include_exact_key && !(result.types & REALTIME) &&
        (((result.types & BIGRAM) && exact_bigram_key == result.value) ||
         (!(result.types & BIGRAM) && input_key == result.value))) {
      MOZC_ADD_DEBUG_CANDIDATE(result, "Key == candidate");
      continue;
    }

    std::string key, value;
    if (result.types & BIGRAM) {
      // remove the prefix of history key and history value.
      key = result.key.substr(history_key.size(),
                              result.key.size() - history_key.size());
      value = result.value.substr(history_value.size(),
                                  result.value.size() - history_value.size());
    } else {
      key = result.key;
      value = result.value;
    }

    if (!seen.insert(value).second) {
      MOZC_ADD_DEBUG_CANDIDATE(result, "Duplicated");
      continue;
    }

    // User input: "おーすとり" (len = 5)
    // key/value:  "おーすとりら" "オーストラリア" (miss match pos = 4)
    if ((result.candidate_attributes &
         Segment::Candidate::SPELLING_CORRECTION) &&
        key != input_key &&
        input_key_len <= GetMissSpelledPosition(key, value) + 1) {
      MOZC_ADD_DEBUG_CANDIDATE(result, "Spelling correction");
      continue;
    }

    if (result.types == SUFFIX && added_suffix++ >= 20) {
      // TODO(toshiyuki): Need refactoring for controlling suffix
      // prediction number after we will fix the appropriate number.
      MOZC_ADD_DEBUG_CANDIDATE(result, "Added suffix >= 20");
      continue;
    }

    Segment::Candidate *candidate = segment->push_back_candidate();
    add_candidate(result, key, value, candidate);
    ++added;
  }

  return added > 0;
#undef MOZC_ADD_DEBUG_CANDIDATE
}

DictionaryPredictor::PredictionTypes
DictionaryPredictor::AggregatePredictionForZeroQuery(
    const ConversionRequest &request, Segments *segments,
    std::vector<Result> *results) const {
  DCHECK(segments);
  DCHECK(results);

  if (!request.request().zero_query_suggestion()) {
    // Zero query is disabled by request.
    return NO_PREDICTION;
  }

  PredictionTypes selected_types = NO_PREDICTION;
  constexpr int kMinHistoryKeyLenForZeroQuery = 2;
  if (HasHistoryKeyLongerThanOrEqualTo(*segments,
                                       kMinHistoryKeyLenForZeroQuery)) {
    AggregateBigramPrediction(
        request, *segments,
        Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_BIGRAM, results);
    selected_types |= BIGRAM;
  }
  if (segments->history_segments_size() > 0) {
    AggregateZeroQuerySuffixPrediction(request, *segments, results);
    selected_types |= SUFFIX;
  }
  return selected_types;
}

DictionaryPredictor::PredictionType
DictionaryPredictor::AggregateUnigramCandidateForLatinInput(
    const ConversionRequest &request, const Segments &segments,
    std::vector<Result> *results) const {
  AggregateEnglishPrediction(request, segments, results);
  return ENGLISH;
}

void DictionaryPredictor::SetDescription(PredictionTypes types,
                                         uint32_t attributes,
                                         std::string *description) {
  if (types & TYPING_CORRECTION) {
    Util::AppendStringWithDelimiter(" ", "補正", description);
  }
  if (attributes & Segment::Candidate::AUTO_PARTIAL_SUGGESTION) {
    Util::AppendStringWithDelimiter(" ", "部分", description);
  }
}

void DictionaryPredictor::SetDebugDescription(PredictionTypes types,
                                              std::string *description) {
  std::string debug_desc;
  if (types & UNIGRAM) {
    debug_desc.append(1, 'U');
  }
  if (types & BIGRAM) {
    debug_desc.append(1, 'B');
  }
  if (types & REALTIME_TOP) {
    debug_desc.append("R1");
  } else if (types & REALTIME) {
    debug_desc.append(1, 'R');
  }
  if (types & SUFFIX) {
    debug_desc.append(1, 'S');
  }
  if (types & ENGLISH) {
    debug_desc.append(1, 'E');
  }
  // Note that description for TYPING_CORRECTION is omitted
  // because it is appended by SetDescription.
  if (!debug_desc.empty()) {
    Util::AppendStringWithDelimiter(" ", debug_desc, description);
  }
}

// Returns cost for |result| when it's transitioned from |rid|.  Suffix penalty
// is also added for non-realtime results.
int DictionaryPredictor::GetLMCost(const Result &result, int rid) const {
  const int cost_with_context = connector_->GetTransitionCost(rid, result.lid);

  int lm_cost = 0;

  if (result.types & SUFFIX) {
    // We always respect the previous context to calculate the cost of SUFFIX.
    // Otherwise, the suffix that doesn't match the context will be promoted.
    lm_cost = cost_with_context + result.wcost;
  } else {
    // Sometimes transition cost is too high and causes a bug like b/18112966.
    // For example, "接続詞 が" -> "始まる 動詞,五段活用,基本形" has very large
    // cost and "始まる" is demoted.  To prevent such cases, ImmutableConverter
    // computes transition from BOS/EOS too; see
    // ImmutableConverterImpl::MakeLatticeNodesForHistorySegments().
    // Here, taking the minimum of |cost1| and |cost2| has a similar effect.
    const int cost_without_context =
        connector_->GetTransitionCost(0, result.lid);
    lm_cost = std::min(cost_with_context, cost_without_context) + result.wcost;
  }

  if (!(result.types & REALTIME)) {
    // Relatime conversion already adds perfix/suffix penalties to the result.
    // Note that we don't add prefix penalty the role of "bunsetsu" is
    // ambiguous on zero-query suggestion.
    lm_cost += segmenter_->GetSuffixPenalty(result.rid);
  }

  return lm_cost;
}

namespace {

class FindValueCallback : public DictionaryInterface::Callback {
 public:
  explicit FindValueCallback(absl::string_view target_value)
      : target_value_(target_value), found_(false) {}

  ResultType OnToken(absl::string_view,  // key
                     absl::string_view,  // actual_key
                     const Token &token) override {
    if (token.value != target_value_) {
      return TRAVERSE_CONTINUE;
    }
    found_ = true;
    token_ = token;
    return TRAVERSE_DONE;
  }

  bool found() const { return found_; }

  const Token &token() const { return token_; }

 private:
  absl::string_view target_value_;
  bool found_;
  Token token_;

  DISALLOW_COPY_AND_ASSIGN(FindValueCallback);
};

}  // namespace

void DictionaryPredictor::Result::InitializeByTokenAndTypes(
    const Token &token, PredictionTypes types) {
  SetTypesAndTokenAttributes(types, token.attributes);
  key = token.key;
  value = token.value;
  wcost = token.cost;
  lid = token.lid;
  rid = token.rid;
}

void DictionaryPredictor::Result::SetTypesAndTokenAttributes(
    PredictionTypes prediction_types, Token::AttributesBitfield token_attr) {
  types = prediction_types;
  candidate_attributes = 0;
  if (types & TYPING_CORRECTION) {
    candidate_attributes |= Segment::Candidate::TYPING_CORRECTION;
  }
  if (types & (REALTIME | REALTIME_TOP)) {
    candidate_attributes |= Segment::Candidate::REALTIME_CONVERSION;
  }
  if (token_attr & Token::SPELLING_CORRECTION) {
    candidate_attributes |= Segment::Candidate::SPELLING_CORRECTION;
  }
  if (token_attr & Token::USER_DICTIONARY) {
    candidate_attributes |= (Segment::Candidate::USER_DICTIONARY |
                             Segment::Candidate::NO_VARIANTS_EXPANSION);
  }
}

void DictionaryPredictor::Result::SetSourceInfoForZeroQuery(
    ZeroQueryType type) {
  switch (type) {
    case ZERO_QUERY_NONE:
      source_info |= Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_NONE;
      return;
    case ZERO_QUERY_NUMBER_SUFFIX:
      source_info |=
          Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_NUMBER_SUFFIX;
      return;
    case ZERO_QUERY_EMOTICON:
      source_info |=
          Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_EMOTICON;
      return;
    case ZERO_QUERY_EMOJI:
      source_info |= Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_EMOJI;
      return;
    case ZERO_QUERY_BIGRAM:
      source_info |= Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_BIGRAM;
      return;
    case ZERO_QUERY_SUFFIX:
      source_info |= Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_SUFFIX;
      return;
    default:
      LOG(ERROR) << "Should not come here";
      return;
  }
}

bool DictionaryPredictor::Result::IsUserDictionaryResult() const {
  return (candidate_attributes & Segment::Candidate::USER_DICTIONARY) != 0;
}

// Here, we treat the word as English when its key consists of Latin
// characters.
bool DictionaryPredictor::Result::IsEnglishEntryResult() const {
  return Util::IsEnglishTransliteration(key);
}

bool DictionaryPredictor::GetHistoryKeyAndValue(const Segments &segments,
                                                std::string *key,
                                                std::string *value) const {
  DCHECK(key);
  DCHECK(value);
  if (segments.history_segments_size() == 0) {
    return false;
  }

  const Segment &history_segment =
      segments.history_segment(segments.history_segments_size() - 1);
  if (history_segment.candidates_size() == 0) {
    return false;
  }

  key->assign(history_segment.candidate(0).key);
  value->assign(history_segment.candidate(0).value);
  return true;
}

void DictionaryPredictor::SetPredictionCost(
    const Segments &segments, std::vector<Result> *results) const {
  DCHECK(results);

  int rid = 0;  // 0 (BOS) is default
  if (segments.history_segments_size() > 0) {
    const Segment &history_segment =
        segments.history_segment(segments.history_segments_size() - 1);
    if (history_segment.candidates_size() > 0) {
      rid = history_segment.candidate(0).rid;  // use history segment's id
    }
  }

  const std::string &input_key = segments.conversion_segment(0).key();
  std::string history_key, history_value;
  GetHistoryKeyAndValue(segments, &history_key, &history_value);
  const std::string bigram_key = history_key + input_key;
  const bool is_suggestion = (segments.request_type() == Segments::SUGGESTION);

  // use the same scoring function for both unigram/bigram.
  // Bigram will be boosted because we pass the previous
  // key as a context information.
  const size_t bigram_key_len = Util::CharsLen(bigram_key);
  const size_t unigram_key_len = Util::CharsLen(input_key);

  // In the loop below, we track the minimum cost among those REALTIME
  // candidates that have the same key length as |input_key| so that we can set
  // a slightly smaller cost to REALTIME_TOP than these.
  int realtime_cost_min = kInfinity;
  Result *realtime_top_result = nullptr;

  for (size_t i = 0; i < results->size(); ++i) {
    const Result &result = results->at(i);

    // The cost of REALTIME_TOP is determined after the loop based on the
    // minimum cost for REALTIME. Just remember the pointer of result.
    if (result.types & REALTIME_TOP) {
      realtime_top_result = &results->at(i);
      continue;
    }

    const int cost = GetLMCost(result, rid);
    const size_t query_len =
        (result.types & BIGRAM) ? bigram_key_len : unigram_key_len;
    const size_t key_len = Util::CharsLen(result.key);

    if (IsAggressiveSuggestion(query_len, key_len, cost, is_suggestion,
                               results->size())) {
      results->at(i).cost = kInfinity;
      continue;
    }

    // cost = -500 * log(lang_prob(w) * (1 + remain_length))    -- (1)
    // where lang_prob(w) is a language model probability of the word "w", and
    // remain_length the length of key user must type to input "w".
    //
    // Example:
    // key/value = "とうきょう/東京"
    // user_input = "とう"
    // remain_length = len("とうきょう") - len("とう") = 3
    //
    // By taking the log of (1),
    // cost  = -500 [log(lang_prob(w)) + log(1 + ramain_length)]
    //       = -500 * log(lang_prob(w)) + 500 * log(1 + remain_length)
    //       = cost - 500 * log(1 + remain_length)
    // Because 500 * log(lang_prob(w)) = -cost.
    //
    // lang_prob(w) * (1 + remain_length) represents how user can reduce
    // the total types by choosing this candidate.
    // Before this simple algorithm, we have been using an SVM-base scoring,
    // but we stop usign it with the following reasons.
    // 1) Hard to maintain the ranking.
    // 2) Hard to control the final results of SVM.
    // 3) Hard to debug.
    // 4) Since we used the log(remain_length) as a feature,
    //    the new ranking algorithm and SVM algorithm was essentially
    //    the same.
    // 5) Since we used the length of value as a feature, we find
    //    inconsistencies between the conversion and the prediction
    //    -- the results of top prediction and the top conversion
    //    (the candidate shown after the space key) may differ.
    //
    // The new function brings consistent results. If two candidate
    // have the same reading (key), they should have the same cost bonus
    // from the length part. This implies that the result is reranked by
    // the language model probability as long as the key part is the same.
    // This behavior is baisically the same as the converter.
    //
    // TODO(team): want find the best parameter instead of kCostFactor.
    constexpr int kCostFactor = 500;
    results->at(i).cost =
        cost -
        kCostFactor *
            log(1.0 + std::max(0, static_cast<int>(key_len - query_len)));

    // Update the minimum cost for REALTIME candidates that have the same key
    // length as input_key.
    if (result.types & REALTIME && result.cost < realtime_cost_min &&
        result.key.size() == input_key.size()) {
      realtime_cost_min = result.cost;
    }
  }

  // Ensure that the REALTIME_TOP candidate has relatively smaller cost than
  // those of REALTIME candidates.
  if (realtime_top_result != nullptr) {
    realtime_top_result->cost = std::max(0, realtime_cost_min - 10);
  }
}

void DictionaryPredictor::SetPredictionCostForMixedConversion(
    const Segments &segments, std::vector<Result> *results) const {
  DCHECK(results);

  // ranking for mobile
  int rid = 0;        // 0 (BOS) is default
  int prev_cost = 0;  // cost of the last history candidate.

  if (segments.history_segments_size() > 0) {
    const Segment &history_segment =
        segments.history_segment(segments.history_segments_size() - 1);
    if (history_segment.candidates_size() > 0) {
      rid = history_segment.candidate(0).rid;  // use history segment's id
      prev_cost = history_segment.candidate(0).cost;
      if (prev_cost == 0) {
        // if prev_cost is set to be 0 for some reason, use default cost.
        prev_cost = 5000;
      }
    }
  }

  const size_t input_key_len =
      Util::CharsLen(segments.conversion_segment(0).key());

  for (Result &result : *results) {
    int cost = GetLMCost(result, rid);
    MOZC_WORD_LOG(result, absl::StrCat("GetLMCost: ", cost));

    // Demote filtered word here, because they are not filtered for exact match.
    // Even for exact match, we don't want to show aggressive words with high
    // ranking.
    if (suggestion_filter_->IsBadSuggestion(result.value)) {
      // Cost penalty means for bad suggestion.
      // 3453 = 500 * log(1000)
      constexpr int kBadSuggestionPenalty = 3453;
      cost += kBadSuggestionPenalty;
      MOZC_WORD_LOG(result, absl::StrCat("BadSuggestionPenalty: ", cost));
    }

    // Make exact candidates to have higher ranking.
    // Because for mobile, suggestion is the main candidates and
    // users expect the candidates for the input key on the candidates.
    if (result.types & (UNIGRAM | TYPING_CORRECTION)) {
      const size_t key_len = Util::CharsLen(result.key);
      if (key_len > input_key_len) {
        // Cost penalty means that exact candidates are evaluated
        // 50 times bigger in frequency.
        // Note that the cost is calculated by cost = -500 * log(prob)
        // 1956 = 500 * log(50)
        constexpr int kNotExactPenalty = 1956;
        cost += kNotExactPenalty;
        MOZC_WORD_LOG(result,
                      absl::StrCat("Unigram | Typing correction: ", cost));
      }
    }

    if (result.types & BIGRAM) {
      // When user inputs "六本木" and there is an entry
      // "六本木ヒルズ" in the dictionary, we can suggest
      // "ヒルズ" as a ZeroQuery suggestion. In this case,
      // We can't calcurate the transition cost between "六本木"
      // and "ヒルズ". If we ignore the transition cost,
      // bigram-based suggestion will be overestimated.
      // Here we use kDefaultTransitionCost as an
      // transition cost between "六本木" and "ヒルズ". Currently,
      // the cost is basically the same as the cost between
      // "名詞,一般" and "名詞,一般".
      // TODO(taku): Adjust these parameters.
      // Seems the bigram is overestimated.
      constexpr int kDefaultTransitionCost = 1347;
      // Promoting bigram candidates.
      constexpr int kBigramBonus = 800;  // ~= 500*ln(5)
      cost += (kDefaultTransitionCost - kBigramBonus - prev_cost);
      MOZC_WORD_LOG(result, absl::StrCat("Bigram: ", cost));
    }

    if (result.candidate_attributes & Segment::Candidate::USER_DICTIONARY &&
        result.lid != general_symbol_id_) {
      // Decrease cost for words from user dictionary in order to promote them,
      // provided that it is not a general symbol (Note: emoticons are mapped to
      // general symbol).  Currently user dictionary words are evaluated 5 times
      // bigger in frequency, being capped by 1000 (this number is adhoc, so
      // feel free to adjust).
      constexpr int kUserDictionaryPromotionFactor = 804;  // 804 = 500 * log(5)
      constexpr int kUserDictionaryCostUpperLimit = 1000;
      cost = std::min(cost - kUserDictionaryPromotionFactor,
                      kUserDictionaryCostUpperLimit);
      MOZC_WORD_LOG(result, absl::StrCat("User dictionary: ", cost));
    }

    // Note that the cost is defined as -500 * log(prob).
    // Even after the ad hoc manipulations, cost must remain larger than 0.
    result.cost = std::max(1, cost);
    MOZC_WORD_LOG(result, absl::StrCat("SetLMCost: ", result.cost));
  }
}

void DictionaryPredictor::ApplyPenaltyForKeyExpansion(
    const Segments &segments, std::vector<Result> *results) const {
  if (segments.conversion_segments_size() == 0) {
    return;
  }
  // Cost penalty 1151 means that expanded candidates are evaluated
  // 10 times smaller in frequency.
  // Note that the cost is calcurated by cost = -500 * log(prob)
  // 1151 = 500 * log(10)
  constexpr int kKeyExpansionPenalty = 1151;
  const std::string &conversion_key = segments.conversion_segment(0).key();
  for (size_t i = 0; i < results->size(); ++i) {
    Result &result = results->at(i);
    if (result.types & TYPING_CORRECTION) {
      continue;
    }
    if (!Util::StartsWith(result.key, conversion_key)) {
      result.cost += kKeyExpansionPenalty;
      MOZC_WORD_LOG(result, absl::StrCat("KeyExpansionPenalty: ", result.cost));
    }
  }
}

size_t DictionaryPredictor::GetMissSpelledPosition(
    const std::string &key, const std::string &value) const {
  std::string hiragana_value;
  Util::KatakanaToHiragana(value, &hiragana_value);
  // value is mixed type. return true if key == request_key.
  if (Util::GetScriptType(hiragana_value) != Util::HIRAGANA) {
    return Util::CharsLen(key);
  }

  // Find the first position of character where miss spell occurs.
  int position = 0;
  ConstChar32Iterator key_iter(key);
  for (ConstChar32Iterator hiragana_iter(hiragana_value);
       !hiragana_iter.Done() && !key_iter.Done();
       hiragana_iter.Next(), key_iter.Next(), ++position) {
    if (hiragana_iter.Get() != key_iter.Get()) {
      return position;
    }
  }

  // not find. return the length of key.
  while (!key_iter.Done()) {
    ++position;
    key_iter.Next();
  }

  return position;
}

void DictionaryPredictor::RemoveMissSpelledCandidates(
    size_t request_key_len, std::vector<Result> *results) const {
  DCHECK(results);

  if (results->size() <= 1) {
    return;
  }

  int spelling_correction_size = 5;
  for (size_t i = 0; i < results->size(); ++i) {
    const Result &result = (*results)[i];
    if (!(result.candidate_attributes &
          Segment::Candidate::SPELLING_CORRECTION)) {
      continue;
    }

    // Only checks at most 5 spelling corrections to avoid the case
    // like all candidates have SPELLING_CORRECTION.
    if (--spelling_correction_size == 0) {
      return;
    }

    std::vector<size_t> same_key_index, same_value_index;
    for (size_t j = 0; j < results->size(); ++j) {
      if (i == j) {
        continue;
      }
      const Result &target_result = (*results)[j];
      if (target_result.candidate_attributes &
          Segment::Candidate::SPELLING_CORRECTION) {
        continue;
      }
      if (target_result.key == result.key) {
        same_key_index.push_back(j);
      }
      if (target_result.value == result.value) {
        same_value_index.push_back(j);
      }
    }

    // delete same_key_index and same_value_index
    if (!same_key_index.empty() && !same_value_index.empty()) {
      results->at(i).removed = true;
      MOZC_WORD_LOG(results->at(i), "Removed. same_(key|value)_index.");
      for (size_t k = 0; k < same_key_index.size(); ++k) {
        results->at(same_key_index[k]).removed = true;
        MOZC_WORD_LOG(results->at(i), "Removed. same_(key|value)_index.");
      }
    } else if (same_key_index.empty() && !same_value_index.empty()) {
      results->at(i).removed = true;
      MOZC_WORD_LOG(results->at(i), "Removed. same_value_index.");
    } else if (!same_key_index.empty() && same_value_index.empty()) {
      for (size_t k = 0; k < same_key_index.size(); ++k) {
        results->at(same_key_index[k]).removed = true;
        MOZC_WORD_LOG(results->at(i), "Removed. same_key_index.");
      }
      if (request_key_len <= GetMissSpelledPosition(result.key, result.value)) {
        results->at(i).removed = true;
        MOZC_WORD_LOG(results->at(i), "Removed. Invalid MissSpelledPosition.");
      }
    }
  }
}

bool DictionaryPredictor::IsAggressiveSuggestion(
    size_t query_len, size_t key_len, int cost, bool is_suggestion,
    size_t total_candidates_size) const {
  // Temporal workaround for fixing the problem where longer sentence-like
  // suggestions are shown when user input is very short.
  // "ただしい" => "ただしいけめんにかぎる"
  // "それでもぼ" => "それでもぼくはやっていない".
  // If total_candidates_size is small enough, we don't perform
  // special filtering. e.g., "せんとち" has only two candidates, so
  // showing "千と千尋の神隠し" is OK.
  // Also, if the cost is too small (< 5000), we allow to display
  // long phrases. Examples include "よろしくおねがいします".
  if (is_suggestion && total_candidates_size >= 10 && key_len >= 8 &&
      cost >= 5000 && query_len <= static_cast<size_t>(0.4 * key_len)) {
    return true;
  }

  return false;
}

size_t DictionaryPredictor::GetRealtimeCandidateMaxSize(
    const Segments &segments, bool mixed_conversion) const {
  const Segments::RequestType request_type = segments.request_type();
  DCHECK(request_type == Segments::PREDICTION ||
         request_type == Segments::SUGGESTION ||
         request_type == Segments::PARTIAL_PREDICTION ||
         request_type == Segments::PARTIAL_SUGGESTION);
  if (segments.conversion_segments_size() == 0) {
    return 0;
  }
  const bool is_long_key = IsLongKeyForRealtimeCandidates(segments);
  const size_t max_size =
      GetMaxSizeForRealtimeCandidates(segments, is_long_key);
  const size_t default_size = GetDefaultSizeForRealtimeCandidates(is_long_key);
  size_t size = 0;
  switch (request_type) {
    case Segments::PREDICTION:
      size = mixed_conversion ? max_size : default_size;
      break;
    case Segments::SUGGESTION:
      // Fewer candidatats are needed basically.
      // But on mixed_conversion mode we should behave like as conversion mode.
      size = mixed_conversion ? default_size : 1;
      break;
    case Segments::PARTIAL_PREDICTION:
      // This is kind of prediction so richer result than PARTIAL_SUGGESTION
      // is needed.
      size = max_size;
      break;
    case Segments::PARTIAL_SUGGESTION:
      // PARTIAL_SUGGESTION works like as conversion mode so returning
      // some candidates is needed.
      size = default_size;
      break;
    default:
      size = 0;  // Never reach here
  }

  return std::min(max_size, size);
}

bool DictionaryPredictor::PushBackTopConversionResult(
    const ConversionRequest &request, const Segments &segments,
    std::vector<Result> *results) const {
  DCHECK_EQ(1, segments.conversion_segments_size());

  Segments tmp_segments = segments;
  tmp_segments.set_max_conversion_candidates_size(20);
  ConversionRequest tmp_request = request;
  tmp_request.set_composer_key_selection(ConversionRequest::PREDICTION_KEY);
  // Some rewriters cause significant performance loss. So we skip them.
  tmp_request.set_skip_slow_rewriters(true);
  // This method emulates usual converter's behavior so here disable
  // partial candidates.
  tmp_request.set_create_partial_candidates(false);
  if (!converter_->StartConversionForRequest(tmp_request, &tmp_segments)) {
    return false;
  }

  results->push_back(Result());
  Result *result = &results->back();
  result->key = segments.conversion_segment(0).key();
  result->lid = tmp_segments.conversion_segment(0).candidate(0).lid;
  result->rid =
      tmp_segments
          .conversion_segment(tmp_segments.conversion_segments_size() - 1)
          .candidate(0)
          .rid;
  result->SetTypesAndTokenAttributes(REALTIME | REALTIME_TOP, Token::NONE);
  result->candidate_attributes |= Segment::Candidate::NO_VARIANTS_EXPANSION;

  // Concatenate the top candidates.
  // Note that since StartConversionForRequest() runs in conversion mode, the
  // resulting |tmp_segments| doesn't have inner_segment_boundary. We need to
  // construct it manually here.
  // TODO(noriyukit): This is code duplicate in converter/nbest_generator.cc and
  // we should refactor code after finding more good design.
  bool inner_segment_boundary_success = true;
  for (size_t i = 0; i < tmp_segments.conversion_segments_size(); ++i) {
    const Segment &segment = tmp_segments.conversion_segment(i);
    const Segment::Candidate &candidate = segment.candidate(0);
    result->value.append(candidate.value);
    result->wcost += candidate.cost;

    uint32_t encoded_lengths;
    if (inner_segment_boundary_success &&
        Segment::Candidate::EncodeLengths(
            candidate.key.size(), candidate.value.size(),
            candidate.content_key.size(), candidate.content_value.size(),
            &encoded_lengths)) {
      result->inner_segment_boundary.push_back(encoded_lengths);
    } else {
      inner_segment_boundary_success = false;
    }
  }
  if (!inner_segment_boundary_success) {
    LOG(WARNING) << "Failed to construct inner segment boundary";
    result->inner_segment_boundary.clear();
  }
  return true;
}

void DictionaryPredictor::AggregateRealtimeConversion(
    const ConversionRequest &request, size_t realtime_candidates_size,
    Segments *segments, std::vector<Result> *results) const {
  DCHECK(converter_);
  DCHECK(immutable_converter_);
  DCHECK(segments);
  DCHECK(results);

  // TODO(noriyukit): Currently, |segments| is abused as a temporary output from
  // the immutable converter. Therefore, the first segment needs to be mutable.
  // Fix this bad abuse.
  Segment *segment = segments->mutable_conversion_segment(0);
  DCHECK(!segment->key().empty());

  // First insert a top conversion result.
  if (request.use_actual_converter_for_realtime_conversion()) {
    if (!PushBackTopConversionResult(request, *segments, results)) {
      LOG(WARNING) << "Realtime conversion with converter failed";
    }
  }

  if (realtime_candidates_size == 0) {
    return;
  }
  // In what follows, add results from immutable converter.
  // TODO(noriyukit): The |immutable_converter_| used below can be replaced by
  // |converter_| in principle.  There's a problem of ranking when we get
  // multiple segments, i.e., how to concatenate candidates in each segment.
  // Currently, immutable converter handles such ranking in prediction mode to
  // generate single segment results. So we want to share that code.

  // Preserve the current max_prediction_candidates_size and candidates_size to
  // restore them at the end of this method.
  const size_t prev_candidates_size = segment->candidates_size();
  const size_t prev_max_prediction_candidates_size =
      segments->max_prediction_candidates_size();

  segments->set_max_prediction_candidates_size(prev_candidates_size +
                                               realtime_candidates_size);

  if (!immutable_converter_->ConvertForRequest(request, segments) ||
      prev_candidates_size >= segment->candidates_size()) {
    LOG(WARNING) << "Convert failed";
    return;
  }

  // A little tricky treatment:
  // Since ImmutableConverter::Convert creates a set of new candidates,
  // copy them into the array of Results.
  for (size_t i = prev_candidates_size; i < segment->candidates_size(); ++i) {
    const Segment::Candidate &candidate = segment->candidate(i);
    results->push_back(Result());
    Result *result = &results->back();
    result->key = candidate.key;
    result->value = candidate.value;
    result->wcost = candidate.wcost;
    result->lid = candidate.lid;
    result->rid = candidate.rid;
    result->inner_segment_boundary = candidate.inner_segment_boundary;
    if (IsSimplifiedRankingEnabled(request)) {
      result->SetTypesAndTokenAttributes(
          (candidate.attributes & Segment::Candidate::SUFFIX_DICTIONARY)
              ? REALTIME | SUFFIX
              : REALTIME,
          Token::NONE);
    } else {
      result->SetTypesAndTokenAttributes(REALTIME, Token::NONE);
    }
    result->candidate_attributes |= candidate.attributes;
    result->consumed_key_size = candidate.consumed_key_size;
  }
  // Remove candidates created by ImmutableConverter.
  segment->erase_candidates(prev_candidates_size,
                            segment->candidates_size() - prev_candidates_size);
  // Restore the max_prediction_candidates_size.
  segments->set_max_prediction_candidates_size(
      prev_max_prediction_candidates_size);
}

size_t DictionaryPredictor::GetCandidateCutoffThreshold(
    const Segments &segments) const {
  DCHECK(segments.request_type() == Segments::PREDICTION ||
         segments.request_type() == Segments::SUGGESTION);
  if (segments.request_type() == Segments::PREDICTION) {
    // If PREDICTION, many candidates are needed than SUGGESTION.
    return kPredictionMaxResultsSize;
  }
  return kSuggestionMaxResultsSize;
}

DictionaryPredictor::PredictionType
DictionaryPredictor::AggregateUnigramCandidate(
    const ConversionRequest &request, const Segments &segments,
    std::vector<Result> *results) const {
  DCHECK(results);
  DCHECK(dictionary_);
  DCHECK(segments.request_type() == Segments::PREDICTION ||
         segments.request_type() == Segments::SUGGESTION);

  const size_t cutoff_threshold = GetCandidateCutoffThreshold(segments);
  const size_t prev_results_size = results->size();
  GetPredictiveResults(*dictionary_, "", request, segments, UNIGRAM,
                       cutoff_threshold, Segment::Candidate::SOURCE_INFO_NONE,
                       unknown_id_, results);
  const size_t unigram_results_size = results->size() - prev_results_size;

  // If size reaches max_results_size (== cutoff_threshold).
  // we don't show the candidates, since disambiguation from
  // 256 candidates is hard. (It may exceed max_results_size, because this is
  // just a limit for each backend, so total number may be larger)
  if (unigram_results_size >= cutoff_threshold) {
    results->resize(prev_results_size);
  }
  return UNIGRAM;
}

DictionaryPredictor::PredictionType
DictionaryPredictor::AggregateUnigramCandidateForMixedConversion(
    const ConversionRequest &request, const Segments &segments,
    std::vector<Result> *results) const {
  DCHECK(segments.request_type() == Segments::PREDICTION ||
         segments.request_type() == Segments::SUGGESTION);
  AggregateUnigramCandidateForMixedConversion(*dictionary_, request, segments,
                                              unknown_id_, results);
  return UNIGRAM;
}

void DictionaryPredictor::AggregateUnigramCandidateForMixedConversion(
    const dictionary::DictionaryInterface &dictionary,
    const ConversionRequest &request, const Segments &segments, int unknown_id,
    std::vector<Result> *results) {
  const size_t cutoff_threshold = kPredictionMaxResultsSize;

  std::vector<Result> raw_result;
  // No history key
  GetPredictiveResults(dictionary, "", request, segments, UNIGRAM,
                       cutoff_threshold, Segment::Candidate::SOURCE_INFO_NONE,
                       unknown_id, &raw_result);

  // Hereafter, we split "Needed Results" and "(maybe) Unneeded Results."
  // The algorithm is:
  // 1) Take the Result with minimum cost.
  // 2) Remove results which is "redundant" (defined by MaybeRedundant),
  //    from remaining results.
  // 3) Repeat 1) and 2) five times.
  // Note: to reduce the number of memory allocation, we swap out the
  //   "redundant" results to the end of the |results| vector.
  constexpr size_t kDeleteTrialNum = 5;

  // min_iter is the beginning of the remaining results (inclusive), and
  // max_iter is the end of the remaining results (exclusive).
  typedef std::vector<Result>::iterator Iter;
  Iter min_iter = raw_result.begin();
  Iter max_iter = raw_result.end();
  for (size_t i = 0; i < kDeleteTrialNum; ++i) {
    if (min_iter == max_iter) {
      break;
    }

    // Find the Result with minimum cost. Swap it with the beginning element.
    std::iter_swap(min_iter,
                   std::min_element(min_iter, max_iter, ResultWCostLess()));

    const Result &reference_result = *min_iter;

    // Preserve the reference result.
    ++min_iter;

    // Traverse all remaining elements and check if each result is redundant.
    for (Iter iter = min_iter; iter != max_iter;) {
      // - We do not filter user dictionary word.
      const bool should_check_redundant = !iter->IsUserDictionaryResult();
      if (should_check_redundant &&
          MaybeRedundant(reference_result.value, iter->value)) {
        // Swap out the redundant result.
        --max_iter;
        std::iter_swap(iter, max_iter);
      } else {
        ++iter;
      }
    }
  }

  // Then the |raw_result| contains;
  // [begin, min_iter): reference results in the above loop.
  // [max_iter, end): (maybe) redundant results.
  // [min_iter, max_iter): remaining results.
  // Here, we revive the redundant results up to five in the result cost order.
  constexpr size_t kDoNotDeleteNum = 5;
  if (std::distance(max_iter, raw_result.end()) >= kDoNotDeleteNum) {
    std::partial_sort(max_iter, max_iter + kDoNotDeleteNum, raw_result.end(),
                      ResultWCostLess());
    max_iter += kDoNotDeleteNum;
  } else {
    max_iter = raw_result.end();
  }

  // Finally output the result.
  results->insert(results->end(), raw_result.begin(), max_iter);
}

void DictionaryPredictor::AggregateBigramPrediction(
    const ConversionRequest &request, const Segments &segments,
    Segment::Candidate::SourceInfo source_info,
    std::vector<Result> *results) const {
  DCHECK(results);
  DCHECK(dictionary_);

  // TODO(toshiyuki): Support suggestion from the last 2 histories.
  //  ex) "六本木"+"ヒルズ"->"レジデンス"
  std::string history_key, history_value;
  if (!GetHistoryKeyAndValue(segments, &history_key, &history_value)) {
    return;
  }
  AddBigramResultsFromHistory(history_key, history_value, request, segments,
                              source_info, results);
}

void DictionaryPredictor::AddBigramResultsFromHistory(
    const std::string &history_key, const std::string &history_value,
    const ConversionRequest &request, const Segments &segments,
    Segment::Candidate::SourceInfo source_info,
    std::vector<Result> *results) const {
  // Check that history_key/history_value are in the dictionary.
  FindValueCallback find_history_callback(history_value);
  dictionary_->LookupPrefix(history_key, request, &find_history_callback);

  // History value is not found in the dictionary.
  // User may create this the history candidate from T13N or segment
  // expand/shrinkg operations.
  if (!find_history_callback.found()) {
    return;
  }

  const size_t cutoff_threshold = GetCandidateCutoffThreshold(segments);
  const size_t prev_results_size = results->size();
  GetPredictiveResultsForBigram(*dictionary_, history_key, history_value,
                                request, segments, BIGRAM, cutoff_threshold,
                                source_info, unknown_id_, results);
  const size_t bigram_results_size = results->size() - prev_results_size;

  // if size reaches max_results_size,
  // we don't show the candidates, since disambiguation from
  // 256 candidates is hard. (It may exceed max_results_size, because this is
  // just a limit for each backend, so total number may be larger)
  if (bigram_results_size >= cutoff_threshold) {
    results->resize(prev_results_size);
    return;
  }

  // Obtain the character type of the last history value.
  const size_t history_value_size = Util::CharsLen(history_value);
  if (history_value_size == 0) {
    return;
  }

  const Util::ScriptType history_ctype = Util::GetScriptType(history_value);
  const Util::ScriptType last_history_ctype = Util::GetScriptType(
      Util::Utf8SubString(history_value, history_value_size - 1, 1));
  for (size_t i = prev_results_size; i < results->size(); ++i) {
    CheckBigramResult(find_history_callback.token(), history_ctype,
                      last_history_ctype, request, &(*results)[i]);
  }
}

// Filter out irrelevant bigrams. For example, we don't want to
// suggest "リカ" from the history "アメ".
void DictionaryPredictor::CheckBigramResult(
    const Token &history_token, const Util::ScriptType history_ctype,
    const Util::ScriptType last_history_ctype, const ConversionRequest &request,
    Result *result) const {
  DCHECK(result);

  const std::string &history_key = history_token.key;
  const std::string &history_value = history_token.value;
  const std::string key(result->key, history_key.size(),
                        result->key.size() - history_key.size());
  const std::string value(result->value, history_value.size(),
                          result->value.size() - history_value.size());

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
  const size_t key_len = Util::CharsLen(result->key);
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
    // TODO(toshiyuki): one-length kanji prediciton may be annoying other than
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
    if (!dictionary_->HasKey(key)) {
      result->removed = true;
      MOZC_WORD_LOG(*result, "Removed. No keys are found.");
      return;
    }
  } else {
    FindValueCallback callback(value);
    dictionary_->LookupPrefix(key, request, &callback);
    if (!callback.found()) {
      result->removed = true;
      MOZC_WORD_LOG(*result, "Removed. No prefix found.");
      return;
    }
  }

  MOZC_WORD_LOG(*result, "Valid bigram.");
}

void DictionaryPredictor::GetPredictiveResults(
    const DictionaryInterface &dictionary, const std::string &history_key,
    const ConversionRequest &request, const Segments &segments,
    PredictionTypes types, size_t lookup_limit,
    Segment::Candidate::SourceInfo source_info, int unknown_id_,
    std::vector<Result> *results) {
  if (!request.has_composer()) {
    std::string input_key = history_key;
    input_key.append(segments.conversion_segment(0).key());
    PredictiveLookupCallback callback(types, lookup_limit, input_key.size(),
                                      nullptr, source_info, unknown_id_,
                                      results);
    dictionary.LookupPredictive(input_key, request, &callback);
    return;
  }

  // If we have ambiguity for the input, get expanded key.
  // Example1 roman input: for "あk", we will get |base|, "あ" and |expanded|,
  // "か", "き", etc
  // Example2 kana input: for "あか", we will get |base|, "あ" and |expanded|,
  // "か", and "が".
  std::string base;
  std::set<std::string> expanded;
  request.composer().GetQueriesForPrediction(&base, &expanded);
  std::string input_key;
  if (expanded.empty()) {
    input_key.assign(history_key).append(base);
    PredictiveLookupCallback callback(types, lookup_limit, input_key.size(),
                                      nullptr, source_info, unknown_id_,
                                      results);
    dictionary.LookupPredictive(input_key, request, &callback);
    return;
  }
  // |expanded| is a very small set, so calling LookupPredictive multiple
  // times is not so expensive.  Also, the number of lookup results is limited
  // by |lookup_limit|.
  for (const std::string &expanded_char : expanded) {
    input_key.assign(history_key).append(base).append(expanded_char);
    PredictiveLookupCallback callback(types, lookup_limit, input_key.size(),
                                      nullptr, source_info, unknown_id_,
                                      results);
    dictionary.LookupPredictive(input_key, request, &callback);
  }
}

void DictionaryPredictor::GetPredictiveResultsForBigram(
    const DictionaryInterface &dictionary, const std::string &history_key,
    const std::string &history_value, const ConversionRequest &request,
    const Segments &segments, PredictionTypes types, size_t lookup_limit,
    Segment::Candidate::SourceInfo source_info, int unknown_id_,
    std::vector<Result> *results) const {
  if (!request.has_composer()) {
    std::string input_key = history_key;
    input_key.append(segments.conversion_segment(0).key());
    PredictiveBigramLookupCallback callback(
        types, lookup_limit, input_key.size(), nullptr, history_value,
        source_info, unknown_id_, results);
    dictionary.LookupPredictive(input_key, request, &callback);
    return;
  }

  // If we have ambiguity for the input, get expanded key.
  // Example1 roman input: for "あk", we will get |base|, "あ" and |expanded|,
  // "か", "き", etc
  // Example2 kana input: for "あか", we will get |base|, "あ" and |expanded|,
  // "か", and "が".
  std::string base;
  std::set<std::string> expanded;
  request.composer().GetQueriesForPrediction(&base, &expanded);
  std::string input_key = history_key;
  input_key.append(base);
  PredictiveBigramLookupCallback callback(
      types, lookup_limit, input_key.size(),
      expanded.empty() ? nullptr : &expanded, history_value, source_info,
      unknown_id_, results);
  dictionary.LookupPredictive(input_key, request, &callback);
}

void DictionaryPredictor::GetPredictiveResultsForEnglishKey(
    const DictionaryInterface &dictionary, const ConversionRequest &request,
    const std::string &input_key, PredictionTypes types, size_t lookup_limit,
    std::vector<Result> *results) const {
  const size_t prev_results_size = results->size();
  if (Util::IsUpperAscii(input_key)) {
    // For upper case key, look up its lower case version and then transform
    // the results to upper case.
    std::string key(input_key);
    Util::LowerString(&key);
    PredictiveLookupCallback callback(types, lookup_limit, key.size(), nullptr,
                                      Segment::Candidate::SOURCE_INFO_NONE,
                                      unknown_id_, results);
    dictionary.LookupPredictive(key, request, &callback);
    for (size_t i = prev_results_size; i < results->size(); ++i) {
      Util::UpperString(&(*results)[i].value);
    }
  } else if (Util::IsCapitalizedAscii(input_key)) {
    // For capitalized key, look up its lower case version and then transform
    // the results to capital.
    std::string key(input_key);
    Util::LowerString(&key);
    PredictiveLookupCallback callback(types, lookup_limit, key.size(), nullptr,
                                      Segment::Candidate::SOURCE_INFO_NONE,
                                      unknown_id_, results);
    dictionary.LookupPredictive(key, request, &callback);
    for (size_t i = prev_results_size; i < results->size(); ++i) {
      Util::CapitalizeString(&(*results)[i].value);
    }
  } else {
    // For other cases (lower and as-is), just look up directly.
    PredictiveLookupCallback callback(
        types, lookup_limit, input_key.size(), nullptr,
        Segment::Candidate::SOURCE_INFO_NONE, unknown_id_, results);
    dictionary.LookupPredictive(input_key, request, &callback);
  }
  // If input mode is FULL_ASCII, then convert the results to full-width.
  if (request.has_composer() &&
      request.composer().GetInputMode() == transliteration::FULL_ASCII) {
    std::string tmp;
    for (size_t i = prev_results_size; i < results->size(); ++i) {
      tmp.assign((*results)[i].value);
      Util::HalfWidthAsciiToFullWidthAscii(tmp, &(*results)[i].value);
    }
  }
}

void DictionaryPredictor::GetPredictiveResultsUsingTypingCorrection(
    const DictionaryInterface &dictionary, const std::string &history_key,
    const ConversionRequest &request, const Segments &segments,
    PredictionTypes types, size_t lookup_limit,
    std::vector<Result> *results) const {
  if (!request.has_composer()) {
    return;
  }

  std::vector<composer::TypeCorrectedQuery> queries;
  request.composer().GetTypeCorrectedQueriesForPrediction(&queries);
  for (size_t query_index = 0; query_index < queries.size(); ++query_index) {
    const composer::TypeCorrectedQuery &query = queries[query_index];
    const std::string input_key = history_key + query.base;
    const size_t previous_results_size = results->size();
    PredictiveLookupCallback callback(
        types, lookup_limit, input_key.size(),
        query.expanded.empty() ? nullptr : &query.expanded,
        Segment::Candidate::SOURCE_INFO_NONE, unknown_id_, results);
    dictionary.LookupPredictive(input_key, request, &callback);

    for (size_t i = previous_results_size; i < results->size(); ++i) {
      (*results)[i].wcost += query.cost;
    }
    lookup_limit -= results->size() - previous_results_size;
    if (lookup_limit <= 0) {
      break;
    }
  }
}

// static
bool DictionaryPredictor::GetZeroQueryCandidatesForKey(
    const ConversionRequest &request, const std::string &key,
    const ZeroQueryDict &dict, std::vector<ZeroQueryResult> *results) {
  const int32_t available_emoji_carrier =
      request.request().available_emoji_carrier();

  DCHECK(results);
  results->clear();

  auto range = dict.equal_range(key);
  if (range.first == range.second) {
    return false;
  }
  for (; range.first != range.second; ++range.first) {
    const auto &entry = range.first;
    if (entry.type() != ZERO_QUERY_EMOJI) {
      results->push_back(
          std::make_pair(std::string(entry.value()), entry.type()));
      continue;
    }
    if (available_emoji_carrier & Request::UNICODE_EMOJI &&
        entry.emoji_type() & EMOJI_UNICODE) {
      results->push_back(
          std::make_pair(std::string(entry.value()), entry.type()));
      continue;
    }

    if ((available_emoji_carrier & Request::DOCOMO_EMOJI &&
         entry.emoji_type() & EMOJI_DOCOMO) ||
        (available_emoji_carrier & Request::SOFTBANK_EMOJI &&
         entry.emoji_type() & EMOJI_SOFTBANK) ||
        (available_emoji_carrier & Request::KDDI_EMOJI &&
         entry.emoji_type() & EMOJI_KDDI)) {
      std::string android_pua;
      Util::Ucs4ToUtf8(entry.emoji_android_pua(), &android_pua);
      results->push_back(std::make_pair(android_pua, entry.type()));
    }
  }
  return !results->empty();
}

// static
void DictionaryPredictor::AppendZeroQueryToResults(
    const std::vector<ZeroQueryResult> &candidates, uint16_t lid, uint16_t rid,
    std::vector<Result> *results) {
  int cost = 0;

  for (size_t i = 0; i < candidates.size(); ++i) {
    // Increment cost to show the candidates in order.
    constexpr int kSuffixPenalty = 10;

    results->push_back(Result());
    Result *result = &results->back();
    result->SetTypesAndTokenAttributes(SUFFIX, Token::NONE);
    result->SetSourceInfoForZeroQuery(candidates[i].second);
    result->key = candidates[i].first;
    result->value = candidates[i].first;
    result->wcost = cost;
    result->lid = lid;
    result->rid = rid;

    cost += kSuffixPenalty;
  }
}

// Returns true if we add zero query result.
bool DictionaryPredictor::AggregateNumberZeroQueryPrediction(
    const ConversionRequest &request, const Segments &segments,
    std::vector<Result> *results) const {
  std::string number_key;
  if (!GetNumberHistory(segments, &number_key)) {
    return false;
  }

  std::vector<ZeroQueryResult> candidates_for_number_key;
  GetZeroQueryCandidatesForKey(request, number_key, zero_query_number_dict_,
                               &candidates_for_number_key);

  std::vector<ZeroQueryResult> default_candidates_for_number;
  GetZeroQueryCandidatesForKey(request, "default", zero_query_number_dict_,
                               &default_candidates_for_number);
  DCHECK(!default_candidates_for_number.empty());

  AppendZeroQueryToResults(candidates_for_number_key, counter_suffix_word_id_,
                           counter_suffix_word_id_, results);
  AppendZeroQueryToResults(default_candidates_for_number,
                           counter_suffix_word_id_, counter_suffix_word_id_,
                           results);
  return true;
}

// Returns true if we add zero query result.
bool DictionaryPredictor::AggregateZeroQueryPrediction(
    const ConversionRequest &request, const Segments &segments,
    std::vector<Result> *results) const {
  const size_t history_size = segments.history_segments_size();
  if (history_size <= 0) {
    return false;
  }

  const Segment &last_segment = segments.history_segment(history_size - 1);
  DCHECK_GT(last_segment.candidates_size(), 0);
  const std::string &history_value = last_segment.candidate(0).value;

  std::vector<ZeroQueryResult> candidates;
  if (!GetZeroQueryCandidatesForKey(request, history_value, zero_query_dict_,
                                    &candidates)) {
    return false;
  }

  const uint16_t kId = 0;  // EOS
  AppendZeroQueryToResults(candidates, kId, kId, results);
  return true;
}

void DictionaryPredictor::AggregateSuffixPrediction(
    const ConversionRequest &request, const Segments &segments,
    std::vector<Result> *results) const {
  DCHECK_GT(segments.conversion_segments_size(), 0);
  DCHECK(!segments.conversion_segment(0).key().empty());  // Not zero query
  // Uses larger cutoff (kPredictionMaxResultsSize) in order to consider
  // all suffix entries.
  const size_t cutoff_threshold = kPredictionMaxResultsSize;
  const std::string kEmptyHistoryKey = "";
  GetPredictiveResults(*suffix_dictionary_, kEmptyHistoryKey, request, segments,
                       SUFFIX, cutoff_threshold,
                       Segment::Candidate::SOURCE_INFO_NONE, unknown_id_,
                       results);
}

void DictionaryPredictor::AggregateZeroQuerySuffixPrediction(
    const ConversionRequest &request, const Segments &segments,
    std::vector<Result> *results) const {
  DCHECK_GT(segments.conversion_segments_size(), 0);
  DCHECK(segments.conversion_segment(0).key().empty());

  if (AggregateNumberZeroQueryPrediction(request, segments, results)) {
    return;
  }
  AggregateZeroQueryPrediction(request, segments, results);

  if (IsLatinInputMode(request)) {
    // We do not want zero query results from suffix dictionary for Latin
    // input mode. For example, we do not need "です", "。" just after "when".
    return;
  }
  // Uses larger cutoff (kPredictionMaxResultsSize) in order to consider
  // all suffix entries.
  const size_t cutoff_threshold = kPredictionMaxResultsSize;
  const std::string kEmptyHistoryKey = "";
  GetPredictiveResults(
      *suffix_dictionary_, kEmptyHistoryKey, request, segments, SUFFIX,
      cutoff_threshold,
      Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_SUFFIX, unknown_id_,
      results);
}

void DictionaryPredictor::AggregateEnglishPrediction(
    const ConversionRequest &request, const Segments &segments,
    std::vector<Result> *results) const {
  DCHECK(results);
  DCHECK(dictionary_);

  const size_t cutoff_threshold = GetCandidateCutoffThreshold(segments);
  const size_t prev_results_size = results->size();
  const std::string &input_key = segments.conversion_segment(0).key();
  GetPredictiveResultsForEnglishKey(*dictionary_, request, input_key, ENGLISH,
                                    cutoff_threshold, results);

  size_t unigram_results_size = results->size() - prev_results_size;
  if (unigram_results_size >= cutoff_threshold) {
    results->resize(prev_results_size);
    return;
  }
}

void DictionaryPredictor::AggregateEnglishPredictionUsingRawInput(
    const ConversionRequest &request, const Segments &segments,
    std::vector<Result> *results) const {
  DCHECK(results);
  DCHECK(dictionary_);

  if (!request.has_composer()) {
    return;
  }

  const size_t cutoff_threshold = GetCandidateCutoffThreshold(segments);
  const size_t prev_results_size = results->size();

  std::string input_key;
  request.composer().GetRawString(&input_key);
  GetPredictiveResultsForEnglishKey(*dictionary_, request, input_key, ENGLISH,
                                    cutoff_threshold, results);

  size_t unigram_results_size = results->size() - prev_results_size;
  if (unigram_results_size >= cutoff_threshold) {
    results->resize(prev_results_size);
    return;
  }
}

void DictionaryPredictor::AggregateTypeCorrectingPrediction(
    const ConversionRequest &request, const Segments &segments,
    std::vector<Result> *results) const {
  DCHECK(results);
  DCHECK(dictionary_);

  const size_t prev_results_size = results->size();
  if (prev_results_size > 10000) {
    return;
  }

  const size_t cutoff_threshold = GetCandidateCutoffThreshold(segments);

  // Currently, history key is never utilized.
  const std::string kEmptyHistoryKey = "";
  GetPredictiveResultsUsingTypingCorrection(
      *dictionary_, kEmptyHistoryKey, request, segments, TYPING_CORRECTION,
      cutoff_threshold, results);
  if (results->size() - prev_results_size >= cutoff_threshold) {
    results->resize(prev_results_size);
    return;
  }
}

bool DictionaryPredictor::ShouldAggregateRealTimeConversionResults(
    const ConversionRequest &request, const Segments &segments) {
  constexpr size_t kMaxRealtimeKeySize = 300;  // 300 bytes in UTF8
  const std::string &key = segments.conversion_segment(0).key();
  if (key.empty() || key.size() >= kMaxRealtimeKeySize) {
    // 1) If key is empty, realtime conversion doesn't work.
    // 2) If the key is too long, we'll hit a performance issue.
    return false;
  }

  return (segments.request_type() == Segments::PARTIAL_SUGGESTION ||
          request.config().use_realtime_conversion() ||
          IsMixedConversionEnabled(request.request()));
}

bool DictionaryPredictor::IsZipCodeRequest(const std::string &key) {
  if (key.empty()) {
    return false;
  }

  for (ConstChar32Iterator iter(key); !iter.Done(); iter.Next()) {
    const char32 c = iter.Get();
    if (!('0' <= c && c <= '9') && (c != '-')) {
      return false;
    }
  }
  return true;
}

}  // namespace mozc

#undef MOZC_WORD_LOG_MESSAGE
#undef MOZC_WORD_LOG
