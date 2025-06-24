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

#ifndef MOZC_PREDICTION_NUMBER_DECODER_H_
#define MOZC_PREDICTION_NUMBER_DECODER_H_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "base/container/trie.h"

namespace mozc {

struct NumberDecoderResult;

namespace number_decoder_internal {

enum class Type : std::uint8_t {
  STOP_DECODING,  // "にん", "せんち", ...
  UNIT,           // 0-9
  SMALL_DIGIT,    // 10, 100, 1000
  BIG_DIGIT,      // 万, 億, ...
  // Special type for "にちょう"
  // We do not parse it as "にち"+"ょう" by the longest match
  UNIT_AND_BIG_DIGIT,
  // Special type for "にちょうめ"
  // We do not parse it as "にちょう"+"め" by the longest match
  UNIT_AND_STOP_DECODING,
};

struct Entry {
  Type type = Type::STOP_DECODING;
  int number = 0;
  int digit = 1;
  absl::string_view digit_str;
  // Output the current status before decoding the input with the entry.
  bool output_before_decode = false;
  // For UNIT_AND_BIG_DIGIT and UNIT_AND_STOP_DECODING.
  // The key length for the first part.
  int consume_byte_len_of_first = 0;
};

// We decode the Japanese number reading using big_digit and small_digit.
// Big digit stands for the number digit for 10^4N, e.g. "万", "億", "兆", ...
// Small digit stands for the digit, 1, 10, 100, 1000.
//
// |small_digit| and |big_digit| will be used to validate the current state.
// For example, we do not decode "兆" after decoding "万".
struct State {
  bool IsValid() const {
    return !(small_digit_num == -1 && small_digit == -1 && big_digit == -1);
  }

  std::optional<NumberDecoderResult> Result() const;

  template <typename Sink>
  friend void AbslStringify(Sink &sink, const State &state) {
    absl::Format(
        &sink,
        "small_digit_num: %d, num_str: %s, sd: %d, bd: %d, consumed_blen: %d",
        state.small_digit_num, state.current_num_str, state.small_digit,
        state.big_digit, state.consumed_key_byte_len);
  }

  // Current small digit number in integer (e.g. 2000, <= 9999)
  int small_digit_num = -1;
  // Current number in string (e.g. 46億, 2億6000万)
  std::string current_num_str;
  // The current index for the small digit ('digit' in NumberDecoderEntry).
  // e.g. (small_digit_number : digit index) = (1:1), (10:2), (100:3), (1000:4)
  int small_digit = -1;
  // The current index for the big digit
  // e.g. (digit_str : digit index) = ("万":1), ("億", 2), ...
  int big_digit = -1;
  size_t consumed_key_byte_len = 0;

  // Key to decode
  absl::string_view key;

  // Consumed keys
  // ["に", "じゅう"] for "にじゅう": "20"
  std::vector<absl::string_view> consumed_keys;

  // The digit number
  // 12万(=120000) → 6
  int digit_num = 0;
};

}  // namespace number_decoder_internal

struct NumberDecoderResult {
  NumberDecoderResult() = default;
  NumberDecoderResult(size_t len, std::string c, int digit_num)
      : consumed_key_byte_len(len),
        candidate(std::move(c)),
        digit_num(digit_num) {}

  template <typename Sink>
  friend void AbslStringify(Sink &sink, const NumberDecoderResult &result) {
    absl::Format(&sink, "(%d,\" %s\", %d)", result.consumed_key_byte_len,
                 result.candidate, result.digit_num);
  }

  size_t consumed_key_byte_len;
  std::string candidate;
  int digit_num;  // 12万(=120000) → 6
};

constexpr bool operator==(const NumberDecoderResult &lhs,
                          const NumberDecoderResult &rhs) {
  return std::tie(lhs.consumed_key_byte_len, lhs.candidate, lhs.digit_num) ==
         std::tie(rhs.consumed_key_byte_len, rhs.candidate, rhs.digit_num);
}

std::ostream &operator<<(std::ostream &os, const NumberDecoderResult &r);

class NumberDecoder {
 public:
  using Result = NumberDecoderResult;

  NumberDecoder();

  NumberDecoder(NumberDecoder &&) = default;
  NumberDecoder &operator=(NumberDecoder &&) = default;

  std::vector<Result> Decode(absl::string_view key) const;

 private:
  void DecodeAux(absl::string_view key, number_decoder_internal::State &state,
                 std::vector<Result> &results) const;
  bool HandleUnitEntry(absl::string_view key,
                       const number_decoder_internal::Entry &entry,
                       number_decoder_internal::State &state,
                       std::vector<Result> &results) const;
  bool HandleSmallDigitEntry(absl::string_view key,
                             const number_decoder_internal::Entry &entry,
                             number_decoder_internal::State &state,
                             std::vector<Result> &results) const;
  bool HandleBigDigitEntry(absl::string_view key,
                           const number_decoder_internal::Entry &entry,
                           number_decoder_internal::State &state,
                           std::vector<Result> &results) const;

  const Trie<number_decoder_internal::Entry> &entries_;
};

}  // namespace mozc

#endif  // MOZC_PREDICTION_NUMBER_DECODER_H_
