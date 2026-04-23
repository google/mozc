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

#include "absl/strings/string_view.h"

namespace mozc {

// Represents classes which encapsulates the std::ifstream class (or the
// std::ofstream class) to conceal the encodings of its file names.
// This method accepts the UTF-8 encoded file name as absl::string_view, and
// automatically converts the input filename into a platform dependent
// encoding via pfstring.
class InputFileStream : public std::ifstream {
 public:
  InputFileStream() = default;
  explicit InputFileStream(absl::string_view filename,
                           std::ios_base::openmode mode = std::ios_base::in);

  // Opens the specified file.
  // This function is a wrapper function for the ifstream::open() function
  // to change the encoding of the specified file name from UTF-8 to its native
  // one before calling the ifstream::open() function. This method automatically
  // converts the UTF-8 filename into a platform specific encoding.
  void open(absl::string_view filename,
            std::ios_base::openmode mode = std::ios_base::in);

 private:
  virtual void UnusedKeyMethod();  // go/definekeymethod
};

class OutputFileStream : public std::ofstream {
 public:
  OutputFileStream() = default;
  explicit OutputFileStream(absl::string_view filename,
                            std::ios_base::openmode mode = std::ios_base::out);

  // Opens the specified file.
  // This function is a wrapper function for the ofstream::open() function
  // to change the encoding of the specified file name from UTF-8 to its native
  // one before calling the ofstream::open() function. his method automatically
  // converts the UTF-8 filename into a platform specific encoding.
  void open(absl::string_view filename,
            std::ios_base::openmode mode = std::ios_base::out);

 private:
  virtual void UnusedKeyMethod();  // go/definekeymethod
};

}  // namespace mozc

#endif  // MOZC_BASE_FILE_STREAM_H_
