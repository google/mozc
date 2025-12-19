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

#include "composer/table.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "composer/special_key.h"
#include "config/config_handler.h"
#include "data_manager/testing/mock_data_manager.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "testing/gunit.h"

namespace mozc::composer {
namespace {

using ::mozc::commands::Request;
using ::mozc::composer::internal::DeleteSpecialKeys;
using ::mozc::config::Config;

static void InitTable(Table* table) {
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

std::string GetResult(const Table& table, const absl::string_view key) {
  const Entry* entry = table.LookUp(key);
  if (entry == nullptr) {
    return "<nullptr>";
  }
  return entry->result();
}

std::string GetInput(const Table& table, const absl::string_view key) {
  const Entry* entry = table.LookUp(key);
  if (entry == nullptr) {
    return "<nullptr>";
  }
  return entry->input();
}

class TableTest : public ::testing::Test {
 protected:
  TableTest() { config::ConfigHandler::GetDefaultConfig(&config_); }

  const testing::MockDataManager mock_data_manager_;
  config::Config config_;
};

TEST_F(TableTest, LookUp) {
  static const struct TestCase {
    absl::string_view input;
    const bool expected_result;
    absl::string_view expected_output;
    absl::string_view expected_pending;
  } test_cases[] = {
      {"a", true, "あ", ""},  {"k", false, "", ""},   {"ka", true, "か", ""},
      {"ki", true, "き", ""}, {"ku", true, "く", ""}, {"kk", true, "っ", "k"},
      {"aka", false, "", ""}, {"na", true, "な", ""}, {"n", true, "ん", ""},
      {"nn", true, "ん", ""},
  };

  Table table;
  InitTable(&table);

  for (const TestCase& test : test_cases) {
    std::string output;
    std::string pending;
    const Entry* entry;
    entry = table.LookUp(test.input);

    EXPECT_EQ((entry != nullptr), test.expected_result);
    if (entry == nullptr) {
      continue;
    }
    EXPECT_EQ(entry->result(), test.expected_output);
    EXPECT_EQ(entry->pending(), test.expected_pending);
  }
}

TEST_F(TableTest, LookUpPredictiveAll) {
  Table table;
  InitTable(&table);

  std::vector<const Entry*> results;
  table.LookUpPredictiveAll("k", &results);

  EXPECT_EQ(results.size(), 6);
}

TEST_F(TableTest, Punctuations) {
  constexpr struct TestCase {
    config::Config::PunctuationMethod method;
    absl::string_view input;
    absl::string_view expected;
  } test_cases[] = {
      {config::Config::TOUTEN_KUTEN, ",", "、"},
      {config::Config::TOUTEN_KUTEN, ".", "。"},
      {config::Config::COMMA_PERIOD, ",", "，"},
      {config::Config::COMMA_PERIOD, ".", "．"},
      {config::Config::TOUTEN_PERIOD, ",", "、"},
      {config::Config::TOUTEN_PERIOD, ".", "．"},
      {config::Config::COMMA_KUTEN, ",", "，"},
      {config::Config::COMMA_KUTEN, ".", "。"},
  };

  commands::Request request;
  int index = 0;
  for (const TestCase& test_case : test_cases) {
    config::Config config;
    config.set_punctuation_method(test_case.method);
    Table table;
    ASSERT_TRUE(table.InitializeWithRequestAndConfig(request, config));

    const Entry* entry = table.LookUp(test_case.input);
    EXPECT_NE(entry, nullptr) << "Failed index = " << index;
    if (entry) {
      EXPECT_EQ(entry->result(), test_case.expected);
    }
    index++;
  }
}

TEST_F(TableTest, Symbols) {
  constexpr struct TestCase {
    config::Config::SymbolMethod method;
    absl::string_view input;
    absl::string_view expected;
  } test_cases[] = {
      {config::Config::CORNER_BRACKET_MIDDLE_DOT, "[", "「"},
      {config::Config::CORNER_BRACKET_MIDDLE_DOT, "]", "」"},
      {config::Config::CORNER_BRACKET_MIDDLE_DOT, "/", "・"},
      {config::Config::SQUARE_BRACKET_SLASH, "[", "["},
      {config::Config::SQUARE_BRACKET_SLASH, "]", "]"},
      {config::Config::SQUARE_BRACKET_SLASH, "/", "／"},
      {config::Config::CORNER_BRACKET_SLASH, "[", "「"},
      {config::Config::CORNER_BRACKET_SLASH, "]", "」"},
      {config::Config::CORNER_BRACKET_SLASH, "/", "／"},
      {config::Config::SQUARE_BRACKET_MIDDLE_DOT, "[", "["},
      {config::Config::SQUARE_BRACKET_MIDDLE_DOT, "]", "]"},
      {config::Config::SQUARE_BRACKET_MIDDLE_DOT, "/", "・"},
  };

  commands::Request request;
  int index = 0;
  for (const TestCase& test_case : test_cases) {
    config::Config config;
    config.set_symbol_method(test_case.method);
    Table table;
    ASSERT_TRUE(table.InitializeWithRequestAndConfig(request, config));

    const Entry* entry = table.LookUp(test_case.input);
    EXPECT_NE(entry, nullptr) << "Failed index = " << index;
    if (entry) {
      EXPECT_EQ(entry->result(), test_case.expected);
    }
    index++;
  }
}

TEST_F(TableTest, KanaSuppressed) {
  config_.set_preedit_method(config::Config::KANA);

  commands::Request request;

  Table table;
  ASSERT_TRUE(table.InitializeWithRequestAndConfig(request, config_));

  const Entry* entry = table.LookUp("a");
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->result(), "あ");
  EXPECT_TRUE(entry->pending().empty());
}

TEST_F(TableTest, KanaCombination) {
  Table table;
  commands::Request request;
  ASSERT_TRUE(table.InitializeWithRequestAndConfig(request, config_));

  const Entry* entry = table.LookUp("か゛");
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->result(), "が");
  EXPECT_TRUE(entry->pending().empty());
}

TEST_F(TableTest, InvalidEntryTest) {
  {
    Table table;
    EXPECT_FALSE(table.IsLoopingEntry("a", "b"));
    table.AddRule("a", "aa", "b");

    EXPECT_TRUE(table.IsLoopingEntry("b", "a"));
    table.AddRule("b", "aa", "a");  // looping

    EXPECT_NE(table.LookUp("a"), nullptr);
    EXPECT_EQ(table.LookUp("b"), nullptr);
  }

  {
    Table table;
    EXPECT_FALSE(table.IsLoopingEntry("a", "ba"));
    table.AddRule("a", "aa", "ba");

    EXPECT_TRUE(table.IsLoopingEntry("b", "a"));
    table.AddRule("b", "aa", "a");  // looping

    EXPECT_NE(table.LookUp("a"), nullptr);
    EXPECT_EQ(table.LookUp("b"), nullptr);
  }

  {
    Table table;
    EXPECT_FALSE(table.IsLoopingEntry("a", "b"));
    table.AddRule("a", "aa", "b");

    EXPECT_FALSE(table.IsLoopingEntry("b", "c"));
    table.AddRule("b", "aa", "c");

    EXPECT_FALSE(table.IsLoopingEntry("c", "d"));
    table.AddRule("c", "aa", "d");

    EXPECT_TRUE(table.IsLoopingEntry("d", "a"));
    table.AddRule("d", "aa", "a");  // looping

    EXPECT_NE(table.LookUp("a"), nullptr);
    EXPECT_NE(table.LookUp("b"), nullptr);
    EXPECT_NE(table.LookUp("c"), nullptr);
    EXPECT_EQ(table.LookUp("d"), nullptr);
  }

  {
    Table table;
    table.AddRule("wa", "WA", "");
    table.AddRule("ww", "X", "w");

    EXPECT_FALSE(table.IsLoopingEntry("www", "ww"));
    table.AddRule("www", "W", "ww");  // not looping

    EXPECT_NE(table.LookUp("wa"), nullptr);
    EXPECT_NE(table.LookUp("ww"), nullptr);
    EXPECT_NE(table.LookUp("www"), nullptr);
  }

  {
    Table table;
    table.AddRule("wa", "WA", "");
    table.AddRule("www", "W", "ww");

    EXPECT_FALSE(table.IsLoopingEntry("ww", "w"));
    table.AddRule("ww", "X", "w");

    EXPECT_NE(table.LookUp("wa"), nullptr);
    EXPECT_NE(table.LookUp("ww"), nullptr);
    EXPECT_NE(table.LookUp("www"), nullptr);
  }

  {
    Table table;
    EXPECT_TRUE(table.IsLoopingEntry("a", "a"));
    table.AddRule("a", "aa", "a");  // looping

    EXPECT_EQ(table.LookUp("a"), nullptr);
  }

  // Too long input
  {
    Table table;
    std::string too_long;
    // Maximum size is 300 now.
    for (int i = 0; i < 1024; ++i) {
      too_long += 'a';
    }
    table.AddRule(too_long, "test", "test");
    EXPECT_EQ(table.LookUp(too_long), nullptr);

    table.AddRule("a", too_long, "test");
    EXPECT_EQ(table.LookUp("a"), nullptr);

    table.AddRule("a", "test", too_long);
    EXPECT_EQ(table.LookUp("a"), nullptr);
  }

  // reasonably long
  {
    Table table;
    std::string reasonably_long;
    // Maximum size is 300 now.
    for (int i = 0; i < 200; ++i) {
      reasonably_long += 'a';
    }
    table.AddRule(reasonably_long, "test", "test");
    EXPECT_NE(table.LookUp(reasonably_long), nullptr);

    table.AddRule("a", reasonably_long, "test");
    EXPECT_NE(table.LookUp("a"), nullptr);

    table.AddRule("a", "test", reasonably_long);
    EXPECT_NE(table.LookUp("a"), nullptr);
  }
}

TEST_F(TableTest, CustomPunctuationsAndSymbols) {
  // Test against Issue2465801.
  std::string custom_roman_table;
  custom_roman_table.append("mozc\tMOZC\n");
  custom_roman_table.append(",\tCOMMA\n");
  custom_roman_table.append(".\tPERIOD\n");
  custom_roman_table.append("/\tSLASH\n");
  custom_roman_table.append("[\tOPEN\n");
  custom_roman_table.append("]\tCLOSE\n");

  config_.set_custom_roman_table(custom_roman_table);

  Table table;
  commands::Request request;
  table.InitializeWithRequestAndConfig(request, config_);

  const Entry* entry = nullptr;
  entry = table.LookUp("mozc");
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->result(), "MOZC");

  entry = table.LookUp(",");
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->result(), "COMMA");

  entry = table.LookUp(".");
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->result(), "PERIOD");

  entry = table.LookUp("/");
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->result(), "SLASH");

  entry = table.LookUp("[");
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->result(), "OPEN");

  entry = table.LookUp("]");
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->result(), "CLOSE");
}

TEST_F(TableTest, CaseSensitive) {
  Table table;
  table.AddRule("a", "[a]", "");
  table.AddRule("A", "[A]", "");
  table.AddRule("ba", "[ba]", "");
  table.AddRule("BA", "[BA]", "");
  table.AddRule("Ba", "[Ba]", "");
  // The rule of "bA" is intentionally dropped.
  // table.AddRule("bA",  "[bA]", "");
  table.AddRule("za", "[za]", "");

  // case insensitive
  table.set_case_sensitive(false);
  EXPECT_EQ(GetResult(table, "a"), "[a]");
  EXPECT_EQ(GetResult(table, "A"), "[a]");
  EXPECT_EQ(GetResult(table, "ba"), "[ba]");
  EXPECT_EQ(GetResult(table, "BA"), "[ba]");
  EXPECT_EQ(GetResult(table, "Ba"), "[ba]");
  EXPECT_EQ(GetResult(table, "bA"), "[ba]");

  EXPECT_EQ(GetInput(table, "a"), "a");
  EXPECT_EQ(GetInput(table, "A"), "a");
  EXPECT_EQ(GetInput(table, "ba"), "ba");
  EXPECT_EQ(GetInput(table, "BA"), "ba");
  EXPECT_EQ(GetInput(table, "Ba"), "ba");
  EXPECT_EQ(GetInput(table, "bA"), "ba");

  // Test for HasSubRules
  EXPECT_TRUE(table.HasSubRules("Z"));

  {  // Test for LookUpPrefix
    const Entry* entry = nullptr;
    size_t key_length = 0;
    bool fixed = false;
    entry = table.LookUpPrefix("bA", &key_length, &fixed);
    EXPECT_NE(entry, nullptr);
    EXPECT_EQ(entry->result(), "[ba]");
    EXPECT_EQ(key_length, 2);
    EXPECT_TRUE(fixed);
  }

  // case sensitive
  table.set_case_sensitive(true);
  EXPECT_TRUE(table.case_sensitive());
  EXPECT_EQ(GetResult(table, "a"), "[a]");
  EXPECT_EQ(GetResult(table, "A"), "[A]");
  EXPECT_EQ(GetResult(table, "ba"), "[ba]");
  EXPECT_EQ(GetResult(table, "BA"), "[BA]");
  EXPECT_EQ(GetResult(table, "Ba"), "[Ba]");
  EXPECT_EQ(GetResult(table, "bA"), "<nullptr>");

  EXPECT_EQ(GetInput(table, "a"), "a");
  EXPECT_EQ(GetInput(table, "A"), "A");
  EXPECT_EQ(GetInput(table, "ba"), "ba");
  EXPECT_EQ(GetInput(table, "BA"), "BA");
  EXPECT_EQ(GetInput(table, "Ba"), "Ba");
  EXPECT_EQ(GetInput(table, "bA"), "<nullptr>");

  // Test for HasSubRules
  EXPECT_FALSE(table.HasSubRules("Z"));

  {  // Test for LookUpPrefix
    const Entry* entry = nullptr;
    size_t key_length = 0;
    bool fixed = false;
    entry = table.LookUpPrefix("bA", &key_length, &fixed);
    EXPECT_EQ(entry, nullptr);
    EXPECT_EQ(key_length, 1);
    EXPECT_TRUE(fixed);
  }
}

TEST_F(TableTest, CaseSensitivity) {
  commands::Request request;
  {
    Table table;
    table.InitializeWithRequestAndConfig(request, config_);
    EXPECT_FALSE(table.case_sensitive());
  }
  {
    Table table;
    table.InitializeWithRequestAndConfig(request, config_);
    table.AddRule("", "", "");
    EXPECT_FALSE(table.case_sensitive());
  }
  {
    Table table;
    table.InitializeWithRequestAndConfig(request, config_);
    table.AddRule("a", "", "");
    EXPECT_FALSE(table.case_sensitive());
  }
  {
    Table table;
    table.InitializeWithRequestAndConfig(request, config_);
    table.AddRule("A", "", "");
    EXPECT_TRUE(table.case_sensitive());
  }
  {
    Table table;
    table.InitializeWithRequestAndConfig(request, config_);
    table.AddRule("a{A}a", "", "");
    EXPECT_FALSE(table.case_sensitive());
  }
  {
    Table table;
    table.InitializeWithRequestAndConfig(request, config_);
    table.AddRule("A{A}A", "", "");
    EXPECT_TRUE(table.case_sensitive());
  }
}

// This test case was needed because the case sensitivity was configured
// by the configuration.
// Currently the case sensitivity is independent from the configuration.
TEST_F(TableTest, CaseSensitiveByConfiguration) {
  commands::Request request;
  Table table;

  // config::Config::OFF
  {
    config_.set_shift_key_mode_switch(config::Config::OFF);
    table.InitializeWithRequestAndConfig(request, config_);

    table.AddRule("a", "[a]", "");
    table.AddRule("A", "[A]", "");
    table.AddRule("ba", "[ba]", "");
    table.AddRule("BA", "[BA]", "");
    table.AddRule("Ba", "[Ba]", "");

    EXPECT_TRUE(table.case_sensitive());
    EXPECT_EQ(GetResult(table, "a"), "[a]");
    EXPECT_EQ(GetResult(table, "A"), "[A]");
    EXPECT_EQ(GetResult(table, "ba"), "[ba]");
    EXPECT_EQ(GetResult(table, "BA"), "[BA]");
    EXPECT_EQ(GetResult(table, "Ba"), "[Ba]");
    EXPECT_EQ(GetResult(table, "bA"), "<nullptr>");

    EXPECT_EQ(GetInput(table, "a"), "a");
    EXPECT_EQ(GetInput(table, "A"), "A");
    EXPECT_EQ(GetInput(table, "ba"), "ba");
    EXPECT_EQ(GetInput(table, "BA"), "BA");
    EXPECT_EQ(GetInput(table, "Ba"), "Ba");
    EXPECT_EQ(GetInput(table, "bA"), "<nullptr>");

    // Test for HasSubRules
    EXPECT_FALSE(table.HasSubRules("Z"));

    {  // Test for LookUpPrefix
      const Entry* entry = nullptr;
      size_t key_length = 0;
      bool fixed = false;
      entry = table.LookUpPrefix("bA", &key_length, &fixed);
      EXPECT_EQ(entry, nullptr);
      EXPECT_EQ(key_length, 1);
      EXPECT_TRUE(fixed);
    }
  }

  // config::Config::ASCII_INPUT_MODE
  {
    config_.set_shift_key_mode_switch(config::Config::ASCII_INPUT_MODE);
    table.InitializeWithRequestAndConfig(request, config_);

    table.AddRule("a", "[a]", "");
    table.AddRule("A", "[A]", "");
    table.AddRule("ba", "[ba]", "");
    table.AddRule("BA", "[BA]", "");
    table.AddRule("Ba", "[Ba]", "");

    EXPECT_TRUE(table.case_sensitive());
    EXPECT_EQ(GetResult(table, "a"), "[a]");
    EXPECT_EQ(GetResult(table, "A"), "[A]");
    EXPECT_EQ(GetResult(table, "ba"), "[ba]");
    EXPECT_EQ(GetResult(table, "BA"), "[BA]");
    EXPECT_EQ(GetResult(table, "Ba"), "[Ba]");
    EXPECT_EQ(GetResult(table, "bA"), "<nullptr>");

    EXPECT_EQ(GetInput(table, "a"), "a");
    EXPECT_EQ(GetInput(table, "A"), "A");
    EXPECT_EQ(GetInput(table, "ba"), "ba");
    EXPECT_EQ(GetInput(table, "BA"), "BA");
    EXPECT_EQ(GetInput(table, "Ba"), "Ba");
    EXPECT_EQ(GetInput(table, "bA"), "<nullptr>");

    // Test for HasSubRules
    EXPECT_FALSE(table.HasSubRules("Z"));

    {  // Test for LookUpPrefix
      const Entry* entry = nullptr;
      size_t key_length = 0;
      bool fixed = false;
      entry = table.LookUpPrefix("bA", &key_length, &fixed);
      EXPECT_EQ(entry, nullptr);
      EXPECT_EQ(key_length, 1);
      EXPECT_TRUE(fixed);
    }
  }

  // config::Config::KATAKANA_INPUT_MODE
  {
    config_.set_shift_key_mode_switch(config::Config::KATAKANA_INPUT_MODE);
    table.InitializeWithRequestAndConfig(request, config_);

    table.AddRule("a", "[a]", "");
    table.AddRule("A", "[A]", "");
    table.AddRule("ba", "[ba]", "");
    table.AddRule("BA", "[BA]", "");
    table.AddRule("Ba", "[Ba]", "");

    EXPECT_TRUE(table.case_sensitive());
    EXPECT_EQ(GetResult(table, "a"), "[a]");
    EXPECT_EQ(GetResult(table, "A"), "[A]");
    EXPECT_EQ(GetResult(table, "ba"), "[ba]");
    EXPECT_EQ(GetResult(table, "BA"), "[BA]");
    EXPECT_EQ(GetResult(table, "Ba"), "[Ba]");
    EXPECT_EQ(GetResult(table, "bA"), "<nullptr>");

    EXPECT_EQ(GetInput(table, "a"), "a");
    EXPECT_EQ(GetInput(table, "A"), "A");
    EXPECT_EQ(GetInput(table, "ba"), "ba");
    EXPECT_EQ(GetInput(table, "BA"), "BA");
    EXPECT_EQ(GetInput(table, "Ba"), "Ba");
    EXPECT_EQ(GetInput(table, "bA"), "<nullptr>");

    // Test for HasSubRules
    EXPECT_FALSE(table.HasSubRules("Z"));

    {  // Test for LookUpPrefix
      const Entry* entry = nullptr;
      size_t key_length = 0;
      bool fixed = false;
      entry = table.LookUpPrefix("bA", &key_length, &fixed);
      EXPECT_EQ(entry, nullptr);
      EXPECT_EQ(key_length, 1);
      EXPECT_TRUE(fixed);
    }
  }
}

// Table class automatically enables case-sensitive mode when the given roman
// table has any input rule which contains one or more upper case characters.
//   e.g. "V" -> "5" or "YT" -> "You there"
// This feature was implemented as b/2910223 as per following request.
// http://www.google.com/support/forum/p/ime/thread?tid=4ea9aed4ac8a2ba6&hl=ja
//
// The following test checks if a case-sensitive and a case-insensitive roman
// table enables and disables this "case-sensitive mode", respectively.
TEST_F(TableTest, AutomaticCaseSensitiveDetection) {
  constexpr absl::string_view kCaseInsensitiveRomanTable = {
      "m\tmozc\n"    // m -> mozc
      "n\tnamazu\n"  // n -> namazu
  };
  constexpr absl::string_view kCaseSensitiveRomanTable = {
      "m\tmozc\n"  // m -> mozc
      "M\tMozc\n"  // M -> Mozc
  };

  commands::Request request;

  {
    Table table;
    config::Config config(config::ConfigHandler::DefaultConfig());
    config.set_custom_roman_table(kCaseSensitiveRomanTable);
    EXPECT_FALSE(table.case_sensitive())
        << "case-sensitive mode should be desabled by default.";
    // Load a custom config with case-sensitive custom roman table.
    ASSERT_TRUE(table.InitializeWithRequestAndConfig(request, config));

    EXPECT_TRUE(table.case_sensitive())
        << "Case sensitive roman table should enable case-sensitive mode.";
    // Explicitly disable case-sensitive mode.
    table.set_case_sensitive(false);
    ASSERT_FALSE(table.case_sensitive());
  }

  {
    Table table;
    // Load a custom config with case-insensitive custom roman table.
    config::Config config(config::ConfigHandler::DefaultConfig());
    config.set_custom_roman_table(kCaseInsensitiveRomanTable);
    ASSERT_TRUE(table.InitializeWithRequestAndConfig(request, config));

    EXPECT_FALSE(table.case_sensitive())
        << "Case insensitive roman table should disable case-sensitive mode.";
    // Explicitly enable case-sensitive mode.
    table.set_case_sensitive(true);
    ASSERT_TRUE(table.case_sensitive());
  }
}

TEST_F(TableTest, MobileMode) {
  mozc::commands::Request request;
  request.set_zero_query_suggestion(true);
  request.set_mixed_conversion(true);

  {
    // To 12keys -> Hiragana mode
    request.set_special_romanji_table(
        mozc::commands::Request::TWELVE_KEYS_TO_HIRAGANA);
    mozc::composer::Table table;
    table.InitializeWithRequestAndConfig(request, config_);
    {
      const mozc::composer::Entry* entry = nullptr;
      size_t key_length = 0;
      bool fixed = false;
      entry = table.LookUpPrefix("2", &key_length, &fixed);
      EXPECT_EQ(entry->input(), "2");
      EXPECT_EQ(entry->result(), "");
      EXPECT_EQ(entry->pending(), "か");
      EXPECT_EQ(key_length, 1);
      EXPECT_TRUE(fixed);
    }
    {
      const mozc::composer::Entry* entry = nullptr;
      size_t key_length = 0;
      bool fixed = false;
      entry = table.LookUpPrefix("し*", &key_length, &fixed);
      EXPECT_EQ(entry->input(), "し*");
      EXPECT_EQ(entry->result(), "");
      // U+F001 is a Unicode PUA character converted from "{*}".
      // This codepoint may be changed when the table data is updated.
      EXPECT_EQ(entry->pending(), "\uF001じ");
      EXPECT_EQ(key_length, 4);
      EXPECT_TRUE(fixed);
    }
  }

  {
    // To 12keys -> Halfwidth Ascii mode
    request.set_special_romanji_table(
        mozc::commands::Request::TWELVE_KEYS_TO_HALFWIDTHASCII);
    Table table;
    table.InitializeWithRequestAndConfig(request, config_);
    const mozc::composer::Entry* entry = nullptr;
    size_t key_length = 0;
    bool fixed = false;
    entry = table.LookUpPrefix("2", &key_length, &fixed);
    // U+F000 is a Unicode PUA character converted from "{?}".
    // This codepoint may be changed when the table data is updated.
    EXPECT_EQ(entry->pending(), "\uF000a");
  }

  {
    // To Godan -> Hiragana mode
    request.set_special_romanji_table(
        mozc::commands::Request::GODAN_TO_HIRAGANA);
    Table table;
    table.InitializeWithRequestAndConfig(request, config_);
    {
      const mozc::composer::Entry* entry = nullptr;
      size_t key_length = 0;
      bool fixed = false;
      entry = table.LookUpPrefix("しゃ*", &key_length, &fixed);
      EXPECT_EQ(entry->pending(), "じゃ");
    }
  }

  {
    // To Flick -> Hiragana mode.
    request.set_special_romanji_table(
        mozc::commands::Request::FLICK_TO_HIRAGANA);
    Table table;
    table.InitializeWithRequestAndConfig(request, config_);

    size_t key_length = 0;
    bool fixed = false;
    const mozc::composer::Entry* entry =
        table.LookUpPrefix("a", &key_length, &fixed);
    EXPECT_EQ(entry->pending(), "き");
  }

  {
    // To Notouch -> Hiragana mode.
    request.set_special_romanji_table(
        mozc::commands::Request::NOTOUCH_TO_HIRAGANA);
    Table table;
    table.InitializeWithRequestAndConfig(request, config_);

    size_t key_length = 0;
    bool fixed = false;
    const mozc::composer::Entry* entry =
        table.LookUpPrefix("a", &key_length, &fixed);
    EXPECT_EQ(entry->pending(), "き");
  }
}

TEST_F(TableTest, OrderOfAddRule) {
  // The order of AddRule should not be sensitive.
  {
    Table table;
    table.AddRule("www", "w", "ww");
    table.AddRule("ww", "[X]", "w");
    table.AddRule("we", "[WE]", "");
    EXPECT_TRUE(table.HasSubRules("ww"));

    const Entry* entry;
    entry = table.LookUp("ww");
    EXPECT_NE(entry, nullptr);

    size_t key_length;
    bool fixed;
    entry = table.LookUpPrefix("ww", &key_length, &fixed);
    EXPECT_NE(entry, nullptr);
    EXPECT_EQ(key_length, 2);
    EXPECT_FALSE(fixed);
  }
  {
    Table table;
    table.AddRule("ww", "[X]", "w");
    table.AddRule("we", "[WE]", "");
    table.AddRule("www", "w", "ww");
    EXPECT_TRUE(table.HasSubRules("ww"));

    const Entry* entry = nullptr;
    entry = table.LookUp("ww");
    EXPECT_NE(entry, nullptr);

    size_t key_length = 0;
    bool fixed = false;
    entry = table.LookUpPrefix("ww", &key_length, &fixed);
    EXPECT_NE(entry, nullptr);
    EXPECT_EQ(key_length, 2);
    EXPECT_FALSE(fixed);
  }
}

TEST_F(TableTest, AddRuleWithAttributes) {
  constexpr absl::string_view kInput = "1";
  Table table;
  table.AddRuleWithAttributes(kInput, "", "a", NEW_CHUNK);

  EXPECT_TRUE(table.HasNewChunkEntry(kInput));

  size_t key_length = 0;
  bool fixed = false;
  const Entry* entry = table.LookUpPrefix(kInput, &key_length, &fixed);
  EXPECT_EQ(key_length, 1);
  EXPECT_TRUE(fixed);
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->input(), kInput);
  EXPECT_EQ(entry->result(), "");
  EXPECT_EQ(entry->pending(), "a");
  EXPECT_EQ(entry->attributes(), NEW_CHUNK);

  constexpr absl::string_view kInput2 = "22";
  table.AddRuleWithAttributes(kInput2, "", "b", NEW_CHUNK | NO_TRANSLITERATION);

  EXPECT_TRUE(table.HasNewChunkEntry(kInput2));

  key_length = 0;
  fixed = false;
  entry = table.LookUpPrefix(kInput2, &key_length, &fixed);
  EXPECT_EQ(key_length, 2);
  EXPECT_TRUE(fixed);
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->input(), kInput2);
  EXPECT_EQ(entry->result(), "");
  EXPECT_EQ(entry->pending(), "b");
  EXPECT_EQ(entry->attributes(), (NEW_CHUNK | NO_TRANSLITERATION));
}

TEST_F(TableTest, LoadFromString) {
  constexpr absl::string_view kRule =
      "# This is a comment\n"
      "\n"                      // Empty line to be ignored.
      "a\t[A]\n"                // 2 entry rule
      "kk\t[X]\tk\n"            // 3 entry rule
      "ww\t[W]\tw\tNewChunk\n"  // 3 entry rule + attribute rule
      "xx\t[X]\tx\tNewChunk NoTransliteration\n"  // multiple attribute rules
      // all attributes
      "yy\t[Y]\ty\tNewChunk NoTransliteration DirectInput EndChunk\n"
      "#\t[#]\n";  // This line starts with '#' but should be a rule.
  Table table;
  table.LoadFromString(std::string(kRule));

  const Entry* entry = nullptr;
  // Test for "a\t[A]\n"  -- 2 entry rule
  EXPECT_FALSE(table.HasNewChunkEntry("a"));
  entry = table.LookUp("a");
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->result(), "[A]");
  EXPECT_EQ(entry->pending(), "");

  // Test for "kk\t[X]\tk\n"  -- 3 entry rule
  EXPECT_FALSE(table.HasNewChunkEntry("kk"));
  entry = table.LookUp("kk");
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->result(), "[X]");
  EXPECT_EQ(entry->pending(), "k");

  // Test for "ww\t[W]\tw\tNewChunk\n"  -- 3 entry rule + attribute rule
  EXPECT_TRUE(table.HasNewChunkEntry("ww"));
  entry = table.LookUp("ww");
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->result(), "[W]");
  EXPECT_EQ(entry->pending(), "w");
  EXPECT_EQ(entry->attributes(), NEW_CHUNK);

  // Test for "xx\t[X]\tx\tNewChunk NoTransliteration\n" -- multiple
  // attribute rules
  EXPECT_TRUE(table.HasNewChunkEntry("xx"));
  entry = table.LookUp("xx");
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->result(), "[X]");
  EXPECT_EQ(entry->pending(), "x");
  EXPECT_EQ(entry->attributes(), (NEW_CHUNK | NO_TRANSLITERATION));

  // Test for "yy\t[Y]\ty\tNewChunk NoTransliteration DirectInput EndChunk\n"
  // -- all attributes
  EXPECT_TRUE(table.HasNewChunkEntry("yy"));
  entry = table.LookUp("yy");
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->result(), "[Y]");
  EXPECT_EQ(entry->pending(), "y");
  EXPECT_EQ(entry->attributes(),
            NEW_CHUNK | NO_TRANSLITERATION | DIRECT_INPUT | END_CHUNK);

  // Test for "#\t[#]\n"  -- This line starts with '#' but should be a rule.
  entry = table.LookUp("#");
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->result(), "[#]");
  EXPECT_EQ(entry->pending(), "");
}

TEST_F(TableTest, SpecialKeys) {
  {
    Table table;
    table.AddRule("x{#1}y", "X1Y", "");
    table.AddRule("x{#2}y", "X2Y", "");
    table.AddRule("x{{}", "X{", "");
    table.AddRule("xy", "XY", "");

    const Entry* entry = nullptr;
    entry = table.LookUp("x{#1}y");
    EXPECT_TRUE(nullptr == entry);

    std::string key;
    key = table.ParseSpecialKey("x{#1}y");
    entry = table.LookUp(key);
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->input(), key);
    EXPECT_EQ(entry->result(), "X1Y");

    key = table.ParseSpecialKey("x{#2}y");
    entry = table.LookUp(key);
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->input(), key);
    EXPECT_EQ(entry->result(), "X2Y");

    key = "x{";
    entry = table.LookUp(key);
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->input(), key);
    EXPECT_EQ(entry->result(), "X{");
  }

  {
    // "{{}" is replaced with "{".
    // "{}" is replaced with U+F004.
    // {b} = U+F005, {d} = U+F006, {e} = U+F007, {{-} = U+F008.
    Table table;
    EXPECT_EQ(table.AddRule("{}", "", "")->input(), "\uF004");
    EXPECT_EQ(table.AddRule("{", "", "")->input(), "{");
    EXPECT_EQ(table.AddRule("}", "", "")->input(), "}");
    EXPECT_EQ(table.AddRule("{{}", "", "")->input(), "{");
    EXPECT_EQ(table.AddRule("{{}}", "", "")->input(), "{}");
    EXPECT_EQ(table.AddRule("a{", "", "")->input(), "a{");
    EXPECT_EQ(table.AddRule("{a", "", "")->input(), "{a");
    EXPECT_EQ(table.AddRule("a{a", "", "")->input(), "a{a");
    EXPECT_EQ(table.AddRule("a}", "", "")->input(), "a}");
    EXPECT_EQ(table.AddRule("}a", "", "")->input(), "}a");
    EXPECT_EQ(table.AddRule("a}a", "", "")->input(), "a}a");
    EXPECT_EQ(table.AddRule("a{b}c", "", "")->input(), "a\uF005c");
    EXPECT_EQ(table.AddRule("a{b}c{d}{e}", "", "")->input(),
              "a\uF005c\uF006\uF007");
    EXPECT_EQ(table.AddRule("}-{", "", "")->input(), "}-{");
    EXPECT_EQ(table.AddRule("a{bc", "", "")->input(), "a{bc");

    // This is not a fixed specification, but a current behavior.
    // "{{-}" is treated as a special key.
    EXPECT_EQ(table.AddRule("{{-}}", "", "")->input(), "\uF008}");
  }
}

TEST_F(TableTest, DeleteSpecialKey) {
  const Table table;
  EXPECT_EQ(DeleteSpecialKeys(table.ParseSpecialKey("{!}")), "");
  EXPECT_EQ(DeleteSpecialKeys(table.ParseSpecialKey("a{!}")), "a");
  EXPECT_EQ(DeleteSpecialKeys(table.ParseSpecialKey("{!}a")), "a");
  EXPECT_EQ(DeleteSpecialKeys(table.ParseSpecialKey("{abc}")), "");
  EXPECT_EQ(DeleteSpecialKeys(table.ParseSpecialKey("a{bcd}")), "a");
  EXPECT_EQ(DeleteSpecialKeys(table.ParseSpecialKey("{abc}d")), "d");
  EXPECT_EQ(DeleteSpecialKeys(table.ParseSpecialKey("{!}{abc}d")), "d");
  EXPECT_EQ(DeleteSpecialKeys(table.ParseSpecialKey("{!}a{bc}d")), "ad");
  EXPECT_EQ(DeleteSpecialKeys(table.ParseSpecialKey("{!}ab{cd}")), "ab");

  // Invalid patterns
  //   "\u000F" = parsed-"{"
  //   "\u000E" = parsed-"}"
  EXPECT_EQ(DeleteSpecialKeys("\u000Fab"), "\u000Fab");
  EXPECT_EQ(DeleteSpecialKeys("ab\u000E"), "ab\u000E");
  EXPECT_EQ(DeleteSpecialKeys("\u000F\u000Fab\u000E"), "");
  EXPECT_EQ(DeleteSpecialKeys("\u000Fab\u000E\u000E"), "\u000E");
}

TEST_F(TableTest, TableManager) {
  TableManager table_manager;
  absl::flat_hash_set<std::shared_ptr<const Table>> table_set;
  constexpr commands::Request::SpecialRomanjiTable special_romanji_table[] = {
      commands::Request::DEFAULT_TABLE,
      commands::Request::TWELVE_KEYS_TO_HIRAGANA,
      commands::Request::TWELVE_KEYS_TO_HALFWIDTHASCII,
      commands::Request::FLICK_TO_HIRAGANA,
      commands::Request::FLICK_TO_HALFWIDTHASCII,
      commands::Request::TOGGLE_FLICK_TO_HIRAGANA,
      commands::Request::TOGGLE_FLICK_TO_HALFWIDTHASCII,
      commands::Request::GODAN_TO_HIRAGANA,
      commands::Request::QWERTY_MOBILE_TO_HIRAGANA,
      commands::Request::QWERTY_MOBILE_TO_HALFWIDTHASCII,
      commands::Request::NOTOUCH_TO_HIRAGANA,
      commands::Request::NOTOUCH_TO_HALFWIDTHASCII,
  };
  constexpr config::Config::PreeditMethod preedit_method[] = {
      config::Config::ROMAN, config::Config::KANA};
  constexpr config::Config::PunctuationMethod punctuation_method[] = {
      config::Config::TOUTEN_KUTEN, config::Config::COMMA_PERIOD,
      config::Config::TOUTEN_PERIOD, config::Config::COMMA_KUTEN};
  constexpr config::Config::SymbolMethod symbol_method[] = {
      config::Config::CORNER_BRACKET_MIDDLE_DOT,
      config::Config::SQUARE_BRACKET_SLASH,
      config::Config::CORNER_BRACKET_SLASH,
      config::Config::SQUARE_BRACKET_MIDDLE_DOT};

  for (const auto& romanji : special_romanji_table) {
    for (const auto& preedit : preedit_method) {
      for (const auto& punctuation : punctuation_method) {
        for (const auto& symbol : symbol_method) {
          commands::Request request;
          request.set_special_romanji_table(romanji);
          config::Config config;
          config.set_preedit_method(preedit);
          config.set_punctuation_method(punctuation);
          config.set_symbol_method(symbol);
          std::shared_ptr<const Table> table =
              table_manager.GetTable(request, config);
          EXPECT_NE(table, nullptr);
          EXPECT_EQ(table_manager.GetTable(request, config), table);
          EXPECT_FALSE(table_set.contains(table));
          table_set.insert(table);
        }
      }
    }
  }

  {
    // b/6788850.
    constexpr absl::string_view kRule = "a\t[A]\n";  // 2 entry rule

    commands::Request request;
    request.set_special_romanji_table(Request::DEFAULT_TABLE);
    config::Config config;
    config.set_preedit_method(Config::ROMAN);
    config.set_punctuation_method(Config::TOUTEN_KUTEN);
    config.set_symbol_method(Config::CORNER_BRACKET_MIDDLE_DOT);
    config.set_custom_roman_table(kRule);
    std::shared_ptr<const Table> table =
        table_manager.GetTable(request, config);
    EXPECT_NE(table, nullptr);
    EXPECT_EQ(table_manager.GetTable(request, config), table);
    EXPECT_NE(table->LookUp("a"), nullptr);
    EXPECT_EQ(table->LookUp("kk"), nullptr);

    constexpr absl::string_view kRule2 =
        "a\t[A]\n"       // 2 entry rule
        "kk\t[X]\tk\n";  // 3 entry rule
    config.set_custom_roman_table(kRule2);
    std::shared_ptr<const Table> table2 =
        table_manager.GetTable(request, config);
    EXPECT_NE(table2, nullptr);
    EXPECT_EQ(table_manager.GetTable(request, config), table2);
    EXPECT_NE(table2->LookUp("a"), nullptr);
    EXPECT_NE(table2->LookUp("kk"), nullptr);
  }
}

}  // namespace
}  // namespace mozc::composer
