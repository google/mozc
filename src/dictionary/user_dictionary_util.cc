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
#include <iterator>
#include <memory>
#include <string>

#include "absl/log/log.h"
#include "absl/random/random.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/string_view.h"
#include "base/config_file_stream.h"
#include "base/protobuf/repeated_ptr_field.h"
#include "base/strings/japanese.h"
#include "base/strings/unicode.h"
#include "base/vlog.h"
#include "dictionary/user_pos.h"
#include "protocol/user_dictionary_storage.pb.h"

namespace mozc {
namespace {

using ::mozc::protobuf::RepeatedPtrField;
using ::mozc::user_dictionary::UserDictionaryCommandStatus;

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
    const dictionary::UserPos &user_pos,
    const user_dictionary::UserDictionary::Entry &entry) {
  return ValidateEntry(entry) ==
         UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS;
}

namespace {

bool InRange(const char32_t c, const char32_t lo, const char32_t hi) {
  return lo <= c && c <= hi;
}

bool InternalValidateNormalizedReading(const absl::string_view reading) {
  for (const char32_t c : Utf8AsChars32(reading)) {
    if (!InRange(c, 0x0021, 0x007E) &&  // Basic Latin (Ascii)
        !InRange(c, 0x3041, 0x3096) &&  // Hiragana
        !InRange(c, 0x309B, 0x309C) &&  // KATAKANA-HIRAGANA VOICED/SEMI-VOICED
                                        // SOUND MARK
        !InRange(c, 0x30FB, 0x30FC) &&  // Nakaten, Prolonged sound mark
        !InRange(c, 0x3001, 0x3002) &&  // Japanese punctuation marks
        !InRange(c, 0x300C, 0x300F) &&  // Japanese brackets
        !InRange(c, 0x301C, 0x301C)) {  // Japanese Wavedash
      LOG(INFO) << "Invalid character in reading.";
      return false;
    }
  }
  return true;
}

}  // namespace

bool UserDictionaryUtil::IsValidReading(const absl::string_view reading) {
  return InternalValidateNormalizedReading(NormalizeReading(reading));
}

std::string UserDictionaryUtil::NormalizeReading(
    const absl::string_view input) {
  std::string tmp1 = japanese::FullWidthAsciiToHalfWidthAscii(input);
  std::string tmp2 = japanese::HalfWidthKatakanaToFullWidthKatakana(tmp1);
  return japanese::KatakanaToHiragana(tmp2);
}

UserDictionaryCommandStatus::Status UserDictionaryUtil::ValidateEntry(
    const user_dictionary::UserDictionary::Entry &entry) {
  // Validate reading.
  const std::string &reading = entry.key();
  if (reading.empty()) {
    MOZC_VLOG(1) << "key is empty";
    return UserDictionaryCommandStatus::READING_EMPTY;
  }
  if (reading.size() > kMaxKeySize) {
    MOZC_VLOG(1) << "Too long key.";
    return UserDictionaryCommandStatus::READING_TOO_LONG;
  }
  if (reading.find_first_of(kInvalidChars) != std::string::npos ||
      !strings::IsValidUtf8(reading)) {
    MOZC_VLOG(1) << "Invalid reading";
    return UserDictionaryCommandStatus::READING_CONTAINS_INVALID_CHARACTER;
  }

  // Validate word.
  const std::string &word = entry.value();
  if (word.empty()) {
    return UserDictionaryCommandStatus::WORD_EMPTY;
  }
  if (word.size() > kMaxValueSize) {
    MOZC_VLOG(1) << "Too long value.";
    return UserDictionaryCommandStatus::WORD_TOO_LONG;
  }
  if (word.find_first_of(kInvalidChars) != std::string::npos ||
      !strings::IsValidUtf8(word)) {
    MOZC_VLOG(1) << "Invalid character in value.";
    return UserDictionaryCommandStatus::WORD_CONTAINS_INVALID_CHARACTER;
  }

  // Validate comment.
  const std::string &comment = entry.comment();
  if (comment.size() > kMaxCommentSize) {
    MOZC_VLOG(1) << "Too long comment.";
    return UserDictionaryCommandStatus::COMMENT_TOO_LONG;
  }
  if (comment.find_first_of(kInvalidChars) != std::string::npos ||
      !strings::IsValidUtf8(comment)) {
    MOZC_VLOG(1) << "Invalid character in comment.";
    return UserDictionaryCommandStatus::COMMENT_CONTAINS_INVALID_CHARACTER;
  }

  // Validate pos.
  if (!entry.has_pos() ||
      !user_dictionary::UserDictionary::PosType_IsValid(entry.pos())) {
    MOZC_VLOG(1) << "Invalid POS";
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
  const int n = absl::StrReplaceAll(
      {
          {"\t", ""},
          {"\n", ""},
          {"\r", ""},
      },
      str);

  // Second part: Truncate long strings.
  if (str->size() <= max_size) {
    return n > 0;
  }
  const Utf8AsChars chars(*str);
  size_t pos = 0;
  for (auto it = chars.begin(); pos + it.size() <= max_size; ++it) {
    pos += it.size();
  }
  str->erase(pos);
  return true;
}

UserDictionaryCommandStatus::Status UserDictionaryUtil::ValidateDictionaryName(
    const user_dictionary::UserDictionaryStorage &storage,
    const absl::string_view dictionary_name) {
  if (dictionary_name.empty()) {
    MOZC_VLOG(1) << "Empty dictionary name.";
    return UserDictionaryCommandStatus::DICTIONARY_NAME_EMPTY;
  }
  if (dictionary_name.size() > kMaxDictionaryNameSize) {
    MOZC_VLOG(1) << "Too long dictionary name";
    return UserDictionaryCommandStatus::DICTIONARY_NAME_TOO_LONG;
  }
  if (dictionary_name.find_first_of(kInvalidChars) != std::string::npos) {
    MOZC_VLOG(1) << "Invalid character in dictionary name: " << dictionary_name;
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
constexpr absl::string_view kPosTypeStringTable[] = {
    "品詞なし",     "名詞",           "短縮よみ",     "サジェストのみ",
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

absl::string_view UserDictionaryUtil::GetStringPosType(
    user_dictionary::UserDictionary::PosType pos_type) {
  if (user_dictionary::UserDictionary::PosType_IsValid(pos_type)) {
    return kPosTypeStringTable[pos_type];
  }
  return {};
}

user_dictionary::UserDictionary::PosType UserDictionaryUtil::ToPosType(
    absl::string_view string_pos_type) {
  for (int i = 0; i < std::size(kPosTypeStringTable); ++i) {
    if (kPosTypeStringTable[i] == string_pos_type) {
      return static_cast<user_dictionary::UserDictionary::PosType>(i);
    }
  }

  // Not found. Return invalid value.
  return static_cast<user_dictionary::UserDictionary::PosType>(-1);
}

uint64_t UserDictionaryUtil::CreateNewDictionaryId(
    const user_dictionary::UserDictionaryStorage &storage) {
  static constexpr uint64_t kInvalidDictionaryId = 0;
  absl::BitGen gen;

  uint64_t id = kInvalidDictionaryId;
  while (id == kInvalidDictionaryId) {
    id = absl::Uniform<uint64_t>(gen);

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
    const absl::string_view dictionary_name, uint64_t *new_dictionary_id) {
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
  dictionary->set_name(std::string(dictionary_name));
  return UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS;
}

bool UserDictionaryUtil::DeleteDictionary(
    user_dictionary::UserDictionaryStorage *storage, uint64_t dictionary_id,
    int *original_index,
    std::unique_ptr<user_dictionary::UserDictionary> *deleted_dictionary) {
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
    deleted_dictionary->reset(dictionaries->ReleaseLast());
  }

  return true;
}
}  // namespace mozc
