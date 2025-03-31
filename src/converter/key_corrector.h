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

#ifndef MOZC_CONVERTER_KEY_CORRECTOR_H_
#define MOZC_CONVERTER_KEY_CORRECTOR_H_

#include <cstddef>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"

namespace mozc {

class KeyCorrector final {
 public:
  enum InputMode {
    ROMAN,
    KANA,
  };

  KeyCorrector() : available_(false), mode_(ROMAN) {}
  KeyCorrector(absl::string_view key, InputMode mode, size_t history_size)
      : available_(false), mode_(mode) {
    CorrectKey(key, mode, history_size);
  }

  // Movable
  KeyCorrector(KeyCorrector &&other) = default;
  KeyCorrector &operator=(KeyCorrector &&other) = default;

  InputMode mode() const { return mode_; }

  bool CorrectKey(absl::string_view key, InputMode mode, size_t history_size);

  // return corrected key;
  absl::string_view corrected_key() const { return corrected_key_; }

  // return original key;
  absl::string_view original_key() const { return original_key_; }

  // return true key correction was done successfully
  bool IsAvailable() const { return available_; }

  // return the position of corrected_key corresponding
  // to the original_key_pos
  // return InvalidPosition() if invalid pos is passed.
  // Note that the position is not by Unicode Character but by bytes.
  size_t GetCorrectedPosition(size_t original_key_pos) const;

  // return the position of original_key corresponding
  // to the corrected_key_pos
  // return InvalidPosition() if invalid pos is passed.
  // Note that the position is not by Unicode Character but by bytes.
  size_t GetOriginalPosition(size_t corrected_key_pos) const;

  // return true if pos is NOT kInvalidPos
  static bool IsValidPosition(size_t pos) { return (pos != kInvalidPos); }

  // return true if pos is kInvalidPos
  static bool IsInvalidPosition(size_t pos) { return (pos == kInvalidPos); }

  // return kInvalidPos
  static size_t InvalidPosition() { return kInvalidPos; }

  // return new prefix of string corresponding to
  // the prefix of the original key at "original_key_pos"
  // if new prefix and original prefix are the same, return nullptr.
  // Note that return value won't be nullptr terminated.
  // "length" stores the length of return value.
  // We don't allow empty matching (see GetPrefix(15) below)
  //
  // More formally, this function can be defined as:
  // GetNewPrefix(original_key_pos) ==
  //   corrected_key.substr(GetCorrectedPosition(original_key),
  //                        corrected_key.size() -
  //                        GetCorrectedPosition(original_key))
  //
  // Example:
  //  original: "せかいじゅのはっぱ"
  //  corrected: "せかいじゅうのはっぱ"
  //  GetPrefix(0) = "せかいじゅうのはっぱ"
  //  GetPrefix(3) = "かいじゅうのはっぱ"
  //  GetPrefix(9) = "じゅうのはっぱ"
  //  GetPrefix(12) = "ゅうのはっぱ"
  //  GetPrefix(15) = nullptr (not "うのはっぱ")
  //                  "う" itself doesn't correspond to the original key,
  //                   so, we don't make a new prefix
  //  GetPrefix(18) = nullptr (same as original)
  //
  // Example2:
  //  original: "みんあのほん"
  //  GetPrefix(0) = "みんなのほん"
  //  GetPrefix(3) = "んなのほん"
  //  GetPrefix(9) = "なのほん"
  //  GetPrefix(12) = nullptr
  const char *GetCorrectedPrefix(size_t original_key_pos, size_t *length) const;

  // This is a helper function for CommonPrefixSearch in Converter.
  // Basically it is equivalent to
  // GetOriginalPosition(GetCorrectedPosition(original_key_pos)
  //                     + new_key_offset) - original_key_pos;
  //
  // Usage:
  // const char *corrected_prefix = GetCorrectedPrefix(original_key_pos,
  //                                                   &length);
  // Node *nodes = Lookup(corrected_prefix, length);
  // for node in nodes {
  //   original_offset = GetOriginalOffset(original_key_pos, node->length);
  //   InsertLattice(original_key_pos, original_offset);
  // }
  //
  // Example:
  //  original:  "せかいじゅのはっぱ"
  //  corrected: "せかいじゅうのはっぱ"
  // GetOffset(0, 3) == 3
  // GetOffset(0, 12) == 12
  // GetOffset(0, 15) == 12
  // GetOffset(0, 18) == 15
  //
  // By combining GetCorrectedPrefix() and GetOriginalOffset(),
  // Converter is able to know the position of the lattice
  size_t GetOriginalOffset(size_t original_key_pos,
                           size_t new_key_offset) const;

  // return the cost penalty for the corrected key.
  // The return value is added to the original cost as a penalty.
  static int GetCorrectedCostPenalty(absl::string_view key);

  // clear internal data
  void Clear();

 private:
  // maximum key length KeyCorrector can handle
  // if key is too long, we don't do key correction
  static constexpr size_t kMaxSize = 128;

  // invalid alignment marker
  static constexpr size_t kInvalidPos = static_cast<size_t>(-1);

  bool available_;
  InputMode mode_;
  std::string corrected_key_;
  std::string original_key_;
  std::vector<size_t> alignment_;
  std::vector<size_t> rev_alignment_;
};

}  // namespace mozc

#endif  // MOZC_CONVERTER_KEY_CORRECTOR_H_
