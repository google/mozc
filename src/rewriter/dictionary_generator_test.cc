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

#include "rewriter/dictionary_generator.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace rewriter {

TEST(TokenTest, GetID) {
  Token token_empty;
  Token token_simple;
  token_simple.set_key("mozc");
  token_simple.set_value("MOZC");
  token_simple.set_pos("noun");

  Token token_rich;
  token_rich.set_key("mozc");
  token_rich.set_value("MOZC");
  token_rich.set_pos("noun");
  token_rich.set_sorting_key(100);
  token_rich.set_description("Google IME");

  EXPECT_NE(token_empty.GetID(), token_simple.GetID());
  EXPECT_EQ(token_rich.GetID(), token_simple.GetID());
}

TEST(TokenTest, MergeFrom) {
  Token token_to;
  token_to.set_key("mozc");
  token_to.set_value("MOZC");
  token_to.set_pos("noun");

  Token token_from;
  token_from.set_key("");
  token_from.set_value("moooozc");
  // pos is not set.
  token_from.set_sorting_key(100);
  token_from.set_description("Google IME");

  token_to.MergeFrom(token_from);
  EXPECT_EQ("mozc", token_to.key());
  EXPECT_EQ("moooozc", token_to.value());
  EXPECT_EQ("noun", token_to.pos());
  EXPECT_EQ(100, token_to.sorting_key());
  EXPECT_EQ("Google IME", token_to.description());

  // It is possible that the IDs are different.
  EXPECT_NE(token_from.GetID(), token_to.GetID());
}

}  // namespace rewriter
}  // namespace mozc
