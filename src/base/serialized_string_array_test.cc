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

#include "base/serialized_string_array.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <memory>
#include <string>

#include "base/port.h"
#include "testing/base/public/gunit.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace {

class SerializedStringArrayTest : public ::testing::Test {
 protected:
  absl::string_view AlignString(const char *s, size_t len) {
    return AlignString(std::string(s, len));
  }

  absl::string_view AlignString(const std::string &s) {
    buf_.reset(new uint32_t[(s.size() + 3) / 4]);
    return absl::string_view(
        static_cast<const char *>(memcpy(buf_.get(), s.data(), s.size())),
        s.size());
  }

 private:
  std::unique_ptr<uint32_t[]> buf_;
};

TEST_F(SerializedStringArrayTest, DefaultConstructor) {
  SerializedStringArray a;
  EXPECT_TRUE(a.empty());
  EXPECT_EQ(0, a.size());
}

TEST_F(SerializedStringArrayTest, EmptyArray) {
  const absl::string_view data = AlignString("\x00\x00\x00\x00", 4);
  ASSERT_TRUE(SerializedStringArray::VerifyData(data));

  SerializedStringArray a;
  ASSERT_TRUE(a.Init(data));
  EXPECT_TRUE(a.empty());
  EXPECT_EQ(0, a.size());
}

constexpr char kTestData[] =
    "\x03\x00\x00\x00"                  // Array size = 3
    "\x1c\x00\x00\x00\x05\x00\x00\x00"  // (28, 5)
    "\x22\x00\x00\x00\x04\x00\x00\x00"  // (34, 4)
    "\x27\x00\x00\x00\x06\x00\x00\x00"  // (39, 6)
    "Hello\0"                           // offset = 28, len = 5
    "Mozc\0"                            // offset = 34, len = 4
    "google\0";                         // offset = 39, len = 6

TEST_F(SerializedStringArrayTest, SerializeToBuffer) {
  std::unique_ptr<uint32_t[]> buf;
  const absl::string_view actual = SerializedStringArray::SerializeToBuffer(
      {"Hello", "Mozc", "google"}, &buf);
  const absl::string_view expected(kTestData, std::size(kTestData) - 1);
  EXPECT_EQ(expected, actual);
}

TEST_F(SerializedStringArrayTest, Basic) {
  const absl::string_view data =
      AlignString(std::string(kTestData, std::size(kTestData) - 1));

  ASSERT_TRUE(SerializedStringArray::VerifyData(data));

  SerializedStringArray a;
  ASSERT_TRUE(a.Init(data));
  ASSERT_EQ(3, a.size());
  EXPECT_EQ("Hello", a[0]);
  EXPECT_EQ("Mozc", a[1]);
  EXPECT_EQ("google", a[2]);

  SerializedStringArray b;
  b.Set(a.data());
  ASSERT_EQ(3, b.size());
  EXPECT_EQ("Hello", b[0]);
  EXPECT_EQ("Mozc", b[1]);
  EXPECT_EQ("google", b[2]);

  a.clear();
  EXPECT_TRUE(a.empty());
  EXPECT_EQ(0, a.size());
}

TEST_F(SerializedStringArrayTest, Iterator) {
  const absl::string_view data =
      AlignString(std::string(kTestData, std::size(kTestData) - 1));

  ASSERT_TRUE(SerializedStringArray::VerifyData(data));

  SerializedStringArray a;
  ASSERT_TRUE(a.Init(data));
  {
    auto iter = a.begin();
    ASSERT_NE(a.end(), iter);
    EXPECT_EQ("Hello", *iter);
    ++iter;
    ASSERT_NE(a.end(), iter);
    EXPECT_EQ("Mozc", *iter);
    ++iter;
    ASSERT_NE(a.end(), iter);
    EXPECT_EQ("google", *iter);
    ++iter;
    EXPECT_EQ(a.end(), iter);
  }
  EXPECT_TRUE(std::binary_search(a.begin(), a.end(), "Hello"));
  EXPECT_TRUE(std::binary_search(a.begin(), a.end(), "Mozc"));
  EXPECT_TRUE(std::binary_search(a.begin(), a.end(), "google"));
  EXPECT_FALSE(std::binary_search(a.begin(), a.end(), "Japan"));
}

}  // namespace
}  // namespace mozc
