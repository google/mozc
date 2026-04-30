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

#include "converter/debug_util.h"

#include <string>

#include "converter/lattice.h"
#include "converter/node.h"
#include "testing/gunit.h"

namespace mozc {
namespace converter {
namespace {

TEST(DebugUtilTest, DumpNodesEmpty) {
  Lattice lattice;
  lattice.SetKey("");

  const std::string dump = DumpNodes(lattice);
  EXPECT_FALSE(dump.empty());
  // The header and BOS node should be output.
  EXPECT_NE(dump.find("BOS"), std::string::npos);
}

TEST(DebugUtilTest, DumpNodesSimple) {
  Lattice lattice;
  lattice.SetKey("a");

  Node* n1 = lattice.NewNode();
  n1->key = "a";
  n1->value = "A";
  n1->begin_pos = 0;
  n1->end_pos = 1;
  lattice.Insert(0, n1);

  const std::string dump = DumpNodes(lattice);
  EXPECT_FALSE(dump.empty());
  EXPECT_NE(dump.find("BOS"), std::string::npos);
  EXPECT_NE(dump.find("\ta\tA\t"), std::string::npos);
}

}  // namespace
}  // namespace converter
}  // namespace mozc
