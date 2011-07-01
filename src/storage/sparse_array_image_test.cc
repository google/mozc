// Copyright 2010-2011, Google Inc.
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

#include "storage/sparse_array_image.h"

#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

namespace mozc {
TEST(SparseArray, basic) {
  SparseArrayBuilder builder;
  for (int i = 0; i <= 100; ++i) {
    builder.AddValue(100 + i * i, i);
  }
  builder.AddValue(0, 10);
  builder.AddValue(10, 20);
  builder.AddValue(100, 30);
  builder.AddValue(0x1ffff, 40);
  builder.AddValue(0xffff0001, 50);
  builder.Build();
  SparseArrayImage image(builder.GetImage(), builder.GetSize());
  EXPECT_EQ(image.PeekFromArray(0), 0);
  EXPECT_EQ(image.PeekFromArray(10), 1);
  EXPECT_EQ(image.PeekFromArray(100), 2);
  EXPECT_EQ(image.PeekFromArray(10100), 102);
  EXPECT_EQ(image.PeekFromArray(0x1ffff), 103);
  // check signedness issue.
  EXPECT_EQ(image.PeekFromArray(0xffff0001), 104);


  EXPECT_EQ(image.PeekFromArray(1), -1);
  EXPECT_EQ(image.PeekFromArray(99), -1);
  EXPECT_EQ(image.GetValue(0), 10);
  EXPECT_EQ(image.GetValue(1), 20);
  EXPECT_EQ(image.GetValue(2), 30);
}

TEST(SparseArray, use_1byte_value) {
  SparseArrayBuilder builder;
  builder.SetUse1ByteValue(true);
  for (int i = 0; i <= 100; ++i) {
    builder.AddValue(100 + i * i, i);
  }
  builder.Build();
  SparseArrayImage image(builder.GetImage(), builder.GetSize());
  for (int i = 0; i <= 100; ++i) {
    EXPECT_EQ(i, image.PeekFromArray(100 + i * i));
    EXPECT_EQ(i, image.GetValue(i));
  }
}

TEST(SparseArray, cache) {
  SparseArrayBuilder builder;
  // Cache bucket eviction should happen because 300 > kCacheSize.
  for (int i = 0; i <= 300; ++i) {
    builder.AddValue(100 + i * i, 0);
  }
  builder.Build();
  SparseArrayImage image(builder.GetImage(), builder.GetSize());
  for (int i = 0; i <= 300; ++i) {
    EXPECT_EQ(i, image.Peek(100 + i * i));
  }
}
}  // namespace mozc
