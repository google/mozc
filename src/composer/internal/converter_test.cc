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

#include "composer/internal/converter.h"

#include <iterator>
#include <string>

#include "composer/table.h"
#include "testing/gunit.h"

namespace mozc {
namespace {

void InitTable(mozc::composer::Table *table) {
  table->AddRule("a", "あ", "");
  table->AddRule("i", "い", "");
  table->AddRule("ka", "か", "");
  table->AddRule("ki", "き", "");
  table->AddRule("ku", "く", "");
  table->AddRule("ke", "け", "");
  table->AddRule("ko", "こ", "");
  table->AddRule("kk", "っ", "k");
  table->AddRule("na", "な", "");
  table->AddRule("ni", "に", "");
  table->AddRule("n", "ん", "");
  table->AddRule("nn", "ん", "");
}

TEST(ConverterTest, Converter) {
  static const struct TestCase {
    const char *input;
    const char *expected_output;
  } test_cases[] = {
      {"a", "あ"},
      {"ka", "か"},
      {"ki", "き"},
      {"ku", "く"},
      {"kk", "っk"},
      {"aka", "あか"},
      {"kakizkka", "かきzっか"},
      {"nankanai?", "なんかない?"},
      {"nannkanain?", "なんかないん?"},
      {"nannkanain", "なんかないん"},
  };
  static const int size = std::size(test_cases);

  mozc::composer::Table table;
  InitTable(&table);
  mozc::composer::Converter converter(table);

  for (int i = 0; i < size; ++i) {
    const TestCase &test = test_cases[i];
    std::string output;
    converter.Convert(test.input, &output);
    EXPECT_EQ(output, test.expected_output);
  }
}

}  // namespace
}  // namespace mozc
