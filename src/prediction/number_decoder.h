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

#include <istream>
#include <ostream>
#include <string>
#include <vector>

#include "base/trie.h"
#include "absl/strings/string_view.h"

namespace mozc {

enum NumberDecoderEntryType : std::uint8_t {
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

struct NumberDecoderEntry {
  NumberDecoderEntryType type = STOP_DECODING;
  int number = 0;
  int digit = 1;
  std::string digit_str = "";
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
struct NumberDecoderState {
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

  bool IsValid() const {
    return !(small_digit_num == -1 && small_digit == -1 && big_digit == -1);
  }

  std::string DebugString() const {
    return absl::StrCat("small_digit_num: ", small_digit_num,
                        "\tnum_str: ", current_num_str, "\tsd: ", small_digit,
                        "\tbd: ", big_digit,
                        "\tconsumed_blen: ", consumed_key_byte_len);
  }
};

struct NumberDecoderResult {
  size_t consumed_key_byte_len;
  std::string candidate;

  NumberDecoderResult(size_t len, const std::string &c)
      : consumed_key_byte_len(len), candidate(c) {}

  bool operator==(const NumberDecoderResult &other) const {
    return consumed_key_byte_len == other.consumed_key_byte_len &&
           candidate == other.candidate;
  }

  friend std::ostream &operator<<(std::ostream &s,
                                  const NumberDecoderResult &r) {
    return s << "(" << r.consumed_key_byte_len << ", \"" << r.candidate
             << "\")";
  }
};

class NumberDecoder {
 public:
  using State = NumberDecoderState;
  using Entry = NumberDecoderEntry;
  using Result = NumberDecoderResult;

  NumberDecoder();
  bool Decode(absl::string_view key, std::vector<Result> *results) const;

 private:
  void DecodeAux(absl::string_view key, State *state,
                 std::vector<Result> *results) const;
  bool HandleUnitEntry(const Entry &entry, State *state,
                       std::vector<Result> *results) const;
  bool HandleSmallDigitEntry(const Entry &entry, State *state,
                             std::vector<Result> *results) const;
  bool HandleBigDigitEntry(const Entry &entry, State *state,
                           std::vector<Result> *results) const;
  void MayAppendResults(const State &state, size_t consumed_byte_len,
                        std::vector<Result> *results) const;
  void InitEntries();

  Trie<Entry> entries_;
};

}  // namespace mozc

#endif  // MOZC_PREDICTION_NUMBER_DECODER_H_
