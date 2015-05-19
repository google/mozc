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

#ifndef MOZC_BASE_MMAP_H_
#define MOZC_BASE_MMAP_H_

#ifdef MOZC_USE_PEPPER_FILE_IO
#include <memory>
#endif  // MOZC_USE_PEPPER_FILE_IO
#include <string>

#include "base/port.h"

namespace mozc {
class Mmap {
 public:
  Mmap();
  ~Mmap() {
    Close();
  }
  bool Open(const char *filename, const char *mode = "r");
  void Close();

#ifndef MOZC_USE_PEPPER_FILE_IO
  char &operator[](size_t n) { return *(text_ + n); }
  char operator[](size_t n) const { return *(text_ + n); }
  char *begin() { return text_; }
  const char *begin() const { return text_; }
  char *end() { return text_ + size_; }
  const char *end() const { return text_ + size_; }
#else  // MOZC_USE_PEPPER_FILE_IO
  char &operator[](size_t n) { return *(text_.get() + n); }
  char operator[](size_t n) const { return *(text_.get() + n); }
  char *begin() { return text_.get(); }
  const char *begin() const { return text_.get(); }
  char *end() { return text_.get() + size_; }
  const char *end() const { return text_.get() + size_; }
#endif  // MOZC_USE_PEPPER_FILE_IO

  size_t size() const { return size_; }

#ifdef MOZC_USE_PEPPER_FILE_IO
  // Save the data in memory to the file.
  bool SyncToFile();
#endif  // MOZC_USE_PEPPER_FILE_IO

 private:
#ifndef MOZC_USE_PEPPER_FILE_IO
  char *text_;
#else  // MOZC_USE_PEPPER_FILE_IO
  string filename_;
  std::unique_ptr<char[]> text_;
  bool write_mode_;
#endif  // MOZC_USE_PEPPER_FILE_IO
  size_t size_;

  DISALLOW_COPY_AND_ASSIGN(Mmap);
};
}  // namespace mozc

#endif  // MOZC_BASE_MMAP_H_
