// Copyright 2010-2013, Google Inc.
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

#include "storage/louds/bit_vector_based_array.h"

#include "base/base.h"
#include "storage/louds/bit_vector_based_array_builder.h"
#include "testing/base/public/gunit.h"

namespace {

using ::mozc::storage::louds::BitVectorBasedArray;
using ::mozc::storage::louds::BitVectorBasedArrayBuilder;

class BitVectorBasedArrayTest : public ::testing::Test {
};

TEST_F(BitVectorBasedArrayTest, Get) {
  struct {
    const char* element;
    const size_t length;
    const char* expected_element;
    const size_t expected_length;
  } kTestData[] = {
    { "", 0, "\x00\x00\x00\x00", 4 },
    { "a", 1, "a\x00\x00\x00", 4 },
    { "b", 1, "b\x00\x00\x00", 4 },
    { "c", 1, "c\x00\x00\x00", 4 },
    { "d", 1, "d\x00\x00\x00", 4 },
    { "abcdefg", 7, "abcdefg\x00", 8 },
    { "mozc", 4, "mozc", 4 },
    { "google-japanese-input", 21, "google-japanese-input\x00", 22 },
    { "abcdefg", 7, "abcdefg\x00", 8 },  // Test for storing a same element.
    { "testdata", 8, "testdata", 8 },
    { "another-test-data", 17, "another-test-data\x00", 18 },
  };

  BitVectorBasedArrayBuilder builder;
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestData); ++i) {
    builder.Add(string(kTestData[i].element, kTestData[i].length));
  }
  builder.SetSize(4, 2);
  builder.Build();

  BitVectorBasedArray array;
  array.Open(reinterpret_cast<const uint8*>(builder.image().data()));
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestData); ++i) {
    size_t length;
    const char *result = array.Get(i, &length);
    EXPECT_EQ(
        string(kTestData[i].expected_element, kTestData[i].expected_length),
        string(result, length));
  }

  array.Close();
}
}  // namespace
