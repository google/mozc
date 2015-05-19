// Copyright 2010-2013, Google Inc.
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

#ifdef OS_WIN
#include <Windows.h>
#endif  // OS_WIN

#include <cstring>
#include <map>
#include <string>
#include "base/base.h"
#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/mmap.h"
#include "base/scoped_ptr.h"
#include "base/util.h"

namespace mozc {
namespace storage {
namespace {

const uint32 kStorageVersion = 0;
const uint32 kStorageMagicId = 0x431fe241;  // random seed
const size_t kMaxElementSize = 1024;        // max map size
const size_t kMaxKeySize     = 4096;        // 4k for key/value
const size_t kMaxValueSize   = 4096;        // 4k for key/value
// 1024 * (4096 + 4096) =~ 8MByte
// so 10Mbyte data is reasonable upper bound for file size
const size_t kMaxFileSize     = 1024 * 1024 * 10;  // 10Mbyte

template<typename T>
bool ReadData(char **begin, const char *end, T *value) {
  if (*begin + sizeof(*value) > end) {
    LOG(WARNING) << "accessing invalid pointer";
    return false;
  }
  memcpy(value, *begin, sizeof(*value));
  *begin += sizeof(*value);
  return true;
}

bool IsInvalid(const string &key, const string &value, size_t size) {
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
  virtual ~TinyStorageImpl();

  virtual bool Open(const string &filename);
  virtual bool Sync();
  virtual bool Lookup(const string &key, string *value) const;
  virtual bool Insert(const string &key, const string &value);
  virtual bool Erase(const string &key);
  virtual bool Clear();
  virtual size_t Size() const {
    return dic_.size();
  }

 private:
  string filename_;
  bool should_sync_;
  map<string, string> dic_;
};

TinyStorageImpl::TinyStorageImpl() : should_sync_(true) {
  // the each entry consumes at most
  // sizeof(uint32) * 2 (key/value length) +
  // kMaxKeySize + kMaxValueSize
  DCHECK_GT(
      kMaxFileSize,
      kMaxElementSize * (kMaxKeySize + kMaxValueSize + sizeof(uint32) * 2));
}

TinyStorageImpl::~TinyStorageImpl() {
  if (should_sync_) {
    Sync();
  }
}

bool TinyStorageImpl::Open(const string &filename) {
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

  uint32 version = 0;
  uint32 magic = 0;
  uint32 size = 0;
  // magic is used for checking whether given file is correct or not.
  // magic = (file_size ^ kStorageMagicId)
  if (!ReadData<uint32>(&begin, end, &magic)) {
    LOG(ERROR) << "cannot read magic";
    return false;
  }

  if ((magic ^ kStorageMagicId) != mmap.size()) {
    LOG(ERROR) << "file magic is broken";
    return false;
  }

  if (!ReadData<uint32>(&begin, end, &version)) {
    LOG(ERROR) << "cannot read version";
    return false;
  }

  if (version != kStorageVersion) {
    LOG(ERROR) << "Incompatible version";
    return false;
  }

  if (!ReadData<uint32>(&begin, end, &size)) {
    LOG(ERROR) << "cannot read size";
    return false;
  }

  for (size_t i = 0; i < size; ++i) {
    uint32 key_size = 0;
    uint32 value_size = 0;
    if (!ReadData<uint32>(&begin, end, &key_size)) {
      LOG(ERROR) << "key_size is invalid";
      return false;
    }

    if (begin + key_size > end) {
      LOG(ERROR) << "Too long key is passed";
      return false;
    }

    const string key(begin, key_size);
    begin += key_size;

    if (!ReadData<uint32>(&begin, end, &value_size)) {
      LOG(ERROR) << "value_size is invalid";
      return false;
    }

    if (begin + value_size > end) {
      LOG(ERROR) << "Too long value is passed";
      return false;
    }

    const string value(begin, value_size);
    begin += value_size;

    if (IsInvalid(key, value, dic_.size())) {
      return false;
    }

    dic_.insert(make_pair(key, value));
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

  const string output_filename = filename_ + ".tmp";

  OutputFileStream ofs(output_filename.c_str(),
                       ios::binary | ios::out);
  if (!ofs) {
    LOG(ERROR) << "cannot open " << output_filename;
    return false;
  }

  uint32 magic = 0;
  uint32 size = 0;
  ofs.write(reinterpret_cast<const char *>(&magic),
            sizeof(magic));
  ofs.write(reinterpret_cast<const char *>(&kStorageVersion),
            sizeof(kStorageVersion));
  ofs.write(reinterpret_cast<const char *>(&size),
            sizeof(size));

  for (map<string, string>::const_iterator it = dic_.begin();
       it != dic_.end(); ++it) {
    if (it->first.empty()) {
      continue;
    }
    const string &key = it->first;
    const string &value = it->second;
    const uint32 key_size = static_cast<uint32>(key.size());
    const uint32 value_size = static_cast<uint32>(value.size());
    ofs.write(reinterpret_cast<const char *>(&key_size),
              sizeof(key_size));
    ofs.write(key.data(), key_size);
    ofs.write(reinterpret_cast<const char *>(&value_size),
              sizeof(value_size));
    ofs.write(value.data(), value_size);
    ++size;
  }

  magic = static_cast<uint32>(ofs.tellp());
  ofs.seekp(0);
  magic ^= kStorageMagicId;

  ofs.write(reinterpret_cast<const char *>(&magic),
            sizeof(magic));
  ofs.write(reinterpret_cast<const char *>(&kStorageVersion),
            sizeof(kStorageVersion));
  ofs.write(reinterpret_cast<const char *>(&size),
            sizeof(size));

  // should call close(). Othrwise AtomicRename will be failed.
  ofs.close();

  if (!FileUtil::AtomicRename(output_filename, filename_)) {
    LOG(ERROR) << "AtomicRename failed";
    return false;
  }

#ifdef OS_WIN
  wstring wfilename;
  Util::UTF8ToWide(filename_.c_str(), &wfilename);
  if (!::SetFileAttributes(wfilename.c_str(),
                           FILE_ATTRIBUTE_HIDDEN |
                           FILE_ATTRIBUTE_SYSTEM)) {
    LOG(ERROR) << "Cannot make hidden: " << filename_
               << " " << ::GetLastError();
  }
#endif

  should_sync_ = false;

  return true;
}

bool TinyStorageImpl::Insert(const string &key, const string &value) {
  if (IsInvalid(key, value, dic_.size())) {
    LOG(WARNING) << "invalid key/value is passed";
    return false;
  }
  dic_[key] = value;
  should_sync_ = true;
  return true;
}

bool TinyStorageImpl::Erase(const string &key) {
  map<string, string>::iterator it = dic_.find(key);
  if (it == dic_.end()) {
    VLOG(2) << "cannot erase key: " << key;
    return false;
  }
  dic_.erase(it);
  should_sync_ = true;
  return true;
}

bool TinyStorageImpl::Lookup(const string &key, string *value) const {
  map<string, string>::const_iterator it = dic_.find(key);
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
  scoped_ptr<TinyStorageImpl> storage(new TinyStorageImpl);
  if (!storage->Open(filename)) {
    LOG(ERROR) << "cannot open " << filename;
    return NULL;
  }
  return storage.release();
}

StorageInterface *TinyStorage::New() {
  return new TinyStorageImpl;
}

}  // namespace storage
}  // namespace mozc
