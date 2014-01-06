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

#include <string>

#include "base/file_util.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "win32/base/file_verifier.h"

DECLARE_string(test_srcdir);

namespace mozc {
namespace win32 {

namespace {

class TestableFileVerifier : public FileVerifier {
 public:
  // Change access rights.
  using FileVerifier::VerifyIntegrityImpl;
};

static string GetTestFile(const string &filename) {
  string result = FLAGS_test_srcdir;
  result = FileUtil::JoinPath(result, "data/test/win32/integrity");
  result = FileUtil::JoinPath(result, filename);
  return result;
}

}  // namespace

TEST(FileVerifierTest, FileNotFound) {
  const char kTestFile[] = "__file_not_found__";
  EXPECT_EQ(FileVerifier::kIntegrityFileNotFound,
            TestableFileVerifier::VerifyIntegrityImpl(GetTestFile(kTestFile)));
}

TEST(FileVerifierTest, NonSignedFile) {
  const char kTestFile[] = "mozc_test_binary.exe";
  EXPECT_EQ(FileVerifier::kIntegrityVerifiedWithPEChecksum,
            TestableFileVerifier::VerifyIntegrityImpl(GetTestFile(kTestFile)));
}

TEST(FileVerifierTest, SignedFile) {
  const char kTestFile[] = "mozc_test_binary_signed.exe";
  EXPECT_EQ(FileVerifier::kIntegrityVerifiedWithAuthenticodeHash,
            TestableFileVerifier::VerifyIntegrityImpl(GetTestFile(kTestFile)));
}

TEST(FileVerifierTest, ModifiedNonSignedFile) {
  const char kTestFile[] = "mozc_test_binary_modified.exe";
  EXPECT_EQ(FileVerifier::kIntegrityCorrupted,
            TestableFileVerifier::VerifyIntegrityImpl(GetTestFile(kTestFile)));
}

TEST(FileVerifierTest, ModifiedSignedFile) {
  const char kTestFile[] = "mozc_test_binary_modified_signed.exe";
  EXPECT_EQ(FileVerifier::kIntegrityCorrupted,
            TestableFileVerifier::VerifyIntegrityImpl(GetTestFile(kTestFile)));
}

TEST(FileVerifierTest, NoPEChecksumFile) {
  const char kTestFile[] = "mozc_test_binary_no_checksum.exe";
  EXPECT_EQ(FileVerifier::kIntegrityUnknown,
            TestableFileVerifier::VerifyIntegrityImpl(GetTestFile(kTestFile)));
}

}  // namespace win32
}  // namespace mozc
