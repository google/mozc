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

#ifndef MOZC_STORAGE_GENERIC_STORAGE_MANAGER_H_
#define MOZC_STORAGE_GENERIC_STORAGE_MANAGER_H_

#include "session/commands.pb.h"
#include "storage/lru_storage.h"

namespace mozc {

class GenericStorageInterface;

// For unit test.
class GenericLruStorageProxy;

// Override and set the subclass's instance to
// GenericStorageManager for unit test.
class GenericStorageManagerInterface {
 public:
  GenericStorageManagerInterface() {}
  virtual ~GenericStorageManagerInterface() {}
  virtual GenericStorageInterface *GetStorage(
     commands::GenericStorageEntry::StorageType storage_type) = 0;
};

// Manages generic storages.
class GenericStorageManagerFactory {
 public:
  // Returns corresponding storage's instance.
  // If no instance is available, NULL is returned.
  static GenericStorageInterface *GetStorage(
     commands::GenericStorageEntry::StorageType storage_type);
  // For unit test.
  static void SetGenericStorageManager(GenericStorageManagerInterface *manager);

 private:
  GenericStorageManagerFactory() {}
  ~GenericStorageManagerFactory() {}
};

// Generic interface for storages.
// This class defines only the interfaces.
// Detailed behaviors depend on the subclass's
// backend.
class GenericStorageInterface {
 public:
  GenericStorageInterface() {}
  virtual ~GenericStorageInterface() {}

  // Inserts new entry.
  // If something goes wrong, returns false.
  virtual bool Insert(const string &key, const char *value) = 0;
  // Looks up the value.
  // If something goes wrong, returns NULL.
  virtual const char *Lookup(const string &key) = 0;
  // Lists all the values.
  // If something goes wrong, returns false.
  virtual bool GetAllValues(vector<string> *values) = 0;
  // Clears all the entries.
  virtual bool Clear() = 0;
};

// Storage class of which backend is LRUStorage.
class GenericLruStorage : public GenericStorageInterface {
 public:
  GenericLruStorage(const char *file_name,
                    size_t value_size,
                    size_t size,
                    uint32 seed) :
      file_name_(file_name),
      value_size_(value_size),
      size_(size),
      seed_(seed) {}
  virtual ~GenericLruStorage() {}

  // If the storage has |key|, this method overwrites
  // the old value.
  // If the entiry's size is over GetSize(),
  // the oldest value is disposed.
  virtual bool Insert(const string &key, const char *value);

  virtual const char *Lookup(const string &key);

  // The order is new to old.
  virtual bool GetAllValues(vector<string> *values);

  virtual bool Clear();

 protected:
  // Opens the storage if not opened yet.
  // If something goes wrong, returns false.
  bool EnsureStorage();

 private:
  friend class GenericLruStorageProxy;
  scoped_ptr<LRUStorage> lru_storage_;
  const string file_name_;
  const size_t value_size_;
  const size_t size_;
  const uint32 seed_;
};

}  // namespace mozc

#endif  // MOZC_STORAGE_GENERIC_STORAGE_MANAGER_H_
