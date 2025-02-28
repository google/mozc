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

#include "composer/special_key.h"

#include <string>

#include "composer/table.h"
#include "testing/gmock.h"
#include "testing/gunit.h"

namespace mozc::composer::internal {
namespace {

using ::testing::IsEmpty;

TEST(SpecialKeyTest, TrimLeadingpecialKey) {
  const Table table;
  std::string input = table.ParseSpecialKey("{!}ab");
  EXPECT_EQ(TrimLeadingSpecialKey(input), "ab");

  input = table.ParseSpecialKey("{!}{?}ab");
  EXPECT_EQ(TrimLeadingSpecialKey(input), table.ParseSpecialKey("{?}ab"));

  input = table.ParseSpecialKey("a{!}b");
  EXPECT_EQ(TrimLeadingSpecialKey(input), input);

  // Invalid patterns
  //   "\u000F" = parsed-"{"
  //   "\u000E" = parsed-"}"
  input = "\u000Fab";  // "{ab"
  EXPECT_EQ(TrimLeadingSpecialKey(input), input);
  input = "ab\u000E";  // "ab}"
  EXPECT_EQ(TrimLeadingSpecialKey(input), input);
  input = "\u000F\u000Fab\u000E";  // "{{ab}"
  EXPECT_THAT(TrimLeadingSpecialKey(input), IsEmpty());
  input = "\u000Fab\u000E\u000E";  // "{ab}}"
  EXPECT_EQ(TrimLeadingSpecialKey(input), "\u000E");
}

}  // namespace
}  // namespace mozc::composer::internal
