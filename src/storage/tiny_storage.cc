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

#ifdef OS_WIN
#include <Windows.h>
#endif  // OS_WIN

#include <cstring>
#include <map>
#include <memory>
#include <string>

#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/mmap.h"
#include "base/port.h"

namespace mozc {
namespace storage {
namespace {

const uint32_t kStorageVersion = 0;
const uint32_t kStorageMagicId = 0x431fe241;  // random seed
const size_t kMaxElementSize = 1024;        // max map size
const size_t kMaxKeySize = 4096;            // 4k for key/value
const size_t kMaxValueSize = 4096;          // 4k for key/value
// 1024 * (4096 + 4096) =~ 8MByte
// so 10Mbyte data is reasonable upper bound for file size
const size_t kMaxFileSize = 1024 * 1024 * 10;  // 10Mbyte

template <typename T>
bool ReadData(char **begin, const char *end, T *value) {
  if (*begin + sizeof(*value) > end) {
    LOG(WARNING) << "accessing invalid pointer";
    return false;
  }
  memcpy(value, *begin, sizeof(*value));
  *begin += sizeof(*value);
  return true;
}

bool IsInvalid(const std::string &key, const std::string &value, size_t size) {
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
  std::map<std::string, std::string> dic_;

  DISALLOW_COPY_AND_ASSIGN(TinyStorageImpl);
};

TinyStorageImpl::TinyStorageImpl() : should_sync_(true) {
  // the each entry consumes at most
  // sizeof(uint32) * 2 (key/value length) +
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
  Mmap mmap;
  dic_.clear();
  filename_ = filename;
  if (!mmap.Open(filename.c_str(), "r")) {
    LOG(WARNING) << "cannot open:" << filename;
    // here we return true if we cannot open the file.
    // it happens mostly when file doesn't exist.
    // we just make an empty file from scratch here.
    return true;
  }

  if (mmap.size() > kMaxFileSize) {
    LOG(ERROR) << "tring to open too big file";
    return false;
  }

  char *begin = mmap.begin();
  const char *end = mmap.end();

  uint32_t version = 0;
  uint32_t magic = 0;
  uint32_t size = 0;
  // magic is used for checking whether given file is correct or not.
  // magic = (file_size ^ kStorageMagicId)
  if (!ReadData<uint32_t>(&begin, end, &magic)) {
    LOG(ERROR) << "cannot read magic";
    return false;
  }

  if ((magic ^ kStorageMagicId) != mmap.size()) {
    LOG(ERROR) << "file magic is broken";
    return false;
  }

  if (!ReadData<uint32_t>(&begin, end, &version)) {
    LOG(ERROR) << "cannot read version";
    return false;
  }

  if (version != kStorageVersion) {
    LOG(ERROR) << "Incompatible version";
    return false;
  }

  if (!ReadData<uint32_t>(&begin, end, &size)) {
    LOG(ERROR) << "cannot read size";
    return false;
  }

  for (size_t i = 0; i < size; ++i) {
    uint32_t key_size = 0;
    uint32_t value_size = 0;
    if (!ReadData<uint32_t>(&begin, end, &key_size)) {
      LOG(ERROR) << "key_size is invalid";
      return false;
    }

    if (begin + key_size > end) {
      LOG(ERROR) << "Too long key is passed";
      return false;
    }

    const std::string key(begin, key_size);
    begin += key_size;

    if (!ReadData<uint32_t>(&begin, end, &value_size)) {
      LOG(ERROR) << "value_size is invalid";
      return false;
    }

    if (begin + value_size > end) {
      LOG(ERROR) << "Too long value is passed";
      return false;
    }

    const std::string value(begin, value_size);
    begin += value_size;

    if (IsInvalid(key, value, dic_.size())) {
      return false;
    }

    dic_.insert(std::make_pair(key, value));
  }

  if (static_cast<size_t>(begin - mmap.begin()) != mmap.size()) {
    LOG(ERROR) << "file is broken: " << filename_;
    dic_.clear();
    return false;
  }

  return true;
}

// Format of storage:
// |magic(uint32 file_size ^ kStorageVersion)|version(uint32)|size(uint32)|
// |key_size(uint32)|key(variable length)|
// |value_size(uint32)|value(variable length)| ...
bool TinyStorageImpl::Sync() {
  if (!should_sync_) {
    VLOG(2) << "Already synced";
    return true;
  }

  const std::string output_filename = filename_ + ".tmp";

  OutputFileStream ofs(output_filename.c_str(),
                       std::ios::binary | std::ios::out);
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

  for (std::map<std::string, std::string>::const_iterator it = dic_.begin();
       it != dic_.end(); ++it) {
    if (it->first.empty()) {
      continue;
    }
    const std::string &key = it->first;
    const std::string &value = it->second;
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

  if (!FileUtil::AtomicRename(output_filename, filename_)) {
    LOG(ERROR) << "AtomicRename failed";
    return false;
  }

#ifdef OS_WIN
  if (!FileUtil::HideFile(filename_)) {
    LOG(ERROR) << "Cannot make hidden: " << filename_ << " "
               << ::GetLastError();
  }
#endif

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
  std::map<std::string, std::string>::iterator it = dic_.find(key);
  if (it == dic_.end()) {
    VLOG(2) << "cannot erase key: " << key;
    return false;
  }
  dic_.erase(it);
  should_sync_ = true;
  return true;
}

bool TinyStorageImpl::Lookup(const std::string &key, std::string *value) const {
  std::map<std::string, std::string>::const_iterator it = dic_.find(key);
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

StorageInterface *TinyStorage::Create(const char *filename) {
  std::unique_ptr<TinyStorageImpl> storage(new TinyStorageImpl);
  if (!storage->Open(filename)) {
    LOG(ERROR) << "cannot open " << filename;
    return nullptr;
  }
  return storage.release();
}

StorageInterface *TinyStorage::New() { return new TinyStorageImpl; }

}  // namespace storage
}  // namespace mozc
