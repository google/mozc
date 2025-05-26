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

#ifndef MOZC_TESTING_MOZCTEST_H_
#define MOZC_TESTING_MOZCTEST_H_

#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/file/temp_dir.h"
#include "testing/gunit.h"

namespace mozc {
namespace testing {

// Gets an absolute path of test resource from path components relative to
// mozc's root directory.
//
// Example:
//
// string path = GetSourcePath({"data", "test", "dictionary", "id.def"});
//
// This call gives the absolute path to data/test/dictionary/id.def. (Note that
// the actual result is separated by OS-specific path separator.)
std::string GetSourcePath(absl::Span<const absl::string_view> components);

// Gets the absolute path of a test resource file. Returns an error status if
// the path doesn't exist.
absl::StatusOr<std::string> GetSourceFile(
  absl::Span<const absl::string_view> components);

// Gets an absolute path of test resource file.  If the file doesn't exist,
// terminates the program.
std::string GetSourceFileOrDie(absl::Span<const absl::string_view> components);

// Gets an absolute path of test resource directory.  If the directory doesn't
// exist, terminates the program.
std::string GetSourceDirOrDie(absl::Span<const absl::string_view> components);

// Gets absolute paths of test resource files in a directory.  If one of files
// don't exit, terminates the program.
//
// vector<string> paths = GetSourceDirOrDie({"my", "dir"}, {"file1", "file2"});
// paths = {
//   "/test/srcdir/my/dir/file1",
//   "/test/srcdir/my/dir/file2",
// };
std::vector<std::string> GetSourceFilesInDirOrDie(
    absl::Span<const absl::string_view> dir_components,
    absl::Span<const absl::string_view> filenames);


// Creates and returns a new unique TempDirectory in TempDirectory::Default().
TempDirectory MakeTempDirectoryOrDie();

// Creates and returns a new unique TempFile in TempDirectory::Default().
TempFile MakeTempFileOrDie();

// A test base fixture class for tests that use the user profile directory.
// During the construction, it sets the user profile directory to a unique
// temporary directory. The temporary directory will be deleted as the end of
// the test if the test is successful.
// Derive your test fixtures from TestWithTempUserProfile instead of
// ::testing::Test.
class TestWithTempUserProfile : public ::testing::Test {
 protected:
  TestWithTempUserProfile();
  ~TestWithTempUserProfile() override;

 private:
  TempDirectory temp_dir_;
};

}  // namespace testing
}  // namespace mozc

#endif  // MOZC_TESTING_MOZCTEST_H_
