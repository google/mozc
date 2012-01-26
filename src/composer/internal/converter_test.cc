// Copyright 2010-2012, Google Inc.
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

#include "base/base.h"
#include "composer/table.h"
#include "testing/base/public/gunit.h"

static void InitTable(mozc::composer::Table* table) {
  // "あ"
  table->AddRule("a",  "\xE3\x81\x82", "");
  // "い"
  table->AddRule("i",  "\xE3\x81\x84", "");
  // "か"
  table->AddRule("ka", "\xE3\x81\x8B", "");
  // "き"
  table->AddRule("ki", "\xE3\x81\x8D", "");
  // "く"
  table->AddRule("ku", "\xE3\x81\x8F", "");
  // "け"
  table->AddRule("ke", "\xE3\x81\x91", "");
  // "こ"
  table->AddRule("ko", "\xE3\x81\x93", "");
  // "っ"
  table->AddRule("kk", "\xE3\x81\xA3", "k");
  // "な"
  table->AddRule("na", "\xE3\x81\xAA", "");
  // "に"
  table->AddRule("ni", "\xE3\x81\xAB", "");
  // "ん"
  table->AddRule("n",  "\xE3\x82\x93", "");
  // "ん"
  table->AddRule("nn", "\xE3\x82\x93", "");
}

TEST(ConverterTest, Converter) {
  static const struct TestCase {
    const char* input;
    const char* expected_output;
  } test_cases[] = {
    // "あ"
    { "a", "\xE3\x81\x82" },
    // "か"
    { "ka", "\xE3\x81\x8B" },
    // "き"
    { "ki", "\xE3\x81\x8D" },
    // "く"
    { "ku", "\xE3\x81\x8F" },
    // "っk"
    { "kk", "\xE3\x81\xA3\x6B" },
    // "あか"
    { "aka", "\xE3\x81\x82\xE3\x81\x8B" },
    // "かきzっか"
    { "kakizkka", "\xE3\x81\x8B\xE3\x81\x8D\x7A\xE3\x81\xA3\xE3\x81\x8B" },
    // "なんかない?"
    { "nankanai?", "\xE3\x81\xAA\xE3\x82\x93\xE3\x81\x8B\xE3\x81\xAA\xE3\x81"
                   "\x84\x3F" },
    // "なんかないん?"
    { "nannkanain?", "\xE3\x81\xAA\xE3\x82\x93\xE3\x81\x8B\xE3\x81\xAA\xE3\x81"
                     "\x84\xE3\x82\x93\x3F" },
    // "なんかないん"
    { "nannkanain", "\xE3\x81\xAA\xE3\x82\x93\xE3\x81\x8B\xE3\x81\xAA\xE3\x81"
                    "\x84\xE3\x82\x93" },
  };
  static const int size = ARRAYSIZE(test_cases);

  mozc::composer::Table table;
  InitTable(&table);
  mozc::composer::Converter converter(table);

  for (int i = 0; i < size; ++i) {
    const TestCase& test = test_cases[i];
    string output;
    converter.Convert(test.input, &output);
    EXPECT_EQ(test.expected_output, output);
  }
}
