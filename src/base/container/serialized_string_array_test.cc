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
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "base/bits.h"
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
  {
    std::unique_ptr<uint32_t[]> buf;
    const absl::string_view actual = SerializedStringArray::SerializeToBuffer(
        {"Hello", "Mozc", "google"}, &buf);
    EXPECT_EQ(actual, kTestData);
  }
  {
    std::unique_ptr<uint32_t[]> buf;
    const std::vector<std::string> strs = {"Hello", "Mozc", "google"};
    const absl::string_view actual =
        SerializedStringArray::SerializeToBuffer(strs, &buf);
    EXPECT_EQ(actual, kTestData);
  }
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

TEST(SerializedStringArrayTest, VerifyDataRejectsMalformedImages) {
  // Missing array size (< 4 bytes).
  EXPECT_FALSE(SerializedStringArray::VerifyData(""));
  EXPECT_FALSE(SerializedStringArray::VerifyData("abc"));

  // 32-bit header size wraparound: 4 + 8 * 0x20000000 wraps to 4 in uint32_t.
  {
    std::string overflow_header(16, '\0');
    StoreUnaligned<uint32_t>(0x20000000u, overflow_header.begin());
    EXPECT_FALSE(SerializedStringArray::VerifyData(overflow_header));
  }

  // 32-bit offset + length wraparound: 0xFFFFFFFF + 1 wraps to 0 in uint32_t.
  {
    std::string offset_len_overflow(16, '\0');
    auto iter =
        StoreUnaligned<uint32_t>(uint32_t{1}, offset_len_overflow.begin());
    iter = StoreUnaligned<uint32_t>(0xFFFFFFFFu, iter);
    StoreUnaligned<uint32_t>(uint32_t{1}, iter);
    EXPECT_FALSE(SerializedStringArray::VerifyData(offset_len_overflow));
  }

  // String reaching exact end of buffer without room for terminating '\0'.
  {
    std::string missing_terminator_space(14, '\0');
    auto iter =
        StoreUnaligned<uint32_t>(uint32_t{1}, missing_terminator_space.begin());
    iter = StoreUnaligned<uint32_t>(uint32_t{12}, iter);
    iter = StoreUnaligned<uint32_t>(uint32_t{2}, iter);
    missing_terminator_space[12] = 'a';
    missing_terminator_space[13] = 'b';
    EXPECT_FALSE(SerializedStringArray::VerifyData(missing_terminator_space));
  }

  // Empty string at exact end of buffer (offset = 12, len = 0 in 12-byte buf).
  {
    std::string zero_len_at_end(12, '\0');
    auto iter = StoreUnaligned<uint32_t>(uint32_t{1}, zero_len_at_end.begin());
    iter = StoreUnaligned<uint32_t>(uint32_t{12}, iter);
    StoreUnaligned<uint32_t>(uint32_t{0}, iter);
    EXPECT_FALSE(SerializedStringArray::VerifyData(zero_len_at_end));
  }

  // String not null-terminated at offset + len.
  {
    std::string non_null_terminated(kTestData);
    non_null_terminated.back() = 'x';
    EXPECT_FALSE(SerializedStringArray::VerifyData(non_null_terminated));
  }

  // Overlapping / non-monotonic string offset.
  {
    std::string overlapping_offset(kTestData);
    // Overwrite string[1] offset (bytes 12..15) with string[0]'s offset (28).
    StoreUnaligned<uint32_t>(uint32_t{28}, overlapping_offset.begin() + 12);
    EXPECT_FALSE(SerializedStringArray::VerifyData(overlapping_offset));
  }
}

TEST(SerializedStringArrayTest, OutOfBoundsAccessReturnsEmptyStringView) {
  SerializedStringArray default_array;
  EXPECT_EQ(default_array[-1], "");
  EXPECT_EQ(default_array[0], "");
  EXPECT_EQ(default_array[100], "");
  EXPECT_EQ(*default_array.begin(), "");
  EXPECT_EQ(default_array.begin()[5], "");

  SerializedStringArray truncated_init;
  EXPECT_FALSE(truncated_init.Init("abc"));
  EXPECT_EQ(truncated_init.size(), 0);
  EXPECT_EQ(truncated_init.begin(), truncated_init.end());
  EXPECT_EQ(*truncated_init.begin(), "");
  EXPECT_EQ(truncated_init[0], "");

#ifdef NDEBUG
  SerializedStringArray truncated_set;
  truncated_set.Set("abc");
  EXPECT_EQ(truncated_set.size(), 0);
  EXPECT_EQ(truncated_set.begin(), truncated_set.end());
  EXPECT_EQ(*truncated_set.begin(), "");
  EXPECT_EQ(truncated_set[0], "");
#endif  // NDEBUG

  alignas(uint32_t) constexpr char kEmptyDataArray[] = "\x00\x00\x00\x00";
  SerializedStringArray empty_array;
  ASSERT_TRUE(empty_array.Init(absl::string_view(kEmptyDataArray, 4)));
  EXPECT_EQ(empty_array[-1], "");
  EXPECT_EQ(empty_array[0], "");
  EXPECT_EQ(*empty_array.begin(), "");

  SerializedStringArray array;
  ASSERT_TRUE(array.Init(kTestData));
  ASSERT_EQ(array.size(), 3);
  EXPECT_EQ(array[-1], "");
  EXPECT_EQ(array[3], "");
  EXPECT_EQ(array[1000], "");
  EXPECT_EQ(array[uint32_t{0x80000000u}], "");
  EXPECT_EQ(array[std::numeric_limits<uint32_t>::max()], "");
  if constexpr (sizeof(size_t) > sizeof(uint32_t)) {
    EXPECT_EQ(array[static_cast<size_t>(uint64_t{1} << 32)], "");
  }
  EXPECT_EQ(array[std::numeric_limits<size_t>::max()], "");
  EXPECT_EQ(*array.end(), "");
  EXPECT_EQ(array.begin()[-1], "");
  EXPECT_EQ(array.begin()[3], "");
}
}  // namespace
}  // namespace mozc
