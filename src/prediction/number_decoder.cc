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
#include <istream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "base/number_util.h"
#include "absl/strings/str_split.h"

namespace mozc {

NumberDecoder::NumberDecoder() { InitEntries(); }

void NumberDecoder::InitEntries() {
  // unit
  entries_.AddEntry("ぜろ", Entry({UNIT, 0}));
  entries_.AddEntry("いち", Entry({UNIT, 1}));
  entries_.AddEntry("いっ", Entry({UNIT, 1}));
  entries_.AddEntry("に", Entry({UNIT, 2}));
  entries_.AddEntry("さん", Entry({UNIT, 3}));
  entries_.AddEntry("し", Entry({UNIT, 4}));
  entries_.AddEntry("よん", Entry({UNIT, 4}));
  entries_.AddEntry("よ", Entry({UNIT, 4}));
  entries_.AddEntry("ご", Entry({UNIT, 5}));
  entries_.AddEntry("ろく", Entry({UNIT, 6}));
  entries_.AddEntry("ろっ", Entry({UNIT, 6}));
  entries_.AddEntry("なな", Entry({UNIT, 7}));
  entries_.AddEntry("しち", Entry({UNIT, 7}));
  entries_.AddEntry("はち", Entry({UNIT, 8}));
  entries_.AddEntry("はっ", Entry({UNIT, 8}));
  entries_.AddEntry("きゅう", Entry({UNIT, 9}));
  entries_.AddEntry("きゅー", Entry({UNIT, 9}));
  entries_.AddEntry("く", Entry({UNIT, 9}));

  // small digit
  // "重", etc
  entries_.AddEntry("じゅう", Entry({SMALL_DIGIT, 10, 2, "", true}));
  entries_.AddEntry("じゅー", Entry({SMALL_DIGIT, 10, 2, "", true}));
  entries_.AddEntry("じゅっ", Entry({SMALL_DIGIT, 10, 2}));
  entries_.AddEntry("ひゃく", Entry({SMALL_DIGIT, 100, 3}));
  entries_.AddEntry("ひゃっ", Entry({SMALL_DIGIT, 100, 3}));
  entries_.AddEntry("びゃく", Entry({SMALL_DIGIT, 100, 3}));
  entries_.AddEntry("びゃっ", Entry({SMALL_DIGIT, 100, 3}));
  entries_.AddEntry("ぴゃく", Entry({SMALL_DIGIT, 100, 3}));
  entries_.AddEntry("ぴゃっ", Entry({SMALL_DIGIT, 100, 3}));
  // "戦", etc
  entries_.AddEntry("せん", Entry({SMALL_DIGIT, 1000, 4, "", true}));
  // "膳"
  entries_.AddEntry("ぜん", Entry({SMALL_DIGIT, 1000, 4, "", true}));

  // big digit
  entries_.AddEntry("まん", Entry({BIG_DIGIT, 10000, 1, "万"}));
  entries_.AddEntry("おく", Entry({BIG_DIGIT, -1, 2, "億"}));
  entries_.AddEntry("おっ", Entry({BIG_DIGIT, -1, 2, "億"}));
  // "町", etc
  entries_.AddEntry("ちょう", Entry({BIG_DIGIT, -1, 3, "兆", true}));
  // "系", etc
  entries_.AddEntry("けい", Entry({BIG_DIGIT, -1, 4, "京", true}));
  entries_.AddEntry("がい", Entry({BIG_DIGIT, -1, 5, "垓"}));

  // spacial cases
  // conflict with "にち"
  entries_.AddEntry("にちょう",
                    Entry({UNIT_AND_BIG_DIGIT, 2, 3, "兆", true, 3}));
  entries_.AddEntry("にちょうめ",
                    Entry({UNIT_AND_STOP_DECODING, 2, -1, "", false, 3}));
  entries_.AddEntry("にちゃん",
                    Entry({UNIT_AND_STOP_DECODING, 2, -1, "", false, 3}));
  // サンチーム (currency) v.s. 3チーム
  entries_.AddEntry("さんちーむ",
                    Entry({UNIT_AND_STOP_DECODING, 3, -1, "", true, 6}));

  // number suffix conflicting with the other entries
  const std::vector<std::string> kSuffixEntries = {
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
    entries_.AddEntry(key, Entry());
  }
}

bool NumberDecoder::Decode(absl::string_view key,
                           std::vector<Result> *results) const {
  State state;
  results->clear();
  DecodeAux(key, &state, results);

  MayAppendResults(state, state.consumed_key_byte_len, results);
  return !results->empty();
}

void NumberDecoder::DecodeAux(absl::string_view key, State *state,
                              std::vector<Result> *results) const {
  if (key.size() == 0) {
    return;
  }
  size_t key_byte_len;
  Entry e;
  if (!entries_.LongestMatch(key, &e, &key_byte_len)) {
    return;
  }
  switch (e.type) {
    case STOP_DECODING:
      return;
    case UNIT:
      if (!HandleUnitEntry(e, state, results)) {
        return;
      }
      state->consumed_key_byte_len += key_byte_len;
      break;
    case SMALL_DIGIT:
      if (!HandleSmallDigitEntry(e, state, results)) {
        return;
      }
      state->consumed_key_byte_len += key_byte_len;
      break;
    case BIG_DIGIT:
      if (!HandleBigDigitEntry(e, state, results)) {
        return;
      }
      state->consumed_key_byte_len += key_byte_len;
      break;
    case UNIT_AND_BIG_DIGIT:
      if (!HandleUnitEntry(e, state, results)) {
        return;
      }
      state->consumed_key_byte_len += e.consume_byte_len_of_first;
      if (!HandleBigDigitEntry(e, state, results)) {
        return;
      }
      state->consumed_key_byte_len +=
          (key_byte_len - e.consume_byte_len_of_first);
      break;
    case UNIT_AND_STOP_DECODING:
      if (!HandleUnitEntry(e, state, results)) {
        return;
      }
      state->consumed_key_byte_len += e.consume_byte_len_of_first;
      return;
    default:
      LOG(ERROR) << "Error";
  }
  DCHECK_GT(key_byte_len, 0);
  DecodeAux(key.substr(key_byte_len), state, results);
}

bool NumberDecoder::HandleUnitEntry(const Entry &entry, State *state,
                                    std::vector<Result> *results) const {
  results->clear();

  if (state->IsValid() && entry.number == 0) {
    // Supports 0 only as a dependent number.
    return false;
  }
  if ((state->small_digit_num == 0) ||
      (state->small_digit_num != -1 && state->small_digit_num % 10 != 0)) {
    // Unit has been already decoded.
    // Invlaid: いちさん, ぜろご
    return false;
  }

  if (entry.output_before_decode) {
    MayAppendResults(*state, state->consumed_key_byte_len, results);
  }

  if (state->small_digit_num == -1) {
    state->small_digit_num = entry.number;
  } else {
    DCHECK_EQ(state->small_digit_num % 10, 0);
    state->small_digit_num += entry.number;
  }
  return true;
}

bool NumberDecoder::HandleSmallDigitEntry(const Entry &entry, State *state,
                                          std::vector<Result> *results) const {
  results->clear();
  if (state->small_digit > 1 && entry.digit >= state->small_digit) {
    // Invalid: じゅうせん
    return false;
  }
  if (state->small_digit_num == 0) {
    // Invalid: ぜろじゅう
    return false;
  }

  if (entry.output_before_decode) {
    MayAppendResults(*state, state->consumed_key_byte_len, results);
  }

  if (state->small_digit_num == -1) {
    state->small_digit_num = entry.number;
  } else {
    const int unit = std::max(1, state->small_digit_num % 10);
    const int base = (state->small_digit_num / 10) * 10;
    state->small_digit_num = base + unit * entry.number;
  }
  state->small_digit = entry.digit;
  return true;
}

bool NumberDecoder::HandleBigDigitEntry(const Entry &entry, State *state,
                                        std::vector<Result> *results) const {
  results->clear();

  if (state->big_digit > 0 && entry.digit >= state->big_digit) {
    // Invalid: おくまん
    return false;
  }
  if (state->small_digit_num == -1 || state->small_digit_num == 0) {
    // Do not decode "まん" to "10000"
    return false;
  }

  if (entry.output_before_decode) {
    MayAppendResults(*state, state->consumed_key_byte_len, results);
  }

  //  state->current_num = -1;
  state->current_num_str.append(
      absl::StrCat(state->small_digit_num, entry.digit_str));
  state->small_digit_num = -1;
  state->small_digit = -1;
  state->big_digit = entry.digit;
  return true;
}

void NumberDecoder::MayAppendResults(const State &state,
                                     size_t consumed_byte_len,
                                     std::vector<Result> *results) const {
  if (!state.IsValid()) {
    return;
  }
  const int small_digit =
      (state.small_digit_num > 0) ? state.small_digit_num : 0;

  if (small_digit > 0) {
    // "1万" + "2000"
    results->emplace_back(Result{
        consumed_byte_len, absl::StrCat(state.current_num_str, small_digit)});
  } else if (!state.current_num_str.empty()) {
    // "1万"
    results->emplace_back(
        Result{consumed_byte_len, absl::StrCat(state.current_num_str)});
  } else if (small_digit == 0) {
    // "0"
    results->emplace_back(Result{consumed_byte_len, absl::StrCat(0)});
  }
}

}  // namespace mozc
