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

#include "unix/emacs/mozc_emacs_helper_lib.h"

#include <cstdint>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "base/protobuf/message.h"
#include "protocol/candidate_window.pb.h"
#include "protocol/commands.pb.h"
#include "testing/gmock.h"
#include "testing/gunit.h"
#include "testing/testing_util.h"

namespace mozc::emacs {
namespace {

using ::testing::ElementsAreArray;
using ::testing::IsEmpty;

class MozcEmacsHelperLibTest : public ::testing::Test {
 protected:
  void ParseAndTestInputLine(absl::string_view input_line, uint32_t event_id,
                             uint32_t session_id, absl::string_view protobuf) {
    uint32_t actual_event_id = 0xDEADBEEFU;
    uint32_t actual_session_id = 0xDEADBEEFU;
    commands::Input input;
    ParseInputLine(input_line, &actual_event_id, &actual_session_id, &input);
    EXPECT_EQ(actual_event_id, event_id);
    EXPECT_EQ(actual_session_id, session_id);
    EXPECT_PROTO_EQ(protobuf, input);
  }

  void PrintAndTestSexpr(const protobuf::Message &message,
                         absl::string_view sexpr) {
    std::vector<std::string> buffer;
    PrintMessage(message, &buffer);
    const std::string output = absl::StrJoin(buffer, "");
    EXPECT_EQ(output, sexpr);
  }

  void TestUnquoteString(absl::string_view expected, absl::string_view input) {
    std::string output;
    EXPECT_TRUE(UnquoteString(absl::StrCat("\"", input, "\""), &output));
    EXPECT_EQ(output, expected);
  }

  void ExpectUnquoteStringFails(absl::string_view input) {
    std::string output = "output string must become empty.";
    EXPECT_FALSE(UnquoteString(input, &output));
    EXPECT_THAT(output, IsEmpty());
  }
};

TEST_F(MozcEmacsHelperLibTest, ParseInputLine) {
  // CreateSession
  ParseAndTestInputLine("(0 CreateSession)", 0, 0xDEADBEEFU,
                        "type: CREATE_SESSION");
  // Spaces around parentheses
  ParseAndTestInputLine(" \t( 1\tCreateSession )\t", 1, 0xDEADBEEFU,
                        "type: CREATE_SESSION");

  // DeleteSession
  ParseAndTestInputLine("(2 DeleteSession 0)", 2, 0, "type: DELETE_SESSION");
  // Spaces around parentheses
  ParseAndTestInputLine(" \t( 3\tDeleteSession\t1 )\t", 3, 1,
                        "type: DELETE_SESSION");

  // SendKey
  ParseAndTestInputLine("(4 SendKey 2 97)", 4, 2,
                        "type: SEND_KEY "
                        "key { key_code: 97 }");
  // Modifier keys
  ParseAndTestInputLine("(5 SendKey 3 97 shift)", 5, 3,
                        "type: SEND_KEY "
                        "key { key_code: 97 "
                        "modifier_keys: SHIFT "
                        "}");
  // alt, meta, super, hyper keys will be converted to a single alt key.
  // modifier_keys are sorted by enum value defined in commands.proto.
  ParseAndTestInputLine("(6 SendKey 4 97 shift ctrl meta alt super hyper)", 6,
                        4,
                        "type: SEND_KEY "
                        "key { key_code: 97 "
                        "modifier_keys: CTRL "
                        "modifier_keys: ALT "
                        "modifier_keys: SHIFT "
                        "}");
  // Special keys
  ParseAndTestInputLine("(7 SendKey 5 32)", 7, 5,
                        "type: SEND_KEY "
                        "key { key_code: 32 }");  // space as normal key
  ParseAndTestInputLine("(8 SendKey 6 space)", 8, 6,
                        "type: SEND_KEY "
                        "key { special_key: SPACE }");  // space as special key
  // alt, meta keys will be converted to a single alt key.
  // modifier_keys are sorted by enum value defined in commands.proto.
  ParseAndTestInputLine("(9 SendKey 7 return shift ctrl meta alt)", 9, 7,
                        "type: SEND_KEY "
                        "key { special_key: ENTER "
                        "modifier_keys: CTRL "
                        "modifier_keys: ALT "
                        "modifier_keys: SHIFT "
                        "}");
  // Key and string literal
  ParseAndTestInputLine(
      "(10 SendKey 8 97 \"\343\201\241\")", 10, 8,
      "type: SEND_KEY "
      "key { key_code: 97 "
      // ShortDebugString() prints escape sequences in octal format.
      "key_string: \"\\343\\201\\241\" }");  // "ち"
  ParseAndTestInputLine("(11 SendKey 9 72 \"Hello, World!\")", 11, 9,
                        "type: SEND_KEY "
                        "key { key_code: 72 "
                        "key_string: \"Hello, World!\" }");
  ParseAndTestInputLine("(12 SendKey 10 72 \"\t\n\v\f\r \")", 12, 10,
                        "type: SEND_KEY "
                        "key { key_code: 72 "
                        "key_string: \"\\t\\n\\013\\014\\r \" }");
  ParseAndTestInputLine("(13 SendKey 11 72 \"\\a\\b\\t\\n\\s\\d\")", 13, 11,
                        "type: SEND_KEY "
                        "key { key_code: 72 "
                        "key_string: \"\\007\\010\\t\\n \\177\" }");
}

TEST_F(MozcEmacsHelperLibTest, PrintMessage) {
  // KeyEvent
  commands::KeyEvent key_event;
  PrintAndTestSexpr(key_event, "()");
  key_event.set_special_key(commands::KeyEvent::PAGE_UP);
  PrintAndTestSexpr(key_event, "((special-key . page-up))");
  key_event.add_modifier_keys(commands::KeyEvent::KEY_DOWN);
  PrintAndTestSexpr(key_event,
                    "((special-key . page-up)"
                    "(modifier-keys key-down))");
  key_event.add_modifier_keys(commands::KeyEvent::SHIFT);
  PrintAndTestSexpr(key_event,
                    "((special-key . page-up)"
                    "(modifier-keys key-down shift))");

  // Result
  commands::Result result;
  result.set_type(commands::Result::STRING);
  result.set_value("RESULT_STRING");
  PrintAndTestSexpr(result,
                    "((type . string)"
                    "(value . \"RESULT_STRING\"))");

  // Preedit
  commands::Preedit preedit;
  preedit.set_cursor(1);
  commands::Preedit::Segment *segment;
  segment = preedit.add_segment();
  segment->set_annotation(commands::Preedit::Segment::UNDERLINE);
  segment->set_value("UNDER_LINE");
  segment = preedit.add_segment();
  segment->set_annotation(commands::Preedit::Segment::NONE);
  segment->set_value("なし");
  PrintAndTestSexpr(preedit,
                    "((cursor . 1)"
                    "(segment "
                    "((annotation . underline)"
                    "(value . \"UNDER_LINE\"))"
                    "((annotation . none)"
                    "(value . \"なし\"))))");

  // Output
  commands::Output output;
  output.set_consumed(false);
  PrintAndTestSexpr(output, "((consumed . nil))");  // bool false => nil
  output.set_consumed(true);
  PrintAndTestSexpr(output, "((consumed . t))");  // bool true => t
  // Complex big message
  output.set_id(1234);
  output.set_mode(commands::HIRAGANA);
  output.set_consumed(true);
  *output.mutable_result() = result;
  *output.mutable_preedit() = preedit;
  *output.mutable_key() = key_event;
  PrintAndTestSexpr(output,
                    "((id . \"1234\")"
                    "(mode . hiragana)"
                    "(consumed . t)"
                    "(result . ((type . string)"
                    "(value . \"RESULT_STRING\")))"
                    "(preedit . ((cursor . 1)"
                    "(segment ((annotation . underline)"
                    "(value . \"UNDER_LINE\"))"
                    "((annotation . none)"
                    "(value . \"なし\")))))"
                    "(key . ((special-key . page-up)"
                    "(modifier-keys key-down shift))))");
}

TEST_F(MozcEmacsHelperLibTest, NormalizeSymbol) {
  EXPECT_EQ(NormalizeSymbol("PAGE_UP"), "page-up");
  EXPECT_EQ(NormalizeSymbol("PAGE_DOWN"), "page-down");
  EXPECT_EQ(NormalizeSymbol("key_code"), "key-code");
  EXPECT_EQ(NormalizeSymbol("modifiers"), "modifiers");
  EXPECT_EQ(NormalizeSymbol("123"), "123");
}

TEST_F(MozcEmacsHelperLibTest, QuoteString) {
  EXPECT_EQ(QuoteString(""), "\"\"");
  EXPECT_EQ(QuoteString("abc"), "\"abc\"");
  EXPECT_EQ(QuoteString("\"abc\""), "\"\\\"abc\\\"\"");
  EXPECT_EQ(QuoteString("\\\""), "\"\\\\\\\"\"");
  EXPECT_EQ(QuoteString("\t\n\v\f\r "), "\"\t\n\v\f\r \"");
}

TEST_F(MozcEmacsHelperLibTest, UnquoteString) {
  TestUnquoteString("", "");
  TestUnquoteString("abc", "abc");
  TestUnquoteString("\"abc\"", "\\\"abc\\\"");
  TestUnquoteString(" \n\\", "\\s\\n\\\\");
  TestUnquoteString("\t\n\v\f\r  ", "\\t\\n\\v\\f\\r \\ ");
  TestUnquoteString("\t\n\v\f\r", "\t\n\v\f\r");

  ExpectUnquoteStringFails("");  // no double quotes
  ExpectUnquoteStringFails("abc");
  ExpectUnquoteStringFails("\"");
  ExpectUnquoteStringFails("[\"\"]");
  ExpectUnquoteStringFails("\"\"\"");  // unquoted double quote
  ExpectUnquoteStringFails("\"\\\"");  // No character follows backslash.
}

TEST_F(MozcEmacsHelperLibTest, TokenizeSExpr) {
  constexpr absl::string_view kInput =
      " ('abc \" \t\\r\\\n\\\"\"\t-x0\"い\"p)\n";
  constexpr absl::string_view kGolden[] = {
      "(", "'", "abc", "\" \t\\r\\\n\\\"\"", "-x0", "\"い\"", "p", ")"};

  std::vector<std::string> output;
  EXPECT_TRUE(TokenizeSExpr(kInput, &output));
  EXPECT_THAT(output, ElementsAreArray(kGolden));

  // control character
  EXPECT_FALSE(TokenizeSExpr("\x7f", &output));
  // unclosed double quote
  EXPECT_FALSE(TokenizeSExpr("\"", &output));
}

TEST_F(MozcEmacsHelperLibTest, RemoveUsageDataTest) {
  {
    SCOPED_TRACE("If there are no candidates, do nothing.");
    commands::Output output;
    EXPECT_FALSE(RemoveUsageData(&output));
  }
  {
    SCOPED_TRACE("If there are no usage data, do nothing.");
    commands::Output output;
    output.mutable_candidate_window();
    EXPECT_FALSE(RemoveUsageData(&output));
    EXPECT_TRUE(output.has_candidate_window());
  }
  {
    SCOPED_TRACE("Removes usage data from output");
    commands::Output output;
    commands::CandidateWindow *candidate_window =
        output.mutable_candidate_window();
    candidate_window->mutable_usages();
    EXPECT_TRUE(RemoveUsageData(&output));
    EXPECT_TRUE(output.has_candidate_window());
    EXPECT_TRUE(output.candidate_window().has_usages());
  }
}

}  // namespace
}  // namespace mozc::emacs
