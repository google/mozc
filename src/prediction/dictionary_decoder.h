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

#ifndef MOZC_PREDICTION_DICTIONARY_DECODER_H_
#define MOZC_PREDICTION_DICTIONARY_DECODER_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "base/util.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_token.h"
#include "engine/modules.h"
#include "prediction/result.h"
#include "request/conversion_request.h"

namespace mozc::prediction {

// Decoder specialized for core Unigram, Bigram, and Prefix dictionary
// predictive lookups from system and user dictionaries.
//
// Conversion Types and Concrete Examples:
//  1. Unigram predictive lookup:
//     Searches dictionary words whose reading (key) begins with the input key.
//     - Desktop mode: Triggers on key length >= 3 for suggestions (or >= 1 for
//       explicit predictions).
//       e.g. Input key: "ぐーぐるあ" -> "グーグルアドセンス" (reading:
//       "ぐーぐるあどせんす")
//     - Mobile mode (mixed_conversion): Triggers on key length >= 1 and applies
//       redundancy filtering (RemoveRedundantResults) for compact mobile UI.
//       e.g. Input key: "あ" -> "ありがとう", "明日" (reading: "あした"),
//       "アメリカ" (reading: "あめりか")
//  2. Bigram predictive lookup:
//     Generates predictive candidates following the preceding committed history
//     word.
//     - e.g. History: "グーグル" (reading: "ぐーぐる"), Input key: "あ"
//       -> Candidate: "アドセンス" (forming the bigram "グーグルアドセンス")
//     - e.g. History: "お" (prefix), Input key: "は"
//       -> Candidate: "はよう" (forming "おはよう")
//  3. Prefix candidates (partial key consumption):
//     Generates candidate by consuming a prefix of the input key, leaving the
//     remainder.
//     - e.g. Input key: "ぐーぐるあ" -> Candidate: "グーグル" (consumes
//     "ぐーぐる", leaving "あ")
//     - e.g. Input key: "とうきょうに" -> Candidate: "東京" (consumes
//     "とうきょう", leaving "に")
class DictionaryDecoder {
 public:
  explicit DictionaryDecoder(const engine::Modules& modules);

  // Aggregates unigram predictions for the given request.
  void AggregateUnigram(const ConversionRequest& request,
                        std::vector<Result>* results,
                        int* min_unigram_key_len) const;

  // Aggregates bigram predictions for the given request.
  void AggregateBigram(const ConversionRequest& request,
                       std::vector<Result>* results) const;

  // Aggregates prefix predictions for the given request.
  void AggregatePrefix(const ConversionRequest& request,
                       std::vector<Result>* results) const;

  // Filters out irrelevant bigram candidates by setting `result->removed =
  // true`.
  //
  // Bigram prediction looks up words following committed history words.
  // However, naive lookup can propose unnatural word fragments (e.g. suggesting
  // "リカ" after committed "アメ", or "ターネット" after committed "イン").
  // This method verifies script types, prefix costs, and dictionary presence:
  //   - Suppresses word fragments where whole word is more frequent (e.g.
  //   "アメ" -> "リカ").
  //   - Suppresses short boundaries sharing the same script type (Hiragana <=
  //   9, Katakana <= 5).
  //   - Allows valid compounds like Kanji + Katakana (e.g. "六本木" ->
  //   "ヒルズ")
  //     and long Kanji compounds (e.g. "京都大学" -> "霊長類研究所").
  void CheckBigramResult(const dictionary::Token& history_token,
                         Util::ScriptType history_ctype,
                         Util::ScriptType last_history_ctype,
                         const ConversionRequest& request,
                         Result* result) const;

  // Aggregates unigram predictions for standard (non-mixed-conversion) mode.
  //
  // Suggestions (key length >= 3) and explicit Tab-driven predictions (key
  // length >= 1) look up unigram dictionary entries directly up to the cutoff
  // threshold (adjuster.cutoff_threshold()).
  //
  // Example:
  //   - Suggestion (key = "とうきょう", len >= 3):
  //     Looks up dictionary unigrams up to cutoff_threshold -> "東京"
  //   - Tab prediction (key = "と", Tab pressed, len >= 1):
  //     Looks up predictive candidates -> "東京", "友達", "登録", etc.
  void AggregateUnigramForDesktop(const ConversionRequest& request,
                                  std::vector<Result>* results) const;

  // Aggregates unigram predictions for mixed conversion mode (and partial
  // suggestions/predictions).
  //
  // In mixed conversion mode, unigram lookups trigger from the first character
  // (key length >= 1). It fetches candidates up to `kPredictionMaxResultsSize`
  // and applies `filter::RemoveRedundantResults` to eliminate redundant
  // candidates.
  //
  // Example:
  //   - Mixed conversion (key = "あ", len >= 1):
  //     Fetches raw unigrams ("ありがとう", "明日", "アメリカ", "アイス",
  //     etc.), removes redundant/duplicate results, and populates results.
  void AggregateUnigramForMixedConversion(const ConversionRequest& request,
                                          std::vector<Result>* results) const;

 private:
  const engine::Modules& modules_;
  const dictionary::DictionaryInterface& dictionary_;
  uint16_t kanji_number_id_ = 0;
  uint16_t zip_code_id_ = 0;
  uint16_t unknown_id_ = 0;
};

}  // namespace mozc::prediction

#endif  // MOZC_PREDICTION_DICTIONARY_DECODER_H_
