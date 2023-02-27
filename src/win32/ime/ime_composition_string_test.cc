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

#include "win32/ime/ime_composition_string.h"

#include <cstdint>

#include "base/logging.h"
#include "base/util.h"
#include "protocol/commands.pb.h"
#include "testing/googletest.h"
#include "testing/gunit.h"

namespace mozc {
namespace win32 {
namespace {
constexpr size_t kNumCandidates = 13;
const char *kValueList[kNumCandidates] = {
    "Beta", "ベータ", "BETA",   "beta",  "β",          "Β",    "㌼",
    "Beta", "べーた", "ベータ", "be-ta", "ｂｅ－ｔａ", "ﾍﾞｰﾀ",
};
const int32_t kValueLengths[kNumCandidates] = {
    4, 3, 4, 4, 1, 1, 1, 4, 3, 3, 5, 5, 4,
};
const int32_t kIDs[kNumCandidates] = {
    0, 1, 2, 3, 4, 5, 6, 7, -1, -2, -3, -7, -11,
};

std::string GetStringImpl(const CompositionString &composition,
                          const DWORD offset, const DWORD length) {
  const BYTE *addr = reinterpret_cast<const BYTE *>(&composition);
  const wchar_t *string_start =
      reinterpret_cast<const wchar_t *>(addr + offset);
  const std::wstring wstr(string_start, string_start + length);
  std::string str;
  Util::WideToUtf8(wstr.c_str(), &str);
  return str;
}

BYTE GetAttributeImpl(const CompositionString &composition, const DWORD offset,
                      const DWORD length, size_t index) {
  const BYTE *addr = reinterpret_cast<const BYTE *>(&composition);
  const BYTE *attribute_start = addr + offset;
  EXPECT_LE(0, index);
  EXPECT_LT(index, length);
  return attribute_start[index];
}

#define GET_STRING(composition, field_name)                               \
  GetStringImpl((composition), (composition).info.dw##field_name##Offset, \
                (composition).info.dw##field_name##Len)

#define GET_ATTRIBUTE(composition, field_name, index)                        \
  GetAttributeImpl((composition), (composition).info.dw##field_name##Offset, \
                   (composition).info.dw##field_name##Len, (index))

// TODO(yukawa): Make a common library for this function.
void FillOutputForSuggestion(commands::Output *output) {
  DCHECK_NE(nullptr, output);
  output->Clear();

  output->set_mode(commands::HIRAGANA);
  output->set_consumed(true);
  {
    commands::Preedit *preedit = output->mutable_preedit();
    preedit->set_cursor(4);
    {
      commands::Preedit::Segment *segment = preedit->add_segment();
      segment->set_annotation(commands::Preedit::Segment::UNDERLINE);
      segment->set_value("あるふぁ");
      segment->set_value_length(4);
      segment->set_key("あるふぁ");
    }
  }
  {
    commands::Candidates *candidates = output->mutable_candidates();
    candidates->set_size(2);
    {
      commands::Candidates::Candidate *candidate = candidates->add_candidate();
      candidate->set_index(0);
      candidate->set_value("AlphaBeta");
      candidate->set_id(0);
    }
    {
      commands::Candidates::Candidate *candidate = candidates->add_candidate();
      candidate->set_index(1);
      candidate->set_value("アルファ");
      candidate->set_id(1);
    }
    candidates->set_position(0);
    candidates->set_category(commands::SUGGESTION);
    candidates->set_display_type(commands::MAIN);
    {
      commands::Footer *footer = candidates->mutable_footer();
      footer->set_sub_label("build 436");
    }
  }
  {
    commands::Status *status = output->mutable_status();
    status->set_activated(true);
    status->set_mode(commands::HIRAGANA);
  }
  {
    commands::CandidateList *candidate_list =
        output->mutable_all_candidate_words();
    candidate_list->set_focused_index(0);
    {
      commands::CandidateWord *candidate = candidate_list->add_candidates();
      candidate->set_id(0);
      candidate->set_index(0);
      candidate->set_key("あるふぁべーた");
      candidate->set_value("AlphaBeta");
    }
    {
      commands::CandidateWord *candidate = candidate_list->add_candidates();
      candidate->set_id(1);
      candidate->set_index(1);
      candidate->set_value("アルファ");
    }
    candidate_list->set_category(commands::SUGGESTION);
  }
}

// TODO(yukawa): Make a common library for this function.
void FillOutputForPrediction(commands::Output *output) {
  DCHECK_NE(nullptr, output);
  output->Clear();

  output->set_mode(commands::HIRAGANA);
  output->set_consumed(true);
  {
    commands::Preedit *preedit = output->mutable_preedit();
    preedit->set_cursor(9);
    {
      commands::Preedit::Segment *segment = preedit->add_segment();
      segment->set_annotation(commands::Preedit::Segment::HIGHLIGHT);
      segment->set_value("AlphaBeta");
      segment->set_value_length(9);
      segment->set_key("あるふぁ");
    }
  }
  {
    commands::Candidates *candidates = output->mutable_candidates();
    candidates->set_focused_index(0);
    candidates->set_size(2);
    {
      commands::Candidates::Candidate *candidate = candidates->add_candidate();
      candidate->set_index(0);
      candidate->set_value("AlphaBeta");
      {
        commands::Annotation *annotation = candidate->mutable_annotation();
        annotation->set_shortcut("1");
      }
      candidate->set_id(0);
    }
    {
      commands::Candidates::Candidate *candidate = candidates->add_candidate();
      candidate->set_index(1);
      candidate->set_value("アルファ");
      {
        commands::Annotation *annotation = candidate->mutable_annotation();
        annotation->set_shortcut("2");
      }
      candidate->set_id(1);
    }
    candidates->set_position(0);
    candidates->set_category(commands::PREDICTION);
    candidates->set_display_type(commands::MAIN);
    {
      commands::Footer *footer = candidates->mutable_footer();
      footer->set_index_visible(true);
      footer->set_logo_visible(true);
      footer->set_sub_label("build 436");
    }
  }
  {
    commands::Status *status = output->mutable_status();
    status->set_activated(true);
    status->set_mode(commands::HIRAGANA);
  }
  {
    commands::CandidateList *candidate_list =
        output->mutable_all_candidate_words();
    candidate_list->set_focused_index(0);
    {
      commands::CandidateWord *candidate = candidate_list->add_candidates();
      candidate->set_id(0);
      candidate->set_index(0);
      candidate->set_key("あるふぁべーた");
      candidate->set_value("AlphaBeta");
    }
    {
      commands::CandidateWord *candidate = candidate_list->add_candidates();
      candidate->set_id(1);
      candidate->set_index(1);
      candidate->set_value("アルファ");
    }
    candidate_list->set_category(commands::PREDICTION);
  }
}

// TODO(yukawa): Make a common library for this function.
void FillOutputForConversion(commands::Output *output, int focused_index,
                             bool has_candidates) {
  DCHECK_LE(0, focused_index);
  DCHECK_GT(kNumCandidates, focused_index);
  DCHECK_NE(nullptr, output);
  output->Clear();

  const int32_t focused_value_length = kValueLengths[focused_index];
  const char *focused_value = kValueList[focused_index];
  const int32_t focused_id = kIDs[focused_index];

  output->set_mode(commands::HIRAGANA);
  output->set_consumed(true);
  {
    const int alpha_length = 5;
    commands::Preedit *preedit = output->mutable_preedit();
    preedit->set_cursor(alpha_length + focused_value_length);
    {
      commands::Preedit::Segment *segment = preedit->add_segment();
      segment->set_annotation(commands::Preedit::Segment::UNDERLINE);
      segment->set_value("Alpha");
      segment->set_value_length(alpha_length);
      segment->set_key("あるふぁ");
    }
    {
      commands::Preedit::Segment *segment = preedit->add_segment();
      segment->set_annotation(commands::Preedit::Segment::HIGHLIGHT);
      segment->set_value(focused_value);
      segment->set_value_length(focused_value_length);
      segment->set_key("べーた");
    }
    preedit->set_highlighted_position(alpha_length);
  }

  if (has_candidates) {
    commands::Candidates *candidates = output->mutable_candidates();
    candidates->set_focused_index(focused_index);

    candidates->set_size(9);
    {
      commands::Candidates::Candidate *candidate = candidates->add_candidate();
      candidate->set_index(0);
      candidate->set_value("BETA");
      {
        commands::Annotation *annotation = candidate->mutable_annotation();
        annotation->set_description("[半] アルファベット");
        annotation->set_shortcut("1");
      }
      candidate->set_id(0);
    }
    {
      commands::Candidates::Candidate *candidate = candidates->add_candidate();
      candidate->set_index(1);
      candidate->set_value("ベータ");
      {
        commands::Annotation *annotation = candidate->mutable_annotation();
        annotation->set_description("[全] カタカナ");
        annotation->set_shortcut("2");
      }
      candidate->set_id(1);
    }
    {
      commands::Candidates::Candidate *candidate = candidates->add_candidate();
      candidate->set_index(2);
      candidate->set_value("beta");
      {
        commands::Annotation *annotation = candidate->mutable_annotation();
        annotation->set_description("[半] アルファベット");
        annotation->set_shortcut("3");
      }
      candidate->set_id(2);
    }
    {
      commands::Candidates::Candidate *candidate = candidates->add_candidate();
      candidate->set_index(3);
      candidate->set_value("β");
      {
        commands::Annotation *annotation = candidate->mutable_annotation();
        annotation->set_description("ギリシャ文字(小文字)");
        annotation->set_shortcut("4");
      }
      candidate->set_id(3);
    }
    {
      commands::Candidates::Candidate *candidate = candidates->add_candidate();
      candidate->set_index(4);
      candidate->set_value("Β");
      {
        commands::Annotation *annotation = candidate->mutable_annotation();
        annotation->set_description("ギリシャ文字(大文字)");
        annotation->set_shortcut("5");
      }
      candidate->set_id(4);
    }
    {
      commands::Candidates::Candidate *candidate = candidates->add_candidate();
      candidate->set_index(5);
      candidate->set_value("㌼");
      {
        commands::Annotation *annotation = candidate->mutable_annotation();
        annotation->set_description("<機種依存文字>");
        annotation->set_shortcut("6");
      }
      candidate->set_id(5);
    }
    {
      commands::Candidates::Candidate *candidate = candidates->add_candidate();
      candidate->set_index(6);
      candidate->set_value("Beta");
      {
        commands::Annotation *annotation = candidate->mutable_annotation();
        annotation->set_description("[半] アルファベット");
        annotation->set_shortcut("7");
      }
      candidate->set_id(6);
    }
    {
      commands::Candidates::Candidate *candidate = candidates->add_candidate();
      candidate->set_index(7);
      candidate->set_value("べーた");
      {
        commands::Annotation *annotation = candidate->mutable_annotation();
        annotation->set_description("ひらがな");
        annotation->set_shortcut("8");
      }
      candidate->set_id(7);
    }
    {
      commands::Candidates::Candidate *candidate = candidates->add_candidate();
      candidate->set_index(8);
      candidate->set_value("そのほかの文字種");
      {
        commands::Annotation *annotation = candidate->mutable_annotation();
        annotation->set_shortcut("9");
      }
      candidate->set_id(-3);
    }
    candidates->set_position(5);
    {
      commands::Candidates *sub_candidates =
          candidates->mutable_subcandidates();
      sub_candidates->set_focused_index(2);
      sub_candidates->set_size(5);
      {
        {
          commands::Candidates::Candidate *candidate =
              sub_candidates->add_candidate();
          candidate->set_index(0);
          candidate->set_value("べーた");
          {
            commands::Annotation *annotation = candidate->mutable_annotation();
            annotation->set_description("ひらがな");
          }
          candidate->set_id(-1);
        }
        {
          commands::Candidates::Candidate *candidate =
              sub_candidates->add_candidate();
          candidate->set_index(1);
          candidate->set_value("ベータ");
          {
            commands::Annotation *annotation = candidate->mutable_annotation();
            annotation->set_description("[全] カタカナ");
          }
          candidate->set_id(-2);
        }
        {
          commands::Candidates::Candidate *candidate =
              sub_candidates->add_candidate();
          candidate->set_index(2);
          candidate->set_value("be-ta");
          {
            commands::Annotation *annotation = candidate->mutable_annotation();
            annotation->set_description("[半]");
          }
          candidate->set_id(-3);
        }
        {
          commands::Candidates::Candidate *candidate =
              sub_candidates->add_candidate();
          candidate->set_index(3);
          candidate->set_value("ｂｅ－ｔａ");
          {
            commands::Annotation *annotation = candidate->mutable_annotation();
            annotation->set_description("[全]");
          }
          candidate->set_id(-7);
        }
        {
          commands::Candidates::Candidate *candidate =
              sub_candidates->add_candidate();
          candidate->set_index(4);
          candidate->set_value("ﾍﾞｰﾀ");
          {
            commands::Annotation *annotation = candidate->mutable_annotation();
            annotation->set_description("[半] カタカナ");
          }
          candidate->set_id(-11);
        }
      }
      sub_candidates->set_position(8);
      sub_candidates->set_category(commands::TRANSLITERATION);
      sub_candidates->set_display_type(commands::CASCADE);
    }
    candidates->set_category(commands::CONVERSION);
    candidates->set_display_type(commands::MAIN);
    {
      commands::Footer *footer = candidates->mutable_footer();
      footer->set_index_visible(true);
      footer->set_logo_visible(true);
      footer->set_sub_label("build 436");
    }
  }
  {
    commands::Status *status = output->mutable_status();
    status->set_activated(true);
    status->set_mode(commands::HIRAGANA);
  }

  {
    commands::CandidateList *candidate_list =
        output->mutable_all_candidate_words();
    candidate_list->set_focused_index(focused_index);

    for (size_t i = 0; i < kNumCandidates; ++i) {
      commands::CandidateWord *candidate = candidate_list->add_candidates();
      candidate->set_id(kIDs[i]);
      candidate->set_index(i);
      candidate->set_value(kValueList[i]);
    }
    candidate_list->set_category(commands::CONVERSION);
  }
}
}  // namespace

TEST(ImeCompositionStringTest, StartCompositionTest) {
  CompositionString compstr;
  EXPECT_TRUE(compstr.Initialize());

  commands::Output output;
  commands::Preedit *preedit = output.mutable_preedit();

  commands::Preedit::Segment *segment = preedit->add_segment();
  preedit->set_cursor(1);

  segment->set_annotation(commands::Preedit::Segment::UNDERLINE);
  segment->set_key("が");
  segment->set_value("が");
  segment->set_value_length(1);

  std::vector<UIMessage> messages;
  EXPECT_TRUE(compstr.Update(output, &messages));
  EXPECT_EQ(messages.size(), 2);

  EXPECT_EQ(messages[0].message(), WM_IME_STARTCOMPOSITION);
  EXPECT_EQ(messages[0].wparam(), 0);
  EXPECT_EQ(messages[0].lparam(), 0);

  EXPECT_EQ(messages[1].message(), WM_IME_COMPOSITION);
  EXPECT_EQ(messages[1].wparam(), 0);
  // When the preedit is updated, the following flags should be sent
  // regardless of which field is actually updated. Otherwise, some
  // applications such as wordpad OOo Writer 3.0 will not update composition
  // window and caret state properly.
  EXPECT_EQ(
      messages[1].lparam(),
      (GCS_COMPREADSTR | GCS_COMPREADATTR | GCS_COMPREADCLAUSE | GCS_COMPSTR |
       GCS_COMPATTR | GCS_COMPCLAUSE | GCS_CURSORPOS | GCS_DELTASTART));

  EXPECT_EQ(compstr.focused_character_index_, 0);
  EXPECT_EQ(compstr.info.dwCompReadAttrLen, 2);
  EXPECT_EQ(compstr.info.dwCompReadClauseLen, 8);
  EXPECT_EQ(compstr.info.dwCompReadStrLen, 2);
  EXPECT_EQ(compstr.info.dwCompAttrLen, 1);
  EXPECT_EQ(compstr.info.dwCompClauseLen, 8);
  EXPECT_EQ(compstr.info.dwCompStrLen, 1);
  EXPECT_EQ(compstr.info.dwCursorPos, 1);
  EXPECT_EQ(compstr.info.dwDeltaStart, 0);
  EXPECT_EQ(compstr.info.dwResultReadClauseLen, 0);
  EXPECT_EQ(compstr.info.dwResultReadStrLen, 0);
  EXPECT_EQ(compstr.info.dwResultClauseLen, 0);
  EXPECT_EQ(compstr.info.dwResultStrLen, 0);
  EXPECT_EQ(GET_STRING(compstr, CompReadStr), "ｶﾞ");
  EXPECT_EQ(GET_ATTRIBUTE(compstr, CompReadAttr, 0), ATTR_INPUT);
  EXPECT_EQ(GET_ATTRIBUTE(compstr, CompReadAttr, 1), ATTR_INPUT);

  // TODO(yukawa): Check other fields in |compstr.info|
}

TEST(ImeCompositionStringTest, EndCompositionWhenCompositionBecomesEmpty) {
  // WM_IME_COMPOSITION should be sent with setting 0 to wparam and lparam.
  // Otherwise, OOo 3.0.1 will not show the caret again.

  CompositionString compstr;
  EXPECT_TRUE(compstr.Initialize());
  compstr.info.dwCompStrLen = 1;
  compstr.info.dwCompReadStrLen = 1;

  commands::Output output;
  output.set_consumed(true);
  output.set_mode(mozc::commands::HIRAGANA);
  output.mutable_status()->set_activated(true);
  output.mutable_status()->set_mode(mozc::commands::HIRAGANA);

  std::vector<UIMessage> messages;
  EXPECT_TRUE(compstr.Update(output, &messages));
  EXPECT_EQ(messages.size(), 2);

  EXPECT_EQ(messages[0].message(), WM_IME_COMPOSITION);
  EXPECT_EQ(messages[0].wparam(), 0);
  EXPECT_EQ(messages[0].lparam(), 0);

  EXPECT_EQ(messages[1].message(), WM_IME_ENDCOMPOSITION);
  EXPECT_EQ(messages[1].wparam(), 0);
  EXPECT_EQ(messages[1].lparam(), 0);

  EXPECT_EQ(compstr.focused_character_index_, 0);
  EXPECT_EQ(compstr.info.dwCompReadAttrLen, 0);
  EXPECT_EQ(compstr.info.dwCompReadClauseLen, 0);
  EXPECT_EQ(compstr.info.dwCompReadStrLen, 0);
  EXPECT_EQ(compstr.info.dwCompAttrLen, 0);
  EXPECT_EQ(compstr.info.dwCompClauseLen, 0);
  EXPECT_EQ(compstr.info.dwCompStrLen, 0);
  EXPECT_EQ(compstr.info.dwCursorPos, -1);
  EXPECT_EQ(compstr.info.dwDeltaStart, 0);
  EXPECT_EQ(compstr.info.dwResultReadClauseLen, 0);
  EXPECT_EQ(compstr.info.dwResultReadStrLen, 0);
  EXPECT_EQ(compstr.info.dwResultClauseLen, 0);
  EXPECT_EQ(compstr.info.dwResultStrLen, 0);
}

TEST(ImeCompositionStringTest, EndCompositionWhenCompositionIsCommitted) {
  // WM_IME_COMPOSITION should be sent up to once.
  // Otherwise, the result string will be committed twice in wordpad.exe.

  CompositionString compstr;
  EXPECT_TRUE(compstr.Initialize());
  compstr.info.dwCompStrLen = 1;
  compstr.info.dwCompReadStrLen = 1;

  commands::Output output;
  output.set_consumed(true);
  output.set_mode(mozc::commands::HIRAGANA);
  output.mutable_status()->set_activated(true);
  output.mutable_status()->set_mode(mozc::commands::HIRAGANA);

  commands::Result *result = output.mutable_result();
  result->set_key("が");
  result->set_value("が");
  result->set_type(mozc::commands::Result::STRING);

  std::vector<UIMessage> messages;
  EXPECT_TRUE(compstr.Update(output, &messages));
  EXPECT_EQ(messages.size(), 2);

  EXPECT_EQ(messages[0].message(), WM_IME_COMPOSITION);
  EXPECT_EQ(messages[0].wparam(), 0);
  // When the result string is updated, the following flags should be sent
  // regardless of which field is actually updated. Otherwise, some
  // applications such as wordpad OOo Writer 3.0 will not update composition
  // window and caret state properly.
  EXPECT_EQ(messages[0].lparam(), (GCS_RESULTREADSTR | GCS_RESULTREADCLAUSE |
                                   GCS_RESULTSTR | GCS_RESULTCLAUSE));

  EXPECT_EQ(messages[1].message(), WM_IME_ENDCOMPOSITION);
  EXPECT_EQ(messages[1].wparam(), 0);
  EXPECT_EQ(messages[1].lparam(), 0);

  EXPECT_EQ(compstr.focused_character_index_, 0);
  EXPECT_EQ(compstr.info.dwCompReadAttrLen, 0);
  EXPECT_EQ(compstr.info.dwCompReadClauseLen, 0);
  EXPECT_EQ(compstr.info.dwCompReadStrLen, 0);
  EXPECT_EQ(compstr.info.dwCompAttrLen, 0);
  EXPECT_EQ(compstr.info.dwCompClauseLen, 0);
  EXPECT_EQ(compstr.info.dwCompStrLen, 0);
  EXPECT_EQ(compstr.info.dwCursorPos, -1);
  EXPECT_EQ(compstr.info.dwDeltaStart, 0);
  EXPECT_EQ(compstr.info.dwResultReadClauseLen, 8);
  EXPECT_EQ(compstr.info.dwResultReadStrLen, 2);
  EXPECT_EQ(compstr.info.dwResultClauseLen, 8);
  EXPECT_EQ(compstr.info.dwResultStrLen, 1);
  EXPECT_EQ(GET_STRING(compstr, ResultReadStr), "ｶﾞ");
}

TEST(ImeCompositionStringTest, SpaceKeyWhenIMEIsTurnedOn_Issue3200585) {
  // If the current state is no composition and the server returns a result
  // string, it should be interpreted as one-shot composition as follows.
  //   WM_IME_STARTCOMPOSITION
  //     -> WM_IME_COMPOSITION
  //        -> WM_IME_ENDCOMPOSITION
  // Otherwise, you cannot input Full-width space on Outlook 2010. (b/3200585)

  CompositionString compstr;
  EXPECT_TRUE(compstr.Initialize());

  commands::Output output;
  // Emulate special_key::SPACE key when IME is turned on.
  output.set_mode(mozc::commands::HIRAGANA);
  output.set_consumed(true);
  output.mutable_result()->set_key(" ");
  output.mutable_result()->set_value("　");
  output.mutable_result()->set_type(mozc::commands::Result::STRING);
  output.mutable_status()->set_activated(true);
  output.mutable_status()->set_mode(mozc::commands::HIRAGANA);

  std::vector<UIMessage> messages;
  EXPECT_TRUE(compstr.Update(output, &messages));
  EXPECT_EQ(messages.size(), 3);

  EXPECT_EQ(messages[0].message(), WM_IME_STARTCOMPOSITION);
  EXPECT_EQ(messages[0].wparam(), 0);
  EXPECT_EQ(messages[0].lparam(), 0);

  EXPECT_EQ(messages[1].message(), WM_IME_COMPOSITION);
  EXPECT_EQ(messages[1].wparam(), 0);
  EXPECT_EQ(messages[1].lparam(), (GCS_RESULTREADSTR | GCS_RESULTREADCLAUSE |
                                   GCS_RESULTSTR | GCS_RESULTCLAUSE));

  EXPECT_EQ(messages[2].message(), WM_IME_ENDCOMPOSITION);
  EXPECT_EQ(messages[2].wparam(), 0);
  EXPECT_EQ(messages[2].lparam(), 0);

  EXPECT_EQ(compstr.focused_character_index_, 0);
  EXPECT_EQ(compstr.info.dwCompReadAttrLen, 0);
  EXPECT_EQ(compstr.info.dwCompReadClauseLen, 0);
  EXPECT_EQ(compstr.info.dwCompReadStrLen, 0);
  EXPECT_EQ(compstr.info.dwCompAttrLen, 0);
  EXPECT_EQ(compstr.info.dwCompClauseLen, 0);
  EXPECT_EQ(compstr.info.dwCompStrLen, 0);
  EXPECT_EQ(compstr.info.dwCursorPos, -1);
  EXPECT_EQ(compstr.info.dwDeltaStart, 0);
  EXPECT_EQ(compstr.info.dwResultReadClauseLen, 8);
  EXPECT_EQ(compstr.info.dwResultReadStrLen, 1);
  EXPECT_EQ(compstr.info.dwResultClauseLen, 8);
  EXPECT_EQ(compstr.info.dwResultStrLen, 1);
  EXPECT_EQ(GET_STRING(compstr, ResultStr), "　");
  EXPECT_EQ(GET_STRING(compstr, ResultReadStr), " ");
}

TEST(ImeCompositionStringTest,
     EndCompositionWhenCompositionIsCommittedWithPreedit) {
  // WM_IME_COMPOSITION should be sent up to once.
  // Otherwise, the result string will be committed twice in wordpad.exe.

  CompositionString compstr;
  EXPECT_TRUE(compstr.Initialize());
  compstr.info.dwCompStrLen = 1;
  compstr.info.dwCompReadStrLen = 1;

  commands::Output output;
  output.set_consumed(true);
  output.set_mode(mozc::commands::HIRAGANA);
  output.mutable_status()->set_activated(true);
  output.mutable_status()->set_mode(mozc::commands::HIRAGANA);

  commands::Result *result = output.mutable_result();
  result->set_key("が");
  result->set_value("が");
  result->set_type(mozc::commands::Result::STRING);

  commands::Preedit *preedit = output.mutable_preedit();
  preedit->set_cursor(1);

  commands::Preedit::Segment *segment = preedit->add_segment();
  segment->set_annotation(commands::Preedit::Segment::UNDERLINE);
  segment->set_key("が");
  segment->set_value("が");
  segment->set_value_length(1);

  std::vector<UIMessage> messages;
  EXPECT_TRUE(compstr.Update(output, &messages));
  EXPECT_EQ(messages.size(), 1);

  EXPECT_EQ(messages[0].message(), WM_IME_COMPOSITION);
  EXPECT_EQ(messages[0].wparam(), 0);
  // When both the preedit and result string is updated, the following flags
  // should be sent regardless of which field is actually updated. Otherwise,
  // some applications such as wordpad OOo Writer 3.0 will not update
  // composition window and caret state properly.
  EXPECT_EQ(
      messages[0].lparam(),
      (GCS_COMPREADSTR | GCS_COMPREADATTR | GCS_COMPREADCLAUSE | GCS_COMPSTR |
       GCS_COMPATTR | GCS_COMPCLAUSE | GCS_CURSORPOS | GCS_DELTASTART) |
          (GCS_RESULTREADSTR | GCS_RESULTREADCLAUSE | GCS_RESULTSTR |
           GCS_RESULTCLAUSE));

  EXPECT_EQ(compstr.focused_character_index_, 0);
  EXPECT_EQ(compstr.info.dwCompReadAttrLen, 2);
  EXPECT_EQ(compstr.info.dwCompReadClauseLen, 8);
  EXPECT_EQ(compstr.info.dwCompReadStrLen, 2);
  EXPECT_EQ(compstr.info.dwCompAttrLen, 1);
  EXPECT_EQ(compstr.info.dwCompClauseLen, 8);
  EXPECT_EQ(compstr.info.dwCompStrLen, 1);
  EXPECT_EQ(compstr.info.dwCursorPos, 1);
  EXPECT_EQ(compstr.info.dwDeltaStart, 0);
  EXPECT_EQ(compstr.info.dwResultReadClauseLen, 8);
  EXPECT_EQ(compstr.info.dwResultReadStrLen, 2);
  EXPECT_EQ(compstr.info.dwResultClauseLen, 8);
  EXPECT_EQ(compstr.info.dwResultStrLen, 1);
  EXPECT_EQ(GET_STRING(compstr, CompReadStr), "ｶﾞ");
  EXPECT_EQ(GET_STRING(compstr, ResultReadStr), "ｶﾞ");

  EXPECT_EQ(GET_ATTRIBUTE(compstr, CompReadAttr, 0), ATTR_INPUT);
  EXPECT_EQ(GET_ATTRIBUTE(compstr, CompReadAttr, 1), ATTR_INPUT);
}

TEST(ImeCompositionStringTest, Suggest) {
  commands::Output output;
  FillOutputForSuggestion(&output);

  CompositionString compstr;
  EXPECT_TRUE(compstr.Initialize());

  std::vector<UIMessage> messages;
  EXPECT_TRUE(compstr.Update(output, &messages));
  EXPECT_EQ(messages.size(), 2);

  EXPECT_EQ(messages[0].message(), WM_IME_STARTCOMPOSITION);
  EXPECT_EQ(messages[0].wparam(), 0);
  EXPECT_EQ(messages[0].lparam(), 0);

  EXPECT_EQ(messages[1].message(), WM_IME_COMPOSITION);
  EXPECT_EQ(messages[1].wparam(), 0);
  // When the preedit is updated, the following flags should be sent
  // regardless of which field is actually updated. Otherwise, some
  // applications such as wordpad OOo Writer 3.0 will not update composition
  // window and caret state properly.
  EXPECT_EQ(
      messages[1].lparam(),
      (GCS_COMPREADSTR | GCS_COMPREADATTR | GCS_COMPREADCLAUSE | GCS_COMPSTR |
       GCS_COMPATTR | GCS_COMPCLAUSE | GCS_CURSORPOS | GCS_DELTASTART));

  EXPECT_EQ(compstr.focused_character_index_, 0);
  EXPECT_EQ(compstr.info.dwCompReadAttrLen, 4);
  EXPECT_EQ(compstr.info.dwCompReadClauseLen, 8);
  EXPECT_EQ(compstr.info.dwCompReadStrLen, 4);
  EXPECT_EQ(compstr.info.dwCompAttrLen, 4);
  EXPECT_EQ(compstr.info.dwCompClauseLen, 8);
  EXPECT_EQ(compstr.info.dwCompStrLen, 4);
  EXPECT_EQ(compstr.info.dwCursorPos, 4);
  EXPECT_EQ(compstr.info.dwDeltaStart, 0);
  EXPECT_EQ(compstr.info.dwResultReadClauseLen, 0);
  EXPECT_EQ(compstr.info.dwResultReadStrLen, 0);
  EXPECT_EQ(compstr.info.dwResultClauseLen, 0);
  EXPECT_EQ(compstr.info.dwResultStrLen, 0);

  EXPECT_EQ(GET_STRING(compstr, CompReadStr), "ｱﾙﾌｧ");
  EXPECT_EQ(GET_STRING(compstr, CompStr), "あるふぁ");

  EXPECT_EQ(GET_ATTRIBUTE(compstr, CompReadAttr, 0), ATTR_INPUT);
  EXPECT_EQ(GET_ATTRIBUTE(compstr, CompReadAttr, 1), ATTR_INPUT);
  EXPECT_EQ(GET_ATTRIBUTE(compstr, CompReadAttr, 2), ATTR_INPUT);
  EXPECT_EQ(GET_ATTRIBUTE(compstr, CompReadAttr, 3), ATTR_INPUT);

  EXPECT_EQ(GET_ATTRIBUTE(compstr, CompAttr, 0), ATTR_INPUT);
  EXPECT_EQ(GET_ATTRIBUTE(compstr, CompAttr, 1), ATTR_INPUT);
  EXPECT_EQ(GET_ATTRIBUTE(compstr, CompAttr, 2), ATTR_INPUT);
  EXPECT_EQ(GET_ATTRIBUTE(compstr, CompAttr, 3), ATTR_INPUT);
}

TEST(ImeCompositionStringTest, Predict) {
  commands::Output output;
  FillOutputForPrediction(&output);

  CompositionString compstr;
  EXPECT_TRUE(compstr.Initialize());

  std::vector<UIMessage> messages;
  EXPECT_TRUE(compstr.Update(output, &messages));
  EXPECT_EQ(messages.size(), 2);

  EXPECT_EQ(messages[0].message(), WM_IME_STARTCOMPOSITION);
  EXPECT_EQ(messages[0].wparam(), 0);
  EXPECT_EQ(messages[0].lparam(), 0);

  EXPECT_EQ(messages[1].message(), WM_IME_COMPOSITION);
  EXPECT_EQ(messages[1].wparam(), 0);
  // When the preedit is updated, the following flags should be sent
  // regardless of which field is actually updated. Otherwise, some
  // applications such as wordpad OOo Writer 3.0 will not update composition
  // window and caret state properly.
  EXPECT_EQ(
      messages[1].lparam(),
      (GCS_COMPREADSTR | GCS_COMPREADATTR | GCS_COMPREADCLAUSE | GCS_COMPSTR |
       GCS_COMPATTR | GCS_COMPCLAUSE | GCS_CURSORPOS | GCS_DELTASTART));

  EXPECT_EQ(compstr.focused_character_index_, 0);
  EXPECT_EQ(compstr.info.dwCompReadAttrLen, 4);
  EXPECT_EQ(compstr.info.dwCompReadClauseLen, 8);
  EXPECT_EQ(compstr.info.dwCompReadStrLen, 4);
  EXPECT_EQ(compstr.info.dwCompAttrLen, 9);
  EXPECT_EQ(compstr.info.dwCompClauseLen, 8);
  EXPECT_EQ(compstr.info.dwCompStrLen, 9);
  EXPECT_EQ(compstr.info.dwCursorPos, 9);
  EXPECT_EQ(compstr.info.dwDeltaStart, 0);
  EXPECT_EQ(compstr.info.dwResultReadClauseLen, 0);
  EXPECT_EQ(compstr.info.dwResultReadStrLen, 0);
  EXPECT_EQ(compstr.info.dwResultClauseLen, 0);
  EXPECT_EQ(compstr.info.dwResultStrLen, 0);

  EXPECT_EQ(GET_STRING(compstr, CompReadStr), "ｱﾙﾌｧ");
  EXPECT_EQ(GET_STRING(compstr, CompStr), "AlphaBeta");

  EXPECT_EQ(GET_ATTRIBUTE(compstr, CompReadAttr, 0), ATTR_TARGET_CONVERTED);
  EXPECT_EQ(GET_ATTRIBUTE(compstr, CompReadAttr, 1), ATTR_TARGET_CONVERTED);
  EXPECT_EQ(GET_ATTRIBUTE(compstr, CompReadAttr, 2), ATTR_TARGET_CONVERTED);
  EXPECT_EQ(GET_ATTRIBUTE(compstr, CompReadAttr, 3), ATTR_TARGET_CONVERTED);

  EXPECT_EQ(GET_ATTRIBUTE(compstr, CompAttr, 0), ATTR_TARGET_CONVERTED);
  EXPECT_EQ(GET_ATTRIBUTE(compstr, CompAttr, 1), ATTR_TARGET_CONVERTED);
  EXPECT_EQ(GET_ATTRIBUTE(compstr, CompAttr, 2), ATTR_TARGET_CONVERTED);
  EXPECT_EQ(GET_ATTRIBUTE(compstr, CompAttr, 3), ATTR_TARGET_CONVERTED);
  EXPECT_EQ(GET_ATTRIBUTE(compstr, CompAttr, 4), ATTR_TARGET_CONVERTED);
  EXPECT_EQ(GET_ATTRIBUTE(compstr, CompAttr, 5), ATTR_TARGET_CONVERTED);
  EXPECT_EQ(GET_ATTRIBUTE(compstr, CompAttr, 6), ATTR_TARGET_CONVERTED);
  EXPECT_EQ(GET_ATTRIBUTE(compstr, CompAttr, 7), ATTR_TARGET_CONVERTED);
  EXPECT_EQ(GET_ATTRIBUTE(compstr, CompAttr, 8), ATTR_TARGET_CONVERTED);
}

TEST(ImeCompositionStringTest, Convert) {
  commands::Output output;

  CompositionString compstr;
  EXPECT_TRUE(compstr.Initialize());

  // First conversion
  // It is common for traditional IMEs not to display candidate window for the
  // first conversion.  These IMEs do not fill CandidateInfo for the first
  // conversiontoo, too.  See b/2978825 for details.  Mozc server conforms
  // to this behavior by keeping |output.candidates()| empty for the first
  // conversion.
  FillOutputForConversion(&output, 0, false);
  {
    std::vector<UIMessage> messages;
    EXPECT_TRUE(compstr.Update(output, &messages));
    EXPECT_EQ(messages.size(), 2);

    EXPECT_EQ(messages[0].message(), WM_IME_STARTCOMPOSITION);
    EXPECT_EQ(messages[0].wparam(), 0);
    EXPECT_EQ(messages[0].lparam(), 0);

    EXPECT_EQ(messages[1].message(), WM_IME_COMPOSITION);
    EXPECT_EQ(messages[1].wparam(), 0);
    // When the preedit is updated, the following flags should be sent
    // regardless of which field is actually updated. Otherwise, some
    // applications such as wordpad OOo Writer 3.0 will not update composition
    // window and caret state properly.
    EXPECT_EQ(
        messages[1].lparam(),
        (GCS_COMPREADSTR | GCS_COMPREADATTR | GCS_COMPREADCLAUSE | GCS_COMPSTR |
         GCS_COMPATTR | GCS_COMPCLAUSE | GCS_CURSORPOS | GCS_DELTASTART));

    EXPECT_EQ(compstr.focused_character_index_, 5);
    EXPECT_EQ(compstr.info.dwCompReadAttrLen, 8);
    EXPECT_EQ(compstr.info.dwCompReadClauseLen, 12);
    EXPECT_EQ(compstr.info.dwCompReadStrLen, 8);
    EXPECT_EQ(compstr.info.dwCompAttrLen, 9);
    EXPECT_EQ(compstr.info.dwCompClauseLen, 12);
    EXPECT_EQ(compstr.info.dwCompStrLen, 9);
    EXPECT_EQ(compstr.info.dwCursorPos, 5);
    EXPECT_EQ(compstr.info.dwDeltaStart, 0);
    EXPECT_EQ(compstr.info.dwResultReadClauseLen, 0);
    EXPECT_EQ(compstr.info.dwResultReadStrLen, 0);
    EXPECT_EQ(compstr.info.dwResultClauseLen, 0);
    EXPECT_EQ(compstr.info.dwResultStrLen, 0);

    EXPECT_EQ(GET_STRING(compstr, CompReadStr), "ｱﾙﾌｧﾍﾞｰﾀ");
    EXPECT_EQ(GET_STRING(compstr, CompStr), "AlphaBeta");

    EXPECT_EQ(GET_ATTRIBUTE(compstr, CompReadAttr, 0), ATTR_CONVERTED);
    EXPECT_EQ(GET_ATTRIBUTE(compstr, CompReadAttr, 1), ATTR_CONVERTED);
    EXPECT_EQ(GET_ATTRIBUTE(compstr, CompReadAttr, 2), ATTR_CONVERTED);
    EXPECT_EQ(GET_ATTRIBUTE(compstr, CompReadAttr, 3), ATTR_CONVERTED);
    EXPECT_EQ(GET_ATTRIBUTE(compstr, CompReadAttr, 4), ATTR_TARGET_CONVERTED);
    EXPECT_EQ(GET_ATTRIBUTE(compstr, CompReadAttr, 5), ATTR_TARGET_CONVERTED);
    EXPECT_EQ(GET_ATTRIBUTE(compstr, CompReadAttr, 6), ATTR_TARGET_CONVERTED);
    EXPECT_EQ(GET_ATTRIBUTE(compstr, CompReadAttr, 7), ATTR_TARGET_CONVERTED);

    EXPECT_EQ(GET_ATTRIBUTE(compstr, CompAttr, 0), ATTR_CONVERTED);
    EXPECT_EQ(GET_ATTRIBUTE(compstr, CompAttr, 1), ATTR_CONVERTED);
    EXPECT_EQ(GET_ATTRIBUTE(compstr, CompAttr, 2), ATTR_CONVERTED);
    EXPECT_EQ(GET_ATTRIBUTE(compstr, CompAttr, 3), ATTR_CONVERTED);
    EXPECT_EQ(GET_ATTRIBUTE(compstr, CompAttr, 4), ATTR_CONVERTED);
    EXPECT_EQ(GET_ATTRIBUTE(compstr, CompAttr, 5), ATTR_TARGET_CONVERTED);
    EXPECT_EQ(GET_ATTRIBUTE(compstr, CompAttr, 6), ATTR_TARGET_CONVERTED);
    EXPECT_EQ(GET_ATTRIBUTE(compstr, CompAttr, 7), ATTR_TARGET_CONVERTED);
    EXPECT_EQ(GET_ATTRIBUTE(compstr, CompAttr, 8), ATTR_TARGET_CONVERTED);
  }

  // Second conversion
  FillOutputForConversion(&output, 1, true);
  {
    std::vector<UIMessage> messages;
    EXPECT_TRUE(compstr.Update(output, &messages));
    EXPECT_EQ(messages.size(), 1);

    EXPECT_EQ(messages[0].message(), WM_IME_COMPOSITION);
    EXPECT_EQ(messages[0].wparam(), 0);
    // When the preedit is updated, the following flags should be sent
    // regardless of which field is actually updated. Otherwise, some
    // applications such as wordpad OOo Writer 3.0 will not update composition
    // window and caret state properly.
    EXPECT_EQ(
        messages[0].lparam(),
        (GCS_COMPREADSTR | GCS_COMPREADATTR | GCS_COMPREADCLAUSE | GCS_COMPSTR |
         GCS_COMPATTR | GCS_COMPCLAUSE | GCS_CURSORPOS | GCS_DELTASTART));

    EXPECT_EQ(compstr.focused_character_index_, 5);
    EXPECT_EQ(compstr.info.dwCompReadAttrLen, 8);
    EXPECT_EQ(compstr.info.dwCompReadClauseLen, 12);
    EXPECT_EQ(compstr.info.dwCompReadStrLen, 8);
    EXPECT_EQ(compstr.info.dwCompAttrLen, 8);
    EXPECT_EQ(compstr.info.dwCompClauseLen, 12);
    EXPECT_EQ(compstr.info.dwCompStrLen, 8);
    EXPECT_EQ(compstr.info.dwCursorPos, 5);
    EXPECT_EQ(compstr.info.dwDeltaStart, 0);
    EXPECT_EQ(compstr.info.dwResultReadClauseLen, 0);
    EXPECT_EQ(compstr.info.dwResultReadStrLen, 0);
    EXPECT_EQ(compstr.info.dwResultClauseLen, 0);
    EXPECT_EQ(compstr.info.dwResultStrLen, 0);

    EXPECT_EQ(GET_STRING(compstr, CompReadStr), "ｱﾙﾌｧﾍﾞｰﾀ");
    EXPECT_EQ(GET_STRING(compstr, CompStr), "Alphaベータ");

    EXPECT_EQ(GET_ATTRIBUTE(compstr, CompReadAttr, 0), ATTR_CONVERTED);
    EXPECT_EQ(GET_ATTRIBUTE(compstr, CompReadAttr, 1), ATTR_CONVERTED);
    EXPECT_EQ(GET_ATTRIBUTE(compstr, CompReadAttr, 2), ATTR_CONVERTED);
    EXPECT_EQ(GET_ATTRIBUTE(compstr, CompReadAttr, 3), ATTR_CONVERTED);
    EXPECT_EQ(GET_ATTRIBUTE(compstr, CompReadAttr, 4), ATTR_TARGET_CONVERTED);
    EXPECT_EQ(GET_ATTRIBUTE(compstr, CompReadAttr, 5), ATTR_TARGET_CONVERTED);
    EXPECT_EQ(GET_ATTRIBUTE(compstr, CompReadAttr, 6), ATTR_TARGET_CONVERTED);
    EXPECT_EQ(GET_ATTRIBUTE(compstr, CompReadAttr, 7), ATTR_TARGET_CONVERTED);

    EXPECT_EQ(GET_ATTRIBUTE(compstr, CompAttr, 0), ATTR_CONVERTED);
    EXPECT_EQ(GET_ATTRIBUTE(compstr, CompAttr, 1), ATTR_CONVERTED);
    EXPECT_EQ(GET_ATTRIBUTE(compstr, CompAttr, 2), ATTR_CONVERTED);
    EXPECT_EQ(GET_ATTRIBUTE(compstr, CompAttr, 3), ATTR_CONVERTED);
    EXPECT_EQ(GET_ATTRIBUTE(compstr, CompAttr, 4), ATTR_CONVERTED);
    EXPECT_EQ(GET_ATTRIBUTE(compstr, CompAttr, 5), ATTR_TARGET_CONVERTED);
    EXPECT_EQ(GET_ATTRIBUTE(compstr, CompAttr, 6), ATTR_TARGET_CONVERTED);
    EXPECT_EQ(GET_ATTRIBUTE(compstr, CompAttr, 7), ATTR_TARGET_CONVERTED);
  }
}

// Check if surrogate-pair is handled properly. See b/4163234 and b/4159275
// for details.
TEST(ImeCompositionStringTest, SurrogatePairSupport) {
  CompositionString compstr;
  EXPECT_TRUE(compstr.Initialize());

  commands::Output output;
  output.set_mode(commands::HIRAGANA);
  output.set_consumed(true);
  {
    commands::Preedit *preedit = output.mutable_preedit();
    preedit->set_cursor(5);
    {
      commands::Preedit::Segment *segment = preedit->add_segment();
      segment->set_annotation(commands::Preedit::Segment::UNDERLINE);
      segment->set_value("𠮟る");
      segment->set_value_length(2);
      segment->set_key("しかる");
    }
    {
      commands::Preedit::Segment *segment = preedit->add_segment();
      segment->set_annotation(commands::Preedit::Segment::UNDERLINE);
      segment->set_value("と");
      segment->set_value_length(1);
      segment->set_key("と");
    }
    {
      commands::Preedit::Segment *segment = preedit->add_segment();
      segment->set_annotation(commands::Preedit::Segment::HIGHLIGHT);
      segment->set_value("叱る");
      segment->set_value_length(2);
      segment->set_key("しかる");
    }
    preedit->set_highlighted_position(3);
  }
  {
    commands::Status *status = output.mutable_status();
    status->set_activated(true);
    status->set_mode(commands::HIRAGANA);
  }

  std::vector<UIMessage> messages;
  EXPECT_TRUE(compstr.Update(output, &messages));

  // Here, |focused_character_index_| != Preedit::highlighted_position()
  // because of surrogate parir.
  EXPECT_EQ(compstr.focused_character_index_, 4);
  EXPECT_EQ(compstr.info.dwCompReadAttrLen, 7);
  EXPECT_EQ(compstr.info.dwCompReadClauseLen, 16);
  EXPECT_EQ(compstr.info.dwCompReadStrLen, 7);
  EXPECT_EQ(compstr.info.dwCompAttrLen, 6);
  EXPECT_EQ(compstr.info.dwCompClauseLen, 16);
  EXPECT_EQ(compstr.info.dwCompStrLen, 6);
  EXPECT_EQ(compstr.info.dwCursorPos, 4);
  EXPECT_EQ(compstr.info.dwDeltaStart, 0);
  EXPECT_EQ(compstr.info.dwResultReadClauseLen, 0);
  EXPECT_EQ(compstr.info.dwResultReadStrLen, 0);
  EXPECT_EQ(compstr.info.dwResultClauseLen, 0);
  EXPECT_EQ(compstr.info.dwResultStrLen, 0);

  EXPECT_EQ(GET_STRING(compstr, CompReadStr), "ｼｶﾙﾄｼｶﾙ");

  EXPECT_EQ(GET_ATTRIBUTE(compstr, CompReadAttr, 0), ATTR_CONVERTED);
  EXPECT_EQ(GET_ATTRIBUTE(compstr, CompReadAttr, 1), ATTR_CONVERTED);
  EXPECT_EQ(GET_ATTRIBUTE(compstr, CompReadAttr, 2), ATTR_CONVERTED);

  // TODO(yukawa): Check other fields in |compstr.info|
}
}  // namespace win32
}  // namespace mozc
