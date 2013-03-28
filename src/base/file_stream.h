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

#ifndef MOZC_BASE_FILE_STREAM_H_
#define MOZC_BASE_FILE_STREAM_H_

#ifdef MOZC_USE_PEPPER_FILE_IO
#include <sstream>
#else  // MOZC_USE_PEPPER_FILE_IO
#include <fstream>
#endif  // MOZC_USE_PEPPER_FILE_IO

namespace mozc {

#ifdef MOZC_USE_PEPPER_FILE_IO

// Implementation of ifstream for NaCl.
// This class reads all of the file using Pepper FileIO and sotres the data in
// string_buffer_ when open() is called. This class can't be used in the NaCl
// main thread.
class InputFileStream : public istream {
 public:
  InputFileStream();
  explicit InputFileStream(const char* filename,
                           ios_base::openmode mode = ios_base::in);
  // Opens the file and reads the all data to string_buffer_.
  void open(const char* filename, ios_base::openmode mode = ios_base::in);
  // Do nothing.
  // Note: Error handling after close() is not correctly implemented.
  // TODO(horo) Implement error handling correctly.
  void close();

 private:
  stringbuf string_buffer_;
};

// Implementation of ofstream for NaCl.
// This class writes all of the data to the file using Pepper FileIO in the
// destructor or when close() is called. This class can't be used in the NaCl
// main thread.
class OutputFileStream : public ostream {
 public:
  OutputFileStream();
  explicit OutputFileStream(const char* filename,
                            ios_base::openmode mode = ios_base::in);
  ~OutputFileStream();
  // Sets filename_.
  void open(const char* filename, ios_base::openmode mode = ios_base::in);
  // Write the data to the file using Pepper FileIO.
  // Note: Error handling after close() is not correctly implemented.
  // TODO(horo) Implement error handling correctly.
  void close();

 private:
  string filename_;
  stringbuf string_buffer_;
  bool write_done_;
};

# else  // MOZC_USE_PEPPER_FILE_IO

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
  InputFileStream();
  explicit InputFileStream(const char* filename,
                           ios_base::openmode mode = ios_base::in);

  // Opens the specified file.
  // This function is a wrapper function for the ifstream::open() function
  // to change the encoding of the specified file name from UTF-8 to its native
  // one before calling the ifstream::open() function.
  void open(const char* filename, ios_base::openmode mode = ios_base::in);
};

class OutputFileStream : public ofstream {
 public:
  OutputFileStream();
  explicit OutputFileStream(const char* filename,
                            ios_base::openmode mode = ios_base::out);

  // Opens the specified file.
  // This function is a wrapper function for the ofstream::open() function
  // to change the encoding of the specified file name from UTF-8 to its native
  // one before calling the ofstream::open() function.
  void open(const char* filename, ios_base::openmode mode = ios_base::out);
};
#endif  // MOZC_USE_PEPPER_FILE_IO

}  // namespace mozc

#endif  // MOZC_BASE_FILE_STREAM_H_
