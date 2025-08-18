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

#include "base/unverified_sha1.h"

#include <cstddef>
#include <cstdint>
#include <string>

#include "testing/gunit.h"

namespace mozc {
namespace internal {
namespace {

constexpr size_t kDigestLength = 20;

using ::testing::AssertionFailure;
using ::testing::AssertionResult;
using ::testing::AssertionSuccess;

AssertionResult AssertEqualHashWithFormat(
    const char* expected_expression, const char* actual_expression,
    const uint8_t (&expected)[kDigestLength], const std::string& actual) {
  if (kDigestLength != actual.size()) {
    return AssertionFailure() << "Wrong hash size is " << actual.size();
  }
  for (size_t i = 0; i < kDigestLength; ++i) {
    if (expected[i] != actual[i]) {
      return AssertionFailure()
             << "Hash mismatsh at " << i << " byte."
             << " expected: " << static_cast<uint32_t>(expected[i])
             << ", actual: " << static_cast<uint32_t>(actual[i]);
    }
  }
  return AssertionSuccess();
}

#define EXPECT_EQ_HASH(expected, actual) \
  EXPECT_PRED_FORMAT2(AssertEqualHashWithFormat, expected, actual)

TEST(UnverifiedSHA1Test, OneBlockMessage) {
  // http://csrc.nist.gov/groups/ST/toolkit/documents/Examples/SHA1.pdf
  // Example: one-block message.
  constexpr char kInput[] = "abc";
  constexpr uint8_t kExpected[kDigestLength] = {
      0xa9, 0x99, 0x3e, 0x36, 0x47, 0x06, 0x81, 0x6a, 0xba, 0x3e,
      0x25, 0x71, 0x78, 0x50, 0xc2, 0x6c, 0x9c, 0xd0, 0xd8, 0x9d,
  };
  EXPECT_EQ_HASH(kExpected, UnverifiedSHA1::MakeDigest(kInput));
}

TEST(UnverifiedSHA1Test, TwoBlockMessage) {
  // http://csrc.nist.gov/groups/ST/toolkit/documents/Examples/SHA1.pdf
  // Example: two-block message.
  constexpr char kInput[] =
      "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
  constexpr uint8_t kExpected[kDigestLength] = {
      0x84, 0x98, 0x3e, 0x44, 0x1c, 0x3b, 0xd2, 0x6e, 0xba, 0xae,
      0x4a, 0xa1, 0xf9, 0x51, 0x29, 0xe5, 0xe5, 0x46, 0x70, 0xf1,
  };
  EXPECT_EQ_HASH(kExpected, UnverifiedSHA1::MakeDigest(kInput));
}

TEST(UnverifiedSHA1Test, AnotherTwoBlockMessage) {
  constexpr char kInput[] =
      "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnop";
  constexpr uint8_t kExpected[kDigestLength] = {
      0x47, 0xb1, 0x72, 0x81, 0x07, 0x95, 0x69, 0x9f, 0xe7, 0x39,
      0x19, 0x7d, 0x1a, 0x1f, 0x59, 0x60, 0x70, 0x02, 0x42, 0xf1,
  };
  EXPECT_EQ_HASH(kExpected, UnverifiedSHA1::MakeDigest(kInput));
}

TEST(UnverifiedSHA1Test, ManyBlockMessage) {
  const std::string input(1000000, 'a');
  constexpr uint8_t kExpected[kDigestLength] = {
      0x34, 0xaa, 0x97, 0x3c, 0xd4, 0xc4, 0xda, 0xa4, 0xf6, 0x1e,
      0xeb, 0x2b, 0xdb, 0xad, 0x27, 0x31, 0x65, 0x34, 0x01, 0x6f};
  EXPECT_EQ_HASH(kExpected, UnverifiedSHA1::MakeDigest(input));
}

// TODO(yukawa): Add more tests based on well-known test vectors.

}  // namespace
}  // namespace internal
}  // namespace mozc
