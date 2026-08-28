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

#include "base/port/varint.h"

#include <cstdint>
#include <string>
#include <vector>

#include "base/port/varint_internal.h"
#include "testing/gunit.h"

namespace mozc::port {
namespace {

TEST(VarintTest, BasicEncodeAndParse32Test) {
  const std::vector<uint32_t> test_cases = {
      0,     1,     127,   128,     255,     256,        16383,
      16384, 65535, 65536, 2097151, 2097152, 0x7FFFFFFF, 0xFFFFFFFF,
  };

  for (const uint32_t original : test_cases) {
    char buf[Varint::kMax32];
    char* end = Varint::Encode32(buf, original);
    const int written = end - buf;
    EXPECT_GT(written, 0);
    EXPECT_LE(written, Varint::kMax32);

    uint32_t parsed = 0;
    const char* read_end = Varint::Parse32(buf, &parsed);
    EXPECT_NE(read_end, nullptr);
    EXPECT_EQ(read_end - buf, written);
    EXPECT_EQ(parsed, original);
  }
}

TEST(VarintTest, PortVarintNamespaceTest) {
  char buf[port::Varint::kMax32];
  char* end = port::Varint::Encode32(buf, 12345);
  uint32_t parsed = 0;
  const char* read_end = port::Varint::Parse32(buf, &parsed);
  EXPECT_NE(read_end, nullptr);
  EXPECT_EQ(read_end, end);
  EXPECT_EQ(parsed, 12345);
}

TEST(VarintTest, MultipleValuesTest) {
  char buf[Varint::kMax32 * 3];
  char* p = buf;
  p = Varint::Encode32(p, 10);
  p = Varint::Encode32(p, 1000);
  p = Varint::Encode32(p, 100000);

  const char* read_p = buf;
  uint32_t v1 = 0;
  read_p = Varint::Parse32(read_p, &v1);
  EXPECT_NE(read_p, nullptr);
  EXPECT_EQ(v1, 10);

  uint32_t v2 = 0;
  read_p = Varint::Parse32(read_p, &v2);
  EXPECT_NE(read_p, nullptr);
  EXPECT_EQ(v2, 1000);

  uint32_t v3 = 0;
  read_p = Varint::Parse32(read_p, &v3);
  EXPECT_NE(read_p, nullptr);
  EXPECT_EQ(v3, 100000);

  EXPECT_EQ(read_p, p);
}

TEST(VarintTest, InvalidVarintTest) {
  uint32_t val = 0;

  // 5th byte has overflow bits set (>= 0x10).
  const char overflow_buf[] = {static_cast<char>(0x80), static_cast<char>(0x80),
                               static_cast<char>(0x80), static_cast<char>(0x80),
                               static_cast<char>(0x10)};
  EXPECT_EQ(port::internal::Varint::Parse32(overflow_buf, &val), nullptr);

  // 5th byte has continuation bit set (>= 0x80).
  const char too_long_buf[] = {
      static_cast<char>(0x80), static_cast<char>(0x80),
      static_cast<char>(0x80), static_cast<char>(0x80),
      static_cast<char>(0x80), static_cast<char>(0x01)};
  EXPECT_EQ(port::internal::Varint::Parse32(too_long_buf, &val), nullptr);

  // Valid max 32-bit uint (0xFFFFFFFF). 5th byte is 0x0F (< 0x10).
  const char max_uint32_buf[] = {
      static_cast<char>(0xFF), static_cast<char>(0xFF), static_cast<char>(0xFF),
      static_cast<char>(0xFF), static_cast<char>(0x0F)};
  const char* read_end = port::internal::Varint::Parse32(max_uint32_buf, &val);
  EXPECT_NE(read_end, nullptr);
  EXPECT_EQ(read_end - max_uint32_buf, 5);
  EXPECT_EQ(val, 0xFFFFFFFF);
}

// Compatibility test to ensure gloop Varint and port internal Varint produce
// identical encoded bytes and decode identical values.
TEST(VarintTest, GloopCompatibilityTest) {
  std::vector<uint32_t> test_cases = {
      0,          1,          2,          126,        127,     128,
      129,        254,        255,        256,        16383,   16384,
      16385,      65535,      65536,      2097151,    2097152, 0x0FFFFFFF,
      0x10000000, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF,
  };

  // Add bit-shift patterns.
  for (int i = 0; i < 32; ++i) {
    test_cases.push_back(1u << i);
    test_cases.push_back((1u << i) - 1);
    test_cases.push_back(~(1u << i));
  }

  // Add stepping values.
  for (uint32_t v = 0; v < 100000; v += 137) {
    test_cases.push_back(v);
  }

  for (const uint32_t val : test_cases) {
    char gloop_buf[port::Varint::kMax32] = {};
    char port_buf[port::internal::Varint::kMax32] = {};

    char* gloop_end = port::Varint::Encode32(gloop_buf, val);
    char* port_end = port::internal::Varint::Encode32(port_buf, val);

    const int gloop_len = gloop_end - gloop_buf;
    const int port_len = port_end - port_buf;

    ASSERT_EQ(gloop_len, port_len) << "Length mismatch for value: " << val;
    EXPECT_EQ(std::string(gloop_buf, gloop_len),
              std::string(port_buf, port_len))
        << "Encoded byte mismatch for value: " << val;

    // Cross-decode: Decode gloop encoded data using port Varint.
    uint32_t port_parsed = 0;
    const char* port_read_end =
        port::internal::Varint::Parse32(gloop_buf, &port_parsed);
    EXPECT_NE(port_read_end, nullptr);
    EXPECT_EQ(port_read_end - gloop_buf, gloop_len);
    EXPECT_EQ(port_parsed, val);

    // Cross-decode: Decode port encoded data using gloop Varint.
    uint32_t gloop_parsed = 0;
    const char* gloop_read_end = port::Varint::Parse32(port_buf, &gloop_parsed);
    EXPECT_NE(gloop_read_end, nullptr);
    EXPECT_EQ(gloop_read_end - port_buf, port_len);
    EXPECT_EQ(gloop_parsed, val);
  }
}

TEST(VarintTest, GloopInvalidCompatibilityTest) {
  uint32_t gloop_val = 0;
  uint32_t port_val = 0;

  // 5th byte overflow (>= 0x10).
  const char overflow_buf[] = {static_cast<char>(0x80), static_cast<char>(0x80),
                               static_cast<char>(0x80), static_cast<char>(0x80),
                               static_cast<char>(0x10)};
  EXPECT_EQ(port::Varint::Parse32(overflow_buf, &gloop_val), nullptr);
  EXPECT_EQ(port::internal::Varint::Parse32(overflow_buf, &port_val), nullptr);

  // 5th byte continuation bit set (>= 0x80).
  const char too_long_buf[] = {
      static_cast<char>(0x80), static_cast<char>(0x80),
      static_cast<char>(0x80), static_cast<char>(0x80),
      static_cast<char>(0x80), static_cast<char>(0x01)};
  EXPECT_EQ(port::Varint::Parse32(too_long_buf, &gloop_val), nullptr);
  EXPECT_EQ(port::internal::Varint::Parse32(too_long_buf, &port_val), nullptr);
}

}  // namespace
}  // namespace mozc::port
