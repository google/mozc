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
#include "base/base.h"
#include "base/util.h"
#include "storage/registry.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace storage {

TEST(RegistryTest, TinyStorageTest) {
  Util::SetUserProfileDirectory(FLAGS_test_tmpdir);

  {
    uint64 value = 20;
    EXPECT_TRUE(Registry::Insert("uint64", value));
    uint64 expected = 0;
    EXPECT_TRUE(Registry::Lookup("uint64", &expected));
    EXPECT_EQ(expected, value);
  }

  {
    uint32 value = 20;
    EXPECT_TRUE(Registry::Insert("uint32", value));
    uint32 expected = 0;
    EXPECT_TRUE(Registry::Lookup("uint32", &expected));
    EXPECT_EQ(expected, value);
  }

  {
    uint16 value = 20;
    EXPECT_TRUE(Registry::Insert("uint16", value));
    uint16 expected = 0;
    EXPECT_TRUE(Registry::Lookup("uint16", &expected));
    EXPECT_EQ(expected, value);
  }

  {
    uint8 value = 20;
    EXPECT_TRUE(Registry::Insert("uint8", value));
    uint8 expected = 0;
    EXPECT_TRUE(Registry::Lookup("uint8", &expected));
    EXPECT_EQ(expected, value);
  }

  {
    bool value = true;
    EXPECT_TRUE(Registry::Insert("bool", value));
    bool expected = 0;
    EXPECT_TRUE(Registry::Lookup("bool", &expected));
    EXPECT_EQ(expected, value);
  }


  {
    string value = "test";
    EXPECT_TRUE(Registry::Insert("string", value));
    string expected;
    EXPECT_TRUE(Registry::Lookup("string", &expected));
    EXPECT_EQ(expected, value);
  }
}
}  // storage
}  // mozc
