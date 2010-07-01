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
#include "base/scoped_ptr.h"
#include "session/commands.pb.h"
#include "testing/base/public/gunit.h"
#include "unix/scim/mozc_lookup_table.h"
#include "unix/scim/mozc_response_parser.h"
#include "unix/scim/mozc_connection.h"
#include "unix/scim/scim_mozc.h"

using mozc::commands::Annotation;
using mozc::commands::Candidates;
using mozc::commands::Output;
using mozc::commands::Preedit;
using mozc::commands::Result;

namespace {

// This class implements MozcConnectionInterface.
class DummyMozcConnection : public mozc_unix_scim::MozcConnectionInterface {
 public:
  DummyMozcConnection() {
  }

  virtual bool TryCreateSession() {
    return true;
  }

  virtual bool TryDeleteSession() {
    return true;
  }

  virtual bool TrySendKeyEvent(const scim::KeyEvent &key,
                               mozc::commands::CompositionMode composition_mode,
                               mozc::commands::Output *out,
                               string *out_error) const {
    return true;
  }

  virtual bool TrySendClick(int32 unique_id,
                            mozc::commands::Output *out,
                            string *out_error) const {
    return true;
  }

  virtual bool TrySendCompositionMode(
      mozc::commands::CompositionMode mode,
      mozc::commands::Output *out,
      string *out_error) const {
    return true;
  }

  virtual bool TrySendCommand(mozc::commands::SessionCommand::CommandType type,
                              mozc::commands::Output *out,
                              string *out_error) const {
    return true;
  }

  virtual bool CanSend(const scim::KeyEvent &key) const {
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyMozcConnection);
};

struct Cand {
  const char *shortcut;
  const char *prefix;
  const char *cand;
  const char *suffix;
  const char *description;
};

scim::WideString Widen(const string &str) {
  return scim::utf8_mbstowcs(str);
}

// Adds "result" protocol message to out.
void SetResult(Result::ResultType t, const string &value, Output *out) {
  Result *result = out->mutable_result();
  result->set_type(t);
  result->set_value(value);
}

// Adds "preedit" protocol message to out.
void SetPreedit(uint32 cursor,
                int highlighted_position,
                const char **str,
                const Preedit::Segment::Annotation *annotation,
                size_t num_str,
                Output *out) {
  Preedit *preedit = out->mutable_preedit();
  preedit->set_cursor(cursor);
  if (highlighted_position >= 0) {
    preedit->set_highlighted_position(highlighted_position);
  }
  for (int i = 0; i < num_str; ++i) {
    Preedit::Segment *seg = preedit->add_segment();
    seg->set_annotation(annotation[i]);
    seg->set_value(str[i]);
    seg->set_value_length(strlen(str[i]));
  }
}

// Adds "candidates" protocol message to out.
void SetCandidate(uint32 focused_index,
                  const Cand *candidates_str, size_t num_candidates,
                  uint32 position, Output *out) {
  Candidates *candidates = out->mutable_candidates();
  if (focused_index != -1) {
    candidates->set_focused_index(focused_index);
  }
  candidates->set_size(num_candidates);
  candidates->set_position(position);

  for (int i = 0; i < num_candidates; ++i) {
    Candidates::Candidate *c = candidates->add_candidate();
    c->set_index(i);
    c->set_value(candidates_str[i].cand);
    Annotation *annotation = c->mutable_annotation();
    if (strlen(candidates_str[i].shortcut) > 0) {
      annotation->set_shortcut(candidates_str[i].shortcut);
    }
    if (strlen(candidates_str[i].prefix) > 0) {
      annotation->set_prefix(candidates_str[i].prefix);
    }
    if (strlen(candidates_str[i].suffix) > 0) {
      annotation->set_suffix(candidates_str[i].suffix);
    }
    if (strlen(candidates_str[i].description) > 0) {
      annotation->set_description(candidates_str[i].description);
    }
  }
}

}  // namespace

namespace mozc_unix_scim {

// This class overrides all virtual functions in ScimMozc class.
class ScimMozcTest : public ScimMozc {
 public:
  explicit ScimMozcTest(MozcResponseParser *parser)
      : ScimMozc(NULL /* factory */,  "" /* encoding */, 0 /* id */,
                 NULL /* config */,
                 new DummyMozcConnection, parser) {
  }

  virtual ~ScimMozcTest() {
  }

  // Overrides the function in ScimMozc.
  virtual void SetResultString(const scim::WideString &result_string) {
    test_result_string_ = result_string;
  }

  // Overrides the function in ScimMozc.
  virtual void SetCandidateWindow(const MozcLookupTable *candidates) {
    test_candidates_.reset(candidates);
  }

  // Overrides the function in ScimMozc.
  virtual void SetPreeditInfo(const PreeditInfo *preedit_info) {
    test_preedit_info_.reset(preedit_info);
  }

  // Overrides the function in ScimMozc.
  virtual void SetAuxString(const scim::String &str) {
    test_aux_ = str;
  }

  // Overrides the function in ScimMozc.
  virtual void SetUrl(const string &url) {
    test_url_ = url;
  }

  void Clear() {
    test_result_string_.clear();
    test_candidates_.reset();
    test_preedit_info_.reset();
    test_aux_.clear();
    test_url_.clear();
  }

  const scim::WideString &test_result_string() const {
    return test_result_string_;
  }

  const MozcLookupTable *test_candidates() const {
    return test_candidates_.get();
  }

  const PreeditInfo *test_preedit_info() const {
    return test_preedit_info_.get();
  }

  const scim::String &test_aux() const {
    return test_aux_;
  }

  const scim::String &test_url() const {
    return test_url_;
  }


 private:
  scim::WideString test_result_string_;
  scoped_ptr<const MozcLookupTable> test_candidates_;
  scoped_ptr<const PreeditInfo> test_preedit_info_;
  scim::String test_aux_;
  string test_url_;

  DISALLOW_COPY_AND_ASSIGN(ScimMozcTest);
};

TEST(MozcResponseParserTest, TestParseResponse_NotConsumed) {
  MozcResponseParser *parser = new MozcResponseParser;
  ScimMozcTest ui(parser);

  Output out;
  out.set_id(1);
  out.set_consumed(false);

  EXPECT_FALSE(parser->ParseResponse(out, &ui));
  // Make sure all member variables are not modified.
  EXPECT_TRUE(ui.test_result_string().empty());
  EXPECT_TRUE(ui.test_candidates() == NULL);
  EXPECT_TRUE(ui.test_preedit_info() == NULL);
  EXPECT_TRUE(ui.test_aux().empty()) << ui.test_aux();
}

TEST(MozcResponseParserTest, TestParseResponse_StringResult) {
  MozcResponseParser *parser = new MozcResponseParser;
  ScimMozcTest ui(parser);

  const char kStr[] = "abc";
  const char kTestUrl[] = "http://go/mozc-gohenkan";

  Output out;
  out.set_id(1);
  out.set_consumed(true);
  out.set_url(kTestUrl);
  SetResult(Result::STRING, kStr, &out);

  EXPECT_TRUE(parser->ParseResponse(out, &ui));

  EXPECT_EQ(Widen(kStr), ui.test_result_string());
  EXPECT_TRUE(ui.test_candidates() == NULL);
  EXPECT_TRUE(ui.test_preedit_info() == NULL);
  EXPECT_TRUE(ui.test_aux().empty()) << ui.test_aux();
  EXPECT_EQ(kTestUrl, ui.test_url());
}

TEST(MozcResponseParserTest, TestParseResponse_NoneResult) {
  MozcResponseParser *parser = new MozcResponseParser;
  ScimMozcTest ui(parser);

  const char kStr[] = "abc";

  Output out;
  out.set_id(1);
  out.set_consumed(true);
  SetResult(Result::NONE, kStr, &out);

  EXPECT_TRUE(parser->ParseResponse(out, &ui));

  EXPECT_TRUE(ui.test_result_string().empty());
  EXPECT_TRUE(ui.test_candidates() == NULL);
  EXPECT_TRUE(ui.test_preedit_info() == NULL);
  EXPECT_FALSE(ui.test_aux().empty());
}

// Tests if the parser can parse a string which contains 3 segments, "abcd",
// "de". No Highlight.
TEST(MozcResponseParserTest, TestParseResponse_Preedit) {
  MozcResponseParser *parser = new MozcResponseParser;
  ScimMozcTest ui(parser);

  const Preedit::Segment::Annotation kAnnotate[] = {
    Preedit::Segment::UNDERLINE,
    Preedit::Segment::NONE,
  };
  const char *kStr[] = {
    "abcd",  // UNDERLINE
    "de",  // NONE
  };
  const uint32 kCursor = 4 + 2;  // right edge of the string.

  Output out;
  out.set_id(1);
  out.set_consumed(true);
  SetPreedit(kCursor, -1, kStr, kAnnotate, arraysize(kStr), &out);

  EXPECT_TRUE(parser->ParseResponse(out, &ui));

  EXPECT_TRUE(ui.test_result_string().empty());
  EXPECT_TRUE(ui.test_candidates() == NULL);
  ASSERT_FALSE(ui.test_preedit_info() == NULL);
  EXPECT_TRUE(ui.test_aux().empty()) << ui.test_aux();

  EXPECT_EQ(kCursor, ui.test_preedit_info()->cursor_pos);
  EXPECT_EQ(Widen(string(kStr[0]) + kStr[1]), ui.test_preedit_info()->str);
  ASSERT_EQ(arraysize(kStr), ui.test_preedit_info()->attribute_list.size());
  {
    const scim::Attribute &attr = (ui.test_preedit_info()->attribute_list)[0];
    EXPECT_EQ(scim::SCIM_ATTR_DECORATE, attr.get_type());
    EXPECT_EQ(Preedit::Segment::UNDERLINE, attr.get_value());
  }
  {
    const scim::Attribute &attr = (ui.test_preedit_info()->attribute_list)[1];
    EXPECT_EQ(scim::SCIM_ATTR_NONE, attr.get_type());
  }
}

// Tests if the parser can parse a string which contains 3 segments, "abcd",
// "de", and "fgh". "de" is HIGHLIGHTed.
TEST(MozcResponseParserTest, TestParseResponse_PreeditWithHighlight) {
  MozcResponseParser *parser = new MozcResponseParser;
  ScimMozcTest ui(parser);

  const Preedit::Segment::Annotation kAnnotate[] = {
    Preedit::Segment::UNDERLINE,
    Preedit::Segment::HIGHLIGHT,
    Preedit::Segment::NONE,
  };
  const char *kStr[] = {
    "abcd",  // UNDERLINE
    "de",  // HIGHLIGHT
    "fgh",  // NONE
  };
  const uint32 kPos1 = 4 + 2 + 3;  // right edge of the string.
  const uint32 kPos2 = 4;  // left edge of the highlighted segment.

  Output out;
  out.set_id(1);
  out.set_consumed(true);
  SetPreedit(kPos1, kPos2, kStr, kAnnotate, arraysize(kStr), &out);

  EXPECT_TRUE(parser->ParseResponse(out, &ui));

  EXPECT_TRUE(ui.test_result_string().empty());
  EXPECT_TRUE(ui.test_candidates() == NULL);
  ASSERT_FALSE(ui.test_preedit_info() == NULL);
  EXPECT_TRUE(ui.test_aux().empty()) << ui.test_aux();

  EXPECT_EQ(kPos2, ui.test_preedit_info()->cursor_pos);
  EXPECT_EQ(
      Widen(string(kStr[0]) + kStr[1] + kStr[2]), ui.test_preedit_info()->str);
  ASSERT_EQ(arraysize(kStr), ui.test_preedit_info()->attribute_list.size());
  {
    const scim::Attribute &attr = (ui.test_preedit_info()->attribute_list)[0];
    EXPECT_EQ(scim::SCIM_ATTR_DECORATE, attr.get_type());
    EXPECT_EQ(Preedit::Segment::UNDERLINE, attr.get_value());
  }
  {
    const scim::Attribute &attr = (ui.test_preedit_info()->attribute_list)[1];
    EXPECT_EQ(scim::SCIM_ATTR_DECORATE, attr.get_type());
    EXPECT_EQ(Preedit::Segment::HIGHLIGHT, attr.get_value());
  }
  {
    const scim::Attribute &attr = (ui.test_preedit_info()->attribute_list)[2];
    EXPECT_EQ(scim::SCIM_ATTR_NONE, attr.get_type());
  }
}

TEST(MozcResponseParserTest, TestParseResponse_CandidateWindow) {
  MozcResponseParser *parser = new MozcResponseParser;
  ScimMozcTest ui(parser);

  const uint32 kCursor = 6;
  const char *kStr[] = {
    "abcabc"
  };
  const Preedit::Segment::Annotation kAnnotate[] = {
    Preedit::Segment::HIGHLIGHT,
  };
  const unsigned int kScimAnnotate[] = {
    scim::SCIM_ATTR_DECORATE_HIGHLIGHT,
  };

  const Cand kCandidate[] = {
    { "1", "", "cand1", "", "" },
    { "2", "", "cand2", "", "D1" },
    { "3", "", "cand3", "S1", "" },
    { "4", "", "cand4", "S2", "D2" },
    { "5", "P1", "cand5", "", "" },
    { "6", "P2", "cand6", "", "D3" },
    { "7", "P3", "cand7", "S3", "" },
    { "", "P4", "cand8", "S4", "D4" },
  };
  const char *kAnnotatedStr[] = {
    "cand1",
    "cand2 [D1]",
    "cand3S1",
    "cand4S2 [D2]",
    "P1cand5",
    "P2cand6 [D3]",
    "P3cand7S3",
    "P4cand8S4 [D4]",
  };
  // The position of the window.
  const uint32 kPosition = 3;
  // Focus the 4th candidate.
  const uint32 kFocusedIndex = 4;

  Output out;
  out.set_id(1);
  out.set_consumed(true);
  SetPreedit(kCursor, -1, kStr, kAnnotate, arraysize(kStr), &out);
  SetCandidate(
      kFocusedIndex, kCandidate, arraysize(kCandidate), kPosition, &out);

  parser->set_use_annotation(true);
  EXPECT_TRUE(parser->ParseResponse(out, &ui));

  EXPECT_TRUE(ui.test_result_string().empty());
  ASSERT_FALSE(ui.test_candidates() == NULL);
  ASSERT_FALSE(ui.test_preedit_info() == NULL);
  EXPECT_FALSE(ui.test_aux().empty());

  // Check preedit info.

  EXPECT_EQ(kCursor, ui.test_preedit_info()->cursor_pos);
  EXPECT_EQ(Widen(kStr[0]), ui.test_preedit_info()->str);
  ASSERT_EQ(1, ui.test_preedit_info()->attribute_list.size());
  const scim::Attribute &attr = (ui.test_preedit_info()->attribute_list)[0];
  EXPECT_EQ(scim::SCIM_ATTR_DECORATE, attr.get_type());
  EXPECT_EQ(kScimAnnotate[0], attr.get_value());

  // Check candidate window.
  const scim::LookupTable *table = ui.test_candidates();
  ASSERT_TRUE(table->is_cursor_visible());
  EXPECT_EQ(kFocusedIndex, table->get_cursor_pos());
  for (int i = 0; i < arraysize(kAnnotatedStr); ++i) {
    const scim::WideString &label = table->get_candidate_label(i);
    string orig_label = kCandidate[i].shortcut;
    EXPECT_EQ(Widen(orig_label), label);
    EXPECT_EQ(Widen(kAnnotatedStr[i]), table->get_candidate(i));
  }
}

TEST(MozcResponseParserTest, TestParseResponse_CandidateWindowNoAnnotation) {
  MozcResponseParser *parser = new MozcResponseParser;
  ScimMozcTest ui(parser);

  const uint32 kCursor = 6;
  const char *kStr[] = {
    "abcabc"
  };
  const Preedit::Segment::Annotation kAnnotate[] = {
    Preedit::Segment::HIGHLIGHT,
  };
  const unsigned int kScimAnnotate[] = {
    scim::SCIM_ATTR_DECORATE_HIGHLIGHT,
  };

  const Cand kCandidate[] = {
    { "1", "", "cand1", "", "" },
    { "2", "", "cand2", "", "D1" },
    { "3", "", "cand3", "S1", "" },
    { "4", "", "cand4", "S2", "D2" },
    { "5", "P1", "cand5", "", "" },
    { "6", "P2", "cand6", "", "D3" },
    { "7", "P3", "cand7", "S3", "" },
    { "", "P4", "cand8", "S4", "D4" },
  };
  // The position of the window.
  const uint32 kPosition = 3;
  // Focus the 4th candidate.
  const uint32 kFocusedIndex = 4;

  Output out;
  out.set_id(1);
  out.set_consumed(true);
  SetPreedit(kCursor, -1, kStr, kAnnotate, arraysize(kStr), &out);
  SetCandidate(
      kFocusedIndex, kCandidate, arraysize(kCandidate), kPosition, &out);

  parser->set_use_annotation(false);  // This is default, though.
  EXPECT_TRUE(parser->ParseResponse(out, &ui));

  EXPECT_TRUE(ui.test_result_string().empty());
  ASSERT_FALSE(ui.test_candidates() == NULL);
  ASSERT_FALSE(ui.test_preedit_info() == NULL);
  EXPECT_FALSE(ui.test_aux().empty());

  // Check preedit info.

  EXPECT_EQ(kCursor, ui.test_preedit_info()->cursor_pos);
  EXPECT_EQ(Widen(kStr[0]), ui.test_preedit_info()->str);
  ASSERT_EQ(1, ui.test_preedit_info()->attribute_list.size());
  const scim::Attribute &attr = (ui.test_preedit_info()->attribute_list)[0];
  EXPECT_EQ(scim::SCIM_ATTR_DECORATE, attr.get_type());
  EXPECT_EQ(kScimAnnotate[0], attr.get_value());

  // Check candidate window.
  const scim::LookupTable *table = ui.test_candidates();
  ASSERT_TRUE(table->is_cursor_visible());
  EXPECT_EQ(kFocusedIndex, table->get_cursor_pos());
  for (int i = 0; i < arraysize(kCandidate); ++i) {
    const scim::WideString &label = table->get_candidate_label(i);
    string orig_label = kCandidate[i].shortcut;
    EXPECT_EQ(Widen(orig_label), label);
    EXPECT_EQ(Widen(kCandidate[i].cand), table->get_candidate(i));
  }
}

TEST(MozcResponseParserTest, TestParseResponse_OneLineSuggestion) {
  MozcResponseParser *parser = new MozcResponseParser;
  ScimMozcTest ui(parser);

  const uint32 kCursor = 6;
  const char *kStr[] = {
    "abcabc"
  };
  const Preedit::Segment::Annotation kAnnotate[] = {
    Preedit::Segment::UNDERLINE,
  };
  const unsigned int kScimAnnotate[] = {
    scim::SCIM_ATTR_DECORATE_UNDERLINE,
  };

  // Don't set the shortcut label and the forcused index since it's suggestion.
  const Cand kCandidate[] = {
    { "", "P", "cand", "S", "D" },
  };
  const char kAnnotatedStr[] = "PcandS [D]";
  // The position of the window.
  const uint32 kPosition = 3;
  const int kFocusedIndex = -1;

  Output out;
  out.set_id(1);
  out.set_consumed(true);
  SetPreedit(kCursor, -1, kStr, kAnnotate, arraysize(kStr), &out);
  SetCandidate(
      kFocusedIndex, kCandidate, arraysize(kCandidate), kPosition, &out);

  parser->set_use_annotation(true);
  EXPECT_TRUE(parser->ParseResponse(out, &ui));

  EXPECT_TRUE(ui.test_result_string().empty());
  ASSERT_FALSE(ui.test_candidates() == NULL);
  ASSERT_FALSE(ui.test_preedit_info() == NULL);
  EXPECT_TRUE(ui.test_aux().empty()) << ui.test_aux();

  // Check preedit info.
  EXPECT_EQ(kCursor, ui.test_preedit_info()->cursor_pos);
  EXPECT_EQ(Widen(kStr[0]), ui.test_preedit_info()->str);
  ASSERT_EQ(1, ui.test_preedit_info()->attribute_list.size());
  const scim::Attribute &attr = (ui.test_preedit_info()->attribute_list)[0];
  EXPECT_EQ(scim::SCIM_ATTR_DECORATE, attr.get_type());
  EXPECT_EQ(kScimAnnotate[0], attr.get_value());

  // Check candidate window.
  const scim::LookupTable *table = ui.test_candidates();
  EXPECT_FALSE(table->is_cursor_visible());
  const scim::WideString &label = table->get_candidate_label(0);
  EXPECT_EQ(Widen(""), label);
  EXPECT_EQ(Widen(kAnnotatedStr), table->get_candidate(0));
}

TEST(MozcResponseParserTest, TestParseResponse_MultiLineSuggestion) {
  MozcResponseParser *parser = new MozcResponseParser;
  ScimMozcTest ui(parser);

  const uint32 kCursor = 6;
  const char *kStr[] = {
    "abcabc"
  };
  const Preedit::Segment::Annotation kAnnotate[] = {
    Preedit::Segment::UNDERLINE,
  };
  const unsigned int kScimAnnotate[] = {
    scim::SCIM_ATTR_DECORATE_UNDERLINE,
  };

  // Don't set shortcut labels and the forcused index since it's suggestion.
  const Cand kCandidate[] = {
    { "", "P", "cand", "S", "D" },
    { "", "P2", "cand2", "S2", "D2" },
  };
  const char *kAnnotatedStr[] = {
    "PcandS [D]",
    "P2cand2S2 [D2]",
  };
  // The position of the window.
  const uint32 kPosition = 3;
  const int kFocusedIndex = -1;

  Output out;
  out.set_id(1);
  out.set_consumed(true);
  SetPreedit(kCursor, -1, kStr, kAnnotate, arraysize(kStr), &out);
  SetCandidate(
      kFocusedIndex, kCandidate, arraysize(kCandidate), kPosition, &out);

  parser->set_use_annotation(true);
  EXPECT_TRUE(parser->ParseResponse(out, &ui));

  EXPECT_TRUE(ui.test_result_string().empty());
  ASSERT_FALSE(ui.test_candidates() == NULL);
  ASSERT_FALSE(ui.test_preedit_info() == NULL);
  EXPECT_TRUE(ui.test_aux().empty()) << ui.test_aux();

  // Check preedit info.
  EXPECT_EQ(kCursor, ui.test_preedit_info()->cursor_pos);
  EXPECT_EQ(Widen(kStr[0]), ui.test_preedit_info()->str);
  ASSERT_EQ(1, ui.test_preedit_info()->attribute_list.size());
  const scim::Attribute &attr = (ui.test_preedit_info()->attribute_list)[0];
  EXPECT_EQ(scim::SCIM_ATTR_DECORATE, attr.get_type());
  EXPECT_EQ(kScimAnnotate[0], attr.get_value());

  // Check candidate window.
  const scim::LookupTable *table = ui.test_candidates();
  EXPECT_FALSE(table->is_cursor_visible());
  for (int i = 0; i < arraysize(kAnnotatedStr); ++i) {
    const scim::WideString &label = table->get_candidate_label(i);
    EXPECT_EQ(Widen(""), label);
    EXPECT_EQ(Widen(kAnnotatedStr[i]), table->get_candidate(i));
  }
}

}  // namespace mozc_unix_scim
