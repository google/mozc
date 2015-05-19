// Copyright 2010-2014, Google Inc.
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

#include "base/file_stream.h"

#ifdef MOZC_USE_PEPPER_FILE_IO
#include "base/pepper_file_util.h"
#endif  // MOZC_USE_PEPPER_FILE_IO
#include "base/util.h"

namespace mozc {

#ifdef MOZC_USE_PEPPER_FILE_IO

InputFileStream::InputFileStream()
    : istream() {
  init(&string_buffer_);
}

InputFileStream::InputFileStream(const char* filename,
                                 ios_base::openmode mode)
    : istream() {
  init(&string_buffer_);
  InputFileStream::open(filename, mode);
}

void InputFileStream::open(const char* filename, ios_base::openmode mode) {
  string buffer;
  const bool ret = PepperFileUtil::ReadBinaryFile(filename, &buffer);
  if (ret) {
    string_buffer_.sputn(buffer.c_str(), buffer.length());
  } else {
    setstate(ios_base::failbit);
  }
}

void InputFileStream::close() {}

OutputFileStream::OutputFileStream()
    : ostream(),
      write_done_(false) {
  init(&string_buffer_);
}

OutputFileStream::OutputFileStream(const char* filename,
                                   ios_base::openmode mode)
    : ostream(),
      write_done_(false) {
  init(&string_buffer_);
  OutputFileStream::open(filename, mode);
}

OutputFileStream::~OutputFileStream() {
  close();
}

void OutputFileStream::open(const char* filename, ios_base::openmode mode) {
  filename_ = filename;
}

void OutputFileStream::close() {
  if (write_done_) {
    return;
  }
  if (!PepperFileUtil::WriteBinaryFile(filename_, string_buffer_.str())) {
    LOG(ERROR) << "write error filename: \"" << filename_ << "\""
               << "size:" << string_buffer_.str().length();
  } else {
    write_done_ = true;
  }
}

# else  // MOZC_USE_PEPPER_FILE_IO

InputFileStream::InputFileStream() {}

InputFileStream::InputFileStream(const char* filename,
                                 ios_base::openmode mode) {
  InputFileStream::open(filename, mode);
}

void InputFileStream::open(const char* filename, ios_base::openmode mode) {
#if defined(OS_WIN)
  // Since Windows uses UTF-16 for internationalized file names, we should
  // convert the encoding of the given |filename| from UTF-8 to UTF-16.
  wstring filename_wide;
  if (Util::UTF8ToWide(filename, &filename_wide) > 0) {
    ifstream::open(filename_wide.c_str(), mode);
  }
#else
  ifstream::open(filename, mode);
#endif
}

OutputFileStream::OutputFileStream() {}

OutputFileStream::OutputFileStream(const char* filename,
                                   ios_base::openmode mode) {
  OutputFileStream::open(filename, mode);
}

void OutputFileStream::open(const char* filename, ios_base::openmode mode) {
#if defined(OS_WIN)
  // Since Windows uses UTF-16 for internationalized file names, we should
  // convert the encoding of the given |filename| from UTF-8 to UTF-16.
  wstring filename_wide;
  if (Util::UTF8ToWide(filename, &filename_wide) > 0) {
    ofstream::open(filename_wide.c_str(), mode);
  }
#else
  ofstream::open(filename, mode);
#endif
}
#endif  // MOZC_USE_PEPPER_FILE_IO

}  // namespace mozc
