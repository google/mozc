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

#include "converter/segmenter.h"

#include <cstddef>
#include <cstdint>
#include <memory>

#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "base/container/bitarray.h"
#include "converter/node.h"
#include "testing/gunit.h"

namespace mozc {
namespace {

TEST(SegmenterTest, BasicLookupAndBoundsChecks) {
  constexpr uint16_t kLTable[] = {0, 1};
  constexpr uint16_t kRTable[] = {0, 1};
  constexpr uint16_t kBoundaryData[] = {
      10,
      20,  // POS 0: prefix=10, suffix=20
      30,
      40,  // POS 1: prefix=30, suffix=40
  };
  BitArray bits(4);
  bits.set(0 + 2 * 1);  // rid=0, lid=1 is a boundary
  bits.set(1 + 2 * 0);  // rid=1, lid=0 is a boundary
  const absl::Span<const char> bitarray_span(bits.array(), bits.array_size());

  absl::StatusOr<std::unique_ptr<Segmenter>> status_or_segmenter =
      Segmenter::Create(2, 2, kLTable, kRTable, bitarray_span, kBoundaryData);
  ASSERT_TRUE(status_or_segmenter.ok()) << status_or_segmenter.status();
  const Segmenter& segmenter = **status_or_segmenter;

  EXPECT_FALSE(segmenter.IsBoundary(0, 0));
  EXPECT_TRUE(segmenter.IsBoundary(0, 1));
  EXPECT_TRUE(segmenter.IsBoundary(1, 0));
  EXPECT_FALSE(segmenter.IsBoundary(1, 1));

  // Out-of-bounds POS IDs safely fall back without reading past tables.
  EXPECT_TRUE(segmenter.IsBoundary(2, 0));
  EXPECT_TRUE(segmenter.IsBoundary(0, 2));
  EXPECT_TRUE(segmenter.IsBoundary(65535, 65535));

  Node lnode;
  Node rnode;
  lnode.node_type = Node::NOR_NODE;
  rnode.node_type = Node::NOR_NODE;
  lnode.rid = 5000;
  rnode.lid = 6000;
  EXPECT_TRUE(segmenter.IsBoundary(lnode, rnode, false));

  EXPECT_EQ(segmenter.GetPrefixPenalty(0), 10);
  EXPECT_EQ(segmenter.GetSuffixPenalty(0), 20);
  EXPECT_EQ(segmenter.GetPrefixPenalty(1), 30);
  EXPECT_EQ(segmenter.GetSuffixPenalty(1), 40);
  EXPECT_EQ(segmenter.GetPrefixPenalty(2), 0);
  EXPECT_EQ(segmenter.GetSuffixPenalty(2), 0);
  EXPECT_EQ(segmenter.GetPrefixPenalty(65535), 0);
  EXPECT_EQ(segmenter.GetSuffixPenalty(65535), 0);
}

TEST(SegmenterTest, CreateRejectsInvalidInputs) {
  constexpr uint16_t kValidTable[] = {0, 1};
  constexpr uint16_t kOutOfRangeTable[] = {0, 2};
  constexpr uint16_t kEvenBoundary[] = {10, 20};
  constexpr uint16_t kOddBoundary[] = {10, 20, 30};
  constexpr char kOneByteBitArray[] = {'\x00'};

  // Odd boundary_data length.
  {
    absl::StatusOr<std::unique_ptr<Segmenter>> seg = Segmenter::Create(
        2, 2, kValidTable, kValidTable, kOneByteBitArray, kOddBoundary);
    EXPECT_FALSE(seg.ok());
  }

  // Bitarray too small for l_num_elements * r_num_elements (9 > 8 bits).
  {
    absl::StatusOr<std::unique_ptr<Segmenter>> seg = Segmenter::Create(
        3, 3, kValidTable, kValidTable, kOneByteBitArray, kEvenBoundary);
    EXPECT_FALSE(seg.ok());
  }

  // Dimensions whose product exceeds kMaxBitArrayElements (and overflows 32-bit
  // size_t) without triggering shift-count overflow on 32-bit targets.
  {
    constexpr size_t kLargeDim = size_t{1} << 17;
    absl::StatusOr<std::unique_ptr<Segmenter>> seg = Segmenter::Create(
        kLargeDim, kLargeDim, {}, {}, kOneByteBitArray, kEvenBoundary);
    EXPECT_FALSE(seg.ok());
  }

  // l_table entry >= l_num_elements.
  {
    absl::StatusOr<std::unique_ptr<Segmenter>> seg = Segmenter::Create(
        2, 2, kOutOfRangeTable, kValidTable, kOneByteBitArray, kEvenBoundary);
    EXPECT_FALSE(seg.ok());
  }

  // r_table entry >= r_num_elements.
  {
    absl::StatusOr<std::unique_ptr<Segmenter>> seg = Segmenter::Create(
        2, 2, kValidTable, kOutOfRangeTable, kOneByteBitArray, kEvenBoundary);
    EXPECT_FALSE(seg.ok());
  }
}

}  // namespace
}  // namespace mozc
