// Copyright 2010-2014, Google Inc.
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

#include "converter/node.h"

#include "testing/base/public/gunit.h"

namespace mozc {
namespace {
const char *kTypeKey1 = "type1";
const char *kTypeKey2 = "type2";
class Type1 : public NodeAllocatorData::Data {
 public:
  virtual ~Type1() {}
  int x;
};
class Type2 : public NodeAllocatorData::Data {
 public:
  virtual ~Type2() {}
  int x;
};
}  // namespace

TEST(NodeAllocatorDataTest, Basic) {
  NodeAllocatorData data;
  EXPECT_FALSE(data.has(kTypeKey1));
  Type1 *t1;
  Type2 *t2;
  t2 = data.get<Type2>(kTypeKey2);
  t2->x = 100;
  EXPECT_FALSE(data.has(kTypeKey1));
  EXPECT_TRUE(data.has(kTypeKey2));

  t1 = data.get<Type1>(kTypeKey1);
  t1->x = 10;
  EXPECT_TRUE(data.has(kTypeKey1));
  EXPECT_TRUE(data.has(kTypeKey2));

  t2 = data.get<Type2>(kTypeKey2);
  EXPECT_EQ(100, t2->x);

  data.erase(kTypeKey1);
  EXPECT_FALSE(data.has(kTypeKey1));
  EXPECT_TRUE(data.has(kTypeKey2));

  data.clear();
  EXPECT_FALSE(data.has(kTypeKey1));
  EXPECT_FALSE(data.has(kTypeKey2));
}
}  // mozc
