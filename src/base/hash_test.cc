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

#include "base/hash.h"

#include <cstdint>
#include <string>

#include "testing/gunit.h"

namespace mozc {
namespace {

TEST(HashTest, Basic) {
  std::string s = "";
  EXPECT_EQ(Fingerprint32(s), 0x0d46d8e3);
  EXPECT_EQ(Fingerprint(s), 0x2dcdbae1b24d9501);
  EXPECT_EQ(FingerprintWithSeed(s, 0xdeadbeef), 0x1153f4beb24d9501);

  s = "google";
  EXPECT_EQ(Fingerprint32(s), 0x74290877);
  EXPECT_EQ(Fingerprint(s), 0x56d4ad5eafa6beed);
  EXPECT_EQ(FingerprintWithSeed(s, 0xdeadbeef), 0x1f8cbc0cafa6beed);

  s = "Hello, world!  Hello, Tokyo!  Good afternoon!  Ladies and gentlemen.";
  EXPECT_EQ(Fingerprint32(s), 0xb0f5a2ba);
  EXPECT_EQ(Fingerprint(s), 0x936ccddf9d4f0b39);
  EXPECT_EQ(FingerprintWithSeed(s, 0xdeadbeef), 0xe3fd29979d4f0b39);
}

}  // namespace
}  // namespace mozc
