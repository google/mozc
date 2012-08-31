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

#include "dictionary/user_dictionary_storage.h"

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/base.h"
#include "base/file_stream.h"
#include "base/logging.h"
#include "base/mutex.h"
#include "base/process_mutex.h"
#include "base/protobuf/coded_stream.h"
#include "base/protobuf/descriptor.h"
#include "base/protobuf/message.h"
#include "base/protobuf/protobuf.h"
#include "base/protobuf/repeated_field.h"
#include "base/protobuf/zero_copy_stream_impl.h"
#include "base/util.h"
#include "dictionary/user_dictionary_util.h"

namespace mozc {
namespace {
// Maximum number of dictionary entries per dictionary.
const size_t kMaxDictionaryNameSize =     300;

// 512MByte
// We expand the limit of serialized message from 64MB(default) to 512MB
const size_t kDefaultTotalBytesLimit = 512 << 20;

// If the last file size exceeds kDefaultWarningTotalBytesLimit,
// we show a warning dialog saying that "All words will not be
// saved correctly. Please make the dictionary size smaller"
const size_t kDefaultWarningTotalBytesLimit = 256 << 20;

const size_t kCloudSyncBytesLimit = 1 << 19;  // 0.5MB
// TODO(mukai): translate the name.
const char kSyncDictionaryName[] = "Sync Dictionary";

// "自動登録単語";
const char kAutoRegisteredDictionaryName[] =
  "\xE8\x87\xAA\xE5\x8B\x95\xE7\x99\xBB\xE9\x8C\xB2\xE5\x8D\x98\xE8\xAA\x9E";

}  // namespace

using ::mozc::user_dictionary::UserDictionaryCommandStatus;

UserDictionaryStorage::UserDictionaryStorage(const string &file_name)
    : file_name_(file_name),
      last_error_type_(USER_DICTIONARY_STORAGE_NO_ERROR),
      mutex_(new ProcessMutex(Util::Basename(file_name).c_str())) {}

UserDictionaryStorage::~UserDictionaryStorage() {
  UnLock();
}

const string &UserDictionaryStorage::filename() const {
  return file_name_;
}

bool UserDictionaryStorage::Exists() const {
  return Util::FileExists(file_name_);
}

bool UserDictionaryStorage::LoadInternal(bool run_migration) {
  InputFileStream ifs(file_name_.c_str(), ios::binary);
  if (!ifs) {
    if (Exists()) {
      LOG(ERROR) << file_name_ << " exists but cannot be opened.";
      last_error_type_ = UNKNOWN_ERROR;
    } else {
      LOG(ERROR) << file_name_ << " does not exist.";
      last_error_type_ = FILE_NOT_EXISTS;
    }
    return false;
  }

  // Increase the maximum capacity of file size
  // from 64MB (default) to 512MB.
  // This is a tentative bug fix for http://b/2498675
  // TODO(taku): we have to introduce a restriction to
  // the file size and let user know "import failure" if user
  // wants to use more than 512MB.
  mozc::protobuf::io::IstreamInputStream zero_copy_input(&ifs);
  mozc::protobuf::io::CodedInputStream decoder(&zero_copy_input);
  decoder.SetTotalBytesLimit(kDefaultTotalBytesLimit, -1);
  if (!ParseFromCodedStream(&decoder)) {
    LOG(ERROR) << "Failed to parse";
    if (!decoder.ConsumedEntireMessage() || !ifs.eof()) {
      LOG(ERROR) << "ParseFromStream failed: file seems broken";
      last_error_type_ = BROKEN_FILE;
      return false;
    }
  }

  // Maybe this is just older file format.
  // Note that the data in older format can be parsed "successfully,"
  // so that it is necessary to run migration code from the older format to
  // the newer format.
  if (run_migration) {
    if (!UserDictionaryUtil::ResolveUnknownFieldSet(this)) {
      LOG(ERROR) << "Failed to resolve older fields.";
      return false;
    }
  }

  return true;
}


bool UserDictionaryStorage::LoadAndUpdateSyncDictionaries(
    bool ensure_one_sync_dictionary_exists,
    bool remove_empty_sync_dictionaries,
    bool run_migration) {
  last_error_type_ = USER_DICTIONARY_STORAGE_NO_ERROR;

  bool result = false;

  // Check if the user dictionary exists or not.
  if (Exists()) {
    result = LoadInternal(run_migration);
  } else {
    // This is also an expected scenario: e.g., clean installation, unit tests.
    VLOG(1) << "User dictionary file has not been created.";
    last_error_type_ = FILE_NOT_EXISTS;
    result = false;
  }

  if (ensure_one_sync_dictionary_exists && !EnsureSyncDictionaryExists()) {
    return false;
  }

  if (remove_empty_sync_dictionaries) {
    RemoveUnusedSyncDictionariesIfExist();
  }

  // Check dictionary id here. if id is 0, assign random ID.
  for (int i = 0; i < dictionaries_size(); ++i) {
    const UserDictionary &dict = dictionaries(i);
    if (dict.id() == 0) {
      mutable_dictionaries(i)->set_id(
          UserDictionaryUtil::CreateNewDictionaryId(*this));
    }
  }

  return result;
}

namespace {

const bool kRunMigration = true;

}  // namespace

bool UserDictionaryStorage::Load() {
#ifdef ENABLE_CLOUD_SYNC
  // Create sync dictionary when cloud sync feature is available.
  return LoadAndUpdateSyncDictionaries(true, false, kRunMigration);
#else
  // Do not automatically create sync dictionary.
  // Remove empty sync dictionaries instead.
  return LoadAndUpdateSyncDictionaries(false, true, kRunMigration);
#endif  // ENABLE_CLOUD_SYNC
}

bool UserDictionaryStorage::LoadWithoutMigration() {
#ifdef ENABLE_CLOUD_SYNC
  // Create sync dictionary when cloud sync feature is available.
  return LoadAndUpdateSyncDictionaries(true, false, !kRunMigration);
#else
  // Do not automatically create sync dictionary.
  // Remove empty sync dictionaries instead.
  return LoadAndUpdateSyncDictionaries(false, true, !kRunMigration);
#endif  // ENABLE_CLOUD_SYNC
}

// TODO(peria): Clean up this method, as it isn't used from anywhere.
bool UserDictionaryStorage::LoadWithoutChangingSyncDictionary() {
  return LoadAndUpdateSyncDictionaries(false, false, kRunMigration);
}

bool UserDictionaryStorage::Save() {
  last_error_type_ = USER_DICTIONARY_STORAGE_NO_ERROR;

  if (!locked_) {
    LOG(ERROR) << "Dictionary is not locked. "
               << "Call Lock() before saving the dictionary";
    last_error_type_ = SYNC_FAILURE;
    return false;
  }

  size_t sync_binary_size_total = 0;
  for (size_t i = 0; i < dictionaries_size(); ++i) {
    const UserDictionary &dict = dictionaries(i);
    if (!dict.syncable()) {
      continue;
    }
    if (dict.entries_size() > UserDictionaryUtil::max_sync_entry_size()) {
      LOG(ERROR) << "Sync dictionary has too many entries";
      return false;
    }

    sync_binary_size_total += dict.SerializeAsString().size();
    if (sync_binary_size_total > kCloudSyncBytesLimit) {
      LOG(ERROR) << "Sync dictionary is too large";
      return false;
    }
  }

  return SaveCore();
}

namespace {

bool SerializeUserDictionaryStorageToOstream(
    const user_dictionary::UserDictionaryStorage &input_storage,
    ostream *stream) {
#ifdef OS_ANDROID
  // To keep memory usage low, we do not copy the input storage on mobile.
  // Fortunately, on mobile, we don't need to think about users who re-install
  // older version after a new version is installed. So, we don't need to
  // fill the deprecated field here.
  return input_storage.SerializeToOstream(stream);
#else
  // To support backward compatibility, we set deprecated field temporarily.
  // TODO(hidehiko): remove this after migration.
  user_dictionary::UserDictionaryStorage storage(input_storage);
  UserDictionaryUtil::FillDesktopDeprecatedPosField(&storage);
  return storage.SerializeToOstream(stream);
#endif  // OS_ANDROID
}

}  // namespace

bool UserDictionaryStorage::SaveCore() {
  last_error_type_ = USER_DICTIONARY_STORAGE_NO_ERROR;

  const string tmp_file_name = file_name_ + ".tmp";
  {
    OutputFileStream ofs(tmp_file_name.c_str(),
                         ios::out|ios::binary|ios::trunc);
    if (!ofs) {
      LOG(ERROR) << "cannot open file: " << tmp_file_name;
      last_error_type_ = SYNC_FAILURE;
      return false;
    }

    if (!SerializeUserDictionaryStorageToOstream(*this, &ofs)) {
      LOG(ERROR) << "SerializeToString failed";
      last_error_type_ = SYNC_FAILURE;
      return false;
    }

    if (static_cast<size_t>(ofs.tellp()) >= kDefaultWarningTotalBytesLimit) {
      LOG(ERROR) << "The file size exceeds " << kDefaultWarningTotalBytesLimit;
      // continue "AtomicRename"
      last_error_type_ = TOO_BIG_FILE_BYTES;
    }
  }

  if (!Util::AtomicRename(tmp_file_name, file_name_)) {
    LOG(ERROR) << "AtomicRename failed";
    last_error_type_ = SYNC_FAILURE;
    return false;
  }

  if (last_error_type_ == TOO_BIG_FILE_BYTES) {
    return false;
  }

  return true;
}

bool UserDictionaryStorage::Lock() {
  locked_ = mutex_->Lock();
  LOG_IF(ERROR, !locked_) << "Lock() failed";
  return locked_;
}

bool UserDictionaryStorage::UnLock() {
  mutex_->UnLock();
  locked_ = false;
  return true;
}

bool UserDictionaryStorage::ExportDictionary(
    uint64 dic_id, const string &file_name) {
  const int index = GetUserDictionaryIndex(dic_id);
  if (index < 0) {
    last_error_type_ = INVALID_DICTIONARY_ID;
    LOG(ERROR) << "Invalid dictionary id: " << dic_id;
    return false;
  }

  OutputFileStream ofs(file_name.c_str());
  if (!ofs) {
    last_error_type_ = EXPORT_FAILURE;
    LOG(ERROR) << "Cannot open export file: " << file_name;
    return false;
  }

  const UserDictionary &dic = dictionaries(index);
  for (size_t i = 0; i < dic.entries_size(); ++i) {
    const UserDictionaryEntry &entry = dic.entries(i);
    ofs << entry.key() << "\t"
        << entry.value() << "\t"
        << UserDictionaryUtil::GetStringPosType(entry.pos()) << "\t"
        << entry.comment() << endl;
  }

  return true;
}

bool UserDictionaryStorage::CreateDictionary(
    const string &dic_name, uint64 *new_dic_id) {
  UserDictionaryCommandStatus::Status status =
      UserDictionaryUtil::CreateDictionary(this, dic_name, new_dic_id);
  // Update last_error_type_
  switch (status) {
    case UserDictionaryCommandStatus::DICTIONARY_NAME_EMPTY:
      last_error_type_ = EMPTY_DICTIONARY_NAME;
      break;
    case UserDictionaryCommandStatus::DICTIONARY_NAME_TOO_LONG:
      last_error_type_ = TOO_LONG_DICTIONARY_NAME;
      break;
    case UserDictionaryCommandStatus
        ::DICTIONARY_NAME_CONTAINS_INVALID_CHARACTER:
      last_error_type_ = INVALID_CHARACTERS_IN_DICTIONARY_NAME;
      break;
    case UserDictionaryCommandStatus::DICTIONARY_NAME_DUPLICATED:
      last_error_type_ = DUPLICATED_DICTIONARY_NAME;
      break;
    case UserDictionaryCommandStatus::DICTIONARY_SIZE_LIMIT_EXCEEDED:
      last_error_type_ = TOO_MANY_DICTIONARIES;
      break;
    case UserDictionaryCommandStatus::UNKNOWN_ERROR:
      last_error_type_ = UNKNOWN_ERROR;
      break;
    default:
      last_error_type_ = USER_DICTIONARY_STORAGE_NO_ERROR;
      break;
  }

  return
      status == UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS;
}

bool UserDictionaryStorage::CopyDictionary(uint64 dic_id,
                                           const string &dic_name,
                                           uint64 *new_dic_id) {
  last_error_type_ = USER_DICTIONARY_STORAGE_NO_ERROR;

  if (!UserDictionaryStorage::IsValidDictionaryName(dic_name)) {
    LOG(ERROR) << "Invalid dictionary name is passed";
    return false;
  }

  if (UserDictionaryUtil::IsStorageFull(*this)) {
    last_error_type_ = TOO_MANY_DICTIONARIES;
    LOG(ERROR) << "too many dictionaries";
    return false;
  }

  if (new_dic_id == NULL) {
    last_error_type_ = UNKNOWN_ERROR;
    LOG(ERROR) << "new_dic_id is NULL";
    return false;
  }

  UserDictionary *dic = GetUserDictionary(dic_id);
  if (dic == NULL) {
    last_error_type_ = INVALID_DICTIONARY_ID;
    LOG(ERROR) << "Invalid dictionary id: " << dic_id;
    return false;
  }
  if (dic->syncable()) {
    LOG(ERROR) << "Cannot copy a sync dictionary.";
    return false;
  }

  UserDictionary *new_dic = add_dictionaries();
  new_dic->CopyFrom(*dic);

  *new_dic_id = UserDictionaryUtil::CreateNewDictionaryId(*this);
  dic->set_id(*new_dic_id);
  dic->set_name(dic_name);

  return true;
}

bool UserDictionaryStorage::DeleteDictionary(uint64 dic_id) {
  if (!UserDictionaryUtil::DeleteDictionary(this, dic_id, NULL, NULL)) {
    // Failed to delete dictionary.
    last_error_type_ = INVALID_DICTIONARY_ID;
    return false;
  }

  last_error_type_ = USER_DICTIONARY_STORAGE_NO_ERROR;
  return true;
}

bool UserDictionaryStorage::RenameDictionary(uint64 dic_id,
                                             const string &dic_name) {
  last_error_type_ = USER_DICTIONARY_STORAGE_NO_ERROR;

  if (!UserDictionaryStorage::IsValidDictionaryName(dic_name)) {
    LOG(ERROR) << "Invalid dictionary name is passed";
    return false;
  }

  UserDictionary *dic = GetUserDictionary(dic_id);
  if (dic == NULL) {
    last_error_type_ = INVALID_DICTIONARY_ID;
    LOG(ERROR) << "Invalid dictionary id: " << dic_id;
    return false;
  }

  // same name
  if (dic->name() == dic_name) {
    return true;
  }

  if (dic->syncable()) {
    last_error_type_ = INVALID_DICTIONARY_ID;
    LOG(ERROR) << "Renaming sync dictionary is not allowed.";
    return false;
  }

  for (int i = 0; i < dictionaries_size(); ++i) {
    if (dic_name == dictionaries(i).name()) {
      last_error_type_ = DUPLICATED_DICTIONARY_NAME;
      LOG(ERROR) << "duplicated dictionary name";
      return false;
    }
  }

  dic->set_name(dic_name);

  return true;
}

int UserDictionaryStorage::GetUserDictionaryIndex(uint64 dic_id) const {
  return UserDictionaryUtil::GetUserDictionaryIndexById(*this, dic_id);
}

bool UserDictionaryStorage::GetUserDictionaryId(const string &dic_name,
                                                uint64 *dic_id) {
  for (size_t i = 0; i < dictionaries_size(); ++i) {
    if (dic_name == dictionaries(i).name()) {
      *dic_id = dictionaries(i).id();
      return true;
    }
  }

  return false;
}

user_dictionary::UserDictionary *UserDictionaryStorage::GetUserDictionary(
    uint64 dic_id) {
  return UserDictionaryUtil::GetMutableUserDictionaryById(this, dic_id);
}

UserDictionaryStorage::UserDictionaryStorageErrorType
UserDictionaryStorage::GetLastError() const {
  return last_error_type_;
}

bool UserDictionaryStorage::EnsureSyncDictionaryExists() {
  if (CountSyncableDictionaries(this) > 0) {
    VLOG(1) << "storage already has a sync dictionary.";
    return true;
  }
  UserDictionary *dic = add_dictionaries();
  if (dic == NULL) {
    LOG(ERROR) << "cannot add a new dictionary.";
    return false;
  }

  dic->set_name(kSyncDictionaryName);
  dic->set_syncable(true);
  dic->set_id(UserDictionaryUtil::CreateNewDictionaryId(*this));

  return true;
}

void UserDictionaryStorage::RemoveUnusedSyncDictionariesIfExist() {
  if (CountSyncableDictionaries(this) == 0) {
    // Nothing to do.
    return;
  }

  vector<UserDictionaryStorage::UserDictionary> copied_dictionaries;
  for (size_t i = 0; i < dictionaries_size(); ++i) {
    const UserDictionary &dict = dictionaries(i);
    if (dict.syncable() && dict.entries_size() == 0) {
      continue;
    }
    copied_dictionaries.push_back(dict);
  }

  clear_dictionaries();

  for (size_t i = 0; i < copied_dictionaries.size(); ++i) {
    add_dictionaries()->CopyFrom(copied_dictionaries[i]);
  }
}

  // Add new entry to the auto registered dictionary.
bool UserDictionaryStorage::AddToAutoRegisteredDictionary(
    const string &key, const string &value, UserDictionary::PosType pos) {
  if (!Lock()) {
    LOG(ERROR) << "cannot lock the user dictionary storage";
    return false;
  }

  int auto_index = -1;
  for (int i = 0; i < dictionaries_size(); ++i) {
    if (dictionaries(i).name() == kAutoRegisteredDictionaryName) {
      auto_index = i;
      break;
    }
  }

  UserDictionary *dic = NULL;
  if (auto_index == -1) {
    if (UserDictionaryUtil::IsStorageFull(*this)) {
      last_error_type_ = TOO_MANY_DICTIONARIES;
      LOG(ERROR) << "too many dictionaries";
      UnLock();
      return false;
    }
    dic = add_dictionaries();
    dic->set_id(UserDictionaryUtil::CreateNewDictionaryId(*this));
    dic->set_name(kAutoRegisteredDictionaryName);
  } else {
    dic = mutable_dictionaries(auto_index);
  }

  if (dic == NULL) {
    LOG(ERROR) << "cannot add a new dictionary.";
    UnLock();
    return false;
  }

  if (dic->entries_size() >= max_entry_size()) {
    last_error_type_ = TOO_MANY_ENTRIES;
    LOG(ERROR) << "too many entries";
    UnLock();
    return false;
  }

  UserDictionaryEntry *entry = dic->add_entries();
  if (entry == NULL) {
    LOG(ERROR) << "cannot add new entry";
    UnLock();
    return false;
  }

  entry->set_key(key);
  entry->set_value(value);
  entry->set_pos(pos);
  entry->set_auto_registered(true);

  if (!Save()) {
    UnLock();
    LOG(ERROR) << "cannot save the user dictionary storage";
    return false;
  }

  UnLock();
  return true;
}

// static
int UserDictionaryStorage::CountSyncableDictionaries(
    const user_dictionary::UserDictionaryStorage *storage) {
  int num_syncable_dictionaries = 0;
  for (int i = 0; i < storage->dictionaries_size(); ++i) {
    const UserDictionary &dict = storage->dictionaries(i);
    if (dict.syncable()) {
      ++num_syncable_dictionaries;
    }
  }
  return num_syncable_dictionaries;
}

// static
size_t UserDictionaryStorage::max_entry_size() {
  return UserDictionaryUtil::max_entry_size();
}

// static
size_t UserDictionaryStorage::max_dictionary_size() {
  return UserDictionaryUtil::max_entry_size();
}

bool UserDictionaryStorage::IsValidDictionaryName(const string &name) {
  UserDictionaryCommandStatus::Status status =
      UserDictionaryUtil::ValidateDictionaryName(
          UserDictionaryStorage::default_instance(), name);

  // Update last_error_type_.
  switch (status) {
    // Succeeded case.
    case UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS:
      return true;

    // Failure cases.
    case UserDictionaryCommandStatus::DICTIONARY_NAME_EMPTY:
      last_error_type_ = EMPTY_DICTIONARY_NAME;
      return false;
    case UserDictionaryCommandStatus::DICTIONARY_NAME_TOO_LONG:
      last_error_type_ = TOO_LONG_DICTIONARY_NAME;
      return false;
    case UserDictionaryCommandStatus
        ::DICTIONARY_NAME_CONTAINS_INVALID_CHARACTER:
      last_error_type_ = INVALID_CHARACTERS_IN_DICTIONARY_NAME;
      return false;
    default:
      LOG(WARNING) << "Unknown status: " << status;
      return false;
  }
  // Should never reach here.
}

// static methods around sync dictionary properties.
size_t UserDictionaryStorage::max_sync_dictionary_size() {
  return UserDictionaryUtil::max_sync_dictionary_size();
}

size_t UserDictionaryStorage::max_sync_entry_size() {
  return UserDictionaryUtil::max_sync_entry_size();
}

size_t UserDictionaryStorage::max_sync_binary_size() {
  return kCloudSyncBytesLimit;
}

string UserDictionaryStorage::default_sync_dictionary_name() {
  return string(kSyncDictionaryName);
}

}  // namespace mozc
