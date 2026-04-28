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

#include "base/container/tuple.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "testing/gunit.h"

namespace mozc {
namespace {

struct Target {
  std::string s;
  int i;
  double d;
  std::unique_ptr<int> ptr;

  Target(std::string s, int i, double d)
      : s(std::move(s)), i(i), d(d), ptr(nullptr) {}

  Target(std::string s, std::unique_ptr<int> p)
      : s(std::move(s)), i(0), d(0.0), ptr(std::move(p)) {}
};

TEST(TuplesTest, MakeFromTuplesMixedArgs) {
  auto t1 = std::make_tuple("hello");
  int i = 42;
  auto obj = make_from_tuples<Target>(t1, i, 3.14);

  EXPECT_EQ(obj.s, "hello");
  EXPECT_EQ(obj.i, 42);
  EXPECT_DOUBLE_EQ(obj.d, 3.14);
}

TEST(TuplesTest, MakeFromTuples_MoveOnly) {
  auto p = std::make_unique<int>(100);
  auto t = std::make_tuple("move", std::move(p));
  auto obj = make_from_tuples<Target>(std::move(t));

  EXPECT_EQ(obj.s, "move");
  ASSERT_NE(obj.ptr, nullptr);
  EXPECT_EQ(*(obj.ptr), 100);
}

TEST(TuplesTest, MakeUniqueFromTuplesBasic) {
  auto t = std::make_tuple("world", 10, 2.5);
  auto ptr = make_unique_from_tuples<Target>(t);
  ASSERT_NE(ptr, nullptr);
  EXPECT_EQ(ptr->s, "world");
  EXPECT_EQ(ptr->i, 10);
}

TEST(TuplesTest, ApplyFromTuplesFunctionCall) {
  auto func = [](int a, int b, int c) { return a + b + c; };
  auto t1 = std::make_tuple(10, 20);
  int val = 5;
  const int result = apply_from_tuples(func, t1, val);
  EXPECT_EQ(result, 35);  // 10 + 20 + 5
}

TEST(TuplesTest, ApplyFromTuplesVoidFunction) {
  bool called = false;
  auto func = [&](const std::string& msg) {
    called = (msg == "run");
  };

  apply_from_tuples(func, std::make_tuple("run"));
  EXPECT_TRUE(called);
}

TEST(TuplesTest, EmptyArgs) {
  // Edge cases. empty args.
  struct Empty {
    void Run() {};
  };
  auto obj = make_from_tuples<Empty>();
  obj.Run();  // to avoid compiler optimization.
}

}  // namespace
}  // namespace mozc
