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

#include "composer/composition.h"

#include <cstddef>
#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include "absl/algorithm/container.h"
#include "absl/container/btree_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "base/util.h"
#include "base/vlog.h"
#include "composer/char_chunk.h"
#include "composer/composition_input.h"
#include "composer/table.h"
#include "composer/transliterators.h"

namespace mozc {
namespace composer {

void Composition::Erase() { chunks_.clear(); }

size_t Composition::InsertAt(size_t pos, std::string input) {
  CompositionInput composition_input;
  composition_input.set_raw(std::move(input));
  return InsertInput(pos, std::move(composition_input));
}

size_t Composition::InsertKeyAndPreeditAt(const size_t pos, std::string key,
                                          std::string preedit) {
  CompositionInput composition_input;
  composition_input.set_raw(std::move(key));
  composition_input.set_conversion(std::move(preedit));
  return InsertInput(pos, std::move(composition_input));
}

size_t Composition::InsertInput(size_t pos, CompositionInput input) {
  if (input.Empty()) {
    return pos;
  }

  CharChunkList::iterator right_chunk = MaybeSplitChunkAt(pos);
  while (right_chunk != chunks_.end() &&
         right_chunk->GetLength(input_t12r_) == 0) {
    ++right_chunk;
  }

  CharChunkList::iterator left_chunk = GetInsertionChunk(right_chunk);

  CombinePendingChunks(left_chunk, input);

  while (true) {
    left_chunk->AddCompositionInput(&input);
    if (input.Empty()) {
      break;
    }
    left_chunk = InsertChunk(right_chunk);
    input.set_is_new_input(false);
  }

  // If the chunk is empty as the result of AddCompositionInput above, removes
  // the empty chunk.
  if (left_chunk->raw().empty() && left_chunk->conversion().empty() &&
      left_chunk->pending().empty()) {
    chunks_.erase(left_chunk);
  }

  return GetPosition(Transliterators::LOCAL, right_chunk);
}

// Deletes a right-hand character of the composition at the position.
size_t Composition::DeleteAt(const size_t position) {
  const size_t original_size = GetLength();
  size_t new_position = position;
  // We have to perform deletion repeatedly because there might be 0-length
  // chunk.
  // For example,
  // chunk0 : '{a}'  (invisible character only == 0-length)
  // chunk1 : 'b'
  // And DeleteAt(0) is invoked, we have to delete both chunks.
  while (!chunks_.empty() && GetLength() == original_size) {
    CharChunkList::iterator chunk_it = MaybeSplitChunkAt(position);
    new_position = GetPosition(Transliterators::LOCAL, chunk_it);
    if (chunk_it == chunks_.end()) {
      break;
    }

    // We have to consider 0-length chunk.
    // If a chunk contains only invisible characters,
    // the result of GetLength is 0.
    if (chunk_it->GetLength(Transliterators::LOCAL) <= 1) {
      chunks_.erase(chunk_it);
      continue;
    }

    absl::StatusOr<CharChunk> left_deleted_chunk =
        chunk_it->SplitChunk(Transliterators::LOCAL, 1);
    if (!left_deleted_chunk.ok()) {
      LOG(WARNING) << "SplitChunk: " << left_deleted_chunk.status();
    }
  }
  return new_position;
}

size_t Composition::ConvertPosition(
    const size_t position_from,
    Transliterators::Transliterator transliterator_from,
    Transliterators::Transliterator transliterator_to) const {
  // TODO(komatsu) This is a hacky way.
  if (transliterator_from == transliterator_to) {
    return position_from;
  }

  size_t inner_position_from;
  auto chunk_it =
      GetChunkAt(position_from, transliterator_from, &inner_position_from);

  // No chunk was found, return 0 as a fallback.
  if (chunk_it == chunks_.end()) {
    return 0;
  }

  const size_t chunk_length_from = chunk_it->GetLength(transliterator_from);

  CHECK(inner_position_from <= chunk_length_from);

  const size_t position_to = GetPosition(transliterator_to, chunk_it);

  if (inner_position_from == 0) {
    return position_to;
  }

  const size_t chunk_length_to = chunk_it->GetLength(transliterator_to);
  if (inner_position_from == chunk_length_from) {
    // If the inner_position_from is the end of the chunk (ex. "ka|"
    // vs "か"), the converted position should be the end of the
    // chunk too (ie. "か|").
    return position_to + chunk_length_to;
  }

  if (inner_position_from > chunk_length_to) {
    // When inner_position_from is greater than chunk_length_to
    // (ex. "ts|u" vs "つ", inner_position_from is 2 and
    // chunk_length_to is 1), the converted position should be the end
    // of the chunk (ie. "つ|").
    return position_to + chunk_length_to;
  }

  DCHECK(inner_position_from <= chunk_length_to);
  // When inner_position_from is less than or equal to chunk_length_to
  // (ex. "っ|と" vs "tto", inner_position_from is 1 and
  // chunk_length_to is 2), the converted position is adjusted from
  // the beginning of the chunk (ie. "t|to").
  return position_to + inner_position_from;
}

size_t Composition::SetDisplayMode(
    const size_t position, Transliterators::Transliterator transliterator) {
  SetTransliterator(0, GetLength(), transliterator);
  SetInputMode(transliterator);
  return GetLength();
}

void Composition::SetTransliterator(
    const size_t position_from, const size_t position_to,
    Transliterators::Transliterator transliterator) {
  if (position_from > position_to) {
    LOG(ERROR) << "position_from should not be greater than position_to.";
    return;
  }

  if (chunks_.empty()) {
    return;
  }

  size_t inner_position_from;
  auto chunk_it =
      GetChunkAt(position_from, Transliterators::LOCAL, &inner_position_from);

  size_t inner_position_to;
  auto end_it =
      GetChunkAt(position_to, Transliterators::LOCAL, &inner_position_to);

  // chunk_it and end_it can be the same iterator from the beginning.
  while (chunk_it != end_it) {
    chunk_it->SetTransliterator(transliterator);
    ++chunk_it;
  }
  end_it->SetTransliterator(transliterator);
}

Transliterators::Transliterator Composition::GetTransliterator(
    size_t position) const {
  size_t inner_position = 0;
  const auto chunk_it =
      GetChunkAt(position, Transliterators::LOCAL, &inner_position);
  DCHECK(chunk_it != chunks_.end());
  return chunk_it->GetTransliterator(Transliterators::LOCAL);
}

size_t Composition::GetLength() const {
  return GetPosition(Transliterators::LOCAL, chunks_.end());
}

std::string Composition::GetStringWithModes(
    Transliterators::Transliterator transliterator,
    const TrimMode trim_mode) const {
  if (chunks_.empty()) {
    // This is not an error. For example, the composition should be empty for
    // the first keydown event after turning on the IME.
    return std::string();
  }

  CharChunkList::const_iterator it;
  std::string composition;
  for (it = chunks_.begin(); it != std::prev(chunks_.end()); ++it) {
    it->AppendResult(transliterator, &composition);
  }

  switch (trim_mode) {
    case TRIM:
      it->AppendTrimedResult(transliterator, &composition);
      break;
    case ASIS:
      it->AppendResult(transliterator, &composition);
      break;
    case FIX:
      it->AppendFixedResult(transliterator, &composition);
      break;
    default:
      LOG(WARNING) << "Unexpected trim mode: " << trim_mode;
      break;
  }
  return composition;
}

std::pair<std::string, absl::btree_set<std::string>>
Composition::GetExpandedStrings() const {
  Transliterators::Transliterator transliterator = Transliterators::LOCAL;
  if (chunks_.empty()) {
    MOZC_VLOG(1) << "The composition size is zero.";
    return std::make_pair(std::string(), absl::btree_set<std::string>());
  }

  std::string base;
  CharChunkList::const_iterator it;
  for (it = chunks_.begin(); it != std::prev(chunks_.end()); ++it) {
    it->AppendResult(transliterator, &base);
  }

  chunks_.back().AppendTrimedResult(transliterator, &base);
  // Get expanded from the last chunk
  const absl::btree_set<std::string> expanded =
      chunks_.back().GetExpandedResults();
  return std::make_pair(base, expanded);
}

std::string Composition::GetString() const {
  if (chunks_.empty()) {
    MOZC_VLOG(1) << "The composition size is zero.";
    return std::string();
  }

  std::string composition;
  for (CharChunkList::const_iterator it = chunks_.begin(); it != chunks_.end();
       ++it) {
    it->AppendResult(Transliterators::LOCAL, &composition);
  }
  return composition;
}

std::string Composition::GetStringWithTransliterator(
    Transliterators::Transliterator transliterator) const {
  return GetStringWithModes(transliterator, FIX);
}

std::string Composition::GetStringWithTrimMode(const TrimMode trim_mode) const {
  return GetStringWithModes(Transliterators::LOCAL, trim_mode);
}

void Composition::GetPreedit(size_t position, std::string *left,
                             std::string *focused, std::string *right) const {
  const std::string composition = GetString();
  Util::Utf8SubString(composition, 0, position, left);
  Util::Utf8SubString(composition, position, 1, focused);
  Util::Utf8SubString(composition, position + 1, std::string::npos, right);
}

// This function is essentially a const function, but it cannot be const because
// it outputs a mutable CharChunkList::iterator.  Const version is provided
// below.
CharChunkList::iterator Composition::GetChunkAt(
    const size_t position, Transliterators::Transliterator transliterator,
    size_t *inner_position) {
  if (chunks_.empty()) {
    *inner_position = 0;
    return chunks_.begin();
  }

  size_t chunk_offset = 0;
  size_t chunk_length = 0;
  for (auto it = chunks_.begin(); it != chunks_.end(); ++it) {
    chunk_length = it->GetLength(transliterator);
    if (chunk_offset + chunk_length < position) {
      chunk_offset += chunk_length;
      continue;
    }
    *inner_position = position - chunk_offset;
    return it;
  }
  auto it = std::prev(chunks_.end());
  // Inner position here is the end of the last chunk.
  *inner_position = chunk_length;
  return it;
}

CharChunkList::const_iterator Composition::GetChunkAt(
    size_t position, Transliterators::Transliterator transliterator,
    size_t *inner_position) const {
  // This const_cast is safe as GetChunkAt() above doesn't mutate any members.
  return const_cast<Composition *>(this)->GetChunkAt(position, transliterator,
                                                     inner_position);
}

size_t Composition::GetPosition(Transliterators::Transliterator transliterator,
                                CharChunkList::const_iterator cur_it) const {
  size_t position = 0;
  CharChunkList::const_iterator it;
  for (it = chunks_.begin(); it != cur_it; ++it) {
    position += it->GetLength(transliterator);
  }
  return position;
}

// Return the iterator to the right side CharChunk at the `position`.
// If the `position` is in the middle of a CharChunk, that CharChunk is split.
CharChunkList::iterator Composition::MaybeSplitChunkAt(const size_t position) {
  size_t inner_position;
  CharChunkList::iterator it =
      GetChunkAt(position, Transliterators::LOCAL, &inner_position);
  if (it == chunks_.begin() && inner_position == 0) {
    return it;
  }

  CharChunk &chunk = *it;
  if (inner_position == chunk.GetLength(Transliterators::LOCAL)) {
    return std::next(it);
  }

  absl::StatusOr<CharChunk> left_chunk =
      chunk.SplitChunk(Transliterators::LOCAL, inner_position);
  if (left_chunk.ok()) {
    chunks_.insert(it, *std::move(left_chunk));
  }
  return it;
}

void Composition::CombinePendingChunks(CharChunkList::iterator it,
                                       const CompositionInput &input) {
  // If the input is asis, pending chunks are not related with this input.
  if (input.is_asis()) {
    return;
  }
  // Combine |**it| and |**(--it)| into |**it| as long as possible.
  const absl::string_view next_input =
      input.conversion().empty() ? input.raw() : input.conversion();

  while (it != chunks_.begin()) {
    CharChunkList::iterator left_it = it;
    --left_it;
    if (!left_it->IsConvertible(input_t12r_, *table_,
                                absl::StrCat(it->pending(), next_input))) {
      return;
    }

    it->Combine(*left_it);
    chunks_.erase(left_it);
  }
}

// Insert a chunk to the prev of it.
CharChunkList::iterator Composition::InsertChunk(
    CharChunkList::const_iterator it) {
  return chunks_.insert(it, CharChunk(input_t12r_, table_));
}

const CharChunkList &Composition::GetCharChunkList() const { return chunks_; }

bool Composition::ShouldCommit() const {
  return absl::c_all_of(
      chunks_, [](const CharChunk &chunk) { return chunk.ShouldCommit(); });
}

// Return charchunk to be inserted and iterator of the *next* char chunk.
CharChunkList::iterator Composition::GetInsertionChunk(
    CharChunkList::iterator it) {
  if (it == chunks_.begin()) {
    return InsertChunk(it);
  }

  const CharChunkList::iterator left_it = std::prev(it);
  if (left_it->IsAppendable(input_t12r_, *table_)) {
    return left_it;
  }
  return InsertChunk(it);
}

void Composition::SetInputMode(Transliterators::Transliterator transliterator) {
  input_t12r_ = transliterator;
}

void Composition::SetTable(std::shared_ptr<const Table> table) {
  DCHECK(table);
  table_ = std::move(table);
}

bool Composition::IsToggleable(size_t position) const {
  size_t inner_position = 0;
  const auto it = GetChunkAt(position, Transliterators::LOCAL, &inner_position);
  if (it == chunks_.end()) {
    return false;
  }
  return it->pending().starts_with(table_->ParseSpecialKey("{?}"));
}

}  // namespace composer
}  // namespace mozc
