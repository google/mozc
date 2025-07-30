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

#ifndef MOZC_BASE_FILE_STREAM_H_
#define MOZC_BASE_FILE_STREAM_H_

#include <fstream>
#include <ios>

#include "base/strings/zstring_view.h"

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
//
// Note: std::ifstream and ofstream only take const char * and const std::string
// &. Changing the parameters to string_view could increase the number of
// copies.
class InputFileStream : public std::ifstream {
 public:
  InputFileStream() = default;
  explicit InputFileStream(zstring_view filename,
                           std::ios_base::openmode mode = std::ios_base::in);

  // Opens the specified file.
  // This function is a wrapper function for the ifstream::open() function
  // to change the encoding of the specified file name from UTF-8 to its native
  // one before calling the ifstream::open() function.  Alternatively, you can
  // pass a pfstring value if you already have the string in the native
  // encoding.
  void open(zstring_view filename,
            std::ios_base::openmode mode = std::ios_base::in);

 private:
  virtual void UnusedKeyMethod();  // go/definekeymethod
};

class OutputFileStream : public std::ofstream {
 public:
  OutputFileStream() = default;
  explicit OutputFileStream(zstring_view filename,
                            std::ios_base::openmode mode = std::ios_base::out);

  // Opens the specified file.
  // This function is a wrapper function for the ofstream::open() function
  // to change the encoding of the specified file name from UTF-8 to its native
  // one before calling the ofstream::open() function. Alternatively, you can
  // pass a pfstring value if you already have the string in the native
  // encoding.
  void open(zstring_view filename,
            std::ios_base::openmode mode = std::ios_base::out);

 private:
  virtual void UnusedKeyMethod();  // go/definekeymethod
};

}  // namespace mozc

#endif  // MOZC_BASE_FILE_STREAM_H_
