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

#ifndef MOZC_COMPOSER_INTERNAL_CHAR_CHUNK_H_
#define MOZC_COMPOSER_INTERNAL_CHAR_CHUNK_H_

#include <cstddef>
#include <string>
#include <tuple>
#include <utility>

#include "absl/container/btree_set.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "base/strings/assign.h"
#include "composer/internal/composition_input.h"
#include "composer/internal/transliterators.h"
#include "composer/table.h"

namespace mozc {
namespace composer {

// This class contains a unit of composition string.  The unit consists of
// conversion, pending and raw strings.  Every unit should be the shortest
// size separated by the conversion table.  A sample units with normal
// romaji-hiragana conversion table are {conversion: "か", pending: "", raw:
// "ka"} and {conversion: "っ", pending: "t", raw: "tt"}.
class CharChunk final {
 public:
  // LOCAL transliterator is not accepted.
  CharChunk(Transliterators::Transliterator transliterator, const Table *table);

  // Copyable and movable.
  CharChunk(const CharChunk &x) = default;
  CharChunk &operator=(const CharChunk &x) = default;
  CharChunk(CharChunk &&x) = default;
  CharChunk &operator=(CharChunk &&x) = default;

  void Clear();

  size_t GetLength(Transliterators::Transliterator t12r) const;

  // Append the characters representing this CharChunk according to the
  // transliterator.  If the transliterator is LOCAL, the local
  // transliterator specified via SetTransliterator is used.
  void AppendResult(Transliterators::Transliterator t12r,
                    std::string *result) const;
  void AppendTrimedResult(Transliterators::Transliterator t12r,
                          std::string *result) const;
  void AppendFixedResult(Transliterators::Transliterator t12r,
                         std::string *result) const;

  // Get possible results from current chunk
  absl::btree_set<std::string> GetExpandedResults() const;
  bool IsFixed() const;

  // True if IsAppendable() is true and this object is fixed (|pending_|=="")
  // when |input| is appended.
  bool IsConvertible(Transliterators::Transliterator t12r, const Table *table,
                     absl::string_view input) const;

  // Combines all fields with |left_chunk|.
  // [this chunk] := [left_chunk]+[this chunk]
  // Note that after calling this method,
  // the information contained in |left_chunk| duplicates.
  // Deleting |left_chunk| would be preferable.
  void Combine(const CharChunk &left_chunk);

  // Return true if this char chunk accepts additional characters with
  // the specified transliterator and the table.
  bool IsAppendable(Transliterators::Transliterator t12r,
                    const Table *table) const;

  // Splits this CharChunk at |position| and returns the left chunk. Returns
  // nullptr on failure.
  absl::StatusOr<CharChunk> SplitChunk(Transliterators::Transliterator t12r,
                                       size_t position);

  // Return true if this chunk should be committed immediately.  This
  // function refers DIRECT_INPUT attribute.
  bool ShouldCommit() const;

  bool ShouldInsertNewChunk(const CompositionInput &input) const;
  void AddInput(std::string *input);
  void AddCompositionInput(CompositionInput *input);

  void SetTransliterator(Transliterators::Transliterator transliterator);

  // Gets a transliterator basing on the given |transliterator|.
  // - If |transliterator| is |Transliterators::LOCAL|, the local transliterator
  //   is returned. But if NO_TRANSLITERATION attribute is set,
  //   |Transliterators::CONVERSION_STRING| is returned.
  //   This behavior is for mobile. Without this behavior, raw string is used
  //   on unexpected situation. For example, on 12keys-toggle-alphabet mode,
  //   a user types "2223" to get "cd". In this case local transliterator is
  //   HALF_ASCII and HALF_ASCII transliterator uses raw string
  //   so withtout NO_TRANSLITERATION a user will get "2223" as preedit.
  // - If |transliterator| is not LOCAL, given |transliterator| is returned.
  //   But if NO_TRANSLITERATION attribute is set and |transliterator| is
  //   HALF_ASCII or FULL_ASCII, |Transliterators::CONVERSION_STRING|
  //   is returned.
  //   NO_TRANSLITERATION means that raw input is (basically) meaningless
  //   so HALF_ASCII and FULL_ASCII, which uses raw input, should not be used.
  Transliterators::Transliterator GetTransliterator(
      Transliterators::Transliterator transliterator) const;

  std::string Transliterate(Transliterators::Transliterator transliterator,
                            absl::string_view raw,
                            absl::string_view converted) const;

  // The following accessors and mutators are for test only.
  Transliterators::Transliterator transliterator() const {
    return transliterator_;
  }
  const Table *table() const { return table_; }

  const std::string &raw() const { return raw_; }
  template <typename String>
  void set_raw(String &&raw) {
    strings::Assign(raw_, std::forward<String>(raw));
    local_length_cache_ = std::string::npos;
  }

  const std::string &conversion() const { return conversion_; }
  template <typename String>
  void set_conversion(String &&conversion) {
    strings::Assign(conversion_, std::forward<String>(conversion));
    local_length_cache_ = std::string::npos;
  }

  const std::string &pending() const { return pending_; }
  template <typename String>
  void set_pending(String &&pending) {
    strings::Assign(pending_, std::forward<String>(pending));
    local_length_cache_ = std::string::npos;
  }

  const std::string &ambiguous() const { return ambiguous_; }
  template <typename String>
  void set_ambiguous(String &&ambiguous) {
    strings::Assign(ambiguous_, std::forward<String>(ambiguous));
    local_length_cache_ = std::string::npos;
  }

  TableAttributes attributes() const { return attributes_; }
  void set_attributes(TableAttributes attributes);

  friend bool operator==(const CharChunk &lhs, const CharChunk &rhs) {
    return std::tie(lhs.table_, lhs.raw_, lhs.conversion_, lhs.pending_,
                    lhs.ambiguous_, lhs.transliterator_, lhs.attributes_) ==
           std::tie(rhs.table_, rhs.raw_, rhs.conversion_, rhs.pending_,
                    rhs.ambiguous_, rhs.transliterator_, rhs.attributes_);
  }

  friend bool operator!=(const CharChunk &lhs, const CharChunk &rhs) {
    return !(lhs == rhs);
  }

  template <typename Sink>
  friend void AbslStringify(Sink &sink, const CharChunk &chunk) {
    absl::Format(&sink,
                 "table = %p, raw = %s, conversion = %s, pending = %s, "
                 "ambiguous = %s, transliterator = %v, attributes = %v",
                 chunk.table_, chunk.raw_, chunk.conversion_, chunk.pending_,
                 chunk.ambiguous_, chunk.transliterator_, chunk.attributes_);
  }

  // bool = should loop
  // string_view = rest of the input
  std::pair<bool, absl::string_view> AddInputInternal(absl::string_view input);

 private:
  void AddInputAndConvertedChar(CompositionInput *composition_input);

  const Table *table_;

  // There are four variables to represent a composing text:
  // `raw_`, `conversion_`, `pending_`, and `ambiguous_`.
  // In general, when the user types a key (e.g. "a"),
  // `raw_` contains the typed character (i.e. "a"), and
  // either or some of `conversion_`, `pending_`, and `ambiguous_` contains
  // converted characters. (e.g. "あ" to `converted_`).
  //
  // At least one of `conversion_`, `pending_`, and `ambiguous_` is not empty.

  // `raw_`: Actual characters typed by the user (e.g "ka").
  // In some situations through the user's text editing,
  // it may contain synthesized data transliterated from `conversion_`.
  std::string raw_;
  // `conversion_`: Converted character from `raw_` (e.g. "か").
  // Conversion rules from `raw_` to `conversion_` are described in `table_`.
  std::string conversion_;
  // `pending_`: Converted character from `raw_`.
  // Conversion rules from `raw_` to `pending_` are described in `table_`.
  // If no rules are matched with the input, it is also added to `pending_`.
  // For example, if `raw_` is "tt", conversion_ is "っ" and pending_ is "t".
  // This `pending_` is prepended to the user's input in the next time.
  std::string pending_;
  // `ambiguous_`: Conversted character from `raw_`, but unlike `conversion_`,
  // it is not yet determined (e.g. "ん").
  // If the `raw_` is "n", it can be converted to "ん" at this moment.
  // However it can be converted to "な", if the next input is "a".
  // In this case, "ん" is stored to `ambiguous_` instead of `conversion_`.
  std::string ambiguous_;
  Transliterators::Transliterator transliterator_;
  TableAttributes attributes_;
  mutable size_t local_length_cache_;
};

}  // namespace composer
}  // namespace mozc

#endif  // MOZC_COMPOSER_INTERNAL_CHAR_CHUNK_H_
