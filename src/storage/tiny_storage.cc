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

#include "storage/tiny_storage.h"

#include <cstdint>
#include <cstring>
#include <ios>
#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include "base/bits.h"
#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/mmap.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "storage/storage_interface.h"

namespace mozc {
namespace storage {
namespace {

constexpr uint32_t kStorageVersion = 0;
constexpr uint32_t kStorageMagicId = 0x431fe241;  // random seed
constexpr size_t kMaxElementSize = 1024;          // max map size
constexpr size_t kMaxKeySize = 4096;              // 4k for key/value
constexpr size_t kMaxValueSize = 4096;            // 4k for key/value
// 1024 * (4096 + 4096) =~ 8MByte
// so 10Mbyte data is reasonable upper bound for file size
constexpr size_t kMaxFileSize = 1024 * 1024 * 10;  // 10Mbyte

bool IsInvalid(const absl::string_view key, const absl::string_view value,
               const size_t size) {
  if (size >= kMaxElementSize) {
    LOG(ERROR) << "too many elements";
    return true;
  }
  if (key.size() >= kMaxKeySize) {
    LOG(ERROR) << "too long key";
    return true;
  }
  if (value.size() >= kMaxValueSize) {
    LOG(ERROR) << "too long value";
    return true;
  }
  return false;
}

class TinyStorageImpl : public StorageInterface {
 public:
  TinyStorageImpl();
  TinyStorageImpl(const TinyStorageImpl &) = delete;
  TinyStorageImpl &operator=(const TinyStorageImpl &) = delete;
  ~TinyStorageImpl() override;

  bool Open(const std::string &filename) override;
  bool Sync() override;
  bool Lookup(const std::string &key, std::string *value) const override;
  bool Insert(const std::string &key, const std::string &value) override;
  bool Erase(const std::string &key) override;
  bool Clear() override;
  size_t Size() const override { return dic_.size(); }

 private:
  std::string filename_;
  bool should_sync_;
  absl::flat_hash_map<std::string, std::string> dic_;
};

TinyStorageImpl::TinyStorageImpl() : should_sync_(true) {
  // the each entry consumes at most
  // sizeof(uint32_t) * 2 (key/value length) +
  // kMaxKeySize + kMaxValueSize
  DCHECK_GT(kMaxFileSize, kMaxElementSize * (kMaxKeySize + kMaxValueSize +
                                             sizeof(uint32_t) * 2));
}

TinyStorageImpl::~TinyStorageImpl() {
  if (should_sync_) {
    Sync();
  }
}

bool TinyStorageImpl::Open(const std::string &filename) {
  dic_.clear();
  filename_ = filename;
  absl::StatusOr<Mmap> mmap = Mmap::Map(filename, Mmap::READ_ONLY);
  if (!mmap.ok()) {
    LOG(WARNING) << "cannot open:" << filename << ": " << mmap.status();
    // here we return true if we cannot open the file.
    // it happens mostly when file doesn't exist.
    // we just make an empty file from scratch here.
    return true;
  }

  if (mmap->size() < 20) {
    LOG(ERROR) << "the file is missing the header.";
    return false;
  }
  if (mmap->size() > kMaxFileSize) {
    LOG(ERROR) << "tring to open too big file";
    return false;
  }

  const char *begin = mmap->begin();
  const char *const end = mmap->end();

  // magic is used for checking whether given file is correct or not.
  // magic = (file_size ^ kStorageMagicId)
  const uint32_t magic = LoadUnalignedAdvance<uint32_t>(begin);
  if ((magic ^ kStorageMagicId) != mmap->size()) {
    LOG(ERROR) << "file magic is broken";
    return false;
  }
  const uint32_t version = LoadUnalignedAdvance<uint32_t>(begin);

  if (version != kStorageVersion) {
    LOG(ERROR) << "Incompatible version";
    return false;
  }
  const uint32_t size = LoadUnalignedAdvance<uint32_t>(begin);

  for (size_t i = 0; i < size; ++i) {
    if (std::distance(begin, end) < sizeof(uint32_t)) {
      LOG(ERROR) << "key_size is invalid";
      return false;
    }
    const uint32_t key_size = LoadUnalignedAdvance<uint32_t>(begin);
    if (begin + key_size > end) {
      LOG(ERROR) << "Too long key is passed";
      return false;
    }

    const absl::string_view key(begin, key_size);
    begin += key_size;

    if (std::distance(begin, end) < sizeof(uint32_t)) {
      LOG(ERROR) << "value_size is invalid";
      return false;
    }
    const uint32_t value_size = LoadUnalignedAdvance<uint32_t>(begin);
    if (begin + value_size > end) {
      LOG(ERROR) << "Too long value is passed";
      return false;
    }

    const absl::string_view value(begin, value_size);
    begin += value_size;

    if (IsInvalid(key, value, dic_.size())) {
      return false;
    }

    dic_.emplace(key, value);
  }

  if (static_cast<size_t>(begin - mmap->begin()) != mmap->size()) {
    LOG(ERROR) << "file is broken: " << filename_;
    dic_.clear();
    return false;
  }

  return true;
}

// Format of storage:
// |magic(uint32_t file_size ^
// kStorageVersion)|version(uint32_t)|size(uint32_t)|
// |key_size(uint32_t)|key(variable length)|
// |value_size(uint32_t)|value(variable length)| ...
bool TinyStorageImpl::Sync() {
  if (!should_sync_) {
    VLOG(2) << "Already synced";
    return true;
  }

  const std::string output_filename = absl::StrCat(filename_, ".tmp");

  OutputFileStream ofs(output_filename, std::ios::binary | std::ios::out);
  if (!ofs) {
    LOG(ERROR) << "cannot open " << output_filename;
    return false;
  }

  uint32_t magic = 0;
  uint32_t size = 0;
  ofs.write(reinterpret_cast<const char *>(&magic), sizeof(magic));
  ofs.write(reinterpret_cast<const char *>(&kStorageVersion),
            sizeof(kStorageVersion));
  ofs.write(reinterpret_cast<const char *>(&size), sizeof(size));

  for (auto &[key, value] : dic_) {
    if (key.empty()) {
      continue;
    }
    const uint32_t key_size = static_cast<uint32_t>(key.size());
    const uint32_t value_size = static_cast<uint32_t>(value.size());
    ofs.write(reinterpret_cast<const char *>(&key_size), sizeof(key_size));
    ofs.write(key.data(), key_size);
    ofs.write(reinterpret_cast<const char *>(&value_size), sizeof(value_size));
    ofs.write(value.data(), value_size);
    ++size;
  }

  magic = static_cast<uint32_t>(ofs.tellp());
  ofs.seekp(0);
  magic ^= kStorageMagicId;

  ofs.write(reinterpret_cast<const char *>(&magic), sizeof(magic));
  ofs.write(reinterpret_cast<const char *>(&kStorageVersion),
            sizeof(kStorageVersion));
  ofs.write(reinterpret_cast<const char *>(&size), sizeof(size));

  // should call close(). Othrwise AtomicRename will be failed.
  ofs.close();

  if (absl::Status s = FileUtil::AtomicRename(output_filename, filename_);
      !s.ok()) {
    LOG(ERROR) << "AtomicRename failed: " << s << "; from: " << output_filename
               << ", to:" << filename_;
    return false;
  }

#ifdef _WIN32
  if (!FileUtil::HideFile(filename_)) {
    LOG(ERROR) << "Cannot make hidden: " << filename_ << " "
               << ::GetLastError();
  }
#endif  // _WIN32

  should_sync_ = false;

  return true;
}

bool TinyStorageImpl::Insert(const std::string &key, const std::string &value) {
  if (IsInvalid(key, value, dic_.size())) {
    LOG(WARNING) << "invalid key/value is passed";
    return false;
  }
  dic_[key] = value;
  should_sync_ = true;
  return true;
}

bool TinyStorageImpl::Erase(const std::string &key) {
  auto node = dic_.extract(key);
  if (node.empty()) {
    VLOG(2) << "cannot erase key: " << key;
    return false;
  }
  should_sync_ = true;
  return true;
}

bool TinyStorageImpl::Lookup(const std::string &key, std::string *value) const {
  const auto it = dic_.find(key);
  if (it == dic_.end()) {
    VLOG(3) << "cannot find key: " << key;
    return false;
  }
  *value = it->second;
  return true;
}

bool TinyStorageImpl::Clear() {
  dic_.clear();
  should_sync_ = true;
  return Sync();
}

}  // namespace

std::unique_ptr<StorageInterface> TinyStorage::Create(
    const std::string &filename) {
  auto storage = std::make_unique<TinyStorageImpl>();
  if (!storage->Open(filename)) {
    LOG(ERROR) << "cannot open " << filename;
    return nullptr;
  }
  return storage;
}

std::unique_ptr<StorageInterface> TinyStorage::New() {
  return std::make_unique<TinyStorageImpl>();
}

}  // namespace storage
}  // namespace mozc
