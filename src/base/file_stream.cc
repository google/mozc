// Copyright 2010-2012, Google Inc.
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

#include "base/util.h"

namespace mozc {

InputFileStream::InputFileStream() {}

InputFileStream::InputFileStream(const char* filename,
                                 ios_base::openmode mode) {
  InputFileStream::open(filename, mode);
}

void InputFileStream::open(const char* filename, ios_base::openmode mode) {
#if defined(OS_WINDOWS)
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
#if defined(OS_WINDOWS)
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

}  // namespace mozc
