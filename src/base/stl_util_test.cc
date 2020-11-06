// Copyright 2010-2020, Google Inc.
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

#include "base/stl_util.h"

#include "testing/base/public/gunit.h"

namespace mozc {
namespace {

// This templatized helper can subclass any class and count how many times
// instances have been constructed or destructed.  It is used to make sure
// that deletes are actually occurring.
template <class T>
class InstanceCounter : public T {
 public:
  InstanceCounter<T>() : T() { ++instance_count; }
  ~InstanceCounter<T>() { --instance_count; }
  static int instance_count;
};
template <class T>
int InstanceCounter<T>::instance_count = 0;

}  // namespace

TEST(STLDeleteElementsTest, STLDeleteElements) {
  std::vector<InstanceCounter<std::string> *> v;
  v.push_back(new InstanceCounter<std::string>());
  v.push_back(new InstanceCounter<std::string>());
  v.push_back(new InstanceCounter<std::string>());
  EXPECT_EQ(3, InstanceCounter<std::string>::instance_count);
  v.push_back(nullptr);
  EXPECT_EQ(3, InstanceCounter<std::string>::instance_count);
  mozc::STLDeleteElements(&v);
  EXPECT_EQ(0, InstanceCounter<std::string>::instance_count);
  EXPECT_EQ(0, v.size());

  // Deleting nullptr pointers to containers is ok.
  std::vector<InstanceCounter<std::string> *> *p = nullptr;
  mozc::STLDeleteElements(p);
}

TEST(StlUtilTest, OrderBy) {
  typedef OrderBy<FirstKey, Less> OrderByFirst;
  typedef OrderBy<SecondKey, Less> OrderBySecond;

  EXPECT_TRUE(OrderByFirst()(std::make_pair(0, 1), std::make_pair(1, 0)));
  EXPECT_FALSE(OrderBySecond()(std::make_pair(0, 1), std::make_pair(1, 0)));
}

TEST(StlUtilTest, Key) {
  EXPECT_EQ(1, FirstKey()(std::make_pair(1, "abc")));
  EXPECT_EQ(2, FirstKey()(std::make_pair(2, "abc")));

  EXPECT_STREQ("abc", SecondKey()(std::make_pair(1, "abc")));
  EXPECT_STREQ("def", SecondKey()(std::make_pair(1, "def")));
}

TEST(StlUtilTest, FunctionalComparator) {
  EXPECT_TRUE(EqualTo()(1, 1));
  EXPECT_FALSE(EqualTo()(1, 2));
  EXPECT_FALSE(EqualTo()(2, 1));
  EXPECT_TRUE(EqualTo()(2, 2));

  EXPECT_FALSE(NotEqualTo()(1, 1));
  EXPECT_TRUE(NotEqualTo()(1, 2));
  EXPECT_TRUE(NotEqualTo()(2, 1));
  EXPECT_FALSE(NotEqualTo()(2, 2));

  EXPECT_FALSE(Less()(1, 1));
  EXPECT_TRUE(Less()(1, 2));
  EXPECT_FALSE(Less()(2, 1));
  EXPECT_FALSE(Less()(2, 2));

  EXPECT_FALSE(Greater()(1, 1));
  EXPECT_FALSE(Greater()(1, 2));
  EXPECT_TRUE(Greater()(2, 1));
  EXPECT_FALSE(Greater()(2, 2));

  EXPECT_TRUE(LessEqual()(1, 1));
  EXPECT_TRUE(Less()(1, 2));
  EXPECT_FALSE(Less()(2, 1));
  EXPECT_TRUE(LessEqual()(2, 2));

  EXPECT_TRUE(GreaterEqual()(1, 1));
  EXPECT_FALSE(Greater()(1, 2));
  EXPECT_TRUE(Greater()(2, 1));
  EXPECT_TRUE(GreaterEqual()(2, 2));
}

}  // namespace mozc
