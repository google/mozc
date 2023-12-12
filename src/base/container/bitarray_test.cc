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

#include "base/container/bitarray.h"

#include <cstddef>
#include <utility>
#include <vector>

#include "absl/random/random.h"
#include "testing/gunit.h"

namespace mozc {
namespace {

TEST(BitArray, BitArraySizeTest) {
  {
    BitArray array(0);
    EXPECT_EQ(array.size(), 0);
    EXPECT_EQ(array.array_size(), 4);
  }

  {
    BitArray array(5);
    EXPECT_EQ(array.size(), 5);
    EXPECT_EQ(array.array_size(), 4);
  }

  {
    BitArray array(32);
    EXPECT_EQ(array.size(), 32);
    EXPECT_EQ(array.array_size(), 8);
  }

  {
    BitArray array(100);
    EXPECT_EQ(array.size(), 100);
    EXPECT_EQ(array.array_size(), 16);
  }
}

TEST(BitArray, BitArrayTest) {
  constexpr size_t kBitArraySize[] = {1, 2, 10, 32, 64, 100, 1000, 1024, 10000};
  absl::BitGen gen;

  for (const size_t size : kBitArraySize) {
    // set array
    BitArray array(size);
    EXPECT_EQ(array.size(), size);
    std::vector<int> target(size);
    for (size_t j = 0; j < size; ++j) {
      const bool v = absl::Bernoulli(gen, 0.5);
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
      EXPECT_EQ(BitArray::GetValue(data, j), (target[j] != 0));
      EXPECT_EQ(array.get(j), (target[j] != 0));
    }

    using std::swap;
    BitArray array2(0);
    swap(array, array2);
    EXPECT_EQ(array2.array(), data);
    EXPECT_EQ(array.size(), 0);
    EXPECT_EQ(array2.size(), size);
  }
}

}  // namespace
}  // namespace mozc
