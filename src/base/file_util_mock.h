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

#ifndef MOZC_BASE_FILE_UTIL_MOCK_H_
#define MOZC_BASE_FILE_UTIL_MOCK_H_

#include <map>
#include <string>

#include "base/file_util.h"

namespace mozc {
class FileUtilMock : public FileUtilInterface {
 public:
  FileUtilMock() {
    FileUtil::SetMockForUnitTest(this);
  }
  ~FileUtilMock() override {
    FileUtil::SetMockForUnitTest(nullptr);
  }

  // This is not an override function.
  void CreateFile(const std::string &path) {
    files_[path] = base_id_;
    base_id_ += 100'000;
  }

  bool CreateDirectory(const std::string &path) const override {
    if (FileExists(path)) {
      return false;
    }
    dirs_[path] = true;
    return true;
  }

  bool RemoveDirectory(const std::string &dirname) const override {
    if (FileExists(dirname)) {
      return false;
    }
    dirs_[dirname] = false;
    return true;
  }

  bool Unlink(const std::string &filename) const override {
    if (DirectoryExists(filename)) {
      return false;
    }
    files_[filename] = 0;
    return true;
  }

  bool FileExists(const std::string &filename) const override {
    auto it = files_.find(filename);
    return it != files_.end() && it->second > 0;
  }

  bool DirectoryExists(const std::string &dirname) const override {
    auto it = dirs_.find(dirname);
    return it != dirs_.end() ? it->second : false;
  }

  bool CopyFile(const std::string &from, const std::string &to) const override {
    if (!FileExists(from)) {
      return false;
    }

    files_[to] = files_[from];
    return true;
  }

  bool IsEqualFile(const std::string &filename1,
                   const std::string &filename2) const override {
    return (FileExists(filename1) &&
            FileExists(filename2) &&
            files_[filename1] == files_[filename2]);
  }

  bool AtomicRename(const std::string &from,
                    const std::string &to) const override {
    if (FileExists(from)) {
      files_[to] = files_[from];
      files_[from] = 0;
      return true;
    }
    if (DirectoryExists(from)) {
      dirs_[to] = dirs_[from];
      dirs_[from] = false;
      return true;
    }
    return false;
  }

  bool GetModificationTime(const std::string &filename,
                           FileTimeStamp *modified_at) const override {
    if (!FileExists(filename)) {
      return false;
    }
    *modified_at = files_[filename];
    return true;
  }

  // Use FileTimeStamp as time stamp and also file id.
  // 0 means that the file is removed.
  mutable std::map<std::string, FileTimeStamp> files_;
  FileTimeStamp base_id_ {1'000'000'000};
  mutable std::map<std::string, bool> dirs_;
};
}  // namespace mozc

#endif  // MOZC_BASE_FILE_UTIL_MOCK_H_
