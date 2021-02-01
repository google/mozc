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

#if defined(OS_WIN)
#include <windows.h>
#else  // OS_WIN
#include <sys/types.h>
#endif

#include <string>
#include <vector>

#include "base/port.h"
#include "absl/strings/string_view.h"

// Ad-hoc workaround against macro problem on Windows.
// On Windows, following macros, defined when you include <Windows.h>,
// should be removed here because they affects the method name definition of
// Util class.
// TODO(yukawa): Use different method name if applicable.
#ifdef CreateDirectory
#undef CreateDirectory
#endif  // CreateDirectory
#ifdef RemoveDirectory
#undef RemoveDirectory
#endif  // RemoveDirectory
#ifdef CopyFile
#undef CopyFile
#endif  // CopyFile

namespace mozc {

#if defined(OS_WIN)
using FileTimeStamp = uint64;
#else
using FileTimeStamp = time_t;
#endif  // OS_WIN

class FileUtilInterface {
 public:
  virtual ~FileUtilInterface() = default;

  virtual bool CreateDirectory(const std::string &path) const = 0;
  virtual bool RemoveDirectory(const std::string &dirname) const = 0;
  virtual bool Unlink(const std::string &filename) const = 0;
  virtual bool FileExists(const std::string &filename) const = 0;
  virtual bool DirectoryExists(const std::string &dirname) const = 0;
  virtual bool CopyFile(const std::string &from,
                        const std::string &to) const = 0;
  virtual bool IsEqualFile(const std::string &filename1,
                          const std::string &filename2) const = 0;
  virtual bool AtomicRename(const std::string &from,
                            const std::string &to) const = 0;
  virtual bool GetModificationTime(const std::string &filename,
                                  FileTimeStamp *modified_at) const = 0;

 protected:
  FileUtilInterface() = default;
};

class FileUtil {
 public:
  // Creates a directory. Does not create directories in the way to the path.
  static bool CreateDirectory(const std::string &path);

  // Removes an empty directory.
  static bool RemoveDirectory(const std::string &dirname);

  // Removes a file.
  static bool Unlink(const std::string &filename);

  // Returns true if a file or a directory with the name exists.
  static bool FileExists(const std::string &filename);

  // Returns true if the directory exists.
  static bool DirectoryExists(const std::string &dirname);

#ifdef OS_WIN
  // Adds file attributes to the file to hide it.
  // FILE_ATTRIBUTE_NORMAL will be removed.
  static bool HideFile(const string &filename);
  static bool HideFileWithExtraAttributes(const string &filename,
                                          DWORD extra_attributes);
#endif  // OS_WIN

  // Copies a file to another file, using mmap internally.
  // The destination file will be overwritten if exists.
  // Returns true if the file is copied successfully.
  static bool CopyFile(const std::string &from, const std::string &to);

  // Compares the contents of two given files. Ignores the difference between
  // their path strings.
  // Returns true if both files have same contents.
  static bool IsEqualFile(const std::string &filename1,
                          const std::string &filename2);

  // Moves/Renames a file atomically.
  // Returns true if the file is renamed successfully.
  static bool AtomicRename(const std::string &from, const std::string &to);

  // Joins the give path components using the OS-specific path delimiter.
  static std::string JoinPath(const std::vector<absl::string_view> &components);
  static void JoinPath(const std::vector<absl::string_view> &components,
                       std::string *output);

  // Joins the given two path components using the OS-specific path delimiter.
  static std::string JoinPath(const std::string &path1,
                              const std::string &path2) {
    return JoinPath({path1, path2});
  }
  static void JoinPath(const std::string &path1, const std::string &path2,
                       std::string *output) {
    JoinPath({path1, path2}, output);
  }

  static std::string Basename(const std::string &filename);
  static std::string Dirname(const std::string &filename);

  // Returns the normalized path by replacing '/' with '\\' on Windows.
  // Does nothing on other platforms.
  static std::string NormalizeDirectorySeparator(const std::string &path);

  // Returns the modification time in `modified_at`.
  // Returns false if something went wrong.
  static bool GetModificationTime(const std::string &filename,
                                  FileTimeStamp *modified_at);

  // Sets a mock for unittest.
  static void SetMockForUnitTest(FileUtilInterface *mock);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(FileUtil);
};

}  // namespace mozc

#endif  // MOZC_BASE_FILE_UTIL_H_
