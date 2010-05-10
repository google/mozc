// Copyright 2010, Google Inc.
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

#include <vector>
#include "base/base.h"
#include "base/bitarray.h"
#include "base/util.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

int rand_range(int x) {
  return static_cast<int>(1.0 * rand() / RAND_MAX * x);
}

namespace mozc {
TEST(BitArray, BitArraySizeTest) {
  {
    BitArray array(0);
    EXPECT_EQ(0, array.size());
    EXPECT_EQ(4, array.array_size());
  }

  {
    BitArray array(5);
    EXPECT_EQ(5, array.size());
    EXPECT_EQ(4, array.array_size());
  }

  {
    BitArray array(32);
    EXPECT_EQ(32, array.size());
    EXPECT_EQ(8, array.array_size());
  }

  {
    BitArray array(100);
    EXPECT_EQ(100, array.size());
    EXPECT_EQ(16, array.array_size());
  }
}

TEST(BitArray, BitArrayTest) {
  const size_t kBitArraySize[] =
      { 1, 2, 10, 32, 64, 100, 1000, 1024, 10000 };

  for (size_t i = 0; i < arraysize(kBitArraySize); ++i) {
    const size_t size = kBitArraySize[i];

    // set array
    BitArray array(size);
    EXPECT_EQ(size, array.size());
    vector<int> target(size);
    for (size_t j = 0; j < size; ++j) {
      const bool v = (1.0 * rand() / RAND_MAX > 0.5);
      if (v) {
        target[j] = 1;
        array.set(j);
      } else {
        target[j] = 0;
        array.clear(j);
      }
    }

    const char *data = array.array();

    // verify
    for (size_t j = 0; j < size; ++j) {
      EXPECT_EQ(static_cast<bool>(target[j]), BitArray::GetValue(data, j));
      EXPECT_EQ(static_cast<bool>(target[j]), array.get(j));
    }
  }
}
}  // namespace mozc
