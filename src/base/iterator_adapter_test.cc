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

#include "base/iterator_adapter.h"

#include <algorithm>
#include <vector>

#include "base/port.h"
#include "testing/base/public/gunit.h"

namespace {

using mozc::AdapterBase;
using mozc::MakeIteratorAdapter;

struct TestStruct {
  int field1;
  int field2;
};

struct GetField1 : public AdapterBase<int> {
  int operator()(const TestStruct *test) const { return test->field1; }
};

struct GetField2 : public AdapterBase<int> {
  int operator()(const TestStruct *test) const { return test->field2; }
};

TEST(IteratorAdapterTest, LowerBound) {
  const TestStruct kTestData[] = {
      {0, 10}, {1, 11}, {2, 12}, {3, 13}, {3, 14}, {4, 14},
  };

  EXPECT_EQ(
      kTestData + 3,
      std::lower_bound(
          MakeIteratorAdapter(kTestData, GetField1()),
          MakeIteratorAdapter(kTestData + arraysize(kTestData), GetField1()), 3)
          .base());

  EXPECT_EQ(kTestData + arraysize(kTestData),
            std::lower_bound(MakeIteratorAdapter(kTestData, GetField1()),
                             MakeIteratorAdapter(
                                 kTestData + arraysize(kTestData), GetField1()),
                             12)
                .base());

  EXPECT_EQ(kTestData + 2,
            std::lower_bound(MakeIteratorAdapter(kTestData, GetField2()),
                             MakeIteratorAdapter(
                                 kTestData + arraysize(kTestData), GetField2()),
                             12)
                .base());
}

TEST(IteratorAdapterTest, Count) {
  const TestStruct kTestData[] = {
      {1, 10}, {1, 20}, {2, 30}, {2, 40}, {1, 50},
  };

  EXPECT_EQ(3, std::count(MakeIteratorAdapter(kTestData, GetField1()),
                          MakeIteratorAdapter(kTestData + arraysize(kTestData),
                                              GetField1()),
                          1));
  EXPECT_EQ(2, std::count(MakeIteratorAdapter(kTestData, GetField1()),
                          MakeIteratorAdapter(kTestData + arraysize(kTestData),
                                              GetField1()),
                          2));
}

}  // namespace
