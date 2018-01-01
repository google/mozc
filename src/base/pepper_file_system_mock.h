// Copyright 2010-2018, Google Inc.
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

#ifdef OS_NACL

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/mmap_sync_interface.h"
#include "base/mutex.h"
#include "base/pepper_file_util.h"
#include "base/port.h"

namespace pp {
class Instance;
}  // namespace pp

namespace mozc {
namespace internal {
class MockFileNode {
 public:
  MockFileNode();
  MockFileNode(MockFileNode *parent_node, const string &name,
               bool is_directory);
  ~MockFileNode();
  bool FileExists(const string &filename) const;
  bool DirectoryExists(const string &dirname) const;
  bool IsDirectory() const;
  bool GetFileContent(string *buffer) const;
  bool SetFile(const string &filename, const string &content);
  bool AddDirectory(const string &dirname);
  bool Rename(const string &fiename, MockFileNode *parent_node);
  bool Delete();
  bool Query(PP_FileInfo *file_info) const;
  MockFileNode *GetNode(const string &path);
  string DebugMessage() const;

 private:
  MockFileNode *parent_node_;  // nullptr if this is a root node.
  string name_;
  std::map<string, std::unique_ptr<MockFileNode> > child_nodes_;
  bool is_directory_;
  string content_;

  DISALLOW_COPY_AND_ASSIGN(MockFileNode);
};
}  // namespace internal

// Mock implementation of Papper file system.
// Currently all method just return false.
class PepperFileSystemMock : public PepperFileSystemInterface {
 public:
  PepperFileSystemMock();
  virtual ~PepperFileSystemMock();
  virtual bool Open(pp::Instance *instance, int64 expected_size);
  virtual bool FileExists(const string &filename);
  virtual bool DirectoryExists(const string &dirname);
  virtual bool ReadBinaryFile(const string &filename, string *buffer);
  virtual bool WriteBinaryFile(const string &filename, const string &buffer);
  virtual bool CreateDirectory(const string &dirname);
  virtual bool Delete(const string &path);
  virtual bool Rename(const string &from, const string &to);
  virtual bool RegisterMmap(MmapSyncInterface *mmap);
  virtual bool UnRegisterMmap(MmapSyncInterface *mmap);
  virtual bool SyncMmapToFile();
  virtual bool Query(const string &filename, PP_FileInfo *file_info);

 private:
  std::set<MmapSyncInterface*> mmap_set_;
  internal::MockFileNode root_directory_;
  Mutex mutex_;

  DISALLOW_COPY_AND_ASSIGN(PepperFileSystemMock);
};

}  // namespace mozc

#endif  // OS_NACL

#endif  // MOZC_BASE_PEPPER_FILE_SYSTEM_MOCK_H_
