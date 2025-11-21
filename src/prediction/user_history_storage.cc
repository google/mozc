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

#include "prediction/user_history_storage.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "absl/algorithm/container.h"
#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/config_file_stream.h"
#include "base/file_util.h"
#include "base/hash.h"
#include "base/util.h"
#include "prediction/user_history_predictor.pb.h"
#include "storage/encrypted_string_storage.h"
#include "storage/lru_cache.h"

namespace mozc::prediction {

namespace {

// Uses '\t' as a key/value delimiter
constexpr absl::string_view kDelimiter = "\t";

// On-memory LRU cache size.
// Typically memory/storage footprint becomes kLruCacheSize * 70 bytes.
// Note that actual entries serialized to the disk may be smaller
// than this size.
constexpr size_t kLruCacheSize = 10000;

// File name for the history
#ifdef _WIN32
constexpr absl::string_view kFileName = "user://history.db";
#else   // _WIN32
constexpr absl::string_view kFileName = "user://.history.db";
#endif  // _WIN32
}  // namespace

UserHistoryStorage::UserHistoryStorage(absl::string_view filename)
    : dic_(new DicCache(kLruCacheSize)), filename_(filename) {
  AsyncLoad();
}

UserHistoryStorage::UserHistoryStorage()
    : UserHistoryStorage::UserHistoryStorage(
          ConfigFileStream::GetFileName(kFileName)) {}

UserHistoryStorage::~UserHistoryStorage() {
  if (IsSyncerRunning()) {
    // Stops the loading thread immediately.
    canceled_ = true;
    Wait();
  }

  // Call Saves() just in case as the internal data may be updated while syncer
  // thread is running.
  Save();
}

void UserHistoryStorage::Wait() const { task_manager_.Wait(); }
  
bool UserHistoryStorage::IsSyncerRunning() const {
  return task_manager_.IsRunning();
}

bool UserHistoryStorage::IsSyncerInCriticalSection() const {
  // syncer is running, and mutex is owned by syncer thread.
  return !mutex_.owns_lock() && task_manager_.IsRunning();
}

void UserHistoryStorage::AsyncSave() {
  if (needs_sync_ && !IsSyncerRunning()) {
    task_manager_.Schedule([this] { Save(); });
  }
}

void UserHistoryStorage::AsyncLoad() {
  if (!IsSyncerRunning()) {
    task_manager_.Schedule([this] { Load(); });
  }
}

void UserHistoryStorage::Clear() {
  auto lock = AcquireUniqueLock();
  dic_ = std::make_unique<DicCache>(kLruCacheSize);
  needs_sync_ = true;
  Save();
}

bool UserHistoryStorage::Load() {
  storage::EncryptedStringStorage storage(filename());

  std::string input;
  if (!storage.Load(&input)) {
    LOG(ERROR) << "Can't load user history data.";
    return false;
  }

  user_history_predictor::UserHistory proto;
  if (!proto.ParseFromString(input)) {
    LOG(ERROR) << "ParseFromString failed. message looks broken";
    return false;
  }

  MigrateNextEntries(&proto);

  return Load(std::move(proto));
}

bool UserHistoryStorage::Load(user_history_predictor::UserHistory&& proto) {
  // Enters syncer's critical section.
  auto lock = AcquireUniqueLock();

  dic_->Clear();

  // 1) After loading `dic_` no need to sync.
  // 2) When AsyncLoad is canceled, `dic_` has incomplete data,
  //    so must not be synced.
  needs_sync_ = false;

  for (Entry& entry : *proto.mutable_entries()) {
    if (canceled_) {
      LOG(ERROR) << "Loading thread is canceled";
      return false;
    }

    if (entry.value().empty() || entry.key().empty()) {
      continue;
    }
    // Workaround for b/116826494: Some garbled characters are suggested
    // from user history. This filters such entries.
    if (!Util::IsValidUtf8(entry.value())) {
      LOG(ERROR) << "Invalid UTF8 found in user history: "
                 << absl::BytesToHexString(entry.value());
      continue;
    }
    // conversion_freq is migrated to suggestion_freq.
    entry.set_suggestion_freq(
        std::max(entry.suggestion_freq(), entry.conversion_freq_deprecated()));
    entry.clear_conversion_freq_deprecated();
    // Avoid std::move() is called before EntryFingerprint.

    const uint64_t fp = EntryFingerprint(entry);
    dic_->Insert(fp, std::move(entry));
  }

  return true;
}

bool UserHistoryStorage::Save() {
  if (!needs_sync_) {
    return true;
  }

  user_history_predictor::UserHistory proto;
  {
    // Enters syncer's critical section.
    auto lock = AcquireUniqueLock();

    proto.mutable_entries()->Reserve(dic_->Size());

    for (const DicElement& elm : *dic_) {
      if (proto.entries_size() >= kLruCacheSize) {
        break;
      }
      *proto.add_entries() = elm.value;
    }
  }

  // Reverse the contents to keep the LRU order when loading.
  absl::c_reverse(*proto.mutable_entries());

  std::string output;
  if (!proto.AppendToString(&output)) {
    LOG(ERROR) << "AppendToString failed";
    return false;
  }

  // Remove the storage file when proto is empty because
  // storing empty file causes an error.
  if (output.empty()) {
    FileUtil::UnlinkIfExists(filename()).IgnoreError();
    return true;
  }

  storage::EncryptedStringStorage storage(filename());
  if (!storage.Save(output)) {
    LOG(ERROR) << "Can't save user history data.";
    return false;
  }

  needs_sync_ = false;

  return true;
}

UserHistoryStorage::UniqueLock UserHistoryStorage::AcquireUniqueLock() const {
  return UniqueLock(mutex_);
}

void UserHistoryStorage::ForEach(
    std::function<bool(uint64_t fp, const Entry& entry)> func) const {
  auto lock = AcquireUniqueLock();

  for (const DicElement& elm : *dic_) {
    if (!func(elm.key, elm.value)) {
      break;
    }
  }
}

UserHistoryStorage::EntrySnapshot UserHistoryStorage::Insert(
    uint64_t fp) const {
  auto lock = AcquireUniqueLock();
  needs_sync_ = true;

  DicElement* elm = dic_->Insert(fp);
  return EntrySnapshot(elm ? &elm->value : nullptr, std::move(lock));
}

void UserHistoryStorage::Insert(Entry entry) const {
  const uint64_t fp = EntryFingerprint(entry);

  auto lock = AcquireUniqueLock();
  needs_sync_ = true;
  dic_->Insert(fp, std::move(entry));
}

UserHistoryStorage::EntrySnapshot UserHistoryStorage::MutableLookup(
    uint64_t fp) const {
  auto lock = AcquireUniqueLock();
  needs_sync_ = true;
  return EntrySnapshot(dic_->MutableLookupWithoutInsert(fp), std::move(lock));
}

UserHistoryStorage::ConstEntrySnapshot UserHistoryStorage::Lookup(
    uint64_t fp) const {
  auto lock = AcquireUniqueLock();
  return ConstEntrySnapshot(dic_->LookupWithoutInsert(fp), std::move(lock));
}

UserHistoryStorage::ConstEntrySnapshot UserHistoryStorage::Head() const {
  auto lock = AcquireUniqueLock();
  const DicElement* elm = dic_->Head();
  return ConstEntrySnapshot(elm ? &elm->value : nullptr, std::move(lock));
}

void UserHistoryStorage::Erase(absl::Span<const uint64_t> fps) const {
  if (fps.empty()) {
    return;
  }

  auto lock = AcquireUniqueLock();
  needs_sync_ = true;

  for (const uint64_t fp : fps) {
    dic_->Erase(fp);
  }
}

bool UserHistoryStorage::IsEmpty() const {
  auto lock = AcquireUniqueLock();
  return dic_->empty();
}

// static
uint64_t UserHistoryStorage::Fingerprint(const absl::string_view key,
                                         const absl::string_view value) {
  return CityFingerprint(absl::StrCat(key, kDelimiter, value));
}

// static
uint32_t UserHistoryStorage::FingerprintDepereated(
    const absl::string_view key, const absl::string_view value) {
  return LegacyFingerprint32(absl::StrCat(key, kDelimiter, value));
}

// static
uint64_t UserHistoryStorage::EntryFingerprint(const Entry& entry) {
  return Fingerprint(entry.key(), entry.value());
}

// static
void UserHistoryStorage::MigrateNextEntries(
    mozc::user_history_predictor::UserHistory* absl_nonnull proto) {
  DCHECK(proto);

  bool migrated = true;
  for (const auto& entry : proto->entries()) {
    if (entry.next_entries_deprecated_size() > 0) {
      migrated = false;
      break;
    }
  }

  if (migrated) {
    return;
  }

  absl::flat_hash_map<uint32_t, uint64_t> old2new_fp;
  for (const auto& entry : proto->entries()) {
    const uint32_t old_fp = FingerprintDepereated(entry.key(), entry.value());
    const uint64_t new_fp = Fingerprint(entry.key(), entry.value());
    old2new_fp[old_fp] = new_fp;
  }

  for (auto& entry : *proto->mutable_entries()) {
    for (const auto& next_entry : entry.next_entries_deprecated()) {
      if (const auto it = old2new_fp.find(next_entry.entry_fp());
          it != old2new_fp.end()) {
        entry.add_next_entry_fps(it->second);
      }
    }
    entry.clear_next_entries_deprecated();
  }
}

}  // namespace mozc::prediction
