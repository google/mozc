// Copyright 2010-2018, Google Inc.
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

#ifdef OS_WIN
#include <codecvt>
#include <locale>
#endif  // OS_WIN

#include <string>

#ifdef MOZC_USE_PEPPER_FILE_IO
#include "base/logging.h"
#include "base/pepper_file_util.h"
#endif  // MOZC_USE_PEPPER_FILE_IO

namespace mozc {

#ifdef MOZC_USE_PEPPER_FILE_IO

InputFileStream::InputFileStream()
    : std::istream(nullptr) {
  init(&string_buffer_);
}

InputFileStream::InputFileStream(const char* filename,
                                 std::ios_base::openmode mode)
    : std::istream(nullptr) {
  init(&string_buffer_);
  InputFileStream::open(filename, mode);
}

void InputFileStream::open(const char* filename, std::ios_base::openmode mode) {
  string buffer;
  const bool ret = PepperFileUtil::ReadBinaryFile(filename, &buffer);
  if (ret) {
    string_buffer_.sputn(buffer.c_str(), buffer.length());
  } else {
    setstate(std::ios_base::failbit);
  }
}

void InputFileStream::close() {}

OutputFileStream::OutputFileStream()
    : std::ostream(),
      write_done_(false) {
  init(&string_buffer_);
}

OutputFileStream::OutputFileStream(const char* filename,
                                   std::ios_base::openmode mode)
    : std::ostream(),
      write_done_(false) {
  init(&string_buffer_);
  OutputFileStream::open(filename, mode);
}

OutputFileStream::~OutputFileStream() {
  close();
}

void OutputFileStream::open(const char* filename,
                            std::ios_base::openmode mode) {
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

namespace {

#ifdef OS_WIN
std::wstring ToPlatformString(const char* filename) {
  // Since Windows uses UTF-16 for internationalized file names, we should
  // convert the encoding of the given |filename| from UTF-8 to UTF-16.
  // NOTE: To avoid circular dependency, |Util::UTF8ToWide| shouldn't be used
  // here.
  std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> utf8_to_wide;
  return utf8_to_wide.from_bytes(filename);
}
#else  // OS_WIN
string ToPlatformString(const char* filename) {
  return string(filename);
}
#endif  // OS_WIN or not

}  // namespace

InputFileStream::InputFileStream() {}

InputFileStream::InputFileStream(const char* filename,
                                 std::ios_base::openmode mode) {
  InputFileStream::open(filename, mode);
}

void InputFileStream::open(const char* filename, std::ios_base::openmode mode) {
  std::ifstream::open(ToPlatformString(filename), mode);
}

OutputFileStream::OutputFileStream() {}

OutputFileStream::OutputFileStream(const char* filename,
                                   std::ios_base::openmode mode) {
  OutputFileStream::open(filename, mode);
}

void OutputFileStream::open(const char* filename,
                            std::ios_base::openmode mode) {
  std::ofstream::open(ToPlatformString(filename), mode);
}
#endif  // MOZC_USE_PEPPER_FILE_IO

// Common implementations.

void InputFileStream::ReadToString(string *s) {
  seekg(0, end);
  const size_t size = tellg();
  seekg(0, beg);
  s->resize(size);
  read(&(*s)[0], size);
}

string InputFileStream::Read() {
  string s;
  ReadToString(&s);
  return s;
}

void InputFileStream::UnusedKeyMethod() {}   // go/definekeymethod
void OutputFileStream::UnusedKeyMethod() {}  // go/definekeymethod

}  // namespace mozc
