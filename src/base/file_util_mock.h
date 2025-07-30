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

#include <string>

#include "absl/container/btree_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "base/file_util.h"
#include "base/strings/zstring_view.h"

namespace mozc {
class FileUtilMock : public FileUtilInterface {
 public:
  FileUtilMock() { FileUtil::SetMockForUnitTest(this); }
  ~FileUtilMock() override { FileUtil::SetMockForUnitTest(nullptr); }

  // This is not an override function.
  void CreateFile(zstring_view path) {
    files_[path.view()] = base_id_;
    base_id_ += 100'000;
  }

  absl::Status CreateDirectory(zstring_view path) const override {
    if (FileExists(path).ok()) {
      return absl::AlreadyExistsError(path);
    }
    dirs_[path.view()] = true;
    return absl::OkStatus();
  }

  absl::Status RemoveDirectory(zstring_view dirname) const override {
    if (FileExists(dirname).ok()) {
      return absl::NotFoundError(dirname);
    }
    dirs_[dirname.view()] = false;
    return absl::OkStatus();
  }

  absl::Status Unlink(zstring_view filename) const override {
    if (DirectoryExists(filename).ok()) {
      return absl::FailedPreconditionError(
          absl::StrFormat("%s is a directory", filename.view()));
    }
    files_[filename.view()] = 0;
    return absl::OkStatus();
  }

  absl::Status FileExists(zstring_view filename) const override {
    const auto it = files_.find(filename);
    return it != files_.end() && it->second > 0 ? absl::OkStatus()
                                                : absl::NotFoundError(filename);
  }

  absl::Status DirectoryExists(zstring_view dirname) const override {
    auto it = dirs_.find(dirname);
    return it != dirs_.end() && it->second ? absl::OkStatus()
                                           : absl::NotFoundError(dirname);
  }

  absl::Status FileOrDirectoryExists(zstring_view path) const {
    return FileExists(path).ok() || DirectoryExists(path).ok()
               ? absl::OkStatus()
               : absl::NotFoundError(path);
  }

  absl::Status CopyFile(zstring_view from, zstring_view to) const override {
    if (!FileExists(from).ok()) {
      return absl::NotFoundError(from);
    }
    files_[to.view()] = files_[from.view()];
    return absl::OkStatus();
  }

  absl::StatusOr<bool> IsEqualFile(zstring_view filename1,
                                   zstring_view filename2) const override {
    if (absl::Status s = FileExists(filename1); !s.ok()) {
      return s;
    }
    if (absl::Status s = FileExists(filename2); !s.ok()) {
      return s;
    }
    return files_[filename1.view()] == files_[filename2.view()];
  }

  absl::StatusOr<bool> IsEquivalent(zstring_view filename1,
                                    zstring_view filename2) const override {
    const std::string canonical1 = At(canonical_paths_, filename1, filename1);
    const std::string canonical2 = At(canonical_paths_, filename2, filename2);
    // If either of files does not exist, an error is returned.
    if (FileExists(canonical1).ok() != FileExists(canonical2).ok()) {
      return absl::UnknownError("No such file or directory");
    }
    return canonical1 == canonical2;
  }

  absl::Status AtomicRename(zstring_view from, zstring_view to) const override {
    if (FileExists(from).ok()) {
      files_[to.view()] = files_[from.view()];
      files_[from.view()] = 0;
      return absl::OkStatus();
    }
    if (DirectoryExists(from).ok()) {
      dirs_[to.view()] = dirs_[from.view()];
      dirs_[from.view()] = false;
      return absl::OkStatus();
    }
    return absl::NotFoundError(
        absl::StrFormat("%s doesn't exist", from.view()));
  }

  absl::Status CreateHardLink(zstring_view from, zstring_view to) override {
    // Error if `from` doesn't exist.
    if (absl::Status s = FileOrDirectoryExists(from); !s.ok()) {
      return s;
    }
    // Error if `to` already exists.
    if (absl::Status s = FileOrDirectoryExists(to); s.ok()) {
      return absl::AlreadyExistsError(to);
    }
    canonical_paths_[to.view()] = std::string(from.view());
    if (FileExists(from).ok()) {
      CreateFile(to);
      return absl::OkStatus();
    }
    return CreateDirectory(to);
  }

  absl::StatusOr<FileTimeStamp> GetModificationTime(
      zstring_view filename) const override {
    if (absl::Status s = FileExists(filename); !s.ok()) {
      return s;
    }
    return files_[filename.view()];
  }

  // Use FileTimeStamp as time stamp and also file id.
  // 0 means that the file is removed.
  mutable absl::btree_map<std::string, FileTimeStamp> files_;
  FileTimeStamp base_id_{1'000'000'000};
  mutable absl::btree_map<std::string, bool> dirs_;
  mutable absl::btree_map<std::string, std::string> canonical_paths_;

 private:
  template <typename T>
  T At(const absl::btree_map<std::string, T> &map, zstring_view key,
       zstring_view fallback_value) const {
    auto it = map.find(key);
    return it == map.end() ? std::string(fallback_value.view()) : it->second;
  }
};
}  // namespace mozc

#endif  // MOZC_BASE_FILE_UTIL_MOCK_H_
