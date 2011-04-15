// Copyright 2010-2011, Google Inc.
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

#include <string>
#include <vector>
#include "base/base.h"
#include "base/util.h"
#include "base/mmap.h"

DECLARE_bool(logtostderr);

namespace {

bool OutputRule(const string &filename) {
  mozc::Mmap<char> mmap;
  if (!mmap.Open(filename.c_str(), "r")) {
    return false;
  }
  string buf(mmap.begin(), mmap.end());
  string escaped_buf;
  mozc::Util::Escape(buf, &escaped_buf);
  const string base_filename = mozc::Util::Basename(
      mozc::Util::NormalizeDirectorySeparator(filename));

  cout << " { \"" << base_filename << "\", ";
  cout << " \"" << escaped_buf << "\", ";
  cout << " " << mmap.GetFileSize() << " }";
  return true;
}
}  // namespace

int main(int argc, char **argv) {
  FLAGS_logtostderr = true;
  InitGoogle(argv[0], &argc, &argv, false);

  cout << "static const FileData kFileData[] = {" << endl;
  for (size_t i = 1; i < argc; ++i) {
    if (!OutputRule(argv[i])) {
      return -1;
    }
    if (i != (argc - 1)) {
      cout << ",";
    }
    cout << endl;
  }
  cout << "};" << endl;

  return 0;
}
