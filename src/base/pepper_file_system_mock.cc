// Copyright 2010-2014, Google Inc.
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

#include "base/pepper_file_system_mock.h"

#include <map>
#include <string>

#include "base/mmap.h"
#include "base/mutex.h"

namespace mozc {

PepperFileSystemMock::PepperFileSystemMock() {
}

PepperFileSystemMock::~PepperFileSystemMock() {
}

bool PepperFileSystemMock::Open(pp::Instance *instance, int64 expected_size) {
  scoped_lock l(&mutex_);
  return true;
}

bool PepperFileSystemMock::FileExists(const string &filename) {
  scoped_lock l(&mutex_);
  return file_storage_.find(filename) != file_storage_.end();
}

bool PepperFileSystemMock::DirectoryExists(const string &dirname) {
  scoped_lock l(&mutex_);
  // Currently we don't use directory in NaCl Mozc.
  return false;
}

bool PepperFileSystemMock::ReadBinaryFile(const string &filename,
                                          string *buffer) {
  scoped_lock l(&mutex_);
  map<string, string>::const_iterator it = file_storage_.find(filename);
  if (it == file_storage_.end()) {
    return false;
  }
  *buffer = it->second;
  return true;
}

bool PepperFileSystemMock::WriteBinaryFile(const string &filename,
                                           const string &buffer) {
  scoped_lock l(&mutex_);
  file_storage_[filename] = buffer;
  return true;
}

bool PepperFileSystemMock::DeleteFile(const string &filename) {
  scoped_lock l(&mutex_);
  return file_storage_.erase(filename);
}

bool PepperFileSystemMock::RenameFile(const string &from, const string &to) {
  scoped_lock l(&mutex_);
  map<string, string>::iterator it = file_storage_.find(from);
  if (it == file_storage_.end()) {
    return false;
  }
  file_storage_[to] = it->second;
  file_storage_.erase(from);
  return true;
}

bool PepperFileSystemMock::RegisterMmap(Mmap *mmap) {
  scoped_lock l(&mutex_);
  return mmap_set_.insert(mmap).second;
}

bool PepperFileSystemMock::UnRegisterMmap(Mmap *mmap) {
  scoped_lock l(&mutex_);
  return mmap_set_.erase(mmap);
}

bool PepperFileSystemMock::SyncMmapToFile() {
  scoped_lock l(&mutex_);
  for (set<Mmap*>::iterator it = mmap_set_.begin();
       it != mmap_set_.end(); ++it) {
    (*it)->SyncToFile();
  }
  return true;
}

}  // namespace mozc
