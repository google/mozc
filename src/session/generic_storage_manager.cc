// Copyright 2010-2012, Google Inc.
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

#include "session/generic_storage_manager.h"

#include "base/config_file_stream.h"
#include "base/mutex.h"
#include "base/singleton.h"

namespace {

mozc::Mutex g_storage_ensure_mutex;

mozc::GenericStorageManagerInterface *g_storage_manager = NULL;

const char kSymbolStorageFileName[] =
    "user://symbol_history.db";
// 32 characters * 3 bytes(typical byte size per character)
const size_t kSymbolValueSize = 32 * 3;
const size_t kSymbolSize = 100;
const uint32 kSymbolSeed = 336843897;

const char kEmoticonStorageFileName[] =
    "user://emoticon_history.db";
// 64 characters * 3 bytes(typical byte size per character)
const size_t kEmoticonValueSize = 64 * 3;
const size_t kEmoticonSize = 100;
const uint32 kEmoticonSeed = 236843897;

const char kEmojiStorageFileName[] =
    "user://emoji_history.db";
// 32 characters * 3 bytes(typical byte size per character)
const size_t kEmojiValueSize = 32 * 3;
const size_t kEmojiSize = 100;
const uint32 kEmojiSeed = 136843897;

}  // namespace

namespace mozc {

class GenericStorageManagerImpl
    : public GenericStorageManagerInterface {
 public:
  GenericStorageManagerImpl() :
      symbol_history_storage_(kSymbolStorageFileName,
                              kSymbolValueSize,
                              kSymbolSize,
                              kSymbolSeed),
      emoticon_history_storage_(kEmoticonStorageFileName,
                                kEmoticonValueSize,
                                kEmoticonSize,
                                kEmoticonSeed),
      emoji_history_storage_(kEmojiStorageFileName,
                             kEmojiValueSize,
                             kEmojiSize,
                             kEmojiSeed) {}
  virtual ~GenericStorageManagerImpl() {}
  virtual GenericStorageInterface *GetStorage(
     commands::GenericStorageEntry::StorageType storage_type);
 private:
  GenericLruStorage symbol_history_storage_;
  GenericLruStorage emoticon_history_storage_;
  GenericLruStorage emoji_history_storage_;
};

GenericStorageInterface *GenericStorageManagerImpl::GetStorage(
     commands::GenericStorageEntry::StorageType storage_type) {
  switch (storage_type) {
    case commands::GenericStorageEntry::SYMBOL_HISTORY:
      return &symbol_history_storage_;
    case commands::GenericStorageEntry::EMOTICON_HISTORY:
      return &emoticon_history_storage_;
    case commands::GenericStorageEntry::EMOJI_HISTORY:
      return &emoji_history_storage_;
    default:
      LOG(WARNING) << "Invalid storage type";
  }
  return NULL;
}

// static
void GenericStorageManagerFactory::SetGenericStorageManager(
    GenericStorageManagerInterface *manager) {
  g_storage_manager = manager;
}

// static
GenericStorageInterface *GenericStorageManagerFactory::GetStorage(
     commands::GenericStorageEntry::StorageType storage_type) {
  GenericStorageManagerInterface *manager = g_storage_manager ?
      g_storage_manager : Singleton<GenericStorageManagerImpl>::get();
  return manager->GetStorage(storage_type);
}

bool GenericLruStorage::EnsureStorage() {
  scoped_lock lock(&g_storage_ensure_mutex);
  if (lru_storage_.get()) {
    // We already have prepared storage.
    return true;
  }
  scoped_ptr<LRUStorage> new_storage;
  new_storage.reset(new LRUStorage());
  const string &filename =
      ConfigFileStream::GetFileName(file_name_);
  if (!new_storage->OpenOrCreate(filename.data(), value_size_, size_, seed_)) {
    return false;
  };
  lru_storage_.swap(new_storage);
  return true;
}

bool GenericLruStorage::Insert(const string &key, const char *value) {
  if (!EnsureStorage()) {
    return false;
  }
  return lru_storage_->Insert(key, value);
}

const char *GenericLruStorage::Lookup(const string &key) {
  if (!EnsureStorage()) {
    return NULL;
  }
  return lru_storage_->Lookup(key);
}

bool GenericLruStorage::GetAllValues(vector<string> *values) {
  if (!EnsureStorage()) {
    return false;
  }
  return lru_storage_->GetAllValues(values);
}

bool GenericLruStorage::Clear() {
  if (!EnsureStorage()) {
    return false;
  }
  return lru_storage_->Clear();
}

}  // namespace mozc
