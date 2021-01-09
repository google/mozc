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

#include "composer/composition_interface.h"

#include <list>
#include <set>
#include <string>

#include "base/port.h"

namespace mozc {
namespace composer {

class CharChunk;
using CharChunkList = std::list<CharChunk *>;

class CompositionInput;
class Table;

class Composition : public CompositionInterface {
 public:
  explicit Composition(const Table *table);
  ~Composition() override;

  size_t DeleteAt(size_t position) override;
  size_t InsertAt(size_t position, const std::string &input) override;
  size_t InsertKeyAndPreeditAt(size_t position, const std::string &key,
                               const std::string &preedit) override;
  // Insert the given |input| to the composition at the given |position|
  // and return the new position.
  size_t InsertInput(size_t position, const CompositionInput &input) override;

  void Erase() override;

  // Get the position on mode_to from position_from on mode_from.
  size_t ConvertPosition(
      size_t position_from, Transliterators::Transliterator transliterator_from,
      Transliterators::Transliterator transliterator_to) override;

  // TODO(komatsu): To be deleted.
  size_t SetDisplayMode(
      size_t position, Transliterators::Transliterator transliterator) override;

  // NOTE(komatsu) Kind of SetDisplayMode.
  void SetTransliterator(
      size_t position_from, size_t position_to,
      Transliterators::Transliterator transliterator) override;
  Transliterators::Transliterator GetTransliterator(
      size_t position) const override;

  size_t GetLength() const override;
  void GetString(std::string *composition) const override;
  void GetStringWithTransliterator(
      Transliterators::Transliterator transliterator,
      std::string *output) const override;
  void GetStringWithTrimMode(TrimMode trim_mode,
                             std::string *output) const override;
  // Get string with consideration for ambiguity from pending input
  void GetExpandedStrings(std::string *base,
                          std::set<std::string> *expanded) const override;
  void GetExpandedStringsWithTransliterator(
      Transliterators::Transliterator transliterator, std::string *base,
      std::set<std::string> *expanded) const override;
  void GetPreedit(size_t position, std::string *left, std::string *focused,
                  std::string *right) const override;

  void SetInputMode(Transliterators::Transliterator transliterator) override;

  // Return true if the composition is adviced to be committed immediately.
  bool ShouldCommit() const override;

  // Get a clone.
  Composition *Clone() const override;

  void SetTable(const Table *table) override;

  bool IsToggleable(size_t position) const override;

  // Following methods are declared as public for unit test.
  CharChunkList::iterator GetChunkAt(
      size_t position, Transliterators::Transliterator transliterator,
      size_t *inner_position);
  CharChunkList::const_iterator GetChunkAt(
      size_t position, Transliterators::Transliterator transliterator,
      size_t *inner_position) const;
  size_t GetPosition(Transliterators::Transliterator transliterator,
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
  const Table *table() const { return table_; }
  const CharChunkList &chunks() const { return chunks_; }
  Transliterators::Transliterator input_t12r() const { return input_t12r_; }

 private:
  void GetStringWithModes(Transliterators::Transliterator transliterator,
                          TrimMode trim_mode, std::string *output) const;

  const Table *table_;
  CharChunkList chunks_;
  Transliterators::Transliterator input_t12r_;

  DISALLOW_COPY_AND_ASSIGN(Composition);
};

}  // namespace composer
}  // namespace mozc

#endif  // MOZC_COMPOSER_INTERNAL_COMPOSITION_H_
