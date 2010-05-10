// Copyright 2010, Google Inc.
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

#ifndef MOZC_BASE_FILE_STREAM_H_
#define MOZC_BASE_FILE_STREAM_H_

#include <fstream>
#include "base/util.h"

namespace mozc {

// Represents classes which encapsulates the std::ifstream class (or the
// std::ofstream class) to conceal the encodings of its file names.
// Linux and Mac use UTF-8 as the encodings for their internationalized file
// names. On the other hand, Windows uses UTF-16 for its internationalized file
// names. This class conceals such platform-dependent features from other part
// of the mozc server.
// Since these classes assume an input file is encoded in UTF-8, we have to
// change the open() function and convert its encoding for platforms which use
// encodings except UTF-8 for internationalized file names.
class InputFileStream : public ifstream {
 public:
  InputFileStream() {
  }

  explicit InputFileStream(const char* filename,
                           ios_base::openmode mode = ios_base::in) {
    InputFileStream::open(filename, mode);
  }

  // Opens the specified file.
  // This function is a wrapper function for the ifstream::open() function
  // to change the encoding of the specified file name from UTF-8 to its native
  // one before calling the ifstream::open() function.
  void open(const char* filename, ios_base::openmode mode = ios_base::in) {
#if defined(OS_WINDOWS)
    // Since Windows uses UTF-16 for internationalized file names, we should
    // convert the encoding of the given |filename| from UTF-8 to UTF-16.
    wstring filename_wide;
    if (Util::UTF8ToWide(filename, &filename_wide) > 0)
      ifstream::open(filename_wide.c_str(), mode);
#else
    ifstream::open(filename, mode);
#endif
  }
};

class OutputFileStream : public ofstream {
 public:
  OutputFileStream() {
  }

  explicit OutputFileStream(const char* filename,
                            ios_base::openmode mode = ios_base::out) {
    OutputFileStream::open(filename, mode);
  }

  // Opens the specified file.
  // This function is a wrapper function for the ofstream::open() function
  // to change the encoding of the specified file name from UTF-8 to its native
  // one before calling the ofstream::open() function.
  void open(const char* filename, ios_base::openmode mode = ios_base::out) {
#if defined(OS_WINDOWS)
    // Since Windows uses UTF-16 for internationalized file names, we should
    // convert the encoding of the given |filename| from UTF-8 to UTF-16.
    wstring filename_wide;
    if (Util::UTF8ToWide(filename, &filename_wide) > 0)
      ofstream::open(filename_wide.c_str(), mode);
#else
    ofstream::open(filename, mode);
#endif
  }
};
}  // namespace mozc

#endif  // MOZC_BASE_FILE_STREAM_H_
