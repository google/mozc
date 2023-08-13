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

#ifndef MOZC_STORAGE_STORAGE_INTERFACE_H_
#define MOZC_STORAGE_STORAGE_INTERFACE_H_

#include <cstddef>
#include <string>


namespace mozc {
namespace storage {

class StorageInterface {
 public:
  StorageInterface() = default;
  virtual ~StorageInterface() = default;

  // Binds |filename| to the storage but the interpretation of the
  // |filename| depends on the implementation. implementations can
  // - ignore the specified |filename|.
  // - load the existing data from |filename|.
  // - return true if the specified |filename| does not exist.
  // Despite its name, an implementation may not keep the underlaying
  // data storage specified by |filename| opened.
  // StorageInterface::Sync may try to open |filename| again but it
  // may fail even when this method returns true.
  virtual bool Open(const std::string &filename) = 0;

  // Flushes on-memory data into persistent storage (usually on
  // disk) specified by |filename|.
  virtual bool Sync() = 0;

  // Looks up key and returns the value.
  // If key is not found, returns false.
  // It is not guaranteed that the data is synced to the disk.
  virtual bool Lookup(const std::string &key, std::string *value) const = 0;

  // Inserts key and value.
  // It is not guaranteed that the data is synced to the disk.
  virtual bool Insert(const std::string &key, const std::string &value) = 0;

  // Erases the key-value pair specified by |key|.
  // It is not guaranteed that the data is synced to the disk
  virtual bool Erase(const std::string &key) = 0;

  // clears internal keys and values
  // Sync() is automatically called.
  virtual bool Clear() = 0;

  // Returns the number of keys, not a number of bytes.
  virtual size_t Size() const = 0;
};

}  // namespace storage
}  // namespace mozc

#endif  // MOZC_STORAGE_STORAGE_INTERFACE_H_
