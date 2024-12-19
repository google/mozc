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

#ifndef MOZC_BASE_FILE_UTIL_H_
#define MOZC_BASE_FILE_UTIL_H_

#include <ctime>
#include <ios>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"

#ifdef _WIN32
#include <windows.h>
#endif  // _WIN32

// Ad-hoc workaround against macro problem on Windows.
// On Windows, following macros, defined when you include <windows.h>,
// should be removed here because they affects the method name definition of
// Util class.
// TODO(yukawa): Use different method name if applicable.
#undef CreateDirectory
#undef RemoveDirectory
#undef CopyFile

namespace mozc {

#if defined(_WIN32)
using FileTimeStamp = uint64_t;
#else   // _WIN32
using FileTimeStamp = time_t;
#endif  // _WIN32

class FileUtilInterface {
 public:
  virtual ~FileUtilInterface() = default;

  virtual absl::Status CreateDirectory(const std::string &path) const = 0;
  virtual absl::Status RemoveDirectory(const std::string &dirname) const = 0;
  virtual absl::Status Unlink(const std::string &filename) const = 0;
  virtual absl::Status FileExists(const std::string &filename) const = 0;
  virtual absl::Status DirectoryExists(const std::string &dirname) const = 0;
  virtual absl::Status CopyFile(const std::string &from,
                                const std::string &to) const = 0;
  virtual absl::StatusOr<bool> IsEqualFile(
      const std::string &filename1, const std::string &filename2) const = 0;
  virtual absl::StatusOr<bool> IsEquivalent(
      const std::string &filename1, const std::string &filename2) const = 0;
  virtual absl::Status AtomicRename(const std::string &from,
                                    const std::string &to) const = 0;
  virtual absl::Status CreateHardLink(const std::string &from,
                                      const std::string &to) = 0;
  virtual absl::StatusOr<FileTimeStamp> GetModificationTime(
      const std::string &filename) const = 0;

 protected:
  FileUtilInterface() = default;
};

class FileUtil {
 public:
  FileUtil() = delete;
  ~FileUtil() = delete;

  // Creates a directory. Does not create directories in the way to the path.
  // If the directory already exists:
  // - On Windows: returns an error status.
  // - Others: returns OkStatus and does nothing.
  //
  // The above platform dependent behavior is a temporary solution to avoid
  // freeze of the host application.
  // https://github.com/google/mozc/issues/1076
  static absl::Status CreateDirectory(const std::string &path);

  // Removes an empty directory.
  static absl::Status RemoveDirectory(const std::string &dirname);
  static absl::Status RemoveDirectoryIfExists(const std::string &dirname);

  // Removes a file. The second version returns OK when `filename` doesn't
  // exist. The third version logs error message on failure (i.e., it ignores
  // any error and is not recommended).
  static absl::Status Unlink(const std::string &filename);
  static absl::Status UnlinkIfExists(const std::string &filename);
  static void UnlinkOrLogError(const std::string &filename);

  // Returns true if a file or a directory with the name exists.
  static absl::Status FileExists(const std::string &filename);

  // Returns true if the directory exists.
  static absl::Status DirectoryExists(const std::string &dirname);

#ifdef _WIN32
  // Adds file attributes to the file to hide it.
  // FILE_ATTRIBUTE_NORMAL will be removed.
  static bool HideFile(const std::string &filename);
  static bool HideFileWithExtraAttributes(const std::string &filename,
                                          DWORD extra_attributes);
#endif  // _WIN32

  // Copies a file to another file, using mmap internally.
  // The destination file will be overwritten if exists.
  // Returns true if the file is copied successfully.
  static absl::Status CopyFile(const std::string &from, const std::string &to);

  // Compares the contents of two given files. Ignores the difference between
  // their path strings.
  // Returns true if both files have same contents.
  static absl::StatusOr<bool> IsEqualFile(const std::string &filename1,
                                          const std::string &filename2);

  // Compares the two filenames point to the same file. Symbolic/hard links are
  // considered. This is a wrapper of std::filesystem::equivalent.
  // IsEqualFile reads the contents of the files, but IsEquivalent does not.
  // Returns an error, if either of files doesn't exist.
  static absl::StatusOr<bool> IsEquivalent(const std::string &filename1,
                                           const std::string &filename2);

  // Moves/Renames a file atomically.
  // Returns OK if the file is renamed successfully.
  static absl::Status AtomicRename(const std::string &from,
                                   const std::string &to);

  // Creates a hard link. This returns false if the filesystem does not support
  // hard link or the target file already exists.
  // This is a wrapper of std::filesystem::create_hard_link.
  static absl::Status CreateHardLink(const std::string &from,
                                     const std::string &to);

  // Joins the give path components using the OS-specific path delimiter.
  static std::string JoinPath(absl::Span<const absl::string_view> components);

  // Joins the given two path components using the OS-specific path delimiter.
  static std::string JoinPath(const absl::string_view path1,
                              const absl::string_view path2) {
    return JoinPath({path1, path2});
  }

  static std::string Basename(const std::string &filename);
  static std::string Dirname(const std::string &filename);

  // Returns the normalized path by replacing '/' with '\\' on Windows.
  // Does nothing on other platforms.
  static std::string NormalizeDirectorySeparator(const std::string &path);

  // Returns the modification time in `modified_at`.
  // Returns false if something went wrong.
  static absl::StatusOr<FileTimeStamp> GetModificationTime(
      const std::string &filename);

  // Reads the contents of the file `filename` and returns it.
  static absl::StatusOr<std::string> GetContents(
      const std::string &filename,
      std::ios_base::openmode mode = std::ios::binary);

  // Writes the data provided in `content` to the file `filename`, overwriting
  // any existing content.
  static absl::Status SetContents(
      const std::string &filename, absl::string_view content,
      std::ios_base::openmode mode = std::ios::binary);

  // Sets a mock for unittest.
  static void SetMockForUnitTest(FileUtilInterface *mock);
};

// RAII wrapper for a file. Unlinks the file when this instance goes out of
// scope.
class FileUnlinker final {
 public:
  explicit FileUnlinker(const std::string &filename) : filename_(filename) {}

  FileUnlinker(const FileUnlinker &) = delete;
  FileUnlinker &operator=(const FileUnlinker &) = delete;

  ~FileUnlinker() { FileUtil::UnlinkOrLogError(filename_); }

 private:
  const std::string filename_;
};

}  // namespace mozc

#endif  // MOZC_BASE_FILE_UTIL_H_
