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

#ifndef MOZC_PREDICTION_RESULT_H_
#define MOZC_PREDICTION_RESULT_H_

#include <cstddef>
#include <cstdint>
#include <string>

#include "absl/base/no_destructor.h"
#include "absl/base/nullability.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "composer/query.h"
#include "converter/attribute.h"
#include "converter/inner_segment.h"
#include "dictionary/dictionary_token.h"

namespace mozc {
namespace prediction {

// TODO(b/493100712): This enum is a temporary compatibility shim to keep
// existing client code compiling after the unification of PredictionType
// and converter::Attribute.
//
// Future Plan: Once all clients in both open-source Mozc and Google-internal
// modules have been migrated to use `::mozc::converter::Attribute` directly,
// this enum and its aliases should be completely deleted. Do not add new values
// here; define them in `converter/attribute.h` instead.
enum [[deprecated(
    "Use ::mozc::converter::Attribute instead.")]] PredictionType : uint32_t {
  NO_PREDICTION [[deprecated(
      "Use ::mozc::converter::Attribute::DEFAULT_ATTRIBUTE instead.")]] =
      ::mozc::converter::Attribute::DEFAULT_ATTRIBUTE,
  UNIGRAM [[deprecated("Use ::mozc::converter::Attribute::UNIGRAM instead.")]] =
      ::mozc::converter::Attribute::UNIGRAM,
  BIGRAM [[deprecated("Use ::mozc::converter::Attribute::BIGRAM instead.")]] =
      ::mozc::converter::Attribute::BIGRAM,
  REALTIME [[deprecated(
      "Use ::mozc::converter::Attribute::REALTIME_CONVERSION instead.")]] =
      ::mozc::converter::Attribute::REALTIME_CONVERSION,
  SUFFIX [[deprecated(
      "Use ::mozc::converter::Attribute::SUFFIX_DICTIONARY instead.")]] =
      ::mozc::converter::Attribute::SUFFIX_DICTIONARY,
  ENGLISH [[deprecated("Use ::mozc::converter::Attribute::ENGLISH instead.")]] =
      ::mozc::converter::Attribute::ENGLISH,
  TYPING_CORRECTION [[deprecated(
      "Use ::mozc::converter::Attribute::TYPING_CORRECTION instead.")]] =
      ::mozc::converter::Attribute::TYPING_CORRECTION,
  PREFIX [[deprecated(
      "Use ::mozc::converter::Attribute::PARTIALLY_KEY_CONSUMED instead.")]] =
      ::mozc::converter::Attribute::PARTIALLY_KEY_CONSUMED,
  NUMBER [[deprecated("Use ::mozc::converter::Attribute::NUMBER instead.")]] =
      ::mozc::converter::Attribute::NUMBER,
  SINGLE_KANJI
  [[deprecated("Use ::mozc::converter::Attribute::SINGLE_KANJI instead.")]] =
      ::mozc::converter::Attribute::SINGLE_KANJI,
  TYPING_COMPLETION [[deprecated(
      "Use ::mozc::converter::Attribute::TYPING_COMPLETION instead.")]] =
      ::mozc::converter::Attribute::TYPING_COMPLETION,
  POST_CORRECTION
  [[deprecated("Use ::mozc::converter::Attribute::POST_CORRECTION instead.")]] =
      ::mozc::converter::Attribute::POST_CORRECTION,
  SUPPLEMENTAL_MODEL [[deprecated(
      "Use ::mozc::converter::Attribute::SUPPLEMENTAL_MODEL instead.")]] =
      ::mozc::converter::Attribute::SUPPLEMENTAL_MODEL,
  WEAK_USER_HISTORY_PREDICTION
  [[deprecated("Use ::mozc::converter::Attribute::WEAK_USER_HISTORY_PREDICTION "
               "instead.")]] =
      ::mozc::converter::Attribute::WEAK_USER_HISTORY_PREDICTION,
  REALTIME_TOP
  [[deprecated("Use ::mozc::converter::Attribute::REALTIME_TOP instead.")]] =
      ::mozc::converter::Attribute::REALTIME_TOP,
  KEY_EXPANDED_IN_DICTIONARY
  [[deprecated("Use ::mozc::converter::Attribute::KEY_EXPANDED_IN_DICTIONARY "
               "instead.")]] =
      ::mozc::converter::Attribute::KEY_EXPANDED_IN_DICTIONARY,
  DISABLE_RESCORING [[deprecated(
      "Use ::mozc::converter::Attribute::DISABLE_RESCORING instead.")]] =
      ::mozc::converter::Attribute::DISABLE_RESCORING,
};
// A bitmask of all prediction-specific types. Used exclusively in unit tests
// to verify the exact expected prediction types while ignoring other behavioral
// attributes to keep test assertions consistent and robust.
constexpr uint32_t kPredictionTypesMaskForTesting =
    PredictionType::UNIGRAM | PredictionType::BIGRAM |
    PredictionType::REALTIME | PredictionType::SUFFIX |
    PredictionType::ENGLISH | PredictionType::TYPING_CORRECTION |
    PredictionType::PREFIX | PredictionType::NUMBER |
    PredictionType::SINGLE_KANJI | PredictionType::TYPING_COMPLETION |
    PredictionType::POST_CORRECTION | PredictionType::SUPPLEMENTAL_MODEL |
    PredictionType::WEAK_USER_HISTORY_PREDICTION |
    PredictionType::REALTIME_TOP | PredictionType::KEY_EXPANDED_IN_DICTIONARY |
    PredictionType::DISABLE_RESCORING;
// Bitfield to store a set of PredictionType.
using PredictionTypes = uint32_t;

struct Result {
  void InitializeByTokenAndTypes(const dictionary::Token& token,
                                 PredictionTypes types);
  void SetTypesAndTokenAttributes(
      PredictionTypes prediction_types,
      dictionary::Token::AttributesBitfield token_attr);

  // Returns only the behavioral/semantic attributes (e.g. USER_DICTIONARY,
  // SPELLING_CORRECTION) by masking out the prediction-specific flags.
  // This corresponds to the legacy 'candidate_attributes' field.
  // TODO(b/493100712): This function is a temporary helper to maintain the
  // historical sorting priority key in TiebreakLess. We should eventually
  // simplify the priority logic and remove this function.
  uint32_t GetBehavioralAttributes() const { return attributes & 0xFFFFF; }

  // Returns only the prediction-specific types (e.g. UNIGRAM, BIGRAM) by
  // masking out the behavioral/semantic attributes.
  // This corresponds to the legacy 'types' field and is used to keep test
  // assertions consistent.
  uint32_t GetPredictionTypesForTesting() const {
    return attributes & kPredictionTypesMaskForTesting;
  }

  inline static const Result& DefaultResult() {
    static const absl::NoDestructor<Result> kResult;
    return *kResult;
  }

  std::string key;
  std::string value;
  std::string description;
  std::string display_value;
  // Boundary information for realtime conversion.
  // This will be set only for realtime conversion result candidates.
  // This contains inner segment size for key and value.
  // If the candidate key and value are
  // "わたしの|なまえは|なかのです", " 私の|名前は|中野です",
  // |inner_segment_boundary| have [(4,2), (4, 3), (5, 4)].
  converter::InnerSegmentBoundary inner_segment_boundary;

  // Unified attributes field.
  uint32_t attributes = ::mozc::converter::Attribute::DEFAULT_ATTRIBUTE;
  // Context "insensitive" candidate cost.
  int wcost = 0;
  // Context "sensitive" candidate cost.
  // TODO(noriyukit): The cost is basically calculated by the underlying LM, but
  // currently it is updated by other modules and heuristics at many locations;
  // e.g., see SetPredictionCostForMixedConversion() in
  // dictionary_predictgor.cc. Ideally, such cost adjustments should be kept
  // separately from the original LM cost to perform rescoring in a rigorous
  // manner.
  int cost = 0;
  uint32_t consumed_key_size = 0;
  // The total penalty added to this result.
  int penalty = 0;
  // The original cost before rescoring. Used for debugging purpose.
  int cost_before_rescoring = 0;
  // Confidence score of typing correction. Larger is more confident.
  float typing_correction_score = 0.0;
  // Adjustment for `wcost` made by the typing correction. This value can be
  // zero, positive (penalty) or negative (bonus), and it is added to `wcost`.
  int typing_correction_adjustment = 0;
  // The probability of post correction. When zero, this candidate is not
  // handed by the post-correction component.
  float post_correction_prob = 0.0;

  uint16_t lid = 0;
  uint16_t rid = 0;
  // If removed is true, this result is not used for a candidate.
  bool removed = false;
#ifndef NDEBUG
  std::string log;
#endif  // NDEBUG

  converter::InnerSegments inner_segments() const {
    return converter::InnerSegments(key, value, inner_segment_boundary);
  }

  // Used to emulate positive infinity for cost. This value is set for those
  // candidates that are thought to be aggressive; thus we can eliminate such
  // candidates from suggestion or prediction. Note that for this purpose we
  // don't want to use INT_MAX because someone might further add penalty after
  // cost is set to INT_MAX, which leads to overflow and consequently aggressive
  // candidates would appear in the top results.
  inline static constexpr int kInvalidCost = (2 << 20);

  template <typename S>
  friend void AbslStringify(S& sink, const Result& r) {
    absl::Format(
        &sink,
        "key: %s, value: %s, attrs: %d, wcost: %d, cost: %d, cost_before: %d, "
        "lid: %d, rid: %d, bdd: %s, consumed_key_size: %d, penalty: "
        "%d, tc_adjustment: %d, removed: %v, post_correction_prob: %f, "
        "typing_correction_score: %f",
        r.key, r.value, r.attributes, r.wcost, r.cost, r.cost_before_rescoring,
        r.lid, r.rid, absl::StrJoin(r.inner_segment_boundary, ","),
        r.consumed_key_size, r.penalty, r.typing_correction_adjustment,
        r.removed, r.post_correction_prob, r.typing_correction_score);
#ifndef NDEBUG
    sink.Append(", log:\n");
    for (absl::string_view line : absl::StrSplit(r.log, '\n')) {
      absl::Format(&sink, "    %s\n", line);
    }
#endif  // NDEBUG
  }
};

namespace result_internal {

// ValueLess returns if lhs is less than rhs by comparing the two strings by
// the number of Unicode characters and then value.
// Examples,
//  "ん" < "あいうえお"
//  "あいうえお" < "かきくけこ"
//  "テスト1" < "テスト00"
bool ValueLess(absl::string_view lhs, absl::string_view rhs);

// TiebreakLess returns if lhs is less than rhs by comparing the two results.
// This is used for tie breaking when cost, wcost and value are the same.
// "less than" here means "has higher priority (= lower cost)".
bool TiebreakLess(const Result& lhs, const Result& rhs);

}  // namespace result_internal

// Comparator for sorting prediction candidates.
// If we have words A and AB, for example "六本木" and "六本木ヒルズ",
// assume that cost(A) < cost(AB).
struct ResultWCostLess {
  bool operator()(const Result& lhs, const Result& rhs) const {
    if (lhs.wcost != rhs.wcost) {
      return lhs.wcost < rhs.wcost;
    }
    if (lhs.value != rhs.value) {
      return result_internal::ValueLess(lhs.value, rhs.value);
    }
    return result_internal::TiebreakLess(lhs, rhs);
  }
};

// Returns true if `lhs` is less than `rhs`
struct ResultCostLess {
  bool operator()(const Result& lhs, const Result& rhs) const {
    if (lhs.cost != rhs.cost) {
      return lhs.cost < rhs.cost;
    }
    if (lhs.value != rhs.value) {
      return result_internal::ValueLess(lhs.value, rhs.value);
    }
    return result_internal::TiebreakLess(lhs, rhs);
  }
};

// Populates the typing correction result in `query` to prediction::Result
// TODO(taku): rename `query` as it is not a query.
void PopulateTypeCorrectedQuery(
    const composer::TypeCorrectedQuery& typing_corrected_result,
    Result* absl_nonnull result);

// Makes debug string from `types`.
std::string GetPredictionTypeDebugString(PredictionTypes types);

#ifndef NDEBUG
#define MOZC_WORD_LOG(result, ...)                                  \
  {                                                                 \
    if (!(result).log.empty()) absl::StrAppend(&(result).log, " "); \
    absl::StrAppend(&(result).log, __FILE__, ":", __LINE__, " ",    \
                    ##__VA_ARGS__);                                 \
  }
#else  // NDEBUG
#define MOZC_WORD_LOG(result, ...) \
  {                                \
  }
#endif  // NDEBUG

}  // namespace prediction
}  // namespace mozc

#endif  // MOZC_PREDICTION_RESULT_H_
