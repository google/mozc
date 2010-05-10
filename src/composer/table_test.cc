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

#include "composer/table.h"

#include "base/base.h"
#include "base/util.h"
#include "testing/base/public/gunit.h"
#include "session/config.pb.h"
#include "session/config_handler.h"

DECLARE_string(test_tmpdir);

// This macro is ported from base/basictypes.h.  Due to WinNT.h has
// the same macro, this macro should be undefined at the end of this
// file.
// TODO(komatsu): Move this macro to base or testing.
#ifndef ARRAYSIZE
#define UNDEFINE_ARRAYSIZE
#define ARRAYSIZE(a) \
  ((sizeof(a) / sizeof(*(a))) / \
    static_cast<size_t>(!(sizeof(a) % sizeof(*(a)))))
#endif  // ARRAYSIZE

static void InitTable(mozc::composer::Table* table) {
  // "あ"
  table->AddRule("a",  "\xe3\x81\x82", "");
  // "い"
  table->AddRule("i",  "\xe3\x81\x84", "");
  // "か"
  table->AddRule("ka", "\xe3\x81\x8b", "");
  // "き"
  table->AddRule("ki", "\xe3\x81\x8d", "");
  // "く"
  table->AddRule("ku", "\xe3\x81\x8f", "");
  // "け"
  table->AddRule("ke", "\xe3\x81\x91", "");
  // "こ"
  table->AddRule("ko", "\xe3\x81\x93", "");
  // "っ"
  table->AddRule("kk", "\xe3\x81\xa3", "k");
  // "な"
  table->AddRule("na", "\xe3\x81\xaa", "");
  // "に"
  table->AddRule("ni", "\xe3\x81\xab", "");
  // "ん"
  table->AddRule("n",  "\xe3\x82\x93", "");
  // "ん"
  table->AddRule("nn", "\xe3\x82\x93", "");
}

string GetResult(const mozc::composer::Table &table, const string &key) {
  const mozc::composer::Entry *entry = table.LookUp(key);
  if (entry == NULL) {
    return "<NULL>";
  }
  return entry->result();
}

string GetInput(const mozc::composer::Table &table, const string &key) {
  const mozc::composer::Entry *entry = table.LookUp(key);
  if (entry == NULL) {
    return "<NULL>";
  }
  return entry->input();
}

TEST(TableTest, LookUp) {
  static const struct TestCase {
    const char* input;
    const bool expected_result;
    const char* expected_output;
    const char* expected_pending;
  } test_cases[] = {
    // "あ"
    { "a", true, "\xe3\x81\x82", "" },
    { "k", false, "", "" },
    // "か"
    { "ka", true, "\xe3\x81\x8b", "" },
    // "き"
    { "ki", true, "\xe3\x81\x8d", "" },
    // "く"
    { "ku", true, "\xe3\x81\x8f", "" },
    // "っ"
    { "kk", true, "\xe3\x81\xa3", "k" },
    { "aka", false, "", "" },
    // "な"
    { "na", true, "\xe3\x81\xaa", "" },
    // "ん"
    { "n", true, "\xe3\x82\x93", "" },
    // "ん"
    { "nn", true, "\xe3\x82\x93", "" },
  };
  static const int size = ARRAYSIZE(test_cases);

  mozc::composer::Table table;
  InitTable(&table);

  for (int i = 0; i < size; ++i) {
    const TestCase& test = test_cases[i];
    string output;
    string pending;
    const mozc::composer::Entry* entry;
    entry = table.LookUp(test.input);

    EXPECT_EQ(test.expected_result, (entry != NULL));
    if (entry == NULL) {
      continue;
    }
    EXPECT_EQ(test.expected_output, entry->result());
    EXPECT_EQ(test.expected_pending, entry->pending());
  }
}

TEST(TableTest, Puncutations) {
  static const struct TestCase {
    mozc::config::Config::PunctuationMethod method;
    const char *input;
    const char *expected;
  } test_cases[] = {
    // "、"
    { mozc::config::Config::KUTEN_TOUTEN, ",",  "\xe3\x80\x81" },
    // "。"
    { mozc::config::Config::KUTEN_TOUTEN, ".",  "\xe3\x80\x82" },
    // "，"
    { mozc::config::Config::COMMA_PERIOD, ",",  "\xef\xbc\x8c" },
    // "．"
    { mozc::config::Config::COMMA_PERIOD, ".",  "\xef\xbc\x8e" },
    // "、"
    { mozc::config::Config::KUTEN_PERIOD, ",",  "\xe3\x80\x81" },
    // "．"
    { mozc::config::Config::KUTEN_PERIOD, ".",  "\xef\xbc\x8e" },
    // "，"
    { mozc::config::Config::COMMA_TOUTEN, ",",  "\xef\xbc\x8c" },
    // "。"
    { mozc::config::Config::COMMA_TOUTEN, ".",  "\xe3\x80\x82" },
  };

  const string config_file = mozc::Util::JoinPath(FLAGS_test_tmpdir,
                                                  "mozc_config_test_tmp");
  mozc::Util::Unlink(config_file);
  mozc::config::ConfigHandler::SetConfigFileName(config_file);
  mozc::config::ConfigHandler::Reload();

  for (int i = 0; i < ARRAYSIZE(test_cases); ++i) {
    mozc::config::Config config;
    config.set_punctuation_method(test_cases[i].method);
    EXPECT_TRUE(mozc::config::ConfigHandler::SetConfig(config));
    mozc::composer::Table table;
    table.Initialize();
    const mozc::composer::Entry *entry = table.LookUp(test_cases[i].input);
    EXPECT_TRUE(entry != NULL);
    EXPECT_EQ(test_cases[i].expected, entry->result());
  }
}

TEST(TableTest, Symbols) {
  static const struct TestCase {
    mozc::config::Config::SymbolMethod method;
    const char *input;
    const char *expected;
  } test_cases[] = {
    // "「"
    { mozc::config::Config::CORNER_BRACKET_MIDDLE_DOT, "[",  "\xe3\x80\x8c" },
    // "」"
    { mozc::config::Config::CORNER_BRACKET_MIDDLE_DOT, "]",  "\xe3\x80\x8d" },
    // "・"
    { mozc::config::Config::CORNER_BRACKET_MIDDLE_DOT, "/",  "\xe3\x83\xbb" },
    { mozc::config::Config::SQUARE_BRACKET_SLASH, "[",  "["      },
    { mozc::config::Config::SQUARE_BRACKET_SLASH, "]",  "]"      },
    // "／"
    { mozc::config::Config::SQUARE_BRACKET_SLASH, "/",  "\xef\xbc\x8f"      },
    // "「"
    { mozc::config::Config::CORNER_BRACKET_SLASH, "[",  "\xe3\x80\x8c"      },
    // "」"
    { mozc::config::Config::CORNER_BRACKET_SLASH, "]",  "\xe3\x80\x8d"      },
    // "／"
    { mozc::config::Config::CORNER_BRACKET_SLASH, "/",  "\xef\xbc\x8f"      },
    { mozc::config::Config::SQUARE_BRACKET_MIDDLE_DOT, "[",  "[" },
    { mozc::config::Config::SQUARE_BRACKET_MIDDLE_DOT, "]",  "]" },
    // "・"
    { mozc::config::Config::SQUARE_BRACKET_MIDDLE_DOT, "/",  "\xe3\x83\xbb" },
  };

  const string config_file = mozc::Util::JoinPath(FLAGS_test_tmpdir,
                                                  "mozc_config_test_tmp");
  mozc::Util::Unlink(config_file);
  mozc::config::ConfigHandler::SetConfigFileName(config_file);
  mozc::config::ConfigHandler::Reload();

  for (int i = 0; i < ARRAYSIZE(test_cases); ++i) {
    mozc::config::Config config;
    config.set_symbol_method(test_cases[i].method);
    EXPECT_TRUE(mozc::config::ConfigHandler::SetConfig(config));
    mozc::composer::Table table;
    table.Initialize();
    const mozc::composer::Entry *entry = table.LookUp(test_cases[i].input);
    EXPECT_TRUE(entry != NULL);
    EXPECT_EQ(test_cases[i].expected, entry->result());
  }
}

TEST(TableTest, KanaSuppressed) {
  mozc::config::Config config;
  mozc::config::ConfigHandler::GetConfig(&config);

  config.set_preedit_method(mozc::config::Config::KANA);
  mozc::config::ConfigHandler::SetConfig(config);

  mozc::composer::Table table;
  table.Initialize();

  const mozc::composer::Entry *entry = table.LookUp("a");
  EXPECT_TRUE(entry != NULL);
  // "あ"
  EXPECT_EQ("\xE3\x81\x82", entry->result());
  EXPECT_TRUE(entry->pending().empty());
}

TEST(TableTest, KanaCombination) {
  mozc::composer::Table table;
  ASSERT_TRUE(table.Initialize());
  // "か゛"
  const mozc::composer::Entry *entry = table.LookUp("\xE3\x81\x8B\xE3\x82\x9B");
  EXPECT_TRUE(entry != NULL);
  // "が"
  EXPECT_EQ("\xE3\x81\x8C", entry->result());
  EXPECT_TRUE(entry->pending().empty());
}

TEST(TableTest, InvalidEntryTest) {
  {
    mozc::composer::Table table;
    table.AddRule("a", "aa", "");
    table.AddRule("b", "aa", "a");  // looping
    EXPECT_TRUE(table.LookUp("a") != NULL);
    EXPECT_TRUE(table.LookUp("b") == NULL);
  }

  {
    mozc::composer::Table table;
    table.AddRule("a", "aa", "c");
    table.AddRule("c", "aa", "b");
    table.AddRule("d", "aa", "a");  // looping
    EXPECT_TRUE(table.LookUp("a") != NULL);
    EXPECT_TRUE(table.LookUp("c") != NULL);
    EXPECT_TRUE(table.LookUp("d") == NULL);
  }

  {
    mozc::composer::Table table;
    table.AddRule("a", "aa", "a");
    EXPECT_TRUE(table.LookUp("a") == NULL);
  }
}

TEST(TableTest, CustomPunctuationsAndSymbols) {
  // Test against Issue2465801.
  string custom_roman_table;
  custom_roman_table.append("mozc\tMOZC\n");
  custom_roman_table.append(",\tCOMMA\n");
  custom_roman_table.append(".\tPERIOD\n");
  custom_roman_table.append("/\tSLASH\n");
  custom_roman_table.append("[\tOPEN\n");
  custom_roman_table.append("]\tCLOSE\n");

  mozc::config::Config config;
  config.set_custom_roman_table(custom_roman_table);
  EXPECT_TRUE(mozc::config::ConfigHandler::SetConfig(config));

  mozc::composer::Table table;
  table.Initialize();

  const mozc::composer::Entry *entry = NULL;
  entry = table.LookUp("mozc");
  ASSERT_TRUE(entry != NULL);
  EXPECT_EQ("MOZC", entry->result());

  entry = table.LookUp(",");
  ASSERT_TRUE(entry != NULL);
  EXPECT_EQ("COMMA", entry->result());

  entry = table.LookUp(".");
  ASSERT_TRUE(entry != NULL);
  EXPECT_EQ("PERIOD", entry->result());

  entry = table.LookUp("/");
  ASSERT_TRUE(entry != NULL);
  EXPECT_EQ("SLASH", entry->result());

  entry = table.LookUp("[");
  ASSERT_TRUE(entry != NULL);
  EXPECT_EQ("OPEN", entry->result());

  entry = table.LookUp("]");
  ASSERT_TRUE(entry != NULL);
  EXPECT_EQ("CLOSE", entry->result());
}

TEST(TableTest, CaseSensitive) {
  mozc::composer::Table table;
  table.AddRule("a", "[a]", "");
  table.AddRule("A", "[A]", "");
  table.AddRule("ba", "[ba]", "");
  table.AddRule("BA", "[BA]", "");
  table.AddRule("Ba", "[Ba]", "");
  // The rule of "bA" is intentionally dropped.
  // table.AddRule("bA",  "[bA]", "");
  table.AddRule("za", "[za]", "");

  // case insensitive (default)
  EXPECT_FALSE(table.case_sensitive());
  EXPECT_EQ("[a]", GetResult(table, "a"));
  EXPECT_EQ("[a]", GetResult(table, "A"));
  EXPECT_EQ("[ba]", GetResult(table, "ba"));
  EXPECT_EQ("[ba]", GetResult(table, "BA"));
  EXPECT_EQ("[ba]", GetResult(table, "Ba"));
  EXPECT_EQ("[ba]", GetResult(table, "bA"));

  EXPECT_EQ("a", GetInput(table, "a"));
  EXPECT_EQ("a", GetInput(table, "A"));
  EXPECT_EQ("ba", GetInput(table, "ba"));
  EXPECT_EQ("ba", GetInput(table, "BA"));
  EXPECT_EQ("ba", GetInput(table, "Ba"));
  EXPECT_EQ("ba", GetInput(table, "bA"));

  // Test for HasSubRules
  EXPECT_TRUE(table.HasSubRules("Z"));

  {  // Test for LookUpPrefix
    const mozc::composer::Entry *entry = NULL;
    size_t key_length = 0;
    bool fixed = false;
    entry = table.LookUpPrefix("bA", &key_length, &fixed);
    EXPECT_TRUE(entry != NULL);
    EXPECT_EQ("[ba]", entry->result());
    EXPECT_EQ(2, key_length);
    EXPECT_TRUE(fixed);
  }

  // case sensitive
  table.set_case_sensitive(true);
  EXPECT_TRUE(table.case_sensitive());
  EXPECT_EQ("[a]", GetResult(table, "a"));
  EXPECT_EQ("[A]", GetResult(table, "A"));
  EXPECT_EQ("[ba]", GetResult(table, "ba"));
  EXPECT_EQ("[BA]", GetResult(table, "BA"));
  EXPECT_EQ("[Ba]", GetResult(table, "Ba"));
  EXPECT_EQ("<NULL>", GetResult(table, "bA"));

  EXPECT_EQ("a", GetInput(table, "a"));
  EXPECT_EQ("A", GetInput(table, "A"));
  EXPECT_EQ("ba", GetInput(table, "ba"));
  EXPECT_EQ("BA", GetInput(table, "BA"));
  EXPECT_EQ("Ba", GetInput(table, "Ba"));
  EXPECT_EQ("<NULL>", GetInput(table, "bA"));

  // Test for HasSubRules
  EXPECT_FALSE(table.HasSubRules("Z"));

  {  // Test for LookUpPrefix
    const mozc::composer::Entry *entry = NULL;
    size_t key_length = 0;
    bool fixed = false;
    entry = table.LookUpPrefix("bA", &key_length, &fixed);
    EXPECT_TRUE(entry == NULL);
    EXPECT_EQ(1, key_length);
    EXPECT_TRUE(fixed);
  }
}

#ifdef UNDEFINE_ARRAYSIZE
#undef ARRAYSIZE
#endif  // UNDEFINE_ARRAYSIZE
