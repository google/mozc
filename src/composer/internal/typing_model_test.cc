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

#include "composer/internal/typing_model.h"

#include "testing/base/public/gunit.h"

namespace mozc {
namespace composer {

class TypingModelTest : public ::testing::Test {};

TEST_F(TypingModelTest, Constructor) {
  const char* characters = "abcd";
  const uint8 costs[] = {
      0, 1, 2, 3, 4, 5, 6,
  };
  TypingModel model(characters, strlen(characters), costs, arraysize(costs),
                    nullptr);
  model.character_to_radix_table_['a'] = 1;
  model.character_to_radix_table_['b'] = 2;
  model.character_to_radix_table_['c'] = 3;
  model.character_to_radix_table_['d'] = 4;
  model.character_to_radix_table_['Z'] = 0;
}

TEST_F(TypingModelTest, GetIndex) {
  const char* characters = "abcd";
  const uint8 costs[] = {
      0, 1, 2, 3, 4, 5, 6,
  };
  TypingModel model(characters, strlen(characters), costs, arraysize(costs),
                    nullptr);
  ASSERT_EQ(0, model.GetIndex(""));
  ASSERT_EQ(1, model.GetIndex("a"));
  ASSERT_EQ(4, model.GetIndex("d"));
  ASSERT_EQ(6, model.GetIndex("aa"));
  ASSERT_EQ(9, model.GetIndex("ad"));
  ASSERT_EQ(31, model.GetIndex("aaa"));
}

}  // namespace composer
}  // namespace mozc
