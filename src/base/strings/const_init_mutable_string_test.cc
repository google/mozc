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

#include "base/strings/const_init_mutable_string.h"

#include <string>

#include "testing/gunit.h"

namespace mozc::strings {
namespace {

constinit ConstInitMutableString<256> g_value_no_init;

TEST(ConstInitMutableStringTest, EmptyByDefault) {
  EXPECT_EQ(g_value_no_init.Get(), "");
}

constinit ConstInitMutableString<256> g_value_set_get;

TEST(ConstInitMutableStringTest, SetThenGet) {
  g_value_set_get.Set("hello");
  EXPECT_EQ(g_value_set_get.Get(), "hello");
  g_value_set_get.Set("world");
  EXPECT_EQ(g_value_set_get.Get(), "world");
}

constinit ConstInitMutableString<8> g_value_overflow;

TEST(ConstInitMutableStringTest, HeapFallbackOnSet) {
  const std::string long_value = "this string is longer than the inline buffer";
  g_value_overflow.Set(long_value);
  EXPECT_EQ(g_value_overflow.Get(), long_value);
  // Replacing a heap-backed value with another heap-backed value must free
  // the prior allocation (validated under ASan); behaviorally we just check
  // that the new value is observable.
  const std::string longer_value = long_value + " and then some";
  g_value_overflow.Set(longer_value);
  EXPECT_EQ(g_value_overflow.Get(), longer_value);
  // Replacing a heap-backed value with one that fits inline.
  g_value_overflow.Set("short");
  EXPECT_EQ(g_value_overflow.Get(), "short");
}

constinit ConstInitMutableString<256, wchar_t> g_value_wide;

TEST(ConstInitMutableStringTest, WideChar) {
  EXPECT_EQ(g_value_wide.Get(), std::wstring());
  g_value_wide.Set(L"WideMozc");
  EXPECT_EQ(g_value_wide.Get(), std::wstring(L"WideMozc"));
  g_value_wide.Set(L"WideOverride");
  EXPECT_EQ(g_value_wide.Get(), std::wstring(L"WideOverride"));
}

}  // namespace
}  // namespace mozc::strings
