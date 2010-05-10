// Copyright 2010, Google Inc.
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

#include <string>

// Only DisplayMode is used.
#include "composer/composition_interface.h"

#include "base/base.h"

namespace mozc {
namespace composer {

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
  bool IsFixed() const;

  // Return true if this char chunk accepts additional characters with
  // the specified transliterator.
  bool IsAppendable(const TransliteratorInterface *transliterator) const;

  bool SplitChunk(const TransliteratorInterface *transliterator,
                  size_t position,
                  CharChunk* left_new_chunk);

  void AddInput(const Table &table, string *input);
  void AddConvertedChar(string *input);
  void AddInputAndConvertedChar(const Table &table,
                                string *key,
                                string *converted_char);

  void SetTransliterator(const TransliteratorInterface *transliterator);
  const TransliteratorInterface *GetTransliterator(
      const TransliteratorInterface *transliterator) const;

  const string &raw() const;
  void set_raw(const string &raw);

  const string &conversion() const;
  void set_conversion(const string &conversion);

  const string &pending() const;
  void set_pending(const string &pending);

  enum Status {
    NO_CONVERSION = 1,
    NO_RAW = 2,
  };
  void set_status(uint32 status_mask);
  void add_status(uint32 status_mask);
  bool has_status(uint32 status_mask) const;
  void clear_status();

  // Test only
  bool AddInputInternal(const Table &table, string *input);

 private:
  const TransliteratorInterface *transliterator_;

  string raw_;
  string conversion_;
  string pending_;
  string ambiguous_;
  uint32 status_mask_;
};

}  // namespace composer
}  // namespace mozc

#endif  // MOZC_COMPOSER_INTERNAL_CHAR_CHUNK_H_
