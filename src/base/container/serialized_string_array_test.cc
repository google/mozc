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

#include "base/container/serialized_string_array.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <memory>
#include <string>

#include "absl/strings/string_view.h"
#include "testing/gunit.h"

namespace mozc {
namespace {

TEST(SerializedStringArrayTest, DefaultConstructor) {
  SerializedStringArray a;
  EXPECT_TRUE(a.empty());
  EXPECT_EQ(a.size(), 0);
}

TEST(SerializedStringArrayTest, EmptyArray) {
  alignas(uint32_t) constexpr char kDataArray[] = "\x00\x00\x00\x00";
  const absl::string_view kData(kDataArray, 4);
  ASSERT_TRUE(SerializedStringArray::VerifyData(kData));

  SerializedStringArray a;
  ASSERT_TRUE(a.Init(kData));
  EXPECT_TRUE(a.empty());
  EXPECT_EQ(a.size(), 0);
}

alignas(uint32_t) constexpr char kTestDataArray[] =
    "\x03\x00\x00\x00"                  // Array size = 3
    "\x1c\x00\x00\x00\x05\x00\x00\x00"  // (28, 5)
    "\x22\x00\x00\x00\x04\x00\x00\x00"  // (34, 4)
    "\x27\x00\x00\x00\x06\x00\x00\x00"  // (39, 6)
    "Hello\0"                           // offset = 28, len = 5
    "Mozc\0"                            // offset = 34, len = 4
    "google\0";                         // offset = 39, len = 6

constexpr absl::string_view kTestData(kTestDataArray,
                                      std::size(kTestDataArray) - 1);

TEST(SerializedStringArrayTest, SerializeToBuffer) {
  std::unique_ptr<uint32_t[]> buf;
  const absl::string_view actual = SerializedStringArray::SerializeToBuffer(
      {"Hello", "Mozc", "google"}, &buf);
  EXPECT_EQ(actual, kTestData);
}

TEST(SerializedStringArrayTest, Basic) {
  ASSERT_TRUE(SerializedStringArray::VerifyData(kTestData));

  SerializedStringArray a;
  ASSERT_TRUE(a.Init(kTestData));
  ASSERT_EQ(3, a.size());
  EXPECT_EQ(a[0], "Hello");
  EXPECT_EQ(a[1], "Mozc");
  EXPECT_EQ(a[2], "google");

  SerializedStringArray b;
  b.Set(a.data());
  ASSERT_EQ(3, b.size());
  EXPECT_EQ(b[0], "Hello");
  EXPECT_EQ(b[1], "Mozc");
  EXPECT_EQ(b[2], "google");

  SerializedStringArray empty;
  b.swap(empty);
  EXPECT_TRUE(b.empty());
  EXPECT_EQ(empty[0], "Hello");

  a.clear();
  EXPECT_TRUE(a.empty());
  EXPECT_EQ(a.size(), 0);
}

TEST(SerializedStringArrayTest, Iterator) {
  ASSERT_TRUE(SerializedStringArray::VerifyData(kTestData));

  SerializedStringArray a;
  ASSERT_TRUE(a.Init(kTestData));
  {
    auto iter = a.begin();
    ASSERT_NE(a.end(), iter);
    EXPECT_EQ(*iter, "Hello");
    ++iter;
    ASSERT_NE(a.end(), iter);
    EXPECT_EQ(*iter, "Mozc");
    ++iter;
    ASSERT_NE(a.end(), iter);
    EXPECT_EQ(*iter, "google");
    ++iter;
    EXPECT_EQ(iter, a.end());
  }
  EXPECT_TRUE(std::binary_search(a.begin(), a.end(), "Hello"));
  EXPECT_TRUE(std::binary_search(a.begin(), a.end(), "Mozc"));
  EXPECT_TRUE(std::binary_search(a.begin(), a.end(), "google"));
  EXPECT_FALSE(std::binary_search(a.begin(), a.end(), "Japan"));
}

}  // namespace
}  // namespace mozc
