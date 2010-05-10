// Copyright 2010, Google Inc.
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

#ifndef MOZC_STORAGE_LRU_STORAGE_H_
#define MOZC_STORAGE_LRU_STORAGE_H_

#include <map>
#include <string>
#include "base/base.h"

namespace mozc {

template <class T> class Mmap;

class LRUList;
struct LRUList_Node;

class LRUStorage {
 public:
  bool Open(const char *filename);
  bool Open(char *ptr, size_t ptr_size);  // load from memory buffer
  void Close();

  // Try to open exisiting database
  // If the file is broken or cannot open, tries to recreate
  // new file
  bool OpenOrCreate(const char *filename,
                    size_t value_size,
                    size_t size,
                    uint32 seed);

  // Lookup key
  const char * Lookup(const string &key,
                      uint32 *last_access_time) const;

  const char * Lookup(const string &key) const;

  // clear all LRU cache;
  // mapped file is also initialized
  void Clear();

  // Merge from other data
  bool Merge(const char *filename);
  bool Merge(const char *ptr, size_t ptr_size);

  // update timestamp
  bool Touch(const string &key);

  // Insert key and values
  bool Insert(const string &key,
              const char *value);

  // if key is found, insert value,
  // otherwise return
  bool TryInsert(const string &key,
                 const char *value);

  size_t value_size() const {
    return value_size_;
  }

  size_t size() const {
    return size_;
  }

  size_t used_size() const;

  // Create Instance from file. Call Open internally
  static LRUStorage *Create(const char *filename);

  // Create Instance from file. Call OpenOrCreate internally
  static LRUStorage *Create(const char *filename,
                            size_t value_size,
                            size_t size,
                            uint32 seed);

  // Create an empty LRU db file
  static bool CreateStorageFile(const char *filename,
                                size_t value_size,
                                size_t size,
                                uint32 seed);

  LRUStorage();
  virtual ~LRUStorage();

 private:
  size_t value_size_;
  size_t size_;
  uint32 seed_;
  char *last_item_;
  char *begin_;
  char *end_;
  map<uint64, LRUList_Node *> map_;
  scoped_ptr<LRUList> lru_list_;
  scoped_ptr<Mmap<char> > mmap_;
};
}

#endif  // MOZC_STORAGE_LRU_STORAGE_H_
