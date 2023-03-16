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

#include "base/strings/platform_string.h"

#include <sstream>
#include <string>
#include <utility>

#include "base/logging.h"
#include "testing/gmock.h"
#include "testing/gunit.h"
#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_set.h"
#include "absl/hash/hash_testing.h"

#ifdef _WIN32
#include <windows.h>
#else  // _WIN32
#include <unistd.h>
#endif  // !_WIN32

namespace mozc {
namespace {

using testing::IsEmpty;
using testing::Not;

// Testing with an actual use case.
PlatformString GetHostName() {
  PlatformString result;
#ifdef _WIN32
  DWORD size = 0;
  EXPECT_FALSE(GetComputerNameExW(ComputerNameDnsHostname, nullptr, &size));
  EXPECT_EQ(GetLastError(), ERROR_MORE_DATA);
  result.resize(size);
  EXPECT_TRUE(
      GetComputerNameExW(ComputerNameDnsHostname, result.data(), &size));
  result.resize(size);
#else   // _WIN32
  char buf[_POSIX_HOST_NAME_MAX];
  EXPECT_EQ(gethostname(buf, sizeof(buf)), 0);
  result.assign(buf);
#endif  // !_WIN32
  return result;
}

TEST(PlatformStringTest, GetHostName) {
  const PlatformString s = GetHostName();
  EXPECT_THAT(ToString(s), Not(IsEmpty()));
}

TEST(PlatformStringTest, ToPlatformString) {
  constexpr PlatformStringView expected = PLATFORM_STRING("test string");
  const std::string s = "test string";
  EXPECT_EQ(ToPlatformString(s), expected);
  EXPECT_EQ(ToPlatformString(std::move(s)), expected);
}

TEST(PlatformStringTest, ToString) {
  constexpr absl::string_view expected = "test string";
  const PlatformString ps = PLATFORM_STRING("test string");
  EXPECT_EQ(ToString(ps), expected);
  EXPECT_EQ(ToString(std::move(ps)), expected);
}

TEST(PlatformStringViewTest, Nullptr) {
  constexpr PlatformStringView psv;
  EXPECT_THAT(psv, IsEmpty());
  EXPECT_EQ(psv.c_str(), nullptr);
  EXPECT_TRUE(psv->empty());
}

TEST(PlatformStringViewTest, Smoke) {
  constexpr PlatformStringView psv = PLATFORM_STRING("test string");

  EXPECT_EQ(psv, PlatformString(PLATFORM_STRING("test string")));
  EXPECT_EQ(psv, PLATFORM_STRING("test string"));
  EXPECT_EQ(PLATFORM_STRING("test string"), psv);
  EXPECT_NE(psv, PLATFORM_STRING(""));
  EXPECT_NE(PLATFORM_STRING(""), psv);
  EXPECT_LT(psv, PLATFORM_STRING("z"));
  EXPECT_LT(PLATFORM_STRING("a"), psv);
  const PlatformStringView::element_type sv = *psv;
  EXPECT_EQ(sv, psv);
  const std::string s = ToString(psv);
  EXPECT_EQ(s, "test string");

  std::basic_ostringstream<PlatformChar> os;
  os << psv;
  EXPECT_EQ(os.str(), sv);
}

TEST(PlatformStringViewTest, Container) {
  const PlatformString s1 = PLATFORM_STRING("test1");
  const PlatformString s2 = PLATFORM_STRING("test2");
  absl::flat_hash_set<PlatformStringView> set;
  set.insert(s1);
  set.insert(s2);
  EXPECT_TRUE(set.contains(s1));

  absl::btree_set<PlatformStringView> btree_set;
  btree_set.insert(s1);
  btree_set.insert(s2);
  EXPECT_TRUE(btree_set.contains(s1));
}

TEST(PlatformStringViewTest, AbslHash) {
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly({
      PlatformStringView(),
      PlatformStringView(PLATFORM_STRING("")),
      PlatformStringView(PLATFORM_STRING("test")),
      PlatformStringView(PLATFORM_STRING("私の名前は中野です。")),
  }));
}

}  // namespace
}  // namespace mozc
