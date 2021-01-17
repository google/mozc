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

// This is a minimum implementation of multifile, which enables us to
// treat multiple files like a single concatenated file.

#ifndef MOZC_BASE_MULTIFILE_H_
#define MOZC_BASE_MULTIFILE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/file_stream.h"
#include "base/port.h"

namespace mozc {

class InputMultiFile {
 public:
  // filenames must be separated by comma(s), e.g., "foo.txt,hoge.txt".
  explicit InputMultiFile(const std::string& filenames,
                          std::ios_base::openmode mode = std::ios_base::in);
  ~InputMultiFile();

  // Reads one line. Returns false after reading all the lines.
  bool ReadLine(std::string* line);

 private:
  bool OpenNext();

  std::vector<std::string> filenames_;
  const std::ios_base::openmode mode_;
  std::vector<std::string>::iterator next_iter_;
  std::unique_ptr<InputFileStream> ifs_;

  DISALLOW_COPY_AND_ASSIGN(InputMultiFile);
};

}  // namespace mozc

#endif  // MOZC_BASE_MULTIFILE_H_
