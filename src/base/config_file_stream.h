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

#ifndef MOZC_BASE_CONFIG_FILE_STREAM_H_
#define MOZC_BASE_CONFIG_FILE_STREAM_H_

#include <ios>
#include <istream>
#include <memory>
#include <string>

#include "absl/strings/string_view.h"
#include "base/strings/zstring_view.h"

namespace mozc {

// TODO(taku, komatsu) move ConfigFileStream to config/ directory?

// A class for user-configurable file object
//
// read filename and return istream object.
// filename format:
//
// A) system file embedded in .exe file
//   system://ms-ime.tsv
//   system://kotoeri.tsv
//
// B) user file. expanded into $(USER_PROFILE_DIR)/file_name
//   user://foo.tsv
//   user://foo.bar
//
// C) file -- should only be enabled in debug build
//   file:///foo/bar/foo.tsv
//
// D) on-memory -- for debug.  Does not store to the disk at all.
//   memory://foo.tsv
//   memory://foo.bar
//
class ConfigFileStream {
 public:
  // Open |filename| as a text file with read permission.
  static std::unique_ptr<std::istream> OpenReadText(zstring_view filename) {
    return Open(filename, std::ios_base::in);
  }

  // Open |filename| as a binary file with read permission.
  static std::unique_ptr<std::istream> OpenReadBinary(zstring_view filename) {
    return Open(filename, std::ios_base::in | std::ios_base::binary);
  }

  // Mozc 1.3 and prior had had a following method, which opens |filename|
  // as a text file with read permission.
  //   static istream Open(zstring_view filename) {
  //     return Open(filename, ios_base::in);
  //   }
  // As of Mozc 1.3, a number of files had had depended on this method.
  // However, we did not programmatically replaced all of them with
  // |OpenReadText| because these existing code do not have enough unit
  // tests to check the treatment of line-end character, especially on Windows.
  // Perhaps |OpenReadBinary| might be more appropriate in some cases.
  // So we left |LegacyOpen| as an alias of the old version.
  // You should not use this method in new code.
  // TODO(yukawa): Add unit tests and replace |LegacyOpen| with |OpenReadText|
  //     or |OpenReadBinary| where this method is used.
  static std::unique_ptr<std::istream> LegacyOpen(zstring_view filename) {
    return Open(filename, std::ios_base::in);
  }

  // Update the specified config filename (formatted as above) with
  // the |new_contents| atomically.  Returns true if the update
  // succeeds.
  // Note that this method uses binary mode to update |filename|.
  // TODO(yukawa): Consider to rename to |AtomicUpdateBinary|.
  static bool AtomicUpdate(zstring_view filename,
                           zstring_view new_binary_contens);

  // if prefix is system:// or memory:// return "";
  static std::string GetFileName(absl::string_view filename);

  // Clear all memory:// files.  This is a utility method for testing.
  static void ClearOnMemoryFiles();

#ifdef _WIN32
  static void FixupFilePermission(absl::string_view filename);
#endif  // _WIN32

 private:
  // This function is deprecated. Use OpenReadText or OpenReadBinary instead.
  // TODO(yukawa): Move this function to anonymous namespace in
  //     config_file_stream.cc.
  static std::unique_ptr<std::istream> Open(zstring_view filename,
                                            std::ios_base::openmode mode);
};
}  // namespace mozc

#endif  // MOZC_BASE_CONFIG_FILE_STREAM_H_
