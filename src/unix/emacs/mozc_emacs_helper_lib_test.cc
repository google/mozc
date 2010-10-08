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

#include "unix/emacs/mozc_emacs_helper_lib.h"

#include <sstream>

#include "base/protobuf/message.h"
#include "session/commands.pb.h"
#include "testing/base/public/gunit.h"
#include "testing/base/public/googletest.h"


class MozcEmacsHelperLibTest : public testing::Test {
 protected:
  void ParseAndTestInputLine(
      const string &input_line, uint32 event_id, uint32 session_id,
      const string &protobuf) {
    uint32 actual_event_id = 0xDEADBEEFU;
    uint32 actual_session_id = 0xDEADBEEFU;
    mozc::commands::Input input;
    mozc::emacs::ParseInputLine(
        input_line, &actual_event_id, &actual_session_id, &input);
    EXPECT_EQ(event_id, actual_event_id);
    EXPECT_EQ(session_id, actual_session_id);
    EXPECT_EQ(protobuf, input.ShortDebugString());
  }

  void PrintAndTestSexpr(
      const mozc::protobuf::Message &message, const string &sexpr) {
    ostringstream sexpr_stream;
    mozc::emacs::PrintMessage(message, sexpr_stream);
    // Our string class is special one and not standard one, and
    // ostringstream.str() returns std::string (standard one).
    // Thus we need a type conversion from std::string to string.
    //
    // NOTE: From the view point of porting, we're using
    // ostream class in the standard.  And it requires ostringstream
    // in the standard.
    EXPECT_EQ(sexpr, string(sexpr_stream.str()));
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
  ParseAndTestInputLine("(2 DeleteSession 0)", 2, 0,
      "type: DELETE_SESSION");
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
  ParseAndTestInputLine(
      "(6 SendKey 4 97 shift ctrl meta alt super hyper)", 6, 4,
      "type: SEND_KEY "
      "key { key_code: 97 "
            "modifier_keys: SHIFT "
            "modifier_keys: CTRL "
            "modifier_keys: ALT "
            "modifier_keys: ALT "
            "modifier_keys: ALT "
            "modifier_keys: ALT "
          "}");
  // Special keys
  ParseAndTestInputLine("(7 SendKey 5 32)", 7, 5,
      "type: SEND_KEY "
      "key { key_code: 32 }");  // space as normal key
  ParseAndTestInputLine("(8 SendKey 6 space)", 8, 6,
      "type: SEND_KEY "
      "key { special_key: SPACE }");  // space as special key
  ParseAndTestInputLine("(9 SendKey 7 return shift ctrl meta alt)", 9, 7,
      "type: SEND_KEY "
      "key { special_key: ENTER "
            "modifier_keys: SHIFT "
            "modifier_keys: CTRL "
            "modifier_keys: ALT "
            "modifier_keys: ALT "
          "}");
}

TEST_F(MozcEmacsHelperLibTest, PrintMessage) {
  // KeyEvent
  mozc::commands::KeyEvent key_event;
  PrintAndTestSexpr(key_event, "()");
  key_event.set_special_key(mozc::commands::KeyEvent::PAGE_UP);
  PrintAndTestSexpr(key_event,
      "((special-key . page-up))");
  key_event.add_modifier_keys(mozc::commands::KeyEvent::KEY_DOWN);
  PrintAndTestSexpr(key_event,
      "((special-key . page-up)"
       "(modifier-keys key-down))");
  key_event.add_modifier_keys(mozc::commands::KeyEvent::SHIFT);
  PrintAndTestSexpr(key_event,
      "((special-key . page-up)"
       "(modifier-keys key-down shift))");

  // Result
  mozc::commands::Result result;
  result.set_type(mozc::commands::Result::STRING);
  result.set_value("RESULT_STRING");
  PrintAndTestSexpr(result,
      "((type . string)"
       "(value . \"RESULT_STRING\"))");

  // Preedit
  mozc::commands::Preedit preedit;
  preedit.set_cursor(1);
  mozc::commands::Preedit::Segment *segment;
  segment = preedit.add_segment();
  segment->set_annotation(mozc::commands::Preedit::Segment::UNDERLINE);
  segment->set_value("UNDER_LINE");
  segment = preedit.add_segment();
  segment->set_annotation(mozc::commands::Preedit::Segment::NONE);
  // "なし"
  segment->set_value("\xe3\x81\xaa\xe3\x81\x97");
  PrintAndTestSexpr(preedit,
    "((cursor . 1)"
     "(segment "
      "((annotation . underline)"
       "(value . \"UNDER_LINE\"))"
      "((annotation . none)"
       // "なし"
       "(value . \"\xe3\x81\xaa\xe3\x81\x97\"))))");

  // Output
  mozc::commands::Output output;
  output.set_consumed(false);
  PrintAndTestSexpr(output, "((consumed . nil))");  // bool false => nil
  output.set_consumed(true);
  PrintAndTestSexpr(output, "((consumed . t))");  // bool true => t
  // Complex big message
  output.set_id(1234);
  output.set_mode(mozc::commands::HIRAGANA);
  output.set_consumed(true);
  output.mutable_result()->CopyFrom(result);
  output.mutable_preedit()->CopyFrom(preedit);
  output.mutable_key()->CopyFrom(key_event);
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
                           // "なし"
                           "(value . \"\xe3\x81\xaa\xe3\x81\x97\")))))"
     "(key . ((special-key . page-up)"
             "(modifier-keys key-down shift))))");
}

TEST_F(MozcEmacsHelperLibTest, NormalizeSymbol) {
  using mozc::emacs::NormalizeSymbol;
  EXPECT_EQ("page-up", NormalizeSymbol("PAGE_UP"));
  EXPECT_EQ("page-down", NormalizeSymbol("PAGE_DOWN"));
  EXPECT_EQ("key-code", NormalizeSymbol("key_code"));
  EXPECT_EQ("modifiers", NormalizeSymbol("modifiers"));
  EXPECT_EQ("123", NormalizeSymbol("123"));
}

TEST_F(MozcEmacsHelperLibTest, QuoteString) {
  using mozc::emacs::QuoteString;
  EXPECT_EQ("\"\"", QuoteString(""));
  EXPECT_EQ("\"abc\"", QuoteString("abc"));
  EXPECT_EQ("\"\\\"abc\\\"\"", QuoteString("\"abc\""));
  EXPECT_EQ("\"\\\\\\\"\"", QuoteString("\\\""));
  EXPECT_EQ("\"\t\n\v\f\r \"", QuoteString("\t\n\v\f\r "));
}
