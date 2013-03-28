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

#ifndef MOZC_BASE_FILE_UTIL_H_
#define MOZC_BASE_FILE_UTIL_H_

#include <string>

#include "base/port.h"

// Ad-hoc workadound against macro problem on Windows.
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

class FileUtil {
 public:
  // Some filesystem related methods are disabled on Native Client environment.
#ifndef MOZC_USE_PEPPER_FILE_IO
  // Creates a directory. Does not create directories in the way to the path.
  static bool CreateDirectory(const string &path);

  // Removes an empty directory.
  static bool RemoveDirectory(const string &dirname);
#endif  // MOZC_USE_PEPPER_FILE_IO

  // Removes a file.
  static bool Unlink(const string &filename);

  // Returns true if a file or a directory with the name exists.
  static bool FileExists(const string &filename);

  // Returns true if the directory exists.
  static bool DirectoryExists(const string &filename);

  // Copies a file to another file, using mmap internally.
  // Returns true if the file is copied successfully.
  static bool CopyFile(const string &from, const string &to);

  // Compares the contents of two given files. Ignores the difference between
  // their path strings.
  // Returns true if both files have same contents.
  static bool IsEqualFile(const string &filename1, const string &filename2);

  // Moves/Renames a file atomically.
  // Returns true if the file is renamed successfully.
  static bool AtomicRename(const string &from, const string &to);

  static string JoinPath(const string &path1, const string &path2);
#ifndef SWIG
  static void JoinPath(const string &path1, const string &path2,
                       string *output);
#endif  // SWIG

  static string Basename(const string &filename);
  static string Dirname(const string &filename);

  // Returns the normalized path by replacing '/' with '\\' on Windows.
  // Does nothing on other platforms.
  static string NormalizeDirectorySeparator(const string &path);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(FileUtil);
};

}  // namespace mozc

#endif  // MOZC_BASE_FILE_UTIL_H_
