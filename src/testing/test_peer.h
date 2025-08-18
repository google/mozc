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

#ifndef MOZC_TESTING_TEST_PEER_H_
#define MOZC_TESTING_TEST_PEER_H_

namespace mozc {
namespace testing {

// Helper class  to define TestPeer class.
//
// Usage:
//  Here FooTestPeer allows to access private `CallPrivateMethod`,
//   `CallStaticPrivateMethod`, and `private_variable_` of class `Foo`.
//
// class FooTestPeer : public testing::TestPeer<Foo> {
//   public:
//     explicit FooTestPeer(Foo &foo) : testing:TestPeer<Foo>(foo) {}
//
//   PEER_METHOD(CallPrivateMethod);
//   PEER_STATIC_METHOD(CallStaticPrivateMethod);
//   PEER_VARIABLE(private_variable_);
// };
//
//  Foo foo;
//  EXPECT_TRUE(FooTestPeer(foo).CallPrivateMethod(arg1, arg2, ...));
//  EXPECT_TRUE(FooTestPeer::CallStaticPrivateMethod(arg1, arg2, ...));
//  EXPECT_EQ(FooTestPeer(foo).private_variable_(), 10);

#define PEER_METHOD(func_name)                            \
  template <typename... Args>                             \
  auto func_name(Args&&... args) {                        \
    return value_.func_name(std::forward<Args>(args)...); \
  }

#define PEER_STATIC_METHOD(func_name)                        \
  template <typename... Args>                                \
  static auto func_name(Args&&... args) {                    \
    return TypeName::func_name(std::forward<Args>(args)...); \
  }

#define PEER_VARIABLE(variable_name)                \
  decltype(value_.variable_name)& variable_name() { \
    return value_.variable_name;                    \
  }

#define PEER_DECLARE(type_name) using type_name = TypeName::type_name;

template <typename T>
class TestPeer {
 public:
  explicit TestPeer(T& value) : value_(value) {}

  using TypeName = T;

 protected:
  TypeName& value_;
};
}  // namespace testing
}  // namespace mozc

#endif  // MOZC_TESTING_TEST_PEER_H_
