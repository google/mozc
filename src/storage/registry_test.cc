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

#include "storage/registry.h"

#include <cstdint>
#include <string>

#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace storage {
namespace {

class RegistryTest : public testing::TestWithTempUserProfile {};

TEST_F(RegistryTest, TinyStorageTest) {
  {
    uint64_t value = 20;
    EXPECT_TRUE(Registry::Insert("uint64_t", value));
    uint64_t expected = 0;
    EXPECT_TRUE(Registry::Lookup("uint64_t", &expected));
    EXPECT_EQ(value, expected);
  }

  {
    uint32_t value = 20;
    EXPECT_TRUE(Registry::Insert("uint32_t", value));
    uint32_t expected = 0;
    EXPECT_TRUE(Registry::Lookup("uint32_t", &expected));
    EXPECT_EQ(value, expected);
  }

  {
    uint16_t value = 20;
    EXPECT_TRUE(Registry::Insert("uint16_t", value));
    uint16_t expected = 0;
    EXPECT_TRUE(Registry::Lookup("uint16_t", &expected));
    EXPECT_EQ(value, expected);
  }

  {
    uint8_t value = 20;
    EXPECT_TRUE(Registry::Insert("uint8_t", value));
    uint8_t expected = 0;
    EXPECT_TRUE(Registry::Lookup("uint8_t", &expected));
    EXPECT_EQ(value, expected);
  }

  {
    bool value = true;
    EXPECT_TRUE(Registry::Insert("bool", value));
    bool expected = false;
    EXPECT_TRUE(Registry::Lookup("bool", &expected));
    EXPECT_EQ(value, expected);
  }

  {
    std::string value = "test";
    EXPECT_TRUE(Registry::Insert("string", value));
    std::string expected;
    EXPECT_TRUE(Registry::Lookup("string", &expected));
    EXPECT_EQ(value, expected);
  }
}

}  // namespace
}  // namespace storage
}  // namespace mozc
