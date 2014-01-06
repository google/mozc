// Copyright 2010-2014, Google Inc.
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

#include <set>
#include <string>

#include "base/port.h"
#include "composer/internal/transliterators.h"
// For TableAttributes
#include "composer/table.h"
// for FRIEND_TEST()
#include "testing/base/public/gunit_prod.h"

namespace mozc {
namespace composer {

class CompositionInput;
class Table;

// This class contains a unit of composition string.  The unit consists of
// conversion, pending and raw strings.  Every unit should be the shortest
// size separated by the conversion table.  A sample units with normal
// romaji-hiragana conversion table are {conversion: "か", pending: "", raw:
// "ka"} and {conversion: "っ", pending: "t", raw: "tt"}.
class CharChunk {
 public:
  // LOCAL transliterator is not accepted.
  CharChunk(Transliterators::Transliterator transliterator,
            const Table *table);

  void Clear();

  size_t GetLength(Transliterators::Transliterator transliterator) const;

  // Append the characters representing this CharChunk accoring to the
  // transliterator.  If the transliterator is LOCAL, the local
  // transliterator specified via SetTransliterator is used.
  void AppendResult(Transliterators::Transliterator transliterator,
                    string *result) const;
  void AppendTrimedResult(Transliterators::Transliterator transliterator,
                          string *result) const;
  void AppendFixedResult(Transliterators::Transliterator transliterator,
                         string *result) const;

  // Get possible results from current chunk
  void GetExpandedResults(set<string> *results) const;
  bool IsFixed() const;

  // True if IsAppendable() is true and this object is fixed (|pending_|=="")
  // when |input| is appended.
  bool IsConvertible(
      Transliterators::Transliterator transliterator,
      const Table *table,
      const string &input) const;

  // Combines all fields with |left_chunk|.
  // [this chunk] := [left_chunk]+[this chunk]
  // Note that after calling this method,
  // the information contained in |left_chunk| duplicates.
  // Deleting |left_chunk| would be preferable.
  void Combine(const CharChunk& left_chunk);

  // Return true if this char chunk accepts additional characters with
  // the specified transliterator and the table.
  bool IsAppendable(Transliterators::Transliterator transliterator,
                    const Table *table) const;

  // Split CharChunk at |position| and set split new chunk to |left_new_chunk|.
  // CharChunk doesn't have ownership of the new chunk.
  bool SplitChunk(Transliterators::Transliterator transliterator,
                  size_t position,
                  CharChunk **left_new_chunk);

  // Return true if this chunk should be commited immediately.  This
  // function refers DIRECT_INPUT attribute.
  bool ShouldCommit() const;

  bool ShouldInsertNewChunk(const CompositionInput &input) const;
  void AddInput(string *input);
  void AddConvertedChar(string *input);
  void AddInputAndConvertedChar(string *key,
                                string *converted_char);
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

  string Transliterate(Transliterators::Transliterator transliterator,
                       const string &raw, const string &converted) const;

  // Test only
  const string &raw() const;
  // Test only
  void set_raw(const string &raw);

  // Test only
  const string &conversion() const;
  // Test only
  void set_conversion(const string &conversion);

  // Test only
  const string &pending() const;
  // Test only
  void set_pending(const string &pending);

  // Test only
  const string &ambiguous() const;
  // Test only
  void set_ambiguous(const string &ambiguous);

  CharChunk *Clone() const;

  // Test only
  bool AddInputInternal(string *input);

 private:
  FRIEND_TEST(CharChunkTest, Clone);
  FRIEND_TEST(CharChunkTest, GetTransliterator);

  Transliterators::Transliterator transliterator_;
  const Table *table_;

  string raw_;
  string conversion_;
  string pending_;
  string ambiguous_;
  TableAttributes attributes_;
};

}  // namespace composer
}  // namespace mozc

#endif  // MOZC_COMPOSER_INTERNAL_CHAR_CHUNK_H_
