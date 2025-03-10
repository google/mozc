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
#include <utility>

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

UserDictionaryStorage::UserDictionaryStorage(std::string filename)
    : filename_(std::move(filename)),
      process_mutex_(new ProcessMutex(FileUtil::Basename(filename_))) {}

UserDictionaryStorage::~UserDictionaryStorage() { UnLock(); }

absl::string_view UserDictionaryStorage::filename() const { return filename_; }

absl::Status UserDictionaryStorage::Exists() const {
  return FileUtil::FileExists(filename_);
}

absl::Status UserDictionaryStorage::LoadInternal() {
  InputFileStream ifs(filename_, std::ios::binary);

  if (!ifs) {
    absl::Status s = Exists();
    if (s.ok()) {
      return absl::UnknownError(absl::StrCat(
          filename_, " exists but cannot open it: ", s.ToString()));
    }
    return s;  // kNotFound error.
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
    return absl::DataLossError(
        "ParseFromCodedStream failed. File seems broken");
  }

  return absl::OkStatus();
}

absl::Status UserDictionaryStorage::Load() {
  absl::Status status = LoadInternal();

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

absl::Status UserDictionaryStorage::Save() const {
  {
    absl::MutexLock l(&local_mutex_);
    if (!locked_) {
      return absl::FailedPreconditionError(
          "Must be locked before saving the dictionary (SYNC_FAILURE)");
    }
  }

  const std::string tmp_filename = absl::StrCat(filename_, ".tmp");
  std::string size_error_msg;
  bool too_big_file_bytes = false;
  {
    OutputFileStream ofs(tmp_filename,
                         std::ios::out | std::ios::binary | std::ios::trunc);
    if (!ofs) {
      return absl::PermissionDeniedError(absl::StrFormat(
          "Cannot open %s for write (SYNC_FAILURE)", tmp_filename));
    }

    if (!proto_.SerializeToOstream(&ofs)) {
      return absl::PermissionDeniedError(absl::StrFormat(
          "SerializeToOstream failed (SYNC_FAILURE); path = %s", tmp_filename));
    }

    const size_t file_size = ofs.tellp();

    ofs.close();
    if (ofs.fail()) {
      return absl::PermissionDeniedError(
          absl::StrFormat("Failed to close %s (SYNC_FAILURE)", tmp_filename));
    }

    if (file_size >= kDefaultWarningTotalBytesLimit) {
      size_error_msg = absl::StrFormat(
          "The file size exceeds the limit: size = %d, limit = %d", file_size,
          kDefaultWarningTotalBytesLimit);
      // Perform "AtomicRename" even if the size exceeded.
      too_big_file_bytes = true;
    }
  }

  if (absl::Status s = FileUtil::AtomicRename(tmp_filename, filename_);
      !s.ok()) {
    std::string msg =
        absl::StrFormat("%s; Atomic rename from %s to %s failed (SYNC_FAILURE)",
                        s.message(), tmp_filename, filename_);
    if (too_big_file_bytes) {
      absl::StrAppend(&msg, "; ", size_error_msg);
    }
    return absl::PermissionDeniedError(msg);
  }

  if (too_big_file_bytes) {
    return absl::ResourceExhaustedError(absl::StrFormat(
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

absl::Status UserDictionaryStorage::ExportDictionary(
    uint64_t dic_id, absl::string_view filename) const {
  const int index = GetUserDictionaryIndex(dic_id);
  if (index < 0) {
    return absl::Status(static_cast<absl::StatusCode>(INVALID_DICTIONARY_ID),
                        absl::StrCat("Invalid dictionary id: ", dic_id));
  }

  OutputFileStream ofs(absl::StrCat(filename));
  if (!ofs) {
    return absl::PermissionDeniedError(
        absl::StrCat("Cannot open export file: ", filename));
  }

  const UserDictionary &dic = proto_.dictionaries(index);
  for (size_t i = 0; i < dic.entries_size(); ++i) {
    const UserDictionaryEntry &entry = dic.entries(i);
    ofs << entry.key() << "\t" << entry.value() << "\t"
        << UserDictionaryUtil::GetStringPosType(entry.pos()) << "\t"
        << entry.comment() << std::endl;
  }

  return absl::OkStatus();
}

absl::StatusOr<uint64_t> UserDictionaryStorage::CreateDictionary(
    const absl::string_view dic_name) {
  uint64_t new_dic_id = 0;

  UserDictionaryCommandStatus::Status status =
      UserDictionaryUtil::CreateDictionary(&proto_, dic_name, &new_dic_id);

  ExtendedErrorCode error_code = OK;

  switch (status) {
    case UserDictionaryCommandStatus::DICTIONARY_NAME_EMPTY:
      error_code = EMPTY_DICTIONARY_NAME;
      break;
    case UserDictionaryCommandStatus::DICTIONARY_NAME_TOO_LONG:
      error_code = TOO_LONG_DICTIONARY_NAME;
      break;
    case UserDictionaryCommandStatus ::
        DICTIONARY_NAME_CONTAINS_INVALID_CHARACTER:
      error_code = INVALID_CHARACTERS_IN_DICTIONARY_NAME;
      break;
    case UserDictionaryCommandStatus::DICTIONARY_NAME_DUPLICATED:
      error_code = DUPLICATED_DICTIONARY_NAME;
      break;
    case UserDictionaryCommandStatus::DICTIONARY_SIZE_LIMIT_EXCEEDED:
      error_code = TOO_MANY_DICTIONARIES;
      break;
    case UserDictionaryCommandStatus::UNKNOWN_ERROR:
      // Reuses kUnknown.
      error_code = static_cast<ExtendedErrorCode>(absl::StatusCode::kUnknown);
      break;
    default:
      break;
  }

  if (error_code == OK) {
    return new_dic_id;
  }

  return absl::Status(static_cast<absl::StatusCode>(error_code),
                      "Failed to create dictionary");
}

absl::Status UserDictionaryStorage::DeleteDictionary(uint64_t dic_id) {
  if (!UserDictionaryUtil::DeleteDictionary(&proto_, dic_id, nullptr,
                                            nullptr)) {
    return absl::Status(static_cast<absl::StatusCode>(INVALID_DICTIONARY_ID),
                        "Failed to delete entry");
  }

  return absl::OkStatus();
}

absl::Status UserDictionaryStorage::RenameDictionary(
    const uint64_t dic_id, const absl::string_view dic_name) {
  if (absl::Status status = IsValidDictionaryName(dic_name); !status.ok()) {
    return status;
  }

  UserDictionary *dic = GetUserDictionary(dic_id);
  if (!dic) {
    return absl::Status(static_cast<absl::StatusCode>(INVALID_DICTIONARY_ID),
                        absl::StrCat("Invalid dictionary id: ", dic_id));
  }

  // same name
  if (dic->name() == dic_name) {
    return absl::OkStatus();
  }

  for (int i = 0; i < proto_.dictionaries_size(); ++i) {
    if (dic_name == proto_.dictionaries(i).name()) {
      return absl::Status(
          static_cast<absl::StatusCode>(DUPLICATED_DICTIONARY_NAME),
          absl::StrCat("duplicated dictionary name: ", dic_name));
    }
  }

  dic->set_name(dic_name);

  return absl::OkStatus();
}

int UserDictionaryStorage::GetUserDictionaryIndex(uint64_t dic_id) const {
  return UserDictionaryUtil::GetUserDictionaryIndexById(proto_, dic_id);
}

absl::StatusOr<uint64_t> UserDictionaryStorage::GetUserDictionaryId(
    absl::string_view dic_name) const {
  for (size_t i = 0; i < proto_.dictionaries_size(); ++i) {
    if (dic_name == proto_.dictionaries(i).name()) {
      return proto_.dictionaries(i).id();
    }
  }
  return absl::NotFoundError(
      absl::StrCat("Dictionary id is not found for ", dic_name));
}

user_dictionary::UserDictionary *UserDictionaryStorage::GetUserDictionary(
    uint64_t dic_id) {
  return UserDictionaryUtil::GetMutableUserDictionaryById(&proto_, dic_id);
}

// static
size_t UserDictionaryStorage::max_entry_size() {
  return UserDictionaryUtil::max_entry_size();
}

// static
size_t UserDictionaryStorage::max_dictionary_size() {
  return UserDictionaryUtil::max_entry_size();
}

absl::Status UserDictionaryStorage::IsValidDictionaryName(
    const absl::string_view name) const {
  const UserDictionaryCommandStatus::Status status =
      UserDictionaryUtil::ValidateDictionaryName(
          user_dictionary::UserDictionaryStorage::default_instance(), name);

  ExtendedErrorCode error_code = OK;

  switch (status) {
    // Succeeded case.
    case UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS:
      return absl::OkStatus();
    // Failure cases.
    case UserDictionaryCommandStatus::DICTIONARY_NAME_EMPTY:
      error_code = EMPTY_DICTIONARY_NAME;
      break;
    case UserDictionaryCommandStatus::DICTIONARY_NAME_TOO_LONG:
      error_code = TOO_LONG_DICTIONARY_NAME;
      break;
    case UserDictionaryCommandStatus::
        DICTIONARY_NAME_CONTAINS_INVALID_CHARACTER:
      error_code = INVALID_CHARACTERS_IN_DICTIONARY_NAME;
      break;
    default:
      // Reuses kUnknown.
      error_code = static_cast<ExtendedErrorCode>(absl::StatusCode::kUnknown);
      break;
  }

  return absl::Status(
      static_cast<absl::StatusCode>(error_code),
      absl::StrCat("Invalid dictionary_name is passed: ", name));
}

}  // namespace mozc
