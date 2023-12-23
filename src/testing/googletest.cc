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


#include "testing/googletest.h"

#include <string>

#include "absl/flags/flag.h"
#include "base/environ.h"
#include "base/file/temp_dir.h"

ABSL_FLAG(std::string, test_srcdir, "",
          "A directory that contains the input data files for a test.");

ABSL_FLAG(std::string, test_tmpdir, "",
          "Directory for all temporary testing files.");


namespace mozc {
namespace {

#include "testing/mozc_data_dir.h"

std::string GetTestSrcdir() {
  const char* srcdir_env = Environ::GetEnv("TEST_SRCDIR");
  if (srcdir_env && srcdir_env[0]) {
    return srcdir_env;
  }

  return kMozcDataDir;
}

}  // namespace

void InitTestFlags() {
  if (absl::GetFlag(FLAGS_test_srcdir).empty()) {
    absl::SetFlag(&FLAGS_test_srcdir, GetTestSrcdir());
  }
  if (absl::GetFlag(FLAGS_test_tmpdir).empty()) {
    TempDirectory tempdir = TempDirectory::Default();
    absl::SetFlag(&FLAGS_test_tmpdir, tempdir.path());
  }
}

}  // namespace mozc

