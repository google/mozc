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

#include "base/multifile.h"

#include "base/logging.h"
#include "base/util.h"

namespace mozc {

InputMultiFile::InputMultiFile(const string &filenames, ios_base::openmode mode)
    : mode_(mode), ifs_(NULL) {
  Util::SplitStringUsing(filenames, ",", &filenames_);
  next_iter_ = filenames_.begin();
  if (next_iter_ != filenames_.end()) {
    OpenNext();
  } else {
    LOG(ERROR) << "empty filenames";
  }
}

InputMultiFile::~InputMultiFile() {
  ifs_.reset(NULL);
}

bool InputMultiFile::ReadLine(string* line) {
  if (ifs_.get() == NULL) {
    return false;
  }
  do {
    if (!getline(*ifs_, *line).fail()) {
      return true;
    }
  } while (OpenNext());
  return false;
}

bool InputMultiFile::OpenNext() {
  while (next_iter_ != filenames_.end()) {
    const char *filename = next_iter_->c_str();
    ifs_.reset(new InputFileStream(filename, mode_));
    ++next_iter_;
    if (!ifs_->fail()) {
      return true;
    }
    LOG(ERROR) << "Cannot open " << filename << endl;
  }
  ifs_.reset();
  return false;
}

}  // namespace mozc
