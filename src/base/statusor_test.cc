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

#include "base/statusor.h"

#include <memory>

#include "base/status.h"
#include "testing/base/public/gunit.h"
#include "absl/memory/memory.h"

namespace mozc {
namespace {

const char kMessage[] = "test message";

TEST(StatusOrTest, DefaultConstructor) {
  StatusOr<int> s;
  EXPECT_FALSE(s.ok());
  EXPECT_EQ(StatusCode::kUnknown, s.status().code());
}

TEST(StatusOrTest, ConstructorWithStatus) {
  const StatusOr<int> s(Status(StatusCode::kOutOfRange, kMessage));
  EXPECT_FALSE(s.ok());
  EXPECT_EQ(StatusCode::kOutOfRange, s.status().code());
  EXPECT_EQ(kMessage, s.status().message());
}

TEST(StatusOrTest, ConstructorWithValue) {
  {
    // const T& version.
    const std::string kValue = "hello";
    const StatusOr<std::string> s(kValue);
    EXPECT_TRUE(s.ok());
    EXPECT_EQ(kValue, s.value());
    EXPECT_EQ(kValue, *s);
  }
  {
    // T&& version.
    auto value = absl::make_unique<int>(123);
    const auto *ptr = value.get();
    const StatusOr<std::unique_ptr<int>> s(std::move(value));
    EXPECT_TRUE(s.ok());
    EXPECT_EQ(ptr, s->get());
    EXPECT_EQ(123, *s.value());
    EXPECT_EQ(123, **s);
  }
}

TEST(StatusOrTest, MoveConstructor) {
  auto value = absl::make_unique<int>(123);
  const auto *ptr = value.get();
  StatusOr<std::unique_ptr<int>> s(std::move(value));
  StatusOr<std::unique_ptr<int>> t(std::move(s));
  EXPECT_TRUE(t.ok());
  EXPECT_EQ(ptr, t->get());
  EXPECT_EQ(123, *t.value());
  EXPECT_EQ(123, **t);
}

TEST(StatusOrTest, MoveValue) {
  auto value = absl::make_unique<int>(123);
  const auto *ptr = value.get();
  StatusOr<std::unique_ptr<int>> s(std::move(value));
  ASSERT_TRUE(s.ok());
  std::unique_ptr<int> extracted = *std::move(s);
  EXPECT_EQ(ptr, extracted.get());
}

}  // namespace
}  // namespace mozc
