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

#include "dictionary/user_dictionary_util.h"

#include <string.h>
#include <algorithm>

#include "base/base.h"
#include "base/config_file_stream.h"
#include "base/file_stream.h"
#include "base/logging.h"
#include "base/protobuf/message.h"
#include "base/protobuf/unknown_field_set.h"
#include "base/util.h"
#include "dictionary/user_pos_interface.h"

namespace mozc {

using ::mozc::protobuf::RepeatedPtrField;
using ::mozc::protobuf::UnknownField;
using ::mozc::protobuf::UnknownFieldSet;
using ::mozc::user_dictionary::UserDictionaryCommandStatus;

namespace {
// Maximum string length in UserDictionaryEntry's field
const size_t kMaxKeySize = 300;
const size_t kMaxValueSize = 300;
const size_t kMaxPOSSize = 300;
const size_t kMaxCommentSize = 300;
const char kInvalidChars[]= "\n\r\t";
const char kUserDictionaryFile[] = "user://user_dictionary.db";

// Maximum string length for dictionary name.
const size_t kMaxDictionaryNameSize = 300;

// The limits of dictionary/entry size.
const size_t kMaxDictionarySize = 100;
const size_t kMaxEntrySize = 1000000;
const size_t kMaxSyncDictionarySize = 1;
const size_t kMaxSyncEntrySize = 10000;
}  // namespace

size_t UserDictionaryUtil::max_dictionary_size() {
  return kMaxDictionarySize;
}

size_t UserDictionaryUtil::max_entry_size() {
  return kMaxEntrySize;
}

size_t UserDictionaryUtil::max_sync_dictionary_size() {
  return kMaxSyncDictionarySize;
}

size_t UserDictionaryUtil::max_sync_entry_size() {
  return kMaxSyncEntrySize;
}

bool UserDictionaryUtil::IsValidEntry(
    const UserPOSInterface &user_pos,
    const user_dictionary::UserDictionary::Entry &entry) {
  return ValidateEntry(entry) ==
      UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS;
}

namespace {

#define INRANGE(w, a, b) ((w) >= (a) && (w) <= (b))

bool InternalValidateNormalizedReading(const string &normalized_reading) {
  const char *begin = normalized_reading.c_str();
  const char *end = begin + normalized_reading.size();
  size_t mblen = 0;
  while (begin < end) {
    const uint16 w = Util::UTF8ToUCS2(begin, end, &mblen);
    if (INRANGE(w, 0x0021, 0x007E) ||  // Basic Latin (Ascii)
        INRANGE(w, 0x3041, 0x3096) ||  // Hiragana
        INRANGE(w, 0x309B, 0x309C) ||  // KATAKANA-HIRAGANA VOICED/SEMI-VOICED
                                       // SOUND MARK
        INRANGE(w, 0x30FB, 0x30FC) ||  // Nakaten, Prolonged sound mark
        INRANGE(w, 0x3001, 0x3002) ||  // Japanese punctuation marks
        INRANGE(w, 0x300C, 0x300F) ||  // Japanese brackets
        INRANGE(w, 0x301C, 0x301C)) {  // Japanese Wavedash
      begin += mblen;
    } else {
      LOG(INFO) << "Invalid character in reading.";
      return false;
    }
  }
  return true;
}

#undef INRANGE

}  // namespace

bool UserDictionaryUtil::IsValidReading(const string &reading) {
  string normalized;
  NormalizeReading(reading, &normalized);
  return InternalValidateNormalizedReading(normalized);
}

void UserDictionaryUtil::NormalizeReading(const string &input, string *output) {
  output->clear();
  string tmp1, tmp2;
  Util::FullWidthAsciiToHalfWidthAscii(input, &tmp1);
  Util::HalfWidthKatakanaToFullWidthKatakana(tmp1, &tmp2);
  Util::KatakanaToHiragana(tmp2, output);
}

UserDictionaryCommandStatus::Status UserDictionaryUtil::ValidateEntry(
    const user_dictionary::UserDictionary::Entry &entry) {
  // Validate reading.
  const string &reading = entry.key();
  if (reading.empty()) {
    VLOG(1) << "key is empty";
    return UserDictionaryCommandStatus::READING_EMPTY;
  }
  if (reading.size() > kMaxKeySize) {
    VLOG(1) << "Too long key.";
    return UserDictionaryCommandStatus::READING_TOO_LONG;
  }
  if (!IsValidReading(reading)) {
    VLOG(1) << "Invalid reading";
    return UserDictionaryCommandStatus::READING_CONTAINS_INVALID_CHARACTER;
  }

  // Validate word.
  const string &word = entry.value();
  if (word.empty()) {
    return UserDictionaryCommandStatus::WORD_EMPTY;
  }
  if (word.size() > kMaxValueSize) {
    VLOG(1) << "Too long value.";
    return UserDictionaryCommandStatus::WORD_TOO_LONG;
  }
  if (word.find_first_of(kInvalidChars) != string::npos) {
    VLOG(1) << "Invalid character in value.";
    return UserDictionaryCommandStatus::WORD_CONTAINS_INVALID_CHARACTER;
  }

  // Validate comment.
  const string &comment = entry.comment();
  if (comment.size() > kMaxCommentSize) {
    VLOG(1) << "Too long comment.";
    return UserDictionaryCommandStatus::COMMENT_TOO_LONG;
  }
  if (comment.find_first_of(kInvalidChars) != string::npos) {
    VLOG(1) << "Invalid character in comment.";
    return UserDictionaryCommandStatus::COMMENT_CONTAINS_INVALID_CHARACTER;
  }

  // Validate pos.
  if (!entry.has_pos() ||
      !user_dictionary::UserDictionary::PosType_IsValid(entry.pos())) {
    VLOG(1) << "Invalid POS";
    return UserDictionaryCommandStatus::INVALID_POS_TYPE;
  }

  return UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS;
}

bool UserDictionaryUtil::IsStorageFull(
    const user_dictionary::UserDictionaryStorage &storage) {
  return storage.dictionaries_size() >= kMaxDictionarySize;
}

bool UserDictionaryUtil::IsDictionaryFull(
    const user_dictionary::UserDictionary &dictionary) {
  const size_t max_entry_size = dictionary.syncable() ?
      kMaxSyncEntrySize : kMaxEntrySize;
  return dictionary.entries_size() >= max_entry_size;
}

const user_dictionary::UserDictionary *
UserDictionaryUtil::GetUserDictionaryById(
    const user_dictionary::UserDictionaryStorage &storage,
    uint64 dictionary_id) {
  int index = GetUserDictionaryIndexById(storage, dictionary_id);
  return index >= 0 ? &storage.dictionaries(index) : NULL;
}

user_dictionary::UserDictionary *
UserDictionaryUtil::GetMutableUserDictionaryById(
    user_dictionary::UserDictionaryStorage *storage, uint64 dictionary_id) {
  int index = GetUserDictionaryIndexById(*storage, dictionary_id);
  return index >= 0 ? storage->mutable_dictionaries(index) : NULL;
}

int UserDictionaryUtil::GetUserDictionaryIndexById(
    const user_dictionary::UserDictionaryStorage &storage,
    uint64 dictionary_id) {
  for (int i = 0; i < storage.dictionaries_size(); ++i) {
    const user_dictionary::UserDictionary &dictionary =
        storage.dictionaries(i);
    if (dictionary.id() == dictionary_id) {
      return i;
    }
  }

  LOG(ERROR) << "Cannot find dictionary id: " << dictionary_id;
  return -1;
}

string UserDictionaryUtil::GetUserDictionaryFileName() {
  return ConfigFileStream::GetFileName(kUserDictionaryFile);
}

// static
bool UserDictionaryUtil::SanitizeEntry(
    user_dictionary::UserDictionary::Entry *entry) {
  bool modified = false;
  modified |= Sanitize(entry->mutable_key(), kMaxKeySize);
  modified |= Sanitize(entry->mutable_value(), kMaxValueSize);
  if (!user_dictionary::UserDictionary::PosType_IsValid(entry->pos())) {
    // Fallback to NOUN.
    entry->set_pos(user_dictionary::UserDictionary::NOUN);
    modified = true;
  }
  modified |= Sanitize(entry->mutable_comment(), kMaxCommentSize);
  return modified;
}

// static
bool UserDictionaryUtil::Sanitize(string *str, size_t max_size) {
  // First part: Remove invalid characters.
  {
    const size_t original_size = str->size();
    string::iterator begin = str->begin();
    string::iterator end = str->end();
    end = remove(begin, end, '\t');
    end = remove(begin, end, '\n');
    end = remove(begin, end, '\r');

    if (end - begin <= max_size) {
      if (end - begin == original_size) {
        return false;
      } else {
        str->erase(end - begin);
        return true;
      }
    }
  }

  // Second part: Truncate long strings.
  {
    const char *begin = str->data();
    const char *p = begin;
    const char *end = begin + str->size();
    while (p < end) {
      const size_t len = Util::OneCharLen(p);
      if ((p + len - begin) > max_size) {
        str->erase(p - begin);
        return true;
      }
      p += len;
    }
    LOG(FATAL) <<
        "There should be a bug in implementation of the function.";
  }

  return true;
}

UserDictionaryCommandStatus::Status UserDictionaryUtil::ValidateDictionaryName(
    const user_dictionary::UserDictionaryStorage &storage,
    const string &dictionary_name) {
  if (dictionary_name.empty()) {
    VLOG(1) << "Empty dictionary name.";
    return UserDictionaryCommandStatus::DICTIONARY_NAME_EMPTY;
  }
  if (dictionary_name.size() > kMaxDictionaryNameSize) {
    VLOG(1) << "Too long dictionary name";
    return UserDictionaryCommandStatus::DICTIONARY_NAME_TOO_LONG;
  }
  if (dictionary_name.find_first_of(kInvalidChars) != string::npos) {
    VLOG(1) << "Invalid character in dictionary name: " << dictionary_name;
    return UserDictionaryCommandStatus
        ::DICTIONARY_NAME_CONTAINS_INVALID_CHARACTER;
  }
  for (int i = 0; i < storage.dictionaries_size(); ++i) {
    if (storage.dictionaries(i).name() == dictionary_name) {
      LOG(ERROR) << "duplicated dictionary name";
      return UserDictionaryCommandStatus::DICTIONARY_NAME_DUPLICATED;
    }
  }

  return UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS;
}

namespace {
// The index of each element should be matched with the actual value of enum.
// See also user_dictionary_storage.proto for the definition of the enum.
// Note that the '0' is invalid in the definition, so the corresponding
// element is NULL.
const char *kPosTypeStringTable[] = {
  NULL,
  "\xE5\x90\x8D\xE8\xA9\x9E",  // "名詞"
  "\xE7\x9F\xAD\xE7\xB8\xAE\xE3\x82\x88\xE3\x81\xBF",  // "短縮よみ"
  "\xE3\x82\xB5\xE3\x82\xB8\xE3\x82\xA7\xE3\x82\xB9\xE3\x83\x88"
      "\xE3\x81\xAE\xE3\x81\xBF",  // "サジェストのみ"
  "\xE5\x9B\xBA\xE6\x9C\x89\xE5\x90\x8D\xE8\xA9\x9E",  // "固有名詞"
  "\xE4\xBA\xBA\xE5\x90\x8D",  // "人名"
  "\xE5\xA7\x93",  // "姓"
  "\xE5\x90\x8D",  // "名"
  "\xE7\xB5\x84\xE7\xB9\x94",  // "組織"
  "\xE5\x9C\xB0\xE5\x90\x8D",  // "地名"
  "\xE5\x90\x8D\xE8\xA9\x9E\xE3\x82\xB5\xE5\xA4\x89",  // "名詞サ変"
  "\xE5\x90\x8D\xE8\xA9\x9E\xE5\xBD\xA2\xE5\x8B\x95",  // "名詞形動"
  "\xE6\x95\xB0",  // "数"
  "\xE3\x82\xA2\xE3\x83\xAB\xE3\x83\x95\xE3\x82\xA1"
      "\xE3\x83\x99\xE3\x83\x83\xE3\x83\x88",  // "アルファベット"
  "\xE8\xA8\x98\xE5\x8F\xB7",  // "記号"
  "\xE9\xA1\x94\xE6\x96\x87\xE5\xAD\x97",  // "顔文字"

  "\xE5\x89\xAF\xE8\xA9\x9E",  // "副詞"
  "\xE9\x80\xA3\xE4\xBD\x93\xE8\xA9\x9E",  // "連体詞"
  "\xE6\x8E\xA5\xE7\xB6\x9A\xE8\xA9\x9E",  // "接続詞"
  "\xE6\x84\x9F\xE5\x8B\x95\xE8\xA9\x9E",  // "感動詞"
  "\xE6\x8E\xA5\xE9\xA0\xAD\xE8\xAA\x9E",  // "接頭語"
  "\xE5\x8A\xA9\xE6\x95\xB0\xE8\xA9\x9E",  // "助数詞"
  "\xE6\x8E\xA5\xE5\xB0\xBE\xE4\xB8\x80\xE8\x88\xAC",  // "接尾一般"
  "\xE6\x8E\xA5\xE5\xB0\xBE\xE4\xBA\xBA\xE5\x90\x8D",  // "接尾人名"
  "\xE6\x8E\xA5\xE5\xB0\xBE\xE5\x9C\xB0\xE5\x90\x8D",  // "接尾地名"
  "\xE5\x8B\x95\xE8\xA9\x9E"
      "\xE3\x83\xAF\xE8\xA1\x8C\xE4\xBA\x94\xE6\xAE\xB5",  // "動詞ワ行五段"
  "\xE5\x8B\x95\xE8\xA9\x9E"
      "\xE3\x82\xAB\xE8\xA1\x8C\xE4\xBA\x94\xE6\xAE\xB5",  // "動詞カ行五段"
  "\xE5\x8B\x95\xE8\xA9\x9E"
      "\xE3\x82\xB5\xE8\xA1\x8C\xE4\xBA\x94\xE6\xAE\xB5",  // "動詞サ行五段"
  "\xE5\x8B\x95\xE8\xA9\x9E"
      "\xE3\x82\xBF\xE8\xA1\x8C\xE4\xBA\x94\xE6\xAE\xB5",  // "動詞タ行五段"
  "\xE5\x8B\x95\xE8\xA9\x9E"
      "\xE3\x83\x8A\xE8\xA1\x8C\xE4\xBA\x94\xE6\xAE\xB5",  // "動詞ナ行五段"
  "\xE5\x8B\x95\xE8\xA9\x9E"
      "\xE3\x83\x9E\xE8\xA1\x8C\xE4\xBA\x94\xE6\xAE\xB5",  // "動詞マ行五段"
  "\xE5\x8B\x95\xE8\xA9\x9E"
      "\xE3\x83\xA9\xE8\xA1\x8C\xE4\xBA\x94\xE6\xAE\xB5",  // "動詞ラ行五段"
  "\xE5\x8B\x95\xE8\xA9\x9E"
      "\xE3\x82\xAC\xE8\xA1\x8C\xE4\xBA\x94\xE6\xAE\xB5",  // "動詞ガ行五段"
  "\xE5\x8B\x95\xE8\xA9\x9E"
      "\xE3\x83\x90\xE8\xA1\x8C\xE4\xBA\x94\xE6\xAE\xB5",  // "動詞バ行五段"
  "\xE5\x8B\x95\xE8\xA9\x9E"
      "\xE3\x83\x8F\xE8\xA1\x8C\xE5\x9B\x9B\xE6\xAE\xB5",  // "動詞ハ行四段"
  "\xE5\x8B\x95\xE8\xA9\x9E\xE4\xB8\x80\xE6\xAE\xB5",  // "動詞一段"
  "\xE5\x8B\x95\xE8\xA9\x9E\xE3\x82\xAB\xE5\xA4\x89",  // "動詞カ変"
  "\xE5\x8B\x95\xE8\xA9\x9E\xE3\x82\xB5\xE5\xA4\x89",  // "動詞サ変"
  "\xE5\x8B\x95\xE8\xA9\x9E\xE3\x82\xB6\xE5\xA4\x89",  // "動詞ザ変"
  "\xE5\x8B\x95\xE8\xA9\x9E\xE3\x83\xA9\xE5\xA4\x89",  // "動詞ラ変"
  "\xE5\xBD\xA2\xE5\xAE\xB9\xE8\xA9\x9E",  // "形容詞"
  "\xE7\xB5\x82\xE5\x8A\xA9\xE8\xA9\x9E",  // "終助詞"
  "\xE5\x8F\xA5\xE8\xAA\xAD\xE7\x82\xB9",  // "句読点"
  "\xE7\x8B\xAC\xE7\xAB\x8B\xE8\xAA\x9E",  // "独立語"
  "\xE6\x8A\x91\xE5\x88\xB6\xE5\x8D\x98\xE8\xAA\x9E",  // "抑制単語"
};
}  // namespace

const char* UserDictionaryUtil::GetStringPosType(
    user_dictionary::UserDictionary::PosType pos_type) {
  if (user_dictionary::UserDictionary::PosType_IsValid(pos_type)) {
    return kPosTypeStringTable[pos_type];
  }
  return NULL;
}

user_dictionary::UserDictionary::PosType UserDictionaryUtil::ToPosType(
    const char *string_pos_type) {
  // Skip the element at 0.
  for (int i = 1; i < arraysize(kPosTypeStringTable); ++i) {
    if (strcmp(kPosTypeStringTable[i], string_pos_type) == 0) {
      return static_cast<user_dictionary::UserDictionary::PosType>(i);
    }
  }

  // Not found. Return invalid value.
  return static_cast<user_dictionary::UserDictionary::PosType>(-1);
}

namespace {

const UnknownField *GetUnknownFieldByTagNumber(
    const UnknownFieldSet &unknown_field_set, int tag_number) {
  for (int i = 0; i < unknown_field_set.field_count(); ++i) {
    const UnknownField &field = unknown_field_set.field(i);
    if (field.number() == tag_number) {
      // Use first entry.
      return &field;
    }
  }
  return NULL;
}

void RemoveUnknownFieldByTagNumber(
    int tag_number, UnknownFieldSet *unknown_field_set) {
  UnknownFieldSet temporary_unknown_field_set;
  for (int i = 0; i < unknown_field_set->field_count(); ++i) {
    const UnknownField &field = unknown_field_set->field(i);
    if (field.number() == tag_number) {
      continue;
    }
    temporary_unknown_field_set.AddField(field);
  }
  unknown_field_set->Swap(&temporary_unknown_field_set);
}

// The deprecated tag number of "pos" field in UserDictionary::Entry.
const int kDeprecatedPosTagNumber = 3;

}  // namespace

bool UserDictionaryUtil::ResolveUnknownFieldSet(
    user_dictionary::UserDictionaryStorage *storage) {
  using mozc::user_dictionary::UserDictionary;
  typedef UserDictionary::Entry Entry;

  static const UserDictionary::PosType kInvalidPosType =
      static_cast<UserDictionary::PosType>(-1);

  bool result = true;
  for (int i = 0; i < storage->dictionaries_size(); ++i) {
    UserDictionary *dictionary = storage->mutable_dictionaries(i);
    for (int j = 0; j < dictionary->entries_size(); ++j) {
      Entry *entry = dictionary->mutable_entries(j);

      const UnknownField *unknown_field = GetUnknownFieldByTagNumber(
          entry->unknown_fields(), kDeprecatedPosTagNumber);
      if (unknown_field == NULL) {
        // Here, there are two cases:
        // 1) The entry is already in the new format. Don't need migration.
        // 2) The entry doesn't have POS actually.
        // Note that it is "possible" (but not valid) for an entry to keep
        // its POS field empty. Do not treat it as "resolving failure."
        LOG_IF(WARNING, !entry->has_pos()) << "Unknown field is not found.";
        continue;
      }

      UserDictionary::PosType pos_type = kInvalidPosType;
      switch (unknown_field->type()) {
        case UnknownField::TYPE_VARINT:
          pos_type =
              static_cast<UserDictionary::PosType>(unknown_field->varint());
          break;
        case UnknownField::TYPE_LENGTH_DELIMITED:
          pos_type = ToPosType(unknown_field->length_delimited().c_str());
          break;
        default:
          LOG(ERROR) << "Unknown deprecated pos type value: "
                     << unknown_field->type();
          break;
      }

      if (pos_type == kInvalidPosType) {
        LOG(ERROR) << "Failed to resolve old pos data.";
        result = false;
        continue;
      }

      if (entry->has_pos()) {
        if (entry->pos() != pos_type) {
          LOG(ERROR) << "Failed to resolve the entry due to "
                     << "pos type inconsistency: "
                     << entry->pos() << ", " << pos_type;
          result = false;
          continue;
        }
      } else {
        entry->set_pos(pos_type);
      }

      // In future, we may want to add some fields into the message.
      // So, don't touch any fields other than ones we processed.
      UnknownFieldSet *unknown_field_set = entry->mutable_unknown_fields();
      RemoveUnknownFieldByTagNumber(
          kDeprecatedPosTagNumber, unknown_field_set);
      if (unknown_field_set->field_count() == 0) {
        entry->DiscardUnknownFields();
      }
    }
  }

  return result;
}

void UserDictionaryUtil::FillDesktopDeprecatedPosField(
    user_dictionary::UserDictionaryStorage *storage) {
  for (int i = 0; i < storage->dictionaries_size(); ++i) {
    user_dictionary::UserDictionary *dictionary =
        storage->mutable_dictionaries(i);
    for (int j = 0; j < dictionary->entries_size(); ++j) {
      user_dictionary::UserDictionary::Entry *entry =
          dictionary->mutable_entries(j);
      if (!entry->has_pos()) {
        // No pos is found, so don't need backward compatibility process.
        continue;
      }

      UnknownFieldSet *unknown_field_set = entry->mutable_unknown_fields();
      static const int kDeprecatedPosTagNumber = 3;
      unknown_field_set->AddLengthDelimited(
          kDeprecatedPosTagNumber,
          UserDictionaryUtil::GetStringPosType(entry->pos()));
    }
  }
}

uint64 UserDictionaryUtil::CreateNewDictionaryId(
    const user_dictionary::UserDictionaryStorage &storage) {
  static const uint64 kInvalidDictionaryId = 0;

  uint64 id = kInvalidDictionaryId;
  while (id == kInvalidDictionaryId) {
    if (!Util::GetSecureRandomSequence(
            reinterpret_cast<char *>(&id), sizeof(id))) {
      LOG(ERROR) << "GetSecureRandomSequence() failed. use random value.";
      id = static_cast<uint64>(Util::Random(RAND_MAX));
    }

    // Duplication check.
    for (int i = 0; i < storage.dictionaries_size(); ++i) {
      if (storage.dictionaries(i).id() == id) {
        // Duplicated id is found. So invalidate it to retry the generating.
        id = kInvalidDictionaryId;
        break;
      }
    }
  }

  return id;
}

UserDictionaryCommandStatus::Status UserDictionaryUtil::CreateDictionary(
    user_dictionary::UserDictionaryStorage *storage,
    const string &dictionary_name,
    uint64 *new_dictionary_id) {
  UserDictionaryCommandStatus::Status status =
      ValidateDictionaryName(*storage, dictionary_name);
  if (status != UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS) {
    LOG(ERROR) << "Invalid dictionary name is passed";
    return status;
  }

  if (IsStorageFull(*storage)) {
    LOG(ERROR) << "too many dictionaries";
    return UserDictionaryCommandStatus::DICTIONARY_SIZE_LIMIT_EXCEEDED;
  }

  if (new_dictionary_id == NULL) {
    LOG(ERROR) << "new_dictionary_id is NULL";
    return UserDictionaryCommandStatus::UNKNOWN_ERROR;
  }

  *new_dictionary_id = CreateNewDictionaryId(*storage);
  user_dictionary::UserDictionary* dictionary = storage->add_dictionaries();
  if (dictionary == NULL) {
    LOG(ERROR) << "add_dictionaries failed.";
    return UserDictionaryCommandStatus::UNKNOWN_ERROR;
  }

  dictionary->set_id(*new_dictionary_id);
  dictionary->set_name(dictionary_name);
  return UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS;
}

bool UserDictionaryUtil::DeleteDictionary(
    user_dictionary::UserDictionaryStorage *storage,
    uint64 dictionary_id,
    int *original_index,
    user_dictionary::UserDictionary **deleted_dictionary) {
  const int index = GetUserDictionaryIndexById(*storage, dictionary_id);
  if (original_index != NULL) {
    *original_index = index;
  }

  if (index < 0) {
    LOG(ERROR) << "Invalid dictionary id: " << dictionary_id;
    return false;
  }

  // Do not delete sync dictionary.
  if (storage->dictionaries(index).syncable()) {
    LOG(ERROR) << "Cannot delete sync dictionary";
    return false;
  }

  RepeatedPtrField<user_dictionary::UserDictionary> *dictionaries =
      storage->mutable_dictionaries();
  // Move the target dictionary to the end.
  rotate(dictionaries->pointer_begin() + index,
         dictionaries->pointer_begin() + index + 1,
         dictionaries->pointer_end());

  if (deleted_dictionary == NULL) {
    dictionaries->RemoveLast();
  } else {
    *deleted_dictionary = dictionaries->ReleaseLast();
  }

  return true;
}

}  // namespace mozc
