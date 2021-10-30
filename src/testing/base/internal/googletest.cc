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


#include "testing/base/public/googletest.h"

#ifdef OS_WIN
#include <windows.h>
#else  // OS_WIN
#include <unistd.h>
#endif  // OS_WIN

#include <climits>
#include <string>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/util.h"
#include "absl/flags/declare.h"
#include "absl/flags/flag.h"

ABSL_FLAG(std::string, test_srcdir, "",
          "A directory that contains the input data files for a test.");

ABSL_FLAG(std::string, test_tmpdir, "",
          "Directory for all temporary testing files.");

ABSL_DECLARE_FLAG(std::string, program_invocation_name);

namespace mozc {
namespace {

#include "testing/mozc_data_dir.h"

#ifdef OS_WIN
std::string GetProgramPath() {
  wchar_t w_path[MAX_PATH];
  const DWORD char_size =
      GetModuleFileNameW(nullptr, w_path, std::size(w_path));
  if (char_size == 0) {
    LOG(ERROR) << "GetModuleFileNameW failed.  error = " << ::GetLastError();
    return "";
  } else if (char_size >= std::size(w_path)) {
    LOG(ERROR) << "The result of GetModuleFileNameW was truncated.";
    return "";
  }
  std::string path;
  Util::WideToUtf8(w_path, &path);
  return path;
}

std::string GetTestSrcdir() {
  const std::string srcdir(kMozcDataDir);
  CHECK(FileUtil::DirectoryExists(srcdir).ok())
      << srcdir << " is not a directory.";
  return srcdir;
}

std::string GetTestTmpdir() {
  const std::string tmpdir = GetProgramPath() + ".tmp";

  if (!FileUtil::DirectoryExists(tmpdir).ok()) {
    absl::Status s = FileUtil::CreateDirectory(tmpdir);
    CHECK(s.ok()) << s;
  }
  return tmpdir;
}

#else  // OS_WIN

// Get absolute path to this executable. Corresponds to argv[0] plus
// directory information. E.g like "/spam/eggs/foo_unittest".
std::string GetProgramPath() {
  const std::string& program_invocation_name =
      absl::GetFlag(FLAGS_program_invocation_name);
  if (program_invocation_name.empty() || program_invocation_name[0] == '/') {
    return program_invocation_name;
  }

  // Turn relative filename into absolute
  char cwd_buf[PATH_MAX + 1];
  CHECK_NE(getcwd(cwd_buf, PATH_MAX), nullptr);
  cwd_buf[PATH_MAX] = '\0';  // make sure it's terminated
  return FileUtil::JoinPath(cwd_buf, program_invocation_name);
}

std::string GetTestSrcdir() {
  const char* srcdir_env = getenv("TEST_SRCDIR");
  if (srcdir_env && srcdir_env[0]) {
    return srcdir_env;
  }

  const std::string srcdir(kMozcDataDir);

#if !defined(OS_ANDROID)
  // TestSrcdir is not supported in Android.
  // FIXME(komatsu): We should implement "genrule" and "exports_files"
  // in build.py to install the data files into srcdir.
  CHECK_EQ(access(srcdir.c_str(), R_OK | X_OK), 0)
      << "Access failure: " << srcdir;
#endif  // !defined(OS_ANDROID)
  return srcdir;
}

std::string GetTestTmpdir() {
  std::string tmpdir;
  const char* value = getenv("TEST_TMPDIR");
  if (value && value[0]) {
    tmpdir = value;
  } else {
    tmpdir = GetProgramPath() + ".tmp";
  }

  if (access(tmpdir.c_str(), R_OK | X_OK) != 0) {
    absl::Status s = FileUtil::CreateDirectory(tmpdir);
    CHECK(s.ok()) << s;
  }
  return tmpdir;
}
#endif  // OS_WIN

}  // namespace

void InitTestFlags() {
  if (absl::GetFlag(FLAGS_test_srcdir).empty()) {
    absl::SetFlag(&FLAGS_test_srcdir, GetTestSrcdir());
  }
  if (absl::GetFlag(FLAGS_test_tmpdir).empty()) {
    absl::SetFlag(&FLAGS_test_tmpdir, GetTestTmpdir());
  }
}

}  // namespace mozc

