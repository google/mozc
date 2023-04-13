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

#include "base/win32/hresultor.h"

#include <winerror.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "testing/gmock.h"
#include "testing/gunit.h"
#include "testing/base/public/gunit.h"
#include "absl/strings/string_view.h"

namespace mozc::win32 {
namespace {

using testing::IsEmpty;

TEST(HResultOr, Int) {
  constexpr HResultOr<int> v_default;
  EXPECT_TRUE(v_default.ok());

  HResultOr<int> v(std::in_place, 42);
  EXPECT_TRUE(v.ok());
  EXPECT_EQ(v.value(), 42);
  EXPECT_EQ(*v, 42);
  EXPECT_EQ(*std::move(v), 42);
  v = 12;
  EXPECT_EQ(std::move(v).value(), 12);

  constexpr HResultOr<int> error(E_FAIL);
  EXPECT_FALSE(error.ok());
  EXPECT_EQ(error.hr(), E_FAIL);

  constexpr HResultOr<int> c(123);
  EXPECT_EQ(c.hr(), 123);
}

TEST(HResultOr, String) {
  const HResultOr<std::string> v_default;
  EXPECT_THAT(*v_default, IsEmpty());
  EXPECT_TRUE(v_default->empty());
  EXPECT_THAT(v_default.value(), IsEmpty());

  constexpr absl::string_view kTestStr = "hello";
  HResultOr<std::string> v(kTestStr);
  EXPECT_TRUE(v.ok());
  EXPECT_EQ(v.value(), kTestStr);
  EXPECT_EQ(*v, kTestStr);
  EXPECT_EQ(*std::move(v), kTestStr);
  v = "world";
  EXPECT_EQ(std::move(v).value(), "world");

  std::string tmp("abc");
  v = std::move(tmp);
  EXPECT_EQ(*v, "abc");

  tmp = "def";
  HResultOr<std::string> v2(std::move(tmp));
  EXPECT_EQ(*v2, "def");
  *v2 = "ghi";
  EXPECT_EQ(v2, "ghi");

  const HResultOr<std::string> literal("hello, world");
  EXPECT_EQ(literal, "hello, world");

  const HResultOr<std::string> in_place(std::in_place, 7, 'a');
  EXPECT_EQ(in_place, std::string(7, 'a'));
  EXPECT_TRUE(in_place.ok());
}

TEST(HResultOr, Vector) {
  using vector_t = std::vector<int>;

  const HResultOr<vector_t> v(std::in_place, {1, 2, 3});
  EXPECT_TRUE(v.ok());
  HResultOr<vector_t> v2(v);
  EXPECT_EQ(v, v2);
  vector_t expected = {1, 2, 3};
  EXPECT_EQ(v, expected);
  EXPECT_EQ(expected, v);

  HResultOr<vector_t> error(E_FAIL);
  EXPECT_NE(v, error);
  EXPECT_NE(error, expected);
  EXPECT_NE(expected, error);

  std::swap(error, v2);
  EXPECT_TRUE(error.ok());
  EXPECT_EQ(error, expected);
  EXPECT_FALSE(v2.ok());
}

TEST(HResultOr, HResultOk) {
  HResultOr<int> vint = HResultOk(42);
  EXPECT_EQ(vint, 42);
  EXPECT_TRUE(vint.ok());

  const std::string str("hello");
  HResultOr<std::string> vstr = HResultOk(str);
  EXPECT_EQ(vstr, "hello");
  EXPECT_TRUE(vstr.ok());

  HResultOr<std::unique_ptr<int>> vptr = HResultOk(std::make_unique<int>(-1));
  EXPECT_TRUE(*vptr);
  EXPECT_EQ(**vptr, -1);
}

}  // namespace
}  // namespace mozc::win32
