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

#include "base/strings/zstring_view.h"

#include <sstream>
#include <string>

#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_set.h"
#include "absl/hash/hash_testing.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "base/strings/pfchar.h"
#include "testing/gmock.h"
#include "testing/gunit.h"

namespace mozc {
namespace {

using ::testing::IsEmpty;

TEST(zpfstring_viewTest, Nullptr) {
  constexpr zpfstring_view zpsv;
  EXPECT_THAT(zpsv, IsEmpty());
  EXPECT_EQ(zpsv.c_str(), nullptr);
  EXPECT_TRUE(zpsv->empty());
}

TEST(ZStringViewTest, Smoke) {
  constexpr zpfstring_view zpsv = PF_STRING("test string");
  EXPECT_EQ(std::char_traits<pfchar_t>::length(zpsv.c_str()), zpsv.size());

  EXPECT_EQ(zpsv, pfstring(PF_STRING("test string")));
  EXPECT_EQ(zpsv, PF_STRING("test string"));
  EXPECT_EQ(PF_STRING("test string"), zpsv);
  EXPECT_NE(zpsv, PF_STRING(""));
  EXPECT_NE(PF_STRING(""), zpsv);
  EXPECT_LT(zpsv, PF_STRING("z"));
  EXPECT_LT(PF_STRING("a"), zpsv);
  const pfstring_view sv = zpsv.view();
  EXPECT_EQ(sv, zpsv);
  const std::string s = to_string(zpsv);
  EXPECT_EQ(s, "test string");

  std::basic_ostringstream<pfchar_t> os;
  os << zpsv;
  EXPECT_EQ(os.str(), sv);

  // Constructor with explicit length.
  constexpr zstring_view zsv("test", 4);
  EXPECT_EQ(zsv.size(), 4);
  EXPECT_EQ(zsv, "test");
  EXPECT_EQ(std::char_traits<char>::length(zsv.c_str()), zsv.size());

  EXPECT_EQ(zsv.max_size(), zsv->max_size());
  EXPECT_EQ(zsv.begin(), zsv->begin());
  EXPECT_EQ(zsv.end(), zsv->end());
  EXPECT_EQ(zsv.cbegin(), zsv->cbegin());
  EXPECT_EQ(zsv.cend(), zsv->cend());
}

TEST(ZStringViewTest, Container) {
  const pfstring s1 = PF_STRING("test1");
  const pfstring s2 = PF_STRING("test2");
  absl::flat_hash_set<zpfstring_view> set;
  set.insert(s1);
  set.insert(s2);
  EXPECT_TRUE(set.contains(s1));

  absl::btree_set<zpfstring_view> btree_set;
  btree_set.insert(s1);
  btree_set.insert(s2);
  EXPECT_TRUE(btree_set.contains(s1));
}

TEST(ZStringViewTest, AbslHash) {
  // Hashing is supported via implicit conversion to string_view.
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly({
      pfstring_view(),
      pfstring_view(PF_STRING("")),
      pfstring_view(PF_STRING("test")),
      pfstring_view(PF_STRING("私の名前は中野です。")),
  }));
}

TEST(ZStringViewTest, AbslStringFuncs) {
  constexpr zstring_view a = "あいう", b = "えお";
  EXPECT_EQ(absl::StrCat(a, b), "あいうえお");

  EXPECT_EQ(absl::StrFormat("%s", a), "あいう");
}

}  // namespace
}  // namespace mozc
