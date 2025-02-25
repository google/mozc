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

#ifndef MOZC_COMPOSER_INTERNAL_COMPOSITION_H_
#define MOZC_COMPOSER_INTERNAL_COMPOSITION_H_

#include <cstddef>
#include <list>
#include <string>
#include <utility>

#include "absl/container/btree_set.h"
#include "absl/log/check.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "composer/internal/char_chunk.h"
#include "composer/internal/composition_input.h"
#include "composer/internal/transliterators.h"
#include "composer/table.h"

namespace mozc {
namespace composer {

using CharChunkList = std::list<CharChunk>;

enum TrimMode {
  TRIM,  // "かn" => "か"
  ASIS,  // "かn" => "かn"
  FIX,   // "かn" => "かん"
};

class Composition final {
 public:
  Composition() = default;
  explicit Composition(std::shared_ptr<const Table> table)
      : table_(std::move(table)) {
    DCHECK(table_);
  }

  // Copyable and movable.
  Composition(const Composition &) = default;
  Composition &operator=(const Composition &) = default;
  Composition(Composition &&) = default;
  Composition &operator=(Composition &&) = default;

  // Deletes a right-hand character of the composition at the position.
  // e.g.
  // "012".DeleteAt(0) -> "12"
  // "{0}12".DeleteAt(0) -> "2"  // {0} is an invisible character.
  //
  // Correctly speaking, DeleteAt removes char-chunks ranged between
  // * the left-point where the length is `position`
  // * the right-point where the length is `position + 1`
  //
  // If the char-chunks are ["a", "b", "c"], DeleteAt(1) removes "b" because
  // the range of length 1 and 2 is ["a", < "b" >, "c"].
  // If the char-chunks are ["a", "bc", "d"], DeleteAt(1) removes "b" too.
  // The char-chunks are split if necessary first (e.g.["a", "b", "c", "d"]).
  // Then it becomes ["a", < "b" > , "c", "d"].
  //
  // The same thing is applied to invisible characters.
  // If the char-chunks are ["a", "{b}", "c", "d"],
  // DeleteAt(1) removes both "{b}" and "c",
  // because the range is ["a", < "{b}", "c", > "d"],
  //
  // Note, if the last removed char-chunk has trailing invisible characters,
  // they are also removed.
  // If the char-chunks are ["a", "{b}", "c{d}", "e"],
  // DeleteAt(1) removes both "{b}" and "c{d}".
  //
  // Examples:
  //   ["a", "{b}c", "d"].DeleteAt(0) -> ["{b}c", "d"]
  //   ["a", "{b}c", "d"].DeleteAt(1) -> ["a", "d"]
  //   ["a{b}", "c", "d"].DeleteAt(0) -> ["c", "d"]
  //   ["a{b}", "c", "d"].DeleteAt(1) -> ["a{b}", "d"]
  //   ["a", "{b}", "c"].DeleteAt(0) -> ["{b}, "c"]
  //   ["a", "{b}", "c"].DeleteAt(1) -> ["a"]
  size_t DeleteAt(size_t position);

  size_t InsertAt(size_t position, std::string input);
  size_t InsertKeyAndPreeditAt(size_t position, std::string key,
                               std::string preedit);
  // Insert the given |input| to the composition at the given |position|
  // and return the new position.
  size_t InsertInput(size_t position, CompositionInput input);

  // Clear all chunks.
  void Erase();

  // Get the position on mode_to from position_from on mode_from.
  size_t ConvertPosition(
      size_t position_from, Transliterators::Transliterator transliterator_from,
      Transliterators::Transliterator transliterator_to) const;

  // TODO(komatsu): To be deleted.
  size_t SetDisplayMode(size_t position,
                        Transliterators::Transliterator transliterator);

  // NOTE(komatsu) Kind of SetDisplayMode.
  void SetTransliterator(size_t position_from, size_t position_to,
                         Transliterators::Transliterator transliterator);
  Transliterators::Transliterator GetTransliterator(size_t position) const;

  size_t GetLength() const;
  std::string GetString() const;
  std::string GetStringWithTransliterator(
      Transliterators::Transliterator transliterator) const;
  std::string GetStringWithTrimMode(TrimMode trim_mode) const;
  // Get string with consideration for ambiguity from pending input
  std::pair<std::string, absl::btree_set<std::string>> GetExpandedStrings()
      const;
  void GetPreedit(size_t position, std::string *left, std::string *focused,
                  std::string *right) const;

  void SetInputMode(Transliterators::Transliterator transliterator);

  // Return true if the composition is advised to be committed immediately.
  bool ShouldCommit() const;

  void SetTable(std::shared_ptr<const Table> table);

  bool IsToggleable(size_t position) const;

  // Following methods are declared as public for unit test.

  // Return the focused CharChunk iterator at the `position`,
  // and fill `inner_position` as the position inside the returned CharChunk.
  // ["a", "bc", "e"].GetChunkAt(2) returns "bc" and fills inner_position to 1,
  // as "c" is the focused character.
  CharChunkList::iterator GetChunkAt(
      size_t position, Transliterators::Transliterator transliterator,
      size_t *inner_position);
  CharChunkList::const_iterator GetChunkAt(
      size_t position, Transliterators::Transliterator transliterator,
      size_t *inner_position) const;
  size_t GetPosition(Transliterators::Transliterator transliterator,
                     CharChunkList::const_iterator it) const;

  // Return CharChunk to be inserted a new character.
  // The argument `it` is the focused CharChunk by the cursor.
  CharChunkList::iterator GetInsertionChunk(CharChunkList::iterator it);

  CharChunkList::iterator InsertChunk(CharChunkList::const_iterator it);

  // Return the iterator to the right side CharChunk at the `position`.
  // If the `position` is in the middle of a CharChunk, that CharChunk is split.
  //
  // examples
  // # typical cases
  // chunks: ["a", "bc", "d"], pos: 0 -> "a" (= begin)
  // chunks: ["a", "bc", "d"], pos: 1 -> "bc"
  // chunks: ["a", "bc", "d"], pos: 2 -> "c", chunks become ["a", "b", "c", "d"]
  // chunks: ["a", "bc", "d"], pos: 3 -> "d"
  // chunks: ["a", "bc", "d"], pos: 4 -> end
  // # {b} is an invisible character
  // chunks: ["a", "{b}c", "d"], pos: 1 -> "{b}c"
  // chunks: ["a", "{b}c", "d"], pos: 2 -> "d"
  // chunks: ["a", "{b}c", "d"], pos: 3 -> end
  // # {c} is an invisible character
  // chunks: ["a", "b{c}", "d"], pos: 1 -> "b{c}"
  // chunks: ["a", "b{c}", "d"], pos: 2 -> "d"
  // chunks: ["a", "b{c}", "d"], pos: 3 -> end
  CharChunkList::iterator MaybeSplitChunkAt(size_t position);

  // Combine |input| and chunks from |it| to left direction,
  // which have pending data and can be combined.
  // e.g. [pending='q']+[pending='k']+[pending='y']+[input='o'] are combined
  //      into [pending='q']+[pending='ky'] because [pending='ky']+[input='o']
  //      can turn to be a fixed chunk.
  // e.g. [pending='k']+[pending='y']+[input='q'] are not combined.
  void CombinePendingChunks(CharChunkList::iterator it,
                            const CompositionInput &input);
  const CharChunkList &GetCharChunkList() const;
  std::shared_ptr<const Table> table_for_testing() const { return table_; }
  const CharChunkList &chunks() const { return chunks_; }
  Transliterators::Transliterator input_t12r() const { return input_t12r_; }

  friend bool operator==(const Composition &lhs, const Composition &rhs) {
    return std::tie(lhs.table_, lhs.chunks_, lhs.input_t12r_) ==
           std::tie(rhs.table_, rhs.chunks_, rhs.input_t12r_);
  }
  friend bool operator!=(const Composition &lhs, const Composition &rhs) {
    return !(lhs == rhs);
  }

  template <typename Sink>
  friend void AbslStringify(Sink &sink, const Composition &composition) {
    absl::Format(&sink, "table = %p, input transliterator = %v, chunks = [%s]",
                 composition.table_.get(), composition.input_t12r_,
                 absl::StrJoin(composition.chunks_, ", "));
  }

 private:
  std::string GetStringWithModes(Transliterators::Transliterator transliterator,
                                 TrimMode trim_mode) const;

  std::shared_ptr<const Table> table_;
  CharChunkList chunks_;
  Transliterators::Transliterator input_t12r_ =
      Transliterators::CONVERSION_STRING;
};

}  // namespace composer
}  // namespace mozc

#endif  // MOZC_COMPOSER_INTERNAL_COMPOSITION_H_
