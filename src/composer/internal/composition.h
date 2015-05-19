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

#ifndef MOZC_COMPOSER_INTERNAL_COMPOSITION_H_
#define MOZC_COMPOSER_INTERNAL_COMPOSITION_H_

#include "composer/composition_interface.h"

#include <list>
#include <set>
#include <string>

#include "base/port.h"

namespace mozc {
namespace composer {

class CharChunk;
typedef list<CharChunk*> CharChunkList;

class CompositionInput;
class Table;
class TransliteratorInterface;

class Composition : public CompositionInterface {
 public:
  explicit Composition(const Table *table);
  virtual ~Composition();

  virtual size_t DeleteAt(size_t position);
  virtual size_t InsertAt(size_t position, const string &input);
  virtual size_t InsertKeyAndPreeditAt(size_t position,
                                       const string &key,
                                       const string &preedit);
  // Insert the given |input| to the composition at the given |position|
  // and return the new position.
  virtual size_t InsertInput(size_t position, const CompositionInput &input);

  virtual void Erase();

  // Get the position on mode_to from position_from on mode_from.
  virtual size_t ConvertPosition(
      size_t position_from,
      const TransliteratorInterface *transliterator_from,
      const TransliteratorInterface *transliterator_to);

  // TODO(komatsu): To be deleted.
  virtual size_t SetDisplayMode(size_t position,
                                const TransliteratorInterface *transliterator);

  // NOTE(komatsu) Kind of SetDisplayMode.
  virtual void SetTransliterator(
      size_t position_from,
      size_t position_to,
      const TransliteratorInterface *transliterator);
  virtual const TransliteratorInterface *GetTransliterator(size_t position);

  virtual size_t GetLength() const;
  virtual void GetString(string *composition) const;
  virtual void GetStringWithTransliterator(
      const TransliteratorInterface *transliterator,
      string *output) const;
  virtual void GetStringWithTrimMode(TrimMode trim_mode, string* output) const;
  // Get string with consideration for ambiguity from pending input
  virtual void GetExpandedStrings(string *base,
                                  set<string> *expanded) const;
  virtual void GetExpandedStringsWithTransliterator(
      const TransliteratorInterface *transliterator,
      string *base,
      set<string> *expanded) const;
  virtual void GetPreedit(
      size_t position, string *left, string *focused, string *right) const;

  virtual void SetInputMode(const TransliteratorInterface *transliterator);

  // Return true if the composition is adviced to be committed immediately.
  virtual bool ShouldCommit() const;

  // Get a clone.
  // Clone is a thin wrapper of CloneImpl.
  // CloneImpl is created to write test codes without dynamic_cast.
  virtual CompositionInterface *Clone() const;
  Composition *CloneImpl() const;

  virtual void SetTable(const Table *table);

  // Following methods are declared as public for unit test.
  void GetChunkAt(size_t position,
                  const TransliteratorInterface *transliterator,
                  CharChunkList::iterator *chunk_it,
                  size_t *inner_position);
  size_t GetPosition(const TransliteratorInterface *transliterator,
                     const CharChunkList::const_iterator &it) const;

  CharChunkList::iterator GetInsertionChunk(CharChunkList::iterator *it);
  CharChunkList::iterator InsertChunk(CharChunkList::iterator *left_it);

  CharChunk *MaybeSplitChunkAt(size_t position, CharChunkList::iterator *it);

  // Combine |input| and chunks from |it| to left direction,
  // which have pending data and can be combined.
  // e.g. [pending='q']+[pending='k']+[pending='y']+[input='o'] are combined
  //      into [pending='q']+[pending='ky'] because [pending='ky']+[input='o']
  //      can turn to be a fixed chunk.
  // e.g. [pending='k']+[pending='y']+[input='q'] are not combined.
  void CombinePendingChunks(CharChunkList::iterator it,
                            const CompositionInput &input);
  const CharChunkList &GetCharChunkList() const;
  const Table *table() const {
    return table_;
  }
  const CharChunkList &chunks() const {
    return chunks_;
  }
  const TransliteratorInterface *input_t12r() const {
    return input_t12r_;
  }

 private:
  void SplitChunk(const size_t position,
                  CharChunk *left_new_chunk,
                  CharChunk *right_orig_chunk) const;

  void GetStringWithModes(const TransliteratorInterface *transliterator,
                          TrimMode trim_mode,
                          string *output) const;

  static void SplitConversionChunk(const size_t position,
                                   CharChunk *left_new_chunk,
                                   CharChunk *right_orig_chunk);
  static void SplitRawChunk(const size_t position,
                            CharChunk *left_new_chunk,
                            CharChunk *right_orig_chunk);

  const Table *table_;
  CharChunkList chunks_;
  const TransliteratorInterface *input_t12r_;

  DISALLOW_COPY_AND_ASSIGN(Composition);
};

}  // namespace composer
}  // namespace mozc

#endif  // MOZC_COMPOSER_INTERNAL_COMPOSITION_H_
