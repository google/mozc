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

#include <string>
#include "base/base.h"
#include "converter/node.h"
#include "converter/converter_data.h"
#include "testing/base/public/gunit.h"

TEST(ConverterDataTest, ConverterDataTest) {
  mozc::ConverterData data;

  EXPECT_EQ("", data.key());
  EXPECT_FALSE(data.has_lattice());

  data.set_key("this is a test", mozc::KeyCorrector::ROMAN);
  EXPECT_TRUE(data.has_lattice());

  mozc::Node *node = data.NewNode();
  EXPECT_TRUE(node != NULL);
  EXPECT_EQ(0, node->lid);
  EXPECT_EQ(0, node->rid);

  mozc::Node **bos = data.begin_nodes_list();
  EXPECT_TRUE(bos != NULL);

  mozc::Node **eos = data.end_nodes_list();
  EXPECT_TRUE(eos != NULL);

  data.clear_lattice();
  EXPECT_EQ("", data.key());
  EXPECT_FALSE(data.has_lattice());
}
