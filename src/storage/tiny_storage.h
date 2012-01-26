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

#ifndef MOZC_STORAGE_TINY_STORAGE_H_
#define MOZC_STORAGE_TINY_STORAGE_H_

#include <map>
#include <string>
#include "base/base.h"

namespace mozc {
namespace storage {

class StorageInterface {
 public:
  // Open storage
  virtual bool Open(const string &filename) = 0;

  // Syncing into the disk
  virtual bool Sync() = 0;

  // Lookup key and return the value.
  // if key is not found, return false.
  // It is not guranteed that the data is synced to the disk.
  virtual bool Lookup(const string &key, string *value) const = 0;

  // Insert key and value
  // It is not guranteed that the data is synced to the disk.
  virtual bool Insert(const string &key, const string &value) = 0;

  // Erase key
  // It is not guranteed that the data is synced to the disk
  virtual bool Erase(const string &key) = 0;

  // clear internal keys and values
  // Sync() is automatically called.
  virtual bool Clear() = 0;

  // return the size of keys
  virtual size_t Size() const = 0;

  StorageInterface() {}
  virtual ~StorageInterface() {}
};

// Very simple and tiny key/value storage.
// Use it just for saving small data which
// are not updated frequently, like timestamp, auth_token, etc.
// We will replace it with faster and more robust implementation.
class TinyStorage: public StorageInterface {
 public:
  TinyStorage();
  virtual ~TinyStorage();

  bool Open(const string &filename);
  bool Sync();
  bool Lookup(const string &key, string *value) const;
  bool Insert(const string &key, const string &value);
  bool Erase(const string &key);
  bool Clear();
  size_t Size() const {
    return dic_.size();
  }

  // return TinyStorage instance
  static TinyStorage *Create(const char *filename);

 private:
  string filename_;
  bool should_sync_;
  map<string, string> dic_;
};
}  // storage
}  // mozc
#endif  // MOZC_STORAGE_TINY_STORAGE_H_
