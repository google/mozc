// Copyright 2010-2011, Google Inc.
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

#include "base/file_stream.h"

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
// B) user file. expaneded into $(USER_PROFILE_DIR)/file_name
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
  static istream *Open(const string &filename) {
    return Open(filename, ios_base::in);
  }

  static istream *Open(const string &filename,
                       ios_base::openmode mode);

  // Update the specified config filename (formatted as above) with
  // the |new_contents| atomically.  Returns true if the update
  // succeeds.
  static bool AtomicUpdate(const string &filename,
                           const string &new_contents);

  // if prefix is system:// or memory:// return "";
  static string GetFileName(const string &filename);
};
}  // namespace mozc

#endif  // MOZC_BASE_CONFIG_FILE_STREAM_H_
