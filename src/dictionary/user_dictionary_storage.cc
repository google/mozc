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

#include <cstddef>
#include <cstdint>
#include <ios>
#include <ostream>
#include <string>

#include "absl/base/optimization.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/process_mutex.h"
#include "base/protobuf/coded_stream.h"
#include "base/protobuf/zero_copy_stream_impl.h"
#include "base/vlog.h"
#include "dictionary/user_dictionary_util.h"
#include "protocol/user_dictionary_storage.pb.h"

namespace mozc {
namespace {

// 512MByte
// We expand the limit of serialized message from 64MB(default) to 512MB
constexpr size_t kDefaultTotalBytesLimit = 512 << 20;

// If the last file size exceeds kDefaultWarningTotalBytesLimit,
// we show a warning dialog saying that "All words will not be
// saved correctly. Please make the dictionary size smaller"
constexpr size_t kDefaultWarningTotalBytesLimit = 256 << 20;

using ::mozc::user_dictionary::UserDictionaryCommandStatus;

}  // namespace

UserDictionaryStorage::UserDictionaryStorage(const std::string &file_name)
    : file_name_(file_name),
      process_mutex_(new ProcessMutex(FileUtil::Basename(file_name))) {}

UserDictionaryStorage::~UserDictionaryStorage() { UnLock(); }

const std::string &UserDictionaryStorage::filename() const {
  return file_name_;
}

absl::Status UserDictionaryStorage::Exists() const {
  return FileUtil::FileExists(file_name_);
}

absl::Status UserDictionaryStorage::LoadInternal() {
  InputFileStream ifs(file_name_, std::ios::binary);
  if (!ifs) {
    absl::Status s = Exists();
    if (s.ok()) {
      last_error_type_ = UNKNOWN_ERROR;
      return absl::UnknownError(absl::StrCat(
          file_name_, " exists but cannot open it: ", s.ToString()));
    }
    last_error_type_ = FILE_NOT_EXISTS;
    return s;
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
    last_error_type_ = BROKEN_FILE;
    return absl::UnknownError("ParseFromCodedStream failed. File seems broken");
  }
  return absl::OkStatus();
}

absl::Status UserDictionaryStorage::Load() {
  last_error_type_ = USER_DICTIONARY_STORAGE_NO_ERROR;

  absl::Status status = Exists();

  // Check if the user dictionary exists or not.
  if (status.ok()) {
    status = LoadInternal();
  } else if (absl::IsNotFound(status)) {
    // This is also an expected scenario: e.g., clean installation, unit tests.
    MOZC_VLOG(1) << "User dictionary file has not been created";
    last_error_type_ = FILE_NOT_EXISTS;
  } else {
    // Failed to check file existnce.
    status = absl::Status(
        status.code(),
        absl::StrCat("Cannot check if the user dictionary file exists: file=",
                     file_name_, ": ", status.message()));
    last_error_type_ = UNKNOWN_ERROR;
  }

  // Check dictionary id here. if id is 0, assign random ID.
  for (int i = 0; i < proto_.dictionaries_size(); ++i) {
    const UserDictionary &dict = proto_.dictionaries(i);
    if (dict.id() == 0) {
      proto_.mutable_dictionaries(i)->set_id(
          UserDictionaryUtil::CreateNewDictionaryId(proto_));
    }
  }

  return status;
}

absl::Status UserDictionaryStorage::Save() {
  last_error_type_ = USER_DICTIONARY_STORAGE_NO_ERROR;

  {
    absl::MutexLock l(&local_mutex_);
    if (!locked_) {
      last_error_type_ = SYNC_FAILURE;
      return absl::FailedPreconditionError(
          "Must be locked before saving the dictionary (SYNC_FAILURE)");
    }
  }

  const std::string tmp_file_name = file_name_ + ".tmp";
  std::string size_error_msg;
  {
    OutputFileStream ofs(tmp_file_name,
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
      absl::StrAppend(&msg, "; ", size_error_msg);
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
  absl::MutexLock l(&local_mutex_);
  locked_ = process_mutex_->Lock();
  LOG_IF(ERROR, !locked_) << "Lock() failed";
  return locked_;
}

bool UserDictionaryStorage::UnLock() {
  absl::MutexLock l(&local_mutex_);
  process_mutex_->UnLock();
  locked_ = false;
  return true;
}

bool UserDictionaryStorage::ExportDictionary(const uint64_t dic_id,
                                             const std::string &file_name) {
  const int index = GetUserDictionaryIndex(dic_id);
  if (index < 0) {
    last_error_type_ = INVALID_DICTIONARY_ID;
    LOG(ERROR) << "Invalid dictionary id: " << dic_id;
    return false;
  }

  OutputFileStream ofs(file_name);
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

bool UserDictionaryStorage::CreateDictionary(const absl::string_view dic_name,
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

bool UserDictionaryStorage::RenameDictionary(const uint64_t dic_id,
                                             const absl::string_view dic_name) {
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

  dic->set_name(std::string(dic_name));

  return true;
}

int UserDictionaryStorage::GetUserDictionaryIndex(uint64_t dic_id) const {
  return UserDictionaryUtil::GetUserDictionaryIndexById(proto_, dic_id);
}

bool UserDictionaryStorage::GetUserDictionaryId(
    const absl::string_view dic_name, uint64_t *dic_id) {
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

// static
size_t UserDictionaryStorage::max_entry_size() {
  return UserDictionaryUtil::max_entry_size();
}

// static
size_t UserDictionaryStorage::max_dictionary_size() {
  return UserDictionaryUtil::max_entry_size();
}

bool UserDictionaryStorage::IsValidDictionaryName(
    const absl::string_view name) {
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
  ABSL_UNREACHABLE();
}

}  // namespace mozc
