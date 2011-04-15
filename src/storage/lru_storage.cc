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

#include <time.h>
#include <stdlib.h>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <algorithm>
#include "base/base.h"
#include "base/file_stream.h"
#include "base/util.h"
#include "base/mmap.h"
#include "storage/lru_storage.h"

namespace {

const size_t kMaxLRUSize   = 1000000;  // 1M
const size_t kMaxValueSize = 1024;     // 1024 byte

template <class T>
inline void ReadValue(char **ptr, T *value) {
  memcpy(value, *ptr, sizeof(*value));
  *ptr += sizeof(*value);
}

uint64 GetFP(const char *ptr) {
  return *reinterpret_cast<const uint64 *>(ptr);
}

uint32 GetTimeStamp(const char *ptr) {
  return *reinterpret_cast<const uint32 *>(ptr + 8);
}

const char* GetValue(const char *ptr) {
  return ptr + 12;
}

void Update(char *ptr) {
  const uint32 last_access_time = static_cast<uint32>(time(NULL));
  memcpy(ptr + 8, reinterpret_cast<const char *>(&last_access_time), 4);
}

void Update(char *ptr, uint64 fp, const char *value, size_t value_size) {
  const uint32 last_access_time = static_cast<uint32>(time(NULL));
  memcpy(ptr,     reinterpret_cast<const char *>(&fp), 8);
  memcpy(ptr + 8, reinterpret_cast<const char *>(&last_access_time), 4);
  memcpy(ptr + 12, value, value_size);
}

class CompareByTimeStamp {
 public:
  bool operator()(const char *a, const char *b) const {
    return GetTimeStamp(a) > GetTimeStamp(b);
  }
};
}

namespace mozc {

struct LRUList_Node {
  LRUList_Node *next;
  LRUList_Node *prev;
  char *value;
  LRUList_Node(): next(NULL), prev(NULL), value(NULL) {}
};

class LRUList {
 public:
  explicit LRUList(size_t max_size)
      : max_size_(max_size), size_(0),
        last_(NULL), top_(NULL) {}

  virtual ~LRUList() {
    Clear();
  }

  void Clear() {
    LRUList_Node *node = top_;
    while (node != NULL) {
      LRUList_Node *next = node->next;
      delete node;
      node = next;
    }
    size_ = 0;
    top_ = last_ = NULL;
  }

  LRUList_Node *Add(char *value) {
    if (size_ < max_size_) {
      LRUList_Node *node = new LRUList_Node;
      node->value = value;
      if (last_ == NULL) {
        node->prev = NULL;
        top_ = node;
      } else {
        last_->next = node;
      }
      node->next = NULL;
      node->prev = last_;
      last_ = node;
      ++size_;
      return node;
    }
    LOG(WARNING) << "LRUList is full";
    return NULL;
  }

  bool empty() const {
    return (top_ == NULL);
  }

  size_t size() const {
    return size_;
  }

  LRUList_Node *GetLastNode() {
    return last_;
  }

  void MoveToTop(LRUList_Node *node) {
    if (node->prev != NULL) {  // this is top
      LRUList_Node *prev = node->prev;
      LRUList_Node *next = node->next;
      prev->next = next;
      if (next == NULL) {
        last_ = prev;
      } else {
        next->prev = prev;
      }
      node->next = top_;
      top_->prev = node;
      top_ = node;
      top_->prev = NULL;
    }
  }

 private:
  size_t max_size_;
  size_t size_;
  LRUList_Node *last_;
  LRUList_Node *top_;
};

LRUStorage *LRUStorage::Create(const char *filename) {
  LRUStorage *n = new LRUStorage;
  if (!n->Open(filename)) {
    delete n;
    LOG(ERROR) << "could not open LRUStorage";
    return NULL;
  }
  return n;
}

LRUStorage *LRUStorage::Create(const char *filename,
                               size_t value_size,
                               size_t size,
                               uint32 seed) {
  LRUStorage *n = new LRUStorage;
  if (!n->OpenOrCreate(filename, value_size, size, seed)) {
    delete n;
    LOG(ERROR) << "could not open LRUStorage";
    return NULL;
  }
  return n;
}

bool LRUStorage::CreateStorageFile(const char *filename,
                                   size_t value_size,
                                   size_t size,
                                   uint32 seed) {
  if (value_size == 0 || value_size > kMaxValueSize) {
    LOG(ERROR) << "value_size is out of range";
    return false;
  }

  if (size == 0 || size > kMaxLRUSize) {
    LOG(ERROR) << "size is out of range";
    return false;
  }

  if (value_size % 4 != 0) {
    LOG(ERROR) << "value_size_ must be 4 byte alignment";
    return false;
  }

  OutputFileStream ofs(filename, ios::binary|ios::out);
  if (!ofs) {
    LOG(ERROR) << "cannot open " << filename;
    return false;
  }

  const uint32 value_size_uint32 = static_cast<uint32>(value_size);
  const uint32 size_uint32 = static_cast<uint32>(size);

  ofs.write(reinterpret_cast<const char *>(&value_size_uint32),
            sizeof(value_size_uint32));
  ofs.write(reinterpret_cast<const char *>(&size_uint32),
            sizeof(size_uint32));
  ofs.write(reinterpret_cast<const char *>(&seed),
            sizeof(seed));
  vector<char> ary(value_size, '\0');
  const uint32 last_access_time = 0;
  const uint64 fp = 0;

  for (size_t i = 0; i < size; ++i) {
    ofs.write(reinterpret_cast<const char *>(&fp), sizeof(fp));
    ofs.write(reinterpret_cast<const char *>(&last_access_time),
              sizeof(last_access_time));
    ofs.write(reinterpret_cast<const char *>(&ary[0]),
              static_cast<std::streamsize>(ary.size() * sizeof(ary[0])));
  }

  return true;
}

// Reopen file after initializing mapped page.
void LRUStorage::Clear() {
  // Don't need to clear the page if the lru list is empty
  if (mmap_.get() == NULL || lru_list_.get() == NULL ||
      lru_list_->size() == 0) {
    return;
  }
  const size_t offset =
      sizeof(value_size_) + sizeof(size_) + sizeof(seed_);
  if (offset >= mmap_->GetFileSize()) {   // should not happen
    return;
  }
  memset(mmap_->begin() + offset, '\0',
         mmap_->GetFileSize() - offset);
  lru_list_.reset(NULL);
  map_.clear();
  Open(mmap_->begin(), mmap_->GetFileSize());
  return;
}

bool LRUStorage::Merge(const char *filename) {
  LRUStorage target_storage;
  if (!target_storage.Open(filename)) {
    return false;
  }
  return Merge(target_storage);
}

bool LRUStorage::Merge(const LRUStorage &storage) {
  if (storage.value_size() !=  value_size()) {
    return false;
  }

  if (seed_ != storage.seed_) {
    return false;
  }

  vector<const char *> ary;

  // this file
  {
    const char *begin = begin_;
    const char *end = end_;
    while (begin < end) {
      ary.push_back(begin);
      begin += (value_size_ + 12);
    }
  }

  // target file
  {
    const char *begin = storage.begin_;
    const char *end = storage.end_;
    while (begin < end) {
      ary.push_back(begin);
      begin += (value_size_ + 12);
    }
  }

  stable_sort(ary.begin(), ary.end(), CompareByTimeStamp());

  string buf;
  set<uint64> seen;   // remove duplicated entries.
  for (size_t i = 0; i < ary.size(); ++i) {
    if (!seen.insert(GetFP(ary[i])).second) {
      continue;
    }
    buf.append(const_cast<const char *>(ary[i]), value_size_ + 12);
  }

  const size_t old_size = static_cast<size_t>(end_ - begin_);
  const size_t new_size = min(buf.size(), old_size);

  // TODO(taku): this part is not atomic.
  // If the converter process is killed while memcpy or memset is running,
  // the storage data will be broken.
  memcpy(begin_, buf.data(), new_size);
  if (new_size < old_size) {
    memset(begin_ + new_size, '\0', old_size - new_size);
  }

  return Open(mmap_->begin(), mmap_->GetFileSize());
}

LRUStorage::LRUStorage()
    : value_size_(0),
      size_(0),
      seed_(0),
      last_item_(NULL),
      begin_(NULL), end_(NULL) {}

LRUStorage::~LRUStorage() {
  Close();
}

bool LRUStorage::OpenOrCreate(const char *filename,
                              size_t _value_size,
                              size_t _size,
                              uint32 _seed) {
  if (!Open(filename)) {
    Close();
    LOG(ERROR) << "cannot open file. call CreateStorageFile";
    if (!LRUStorage::CreateStorageFile(filename,
                                       _value_size,
                                       _size, _seed)) {
      LOG(ERROR) << "CreateStorageFile failed";
      return false;
    }
    if (!Open(filename)) {
      Close();
      LOG(ERROR) << "Open failed after CreateStorageFile";
      return false;
    }
  }

  // File format has changed
  if (_value_size != value_size() || _size != size()) {
    Close();
    if (!LRUStorage::CreateStorageFile(filename,
                                       _value_size,
                                       _size, _seed)) {
      LOG(ERROR) << "CreateStorageFile failed";
      return false;
    }
    if (!Open(filename)) {
      Close();
      LOG(ERROR) << "Open failed after CreateStorageFile";
      return false;
    }
  }

  if (_value_size != value_size() || _size != size()) {
    Close();
    LOG(ERROR) << "file is broken";
    return false;
  }

  return true;
}

bool LRUStorage::Open(const char *filename) {
  mmap_.reset(new Mmap<char>);

  if (mmap_.get() == NULL) {
    LOG(ERROR) << "cannot make Mmap object";
    return false;
  }

  if (!mmap_->Open(filename, "r+")) {
    LOG(ERROR) << "cannot open " << filename
               << " with read+write mode";
    return false;
  }

  if (mmap_->GetFileSize() < 8) {
    LOG(ERROR) << "file size is too small";
    return false;
  }

  filename_ = filename;
  return Open(mmap_->begin(), mmap_->GetFileSize());
}

bool LRUStorage::Open(char *ptr, size_t ptr_size) {
  begin_ = ptr;
  end_ = ptr + ptr_size;

  uint32 value_size_uint32 = 0;
  uint32 size_uint32 = 0;

  ReadValue<uint32>(&begin_, &value_size_uint32);
  ReadValue<uint32>(&begin_, &size_uint32);
  ReadValue<uint32>(&begin_, &seed_);

  value_size_ = static_cast<size_t>(value_size_uint32);
  size_ = static_cast<size_t>(size_uint32);

  if (value_size_ % 4 != 0) {
    LOG(ERROR) << "value_size_ must be 4 byte alignment";
    return false;
  }

  if (size_ == 0 || size_ > kMaxLRUSize) {
    LOG(ERROR) << "LRU size is invalid: " << size_;
    return false;
  }

  if (value_size_ == 0 || value_size_ > kMaxValueSize) {
    LOG(ERROR) << "value_size is invalid: " << value_size_;
    return false;
  }

  const size_t file_size = mmap_->GetFileSize() - 12;
  if ((value_size_ + 12) * size_ != file_size) {
    LOG(ERROR) << "LRU file is broken";
    return false;
  }

  vector<char *> ary;
  char *begin = begin_;
  char *end = end_;
  while (begin < end) {
    ary.push_back(begin);
    begin += (value_size_ + 12);
  }
  stable_sort(ary.begin(), ary.end(),
              CompareByTimeStamp());

  lru_list_.reset(new LRUList(size_));
  map_.clear();
  last_item_ = NULL;
  for (size_t i = 0; i < ary.size(); ++i) {
    if (GetTimeStamp(ary[i]) != 0) {
      LRUList_Node *node = lru_list_->Add(ary[i]);
      map_.insert(make_pair(GetFP(ary[i]), node));
    } else if (last_item_ == NULL) {
      last_item_ = ary[i];
    }
  }

  return true;
}

void LRUStorage::Close() {
  filename_.clear();
  mmap_.reset(NULL);
  lru_list_.reset(NULL);
  map_.clear();
}

const char* LRUStorage::Lookup(const string &key) const {
  uint32 last_access_time = 0;
  return Lookup(key, &last_access_time);
}

const char* LRUStorage::Lookup(const string &key,
                               uint32 *last_access_time) const {
  const uint64 fp = Util::FingerprintWithSeed(key.data(),
                                              key.size(),
                                              seed_);
  map<uint64, LRUList_Node *>::const_iterator it = map_.find(fp);
  if (it == map_.end()) {
    return NULL;
  }
  *last_access_time = GetTimeStamp(it->second->value);
  return GetValue(it->second->value);
}

bool LRUStorage::Touch(const string &key) {
  const uint64 fp = Util::FingerprintWithSeed(key.data(),
                                              key.size(),
                                              seed_);
  map<uint64, LRUList_Node *>::iterator it = map_.find(fp);
  if (it != map_.end()) {     // find in the cache
    Update(it->second->value);
    lru_list_->MoveToTop(it->second);
    return true;
  }
  return false;
}

bool LRUStorage::Insert(const string &key, const char *value) {
  const uint64 fp = Util::FingerprintWithSeed(key.data(),
                                              key.size(),
                                              seed_);
  map<uint64, LRUList_Node *>::iterator it = map_.find(fp);
  if (it != map_.end()) {     // find in the cache
    Update(it->second->value, fp, value, value_size_);
    lru_list_->MoveToTop(it->second);
  } else if (lru_list_->size() >= size_ ||
             last_item_ == NULL) {  // not found, but cache is FULL
    LRUList_Node *node = lru_list_->GetLastNode();
    const uint64 old_fp = GetFP(node->value);  // remove oldest item
    map<uint64, LRUList_Node *>::iterator old_it = map_.find(old_fp);
    if (old_it != map_.end()) {
      map_.erase(old_it);
    }
    lru_list_->MoveToTop(node);
    Update(node->value, fp, value, value_size_);
    map_.insert(make_pair(fp, node));
  } else if (last_item_ < mmap_->end()) {  // not found, cahce is not FULL
    LRUList_Node *node = lru_list_->Add(last_item_);
    lru_list_->MoveToTop(node);
    Update(node->value, fp, value, value_size_);
    map_.insert(make_pair(fp, node));
    last_item_ += (value_size_ + 12);
    if (last_item_ >= mmap_->end()) {
      last_item_ = NULL;
    }
  } else {
    LOG(ERROR) << "insertion failed";
    return false;
  }

  return true;
}

bool LRUStorage::TryInsert(const string &key, const char *value) {
  const uint64 fp = Util::FingerprintWithSeed(key.data(),
                                              key.size(),
                                              seed_);
  map<uint64, LRUList_Node *>::iterator it = map_.find(fp);
  if (it != map_.end()) {     // find in the cache
    Update(it->second->value, fp, value, value_size_);
    lru_list_->MoveToTop(it->second);
  }

  return true;
}

size_t LRUStorage::value_size() const {
  return value_size_;
}

size_t LRUStorage::size() const {
  return size_;
}

size_t LRUStorage::used_size() const {
  return lru_list_.get() == NULL ? 0 : lru_list_->size();
}

uint32 LRUStorage::seed() const {
  return seed_;
}

const string &LRUStorage::filename() const {
  return filename_;
}

void LRUStorage::Write(size_t i,
                       uint64 fp,
                       const string &value,
                       uint32 last_access_time) {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, size_);
  char *ptr = begin_ + (i * (value_size_ + 12));
  memcpy(ptr,     reinterpret_cast<const char *>(&fp), 8);
  memcpy(ptr + 8, reinterpret_cast<const char *>(&last_access_time), 4);
  if (value.size() == value_size_) {
    memcpy(ptr + 12, value.data(), value_size_);
  } else {
    LOG(ERROR) << "value size is not " << value_size_ << " byte.";
  }
}

void LRUStorage::Read(size_t i,
                      uint64 *fp,
                      string *value,
                      uint32 *last_access_time) const {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, size_);
  const char *ptr = begin_ + (i * (value_size_ + 12));
  *fp = GetFP(ptr);
  value->assign(GetValue(ptr), value_size_);
  *last_access_time = GetTimeStamp(ptr);
}
}   // end of mozc
