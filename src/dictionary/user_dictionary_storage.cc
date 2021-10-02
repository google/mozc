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

#include "dictionary/user_dictionary_storage.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/mutex.h"
#include "base/port.h"
#include "base/process_mutex.h"
#include "base/protobuf/zero_copy_stream_impl.h"
#include "base/util.h"
#include "dictionary/user_dictionary_util.h"
#include "absl/status/status.h"
#include "absl/strings/str_format.h"

namespace mozc {
namespace {

// 512MByte
// We expand the limit of serialized message from 64MB(default) to 512MB
constexpr size_t kDefaultTotalBytesLimit = 512 << 20;

// If the last file size exceeds kDefaultWarningTotalBytesLimit,
// we show a warning dialog saying that "All words will not be
// saved correctly. Please make the dictionary size smaller"
constexpr size_t kDefaultWarningTotalBytesLimit = 256 << 20;

constexpr char kDefaultSyncDictionaryName[] = "Sync Dictionary";
constexpr char kDictionaryNameConvertedFromSyncableDictionary[] = "同期用辞書";

using ::mozc::user_dictionary::UserDictionaryCommandStatus;

}  // namespace

UserDictionaryStorage::UserDictionaryStorage(const std::string &file_name)
    : file_name_(file_name),
      local_mutex_(new Mutex),
      process_mutex_(new ProcessMutex(FileUtil::Basename(file_name).c_str())) {}

UserDictionaryStorage::~UserDictionaryStorage() { UnLock(); }

const std::string &UserDictionaryStorage::filename() const {
  return file_name_;
}

bool UserDictionaryStorage::Exists() const {
  return FileUtil::FileExists(file_name_);
}

bool UserDictionaryStorage::LoadInternal() {
  InputFileStream ifs(file_name_.c_str(), std::ios::binary);
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
  decoder.SetTotalBytesLimit(kDefaultTotalBytesLimit);
  if (!proto_.ParseFromCodedStream(&decoder) ||
      !decoder.ConsumedEntireMessage() || !ifs.eof()) {
    LOG(ERROR) << "ParseFromStream failed: file seems broken";
    last_error_type_ = BROKEN_FILE;
    return false;
  }
  return true;
}

bool UserDictionaryStorage::Load() {
  last_error_type_ = USER_DICTIONARY_STORAGE_NO_ERROR;

  bool result = false;

  // Check if the user dictionary exists or not.
  if (Exists()) {
    result = LoadInternal();
  } else {
    // This is also an expected scenario: e.g., clean installation, unit tests.
    VLOG(1) << "User dictionary file has not been created.";
    last_error_type_ = FILE_NOT_EXISTS;
    result = false;
  }

  // Check dictionary id here. if id is 0, assign random ID.
  for (int i = 0; i < proto_.dictionaries_size(); ++i) {
    const UserDictionary &dict = proto_.dictionaries(i);
    if (dict.id() == 0) {
      proto_.mutable_dictionaries(i)->set_id(
          UserDictionaryUtil::CreateNewDictionaryId(proto_));
    }
  }

  return result;
}

absl::Status UserDictionaryStorage::Save() {
  last_error_type_ = USER_DICTIONARY_STORAGE_NO_ERROR;

  {
    scoped_lock l(local_mutex_.get());
    if (!locked_) {
      last_error_type_ = SYNC_FAILURE;
      return absl::FailedPreconditionError(
          "Must be locked before saving the dictionary (SYNC_FAILURE)");
    }
  }

  const std::string tmp_file_name = file_name_ + ".tmp";
  std::string size_error_msg;
  {
    OutputFileStream ofs(tmp_file_name.c_str(),
                         std::ios::out | std::ios::binary | std::ios::trunc);
    if (!ofs) {
      last_error_type_ = SYNC_FAILURE;
      return absl::PermissionDeniedError(absl::StrFormat(
          "Cannot open %s for write (SYNC_FAILURE)", tmp_file_name));
    }

    if (!proto_.SerializeToOstream(&ofs)) {
      last_error_type_ = SYNC_FAILURE;
      return absl::InternalError(
          absl::StrFormat("SerializeToOstream failed (SYNC_FAILURE); path = %s",
                          tmp_file_name));
    }

    const size_t file_size = ofs.tellp();

    ofs.close();
    if (ofs.fail()) {
      last_error_type_ = SYNC_FAILURE;
      return absl::UnknownError(
          absl::StrFormat("Failed to close %s (SYNC_FAILURE)", tmp_file_name));
    }

    if (file_size >= kDefaultWarningTotalBytesLimit) {
      size_error_msg = absl::StrFormat(
          "The file size exceeds the limit: size = %d, limit = %d", file_size,
          kDefaultWarningTotalBytesLimit);
      // Perform "AtomicRename" even if the size exceeded.
      last_error_type_ = TOO_BIG_FILE_BYTES;
    }
  }

  if (absl::Status s = FileUtil::AtomicRename(tmp_file_name, file_name_);
      !s.ok()) {
    std::string msg =
        absl::StrFormat("%s; Atomic rename from %s to %s failed (SYNC_FAILURE)",
                        s.message(), tmp_file_name, file_name_);
    if (last_error_type_ == TOO_BIG_FILE_BYTES) {
      msg.append("; ").append(size_error_msg);
    }
    last_error_type_ = SYNC_FAILURE;
    return absl::Status(s.code(), msg);
  }

  if (last_error_type_ == TOO_BIG_FILE_BYTES) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Save was successful with error (TOO_BIG_FILE_BYTES): %s",
        size_error_msg));
  }

  return absl::OkStatus();
}

bool UserDictionaryStorage::Lock() {
  scoped_lock l(local_mutex_.get());
  locked_ = process_mutex_->Lock();
  LOG_IF(ERROR, !locked_) << "Lock() failed";
  return locked_;
}

bool UserDictionaryStorage::UnLock() {
  scoped_lock l(local_mutex_.get());
  process_mutex_->UnLock();
  locked_ = false;
  return true;
}

bool UserDictionaryStorage::ExportDictionary(uint64_t dic_id,
                                             const std::string &file_name) {
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

  const UserDictionary &dic = proto_.dictionaries(index);
  for (size_t i = 0; i < dic.entries_size(); ++i) {
    const UserDictionaryEntry &entry = dic.entries(i);
    ofs << entry.key() << "\t" << entry.value() << "\t"
        << UserDictionaryUtil::GetStringPosType(entry.pos()) << "\t"
        << entry.comment() << std::endl;
  }

  return true;
}

bool UserDictionaryStorage::CreateDictionary(const std::string &dic_name,
                                             uint64_t *new_dic_id) {
  UserDictionaryCommandStatus::Status status =
      UserDictionaryUtil::CreateDictionary(&proto_, dic_name, new_dic_id);
  // Update last_error_type_
  switch (status) {
    case UserDictionaryCommandStatus::DICTIONARY_NAME_EMPTY:
      last_error_type_ = EMPTY_DICTIONARY_NAME;
      break;
    case UserDictionaryCommandStatus::DICTIONARY_NAME_TOO_LONG:
      last_error_type_ = TOO_LONG_DICTIONARY_NAME;
      break;
    case UserDictionaryCommandStatus ::
        DICTIONARY_NAME_CONTAINS_INVALID_CHARACTER:
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

  return status == UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS;
}

bool UserDictionaryStorage::DeleteDictionary(uint64_t dic_id) {
  if (!UserDictionaryUtil::DeleteDictionary(&proto_, dic_id, nullptr,
                                            nullptr)) {
    // Failed to delete dictionary.
    last_error_type_ = INVALID_DICTIONARY_ID;
    return false;
  }

  last_error_type_ = USER_DICTIONARY_STORAGE_NO_ERROR;
  return true;
}

bool UserDictionaryStorage::RenameDictionary(uint64_t dic_id,
                                             const std::string &dic_name) {
  last_error_type_ = USER_DICTIONARY_STORAGE_NO_ERROR;

  if (!UserDictionaryStorage::IsValidDictionaryName(dic_name)) {
    LOG(ERROR) << "Invalid dictionary name is passed";
    return false;
  }

  UserDictionary *dic = GetUserDictionary(dic_id);
  if (dic == nullptr) {
    last_error_type_ = INVALID_DICTIONARY_ID;
    LOG(ERROR) << "Invalid dictionary id: " << dic_id;
    return false;
  }

  // same name
  if (dic->name() == dic_name) {
    return true;
  }

  for (int i = 0; i < proto_.dictionaries_size(); ++i) {
    if (dic_name == proto_.dictionaries(i).name()) {
      last_error_type_ = DUPLICATED_DICTIONARY_NAME;
      LOG(ERROR) << "duplicated dictionary name";
      return false;
    }
  }

  dic->set_name(dic_name);

  return true;
}

int UserDictionaryStorage::GetUserDictionaryIndex(uint64_t dic_id) const {
  return UserDictionaryUtil::GetUserDictionaryIndexById(proto_, dic_id);
}

bool UserDictionaryStorage::GetUserDictionaryId(const std::string &dic_name,
                                                uint64_t *dic_id) {
  for (size_t i = 0; i < proto_.dictionaries_size(); ++i) {
    if (dic_name == proto_.dictionaries(i).name()) {
      *dic_id = proto_.dictionaries(i).id();
      return true;
    }
  }

  return false;
}

user_dictionary::UserDictionary *UserDictionaryStorage::GetUserDictionary(
    uint64_t dic_id) {
  return UserDictionaryUtil::GetMutableUserDictionaryById(&proto_, dic_id);
}

UserDictionaryStorage::UserDictionaryStorageErrorType
UserDictionaryStorage::GetLastError() const {
  return last_error_type_;
}

bool UserDictionaryStorage::ConvertSyncDictionariesToNormalDictionaries() {
  if (CountSyncableDictionaries(proto_) == 0) {
    return false;
  }

  for (int dictionary_index = proto_.dictionaries_size() - 1;
       dictionary_index >= 0; --dictionary_index) {
    UserDictionary *dic = proto_.mutable_dictionaries(dictionary_index);
    if (!dic->syncable()) {
      continue;
    }

    // Delete removed entries.
    for (int i = dic->entries_size() - 1; i >= 0; --i) {
      if (dic->entries(i).removed()) {
        for (int j = i + 1; j < dic->entries_size(); ++j) {
          dic->mutable_entries()->SwapElements(j - 1, j);
        }
        dic->mutable_entries()->RemoveLast();
      }
    }

    // Delete removed or unused sync dictionaries.
    if (dic->removed() || dic->entries_size() == 0) {
      for (int i = dictionary_index + 1; i < proto_.dictionaries_size(); ++i) {
        proto_.mutable_dictionaries()->SwapElements(i - 1, i);
      }
      proto_.mutable_dictionaries()->RemoveLast();
      continue;
    }

    if (dic->name() == default_sync_dictionary_name()) {
      std::string new_dictionary_name =
          kDictionaryNameConvertedFromSyncableDictionary;
      int index = 0;
      while (UserDictionaryUtil::ValidateDictionaryName(proto_,
                                                        new_dictionary_name) !=
             UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS) {
        ++index;
        new_dictionary_name = Util::StringPrintf(
            "%s_%d", kDictionaryNameConvertedFromSyncableDictionary, index);
      }
      dic->set_name(new_dictionary_name);
    }
    dic->set_syncable(false);
  }

  DCHECK_EQ(0, CountSyncableDictionaries(proto_));

  return true;
}

// static
int UserDictionaryStorage::CountSyncableDictionaries(
    const user_dictionary::UserDictionaryStorage &storage) {
  int num_syncable_dictionaries = 0;
  for (int i = 0; i < storage.dictionaries_size(); ++i) {
    const UserDictionary &dict = storage.dictionaries(i);
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

bool UserDictionaryStorage::IsValidDictionaryName(const std::string &name) {
  UserDictionaryCommandStatus::Status status =
      UserDictionaryUtil::ValidateDictionaryName(
          user_dictionary::UserDictionaryStorage::default_instance(), name);

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
    case UserDictionaryCommandStatus ::
        DICTIONARY_NAME_CONTAINS_INVALID_CHARACTER:
      last_error_type_ = INVALID_CHARACTERS_IN_DICTIONARY_NAME;
      return false;
    default:
      LOG(WARNING) << "Unknown status: " << status;
      return false;
  }
  // Should never reach here.
}

std::string UserDictionaryStorage::default_sync_dictionary_name() {
  return std::string(kDefaultSyncDictionaryName);
}

}  // namespace mozc
