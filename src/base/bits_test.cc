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

#include "base/bits.h"

#include <cstdint>
#include <iterator>
#include <string>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/strings/string_view.h"
#include "testing/gmock.h"
#include "testing/gunit.h"

namespace mozc {
namespace {

using ::testing::Each;
using ::testing::ElementsAre;

TEST(BitsTest, LoadUnaligned) {
  constexpr char kArray[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  EXPECT_EQ(LoadUnaligned<char>(&kArray[2]), 2);
  EXPECT_EQ(HostToLittle(LoadUnaligned<uint16_t>(&kArray[1])), 0x0201);
  EXPECT_EQ(HostToLittle(LoadUnaligned<uint32_t>(&kArray[5])), 0x08070605);
  EXPECT_EQ(HostToLittle(LoadUnaligned<uint64_t>(&kArray[3])),
            0xa09080706050403);

  {
    absl::string_view sv(kArray, sizeof(kArray));
    auto iter = sv.begin() + 1;
    EXPECT_EQ(HostToLittle(LoadUnalignedAdvance<uint32_t>(iter)), 0x04030201);
    EXPECT_EQ(HostToLittle(LoadUnalignedAdvance<uint32_t>(iter)), 0x08070605);
    EXPECT_EQ(std::distance(sv.begin(), iter), 1 + 2 * sizeof(uint32_t));
  }

  {
    std::vector<uint16_t> vec = {0x1234, 0x5678, 0x9abc, 0xdef0};
    auto iter = vec.begin();
    EXPECT_EQ(HostToNet(LoadUnalignedAdvance<uint32_t>(iter)), 0x34127856);
    EXPECT_EQ(HostToNet(LoadUnalignedAdvance<uint32_t>(iter)), 0xbc9af0de);
    EXPECT_EQ(std::distance(vec.begin(), iter), 4);
  }
}

TEST(BitsTest, StoreAndLoadUnaligned) {
  std::string buf(12, 0xff);

  EXPECT_EQ(StoreUnaligned<uint16_t>(static_cast<uint16_t>(42), buf.data()),
            buf.data() + sizeof(uint16_t));
  EXPECT_EQ(LoadUnaligned<uint16_t>(buf.data()), 42);
  EXPECT_THAT(buf.substr(2), Each(0xff));

  absl::c_fill(buf, 0xff);
  auto iter =
      StoreUnaligned<uint32_t>(HostToLittle(0x31415926), buf.begin() + 5);
  EXPECT_EQ(LoadUnaligned<uint32_t>(buf.begin() + 5), 0x31415926);
  EXPECT_THAT(buf, ElementsAre(0xff, 0xff, 0xff, 0xff, 0xff, 0x26, 0x59, 0x41,
                               0x31, 0xff, 0xff, 0xff));
  EXPECT_EQ(std::distance(buf.begin(), iter), 5 + sizeof(uint32_t));

  absl::c_fill(buf, 0xff);
  iter =
      StoreUnaligned<uint64_t>(HostToNet(0x0123456789abcdef), buf.begin() + 3);
  EXPECT_THAT(buf, ElementsAre(0xff, 0xff, 0xff, 0x01, 0x23, 0x45, 0x67, 0x89,
                               0xab, 0xcd, 0xef, 0xff));
  EXPECT_EQ(std::distance(buf.begin(), iter), 3 + sizeof(uint64_t));

  uint32_t array32[] = {0, 0};
  EXPECT_EQ(StoreUnaligned<uint32_t>(123, array32), array32 + 1);
  EXPECT_THAT(array32, ElementsAre(123, 0));
}

TEST(BitsTest, ByteSwap) {
  EXPECT_EQ(byteswap<uint8_t>(0x37), 0x37);
  const uint16_t u16 = 0x1234;
  EXPECT_EQ(byteswap(u16), 0x3412);
  EXPECT_EQ(byteswap(byteswap(u16)), u16);
  const int16_t i16 = 0x1234;
  EXPECT_EQ(byteswap(i16), 0x3412);
  const char16_t c16 = 0x1234;
  EXPECT_EQ(byteswap(c16), 0x3412);
  const uint32_t u32 = 0x12345678;
  EXPECT_EQ(byteswap(u32), 0x78563412);
  EXPECT_EQ(byteswap(byteswap(u32)), u32);
  const int32_t i32 = 0x12345678;
  EXPECT_EQ(byteswap(i32), 0x78563412);
  const char32_t c32 = 0x12345678;
  EXPECT_EQ(byteswap(c32), 0x78563412);
  const uint64_t u64 = 0x1234567890abcdef;
  EXPECT_EQ(byteswap(u64), 0xefcdab9078563412);
  EXPECT_EQ(byteswap(byteswap(u64)), u64);
  const int64_t i64 = 0x1234567890abcdef;
  EXPECT_EQ(byteswap(i64), 0xefcdab9078563412);
}

}  // namespace
}  // namespace mozc
