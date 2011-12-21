// Copyright 2010-2011, Google Inc.
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

#include <algorithm>
#include <string>
#include <vector>

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#include "base/base.h"
#include "base/file_stream.h"
#include "base/mutex.h"
#include "base/process_mutex.h"
#include "base/protobuf/descriptor.h"
#include "base/protobuf/message.h"
#include "base/protobuf/protobuf.h"
#include "base/protobuf/repeated_field.h"
#include "base/util.h"

namespace mozc {
namespace {
// Maximum number of dictionary entries per dictionary.
const size_t kMaxEntrySize          = 1000000;
const size_t kMaxDictionarySize     =     100;
const size_t kMaxDictionaryNameSize =     300;

// 512MByte
// We expand the limit of serialized message from 64MB(default) to 512MB
const size_t kDefaultTotalBytesLimit = 512 << 20;

// If the last file size exceeds kDefaultWarningTotalBytesLimit,
// we show a warning dialog saying that "All words will not be
// saved correctly. Please make the dictionary size smaller"
const size_t kDefaultWarningTotalBytesLimit = 256 << 20;

const size_t kMaxSyncEntrySize = 10000;
const size_t kMaxSyncDictionarySize = 1;
const size_t kCloudSyncBytesLimit = 1 << 19;  // 0.5MB
// TODO(mukai): translate the name.
const char kSyncDictionaryName[] = "Sync Dictionary";

// Create Random ID for dictionary
uint64 CreateID() {
  uint64 id = 0;

  // dic_id == 0 is used as a magic number
  while (id == 0) {
    if (!Util::GetSecureRandomSequence(
            reinterpret_cast<char *>(&id), sizeof(id))) {
      LOG(ERROR) << "GetSecureRandomSequence() failed. use rand()";
      id = static_cast<uint64>(rand());
    }
  }

  return id;
}
}  // namespace

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

bool UserDictionaryStorage::LoadInternal() {
  InputFileStream ifs(file_name_.c_str(), ios::binary);
  if (!ifs) {
    LOG(ERROR) << "cannot open file: " << file_name_;
    last_error_type_ = FILE_NOT_EXISTS;
    return false;
  }

  // Increase the maximum capacity of file size
  // from 64MB (default) to 512MB.
  // This is a tentative bug fix for http://b/2498675
  // TODO(taku): we have to introduce a restriction to
  // the file size and let user know "import failure" if user
  // wants to use more than 512MB.
  google::protobuf::io::IstreamInputStream zero_copy_input(&ifs);
  google::protobuf::io::CodedInputStream decoder(&zero_copy_input);
  decoder.SetTotalBytesLimit(kDefaultTotalBytesLimit, -1);
  if (!ParseFromCodedStream(&decoder) ||
      !decoder.ConsumedEntireMessage() ||
      !ifs.eof()) {
    LOG(ERROR) << "ParseFromStream failed: file seems broken";
    last_error_type_ = BROKEN_FILE;
    return false;
  }

  return true;
}

bool UserDictionaryStorage::Load() {
  last_error_type_ = USER_DICTIONARY_STORAGE_NO_ERROR;

  bool result = LoadInternal();

  // Create sync dictionary when and only when the build target is official
  // build because sync feature is not supported on OSS Mozc.
#ifdef GOOGLE_JAPANESE_INPUT_BUILD
  if (!EnsureSyncDictionaryExists()) {
    return false;
  }
#endif  // GOOGLE_JAPANESE_INPUT_BUILD


  // Check dictionary id here. if id is 0, assign random ID.
  for (int i = 0; i < dictionaries_size(); ++i) {
    const UserDictionary &dict = dictionaries(i);
    if (dict.id() == 0) {
      mutable_dictionaries(i)->set_id(CreateID());
    }
  }

  return result;
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
    if (dict.entries_size() > kMaxSyncEntrySize) {
      LOG(ERROR) << "Sync dictionary has too many entries";
      return false;
    }

    sync_binary_size_total += dict.SerializeAsString().size();
    if (sync_binary_size_total > kCloudSyncBytesLimit) {
      LOG(ERROR) << "Sync dictionary is too large";
      return false;
    }
  }

  const string tmp_file_name = file_name_ + ".tmp";
  {
    OutputFileStream ofs(tmp_file_name.c_str(),
                         ios::out|ios::binary|ios::trunc);
    if (!ofs) {
      LOG(ERROR) << "cannot open file: " << tmp_file_name;
      last_error_type_ = SYNC_FAILURE;
      return false;
    }

    if (!SerializeToOstream(&ofs)) {
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
        << entry.pos() << "\t"
        << entry.comment() << endl;
  }

  return true;
}

bool UserDictionaryStorage::CreateDictionary(
    const string &dic_name, uint64 *new_dic_id) {
  last_error_type_ = USER_DICTIONARY_STORAGE_NO_ERROR;

  if (!UserDictionaryStorage::IsValidDictionaryName(dic_name)) {
    LOG(ERROR) << "Invalid dictionary name is passed";
    return false;
  }

  if (dictionaries_size() >= kMaxDictionarySize) {
    last_error_type_ = TOO_MANY_DICTIONARIES;
    LOG(ERROR) << "too many dictionaries";
    return false;
  }

  for (int i = 0; i < dictionaries_size(); ++i) {
    if (dic_name == dictionaries(i).name()) {
      last_error_type_ = DUPLICATED_DICTIONARY_NAME;
      LOG(ERROR) << "duplicated dictionary name";
      return false;
    }
  }

  if (new_dic_id == NULL) {
    last_error_type_ = UNKNOWN_ERROR;
    LOG(ERROR) << "new_dic_id is NULL";
    return false;
  }

  UserDictionary *dic = add_dictionaries();
  if (dic == NULL) {
    last_error_type_ = UNKNOWN_ERROR;
    LOG(ERROR) << "add_dictionaries() failed";
    return false;
  }

  *new_dic_id = CreateID();

  dic->set_id(*new_dic_id);
  dic->set_name(dic_name);
  dic->clear_entries();

  return true;
}

bool UserDictionaryStorage::CopyDictionary(uint64 dic_id,
                                           const string &dic_name,
                                           uint64 *new_dic_id) {
  last_error_type_ = USER_DICTIONARY_STORAGE_NO_ERROR;

  if (!UserDictionaryStorage::IsValidDictionaryName(dic_name)) {
    LOG(ERROR) << "Invalid dictionary name is passed";
    return false;
  }

  if (dictionaries_size() >= kMaxDictionarySize) {
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

  *new_dic_id = CreateID();
  dic->set_id(*new_dic_id);
  dic->set_name(dic_name);

  return true;
}

bool UserDictionaryStorage::DeleteDictionary(uint64 dic_id) {
  last_error_type_ = USER_DICTIONARY_STORAGE_NO_ERROR;

  const int delete_index = GetUserDictionaryIndex(dic_id);
  if (delete_index == -1) {
    last_error_type_ = INVALID_DICTIONARY_ID;
    LOG(ERROR) << "Invalid dictionary id: " << dic_id;
    return false;
  }

  // Do not delete sync dictionary.
  if (dictionaries(delete_index).syncable()) {
    last_error_type_ = INVALID_DICTIONARY_ID;
    LOG(ERROR) << "Cannot delete sync dictionary";
    return false;
  }

  google::protobuf::RepeatedPtrField<UserDictionary> *dics =
      mutable_dictionaries();

  UserDictionary **data = dics->mutable_data();
  for (int i = delete_index; i < dictionaries_size() - 1; ++i) {
    swap(data[i], data[i + 1]);
  }

  dics->RemoveLast();

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
  for (int i = 0; i < dictionaries_size(); ++i) {
    if (dic_id == dictionaries(i).id()) {
      return i;
    }
  }

  LOG(ERROR) << "Cannot find dictionary id: " << dic_id;
  return -1;
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

UserDictionaryStorage::UserDictionary *
UserDictionaryStorage::GetUserDictionary(uint64 dic_id) {
  const int index = GetUserDictionaryIndex(dic_id);
  if (index < 0) {
    LOG(ERROR) << "Invalid dictionary id: " << dic_id;
    return NULL;
  }

  return mutable_dictionaries(index);
}

UserDictionaryStorage::UserDictionaryStorageErrorType
UserDictionaryStorage::GetLastError() const {
  return last_error_type_;
}

bool UserDictionaryStorage::EnsureSyncDictionaryExists() {
  if (CountSyncableDictionaries(this) > 0) {
    LOG(INFO) << "storage already has a sync dictionary.";
    return true;
  }
  UserDictionary *dic = add_dictionaries();
  if (dic == NULL) {
    LOG(WARNING) << "cannot add a new dictionary.";
    return false;
  }

  dic->set_name(kSyncDictionaryName);
  dic->set_syncable(true);
  dic->set_id(CreateID());

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
  return kMaxEntrySize;
}

// static
size_t UserDictionaryStorage::max_dictionary_size() {
  return kMaxDictionarySize;
}

bool UserDictionaryStorage::IsValidDictionaryName(const string &name) {
  if (name.empty()) {
    VLOG(1) << "Empty dictionary name.";
    last_error_type_ = EMPTY_DICTIONARY_NAME;
    return false;
  } else if (name.size() > kMaxDictionaryNameSize) {
    last_error_type_ = TOO_LONG_DICTIONARY_NAME;
    VLOG(1) << "Too long dictionary name";
    return false;
  } else if (name.find_first_of("\n\r\t") != string::npos) {
    last_error_type_ = INVALID_CHARACTERS_IN_DICTIONARY_NAME;
    VLOG(1) << "Invalid character in dictionary name: " << name;
    return false;
  }
  return true;
}

// static methods around sync dictionary properties.
size_t UserDictionaryStorage::max_sync_dictionary_size() {
  return kMaxSyncDictionarySize;
}

size_t UserDictionaryStorage::max_sync_entry_size() {
  return kMaxSyncEntrySize;
}

size_t UserDictionaryStorage::max_sync_binary_size() {
  return kCloudSyncBytesLimit;
}

string UserDictionaryStorage::default_sync_dictionary_name() {
  return string(kSyncDictionaryName);
}
}  // namespace mozc
