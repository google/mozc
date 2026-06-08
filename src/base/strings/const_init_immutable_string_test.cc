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

#include "base/strings/const_init_immutable_string.h"

#include <string>
#include <string_view>

#include "testing/gunit.h"

namespace mozc::strings {
namespace {

constinit ConstInitImmutableString<256> g_value_simple_init(
    []() -> std::string { return "Mozc"; });

TEST(ConstInitImmutableStringTest, SimpleInit) {
  EXPECT_EQ(g_value_simple_init.GetOrInit(), "Mozc");
  EXPECT_EQ(g_value_simple_init.GetOrInit(), "Mozc");
}

TEST(ConstInitImmutableStringTest, NullTermination) {
  const auto value = g_value_simple_init.GetOrInit();
  EXPECT_EQ(value.data()[value.size()], '\0');
}

constexpr std::string_view kLongValue =
    "this string is longer than the inline buffer";

constinit ConstInitImmutableString<8> g_value_overflow([]() -> std::string {
  return std::string("this string is longer than the inline buffer");
});

TEST(ConstInitImmutableStringTest, HeapFallback) {
  const auto value = g_value_overflow.GetOrInit();
  EXPECT_EQ(value, kLongValue);
  EXPECT_EQ(value.data()[value.size()], '\0');
  // Repeated calls return the same pointer (stable storage).
  EXPECT_EQ(g_value_overflow.GetOrInit().data(), value.data());
}

constinit ConstInitImmutableString<256, wchar_t> g_value_wide(
    []() -> std::wstring { return L"WideMozc"; });

TEST(ConstInitImmutableStringTest, WideChar) {
  EXPECT_EQ(g_value_wide.GetOrInit(), std::wstring_view(L"WideMozc"));
  const auto value = g_value_wide.GetOrInit();
  EXPECT_EQ(value.data()[value.size()], L'\0');
}

}  // namespace
}  // namespace mozc::strings
