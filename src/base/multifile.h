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

// This is a minimum implementation of multifile, which enables us to
// treat multiple files like a single concatenated file.

#ifndef MOZC_BASE_MULTIFILE_H_
#define MOZC_BASE_MULTIFILE_H_

#include <fstream>
#include <string>
#include <vector>

#include "base/base.h"
#include "base/file_stream.h"
#include "base/logging.h"
#include "base/util.h"

namespace mozc {

class InputMultiFile {
 public:
  // filenames must be separated by comma(s), e.g., "foo.txt,hoge.txt".
  InputMultiFile(const string& filenames,
                 ios_base::openmode mode = ios_base::in)
      : mode_(mode), ifs_(NULL) {
    Util::SplitStringUsing(filenames, ",", &filenames_);
    next_iter_ = filenames_.begin();
    if (next_iter_ != filenames_.end()) {
      OpenNext();
    } else {
      LOG(ERROR) << "empty filenames";
    }
  }

  virtual ~InputMultiFile() {
    ifs_.reset(NULL);
  }

  // Reads one line. Returns false after reading all the lines.
  bool ReadLine(string* line) {
    if (ifs_.get() == NULL) {
      return false;
    }
    do {
      if (getline(*ifs_, *line)) {
        return true;
      }
    } while (OpenNext());
    return false;
  }

 private:
  bool OpenNext() {
    while (next_iter_ != filenames_.end()) {
      const char* filename = next_iter_->c_str();
      ifs_.reset(new InputFileStream(filename, mode_));
      ++next_iter_;
      if (!ifs_->fail()) {
        return true;
      }
      LOG(ERROR) << "Cannot open " << filename << endl;
    }
    return false;
  }

  vector<string> filenames_;
  const ios_base::openmode mode_;
  vector<string>::iterator next_iter_;
  scoped_ptr<InputFileStream> ifs_;

  DISALLOW_COPY_AND_ASSIGN(InputMultiFile);
};

}  // namespace mozc

#endif  // MOZC_BASE_MULTIFILE_H_
