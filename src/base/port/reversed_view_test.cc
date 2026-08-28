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

#include "base/port/reversed_view.h"

#include <array>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "base/port/reversed_view_internal.h"
#include "testing/gunit.h"

namespace mozc::port {
namespace {

TEST(ReversedViewTest, VectorTest) {
  std::vector<int> v = {1, 2, 3, 4, 5};
  std::vector<int> reversed;
  for (const int x : reversed_view(v)) {
    reversed.push_back(x);
  }
  EXPECT_EQ(reversed, (std::vector<int>{5, 4, 3, 2, 1}));
}

TEST(ReversedViewTest, ConstVectorTest) {
  const std::vector<int> v = {10, 20, 30};
  std::vector<int> reversed;
  for (const int x : reversed_view(v)) {
    reversed.push_back(x);
  }
  EXPECT_EQ(reversed, (std::vector<int>{30, 20, 10}));
}

TEST(ReversedViewTest, MutationTest) {
  std::vector<int> v = {1, 2, 3};
  for (int& x : reversed_view(v)) {
    x *= 2;
  }
  EXPECT_EQ(v, (std::vector<int>{2, 4, 6}));
}

TEST(ReversedViewTest, EmptyContainerTest) {
  std::vector<int> v;
  int count = 0;
  for ([[maybe_unused]] const int x : reversed_view(v)) {
    ++count;
  }
  EXPECT_EQ(count, 0);
}

TEST(ReversedViewTest, StringViewTest) {
  std::vector<std::string> strings = {"apple", "banana", "cherry"};
  std::string result;
  for (absl::string_view s : reversed_view(strings)) {
    result += s;
  }
  EXPECT_EQ(result, "cherrybananaapple");
}

TEST(ReversedViewTest, ArrayTest) {
  std::array<int, 4> arr = {1, 2, 3, 4};
  std::vector<int> reversed;
  for (const int x : reversed_view(arr)) {
    reversed.push_back(x);
  }
  EXPECT_EQ(reversed, (std::vector<int>{4, 3, 2, 1}));
}

TEST(ReversedViewTest, PortReversedViewNamespaceTest) {
  std::vector<int> v = {1, 2, 3};
  std::vector<int> reversed;
  for (const int x : port::reversed_view(v)) {
    reversed.push_back(x);
  }
  EXPECT_EQ(reversed, (std::vector<int>{3, 2, 1}));
}

TEST(ReversedViewTest, GloopCompatibilityTest) {
  std::vector<int> v = {10, 20, 30, 40, 50};
  std::vector<int> port_results;
  for (const int x : port::reversed_view(v)) {
    port_results.push_back(x);
  }

  std::vector<int> internal_results;
  for (const int x : port::internal::reversed_view(v)) {
    internal_results.push_back(x);
  }

  EXPECT_EQ(port_results, internal_results);
}

}  // namespace
}  // namespace mozc::port
