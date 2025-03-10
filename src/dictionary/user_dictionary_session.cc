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

#include "dictionary/user_dictionary_session.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/optimization.h"
#include "absl/container/fixed_array.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "base/protobuf/repeated_ptr_field.h"
#include "base/strings/assign.h"
#include "dictionary/user_dictionary_importer.h"
#include "dictionary/user_dictionary_storage.h"
#include "dictionary/user_dictionary_util.h"
#include "protocol/user_dictionary_storage.pb.h"

namespace mozc {
namespace user_dictionary {
namespace {

using ::mozc::protobuf::RepeatedPtrField;

// Implement "undo commands" for each operations.

class UndoCreateDictionaryCommand : public UserDictionarySession::UndoCommand {
 public:
  bool RunUndo(mozc::UserDictionaryStorage *storage) override {
    if (storage->GetProto().dictionaries_size() == 0) {
      return false;
    }

    storage->GetProto().mutable_dictionaries()->RemoveLast();
    return true;
  }
};

class UndoDeleteDictionaryCommand : public UserDictionarySession::UndoCommand {
 public:
  // This instance takes the ownership of the given dictionary.
  UndoDeleteDictionaryCommand(int index,
                              std::unique_ptr<UserDictionary> dictionary)
      : index_(index), dictionary_(std::move(dictionary)) {}

  bool RunUndo(mozc::UserDictionaryStorage *storage) override {
    if (dictionary_ == nullptr) {
      return false;
    }

    RepeatedPtrField<UserDictionary> *dictionaries =
        storage->GetProto().mutable_dictionaries();
    dictionaries->AddAllocated(dictionary_.release());

    // Adjust the position of the reverted dictionary.
    std::rotate(dictionaries->pointer_begin() + index_,
                dictionaries->pointer_end() - 1, dictionaries->pointer_end());
    return true;
  }

 private:
  int index_;
  std::unique_ptr<UserDictionary> dictionary_;
};

class UndoDeleteDictionaryWithEnsuringNonEmptyStorageCommand
    : public UserDictionarySession::UndoCommand {
 public:
  // This instance takes the ownership of the given dictionary.
  explicit UndoDeleteDictionaryWithEnsuringNonEmptyStorageCommand(
      std::unique_ptr<UserDictionary> dictionary)
      : dictionary_(std::move(dictionary)) {}

  bool RunUndo(mozc::UserDictionaryStorage *storage) override {
    if (storage->GetProto().dictionaries_size() != 1) {
      return false;
    }
    dictionary_->Swap(storage->GetProto().mutable_dictionaries(0));
    return true;
  }

 private:
  std::unique_ptr<UserDictionary> dictionary_;
};

class UndoRenameDictionaryCommand : public UserDictionarySession::UndoCommand {
 public:
  UndoRenameDictionaryCommand(uint64_t dictionary_id,
                              absl::string_view original_name)
      : dictionary_id_(dictionary_id), original_name_(original_name) {}

  bool RunUndo(mozc::UserDictionaryStorage *storage) override {
    UserDictionary *dictionary =
        UserDictionaryUtil::GetMutableUserDictionaryById(&storage->GetProto(),
                                                         dictionary_id_);
    if (dictionary == nullptr) {
      return false;
    }

    dictionary->set_name(original_name_);
    return true;
  }

 private:
  uint64_t dictionary_id_;
  std::string original_name_;
};

class UndoAddEntryCommand : public UserDictionarySession::UndoCommand {
 public:
  explicit UndoAddEntryCommand(uint64_t dictionary_id)
      : dictionary_id_(dictionary_id) {}

  bool RunUndo(mozc::UserDictionaryStorage *storage) override {
    UserDictionary *dictionary =
        UserDictionaryUtil::GetMutableUserDictionaryById(&storage->GetProto(),
                                                         dictionary_id_);
    if (dictionary == nullptr || dictionary->entries_size() == 0) {
      return false;
    }

    dictionary->mutable_entries()->RemoveLast();
    return true;
  }

 private:
  uint64_t dictionary_id_;
};

class UndoEditEntryCommand : public UserDictionarySession::UndoCommand {
 public:
  UndoEditEntryCommand(uint64_t dictionary_id, int index,
                       const UserDictionary::Entry &original_entry)
      : dictionary_id_(dictionary_id),
        index_(index),
        original_entry_(original_entry) {}

  bool RunUndo(mozc::UserDictionaryStorage *storage) override {
    UserDictionary *dictionary =
        UserDictionaryUtil::GetMutableUserDictionaryById(&storage->GetProto(),
                                                         dictionary_id_);
    if (dictionary == nullptr || index_ < 0 ||
        dictionary->entries_size() <= index_) {
      return false;
    }

    *dictionary->mutable_entries(index_) = original_entry_;
    return true;
  }

 private:
  uint64_t dictionary_id_;
  int index_;
  UserDictionary::Entry original_entry_;
};

struct DeleteEntryComparator {
  bool operator()(const std::pair<int, UserDictionary::Entry *> &entry1,
                  const std::pair<int, UserDictionary::Entry *> &entry2) {
    return entry1.first < entry2.first;
  }
};

class UndoDeleteEntryCommand : public UserDictionarySession::UndoCommand {
 public:
  // This instance takes the ownership of the given entries.
  UndoDeleteEntryCommand(
      uint64_t dictionary_id,
      std::vector<std::pair<int, UserDictionary::Entry *>> deleted_entries)
      : dictionary_id_(dictionary_id),
        deleted_entries_(std::move(deleted_entries)) {
    std::sort(deleted_entries_.begin(), deleted_entries_.end(),
              DeleteEntryComparator());
  }

  ~UndoDeleteEntryCommand() override {
    for (size_t i = 0; i < deleted_entries_.size(); ++i) {
      delete deleted_entries_[i].second;
    }
  }

  bool RunUndo(mozc::UserDictionaryStorage *storage) override {
    UserDictionary *dictionary =
        UserDictionaryUtil::GetMutableUserDictionaryById(&storage->GetProto(),
                                                         dictionary_id_);
    if (dictionary == nullptr) {
      return false;
    }

    // Check validity of the holding indices.
    const int num_merged_entries =
        dictionary->entries_size() + deleted_entries_.size();
    for (size_t i = 0; i < deleted_entries_.size(); ++i) {
      const int index = deleted_entries_[i].first;
      if (index < 0 || num_merged_entries <= index) {
        return false;
      }
    }

    RepeatedPtrField<UserDictionary::Entry> *entries =
        dictionary->mutable_entries();

    // Move instances to backup vector.
    absl::FixedArray<UserDictionary::Entry *> backup(entries->size());
    entries->ExtractSubrange(0, entries->size(), backup.data());

    // Merge two vectors into entries.
    int backup_index = 0;
    int deleted_entry_index = 0;
    for (int index = 0; deleted_entry_index < deleted_entries_.size();
         ++index) {
      if (index == deleted_entries_[deleted_entry_index].first) {
        entries->AddAllocated(deleted_entries_[deleted_entry_index].second);
        ++deleted_entry_index;
      } else {
        entries->AddAllocated(backup[backup_index]);
        ++backup_index;
      }
    }

    // Add remaining entries in backup to the entries.
    for (; backup_index < backup.size(); ++backup_index) {
      entries->AddAllocated(backup[backup_index]);
    }

    // Release the entries.
    deleted_entries_.clear();
    return true;
  }

 private:
  uint64_t dictionary_id_;
  std::vector<std::pair<int, UserDictionary::Entry *>> deleted_entries_;
};

class UndoImportFromStringCommand : public UserDictionarySession::UndoCommand {
 public:
  UndoImportFromStringCommand(uint64_t dictionary_id, int original_num_entries)
      : dictionary_id_(dictionary_id),
        original_num_entries_(original_num_entries) {}

  bool RunUndo(mozc::UserDictionaryStorage *storage) override {
    UserDictionary *dictionary =
        UserDictionaryUtil::GetMutableUserDictionaryById(&storage->GetProto(),
                                                         dictionary_id_);
    if (dictionary == nullptr) {
      return false;
    }

    RepeatedPtrField<UserDictionary::Entry> *entries =
        dictionary->mutable_entries();
    while (original_num_entries_ < entries->size()) {
      entries->RemoveLast();
    }
    return true;
  }

 private:
  uint64_t dictionary_id_;
  int original_num_entries_;
};

// The limit of the number of commands remembered by the session for undo.
constexpr int kMaxUndoHistory = 30;

// The default name of a dictionary, which is created to ensure "non-empty"
// storage.
constexpr char kDefaultDictionaryName[] = "user dictionary";

}  // namespace

UserDictionarySession::UserDictionarySession(const std::string &filepath)
    : storage_(std::make_unique<mozc::UserDictionaryStorage>(filepath)),
      default_dictionary_name_(kDefaultDictionaryName) {}

// TODO(hidehiko) move this to header.
const UserDictionaryStorage &UserDictionarySession::storage() const {
  return storage_->GetProto();
}
mozc::UserDictionaryStorage *UserDictionarySession::mutable_storage() {
  return storage_.get();
}

UserDictionaryCommandStatus::Status
UserDictionarySession::SetDefaultDictionaryName(
    const absl::string_view dictionary_name) {
  // Validate the name for the default dictionary. The name is used to create
  // a dictionary "for an empty storage", so check the validity with the
  // default instance of UserDictionaryStorage.
  UserDictionaryCommandStatus::Status status =
      UserDictionaryUtil::ValidateDictionaryName(
          UserDictionaryStorage::default_instance(), dictionary_name);
  if (status == UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS) {
    strings::Assign(default_dictionary_name_, dictionary_name);
  }
  return status;
}

UserDictionaryCommandStatus::Status UserDictionarySession::Load() {
  return LoadInternal(false);
}

UserDictionaryCommandStatus::Status
UserDictionarySession::LoadWithEnsuringNonEmptyStorage() {
  return LoadInternal(true);
}

UserDictionaryCommandStatus::Status UserDictionarySession::LoadInternal(
    bool ensure_non_empty_storage) {
  UserDictionaryCommandStatus::Status status;
  absl::Status s = storage_->Load();

  if (s.ok()) {
    status = UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS;
  } else {
    LOG(ERROR) << "Load failed: " << s;
    if (absl::IsNotFound(s)) {
      status = UserDictionaryCommandStatus::FILE_NOT_FOUND;
    } else if (absl::IsDataLoss(s)) {
      status = UserDictionaryCommandStatus::INVALID_FILE_FORMAT;
    } else {
      status = UserDictionaryCommandStatus::UNKNOWN_ERROR;
    }
  }

  if ((ensure_non_empty_storage && EnsureNonEmptyStorage()) ||
      status == UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS) {
    // If the storage is updated, clear the undo history.
    ClearUndoHistory();
  }

  return status;
}

namespace {
// Locker of mozc::UserDictionaryStorage in RAII idiom.
class ScopedUserDictionaryLocker {
 public:
  explicit ScopedUserDictionaryLocker(mozc::UserDictionaryStorage *storage)
      : storage_(storage) {
    is_locked_ = storage_->Lock();
  }
  ScopedUserDictionaryLocker(const ScopedUserDictionaryLocker &) = delete;
  ScopedUserDictionaryLocker &operator=(const ScopedUserDictionaryLocker &) =
      delete;
  ~ScopedUserDictionaryLocker() {
    if (is_locked_) {
      storage_->UnLock();
    }
  }

  bool is_locked() const { return is_locked_; }

 private:
  mozc::UserDictionaryStorage *storage_;
  bool is_locked_;
};
}  // namespace

UserDictionaryCommandStatus::Status UserDictionarySession::Save() {
  ScopedUserDictionaryLocker locker(storage_.get());
  if (!locker.is_locked()) {
    LOG(ERROR) << "Failed to take a lock.";
    return UserDictionaryCommandStatus::UNKNOWN_ERROR;
  }

  if (absl::Status s = storage_->Save(); !s.ok()) {
    LOG(ERROR) << "Failed to save to storage: " << s;
    if (absl::IsResourceExhausted(s)) {
      return UserDictionaryCommandStatus::FILE_SIZE_LIMIT_EXCEEDED;
    }
    return UserDictionaryCommandStatus::UNKNOWN_ERROR;
  }

  return UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS;
}

UserDictionaryCommandStatus::Status UserDictionarySession::Undo() {
  if (undo_history_.empty()) {
    return UserDictionaryCommandStatus::NO_UNDO_HISTORY;
  }

  UndoCommand *undo_command = undo_history_.back().get();
  const UserDictionaryCommandStatus::Status result =
      undo_command->RunUndo(storage_.get())
          ? UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS
          : UserDictionaryCommandStatus::UNKNOWN_ERROR;
  undo_history_.pop_back();
  return result;
}

UserDictionaryCommandStatus::Status UserDictionarySession::CreateDictionary(
    const absl::string_view dictionary_name, uint64_t *new_dictionary_id) {
  UserDictionaryCommandStatus::Status status =
      UserDictionaryUtil::CreateDictionary(&storage_->GetProto(),
                                           dictionary_name, new_dictionary_id);
  if (status == UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS) {
    AddUndoCommand(std::make_unique<UndoCreateDictionaryCommand>());
  }
  return status;
}

UserDictionaryCommandStatus::Status UserDictionarySession::DeleteDictionary(
    uint64_t dictionary_id) {
  return DeleteDictionaryInternal(dictionary_id, false);
}

UserDictionaryCommandStatus::Status
UserDictionarySession::DeleteDictionaryWithEnsuringNonEmptyStorage(
    uint64_t dictionary_id) {
  return DeleteDictionaryInternal(dictionary_id, true);
}

UserDictionaryCommandStatus::Status
UserDictionarySession::DeleteDictionaryInternal(uint64_t dictionary_id,
                                                bool ensure_non_empty_storage) {
  int original_index;
  std::unique_ptr<UserDictionary> deleted_dictionary;
  if (!UserDictionaryUtil::DeleteDictionary(&storage_->GetProto(),
                                            dictionary_id, &original_index,
                                            &deleted_dictionary)) {
    // Failed to delete the dictionary.
    return UserDictionaryCommandStatus::UNKNOWN_DICTIONARY_ID;
  }

  if ((ensure_non_empty_storage && EnsureNonEmptyStorage())) {
    // The storage was empty.
    AddUndoCommand(std::make_unique<
                   UndoDeleteDictionaryWithEnsuringNonEmptyStorageCommand>(
        std::move(deleted_dictionary)));
  } else {
    AddUndoCommand(std::make_unique<UndoDeleteDictionaryCommand>(
        original_index, std::move(deleted_dictionary)));
  }

  return UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS;
}

UserDictionaryCommandStatus::Status UserDictionarySession::RenameDictionary(
    const uint64_t dictionary_id, const absl::string_view dictionary_name) {
  std::string original_name;
  const UserDictionary *dictionary = UserDictionaryUtil::GetUserDictionaryById(
      storage_->GetProto(), dictionary_id);
  if (dictionary != nullptr) {
    // Note that if dictionary is null, it means the dictionary_id is invalid
    // so following RenameDictionary will fail, and error handling is done
    // in the following codes.
    original_name = dictionary->name();
  }

  if (absl::Status s =
          storage_->RenameDictionary(dictionary_id, dictionary_name);
      !s.ok()) {
    switch (s.raw_code()) {
      case mozc::UserDictionaryStorage::EMPTY_DICTIONARY_NAME:
        return UserDictionaryCommandStatus::DICTIONARY_NAME_EMPTY;
      case mozc::UserDictionaryStorage::TOO_LONG_DICTIONARY_NAME:
        return UserDictionaryCommandStatus::DICTIONARY_NAME_TOO_LONG;
      case mozc::UserDictionaryStorage::INVALID_CHARACTERS_IN_DICTIONARY_NAME:
        return UserDictionaryCommandStatus ::
            DICTIONARY_NAME_CONTAINS_INVALID_CHARACTER;
      case mozc::UserDictionaryStorage::DUPLICATED_DICTIONARY_NAME:
        return UserDictionaryCommandStatus::DICTIONARY_NAME_DUPLICATED;
      case mozc::UserDictionaryStorage::INVALID_DICTIONARY_ID:
        return UserDictionaryCommandStatus::UNKNOWN_DICTIONARY_ID;
      default:
        LOG(ERROR) << "Unknown error code: " << s;
        return UserDictionaryCommandStatus::UNKNOWN_ERROR;
    }
    ABSL_UNREACHABLE();
  }

  AddUndoCommand(std::make_unique<UndoRenameDictionaryCommand>(dictionary_id,
                                                               original_name));
  return UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS;
}

UserDictionaryCommandStatus::Status UserDictionarySession::AddEntry(
    uint64_t dictionary_id, const UserDictionary::Entry &entry) {
  UserDictionary *dictionary = UserDictionaryUtil::GetMutableUserDictionaryById(
      &storage_->GetProto(), dictionary_id);
  if (dictionary == nullptr) {
    return UserDictionaryCommandStatus::UNKNOWN_DICTIONARY_ID;
  }

  if (UserDictionaryUtil::IsDictionaryFull(*dictionary)) {
    return UserDictionaryCommandStatus::ENTRY_SIZE_LIMIT_EXCEEDED;
  }

  const UserDictionaryCommandStatus::Status status =
      UserDictionaryUtil::ValidateEntry(entry);
  if (status != UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS) {
    // Invalid entry.
    return status;
  }

  UserDictionary::Entry *new_entry = dictionary->add_entries();
  *new_entry = entry;
  UserDictionaryUtil::SanitizeEntry(new_entry);

  AddUndoCommand(std::make_unique<UndoAddEntryCommand>(dictionary_id));
  return UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS;
}

UserDictionaryCommandStatus::Status UserDictionarySession::EditEntry(
    uint64_t dictionary_id, int index, const UserDictionary::Entry &entry) {
  UserDictionary *dictionary = UserDictionaryUtil::GetMutableUserDictionaryById(
      &storage_->GetProto(), dictionary_id);
  if (dictionary == nullptr) {
    return UserDictionaryCommandStatus::UNKNOWN_DICTIONARY_ID;
  }

  if (index < 0 || dictionary->entries_size() <= index) {
    return UserDictionaryCommandStatus::ENTRY_INDEX_OUT_OF_RANGE;
  }

  const UserDictionaryCommandStatus::Status status =
      UserDictionaryUtil::ValidateEntry(entry);
  if (status != UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS) {
    // Invalid entry.
    return status;
  }

  UserDictionary::Entry *target_entry = dictionary->mutable_entries(index);
  AddUndoCommand(std::make_unique<UndoEditEntryCommand>(dictionary_id, index,
                                                        *target_entry));

  *target_entry = entry;
  UserDictionaryUtil::SanitizeEntry(target_entry);
  return UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS;
}

UserDictionaryCommandStatus::Status UserDictionarySession::DeleteEntry(
    uint64_t dictionary_id, std::vector<int> index_list) {
  UserDictionary *dictionary = UserDictionaryUtil::GetMutableUserDictionaryById(
      &storage_->GetProto(), dictionary_id);
  if (dictionary == nullptr) {
    return UserDictionaryCommandStatus::UNKNOWN_DICTIONARY_ID;
  }

  for (size_t i = 0; i < index_list.size(); ++i) {
    const int index = index_list[i];
    if (index < 0 || dictionary->entries_size() <= index) {
      return UserDictionaryCommandStatus::ENTRY_INDEX_OUT_OF_RANGE;
    }
  }

  std::vector<std::pair<int, UserDictionary::Entry *>> deleted_entries;
  deleted_entries.reserve(index_list.size());

  // Sort these in descending order so the indices don't change as we remove
  // elements.
  std::sort(index_list.begin(), index_list.end(), std::greater<int>());

  RepeatedPtrField<UserDictionary::Entry> *entries =
      dictionary->mutable_entries();
  for (size_t i = 0; i < index_list.size(); ++i) {
    const int index = index_list[i];

    std::rotate(entries->pointer_begin() + index,
                entries->pointer_begin() + index + 1, entries->pointer_end());
    deleted_entries.push_back(std::make_pair(index, entries->ReleaseLast()));
  }

  AddUndoCommand(std::make_unique<UndoDeleteEntryCommand>(
      dictionary_id, std::move(deleted_entries)));
  return UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS;
}

UserDictionaryCommandStatus::Status UserDictionarySession::ImportFromString(
    const uint64_t dictionary_id, const absl::string_view data) {
  UserDictionary *dictionary = UserDictionaryUtil::GetMutableUserDictionaryById(
      &storage_->GetProto(), dictionary_id);
  if (dictionary == nullptr) {
    return UserDictionaryCommandStatus::UNKNOWN_DICTIONARY_ID;
  }

  int original_num_entries = dictionary->entries_size();
  UserDictionaryCommandStatus::Status status =
      ImportFromStringInternal(dictionary, data);

  // Remember the command regardless of whether the importing is successfully
  // done or not, because ImportFromStringInternal updates the dictionary
  // always.
  AddUndoCommand(std::make_unique<UndoImportFromStringCommand>(
      dictionary_id, original_num_entries));

  return status;
}

UserDictionaryCommandStatus::Status
UserDictionarySession::ImportFromStringInternal(UserDictionary *dictionary,
                                                const absl::string_view data) {
  UserDictionaryImporter::ErrorType import_result;
  {
    UserDictionaryImporter::StringTextLineIterator iter(data);
    import_result = UserDictionaryImporter::ImportFromTextLineIterator(
        UserDictionaryImporter::IME_AUTO_DETECT, &iter, dictionary);
  }

  LOG_IF(WARNING, import_result != UserDictionaryImporter::IMPORT_NO_ERROR)
      << "Import failed: " << import_result;

  // Return status code.
  switch (import_result) {
    case UserDictionaryImporter::IMPORT_NO_ERROR:
      // Succeeded.
      return UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS;

    // Failed on some reasons.
    case UserDictionaryImporter::IMPORT_TOO_MANY_WORDS:
      return UserDictionaryCommandStatus::IMPORT_TOO_MANY_WORDS;
    case UserDictionaryImporter::IMPORT_INVALID_ENTRIES:
      return UserDictionaryCommandStatus::IMPORT_INVALID_ENTRIES;
    default:
      LOG(ERROR) << "Unknown error: " << import_result;
      return UserDictionaryCommandStatus::UNKNOWN_ERROR;
  }
}

UserDictionaryCommandStatus::Status
UserDictionarySession::ImportToNewDictionaryFromString(
    const absl::string_view dictionary_name, const absl::string_view data,
    uint64_t *new_dictionary_id) {
  UserDictionaryCommandStatus::Status status =
      UserDictionaryUtil::CreateDictionary(&storage_->GetProto(),
                                           dictionary_name, new_dictionary_id);
  if (status != UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS) {
    return status;
  }

  // We can use undo command for CreateDictionary here, too.
  AddUndoCommand(std::make_unique<UndoCreateDictionaryCommand>());

  UserDictionary *dictionary = UserDictionaryUtil::GetMutableUserDictionaryById(
      &storage_->GetProto(), *new_dictionary_id);
  if (dictionary == nullptr) {
    // The dictionary should be always found.
    return UserDictionaryCommandStatus::UNKNOWN_ERROR;
  }

  return ImportFromStringInternal(dictionary, data);
}

bool UserDictionarySession::EnsureNonEmptyStorage() {
  if (storage_->GetProto().dictionaries_size() > 0) {
    // The storage already has at least one dictionary. Do nothing.
    return false;
  }

  // Creates a dictionary with the default name. Should never fail.
  uint64_t new_dictionary_id;
  UserDictionaryCommandStatus::Status status =
      UserDictionaryUtil::CreateDictionary(
          &storage_->GetProto(), default_dictionary_name_, &new_dictionary_id);
  CHECK_EQ(status,
           UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS);
  return true;
}

void UserDictionarySession::ClearUndoHistory() { undo_history_.clear(); }

void UserDictionarySession::AddUndoCommand(
    std::unique_ptr<UndoCommand> undo_command) {
  // To avoid OOM due to huge undo history, we limit the undo-able
  // command size by kMaxUndoHistory.
  while (undo_history_.size() >= kMaxUndoHistory) {
    undo_history_.pop_front();
  }

  undo_history_.push_back(std::move(undo_command));
}

void UserDictionarySession::ClearDictionariesAndUndoHistory() {
  ScopedUserDictionaryLocker l(storage_.get());
  storage_->GetProto().clear_dictionaries();
  ClearUndoHistory();
}

}  // namespace user_dictionary
}  // namespace mozc
