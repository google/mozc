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

#include "testing/mozctest.h"

#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include "base/file/temp_dir.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/status.h"
#include "base/system_util.h"
#include "testing/googletest.h"
#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace testing {

std::string GetSourcePath(const std::vector<absl::string_view> &components) {
  const std::string test_srcdir = absl::GetFlag(FLAGS_test_srcdir);
  std::vector<absl::string_view> abs_components = {test_srcdir};

  const char *workspace = std::getenv("TEST_WORKSPACE");
  if (workspace && workspace[0]) {
    abs_components.push_back(workspace);
  }

  abs_components.insert(abs_components.end(), components.begin(),
                        components.end());
  return FileUtil::JoinPath(abs_components);
}

absl::StatusOr<std::string> GetSourceFile(
    const std::vector<absl::string_view> &components) {
  std::string path = GetSourcePath(components);
  if (absl::Status s = FileUtil::FileExists(path); !s.ok()) {
    return s;
  }
  return path;
}

std::string GetSourceFileOrDie(
    const std::vector<absl::string_view> &components) {
  absl::StatusOr<std::string> abs_path = GetSourceFile(components);
  CHECK_OK(abs_path);
  return *std::move(abs_path);
}

std::string GetSourceDirOrDie(
    const std::vector<absl::string_view> &components) {
  const std::string path = GetSourcePath(components);
  CHECK_OK(FileUtil::DirectoryExists(path))
      << ": Directory doesn't exist: " << path;
  return path;
}

std::vector<std::string> GetSourceFilesInDirOrDie(
    const std::vector<absl::string_view> &dir_components,
    const std::vector<absl::string_view> &filenames) {
  const std::string dir = GetSourceDirOrDie(dir_components);
  std::vector<std::string> paths;
  for (size_t i = 0; i < filenames.size(); ++i) {
    paths.push_back(FileUtil::JoinPath({dir, filenames[i]}));
    CHECK_OK(FileUtil::FileExists(paths.back()))
        << ": File doesn't exist: " << paths.back();
  }
  return paths;
}

// Creates a new unique TempDirectory and returns it.
TempDirectory MakeTempDirectoryOrDie() {
  absl::StatusOr<TempDirectory> result =
      TempDirectory::Default().CreateTempDirectory();
  CHECK_OK(result);
  return *std::move(result);
}

ScopedTempUserProfileDirectory::ScopedTempUserProfileDirectory()
    : temp_dir_(MakeTempDirectoryOrDie()),
      original_dir_(SystemUtil::GetUserProfileDirectory()) {
  SystemUtil::SetUserProfileDirectory(temp_dir_.path());
}

ScopedTempUserProfileDirectory::~ScopedTempUserProfileDirectory() {
  SystemUtil::SetUserProfileDirectory(original_dir_);
}

}  // namespace testing
}  // namespace mozc
