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

// TODO(horo) Implement correctly and write tests.

#include "base/pepper_file_system_mock.h"

namespace mozc {

PepperFileSystemMock::PepperFileSystemMock() {
}

PepperFileSystemMock::~PepperFileSystemMock() {
}

bool PepperFileSystemMock::Open(pp::Instance *instance, int64 expected_size) {
  // TODO(horo) Implement this method correctly.
  return false;
}

bool PepperFileSystemMock::FileExists(const string &filename) {
  // TODO(horo) Implement this method correctly.
  return false;
}

bool PepperFileSystemMock::DirectoryExists(const string &dirname) {
  // TODO(horo) Implement this method correctly.
  return false;
}

bool PepperFileSystemMock::ReadBinaryFile(const string &filename,
                                          string *buffer) {
  // TODO(horo) Implement this method correctly.
  return false;
}

bool PepperFileSystemMock::WriteBinaryFile(const string &filename,
                                           const string &buffer) {
  // TODO(horo) Implement this method correctly.
  return false;
}

bool PepperFileSystemMock::DeleteFile(const string &filename) {
  // TODO(horo) Implement this method correctly.
  return false;
}
bool PepperFileSystemMock::RenameFile(const string &from, const string &to) {
  // TODO(horo) Implement this method correctly.
  return false;
}

bool PepperFileSystemMock::RegisterMmap(Mmap *mmap) {
  // TODO(horo) Implement this method correctly.
  return false;
}
bool PepperFileSystemMock::UnRegisterMmap(Mmap *mmap) {
  // TODO(horo) Implement this method correctly.
  return false;
}
bool PepperFileSystemMock::SyncMmapToFile() {
  // TODO(horo) Implement this method correctly.
  return false;
}

}  // namespace mozc
