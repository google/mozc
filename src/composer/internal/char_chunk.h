// Copyright 2010-2012, Google Inc.
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

#include "base/base.h"
// For TableAttributes
#include "composer/table.h"
// for FRIEND_TEST()
#include "testing/base/public/gunit_prod.h"

namespace mozc {
namespace composer {

class CompositionInput;
class TransliteratorInterface;

// This class contains a unit of composition string.  The unit consists of
// conversion, pending and raw strings.  Every unit should be the shortest
// size separated by the conversion table.  A sample units with normal
// romaji-hiragana conversion table are {conversion: "か", pending: "", raw:
// "ka"} and {conversion: "っ", pending: "t", raw: "tt"}.
class CharChunk {
 public:
  CharChunk();

  void Clear();

  size_t GetLength(const TransliteratorInterface *transliterator) const;

  // Append the characters representing this CharChunk accoring to the
  // transliterator.  If the transliterator is NULL, the local
  // transliterator specified via SetTransliterator is used.
  void AppendResult(const Table &table,
                    const TransliteratorInterface *transliterator,
                    string *result) const;
  void AppendTrimedResult(const Table &table,
                          const TransliteratorInterface *transliterator,
                          string *result) const;
  void AppendFixedResult(const Table &table,
                         const TransliteratorInterface *transliterator,
                         string *result) const;

  // Get possible results from current chunk
  void GetExpandedResults(const Table &table,
                          const TransliteratorInterface *transliterator,
                          set<string> *results) const;
  bool IsFixed() const;

  // True if IsAppendable() is true and this object is fixed (|pending_|=="")
  // when |input| is appended.
  bool IsConvertible(
      const TransliteratorInterface *transliterator,
      const Table &table,
      const string &input) const;

  // Combines all fields with |left_chunk|.
  // [this chunk] := [left_chunk]+[this chunk]
  // Note that after calling this method,
  // the information contained in |left_chunk| duplicates.
  // Deleting |left_chunk| would be preferable.
  void Combine(const CharChunk& left_chunk);

  // Return true if this char chunk accepts additional characters with
  // the specified transliterator.
  bool IsAppendable(const TransliteratorInterface *transliterator) const;

  bool SplitChunk(const TransliteratorInterface *transliterator,
                  size_t position,
                  CharChunk* left_new_chunk);

  // Return true if this chunk should be commited immediately.  This
  // function refers DIRECT_INPUT attribute.
  bool ShouldCommit() const;

  bool ShouldInsertNewChunk(const Table &table,
                            const CompositionInput &input) const;
  void AddInput(const Table &table, string *input);
  void AddConvertedChar(string *input);
  void AddInputAndConvertedChar(const Table &table,
                                string *key,
                                string *converted_char);
  void AddCompositionInput(const Table &table, CompositionInput *input);

  void SetTransliterator(const TransliteratorInterface *transliterator);
  const TransliteratorInterface *GetTransliterator(
      const TransliteratorInterface *transliterator) const;

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

  enum Status {
    NO_CONVERSION = 1,
    NO_RAW = 2,
  };
  void set_status(uint32 status_mask);
  void add_status(uint32 status_mask);
  bool has_status(uint32 status_mask) const;
  void clear_status();

  void CopyFrom(const CharChunk &src);

  // Test only
  bool AddInputInternal(const Table &table, string *input);

 private:
  FRIEND_TEST(CharChunkTest, CopyFrom);

  const TransliteratorInterface *transliterator_;

  string raw_;
  string conversion_;
  string pending_;
  string ambiguous_;
  uint32 status_mask_;
  TableAttributes attributes_;
};

}  // namespace composer
}  // namespace mozc

#endif  // MOZC_COMPOSER_INTERNAL_CHAR_CHUNK_H_
