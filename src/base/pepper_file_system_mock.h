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

#ifndef MOZC_BASE_PEPPER_FILE_SYSTEM_MOCK_H_
#define MOZC_BASE_PEPPER_FILE_SYSTEM_MOCK_H_

#include <string>

#include "base/pepper_file_util.h"
#include "base/port.h"

namespace pp {
class Instance;
}  // namespace pp

namespace mozc {

// Mock implementation of Papper file system.
// Currently all method just return false.
// TODO(horo) Implement these methods correctly.
class PepperFileSystemMock : public PepperFileSystemInterface {
 public:
  PepperFileSystemMock();
  virtual ~PepperFileSystemMock();
  virtual bool Open(pp::Instance *instance, int64 expected_size);
  virtual bool FileExists(const string &filename);
  virtual bool DirectoryExists(const string &dirname);
  virtual bool ReadBinaryFile(const string &filename, string *buffer);
  virtual bool WriteBinaryFile(const string &filename,
                               const string &buffer);
  virtual bool DeleteFile(const string &filename);
  virtual bool RenameFile(const string &from, const string &to);
  virtual bool RegisterMmap(Mmap *mmap);
  virtual bool UnRegisterMmap(Mmap *mmap);
  virtual bool SyncMmapToFile();

 private:
  DISALLOW_COPY_AND_ASSIGN(PepperFileSystemMock);
};

}  // namespace mozc

#endif  // MOZC_BASE_PEPPER_FILE_SYSTEM_MOCK_H_
