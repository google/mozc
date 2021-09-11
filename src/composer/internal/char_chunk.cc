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

#include <set>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/util.h"
#include "composer/internal/composition_input.h"
#include "composer/internal/transliterators.h"
#include "composer/table.h"

namespace mozc {
namespace composer {

namespace {
// Max recursion count for looking up pending loop.
constexpr int kMaxRecursion = 4;

// Delete "end" from "target", if "target" ends with the "end".
bool DeleteEnd(const std::string &end, std::string *target) {
  const std::string::size_type rindex = target->rfind(end);
  if (rindex == std::string::npos) {
    return false;
  }
  target->erase(rindex);
  return true;
}

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
bool GetFromPending(const Table *table, const std::string &key,
                    int recursion_count, std::set<std::string> *result) {
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
  result->insert(key);

  std::vector<const Entry *> entries;
  table->LookUpPredictiveAll(key, &entries);
  for (size_t i = 0; i < entries.size(); ++i) {
    if (!entries[i]->result().empty()) {
      // skip rules with result, because this causes too many results.
      // for example, if we have
      //  'k' -> 'っ', 'k'
      //  'ka' -> 'か', ''
      // From the input 'k', this causes 'か', 'っ', 'っか', ...
      // So here we stop calling recursion.
      return false;
    }
    if (!GetFromPending(table, entries[i]->pending(), recursion_count - 1,
                        result)) {
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

CharChunk::CharChunk(const CharChunk &x) = default;
CharChunk &CharChunk::operator=(const CharChunk &x) = default;

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
  const std::string t13n =
      Transliterate(t12r, Table::DeleteSpecialKey(raw_),
                    Table::DeleteSpecialKey(conversion_ + pending_));
  size_t length = Util::CharsLen(t13n);
  if (t12r == Transliterators::LOCAL) {
    local_length_cache_ = length;
  }
  return length;
}

void CharChunk::AppendResult(Transliterators::Transliterator t12r,
                             std::string *result) const {
  const std::string t13n =
      Transliterate(t12r, Table::DeleteSpecialKey(raw_),
                    Table::DeleteSpecialKey(conversion_ + pending_));
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
  result->append(Transliterate(t12r, Table::DeleteSpecialKey(raw_),
                               Table::DeleteSpecialKey(converted)));
}

void CharChunk::AppendFixedResult(Transliterators::Transliterator t12r,
                                  std::string *result) const {
  std::string converted = conversion_;
  if (!ambiguous_.empty()) {
    // Add the |ambiguous_| value as a fixed value.  |ambiguous_|
    // contains an undetermined result string like "ん" converted
    // from a single 'n'.
    converted.append(ambiguous_);
  } else {
    // If |pending_| exists but |ambiguous_| does not exist,
    // |pending_| is appended.  If |ambiguous_| exists, the value of
    // |pending_| is usually equal to |ambiguous_| so it is not
    // appended.
    converted.append(pending_);
  }
  result->append(Transliterate(t12r, Table::DeleteSpecialKey(raw_),
                               Table::DeleteSpecialKey(converted)));
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
void CharChunk::GetExpandedResults(std::set<std::string> *results) const {
  DCHECK(results);

  if (pending_.empty()) {
    return;
  }
  // Append current pending string
  if (conversion_.empty()) {
    results->insert(Table::DeleteSpecialKey(pending_));
  }
  std::vector<const Entry *> entries;
  table_->LookUpPredictiveAll(pending_, &entries);
  for (size_t i = 0; i < entries.size(); ++i) {
    if (!entries[i]->result().empty()) {
      results->insert(Table::DeleteSpecialKey(entries[i]->result()));
    }
    if (entries[i]->pending().empty()) {
      continue;
    }
    std::set<std::string> loop_result;
    if (!GetFromPending(table_, entries[i]->pending(), kMaxRecursion,
                        &loop_result)) {
      continue;
    }
    for (std::set<std::string>::const_iterator itr = loop_result.begin();
         itr != loop_result.end(); ++itr) {
      results->insert(Table::DeleteSpecialKey(*itr));
    }
  }
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
                              const std::string &input) const {
  if (!IsAppendable(t12r, table)) {
    return false;
  }

  size_t key_length = 0;
  bool fixed = false;
  std::string key = pending_ + input;
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

bool CharChunk::AddInputInternal(std::string *input) {
  constexpr bool kNoLoop = false;

  size_t key_length = 0;
  bool fixed = false;
  std::string key = pending_ + *input;
  const Entry *entry = table_->LookUpPrefix(key, &key_length, &fixed);
  local_length_cache_ = std::string::npos;

  if (entry == nullptr) {
    if (key_length == 0) {
      // No prefix character is not contained in the table, fallback
      // operation is performed.
      if (pending_.empty()) {
        // Conversion data was not found.
        AddConvertedChar(input);
      }
      return kNoLoop;
    }

    if (key_length < pending_.size()) {
      // Do not modify this char_chunk, all key characters will be used
      // by the next char_chunk.
      return kNoLoop;
    }

    DCHECK_GE(key_length, pending_.size());
    // Some prefix character is contained in the table, but not
    // reached any conversion result (like "t" with "ta->た").
    key_length -= pending_.size();

    // Conversion data had only pending.
    const std::string new_pending_chars(*input, 0, key_length);
    raw_.append(new_pending_chars);
    pending_.append(new_pending_chars);
    if (!ambiguous_.empty()) {
      // If ambiguous_ has already a value, ambiguous_ is appended.
      // ex. "ny" makes ambiguous_ "んy", but "sh" leaves ambiguous_ empty.
      ambiguous_.append(new_pending_chars);
    }
    input->erase(0, key_length);
    return kNoLoop;
  }

  // The prefix of key reached a conversion result, thus entry is not nullptr.

  if (key.size() == key_length) {
    const bool is_following_entry =
        (!conversion_.empty() ||
         (!raw_.empty() && !pending_.empty() && raw_ != pending_));

    raw_.append(*input);
    input->clear();
    if (fixed) {
      // The whole key has been used, table lookup has reached a fixed
      // value.  It is a stable world. (like "ka->か", "q@->だ").
      conversion_.append(entry->result());
      pending_ = entry->pending();
      ambiguous_.clear();

      // If this entry is the first entry, the table attributes are
      // applied to this chunk.
      if (!is_following_entry) {
        attributes_ = entry->attributes();
      }
    } else {  // !fixed
      // The whole string of key reached a conversion result, but the
      // result is ambiguous (like "n" with "n->ん and na->な").
      pending_ = key;
      ambiguous_ = entry->result();
    }
    return kNoLoop;
  }

  // Delete pending_ from raw_ if matched.
  DeleteEnd(pending_, &raw_);

  // A result was found without any ambiguity.
  input->assign(key, key_length, key.size() - key_length);
  raw_.append(key, 0, key_length);
  conversion_.append(entry->result());
  pending_ = entry->pending();
  ambiguous_.clear();

  if (input->empty() || pending_.empty()) {
    // If the remaining input character or pending character is empty,
    // there is no reason to continue the looping.
    return kNoLoop;
  }

  constexpr bool kLoop = true;
  return kLoop;
}

void CharChunk::AddInput(std::string *input) {
  while (AddInputInternal(input)) {
  }
}

void CharChunk::AddConvertedChar(std::string *input) {
  // TODO(komatsu) Nice to make "string Util::PopOneChar(string *str);".
  const absl::string_view first_char = Util::Utf8SubString(*input, 0, 1);
  conversion_.append(first_char.data(), first_char.size());
  raw_.append(first_char.data(), first_char.size());
  Util::Utf8SubString(*input, 1, std::string::npos, input);
  local_length_cache_ = std::string::npos;
}

void CharChunk::AddInputAndConvertedChar(std::string *key,
                                         std::string *converted_char) {
  local_length_cache_ = std::string::npos;
  // If this chunk is empty, the key and converted_char are simply
  // copied.
  if (raw_.empty() && pending_.empty() && conversion_.empty()) {
    raw_ = *key;
    key->clear();
    pending_ = *converted_char;
    // TODO(komatsu): We should check if the |converted_char| is
    // really ambiguous or not, otherwise the last character of the
    // preedit on Kana input is always dropped.
    ambiguous_ = *converted_char;
    converted_char->clear();

    // If this entry is the first entry, the table attributes are
    // applied to this chunk.
    const Entry *entry = table_->LookUp(pending_);
    if (entry != nullptr) {
      attributes_ = entry->attributes();
    }
    return;
  }

  const std::string input = pending_ + *converted_char;
  size_t key_length = 0;
  bool fixed = false;
  const Entry *entry = table_->LookUpPrefix(input, &key_length, &fixed);
  if (entry == nullptr) {
    // Do not modify this char_chunk, all key and converted_char
    // values will be used by the next char_chunk.
    return;
  }

  // The whole input string was used.
  if (key_length == input.size()) {
    raw_.append(*key);
    if (fixed) {
      conversion_.append(entry->result());
      pending_ = entry->pending();
      ambiguous_.clear();
    } else {  // !fixed
      // |conversion_| remains the current value.
      pending_ = entry->result();
      ambiguous_ = entry->result();
    }
    key->clear();
    converted_char->clear();
    return;
  }

  // The following key_length == pending_.size() means the new key and
  // and converted_char does not affect at all.  Do not any thing here
  // and a new char_chunk will be made for the new key and
  // converted_char.
  if (key_length == pending_.size()) {
    return;
  }

  // The input is partially used.
  raw_.append(*key);
  conversion_.append(entry->result());
  pending_ = entry->pending();
  // While the whole key is used in this char_chunk, the
  // converted_char is separated to this char_chunk and the next
  // char_chunk.  This is not a preferred behavior, but there is no
  // better way to work around this limitation.
  key->clear();
  converted_char->assign(input, key_length, input.size() - key_length);
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
  if (input->has_conversion()) {
    AddInputAndConvertedChar(input->mutable_raw(), input->mutable_conversion());
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

void CharChunk::set_raw(const std::string &raw) {
  raw_ = raw;
  local_length_cache_ = std::string::npos;
}

void CharChunk::set_conversion(const std::string &conversion) {
  conversion_ = conversion;
  local_length_cache_ = std::string::npos;
}

void CharChunk::set_pending(const std::string &pending) {
  pending_ = pending;
  local_length_cache_ = std::string::npos;
}

void CharChunk::set_ambiguous(const std::string &ambiguous) {
  ambiguous_ = ambiguous;
  local_length_cache_ = std::string::npos;
}

void CharChunk::set_attributes(TableAttributes attributes) {
  attributes_ = attributes;
  local_length_cache_ = std::string::npos;
}

std::unique_ptr<CharChunk> CharChunk::SplitChunk(
    Transliterators::Transliterator t12r, const size_t position) {
  if (position <= 0 || position >= GetLength(t12r)) {
    LOG(WARNING) << "Invalid position: " << position;
    return nullptr;
  }

  local_length_cache_ = std::string::npos;
  std::string raw_lhs, raw_rhs, converted_lhs, converted_rhs;
  Transliterators::GetTransliterator(GetTransliterator(t12r))
      ->Split(position, Table::DeleteSpecialKey(raw_),
              Table::DeleteSpecialKey(conversion_ + pending_), &raw_lhs,
              &raw_rhs, &converted_lhs, &converted_rhs);

  auto left_new_chunk = absl::make_unique<CharChunk>(transliterator_, table_);
  left_new_chunk->set_raw(raw_lhs);
  set_raw(raw_rhs);

  if (converted_lhs.size() > conversion_.size()) {
    // [ conversion | pending ] => [ conv | pend#1 ] [ pend#2 ]
    const std::string pending_lhs(converted_lhs, conversion_.size());
    left_new_chunk->set_conversion(conversion_);
    left_new_chunk->set_pending(pending_lhs);

    conversion_.clear();
    pending_ = converted_rhs;
    ambiguous_.clear();
  } else {
    // [ conversion | pending ] => [ conv#1 ] [ conv#2 | pending ]
    left_new_chunk->set_conversion(converted_lhs);
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
    Transliterators::Transliterator transliterator, const std::string &raw,
    const std::string &converted) const {
  return Transliterators::GetTransliterator(GetTransliterator(transliterator))
      ->Transliterate(raw, converted);
}

}  // namespace composer
}  // namespace mozc
