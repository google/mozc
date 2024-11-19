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

#include "composer/internal/char_chunk.h"

#include <cstddef>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/container/btree_set.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "base/strings/unicode.h"
#include "base/util.h"
#include "composer/internal/composition_input.h"
#include "composer/internal/special_key.h"
#include "composer/internal/transliterators.h"
#include "composer/table.h"

namespace mozc {
namespace composer {
namespace {

using ::mozc::composer::internal::DeleteSpecialKeys;
using ::mozc::composer::internal::TrimLeadingSpecialKey;
using ::mozc::strings::FrontChar;

// Max recursion count for looking up pending loop.
constexpr int kMaxRecursion = 4;

// Get from pending rules recursively
// The recursion will be stopped if recursion_count is 0.
// When returns false, the caller doesn't append result entries.
// If we have the rule,
// '1' -> '', 'あ'
// 'あ1' -> '', 'い'
// 'い1' -> '', 'う'
// 'う1' -> '', 'え'
// 'え1' -> '', 'お'
// 'お1' -> '', 'あ'
// 'あ*' -> '', '{*}ぁ'
// '{*}ぁ' -> '', '{*}あ'
// '{*}あ' -> '', '{*}ぁ'
// 'い*' -> '', '{*}ぃ'
// '{*}ぃ' -> '', '{*}い'
// '{*}い' -> '', '{*}ぃ'
// Here, we want to get '{*}あ' <-> '{*}ぁ' loop from the input, 'あ'
bool GetFromPending(const Table *table, const absl::string_view key,
                    int recursion_count, absl::btree_set<std::string> *result) {
  DCHECK(result);
  if (recursion_count == 0) {
    // Don't find the loop within the |recursion_count|.
    return false;
  }
  if (result->find(key) != result->end()) {
    // Found the entry that is already looked up.
    // Return true because we found the loop.
    return true;
  }
  result->emplace(key);

  std::vector<const Entry *> entries;
  table->LookUpPredictiveAll(key, &entries);
  for (const Entry *entry : entries) {
    if (!entry->result().empty()) {
      // skip rules with result, because this causes too many results.
      // for example, if we have
      //  'k' -> 'っ', 'k'
      //  'ka' -> 'か', ''
      // From the input 'k', this causes 'か', 'っ', 'っか', ...
      // So here we stop calling recursion.
      return false;
    }
    if (!GetFromPending(table, entry->pending(), recursion_count - 1, result)) {
      return false;
    }
  }
  return true;
}
}  // namespace

CharChunk::CharChunk(Transliterators::Transliterator transliterator,
                     const Table *table)
    : table_(table),
      transliterator_(transliterator),
      attributes_(NO_TABLE_ATTRIBUTE),
      local_length_cache_(std::string::npos) {
  DCHECK_NE(Transliterators::LOCAL, transliterator);
}

void CharChunk::Clear() {
  raw_.clear();
  conversion_.clear();
  pending_.clear();
  ambiguous_.clear();
  local_length_cache_ = std::string::npos;
}

size_t CharChunk::GetLength(Transliterators::Transliterator t12r) const {
  if (t12r == Transliterators::LOCAL &&
      local_length_cache_ != std::string::npos) {
    return local_length_cache_;
  }
  const std::string t13n = Transliterate(
      t12r, DeleteSpecialKeys(raw_), DeleteSpecialKeys(conversion_ + pending_));
  size_t length = Util::CharsLen(t13n);
  if (t12r == Transliterators::LOCAL) {
    local_length_cache_ = length;
  }
  return length;
}

void CharChunk::AppendResult(Transliterators::Transliterator t12r,
                             std::string *result) const {
  const std::string t13n =
      Transliterate(t12r, DeleteSpecialKeys(raw_),
                    DeleteSpecialKeys(absl::StrCat(conversion_, pending_)));
  result->append(t13n);
}

void CharChunk::AppendTrimedResult(Transliterators::Transliterator t12r,
                                   std::string *result) const {
  // Only determined value (e.g. |conversion_| only) is added.
  std::string converted = conversion_;
  if (!pending_.empty()) {
    size_t key_length = 0;
    bool fixed = false;
    const Entry *entry = table_->LookUpPrefix(pending_, &key_length, &fixed);
    if (entry != nullptr && entry->input() == entry->result()) {
      converted.append(entry->result());
    }
  }
  result->append(Transliterate(t12r, DeleteSpecialKeys(raw_),
                               DeleteSpecialKeys(converted)));
}

void CharChunk::AppendFixedResult(Transliterators::Transliterator t12r,
                                  std::string *result) const {
  std::string converted;
  if (!ambiguous_.empty()) {
    // Add the |ambiguous_| value as a fixed value.  |ambiguous_|
    // contains an undetermined result string like "ん" converted
    // from a single 'n'.
    converted = absl::StrCat(conversion_, ambiguous_);
  } else {
    // If |pending_| exists but |ambiguous_| does not exist,
    // |pending_| is appended.  If |ambiguous_| exists, the value of
    // |pending_| is usually equal to |ambiguous_| so it is not
    // appended.
    converted = absl::StrCat(conversion_, pending_);
  }
  result->append(Transliterate(t12r, DeleteSpecialKeys(raw_),
                               DeleteSpecialKeys(converted)));
}

// If we have the rule (roman),
// 1: 'ka' -> 'か', ''
// 2: 'ki' -> 'き', ''
// 3: 'ku' -> 'く', ''
// 4: 'kk' -> 'っ', 'k'
// From the input 'k', we want to correct 'k', 'か', 'き', 'く', 'っ'
// We don't expand for next k for rule 4, because it causes
// many useless looped results, like 'っか', 'っっか', 'っっ', etc
//
// If we have the input 'kk', we will get 'か', 'き', 'く', 'っ' from the
// pending 'k' of rule 4.
// With the result of AppendTrimedResult, 'っ', we can get
// 'っか', 'っき', 'っく', 'っっ' from this input.
//
// If we have the rule (kana),
// 'は゜' -> 'ぱ', ''
// 'は゛' -> 'ば', ''
// From the input 'は', we want 'は', 'ば', 'ぱ'
//
// For mobile rules,
// If we have the rule,
// '1' -> '', 'あ'
// 'あ1' -> '', 'い'
// 'い1' -> '', 'う'
// 'う1' -> '', 'え'
// 'え1' -> '', 'お'
// 'お1' -> '', 'あ'
// 'あ*' -> '', '{*}ぁ'
// '{*}ぁ' -> '', '{*}あ'
// '{*}あ' -> '', '{*}ぁ'
// 'い*' -> '', '{*}ぃ'
// '{*}ぃ' -> '', '{*}い'
// '{*}い' -> '', '{*}ぃ'
// From the input '1', we want 'あ', 'ぁ', not including 'い', 'ぃ', 'う', etc
//
// Actually, we don't need to recursive lookup for above two patterns.
//
// What we want to append here is the 'looped rule' in |kMaxRecursion| lookup.
// Here, '{*}ぁ' -> '{*}あ' -> '{*}ぁ' is the loop.
absl::btree_set<std::string> CharChunk::GetExpandedResults() const {
  absl::btree_set<std::string> results;
  if (pending_.empty()) {
    return results;
  }
  // Append current pending string
  if (conversion_.empty()) {
    results.insert(DeleteSpecialKeys(pending_));
  }
  std::vector<const Entry *> entries;
  table_->LookUpPredictiveAll(pending_, &entries);
  for (const Entry *entry : entries) {
    if (!entry->result().empty()) {
      results.insert(DeleteSpecialKeys(entry->result()));
    }
    if (entry->pending().empty()) {
      continue;
    }
    absl::btree_set<std::string> loop_result;
    if (!GetFromPending(table_, entry->pending(), kMaxRecursion,
                        &loop_result)) {
      continue;
    }
    for (const std::string &result : loop_result) {
      results.insert(DeleteSpecialKeys(result));
    }
  }
  return results;
}

bool CharChunk::IsFixed() const { return pending_.empty(); }

bool CharChunk::IsAppendable(Transliterators::Transliterator t12r,
                             const Table *table) const {
  return !pending_.empty() &&
         (t12r == Transliterators::LOCAL || t12r == transliterator_) &&
         table == table_;
}

bool CharChunk::IsConvertible(Transliterators::Transliterator t12r,
                              const Table *table,
                              const absl::string_view input) const {
  if (!IsAppendable(t12r, table)) {
    return false;
  }

  size_t key_length = 0;
  bool fixed = false;
  std::string key = absl::StrCat(pending_, input);
  const Entry *entry = table->LookUpPrefix(key, &key_length, &fixed);

  return entry && (key.size() == key_length) && fixed;
}

void CharChunk::Combine(const CharChunk &left_chunk) {
  conversion_ = left_chunk.conversion_ + conversion_;
  raw_ = left_chunk.raw_ + raw_;
  local_length_cache_ = std::string::npos;
  // TODO(komatsu): This is a hacky way.  We should look up the
  // conversion table with the new |raw_| value.
  if (left_chunk.ambiguous_.empty()) {
    ambiguous_.clear();
  } else {
    if (ambiguous_.empty()) {
      ambiguous_ = left_chunk.ambiguous_ + pending_;
    } else {
      ambiguous_ = left_chunk.ambiguous_ + ambiguous_;
    }
  }
  pending_ = left_chunk.pending_ + pending_;
}

std::pair<bool, absl::string_view> CharChunk::AddInputInternal(
    absl::string_view input) {
  constexpr bool kLoop = true;
  constexpr bool kNoLoop = false;

  size_t used_key_length = 0;
  bool fixed = false;
  std::string key = absl::StrCat(pending_, input);
  const Entry *entry = table_->LookUpPrefix(key, &used_key_length, &fixed);
  local_length_cache_ = std::string::npos;

  if (entry == nullptr) {
    if (used_key_length == 0) {
      // If `input` starts with a special key, erases it and keeps the loop.
      // For example, if `input` is "{!}ab{?}", `input` becomes "ab{?}".
      const absl::string_view trimmed = TrimLeadingSpecialKey(input);
      if (trimmed.size() < input.size()) {
        return {kLoop, trimmed};
      }

      // The prefix characters are not contained in the table, fallback
      // operation is performed.
      if (pending_.empty()) {
        // Conversion data was not found. Add one character to the chunk.
        const auto [front, rest] = FrontChar(input);
        absl::StrAppend(&raw_, front);
        absl::StrAppend(&conversion_, front);
        input = rest;
      }
      return {kNoLoop, input};
    }

    if (used_key_length == pending_.size()) {
      // If the `input` starts with a special key (e.g. "{!}"),
      // and it is not used for this chunk and the next chunk,
      // that special key is removed.
      size_t used_length = 0;
      bool next_fixed = false;
      const Entry *next_entry =
          table_->LookUpPrefix(input, &used_length, &next_fixed);
      const bool no_entry = (next_entry == nullptr && used_length == 0);
      const absl::string_view trimmed = TrimLeadingSpecialKey(input);
      if (no_entry && trimmed.size() < input.size()) {
        return {kLoop, trimmed};
      }
      return {kNoLoop, input};
    }

    if (used_key_length < pending_.size()) {
      // Do not modify this char_chunk, all key characters will be used
      // by the next char_chunk.
      return {kNoLoop, input};
    }

    // Some prefix character is contained in the table, but not
    // reached any conversion result (like "t" with "ta->た").
    // Conversion data had only pending.

    // Move used input characters to CharChunk data.
    DCHECK_GT(used_key_length, pending_.size());
    const size_t used_input_length = used_key_length - pending_.size();
    const absl::string_view used_input_chars =
        input.substr(0, used_input_length);
    absl::StrAppend(&raw_, used_input_chars);
    absl::StrAppend(&pending_, used_input_chars);
    ambiguous_.clear();
    return {kNoLoop, input.substr(used_input_length)};
  }

  // The prefix of key reached a conversion result, thus entry is not nullptr.

  // Check if this CharChunk does not contain a conversion result before.
  const bool is_first_entry =
      (conversion_.empty() &&
       (raw_.empty() || pending_.empty() || raw_ == pending_));
  // If this entry is the first entry, the attributes are applied to this chunk.
  if (is_first_entry) {
    attributes_ = entry->attributes();
  }

  // Move used input characters to raw_.
  const size_t used_input_length = used_key_length - pending_.size();
  absl::StrAppend(&raw_, input.substr(0, used_input_length));
  input = input.substr(used_input_length);

  if (fixed || key.size() > used_key_length) {
    // A result was found. Ambiguity is resolved because `fixed` is true or
    // the key still remains. e.g. If the key is "nk", "n" is used for "ん"
    // because the next remaining character "k" is not used with "n".
    conversion_.append(entry->result());
    pending_ = entry->pending();
    ambiguous_.clear();
  } else {
    // A result was found, but it is still ambiguous.
    // e.g. "n" with "n->ん and na->な".
    pending_ = key;
    ambiguous_ = entry->result();
  }

  // If the lookup is deterministically done (e.g. fixed is true and input is
  // empty) but the output is empty and the raw input is not used (e.g.
  // NO_TRANSLITERATION), raw_ is also cleared to make an empty chunk to be
  // removed by the caller (see: Composition::InsertInput).
  if (fixed && input.empty() && conversion_.empty() && pending_.empty() &&
      (attributes_ & NO_TRANSLITERATION)) {
    raw_.clear();
    return {kNoLoop, input};
  }

  if (input.empty() || pending_.empty()) {
    // If the remaining input character or pending character is empty,
    // there is no reason to continue the looping.
    return {kNoLoop, input};
  }

  return {kLoop, input};
}

void CharChunk::AddInput(std::string *input) {
  absl::string_view tmp = *input;
  bool loop = true;
  while (loop) {
    std::tie(loop, tmp) = AddInputInternal(tmp);
  }
  input->erase(0, input->size() - tmp.size());
}

void CharChunk::AddInputAndConvertedChar(CompositionInput *input) {
  local_length_cache_ = std::string::npos;

  if (input->is_asis()) {
    if (raw_.empty() && pending_.empty() && conversion_.empty()) {
      raw_ = input->raw();
      conversion_ = input->conversion();
      input->Clear();
    }
    return;
  }

  // If this chunk is empty, the raw and conversion are simply copied.
  if (raw_.empty() && pending_.empty() && conversion_.empty()) {
    raw_ = input->raw();
    input->clear_raw();
    pending_ = input->conversion();
    // TODO(komatsu): We should check if the `conversion` is
    // really ambiguous or not, otherwise the last character of the
    // preedit on Kana input is always dropped.
    ambiguous_ = input->conversion();
    input->clear_conversion();

    // If this entry is the first entry, the table attributes are
    // applied to this chunk.
    const Entry *entry = table_->LookUp(pending_);
    if (entry != nullptr) {
      attributes_ = entry->attributes();
    }
    return;
  }

  const std::string key_input = pending_ + input->conversion();
  size_t key_length = 0;
  bool fixed = false;
  const Entry *entry = table_->LookUpPrefix(key_input, &key_length, &fixed);
  if (entry == nullptr) {
    // Do not modify this char_chunk, all `raw` and `conversion`
    // values of `input` will be used by the next char_chunk.
    return;
  }

  // The whole input string was used.
  if (key_length == key_input.size()) {
    raw_.append(input->raw());
    if (fixed) {
      conversion_.append(entry->result());
      pending_ = entry->pending();
      ambiguous_.clear();
    } else {  // !fixed
      // |conversion_| remains the current value.
      pending_ = entry->result();
      ambiguous_ = entry->result();
    }
    input->clear_raw();
    input->clear_conversion();
    return;
  }

  // The following key_length == pending_.size() means the new `raw` and
  // and `conversion` of `input` does not affect at all.  Do not any thing here
  // and a new char_chunk will be made for `input`.
  if (key_length == pending_.size()) {
    return;
  }

  // The `input` is partially used.
  raw_.append(input->raw());
  conversion_.append(entry->result());
  pending_ = entry->pending();
  // While the whole `raw` is used in this char_chunk, the
  // `conversion` is separated to this char_chunk and the next
  // char_chunk.  This is not a preferred behavior, but there is no
  // better way to work around this limitation.
  input->clear_raw();
  input->set_conversion(key_input.substr(key_length));
}

bool CharChunk::ShouldCommit() const {
  return (attributes_ & DIRECT_INPUT) && pending_.empty();
}

bool CharChunk::ShouldInsertNewChunk(const CompositionInput &input) const {
  if (raw_.empty() && conversion_.empty() && pending_.empty()) {
    return false;
  }

  const bool is_new_input =
      input.is_new_input() || ((attributes_ & END_CHUNK) && pending_.empty());

  if (is_new_input && (table_->HasNewChunkEntry(input.raw()) ||
                       !table_->HasSubRules(input.raw()))) {
    // Such input which is not on the table is treated as NewChunk entry.
    return true;
  }
  return false;
}

void CharChunk::AddCompositionInput(CompositionInput *input) {
  local_length_cache_ = std::string::npos;
  if (!input->conversion().empty()) {
    AddInputAndConvertedChar(input);
    return;
  }

  if (ShouldInsertNewChunk(*input)) {
    return;
  }
  AddInput(input->mutable_raw());
}

void CharChunk::SetTransliterator(
    const Transliterators::Transliterator transliterator) {
  if (transliterator == Transliterators::LOCAL) {
    // LOCAL transliterator shouldn't be set as local transliterator.
    // Just ignore.
    return;
  }
  local_length_cache_ = std::string::npos;
  transliterator_ = transliterator;
}

void CharChunk::set_attributes(TableAttributes attributes) {
  attributes_ = attributes;
  local_length_cache_ = std::string::npos;
}

absl::StatusOr<CharChunk> CharChunk::SplitChunk(
    Transliterators::Transliterator t12r, const size_t position) {
  if (position <= 0 || position >= GetLength(t12r)) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid position: ", position));
  }

  local_length_cache_ = std::string::npos;
  std::string raw_lhs, raw_rhs, converted_lhs, converted_rhs;
  Transliterators::GetTransliterator(GetTransliterator(t12r))
      ->Split(position, DeleteSpecialKeys(raw_),
              DeleteSpecialKeys(conversion_ + pending_), &raw_lhs, &raw_rhs,
              &converted_lhs, &converted_rhs);

  CharChunk left_new_chunk(transliterator_, table_);
  left_new_chunk.set_raw(raw_lhs);
  set_raw(std::move(raw_rhs));

  if (converted_lhs.size() > conversion_.size()) {
    // [ conversion | pending ] => [ conv | pend#1 ] [ pend#2 ]
    std::string pending_lhs(converted_lhs, conversion_.size());
    left_new_chunk.set_conversion(conversion_);
    left_new_chunk.set_pending(std::move(pending_lhs));

    conversion_.clear();
    pending_ = std::move(converted_rhs);
    ambiguous_.clear();
  } else {
    // [ conversion | pending ] => [ conv#1 ] [ conv#2 | pending ]
    left_new_chunk.set_conversion(std::move(converted_lhs));
    // left_new_chunk->set_pending("");
    const size_t pending_pos = converted_rhs.size() - pending_.size();
    conversion_.assign(converted_rhs, 0, pending_pos);
    // pending_ = pending_;
  }
  return left_new_chunk;
}

Transliterators::Transliterator CharChunk::GetTransliterator(
    Transliterators::Transliterator transliterator) const {
  if (attributes_ & NO_TRANSLITERATION) {
    if (transliterator == Transliterators::LOCAL ||
        transliterator == Transliterators::HALF_ASCII ||
        transliterator == Transliterators::FULL_ASCII) {
      return Transliterators::CONVERSION_STRING;
    }
    return transliterator;
  }
  if (transliterator == Transliterators::LOCAL) {
    return transliterator_;
  }
  return transliterator;
}

std::string CharChunk::Transliterate(
    Transliterators::Transliterator transliterator, const absl::string_view raw,
    const absl::string_view converted) const {
  return Transliterators::GetTransliterator(GetTransliterator(transliterator))
      ->Transliterate(raw, converted);
}

}  // namespace composer
}  // namespace mozc
