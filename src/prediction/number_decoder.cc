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

#include "prediction/number_decoder.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "base/container/trie.h"
#include "base/util.h"
#include "converter/attribute.h"
#include "dictionary/pos_matcher.h"
#include "prediction/result.h"
#include "request/conversion_request.h"

namespace mozc::prediction {

using ::mozc::converter::Attribute;
namespace {

using ::mozc::prediction::number_decoder_internal::Entry;
using ::mozc::prediction::number_decoder_internal::State;
using ::mozc::prediction::number_decoder_internal::Type;

// Returns true if |key| is a geminated (促音) number token that cannot stand
// alone at the end of a bare number (e.g., "いっ", "ろっ", "はっ", "じゅっ",
// "ひゃっ", "おっ").
bool IsSokuonKey(absl::string_view key) {
  return key == "いっ" || key == "ろっ" || key == "はっ" || key == "じゅっ" ||
         key == "じっ" || key == "ひゃっ" || key == "びゃっ" ||
         key == "ぴゃっ" || key == "おっ";
}

// TailType, kCounterSuffixRules, kWagoNumberSuffixRules, kWagoPrefixes,
// kWagoPrefixSuffixRules, and kStopSuffixEntries are defined in
// number_decoder_data.inc.
#include "prediction/number_decoder_data.inc"

// Maps the last consumed number token to its TailType bit used by
// kCounterSuffixRules.
TailType GetTailType(absl::string_view key) {
  if (key == "ぜろ") return kZero;
  if (key == "いち") return kIchi;
  if (key == "いっ") return kIt;
  if (key == "に") return kNi;
  if (key == "さん") return kSan;
  if (key == "よん") return kYon;
  if (key == "し") return kShi;
  if (key == "よ") return kYo;
  if (key == "ご") return kGo;
  if (key == "ろく") return kRoku;
  if (key == "ろっ") return kRok;
  if (key == "なな") return kNana;
  if (key == "しち") return kShichi;
  if (key == "はち") return kHachi;
  if (key == "はっ") return kHat;
  if (key == "きゅう" || key == "きゅー") return kKyu;
  if (key == "く") return kKu;
  if (key == "じゅう" || key == "じゅー") return kJu;
  if (key == "じゅっ" || key == "じっ") return kJut;
  if (key == "ひゃく" || key == "びゃく" || key == "ぴゃく") return kHyaku;
  if (key == "ひゃっ" || key == "びゃっ" || key == "ぴゃっ") return kHyak;
  if (key == "せん" || key == "ぜん") return kSen;
  if (key == "まん") return kMan;
  if (key == "おく") return kOku;
  if (key == "おっ") return kOk;
  if (key == "ちょう" || key == "けい" || key == "がい") return kCho;
  return kTailNone;
}

// Returns true if the boundary between the just-matched number |token| and the
// remaining unconsumed suffix |rest| is phonologically valid.
//
// Parameters:
// - |token|: The number token matched by the Trie (e.g., "に", "し", "よ",
//   "ご", "じゅう").
// - |rest|: The remaining unconsumed portion of the input key immediately
//   following |token|.
//
// Examples of rejected (false) boundaries:
// - |rest| starts with a small kana or prolonged sound mark ("ゃ", "ゅ", "ょ",
//   "ぁ", "ぃ", "ぅ", "ぇ", "ぉ", "ゎ", "ー"):
//   - "にゃんこ" -> |token|="に", |rest|="ゃんこ" (prevents splitting "にゃ")
//   - "じゅうしょ" -> |token|="じゅう"+"し", |rest|="ょ" (prevents "14"+"ょ")
// - |rest| starts with "っ" (except "よ" + "っか" for 4日/14日/24日):
//   - "にっき" -> |token|="に", |rest|="っき" (prevents "2"+"っき")
//   - "よっか" -> |token|="よ", |rest|="っか" (allowed for "4日")
// - |token| is "ぜろ", "よ", or "ご" (o-row vowel) and |rest| starts with "う":
//   - "ごうかく" -> |token|="ご", |rest|="うかく" (prevents splitting long
//     vowel "ごう" into "5"+"うかく")
//   - "ようかい" -> |token|="よ", |rest|="うかい" (prevents splitting "よう")
bool IsValidTokenBoundary(absl::string_view token, absl::string_view rest) {
  if (rest.empty()) {
    return true;
  }
  static constexpr absl::string_view kInvalidStartKana[] = {
      "ゃ", "ゅ", "ょ", "ぁ", "ぃ", "ぅ", "ぇ", "ぉ", "ゎ", "ー",
  };
  for (absl::string_view kana : kInvalidStartKana) {
    if (absl::StartsWith(rest, kana)) {
      return false;
    }
  }
  if (absl::StartsWith(rest, "っ")) {
    // Allow "よっか" / "よっかかん" (4日 / 4日間, 14日, 24日).
    if (!(token == "よ" && absl::StartsWith(rest, "っか"))) {
      return false;
    }
  }
  if (absl::StartsWith(rest, "う")) {
    // Prevent splitting long vowels like "ごうかく" (ご+うかく) or "ようかい"
    // (よ+うかい).
    if (token == "ぜろ" || token == "よ" || token == "ご") {
      return false;
    }
  }
  return true;
}

// Returns true if |key| begins with a small-digit multiplier reading (10, 100,
// or 1000). Used when a second unit digit is encountered (e.g., "にさんじゅう"
// [2 + 30]) to check whether the first unit digit ("に") ends at a valid
// number-phrase boundary.
bool StartsWithSmallDigitKey(absl::string_view key) {
  static constexpr absl::string_view kSmallDigitKeys[] = {
      "じゅう", "じゅー", "じゅっ", "じっ",   "ひゃく", "ひゃっ",
      "びゃく", "びゃっ", "ぴゃく", "ぴゃっ", "せん",   "ぜん",
  };
  for (absl::string_view small_digit_key : kSmallDigitKeys) {
    if (absl::StartsWith(key, small_digit_key)) {
      return true;
    }
  }
  return false;
}

// Appends `<number><counter>` candidates (e.g., "1枚", "300杯", "24日") to
// |results| when the unconsumed suffix of `state.key` exactly matches a
// counter suffix reading in `kCounterSuffixRules` that is compatible with the
// last consumed number token in `state.consumed_keys`.
void AppendCounterSuffixResults(const State& state,
                                std::vector<NumberDecoderResult>& results) {
  if (!state.IsValid() || state.consumed_keys.empty()) {
    return;
  }
  for (size_t i = 0; i + 1 < state.consumed_keys.size(); ++i) {
    const absl::string_view key = state.consumed_keys[i];
    if (key == "よ" || key == "く") {
      return;
    }
    if (key == "し") {
      const absl::string_view next_key = state.consumed_keys[i + 1];
      if (next_key != "じゅう" && next_key != "じゅー" &&
          next_key != "じゅっ" && next_key != "じっ") {
        return;
      }
    }
  }
  const std::optional<NumberDecoderResult> num_result = state.Result();
  if (!num_result.has_value()) {
    return;
  }
  const TailType tail_type = GetTailType(state.consumed_keys.back());
  if (tail_type == kTailNone) {
    return;
  }
  const bool is_multi_digit = state.consumed_keys.size() > 1;
  // Standalone "じっ" + counter collides with many common 実〜 words (e.g.,
  // "じっけん" [実験] vs. 10件, "じっさい" [実際] vs. 10歳, "じっこう" [実行]
  // vs. 10校). "じゅっ" covers the same readings, so require a preceding digit
  // for "じっ" (e.g., "にじっけん" -> "20件").
  if (!is_multi_digit && state.consumed_keys.back() == "じっ") {
    return;
  }
  const absl::string_view rem = state.key.substr(state.consumed_key_byte_len);
  if (rem.empty()) {
    return;
  }
  for (const CounterSuffixRule& rule : kCounterSuffixRules) {
    if (rem != rule.reading) {
      continue;
    }
    const bool allowed =
        (tail_type & rule.allowed_tail_mask) != 0 &&
        (is_multi_digit || (tail_type & rule.multi_digit_only_tail_mask) == 0);
    if (!allowed) {
      continue;
    }
    for (const absl::string_view suffix : absl::StrSplit(rule.suffixes, ',')) {
      results.emplace_back(state.consumed_key_byte_len + rule.reading.size(),
                           absl::StrCat(num_result->candidate, suffix),
                           num_result->digit_num, /*is_counter_suffix=*/true);
    }
  }
}

// Appends standalone Wago numeral candidates (e.g., "ひとり" -> "1人",
// "ふつか" -> "2日", "ひとつ" -> "1つ") and Wago prefix + counter candidates
// (e.g., "ひとはこ" -> "1箱", "みへや" -> "3部屋") when |key| matches the
// entire input.
void DecodeWagoNumberSuffixes(absl::string_view key,
                              std::vector<NumberDecoderResult>& results) {
  if (key.empty()) {
    return;
  }
  for (const WagoNumberSuffixRule& rule : kWagoNumberSuffixRules) {
    if (key != rule.reading) {
      continue;
    }
    results.emplace_back(rule.reading.size(), std::string(rule.candidate),
                         rule.digit_num, /*is_counter_suffix=*/true);
  }

  for (const WagoPrefixEntry& prefix : kWagoPrefixes) {
    if (!absl::StartsWith(key, prefix.reading)) {
      continue;
    }
    const absl::string_view rem = key.substr(prefix.reading.size());
    for (const WagoPrefixSuffixRule& rule : kWagoPrefixSuffixRules) {
      if (rem != rule.reading ||
          (prefix.flag & rule.allowed_prefix_mask) == 0) {
        continue;
      }
      for (const absl::string_view suffix :
           absl::StrSplit(rule.suffixes, ',')) {
        results.emplace_back(key.size(), absl::StrCat(prefix.number, suffix),
                             /*digit_num=*/1, /*is_counter_suffix=*/true);
      }
    }
  }
}

// Appends the bare number result of |state| to |results| unless it is an
// invalid or suppressed sequence.
// - |allow_sokuon_tail|: accept a geminated last token, which is otherwise
//   invalid at the end of a number (e.g., "いっ" of "いっちょう" when emitting
//   "1" before decoding "ちょう").
// - |allow_single_key_partial|: accept a single-token partial-key result
//   (e.g., "に" of "にじゅう" when emitting "2" before decoding "じゅう").
void MaybeAppendResult(const State& state,
                       std::vector<NumberDecoderResult>& results,
                       bool allow_sokuon_tail, bool allow_single_key_partial) {
  std::optional<NumberDecoderResult> result = state.Result();
  if (!result.has_value()) {
    return;
  }
  if (!allow_sokuon_tail && !state.consumed_keys.empty() &&
      IsSokuonKey(state.consumed_keys.back())) {
    return;
  }
  if (state.consumed_key_byte_len < state.key.size() &&
      state.consumed_keys.size() == 1 && !allow_single_key_partial &&
      !state.stopped_at_explicit_boundary) {
    return;
  }

  // Filter out invalid number sequences.
  std::vector<absl::string_view> keys = state.consumed_keys;
  if (state.consumed_key_byte_len < state.key.size()) {
    const absl::string_view rem = state.key.substr(state.consumed_key_byte_len);
    if (!state.consumed_keys.empty() &&
        (state.consumed_keys.back() == "じゅう" ||
         state.consumed_keys.back() == "じゅー") &&
        absl::StartsWith(rem, "かい")) {
      // "10回" / "10階" requires sokuon ("じゅっかい" / "じっかい"), not
      // "じゅうかい" (which conflicts with "重解", "十戒", etc.).
      return;
    }
    keys.emplace_back(rem);
  }
  for (int i = 0; i < keys.size(); ++i) {
    const absl::string_view key = keys[i];
    if (key == "よ") {
      // "よ" is only valid before specific counter suffixes (e.g., よにん,
      // よじ).
      return;
    }
    if (key == "く" && (i > 0 || i + 1 < keys.size())) {
      // "く" (9) is valid only as a standalone single-key number. In
      // particular, a trailing "く" is rejected ("じゅうく" -> *19,
      // "にじゅうく" -> *29) because it collides with 〜重苦 (二重苦,
      // 三重苦); use "きゅう" for 19, 29, etc. "く" before a counter suffix
      // (e.g., "じゅうくにち" -> "19日") is handled by
      // AppendCounterSuffixResults.
      return;
    }
    if (key == "し" && !(i + 1 == keys.size() || keys[i + 1] == "じゅう" ||
                         keys[i + 1] == "じゅー" || keys[i + 1] == "じゅっ" ||
                         keys[i + 1] == "じっ")) {
      // Valid: "し:4", "じゅうし:4", "しじゅう:4"
      // Invalid: "しひゃく", "しせん", etc
      return;
    }
  }
  results.push_back(*std::move(result));
}

std::unique_ptr<const Trie<Entry>> CreateDefaultEntries() {
  auto result = std::make_unique<Trie<Entry>>();
  // unit
  result->AddEntry("ぜろ", Entry({Type::UNIT, 0}));
  result->AddEntry("いち", Entry({Type::UNIT, 1}));
  result->AddEntry("いっ", Entry({Type::UNIT, 1}));
  result->AddEntry("に", Entry({Type::UNIT, 2}));
  result->AddEntry("さん", Entry({Type::UNIT, 3}));
  result->AddEntry("し", Entry({Type::UNIT, 4}));
  result->AddEntry("よん", Entry({Type::UNIT, 4}));
  result->AddEntry("よ", Entry({Type::UNIT, 4}));
  result->AddEntry("ご", Entry({Type::UNIT, 5}));
  result->AddEntry("ろく", Entry({Type::UNIT, 6}));
  result->AddEntry("ろっ", Entry({Type::UNIT, 6}));
  result->AddEntry("なな", Entry({Type::UNIT, 7}));
  result->AddEntry("しち", Entry({Type::UNIT, 7}));
  result->AddEntry("はち", Entry({Type::UNIT, 8}));
  result->AddEntry("はっ", Entry({Type::UNIT, 8}));
  result->AddEntry("きゅう", Entry({Type::UNIT, 9}));
  result->AddEntry("きゅー", Entry({Type::UNIT, 9}));
  result->AddEntry("く", Entry({Type::UNIT, 9}));

  // small digit
  // "重", etc
  result->AddEntry("じゅう", Entry({Type::SMALL_DIGIT, 10, 2, "", true}));
  result->AddEntry("じゅー", Entry({Type::SMALL_DIGIT, 10, 2, "", true}));
  result->AddEntry("じゅっ", Entry({Type::SMALL_DIGIT, 10, 2}));
  result->AddEntry("じっ", Entry({Type::SMALL_DIGIT, 10, 2}));
  result->AddEntry("ひゃく", Entry({Type::SMALL_DIGIT, 100, 3}));
  result->AddEntry("ひゃっ", Entry({Type::SMALL_DIGIT, 100, 3}));
  result->AddEntry("びゃく", Entry({Type::SMALL_DIGIT, 100, 3}));
  result->AddEntry("びゃっ", Entry({Type::SMALL_DIGIT, 100, 3}));
  result->AddEntry("ぴゃく", Entry({Type::SMALL_DIGIT, 100, 3}));
  result->AddEntry("ぴゃっ", Entry({Type::SMALL_DIGIT, 100, 3}));
  // "戦", etc
  result->AddEntry("せん", Entry({Type::SMALL_DIGIT, 1000, 4, "", true}));
  // "膳"
  result->AddEntry("ぜん", Entry({Type::SMALL_DIGIT, 1000, 4, "", true}));

  // big digit
  result->AddEntry("まん", Entry({Type::BIG_DIGIT, 10000, 5, "万"}));
  result->AddEntry("おく", Entry({Type::BIG_DIGIT, -1, 9, "億"}));
  result->AddEntry("おっ", Entry({Type::BIG_DIGIT, -1, 9, "億"}));
  // "町", etc
  result->AddEntry("ちょう", Entry({Type::BIG_DIGIT, -1, 13, "兆", true}));
  // "系", etc
  result->AddEntry("けい", Entry({Type::BIG_DIGIT, -1, 17, "京", true}));
  result->AddEntry("がい", Entry({Type::BIG_DIGIT, -1, 21, "垓"}));

  // spacial cases
  // conflict with "にち"
  result->AddEntry("にちょう",
                   Entry({Type::UNIT_AND_BIG_DIGIT, 2, 13, "兆", true, 3}));
  result->AddEntry("にちょうめ",
                   Entry({Type::UNIT_AND_STOP_DECODING, 2, -1, "", false, 3}));
  result->AddEntry("にちゃん",
                   Entry({Type::UNIT_AND_STOP_DECODING, 2, -1, "", false, 3}));
  // サンチーム (currency) v.s. 3チーム
  result->AddEntry("さんちーむ",
                   Entry({Type::UNIT_AND_STOP_DECODING, 3, -1, "", true, 6}));

  // Counter suffixes conflicting with the number tokens above.
  for (const StopSuffixEntry& suffix : kStopSuffixEntries) {
    Entry entry;
    entry.emits_bare_number_prefix = suffix.emits_bare_number_prefix;
    result->AddEntry(suffix.reading, entry);
  }
  return result;
}

const Trie<Entry>& InitEntries() {
  // Returns a singleton enries.
  static const absl::NoDestructor<std::unique_ptr<const Trie<Entry>>>
      kDefaultEntries(CreateDefaultEntries());
  return **kDefaultEntries;
}

}  // namespace

std::ostream& operator<<(std::ostream& os, const NumberDecoderResult& r) {
  os << absl::StreamFormat("%v", r);
  return os;
}

NumberDecoder::NumberDecoder(const dictionary::PosMatcher& pos_matcher)
    : entries_(InitEntries()),
      kanji_number_id_(pos_matcher.GetKanjiNumberId()),
      number_id_(pos_matcher.GetNumberId()),
      counter_suffix_word_id_(pos_matcher.GetCounterSuffixWordId()) {}

std::vector<prediction::Result> NumberDecoder::Decode(
    const ConversionRequest& request) const {
  std::vector<prediction::Result> results;
  absl::string_view request_key = request.key();

  // Counter suffix candidates for the same key share the same base cost, so
  // without a tiebreaker they would be ordered by code point of the value
  // (e.g., "5把" < "5羽" < "5話"). Add a small penalty in emission order so
  // that the order of kCounterSuffixRules (and of the comma-separated
  // suffixes within a rule) determines the ranking instead.
  constexpr int kCounterSuffixRankCost = 100;
  int counter_suffix_rank = 0;

  for (const auto& decode_result : Decode(request_key)) {
    // When the key is already a typing-corrected guess, do not stack a counter
    // suffix guess on top of it; only bare numbers are emitted for such keys.
    if (decode_result.is_counter_suffix &&
        request.options().use_already_typing_corrected_key) {
      continue;
    }
    Result result;
    // Note: counter suffix candidates (e.g., "45回") are mixed script, so
    // |is_arabic| is false and they get kanji_number_id_ as lid, the same as
    // "1万".
    const bool is_arabic =
        Util::GetScriptType(decode_result.candidate) == Util::NUMBER;
    result.key = request_key.substr(0, decode_result.consumed_key_byte_len);
    result.value = std::move(decode_result.candidate);
    result.attributes |= Attribute::NUMBER;
    result.attributes |= Attribute::NO_SUGGEST_LEARNING;
    // Heuristic cost:
    // Large digit number (1億, 1兆, etc) should have larger cost
    // 1000 ~= 500 * log(10)
    // Counter suffix candidates get an extra penalty so that they rank below
    // the bare number and dictionary candidates.
    constexpr int kCounterSuffixCostPenalty = 2500;
    result.wcost = 1000 * (1 + decode_result.digit_num);
    if (decode_result.is_counter_suffix) {
      result.wcost += kCounterSuffixCostPenalty +
                      kCounterSuffixRankCost * counter_suffix_rank++;
    }
    result.lid = is_arabic ? number_id_ : kanji_number_id_;
    result.rid = decode_result.is_counter_suffix
                     ? counter_suffix_word_id_
                     : (is_arabic ? number_id_ : kanji_number_id_);
    if (decode_result.consumed_key_byte_len < request_key.size()) {
      result.attributes |= Attribute::PARTIALLY_KEY_CONSUMED;
      result.consumed_key_size = Util::CharsLen(result.key);
    }
    results.emplace_back(std::move(result));
  }

  return results;
}

std::vector<NumberDecoderResult> NumberDecoder::Decode(
    absl::string_view key) const {
  State state;
  state.key = key;
  std::vector<NumberDecoderResult> results;
  DecodeAux(key, state, results);

  MaybeAppendResult(state, results, /*allow_sokuon_tail=*/false,
                    /*allow_single_key_partial=*/false);
  AppendCounterSuffixResults(state, results);
  DecodeWagoNumberSuffixes(key, results);
  return results;
}

void NumberDecoder::DecodeAux(absl::string_view key, State& state,
                              std::vector<NumberDecoderResult>& results) const {
  if (key.empty()) {
    return;
  }
  size_t key_byte_len;
  Entry e;
  if (!entries_.LongestMatch(key, &e, &key_byte_len)) {
    return;
  }
  const absl::string_view matched_key = key.substr(0, key_byte_len);
  const absl::string_view rest = key.substr(key_byte_len);
  if (e.type != Type::STOP_DECODING && e.type != Type::UNIT_AND_STOP_DECODING &&
      !IsValidTokenBoundary(matched_key, rest)) {
    return;
  }
  switch (e.type) {
    case Type::STOP_DECODING:
      // Keep the bare number prefix as a partial-key candidate only for the
      // suffixes flagged in kStopSuffixEntries (e.g., "にせんち" -> "2").
      if (rest.empty() && e.emits_bare_number_prefix) {
        state.stopped_at_explicit_boundary = true;
      }
      return;
    case Type::UNIT:
      if (!HandleUnitEntry(matched_key, e, state, results)) {
        return;
      }
      state.consumed_key_byte_len += key_byte_len;
      break;
    case Type::SMALL_DIGIT:
      if (!HandleSmallDigitEntry(matched_key, e, state, results)) {
        return;
      }
      state.consumed_key_byte_len += key_byte_len;
      break;
    case Type::BIG_DIGIT:
      if (!HandleBigDigitEntry(matched_key, e, state, results)) {
        return;
      }
      state.consumed_key_byte_len += key_byte_len;
      break;
    case Type::UNIT_AND_BIG_DIGIT: {
      const absl::string_view unit_key =
          key.substr(0, e.consume_byte_len_of_first);
      if (!HandleUnitEntry(unit_key, e, state, results)) {
        return;
      }
      state.consumed_key_byte_len += e.consume_byte_len_of_first;
      const absl::string_view digit_key =
          key.substr(e.consume_byte_len_of_first,
                     (key_byte_len - e.consume_byte_len_of_first));
      if (!HandleBigDigitEntry(digit_key, e, state, results)) {
        return;
      }
      state.consumed_key_byte_len +=
          (key_byte_len - e.consume_byte_len_of_first);
      break;
    }
    case Type::UNIT_AND_STOP_DECODING: {
      const absl::string_view unit_key =
          key.substr(0, e.consume_byte_len_of_first);
      if (!HandleUnitEntry(unit_key, e, state, results)) {
        return;
      }
      state.consumed_key_byte_len += e.consume_byte_len_of_first;
      state.stopped_at_explicit_boundary = true;
      return;
    }
    default:
      LOG(ERROR) << "Error";
  }
  DCHECK_GT(key_byte_len, 0);
  DecodeAux(key.substr(key_byte_len), state, results);
}

bool NumberDecoder::HandleUnitEntry(
    absl::string_view key, const Entry& entry, State& state,
    std::vector<NumberDecoderResult>& results) const {
  results.clear();
  if (!state.consumed_keys.empty() && IsSokuonKey(state.consumed_keys.back())) {
    return false;
  }
  if ((state.IsValid() && entry.number == 0) || (state.small_digit_num == 0) ||
      (state.small_digit_num != -1 && state.small_digit_num % 10 != 0)) {
    // Unit has been already decoded (e.g. ぜろに, にいち, にぜろ,
    // にさんじゅう).
    const absl::string_view after_key =
        state.key.substr(state.consumed_key_byte_len + key.size());
    if (key != "し" && key != "よ" && key != "く" && !IsSokuonKey(key) &&
        (after_key.empty() || StartsWithSmallDigitKey(after_key))) {
      state.stopped_at_explicit_boundary = true;
    }
    return false;
  }

  if (entry.output_before_decode) {
    MaybeAppendResult(state, results, /*allow_sokuon_tail=*/false,
                      /*allow_single_key_partial=*/true);
  }

  if (state.small_digit_num == -1) {
    state.small_digit_num = entry.number;
  } else {
    DCHECK_EQ(state.small_digit_num % 10, 0);
    state.small_digit_num += entry.number;
  }
  state.consumed_keys.emplace_back(key);
  state.digit_num = std::max(state.digit_num, 1);
  return true;
}

bool NumberDecoder::HandleSmallDigitEntry(
    absl::string_view key, const Entry& entry, State& state,
    std::vector<NumberDecoderResult>& results) const {
  results.clear();
  if (state.small_digit > 1 && entry.digit >= state.small_digit) {
    // Invalid: じゅうせん
    return false;
  }
  if (state.small_digit_num == 0) {
    // Invalid: ぜろじゅう
    return false;
  }
  const absl::string_view prev_key =
      state.consumed_keys.empty() ? "" : state.consumed_keys.back();
  const bool has_unit =
      (state.small_digit_num != -1 && state.small_digit_num % 10 != 0);
  if (!has_unit && IsSokuonKey(prev_key)) {
    return false;
  }
  if (entry.number == 10) {
    if (has_unit && (IsSokuonKey(prev_key) || prev_key == "いち")) {
      return false;
    }
  } else if (entry.number == 100) {
    if (key == "びゃく" || key == "びゃっ") {
      if (!has_unit || prev_key != "さん") {
        return false;
      }
    } else if (key == "ぴゃく" || key == "ぴゃっ") {
      if (!has_unit ||
          (prev_key != "ろっ" && prev_key != "はっ" && prev_key != "いっ")) {
        return false;
      }
    } else {
      // "ひゃく" or "ひゃっ"
      if (has_unit &&
          (prev_key == "さん" || prev_key == "ろく" || prev_key == "ろっ" ||
           prev_key == "はち" || prev_key == "はっ" || prev_key == "いち" ||
           prev_key == "いっ" || prev_key == "しち")) {
        return false;
      }
    }
  } else if (entry.number == 1000) {
    if (key == "ぜん") {
      if (!has_unit || prev_key != "さん") {
        return false;
      }
    } else {
      // "せん"
      if (has_unit &&
          (prev_key == "さん" || prev_key == "いち" || prev_key == "ろっ" ||
           prev_key == "はち" || prev_key == "しち")) {
        return false;
      }
    }
  }

  if (entry.output_before_decode) {
    MaybeAppendResult(state, results, /*allow_sokuon_tail=*/false,
                      /*allow_single_key_partial=*/true);
  }

  if (state.small_digit_num == -1) {
    state.small_digit_num = entry.number;
  } else {
    const int unit = std::max(1, state.small_digit_num % 10);
    const int base = (state.small_digit_num / 10) * 10;
    state.small_digit_num = base + unit * entry.number;
  }
  state.small_digit = entry.digit;
  state.consumed_keys.emplace_back(key);
  state.digit_num = std::max(state.digit_num, entry.digit);
  return true;
}

bool NumberDecoder::HandleBigDigitEntry(
    absl::string_view key, const Entry& entry, State& state,
    std::vector<NumberDecoderResult>& results) const {
  results.clear();
  if (state.big_digit > 0 && entry.digit >= state.big_digit) {
    // Invalid: おくまん
    return false;
  }
  if (state.small_digit_num == -1 || state.small_digit_num == 0) {
    // Do not decode "まん" to "10000"
    return false;
  }
  const absl::string_view prev_key =
      state.consumed_keys.empty() ? "" : state.consumed_keys.back();
  if (prev_key == "しち") {
    return false;
  }
  if (key == "まん" || key == "おく" || key == "おっ" || key == "がい") {
    if (IsSokuonKey(prev_key)) {
      return false;
    }
  } else if (key == "ちょう") {
    if (prev_key == "いち" || prev_key == "はち" || prev_key == "じゅう" ||
        prev_key == "じゅー" || prev_key == "ろっ" || prev_key == "おっ") {
      return false;
    }
  } else if (key == "けい") {
    if (prev_key == "いち" || prev_key == "ろく" || prev_key == "はち" ||
        prev_key == "じゅう" || prev_key == "じゅー" || prev_key == "ひゃく" ||
        prev_key == "びゃく" || prev_key == "ぴゃく" || prev_key == "おっ") {
      return false;
    }
  }

  if (entry.output_before_decode) {
    MaybeAppendResult(state, results, /*allow_sokuon_tail=*/true,
                      /*allow_single_key_partial=*/true);
  }
  // "がい" is about to be consumed as 垓, so emit the 階 counter candidate here
  // (e.g., "さんがい" -> "3階"). When this handler rejects "がい" above, "がい"
  // stays unconsumed and NumberDecoder::Decode() emits the counter candidate
  // instead; calling it here as well would duplicate the result.
  if (key == "がい") {
    AppendCounterSuffixResults(state, results);
  }

  //  state.current_num = -1;
  absl::StrAppend(&state.current_num_str, state.small_digit_num,
                  entry.digit_str);

  state.digit_num = std::max(
      state.digit_num,
      entry.digit +
          static_cast<int>(absl::StrCat(state.small_digit_num).size()) - 1);

  state.small_digit_num = -1;
  state.small_digit = -1;
  state.big_digit = entry.digit;
  state.consumed_keys.emplace_back(key);

  return true;
}

namespace number_decoder_internal {

std::optional<NumberDecoderResult> State::Result() const {
  if (!IsValid()) {
    return std::nullopt;
  }

  const int small_digit = std::max(small_digit_num, 0);

  if (small_digit > 0) {
    // "1万" + "2000"
    return NumberDecoderResult{consumed_key_byte_len,
                               absl::StrCat(current_num_str, small_digit),
                               digit_num};
  } else if (!current_num_str.empty()) {
    // "1万"
    return NumberDecoderResult{consumed_key_byte_len, current_num_str,
                               digit_num};
  } else if (small_digit == 0) {
    // "0"
    return NumberDecoderResult{consumed_key_byte_len, "0", 1};
  }
  return std::nullopt;
}

}  // namespace number_decoder_internal
}  // namespace mozc::prediction
