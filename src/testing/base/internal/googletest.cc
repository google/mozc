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

#include "testing/base/public/googletest.h"

#ifdef OS_WINDOWS
#include <windows.h>
#else
#include <unistd.h>
#endif  // OS_WINDOWS

#include <climits>
#include <string>

#include "base/init.h"
#include "base/util.h"

DEFINE_string(test_srcdir, "",
              "A directory that contains the input data files for a test.");

DEFINE_string(test_tmpdir, "",
              "Directory for all temporary testing files.");

DECLARE_string(program_invocation_name);

namespace mozc {
namespace {

#ifdef OS_WINDOWS
string GetProgramPath() {
  wchar_t w_path[MAX_PATH];
  const DWORD char_size = GetModuleFileNameW(NULL, w_path, arraysize(w_path));
  if (char_size == 0) {
    LOG(ERROR) << "GetModuleFileNameW failed.  error = " << ::GetLastError();
    return "";
  } else if (char_size >= arraysize(w_path)) {
    LOG(ERROR) << "The result of GetModuleFileNameW was truncated.";
    return "";
  }
  string path;
  Util::WideToUTF8(w_path, &path);
  return path;
}

string GetTestSrcdir() {
  // Hack: Gyp does not support escaping strings (i.e. '\\' to '\\\\') while
  // Gyp normalizes path separators from '/' to '\\' for Windows, so we have
  // a trouble that, if MOZC_DATA_DIR is defined as "a:\r\n", backslashes are
  // not treated as path separators and they are treated as escape sequences
  // (carriage return and new line).
  // Thus we need a little hack.  First, escape all characters in MOZC_DATA_DIR
  // with AS_STRING ("a:\r\n" -> "\"a:\\r\\n\""), and then remove unwanted
  // double-quotes at both ends.
  string srcdir = AS_STRING(MOZC_DATA_DIR);
  CHECK(srcdir.length() > 2 &&
        srcdir[0] == '\"' &&
        srcdir[srcdir.length() - 1] == '\"');
  srcdir = srcdir.substr(1, srcdir.length() - 2);

  CHECK(Util::DirectoryExists(srcdir)) << srcdir << " is not a directory.";
  return srcdir;
}

string GetTestTmpdir() {
  const string tmpdir = GetProgramPath() + ".tmp";

  if (!Util::DirectoryExists(tmpdir)) {
    CHECK(Util::CreateDirectory(tmpdir));
  }
  return tmpdir;
}

#else  // OS_WINDOWS

// Get absolute path to this executable. Corresponds to argv[0] plus
// directory information. E.g like "/spam/eggs/foo_unittest".
string GetProgramPath() {
  const string &program_invocation_name = FLAGS_program_invocation_name;
  if (program_invocation_name.empty() || program_invocation_name[0] == '/') {
    return program_invocation_name;
  }

  // Turn relative filename into absolute
  char cwd_buf[PATH_MAX+1];
  CHECK_GT(getcwd(cwd_buf, PATH_MAX), 0);
  cwd_buf[PATH_MAX] = '\0';  // make sure it's terminated
  return Util::JoinPath(cwd_buf, program_invocation_name);
}

string GetTestSrcdir() {
  const string srcdir = MOZC_DATA_DIR;

  // FIXME(komatsu): We should implement "genrule" and "exports_files"
  // in build.py to install the data files into srcdir.
  CHECK_EQ(access(srcdir.c_str(), R_OK|X_OK), 0)
      << "Access failure: " << srcdir;
  return srcdir;
}

string GetTestTmpdir() {
  const string tmpdir = GetProgramPath() + ".tmp";

  if (access(tmpdir.c_str(), R_OK|X_OK) != 0) {
    CHECK(Util::CreateDirectory(tmpdir));
  }
  return tmpdir;
}
#endif  // OS_WINDOWS

}  // namespace

void InitTestFlags() {
  if (FLAGS_test_srcdir.empty()) {
    FLAGS_test_srcdir = GetTestSrcdir();
  }
  if (FLAGS_test_tmpdir.empty()) {
    FLAGS_test_tmpdir = GetTestTmpdir();
  }
}

}  // namespace mozc
