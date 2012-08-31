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

#include "dictionary/user_dictionary_session.h"

#include <algorithm>

#include "base/base.h"
#include "base/logging.h"
#include "base/protobuf/protobuf.h"
#include "base/protobuf/repeated_field.h"
#include "dictionary/user_dictionary_importer.h"
#include "dictionary/user_dictionary_storage.h"
#include "dictionary/user_dictionary_storage.pb.h"
#include "dictionary/user_dictionary_util.h"

namespace mozc {
namespace user_dictionary {

using ::mozc::protobuf::RepeatedPtrField;

class UserDictionarySession::UndoCommand {
 public:
  virtual ~UndoCommand() {
  }
  virtual bool RunUndo(mozc::UserDictionaryStorage *storage) = 0;

 protected:
  UndoCommand() {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(UndoCommand);
};

namespace {
// Implement "undo commands" for each operations.

class UndoCreateDictionaryCommand : public UserDictionarySession::UndoCommand {
 public:
  UndoCreateDictionaryCommand() {
  }

  virtual bool RunUndo(mozc::UserDictionaryStorage *storage) {
    if (storage->dictionaries_size() == 0) {
      return false;
    }

    storage->mutable_dictionaries()->RemoveLast();
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(UndoCreateDictionaryCommand);
};

class UndoDeleteDictionaryCommand : public UserDictionarySession::UndoCommand {
 public:
  // This instance takes the ownership of the given dictionary.
  UndoDeleteDictionaryCommand(int index, UserDictionary *dictionary)
      : index_(index), dictionary_(dictionary) {
  }

  virtual bool RunUndo(mozc::UserDictionaryStorage *storage) {
    if (dictionary_.get() == NULL) {
      return false;
    }

    RepeatedPtrField<UserDictionary> *dictionaries =
        storage->mutable_dictionaries();
    dictionaries->AddAllocated(dictionary_.release());

    // Adjust the position of the reverted dictionary.
    rotate(dictionaries->pointer_begin() + index_,
           dictionaries->pointer_end() - 1,
           dictionaries->pointer_end());
    return true;
  }

 private:
  int index_;
  scoped_ptr<UserDictionary> dictionary_;

  DISALLOW_COPY_AND_ASSIGN(UndoDeleteDictionaryCommand);
};

class UndoDeleteDictionaryWithEnsuringNonEmptyStorageCommand
    : public UserDictionarySession::UndoCommand {
 public:
  // This instance takes the ownership of the given dictionary.
  explicit UndoDeleteDictionaryWithEnsuringNonEmptyStorageCommand(
      UserDictionary *dictionary)
      : dictionary_(dictionary) {
  }

  virtual bool RunUndo(mozc::UserDictionaryStorage *storage) {
    if (storage->dictionaries_size() != 1) {
      return false;
    }
    dictionary_->Swap(storage->mutable_dictionaries(0));
    return true;
  }

 private:
  scoped_ptr<UserDictionary> dictionary_;

  DISALLOW_COPY_AND_ASSIGN(
      UndoDeleteDictionaryWithEnsuringNonEmptyStorageCommand);
};

class UndoRenameDictionaryCommand : public UserDictionarySession::UndoCommand {
 public:
  UndoRenameDictionaryCommand(
      uint64 dictionary_id, const string &original_name)
      : dictionary_id_(dictionary_id), original_name_(original_name) {
  }

  virtual bool RunUndo(mozc::UserDictionaryStorage *storage) {
    UserDictionary *dictionary =
        UserDictionaryUtil::GetMutableUserDictionaryById(
            storage, dictionary_id_);
    if (dictionary == NULL) {
      return false;
    }

    dictionary->set_name(original_name_);
    return true;
  }

 private:
  uint64 dictionary_id_;
  string original_name_;

  DISALLOW_COPY_AND_ASSIGN(UndoRenameDictionaryCommand);
};

class UndoAddEntryCommand : public UserDictionarySession::UndoCommand {
 public:
  explicit UndoAddEntryCommand(uint64 dictionary_id)
      : dictionary_id_(dictionary_id) {
  }

  virtual bool RunUndo(mozc::UserDictionaryStorage *storage) {
    UserDictionary *dictionary =
        UserDictionaryUtil::GetMutableUserDictionaryById(
            storage, dictionary_id_);
    if (dictionary == NULL || dictionary->entries_size() == 0) {
      return false;
    }

    dictionary->mutable_entries()->RemoveLast();
    return true;
  }

 private:
  uint64 dictionary_id_;

  DISALLOW_COPY_AND_ASSIGN(UndoAddEntryCommand);
};

class UndoEditEntryCommand : public UserDictionarySession::UndoCommand {
 public:
  UndoEditEntryCommand(uint64 dictionary_id, int index,
                       const UserDictionary::Entry &original_entry)
      : dictionary_id_(dictionary_id), index_(index),
        original_entry_(original_entry) {
  }

  virtual bool RunUndo(mozc::UserDictionaryStorage *storage) {
    UserDictionary *dictionary =
        UserDictionaryUtil::GetMutableUserDictionaryById(
            storage, dictionary_id_);
    if (dictionary == NULL ||
        index_ < 0 || dictionary->entries_size() <= index_) {
      return false;
    }

    dictionary->mutable_entries(index_)->CopyFrom(original_entry_);
    return true;
  }

 private:
  uint64 dictionary_id_;
  int index_;
  UserDictionary::Entry original_entry_;

  DISALLOW_COPY_AND_ASSIGN(UndoEditEntryCommand);
};

struct DeleteEntryComparator {
  bool operator()(const pair<int, UserDictionary::Entry*> &entry1,
                  const pair<int, UserDictionary::Entry*> &entry2) {
    return entry1.first < entry2.first;
  }
};

class UndoDeleteEntryCommand : public UserDictionarySession::UndoCommand {
 public:
  // This instance takes the ownership of the given entries.
  UndoDeleteEntryCommand(
      uint64 dictionary_id,
      const vector<pair<int, UserDictionary::Entry*> > deleted_entries)
      : dictionary_id_(dictionary_id), deleted_entries_(deleted_entries) {
    sort(deleted_entries_.begin(), deleted_entries_.end(),
         DeleteEntryComparator());
  }
  virtual ~UndoDeleteEntryCommand() {
    for (size_t i = 0; i < deleted_entries_.size(); ++i) {
      delete deleted_entries_[i].second;
    }
  }

  virtual bool RunUndo(mozc::UserDictionaryStorage *storage) {
    UserDictionary *dictionary =
        UserDictionaryUtil::GetMutableUserDictionaryById(
            storage, dictionary_id_);
    if (dictionary == NULL) {
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
    vector<UserDictionary::Entry*> backup(
        entries->pointer_begin(), entries->pointer_end());
    while (entries->size() > 0) {
      entries->ReleaseLast();
    }

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
  uint64 dictionary_id_;
  vector<pair<int, UserDictionary::Entry*> > deleted_entries_;

  DISALLOW_COPY_AND_ASSIGN(UndoDeleteEntryCommand);
};

class UndoImportFromStringCommand : public UserDictionarySession::UndoCommand {
 public:
  UndoImportFromStringCommand(uint64 dictionary_id, int original_num_entries)
      : dictionary_id_(dictionary_id),
        original_num_entries_(original_num_entries) {
  }

  virtual bool RunUndo(mozc::UserDictionaryStorage *storage) {
    UserDictionary *dictionary =
        UserDictionaryUtil::GetMutableUserDictionaryById(
            storage, dictionary_id_);
    if (dictionary == NULL) {
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
  uint64 dictionary_id_;
  int original_num_entries_;

  DISALLOW_COPY_AND_ASSIGN(UndoImportFromStringCommand);
};

// The limit of the number of commands remembered by the session for undo.
const int kMaxUndoHistory = 30;

// The default name of a dictionary, which is created to ensure "non-empty"
// storage.
const char kDefaultDictionaryName[] = "user dictionary";

}  // namespace

UserDictionarySession::UserDictionarySession(const string &filepath)
    : storage_(new mozc::UserDictionaryStorage(filepath)),
      default_dictionary_name_(kDefaultDictionaryName) {
}
UserDictionarySession::~UserDictionarySession() {
  ClearUndoHistory();
}

// TODO(hidehiko) move this to header.
const UserDictionaryStorage &UserDictionarySession::storage() const {
  return *storage_;
}
mozc::UserDictionaryStorage *UserDictionarySession::mutable_storage() {
  return storage_.get();
}

UserDictionaryCommandStatus::Status
UserDictionarySession::SetDefaultDictionaryName(
    const string &dictionary_name) {
  // Validate the name for the default dictionary. The name is used to create
  // a dictionary "for an empty storage", so check the validity with the
  // default instance of UserDictionaryStorage.
  UserDictionaryCommandStatus::Status status =
      UserDictionaryUtil::ValidateDictionaryName(
          UserDictionaryStorage::default_instance(), dictionary_name);
  if (status == UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS) {
    default_dictionary_name_ = dictionary_name;
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
  if (storage_->Load()) {
    status = UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS;
  } else {
    switch (storage_->GetLastError()) {
      case mozc::UserDictionaryStorage::FILE_NOT_EXISTS:
        status = UserDictionaryCommandStatus::FILE_NOT_FOUND;
        break;
      case mozc::UserDictionaryStorage::BROKEN_FILE:
        status = UserDictionaryCommandStatus::INVALID_FILE_FORMAT;
        break;
      default:
        LOG(ERROR) << "Unknown error code: " << storage_->GetLastError();
        status = UserDictionaryCommandStatus::UNKNOWN_ERROR;
        break;
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
  ~ScopedUserDictionaryLocker() {
    if (is_locked_) {
      storage_->UnLock();
    }
  }

  bool is_locked() const { return is_locked_; }
 private:
  mozc::UserDictionaryStorage *storage_;
  bool is_locked_;

  DISALLOW_COPY_AND_ASSIGN(ScopedUserDictionaryLocker);
};
}  // namespace

UserDictionaryCommandStatus::Status UserDictionarySession::Save() {
  ScopedUserDictionaryLocker locker(storage_.get());
  if (!locker.is_locked()) {
    LOG(ERROR) << "Failed to take a lock.";
    return UserDictionaryCommandStatus::UNKNOWN_ERROR;
  }

  if (!storage_->Save()) {
    switch (storage_->GetLastError()) {
      case mozc::UserDictionaryStorage::TOO_BIG_FILE_BYTES:
        return UserDictionaryCommandStatus::FILE_SIZE_LIMIT_EXCEEDED;
        // TODO(hidehiko): Handle SYNC_FAILURE.
      default:
        LOG(ERROR) << "Unknown error code: " << storage_->GetLastError();
        return UserDictionaryCommandStatus::UNKNOWN_ERROR;
    }
    // Should never reach here.
  }

  return UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS;
}

UserDictionaryCommandStatus::Status UserDictionarySession::Undo() {
  if (undo_history_.empty()) {
    return UserDictionaryCommandStatus::NO_UNDO_HISTORY;
  }

  scoped_ptr<UndoCommand> undo_command(undo_history_.back());
  undo_history_.pop_back();
  return undo_command->RunUndo(storage_.get()) ?
      UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS :
      UserDictionaryCommandStatus::UNKNOWN_ERROR;
}

UserDictionaryCommandStatus::Status UserDictionarySession::CreateDictionary(
    const string &dictionary_name, uint64 *new_dictionary_id) {
  UserDictionaryCommandStatus::Status status =
      UserDictionaryUtil::CreateDictionary(
          storage_.get(), dictionary_name, new_dictionary_id);
  if (status == UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS) {
    AddUndoCommand(new UndoCreateDictionaryCommand);
  }
  return status;
}

UserDictionaryCommandStatus::Status UserDictionarySession::DeleteDictionary(
    uint64 dictionary_id) {
  return DeleteDictionaryInternal(dictionary_id, false);
}

UserDictionaryCommandStatus::Status
UserDictionarySession::DeleteDictionaryWithEnsuringNonEmptyStorage(
    uint64 dictionary_id) {
  return DeleteDictionaryInternal(dictionary_id, true);
}

UserDictionaryCommandStatus::Status
UserDictionarySession::DeleteDictionaryInternal(
    uint64 dictionary_id, bool ensure_non_empty_storage) {
  int original_index;
  UserDictionary *deleted_dictionary;
  if (!UserDictionaryUtil::DeleteDictionary(
          storage_.get(), dictionary_id,
          &original_index, &deleted_dictionary)) {
    // Failed to delete the dictionary.
    return UserDictionaryCommandStatus::UNKNOWN_DICTIONARY_ID;
  }

  if ((ensure_non_empty_storage && EnsureNonEmptyStorage())) {
    // The storage was empty.
    AddUndoCommand(new UndoDeleteDictionaryWithEnsuringNonEmptyStorageCommand(
        deleted_dictionary));
  } else {
    AddUndoCommand(
        new UndoDeleteDictionaryCommand(original_index, deleted_dictionary));
  }

  return UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS;
}

UserDictionaryCommandStatus::Status UserDictionarySession::RenameDictionary(
    uint64 dictionary_id, const string &dictionary_name) {
  string original_name;
  const UserDictionary *dictionary =
      UserDictionaryUtil::GetUserDictionaryById(*storage_, dictionary_id);
  if (dictionary != NULL) {
    // Note that if dictionary is null, it means the dictionary_id is invalid
    // so following RenameDictionary will fail, and error handling is done
    // in the following codes.
    original_name = dictionary->name();
  }

  if (!storage_->RenameDictionary(dictionary_id, dictionary_name)) {
    switch (storage_->GetLastError()) {
      case mozc::UserDictionaryStorage::EMPTY_DICTIONARY_NAME:
        return UserDictionaryCommandStatus::DICTIONARY_NAME_EMPTY;
      case mozc::UserDictionaryStorage::TOO_LONG_DICTIONARY_NAME:
        return UserDictionaryCommandStatus::DICTIONARY_NAME_TOO_LONG;
      case mozc::UserDictionaryStorage::INVALID_CHARACTERS_IN_DICTIONARY_NAME:
        return UserDictionaryCommandStatus
            ::DICTIONARY_NAME_CONTAINS_INVALID_CHARACTER;
      case mozc::UserDictionaryStorage::DUPLICATED_DICTIONARY_NAME:
        return UserDictionaryCommandStatus::DICTIONARY_NAME_DUPLICATED;
      case mozc::UserDictionaryStorage::INVALID_DICTIONARY_ID:
        return UserDictionaryCommandStatus::UNKNOWN_DICTIONARY_ID;
      default:
        LOG(ERROR) << "Unknown error code: " << storage_->GetLastError();
        return UserDictionaryCommandStatus::UNKNOWN_ERROR;
    }
    // Should never reach here.
  }

  AddUndoCommand(
      new UndoRenameDictionaryCommand(dictionary_id, original_name));
  return UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS;
}

UserDictionaryCommandStatus::Status UserDictionarySession::AddEntry(
    uint64 dictionary_id, const UserDictionary::Entry &entry) {
  UserDictionary *dictionary =
      UserDictionaryUtil::GetMutableUserDictionaryById(
          storage_.get(), dictionary_id);
  if (dictionary == NULL) {
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
  new_entry->CopyFrom(entry);
  UserDictionaryUtil::SanitizeEntry(new_entry);

  AddUndoCommand(new UndoAddEntryCommand(dictionary_id));
  return UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS;
}

UserDictionaryCommandStatus::Status UserDictionarySession::EditEntry(
    uint64 dictionary_id, int index, const UserDictionary::Entry &entry) {
  UserDictionary *dictionary =
      UserDictionaryUtil::GetMutableUserDictionaryById(
          storage_.get(), dictionary_id);
  if (dictionary == NULL) {
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
  AddUndoCommand(
      new UndoEditEntryCommand(dictionary_id, index, *target_entry));

  target_entry->CopyFrom(entry);
  UserDictionaryUtil::SanitizeEntry(target_entry);
  return UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS;
}

UserDictionaryCommandStatus::Status UserDictionarySession::DeleteEntry(
    uint64 dictionary_id, const vector<int> &index_list) {
  UserDictionary *dictionary =
      UserDictionaryUtil::GetMutableUserDictionaryById(
          storage_.get(), dictionary_id);
  if (dictionary == NULL) {
    return UserDictionaryCommandStatus::UNKNOWN_DICTIONARY_ID;
  }

  for (size_t i = 0; i < index_list.size(); ++i) {
    const int index = index_list[i];
    if (index < 0 || dictionary->entries_size() <= index) {
      return UserDictionaryCommandStatus::ENTRY_INDEX_OUT_OF_RANGE;
    }
  }

  vector<pair<int, UserDictionary::Entry*> > deleted_entries;
  deleted_entries.reserve(index_list.size());

  RepeatedPtrField<UserDictionary::Entry> *entries =
      dictionary->mutable_entries();
  UserDictionary::Entry **data = entries->mutable_data();
  for (size_t i = 0; i < index_list.size(); ++i) {
    const int index = index_list[i];

    deleted_entries.push_back(make_pair(index, data[index]));
    data[index] = NULL;
  }

  UserDictionary::Entry **tail = remove(
      data, data + entries->size(), static_cast<UserDictionary::Entry*>(NULL));
  const int remaining_size = tail - data;
  while (entries->size() > remaining_size) {
    entries->ReleaseLast();
  }

  AddUndoCommand(new UndoDeleteEntryCommand(dictionary_id, deleted_entries));
  return UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS;
}

UserDictionaryCommandStatus::Status UserDictionarySession::ImportFromString(
    uint64 dictionary_id, const string &data) {
  UserDictionary *dictionary =
      UserDictionaryUtil::GetMutableUserDictionaryById(
          storage_.get(), dictionary_id);
  if (dictionary == NULL) {
    return UserDictionaryCommandStatus::UNKNOWN_DICTIONARY_ID;
  }

  int original_num_entries = dictionary->entries_size();
  UserDictionaryCommandStatus::Status status =
      ImportFromStringInternal(dictionary, data);

  // Remember the command regardless of whether the importing is successfully
  // done or not, because ImportFromStringInternal updates the dictionary
  // always.
  AddUndoCommand(
      new UndoImportFromStringCommand(dictionary_id, original_num_entries));

  return status;
}

UserDictionaryCommandStatus::Status
UserDictionarySession::ImportFromStringInternal(
    UserDictionary* dictionary, const string &data) {
  UserDictionaryImporter::ErrorType import_result;
  {
    UserDictionaryImporter::StringTextLineIterator iter(data);
    import_result =
        UserDictionaryImporter::ImportFromTextLineIterator(
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
    const string &dictionary_name, const string &data,
    uint64 *new_dictionary_id) {
  UserDictionaryCommandStatus::Status status =
      UserDictionaryUtil::CreateDictionary(
          storage_.get(), dictionary_name, new_dictionary_id);
  if (status != UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS) {
    return status;
  }

  // We can use undo command for CreateDictionary here, too.
  AddUndoCommand(new UndoCreateDictionaryCommand);

  UserDictionary *dictionary =
      UserDictionaryUtil::GetMutableUserDictionaryById(
          storage_.get(), *new_dictionary_id);
  if (dictionary == NULL) {
    // The dictionary should be always found.
    return UserDictionaryCommandStatus::UNKNOWN_ERROR;
  }

  return ImportFromStringInternal(dictionary, data);
}

bool UserDictionarySession::EnsureNonEmptyStorage() {
  bool has_unsyncable_dictionary = false;
  for (int i = 0; i < storage_->dictionaries_size(); ++i) {
    if (!storage_->dictionaries(i).syncable()) {
      has_unsyncable_dictionary = true;
      break;
    }
  }

  if (has_unsyncable_dictionary) {
    // The storage already has at least one un-syncable dictionary.
    // Do nothing.
    return false;
  }

  // Creates a dictionary with the default name. Should never fail.
  uint64 new_dictionary_id;
  UserDictionaryCommandStatus::Status status =
      UserDictionaryUtil::CreateDictionary(
          storage_.get(), default_dictionary_name_, &new_dictionary_id);
  CHECK_EQ(
      status, UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS);
  return true;
}

void UserDictionarySession::ClearUndoHistory() {
  for (deque<UndoCommand *>::iterator iter = undo_history_.begin();
       iter != undo_history_.end(); ++iter) {
    delete *iter;
  }
  undo_history_.clear();
}

void UserDictionarySession::AddUndoCommand(UndoCommand *undo_command) {
  // To avoid OOM due to huge undo history, we limit the undo-able
  // command size by kMaxUndoHistory.
  while (undo_history_.size() >= kMaxUndoHistory) {
    delete undo_history_.front();
    undo_history_.pop_front();
  }

  undo_history_.push_back(undo_command);
}

}  // namespace user_dictionary
}  // namespace mozc
