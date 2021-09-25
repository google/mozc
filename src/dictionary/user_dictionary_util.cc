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

#include "dictionary/user_dictionary_util.h"

#include <string.h>

#include <algorithm>
#include <cstdint>

#include "base/config_file_stream.h"
#include "base/file_stream.h"
#include "base/logging.h"
#include "base/protobuf/protobuf.h"
#include "base/util.h"
#include "dictionary/user_pos_interface.h"

namespace mozc {

using ::mozc::protobuf::RepeatedPtrField;
using ::mozc::user_dictionary::UserDictionaryCommandStatus;

namespace {
// Maximum string length in UserDictionaryEntry's field
constexpr size_t kMaxKeySize = 300;
constexpr size_t kMaxValueSize = 300;
constexpr size_t kMaxCommentSize = 300;
constexpr char kInvalidChars[] = "\n\r\t";
constexpr char kUserDictionaryFile[] = "user://user_dictionary.db";

// Maximum string length for dictionary name.
constexpr size_t kMaxDictionaryNameSize = 300;

// The limits of dictionary/entry size.
constexpr size_t kMaxDictionarySize = 100;
constexpr size_t kMaxEntrySize = 1000000;
}  // namespace

size_t UserDictionaryUtil::max_dictionary_size() { return kMaxDictionarySize; }

size_t UserDictionaryUtil::max_entry_size() { return kMaxEntrySize; }

bool UserDictionaryUtil::IsValidEntry(
    const UserPosInterface &user_pos,
    const user_dictionary::UserDictionary::Entry &entry) {
  return ValidateEntry(entry) ==
         UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS;
}

namespace {

#define INRANGE(w, a, b) ((w) >= (a) && (w) <= (b))

bool InternalValidateNormalizedReading(const std::string &reading) {
  for (ConstChar32Iterator iter(reading); !iter.Done(); iter.Next()) {
    const char32 c = iter.Get();
    if (!INRANGE(c, 0x0021, 0x007E) &&  // Basic Latin (Ascii)
        !INRANGE(c, 0x3041, 0x3096) &&  // Hiragana
        !INRANGE(c, 0x309B, 0x309C) &&  // KATAKANA-HIRAGANA VOICED/SEMI-VOICED
                                        // SOUND MARK
        !INRANGE(c, 0x30FB, 0x30FC) &&  // Nakaten, Prolonged sound mark
        !INRANGE(c, 0x3001, 0x3002) &&  // Japanese punctuation marks
        !INRANGE(c, 0x300C, 0x300F) &&  // Japanese brackets
        !INRANGE(c, 0x301C, 0x301C)) {  // Japanese Wavedash
      LOG(INFO) << "Invalid character in reading.";
      return false;
    }
  }
  return true;
}

#undef INRANGE

}  // namespace

bool UserDictionaryUtil::IsValidReading(const std::string &reading) {
  std::string normalized;
  NormalizeReading(reading, &normalized);
  return InternalValidateNormalizedReading(normalized);
}

void UserDictionaryUtil::NormalizeReading(const std::string &input,
                                          std::string *output) {
  output->clear();
  std::string tmp1, tmp2;
  Util::FullWidthAsciiToHalfWidthAscii(input, &tmp1);
  Util::HalfWidthKatakanaToFullWidthKatakana(tmp1, &tmp2);
  Util::KatakanaToHiragana(tmp2, output);
}

UserDictionaryCommandStatus::Status UserDictionaryUtil::ValidateEntry(
    const user_dictionary::UserDictionary::Entry &entry) {
  // Validate reading.
  const std::string &reading = entry.key();
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
  const std::string &word = entry.value();
  if (word.empty()) {
    return UserDictionaryCommandStatus::WORD_EMPTY;
  }
  if (word.size() > kMaxValueSize) {
    VLOG(1) << "Too long value.";
    return UserDictionaryCommandStatus::WORD_TOO_LONG;
  }
  if (word.find_first_of(kInvalidChars) != std::string::npos) {
    VLOG(1) << "Invalid character in value.";
    return UserDictionaryCommandStatus::WORD_CONTAINS_INVALID_CHARACTER;
  }

  // Validate comment.
  const std::string &comment = entry.comment();
  if (comment.size() > kMaxCommentSize) {
    VLOG(1) << "Too long comment.";
    return UserDictionaryCommandStatus::COMMENT_TOO_LONG;
  }
  if (comment.find_first_of(kInvalidChars) != std::string::npos) {
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
  return dictionary.entries_size() >= kMaxEntrySize;
}

const user_dictionary::UserDictionary *
UserDictionaryUtil::GetUserDictionaryById(
    const user_dictionary::UserDictionaryStorage &storage,
    uint64_t dictionary_id) {
  int index = GetUserDictionaryIndexById(storage, dictionary_id);
  return index >= 0 ? &storage.dictionaries(index) : nullptr;
}

user_dictionary::UserDictionary *
UserDictionaryUtil::GetMutableUserDictionaryById(
    user_dictionary::UserDictionaryStorage *storage, uint64_t dictionary_id) {
  int index = GetUserDictionaryIndexById(*storage, dictionary_id);
  return index >= 0 ? storage->mutable_dictionaries(index) : nullptr;
}

int UserDictionaryUtil::GetUserDictionaryIndexById(
    const user_dictionary::UserDictionaryStorage &storage,
    uint64_t dictionary_id) {
  for (int i = 0; i < storage.dictionaries_size(); ++i) {
    const user_dictionary::UserDictionary &dictionary = storage.dictionaries(i);
    if (dictionary.id() == dictionary_id) {
      return i;
    }
  }

  LOG(ERROR) << "Cannot find dictionary id: " << dictionary_id;
  return -1;
}

std::string UserDictionaryUtil::GetUserDictionaryFileName() {
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
bool UserDictionaryUtil::Sanitize(std::string *str, size_t max_size) {
  // First part: Remove invalid characters.
  {
    const size_t original_size = str->size();
    std::string::iterator begin = str->begin();
    std::string::iterator end = str->end();
    end = std::remove(begin, end, '\t');
    end = std::remove(begin, end, '\n');
    end = std::remove(begin, end, '\r');

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
    LOG(FATAL) << "There should be a bug in implementation of the function.";
  }

  return true;
}

UserDictionaryCommandStatus::Status UserDictionaryUtil::ValidateDictionaryName(
    const user_dictionary::UserDictionaryStorage &storage,
    const std::string &dictionary_name) {
  if (dictionary_name.empty()) {
    VLOG(1) << "Empty dictionary name.";
    return UserDictionaryCommandStatus::DICTIONARY_NAME_EMPTY;
  }
  if (dictionary_name.size() > kMaxDictionaryNameSize) {
    VLOG(1) << "Too long dictionary name";
    return UserDictionaryCommandStatus::DICTIONARY_NAME_TOO_LONG;
  }
  if (dictionary_name.find_first_of(kInvalidChars) != std::string::npos) {
    VLOG(1) << "Invalid character in dictionary name: " << dictionary_name;
    return UserDictionaryCommandStatus ::
        DICTIONARY_NAME_CONTAINS_INVALID_CHARACTER;
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
// element is nullptr.
const char *kPosTypeStringTable[] = {
    nullptr,        "名詞",           "短縮よみ",     "サジェストのみ",
    "固有名詞",     "人名",           "姓",           "名",
    "組織",         "地名",           "名詞サ変",     "名詞形動",
    "数",           "アルファベット", "記号",         "顔文字",

    "副詞",         "連体詞",         "接続詞",       "感動詞",
    "接頭語",       "助数詞",         "接尾一般",     "接尾人名",
    "接尾地名",     "動詞ワ行五段",   "動詞カ行五段", "動詞サ行五段",
    "動詞タ行五段", "動詞ナ行五段",   "動詞マ行五段", "動詞ラ行五段",
    "動詞ガ行五段", "動詞バ行五段",   "動詞ハ行四段", "動詞一段",
    "動詞カ変",     "動詞サ変",       "動詞ザ変",     "動詞ラ変",
    "形容詞",       "終助詞",         "句読点",       "独立語",
    "抑制単語",
};
}  // namespace

const char *UserDictionaryUtil::GetStringPosType(
    user_dictionary::UserDictionary::PosType pos_type) {
  if (user_dictionary::UserDictionary::PosType_IsValid(pos_type)) {
    return kPosTypeStringTable[pos_type];
  }
  return nullptr;
}

user_dictionary::UserDictionary::PosType UserDictionaryUtil::ToPosType(
    const char *string_pos_type) {
  // Skip the element at 0.
  for (int i = 1; i < std::size(kPosTypeStringTable); ++i) {
    if (strcmp(kPosTypeStringTable[i], string_pos_type) == 0) {
      return static_cast<user_dictionary::UserDictionary::PosType>(i);
    }
  }

  // Not found. Return invalid value.
  return static_cast<user_dictionary::UserDictionary::PosType>(-1);
}

uint64_t UserDictionaryUtil::CreateNewDictionaryId(
    const user_dictionary::UserDictionaryStorage &storage) {
  static constexpr uint64_t kInvalidDictionaryId = 0;

  uint64_t id = kInvalidDictionaryId;
  while (id == kInvalidDictionaryId) {
    Util::GetRandomSequence(reinterpret_cast<char *>(&id), sizeof(id));

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
    const std::string &dictionary_name, uint64_t *new_dictionary_id) {
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

  if (new_dictionary_id == nullptr) {
    LOG(ERROR) << "new_dictionary_id is nullptr";
    return UserDictionaryCommandStatus::UNKNOWN_ERROR;
  }

  *new_dictionary_id = CreateNewDictionaryId(*storage);
  user_dictionary::UserDictionary *dictionary = storage->add_dictionaries();
  if (dictionary == nullptr) {
    LOG(ERROR) << "add_dictionaries failed.";
    return UserDictionaryCommandStatus::UNKNOWN_ERROR;
  }

  dictionary->set_id(*new_dictionary_id);
  dictionary->set_name(dictionary_name);
  return UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS;
}

bool UserDictionaryUtil::DeleteDictionary(
    user_dictionary::UserDictionaryStorage *storage, uint64_t dictionary_id,
    int *original_index, user_dictionary::UserDictionary **deleted_dictionary) {
  const int index = GetUserDictionaryIndexById(*storage, dictionary_id);
  if (original_index != nullptr) {
    *original_index = index;
  }

  if (index < 0) {
    LOG(ERROR) << "Invalid dictionary id: " << dictionary_id;
    return false;
  }

  RepeatedPtrField<user_dictionary::UserDictionary> *dictionaries =
      storage->mutable_dictionaries();
  // Move the target dictionary to the end.
  std::rotate(dictionaries->pointer_begin() + index,
              dictionaries->pointer_begin() + index + 1,
              dictionaries->pointer_end());

  if (deleted_dictionary == nullptr) {
    dictionaries->RemoveLast();
  } else {
    *deleted_dictionary = dictionaries->ReleaseLast();
  }

  return true;
}

}  // namespace mozc
