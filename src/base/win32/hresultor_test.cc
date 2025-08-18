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
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "base/win32/hresult.h"
#include "testing/gunit.h"

namespace mozc::win32 {
namespace {

template <typename T>
void TestHResultOrTypeAttributes() {
  using U = HResultOr<T>;
  static_assert(std::is_trivially_destructible_v<T> ==
                std::is_trivially_destructible_v<U>);

  static_assert(std::is_copy_constructible_v<T> ==
                std::is_copy_constructible_v<U>);
  static_assert(std::is_trivially_copy_constructible_v<T> ==
                std::is_trivially_copy_constructible_v<U>);
  static_assert(std::is_copy_assignable_v<T> == std::is_copy_assignable_v<U>);
  static_assert((std::is_trivially_copy_assignable_v<T> &&
                 std::is_trivially_destructible_v<T>) ==
                std::is_trivially_copy_assignable_v<U>);

  static_assert(std::is_move_constructible_v<T> ==
                std::is_move_constructible_v<U>);
  static_assert(std::is_trivially_move_constructible_v<T> ==
                std::is_trivially_move_constructible_v<U>);
  static_assert(std::is_nothrow_move_constructible_v<T> ==
                std::is_nothrow_move_constructible_v<U>);
  static_assert(std::is_move_assignable_v<T> == std::is_move_assignable_v<U>);
  static_assert((std::is_trivially_move_assignable_v<T> &&
                 std::is_trivially_destructible_v<T>) ==
                std::is_trivially_move_assignable_v<U>);
  static_assert(std::is_nothrow_move_assignable_v<T> ==
                std::is_nothrow_move_assignable_v<U>);

  static_assert((std::is_nothrow_swappable_v<T> &&
                 std::is_nothrow_move_constructible_v<T>) ==
                std::is_nothrow_swappable_v<U>);
}

TEST(HResultOr, Int) {
  TestHResultOrTypeAttributes<int>();

  HResultOr<int> v(std::in_place, 42);
  EXPECT_TRUE(v.has_value());
  EXPECT_EQ(v.value(), 42);
  EXPECT_EQ(*v, 42);
  EXPECT_EQ(*std::move(v), 42);
  v = HResultOk(12);
  EXPECT_EQ(std::move(v).value(), 12);

  constexpr HResultOr<int> error(HResult(E_FAIL));
  EXPECT_FALSE(error.has_value());
  EXPECT_EQ(error.error(), E_FAIL);

  EXPECT_EQ(v = HResult(E_FAIL), HResult(E_FAIL));
  EXPECT_EQ(v.error(), E_FAIL);
  EXPECT_EQ(v = HResult(E_UNEXPECTED), HResult(E_UNEXPECTED));
  EXPECT_EQ(v.error(), E_UNEXPECTED);
}

TEST(HResultOr, String) {
  TestHResultOrTypeAttributes<std::string>();
  TestHResultOrTypeAttributes<std::string*>();
  TestHResultOrTypeAttributes<const std::string*>();
  TestHResultOrTypeAttributes<absl::string_view>();

  constexpr absl::string_view kTestStr = "hello";
  HResultOr<std::string> v(kTestStr);
  EXPECT_TRUE(v.has_value());
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
  EXPECT_TRUE(in_place.has_value());
}

TEST(HResultOr, Vector) {
  using vector_t = std::vector<int>;
  TestHResultOrTypeAttributes<vector_t>();
  TestHResultOrTypeAttributes<vector_t*>();
  TestHResultOrTypeAttributes<const vector_t*>();

  const HResultOr<vector_t> v(std::in_place, {1, 2, 3});
  EXPECT_TRUE(v.has_value());
  HResultOr<vector_t> v2(v);
  EXPECT_EQ(v, v2);
  const vector_t expected = {1, 2, 3};
  EXPECT_EQ(v, expected);

  HResultOr<vector_t> error = HResultFail();

  using std::swap;
  swap(error, v2);
  EXPECT_TRUE(error.has_value());
  EXPECT_EQ(error, expected);
  EXPECT_FALSE(v2.has_value());
  EXPECT_EQ(v2, HResultFail());
}

TEST(HResultOr, Conversions) {
  // Direct conversions
  HResultOr<std::string> s = "abc";
  EXPECT_EQ(s, "abc");
  s = "def";
  EXPECT_EQ(s, "def");
  s = std::string();
  EXPECT_EQ(s, "");

  // Conversions from HResult.
  s = HResultOk("Mozc");
  EXPECT_EQ(s, "Mozc");
  s = HResult(E_FAIL);
  EXPECT_EQ(s.error(), E_FAIL);

  // Conversion constructors
  const HResultOr<std::string> implicit_conversion_ctor = "abc";
  EXPECT_EQ(implicit_conversion_ctor, "abc");
  const HResultOr<std::string> explicit_conversion_ctor(
      absl::string_view("abc"));
  EXPECT_EQ(explicit_conversion_ctor, "abc");

  // Conversions from HResultOr<U>
  HResultOr<absl::string_view> sv_or = "def";
  HResultOr<std::string> s_or(sv_or);
  EXPECT_EQ(s_or, "def");
  s_or = HResultOk("ghi");
  EXPECT_EQ(s_or, "ghi");
  HResultOr<int> int_or = HResultOk(42.0);
  EXPECT_EQ(int_or, 42);
}

class NonCopyableMock {
 public:
  explicit NonCopyableMock(int v) : v_(v) {}
  NonCopyableMock(const NonCopyableMock&) = delete;
  NonCopyableMock& operator=(const NonCopyableMock&) = delete;
  NonCopyableMock(NonCopyableMock&&) = default;
  NonCopyableMock& operator=(NonCopyableMock&&) = default;

  constexpr int operator*() { return v_; }
  constexpr int lvalue() & { return v_; }
  constexpr int rvalue() && { return v_; }
  constexpr int clvalue() const& { return v_; }
  constexpr int crvalue() const&& { return v_; }

  friend constexpr bool operator==(const NonCopyableMock& a,
                                   const NonCopyableMock& b) {
    return a.v_ == b.v_;
  }

 private:
  int v_;
};

TEST(HResultOr, HResultOk) {
  HResultOr<int> vint = HResultOk(42);
  EXPECT_EQ(vint, 42);
  EXPECT_TRUE(vint.has_value());

  const std::string str("hello");
  HResultOr<std::string> vstr = HResultOk(str);
  EXPECT_EQ(vstr, "hello");
  EXPECT_TRUE(vstr.has_value());

  HResultOr<NonCopyableMock> non_copy = HResultOk(NonCopyableMock(-1));
  EXPECT_TRUE(non_copy.has_value());
  EXPECT_EQ(**non_copy, -1);

  non_copy = HResultOk<NonCopyableMock>(5);
  EXPECT_EQ(**non_copy, 5);

  auto ilist = HResultOk<std::string>({'a', 'b', 'c'});
  EXPECT_EQ(ilist, "abc");
}

TEST(HResultOr, LValueRValue) {
  TestHResultOrTypeAttributes<NonCopyableMock>();
  TestHResultOrTypeAttributes<NonCopyableMock*>();
  TestHResultOrTypeAttributes<const NonCopyableMock*>();

  // Copy-free operators
  HResultOr<NonCopyableMock> implicit_direct_ctor = NonCopyableMock(1);
  EXPECT_TRUE(implicit_direct_ctor.has_value());
  EXPECT_EQ(**implicit_direct_ctor, 1);
  HResultOr<NonCopyableMock> explicit_direct_ctor(
      std::move(implicit_direct_ctor));
  EXPECT_TRUE(explicit_direct_ctor.has_value());
  EXPECT_EQ(**explicit_direct_ctor, 1);

  HResultOr<NonCopyableMock> value_or(NonCopyableMock(100));
  EXPECT_EQ(*std::move(value_or).value_or(NonCopyableMock(42)), 100);
  value_or = HResult(E_FAIL);
  EXPECT_EQ(*std::move(value_or).value_or(NonCopyableMock(42)), 42);

  // assignment
  HResultOr<NonCopyableMock> assignments = HResult(E_FAIL);
  assignments = NonCopyableMock(100);
  EXPECT_TRUE(assignments.has_value());
  EXPECT_EQ(**assignments, 100);
  assignments = HResult(E_FAIL);
  assignments = HResultOk(NonCopyableMock(123));
  EXPECT_TRUE(assignments.has_value());
  EXPECT_EQ(**assignments, 123);

  // Pointer operators.
  HResultOr<NonCopyableMock> v(std::in_place, 42);
  EXPECT_EQ(v->lvalue(), 42);
  EXPECT_EQ((*v).lvalue(), 42);
  const HResultOr<NonCopyableMock> cv(std::move(v));
  EXPECT_EQ(cv->clvalue(), 42);
  EXPECT_EQ((*cv).clvalue(), 42);
  EXPECT_EQ((*(std::move(cv))).crvalue(), 42);
  v = NonCopyableMock(-1);
  EXPECT_EQ((*std::move(v)).rvalue(), -1);
}

TEST(HResultOr, ReturnValueOptimization) {
  auto rvo_rvalue_conv = []() -> HResultOr<NonCopyableMock> {
    return NonCopyableMock(30);
  };
  EXPECT_EQ(rvo_rvalue_conv(), NonCopyableMock(30));

  auto rvo_lvalue_conv = []() -> HResultOr<NonCopyableMock> {
    NonCopyableMock v(100);
    return v;
  };
  EXPECT_EQ(rvo_lvalue_conv(), NonCopyableMock(100));
}

TEST(HResultOr, ValueOr) {
  HResultOr<int> err(HResult(E_FAIL));
  EXPECT_EQ(err.value_or(42), 42);
  EXPECT_EQ(std::move(err).value_or(42), 42);

  // Using unique_ptr to guarantee copy-free.
  HResultOr<NonCopyableMock> v = HResultOk(NonCopyableMock(-1));
  EXPECT_EQ(std::move(v).value_or(NonCopyableMock(42)), NonCopyableMock(-1));
}

TEST(HResultOr, AssignOrReturnHResult) {
  auto f = []() -> HRESULT {
    ASSIGN_OR_RETURN_HRESULT(int i, HResultOr<int>(std::in_place, 42));
    EXPECT_EQ(i, 42);
    ASSIGN_OR_RETURN_HRESULT(i, HResultOr<int>(HResult(E_FAIL)));
    ADD_FAILURE() << "should've returned at the previous line.";
    return S_FALSE;
  };
  EXPECT_EQ(f(), E_FAIL);

  TestHResultOrTypeAttributes<std::unique_ptr<int>>();
  auto g = []() -> HResultOr<std::unique_ptr<int>> {
    ASSIGN_OR_RETURN_HRESULT(auto p, HResultOk(std::make_unique<int>(42)));
    EXPECT_EQ(*p, 42);
    ASSIGN_OR_RETURN_HRESULT(p,
                             HResultOr<std::unique_ptr<int>>(HResult(E_FAIL)));
    return HResult(S_FALSE);
  };
  EXPECT_EQ(g().error(), E_FAIL);
}

TEST(HResultOr, ReturnIfErrorHResult) {
  auto g = []() -> HResultOr<int> {
    RETURN_IF_FAILED_HRESULT(E_FAIL);
    return HResultOk(42);
  };
  EXPECT_EQ(g(), HResult(E_FAIL));
}

struct TriviallyCopyableNotTriviallyMovable {
  TriviallyCopyableNotTriviallyMovable(
      const TriviallyCopyableNotTriviallyMovable&) = default;
  TriviallyCopyableNotTriviallyMovable& operator=(
      const TriviallyCopyableNotTriviallyMovable&) = default;

  TriviallyCopyableNotTriviallyMovable(
      TriviallyCopyableNotTriviallyMovable&&) noexcept {}
  TriviallyCopyableNotTriviallyMovable& operator=(
      TriviallyCopyableNotTriviallyMovable&&) {
    return *this;
  }
};

struct NotTriviallyDestructible {
  NotTriviallyDestructible() = default;
  ~NotTriviallyDestructible() { ++destructor_count; }
  NotTriviallyDestructible(const NotTriviallyDestructible&) = default;
  NotTriviallyDestructible& operator=(const NotTriviallyDestructible&) =
      default;
  NotTriviallyDestructible(NotTriviallyDestructible&&) = default;
  NotTriviallyDestructible& operator=(NotTriviallyDestructible&&) = default;

  static int destructor_count;
};

int NotTriviallyDestructible::destructor_count = 0;

struct ConstructibleNotAssignable {
  ConstructibleNotAssignable(const ConstructibleNotAssignable&) = default;
  ConstructibleNotAssignable& operator=(const ConstructibleNotAssignable&) =
      delete;
  ConstructibleNotAssignable(ConstructibleNotAssignable&&) = default;
  ConstructibleNotAssignable& operator=(ConstructibleNotAssignable&&) = delete;
};

struct NotCopyableOrMovable {
  NotCopyableOrMovable(const NotCopyableOrMovable&) = delete;
  NotCopyableOrMovable& operator=(const NotCopyableOrMovable&) = delete;
};

TEST(HResultOr, TypeAttributeTest) {
  TestHResultOrTypeAttributes<TriviallyCopyableNotTriviallyMovable>();
  TestHResultOrTypeAttributes<NotTriviallyDestructible>();
  TestHResultOrTypeAttributes<ConstructibleNotAssignable>();
  TestHResultOrTypeAttributes<NotCopyableOrMovable>();
}

TEST(HResultOr, DestructorCallTest) {
  {
    NotTriviallyDestructible::destructor_count = 0;
    HResultOr<NotTriviallyDestructible> not_trivially_destructible(
        std::in_place);
    not_trivially_destructible = HResult(E_FAIL);
    EXPECT_EQ(NotTriviallyDestructible::destructor_count, 1);
  }
  EXPECT_EQ(NotTriviallyDestructible::destructor_count, 1);

  {
    HResultOr<NotTriviallyDestructible> not_trivially_destructible(
        std::in_place);
  }
  EXPECT_EQ(NotTriviallyDestructible::destructor_count, 2);
}

}  // namespace
}  // namespace mozc::win32
