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

#include "prediction/dictionary_decoder.h"

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
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/number_util.h"
#include "base/util.h"
#include "converter/attribute.h"
#include "converter/node_list_builder.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_token.h"
#include "engine/modules.h"
#include "prediction/result.h"
#include "prediction/result_filter.h"
#include "request/conversion_request.h"

namespace mozc::prediction {
namespace {

using ::mozc::converter::Attribute;
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

// RAII class to adjust the result size to be `cutoff_threshold`.
class ResultsSizeAdjuster {
 public:
  ResultsSizeAdjuster(const ConversionRequest& request,
                      std::vector<Result>* results)
      : cutoff_threshold_(GetCandidateCutoffThreshold(request.request_type())),
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

// TODO(taku): Move shared callback/lookup helper to a common prediction
// library.
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
    if (penalty_ > 0) result.attributes |= KEY_EXPANDED_IN_DICTIONARY;
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
    return PredictiveLookupCallback::OnToken(key, expanded_key, token);
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

std::optional<Token> FindKeyAndValue(const DictionaryInterface& dic,
                                     const ConversionRequest& request,
                                     absl::string_view key,
                                     absl::string_view value) {
  std::optional<Token> result_token;
  dictionary::InlineCallback cb;
  cb.OnToken([&](absl::string_view, absl::string_view, const Token& token) {
    using enum DictionaryInterface::Callback::ResultType;
    if (token.value != value) return TRAVERSE_CONTINUE;
    result_token = token;
    return TRAVERSE_DONE;
  });
  dic.LookupPrefix(key, request.options(), &cb);
  return result_token;
}

void GetPredictiveResultsForUnigram(const DictionaryInterface& dictionary,
                                    const ConversionRequest& request,
                                    PredictionTypes types, size_t lookup_limit,
                                    int zip_code_id, int unknown_id,
                                    std::vector<Result>* results) {
  const absl::btree_set<std::string> empty_expanded;
  if (request.options().use_already_typing_corrected_key) {
    PredictiveLookupCallback callback(types, lookup_limit, request.key().size(),
                                      empty_expanded, zip_code_id, unknown_id,
                                      results);
    dictionary.LookupPredictive(request.key(), request.options(), &callback);
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
                                      expanded, zip_code_id, unknown_id,
                                      results);
    dictionary.LookupPredictive(base, request.options(), &callback);
    return;
  }

  // |expanded| is a very small set, so calling LookupPredictive multiple
  // times is not so expensive.  Also, the number of lookup results is limited
  // by |lookup_limit|.
  for (absl::string_view expanded_char : expanded) {
    const std::string request_key = absl::StrCat(base, expanded_char);
    PredictiveLookupCallback callback(types, lookup_limit, request_key.size(),
                                      empty_expanded, zip_code_id, unknown_id,
                                      results);
    dictionary.LookupPredictive(request_key, request.options(), &callback);
  }
}

void GetPredictiveResultsForBigram(const DictionaryInterface& dictionary,
                                   const absl::string_view history_key,
                                   const absl::string_view history_value,
                                   const ConversionRequest& request,
                                   PredictionTypes types, size_t lookup_limit,
                                   int zip_code_id, int unknown_id,
                                   std::vector<Result>* results) {
  absl::btree_set<std::string> expanded;

  if (request.options().use_already_typing_corrected_key) {
    const std::string request_key = absl::StrCat(history_key, request.key());
    PredictiveBigramLookupCallback callback(
        types, lookup_limit, request_key.size(), expanded, history_key,
        history_value, zip_code_id, unknown_id, results);
    dictionary.LookupPredictive(request_key, request.options(), &callback);
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
      history_value, zip_code_id, unknown_id, results);
  dictionary.LookupPredictive(request_key, request.options(), &callback);
}

}  // namespace

DictionaryDecoder::DictionaryDecoder(const engine::Modules& modules)
    : modules_(modules),
      dictionary_(modules.GetDictionary()),
      kanji_number_id_(modules.GetPosMatcher().GetKanjiNumberId()),
      zip_code_id_(modules.GetPosMatcher().GetZipcodeId()),
      unknown_id_(modules.GetPosMatcher().GetUnknownId()) {}

void DictionaryDecoder::AggregateUnigram(const ConversionRequest& request,
                                         std::vector<Result>* results,
                                         int* min_unigram_key_len) const {
  DCHECK(results);
  DCHECK(min_unigram_key_len);
  *min_unigram_key_len = 0;

  const size_t key_len = Util::CharsLen(request.key());
  if (key_len == 0) {
    return;
  }

  const bool is_mixed_conversion = request.request().mixed_conversion();

  int min_key_len = 1;
  if (is_mixed_conversion) {
    // In mixed conversion mode, we want to show unigram candidates even for
    // short keys to emulate PREDICTION mode.
    *min_unigram_key_len = min_key_len;
    if (key_len >= min_key_len) {
      AggregateUnigramForMixedConversion(request, results);
    }
  } else {
    // Use the standard dictionary-based prediction by default.
    min_key_len =
        (request.request_type() == ConversionRequest::PREDICTION) ? 1 : 3;
    *min_unigram_key_len = min_key_len;
    if (key_len >= min_key_len) {
      AggregateUnigramForDesktop(request, results);
    }
  }
}

void DictionaryDecoder::AggregateUnigramForDesktop(
    const ConversionRequest& request, std::vector<Result>* results) const {
  DCHECK(results);
  DCHECK(request.request_type() == ConversionRequest::PREDICTION ||
         request.request_type() == ConversionRequest::SUGGESTION);

  const ResultsSizeAdjuster adjuster(request, results);
  GetPredictiveResultsForUnigram(dictionary_, request, UNIGRAM,
                                 adjuster.cutoff_threshold(), zip_code_id_,
                                 unknown_id_, results);
}

void DictionaryDecoder::AggregateUnigramForMixedConversion(
    const ConversionRequest& request, std::vector<Result>* results) const {
  DCHECK(results);
  DCHECK(request.request_type() == ConversionRequest::PREDICTION ||
         request.request_type() == ConversionRequest::SUGGESTION ||
         request.request_type() == ConversionRequest::PARTIAL_PREDICTION ||
         request.request_type() == ConversionRequest::PARTIAL_SUGGESTION);

  std::vector<Result> raw_result;
  GetPredictiveResultsForUnigram(dictionary_, request, UNIGRAM,
                                 kPredictionMaxResultsSize, zip_code_id_,
                                 unknown_id_, &raw_result);
  filter::RemoveRedundantResults(&raw_result);
  absl::c_move(raw_result, std::back_inserter(*results));
}

void DictionaryDecoder::AggregateBigram(const ConversionRequest& request,
                                        std::vector<Result>* results) const {
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
  std::optional<Token> find_history_token =
      FindKeyAndValue(dictionary_, request, history_key, history_value);
  if (!find_history_token.has_value()) {
    // History value is not found in the dictionary.
    // User may create this the history candidate from T13N or segment
    // expand/shrinkg operations.
    return;
  }

  const ResultsSizeAdjuster adjuster(request, results);
  GetPredictiveResultsForBigram(dictionary_, history_key, history_value,
                                request, BIGRAM, adjuster.cutoff_threshold(),
                                zip_code_id_, unknown_id_, results);
  adjuster.AdjustSize();

  const Util::ScriptType history_ctype = Util::GetScriptType(history_value);
  const int history_value_size = Util::CharsLen(history_value);
  const Util::ScriptType last_history_ctype = Util::GetScriptType(
      Util::Utf8SubString(history_value, history_value_size - 1, 1));
  for (Result& result : adjuster.GetAddedResults()) {
    CheckBigramResult(*find_history_token, history_ctype, last_history_ctype,
                      request, &result);
  }
}

// Filter out irrelevant bigrams. For example, we don't want to
// suggest "リカ" from the history "アメ".
void DictionaryDecoder::CheckBigramResult(
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
    if (!FindKeyAndValue(dictionary_, request, key, value)) {
      result->removed = true;
      MOZC_WORD_LOG(*result, "Removed. No prefix found.");
      return;
    }
  }

  MOZC_WORD_LOG(*result, "Valid bigram.");
}

void DictionaryDecoder::AggregatePrefix(const ConversionRequest& request,
                                        std::vector<Result>* results) const {
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
  const int limit = GetCandidateCutoffThreshold(request.request_type());

  dictionary::InlineCallback cb;
  cb.OnToken([&](absl::string_view key, absl::string_view actual_key,
                 const Token& token) {
    using enum DictionaryInterface::Callback::ResultType;
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
    if (Util::CharsLen(token.value) < kMinValueCharsLen) {
      return TRAVERSE_CONTINUE;
    }
    Result result;
    result.InitializeByTokenAndTypes(token, PREFIX);
    if (key != actual_key) {
      result.attributes |= Attribute::TYPING_CORRECTION;
    }
    const int key_len = Util::CharsLen(key);
    if (key_len < request_key_len) {
      result.attributes |= Attribute::PARTIALLY_KEY_CONSUMED;
      result.consumed_key_size = key_len;
    }
    results->emplace_back(std::move(result));
    return (results->size() < limit) ? TRAVERSE_CONTINUE : TRAVERSE_DONE;
  });

  dictionary_.LookupPrefix(lookup_key, request.options(), &cb);
}

}  // namespace mozc::prediction
