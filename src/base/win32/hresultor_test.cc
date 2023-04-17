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
#include "absl/strings/string_view.h"

namespace mozc::win32 {
namespace {

TEST(HResultOr, HResult) {
  constexpr HResult hr(E_FAIL);
  EXPECT_EQ(hr, E_FAIL);
  EXPECT_EQ(hr.hr(), E_FAIL);
  EXPECT_FALSE(hr.ok());

  HResult hr1(S_OK), hr2(E_UNEXPECTED);
  EXPECT_EQ(hr1.hr(), S_OK);
  EXPECT_TRUE(hr1.ok());
  EXPECT_EQ(hr2.hr(), E_UNEXPECTED);
  EXPECT_FALSE(hr2.ok());

  std::swap(hr1, hr2);
  EXPECT_EQ(hr1.hr(), E_UNEXPECTED);
  EXPECT_EQ(hr2.hr(), S_OK);
}

TEST(HResultOr, Int) {
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

  EXPECT_EQ(v = HResult(E_FAIL), HResult(E_FAIL));
  EXPECT_EQ(v.hr(), E_FAIL);
  EXPECT_EQ(v = HResult(E_UNEXPECTED), HResult(E_UNEXPECTED));
  EXPECT_EQ(v.hr(), E_UNEXPECTED);
  EXPECT_EQ(v = HResult(S_OK), HResult(S_OK));
  EXPECT_TRUE(v.ok());
}

TEST(HResultOr, String) {
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

  // Using unique_ptr to quarantee copy-free.
  HResultOr<std::unique_ptr<int>> vptr = HResultOk(std::make_unique<int>(-1));
  EXPECT_TRUE(*vptr);
  EXPECT_EQ(**vptr, -1);
}

TEST(HResultOr, ValueOr) {
  HResultOr<int> err(E_FAIL);
  EXPECT_EQ(err.value_or(42), 42);
  EXPECT_EQ(std::move(err).value_or(42), 42);

  // Using unique_ptr to quarantee copy-free.
  HResultOr<std::unique_ptr<int>> vptr = HResultOk(std::make_unique<int>(-1));
  EXPECT_EQ(*std::move(vptr).value_or(nullptr), -1);
}

TEST(HResultOr, AssignOrReturnHResult) {
  auto f = []() -> HRESULT {
    ASSIGN_OR_RETURN_HRESULT(int i, HResultOr<int>(std::in_place, 42));
    EXPECT_EQ(i, 42);
    ASSIGN_OR_RETURN_HRESULT(i, HResultOr<int>(E_FAIL));
    ADD_FAILURE() << "should've returned at the previous line.";
    return S_FALSE;
  };
  EXPECT_EQ(f(), E_FAIL);

  auto g = []() -> HResultOr<std::unique_ptr<int>> {
    ASSIGN_OR_RETURN_HRESULT(auto p, HResultOk(std::make_unique<int>(42)));
    EXPECT_EQ(*p, 42);
    ASSIGN_OR_RETURN_HRESULT(p, HResultOr<std::unique_ptr<int>>(E_FAIL));
    return HResult(S_FALSE);
  };
  EXPECT_EQ(g().hr(), E_FAIL);
}

TEST(HResultOr, ReturnIfErrorHResult) {
  auto f = []() -> HRESULT {
    RETURN_IF_FAILED_HRESULT(S_OK);
    RETURN_IF_FAILED_HRESULT(E_FAIL);
    return S_FALSE;
  };
  EXPECT_EQ(f(), E_FAIL);

  auto g = []() -> HResultOr<int> {
    RETURN_IF_FAILED_HRESULT(E_FAIL);
    return HResultOk(42);
  };
  EXPECT_EQ(g(), HResult(E_FAIL));
}

}  // namespace
}  // namespace mozc::win32
