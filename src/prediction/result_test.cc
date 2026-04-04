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

#include "prediction/result.h"

#include "testing/gunit.h"

namespace mozc {
namespace prediction {
namespace {

TEST(ResultTest, ResultCostLessTest) {
  ResultCostLess cost_less;

  {
    Result r1, r2;
    r1.cost = 100;
    r2.cost = 200;
    EXPECT_TRUE(cost_less(r1, r2));
    EXPECT_FALSE(cost_less(r2, r1));
  }

  {
    Result r1, r2;
    r1.cost = 100;
    r2.cost = 100;
    r1.value = "a";
    r2.value = "b";
    EXPECT_TRUE(cost_less(r1, r2));
    EXPECT_FALSE(cost_less(r2, r1));
  }

  // Same cost and value, but different types.
  {
    Result r1, r2;
    r1.cost = 100;
    r2.cost = 100;
    r1.value = "value";
    r2.value = "value";
    r1.types = 1;
    r2.types = 2;
    // Now distinguish them.
    EXPECT_TRUE(cost_less(r1, r2));
    EXPECT_FALSE(cost_less(r2, r1));
  }

  // Same cost and value, but different candidate_attributes.
  {
    Result r1, r2;
    r1.cost = 100;
    r2.cost = 100;
    r1.value = "value";
    r2.value = "value";
    r1.candidate_attributes = 1;
    r2.candidate_attributes = 2;
    EXPECT_TRUE(cost_less(r1, r2));
    EXPECT_FALSE(cost_less(r2, r1));
  }

  // Same cost and value, but one of different candidate_attributes is 0.
  {
    Result r1, r2;
    r1.cost = 100;
    r2.cost = 100;
    r1.value = "value";
    r2.value = "value";
    r1.candidate_attributes = 1;
    r2.candidate_attributes = 0;
    EXPECT_TRUE(cost_less(r1, r2));
    EXPECT_FALSE(cost_less(r2, r1));
  }

  // Same cost, value, types, attributes, but different lid.
  {
    Result r1, r2;
    r1.cost = 100;
    r1.value = "v";
    r1.types = 1;
    r1.candidate_attributes = 1;
    r2.cost = 100;
    r2.value = "v";
    r2.types = 1;
    r2.candidate_attributes = 1;
    r1.lid = 10;
    r2.lid = 20;
    EXPECT_TRUE(cost_less(r1, r2));
    EXPECT_FALSE(cost_less(r2, r1));
  }

  // Same cost, value, types, attributes, lid, but different rid.
  {
    Result r1, r2;
    r1.cost = 100;
    r1.value = "v";
    r1.types = 1;
    r1.candidate_attributes = 1;
    r1.lid = 10;
    r2.cost = 100;
    r2.value = "v";
    r2.types = 1;
    r2.candidate_attributes = 1;
    r2.lid = 10;
    r1.rid = 10;
    r2.rid = 20;
    EXPECT_TRUE(cost_less(r1, r2));
    EXPECT_FALSE(cost_less(r2, r1));
  }

  // Same but different description.
  {
    Result r1, r2;
    r1.cost = 100;
    r1.value = "v";
    r1.types = 1;
    r1.candidate_attributes = 1;
    r2.cost = 100;
    r2.value = "v";
    r2.types = 1;
    r2.candidate_attributes = 1;
    r1.description = "a";
    r2.description = "b";
    EXPECT_TRUE(cost_less(r1, r2));
    EXPECT_FALSE(cost_less(r2, r1));
  }
}

TEST(ResultTest, ResultWCostLessTest) {
  ResultWCostLess wcost_less;

  {
    Result r1, r2;
    r1.wcost = 100;
    r2.wcost = 200;
    EXPECT_TRUE(wcost_less(r1, r2));
    EXPECT_FALSE(wcost_less(r2, r1));
  }

  {
    Result r1, r2;
    r1.wcost = 100;
    r2.wcost = 100;
    r1.value = "a";
    r2.value = "b";
    EXPECT_TRUE(wcost_less(r1, r2));
    EXPECT_FALSE(wcost_less(r2, r1));
  }
}

}  // namespace
}  // namespace prediction
}  // namespace mozc
