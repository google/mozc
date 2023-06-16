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
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace {

using ::mozc::number_decoder_internal::Entry;
using ::mozc::number_decoder_internal::State;
using ::mozc::number_decoder_internal::Type;

void MaybeAppendResult(const State &state,
                       std::vector<NumberDecoder::Result> &results) {
  std::optional<NumberDecoder::Result> result = state.Result();
  if (result.has_value()) {
    results.push_back(*std::move(result));
  }
}

Trie<Entry> InitEntries() {
  Trie<Entry> result;
  // unit
  result.AddEntry("ぜろ", Entry({Type::UNIT, 0}));
  result.AddEntry("いち", Entry({Type::UNIT, 1}));
  result.AddEntry("いっ", Entry({Type::UNIT, 1}));
  result.AddEntry("に", Entry({Type::UNIT, 2}));
  result.AddEntry("さん", Entry({Type::UNIT, 3}));
  result.AddEntry("し", Entry({Type::UNIT, 4}));
  result.AddEntry("よん", Entry({Type::UNIT, 4}));
  result.AddEntry("よ", Entry({Type::UNIT, 4}));
  result.AddEntry("ご", Entry({Type::UNIT, 5}));
  result.AddEntry("ろく", Entry({Type::UNIT, 6}));
  result.AddEntry("ろっ", Entry({Type::UNIT, 6}));
  result.AddEntry("なな", Entry({Type::UNIT, 7}));
  result.AddEntry("しち", Entry({Type::UNIT, 7}));
  result.AddEntry("はち", Entry({Type::UNIT, 8}));
  result.AddEntry("はっ", Entry({Type::UNIT, 8}));
  result.AddEntry("きゅう", Entry({Type::UNIT, 9}));
  result.AddEntry("きゅー", Entry({Type::UNIT, 9}));
  result.AddEntry("く", Entry({Type::UNIT, 9}));

  // small digit
  // "重", etc
  result.AddEntry("じゅう", Entry({Type::SMALL_DIGIT, 10, 2, "", true}));
  result.AddEntry("じゅー", Entry({Type::SMALL_DIGIT, 10, 2, "", true}));
  result.AddEntry("じゅっ", Entry({Type::SMALL_DIGIT, 10, 2}));
  result.AddEntry("ひゃく", Entry({Type::SMALL_DIGIT, 100, 3}));
  result.AddEntry("ひゃっ", Entry({Type::SMALL_DIGIT, 100, 3}));
  result.AddEntry("びゃく", Entry({Type::SMALL_DIGIT, 100, 3}));
  result.AddEntry("びゃっ", Entry({Type::SMALL_DIGIT, 100, 3}));
  result.AddEntry("ぴゃく", Entry({Type::SMALL_DIGIT, 100, 3}));
  result.AddEntry("ぴゃっ", Entry({Type::SMALL_DIGIT, 100, 3}));
  // "戦", etc
  result.AddEntry("せん", Entry({Type::SMALL_DIGIT, 1000, 4, "", true}));
  // "膳"
  result.AddEntry("ぜん", Entry({Type::SMALL_DIGIT, 1000, 4, "", true}));

  // big digit
  result.AddEntry("まん", Entry({Type::BIG_DIGIT, 10000, 1, "万"}));
  result.AddEntry("おく", Entry({Type::BIG_DIGIT, -1, 2, "億"}));
  result.AddEntry("おっ", Entry({Type::BIG_DIGIT, -1, 2, "億"}));
  // "町", etc
  result.AddEntry("ちょう", Entry({Type::BIG_DIGIT, -1, 3, "兆", true}));
  // "系", etc
  result.AddEntry("けい", Entry({Type::BIG_DIGIT, -1, 4, "京", true}));
  result.AddEntry("がい", Entry({Type::BIG_DIGIT, -1, 5, "垓"}));

  // spacial cases
  // conflict with "にち"
  result.AddEntry("にちょう",
                    Entry({Type::UNIT_AND_BIG_DIGIT, 2, 3, "兆", true, 3}));
  result.AddEntry("にちょうめ",
                    Entry({Type::UNIT_AND_STOP_DECODING, 2, -1, "", false, 3}));
  result.AddEntry("にちゃん",
                    Entry({Type::UNIT_AND_STOP_DECODING, 2, -1, "", false, 3}));
  // サンチーム (currency) v.s. 3チーム
  result.AddEntry("さんちーむ",
                    Entry({Type::UNIT_AND_STOP_DECODING, 3, -1, "", true, 6}));

  // number suffix conflicting with the other entries
  constexpr absl::string_view kSuffixEntries[] = {
      // に
      // 握り, 日, 人
      "にぎり",
      "にち",
      "にん",
      // し
      // cc, シート, シーベルト (unit), 試合, 式, 室, 品, 社, 尺, 種, 周, 勝, 色
      // シリング, 進, シンガポールドル
      "しーしー",
      "しーと",
      "しーべると",
      "しあい",
      "しき",
      "しつ",
      "しな",
      "しゃ",
      "しゅ",
      "しょう",
      "しょく",
      "しりんぐ",
      "しん",
      // よ
      // 葉
      "よう",
      // ご
      // 号
      "ごう",
      // く
      // 口, 組, クラス, クローナ
      "くだり",
      "くち",
      "くみ",
      "くらす",
      "くろーな",
      // せん
      // センチ, セント
      "せんち",
      "せんと",
      // おく
      // オクターブ
      "おくたーぶ",
      // ちょう
      // 丁目
      "ちょうめ",
  };
  for (const auto &key : kSuffixEntries) {
    result.AddEntry(key, Entry());
  }
  return result;
}

}  // namespace

std::ostream &operator<<(std::ostream &os, const NumberDecoderResult &r) {
  os << absl::StreamFormat("%v", r);
  return os;
}

NumberDecoder::NumberDecoder(): entries_(InitEntries()) {}

std::vector<NumberDecoder::Result> NumberDecoder::Decode(
    absl::string_view key) const {
  State state;
  std::vector<NumberDecoder::Result> results;
  DecodeAux(key, state, results);

  MaybeAppendResult(state, results);
  return results;
}

void NumberDecoder::DecodeAux(absl::string_view key, State &state,
                              std::vector<Result> &results) const {
  if (key.empty()) {
    return;
  }
  size_t key_byte_len;
  Entry e;
  if (!entries_.LongestMatch(key, &e, &key_byte_len)) {
    return;
  }
  switch (e.type) {
    case Type::STOP_DECODING:
      return;
    case Type::UNIT:
      if (!HandleUnitEntry(e, state, results)) {
        return;
      }
      state.consumed_key_byte_len += key_byte_len;
      break;
    case Type::SMALL_DIGIT:
      if (!HandleSmallDigitEntry(e, state, results)) {
        return;
      }
      state.consumed_key_byte_len += key_byte_len;
      break;
    case Type::BIG_DIGIT:
      if (!HandleBigDigitEntry(e, state, results)) {
        return;
      }
      state.consumed_key_byte_len += key_byte_len;
      break;
    case Type::UNIT_AND_BIG_DIGIT:
      if (!HandleUnitEntry(e, state, results)) {
        return;
      }
      state.consumed_key_byte_len += e.consume_byte_len_of_first;
      if (!HandleBigDigitEntry(e, state, results)) {
        return;
      }
      state.consumed_key_byte_len +=
          (key_byte_len - e.consume_byte_len_of_first);
      break;
    case Type::UNIT_AND_STOP_DECODING:
      if (!HandleUnitEntry(e, state, results)) {
        return;
      }
      state.consumed_key_byte_len += e.consume_byte_len_of_first;
      return;
    default:
      LOG(ERROR) << "Error";
  }
  DCHECK_GT(key_byte_len, 0);
  DecodeAux(key.substr(key_byte_len), state, results);
}

bool NumberDecoder::HandleUnitEntry(const Entry &entry, State &state,
                                    std::vector<Result> &results) const {
  results.clear();
  if (state.IsValid() && entry.number == 0) {
    // Supports 0 only as a dependent number.
    return false;
  }
  if ((state.small_digit_num == 0) ||
      (state.small_digit_num != -1 && state.small_digit_num % 10 != 0)) {
    // Unit has been already decoded.
    // Invalid: いちさん, ぜろご
    return false;
  }

  if (entry.output_before_decode) {
    MaybeAppendResult(state, results);
  }

  if (state.small_digit_num == -1) {
    state.small_digit_num = entry.number;
  } else {
    DCHECK_EQ(state.small_digit_num % 10, 0);
    state.small_digit_num += entry.number;
  }
  return true;
}

bool NumberDecoder::HandleSmallDigitEntry(const Entry &entry, State &state,
                                          std::vector<Result> &results) const {
  results.clear();
  if (state.small_digit > 1 && entry.digit >= state.small_digit) {
    // Invalid: じゅうせん
    return false;
  }
  if (state.small_digit_num == 0) {
    // Invalid: ぜろじゅう
    return false;
  }

  if (entry.output_before_decode) {
    MaybeAppendResult(state, results);
  }

  if (state.small_digit_num == -1) {
    state.small_digit_num = entry.number;
  } else {
    const int unit = std::max(1, state.small_digit_num % 10);
    const int base = (state.small_digit_num / 10) * 10;
    state.small_digit_num = base + unit * entry.number;
  }
  state.small_digit = entry.digit;
  return true;
}

bool NumberDecoder::HandleBigDigitEntry(const Entry &entry, State &state,
                                        std::vector<Result> &results) const {
  results.clear();
  if (state.big_digit > 0 && entry.digit >= state.big_digit) {
    // Invalid: おくまん
    return false;
  }
  if (state.small_digit_num == -1 || state.small_digit_num == 0) {
    // Do not decode "まん" to "10000"
    return false;
  }

  if (entry.output_before_decode) {
    MaybeAppendResult(state, results);
  }

  //  state.current_num = -1;
  absl::StrAppend(&state.current_num_str, state.small_digit_num,
                  entry.digit_str);
  state.small_digit_num = -1;
  state.small_digit = -1;
  state.big_digit = entry.digit;
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
                               absl::StrCat(current_num_str, small_digit)};
  } else if (!current_num_str.empty()) {
    // "1万"
    return NumberDecoderResult{consumed_key_byte_len, current_num_str};
  } else if (small_digit == 0) {
    // "0"
    return NumberDecoderResult{consumed_key_byte_len, "0"};
  }
  return std::nullopt;
}

}  // namespace number_decoder_internal
}  // namespace mozc
