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
#include <memory>
#include <ostream>
#include <string>
#include <utility>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/random/random.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "base/config_file_stream.h"
#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/process_mutex.h"
#include "base/protobuf/coded_stream.h"
#include "base/protobuf/zero_copy_stream_impl.h"
#include "dictionary/user_dictionary_util.h"
#include "dictionary/user_pos.h"
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

// The limits of dictionary/entry size.
constexpr size_t kMaxDictionarySize = 100;
constexpr size_t kMaxEntrySize = 1000000;

// Default filename of user dictionary.
constexpr absl::string_view kUserDictionaryFile = "user://user_dictionary.db";

}  // namespace

using user_dictionary::ExtendedErrorCode;

// static
std::string UserDictionaryStorage::GetDefaultUserDictionaryFileName() {
  return ConfigFileStream::GetFileName(kUserDictionaryFile);
}

UserDictionaryStorage::UserDictionaryStorage()
    : UserDictionaryStorage::UserDictionaryStorage(
          UserDictionaryStorage::GetDefaultUserDictionaryFileName()) {}

UserDictionaryStorage::UserDictionaryStorage(std::string filename)
    : filename_(std::move(filename)),
      process_mutex_(
          std::make_unique<ProcessMutex>(FileUtil::Basename(filename_))) {}

UserDictionaryStorage::~UserDictionaryStorage() { UnLock(); }

// static
size_t UserDictionaryStorage::max_dictionary_size() {
  return kMaxDictionarySize;
}

// static
size_t UserDictionaryStorage::max_entry_size() { return kMaxEntrySize; }

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
    const UserDictionary& dict = proto_.dictionaries(i);
    if (dict.id() == 0) {
      proto_.mutable_dictionaries(i)->set_id(CreateNewDictionaryId());
    }
  }

  return status;
}

absl::Status UserDictionaryStorage::Save() const {
  if (!process_mutex_->locked()) {
    return absl::FailedPreconditionError(
        "Must be locked before saving the dictionary (SYNC_FAILURE)");
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
  const bool locked = process_mutex_->Lock();
  LOG_IF(ERROR, !locked) << "Lock() failed";
  return locked;
}

bool UserDictionaryStorage::UnLock() { return process_mutex_->UnLock(); }

absl::Status UserDictionaryStorage::ExportDictionary(
    uint64_t dictionary_id, absl::string_view filename) const {
  const int index = GetUserDictionaryIndex(dictionary_id);
  if (index < 0) {
    return ToStatus(ExtendedErrorCode::UNKNOWN_DICTIONARY_ID);
  }

  OutputFileStream ofs(absl::StrCat(filename));
  if (!ofs) {
    return absl::PermissionDeniedError(
        absl::StrCat("Cannot open export file: ", filename));
  }

  const UserDictionary& dic = proto_.dictionaries(index);
  for (size_t i = 0; i < dic.entries_size(); ++i) {
    const UserDictionaryEntry& entry = dic.entries(i);
    ofs << entry.key() << "\t" << entry.value() << "\t"
        << dictionary::UserPos::GetStringPosType(entry.pos()) << "\t"
        << entry.comment() << std::endl;
  }

  return absl::OkStatus();
}

bool UserDictionaryStorage::IsStorageFull() const {
  return proto_.dictionaries_size() >= max_dictionary_size();
}

// static
bool UserDictionaryStorage::IsDictionaryFull(
    const user_dictionary::UserDictionary& dictionary) {
  return dictionary.entries_size() >= max_entry_size();
}

int UserDictionaryStorage::GetUserDictionaryIndex(
    uint64_t dictionary_id) const {
  for (int i = 0; i < proto_.dictionaries_size(); ++i) {
    if (proto_.dictionaries(i).id() == dictionary_id) {
      return i;
    }
  }

  return -1;
}

absl::StatusOr<uint64_t> UserDictionaryStorage::GetUserDictionaryId(
    absl::string_view dictionary_name) const {
  for (size_t i = 0; i < proto_.dictionaries_size(); ++i) {
    if (dictionary_name == proto_.dictionaries(i).name()) {
      return proto_.dictionaries(i).id();
    }
  }

  return absl::NotFoundError(
      absl::StrCat("Dictionary id is not found for ", dictionary_name));
}

user_dictionary::UserDictionary* UserDictionaryStorage::GetUserDictionary(
    uint64_t dictionary_id) {
  const int index = GetUserDictionaryIndex(dictionary_id);
  return index < 0 ? nullptr : proto_.mutable_dictionaries(index);
}

absl::Status UserDictionaryStorage::IsValidDictionaryName(
    absl::string_view dictionary_name) const {
  if (absl::Status status =
          user_dictionary::ValidateDictionaryName(dictionary_name);
      !status.ok()) {
    return status;
  }

  for (int i = 0; i < proto_.dictionaries_size(); ++i) {
    if (proto_.dictionaries(i).name() == dictionary_name) {
      LOG(ERROR) << "duplicated dictionary name";
      return ToStatus(ExtendedErrorCode::DICTIONARY_NAME_DUPLICATED);
    }
  }

  return absl::OkStatus();
}

uint64_t UserDictionaryStorage::CreateNewDictionaryId() const {
  static constexpr uint64_t kInvalidDictionaryId = 0;
  absl::BitGen gen;

  uint64_t id = kInvalidDictionaryId;
  while (id == kInvalidDictionaryId) {
    id = absl::Uniform<uint64_t>(gen);

    // Duplication check.
    for (int i = 0; i < proto_.dictionaries_size(); ++i) {
      if (proto_.dictionaries(i).id() == id) {
        // Duplicated id is found. So invalidate it to retry the generating.
        id = kInvalidDictionaryId;
        break;
      }
    }
  }

  return id;
}

absl::StatusOr<uint64_t> UserDictionaryStorage::CreateDictionary(
    absl::string_view dictionary_name) {
  if (absl::Status status = IsValidDictionaryName(dictionary_name);
      !status.ok()) {
    LOG(ERROR) << "Invalid dictionary name is passed";
    return status;
  }

  if (IsStorageFull()) {
    LOG(ERROR) << "too many dictionaries";
    return ToStatus(ExtendedErrorCode::DICTIONARY_SIZE_LIMIT_EXCEEDED);
  }

  UserDictionary* dictionary = proto_.add_dictionaries();
  if (!dictionary) {
    LOG(ERROR) << "add_dictionaries failed.";
    return ToStatus(ExtendedErrorCode::UNKNOWN_ERROR);
  }

  const uint64_t new_dictionary_id = CreateNewDictionaryId();
  dictionary->set_id(new_dictionary_id);
  dictionary->set_name(dictionary_name);

  return new_dictionary_id;
}

absl::Status UserDictionaryStorage::RenameDictionary(
    uint64_t dictionary_id, absl::string_view dictionary_name) {
  UserDictionary* dictionary = GetUserDictionary(dictionary_id);
  if (!dictionary) {
    return ToStatus(ExtendedErrorCode::UNKNOWN_DICTIONARY_ID);
  }

  if (dictionary->name() == dictionary_name) {
    return absl::OkStatus();
  }

  if (absl::Status status = IsValidDictionaryName(dictionary_name);
      !status.ok()) {
    LOG(ERROR) << "Invalid dictionary name is passed";
    return status;
  }

  dictionary->set_name(dictionary_name);

  return absl::OkStatus();
}

absl::Status UserDictionaryStorage::DeleteDictionary(uint64_t dictionary_id) {
  const int index = GetUserDictionaryIndex(dictionary_id);
  if (index < 0) {
    return ToStatus(ExtendedErrorCode::UNKNOWN_DICTIONARY_ID);
  }

  proto_.mutable_dictionaries()->DeleteSubrange(index, 1);

  return absl::OkStatus();
}

}  // namespace mozc
