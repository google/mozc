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

#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

#include "base/util.h"
#include "session/commands.pb.h"
#include "win32/ime/ime_composition_string.h"

namespace mozc {
namespace win32 {
namespace {
const size_t kNumCandidates = 13;
const char* kValueList[kNumCandidates] = {
    "Beta",
    "\343\203\231\343\203\274\343\202\277",  // "ベータ"
    "BETA",
    "beta",
    "\316\262",                              // "β"
    "\316\222",                              // "Β"
    "\343\214\274",                          // "㌼"
    "Beta",
    "\343\201\271\343\203\274\343\201\237",  // "べーた"
    "\343\203\231\343\203\274\343\202\277",  // "ベータ"
    "be-ta",
    // "ｂｅ－ｔａ"
    "\357\275\202\357\275\205\357\274\215\357\275\224\357\275\201",
    "\357\276\215\357\276\236\357\275\260\357\276\200",  // "ﾍﾞｰﾀ"
};
const int32 kValueLengths[kNumCandidates] = {
    4, 3, 4, 4, 1, 1, 1, 4, 3, 3, 5, 5, 4,
};
const int32 kIDs[kNumCandidates] = {
    0, 1, 2, 3, 4, 5, 6, 7, -1, -2, -3, -7, -11,
};

string GetStringImpl(const CompositionString &composition,
                     const DWORD offset, const DWORD length) {
  const BYTE *addr = reinterpret_cast<const BYTE*>(&composition);
  const wchar_t *string_start =
      reinterpret_cast<const wchar_t *>(addr + offset);
  const wstring wstr(string_start, string_start + length);
  string str;
  Util::WideToUTF8(wstr.c_str(), &str);
  return str;
}

BYTE GetAttributeImpl(const CompositionString &composition,
                      const DWORD offset, const DWORD length, size_t index) {
  const BYTE *addr = reinterpret_cast<const BYTE*>(&composition);
  const BYTE *attribute_start = addr + offset;
  EXPECT_LE(0, index);
  EXPECT_LT(index, length);
  return attribute_start[index];
}

#define GET_STRING(composition, field_name)                                  \
    GetStringImpl((composition), (composition).info.dw##field_name##Offset,  \
                  (composition).info.dw##field_name##Len)

#define GET_ATTRIBUTE(composition, field_name, index)                        \
    GetAttributeImpl((composition),                                          \
                     (composition).info.dw##field_name##Offset,              \
                     (composition).info.dw##field_name##Len,                 \
                     (index))

// TODO(yukawa): Make a common library for this function.
void FillOutputForSuggestion(commands::Output *output) {
  DCHECK_NE(NULL, output);
  output->Clear();

  output->set_mode(commands::HIRAGANA);
  output->set_consumed(true);
  {
    commands::Preedit *preedit = output->mutable_preedit();
    preedit->set_cursor(4);
    {
      commands::Preedit::Segment *segment = preedit->add_segment();
      segment->set_annotation(commands::Preedit::Segment::UNDERLINE);
      // "あるふぁ"
      segment->set_value("\343\201\202\343\202\213\343\201\265\343\201\201");
      segment->set_value_length(4);
      // "あるふぁ"
      segment->set_key("\343\201\202\343\202\213\343\201\265\343\201\201");
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
      // "アルファ"
      candidate->set_value("\343\202\242\343\203\253\343\203\225\343\202\241");
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
  output->set_elapsed_time(1000);
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
      // "あるふぁべーた"
      candidate->set_key("\343\201\202\343\202\213\343\201\265\343\201\201"
                         "\343\201\271\343\203\274\343\201\237");
      candidate->set_value("AlphaBeta");
    }
    {
      commands::CandidateWord *candidate = candidate_list->add_candidates();
      candidate->set_id(1);
      candidate->set_index(1);
      // "アルファ"
      candidate->set_value("\343\202\242\343\203\253\343\203\225\343\202\241");
    }
    candidate_list->set_category(commands::SUGGESTION);
  }
}

// TODO(yukawa): Make a common library for this function.
void FillOutputForPrediction(commands::Output *output) {
  DCHECK_NE(NULL, output);
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
      // "あるふぁ"
      segment->set_key("\343\201\202\343\202\213\343\201\265\343\201\201");
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
      // "アルファ"
      candidate->set_value("\343\202\242\343\203\253\343\203\225\343\202\241");
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
  output->set_elapsed_time(1000);
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
      // "あるふぁべーた"
      candidate->set_key("\343\201\202\343\202\213\343\201\265\343\201\201"
                         "\343\201\271\343\203\274\343\201\237");
      candidate->set_value("AlphaBeta");
    }
    {
      commands::CandidateWord *candidate = candidate_list->add_candidates();
      candidate->set_id(1);
      candidate->set_index(1);
      // "アルファ"
      candidate->set_value("\343\202\242\343\203\253\343\203\225\343\202\241");
    }
    candidate_list->set_category(commands::PREDICTION);
  }
}

// TODO(yukawa): Make a common library for this function.
void FillOutputForConversion(
    commands::Output *output, int focused_index, bool has_candidates) {
  DCHECK_LE(0, focused_index);
  DCHECK_GT(kNumCandidates, focused_index);
  DCHECK_NE(NULL, output);
  output->Clear();

  const int32 focused_value_length = kValueLengths[focused_index];
  const char *focused_value = kValueList[focused_index];
  const int32 focused_id = kIDs[focused_index];

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
      // "あるふぁ"
      segment->set_key("\343\201\202\343\202\213\343\201\265\343\201\201");
    }
    {
      commands::Preedit::Segment *segment = preedit->add_segment();
      segment->set_annotation(commands::Preedit::Segment::HIGHLIGHT);
      segment->set_value(focused_value);
      segment->set_value_length(focused_value_length);
      // "べーた"
      segment->set_key("\343\201\271\343\203\274\343\201\237");
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
        // "[半] アルファベット"
        annotation->set_description(
            "[\345\215\212] \343\202\242\343\203\253\343\203\225\343\202\241"
            "\343\203\231\343\203\203\343\203\210");
        annotation->set_shortcut("1");
      }
      candidate->set_id(0);
    }
    {
      commands::Candidates::Candidate *candidate = candidates->add_candidate();
      candidate->set_index(1);
      candidate->set_value("\343\203\231\343\203\274\343\202\277");
      {
        commands::Annotation *annotation = candidate->mutable_annotation();
        // "[全] カタカナ"
        annotation->set_description(
            "[\345\215\212] \343\202\253\343\202\277\343\202\253\343\203\212");
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
        // "[半] アルファベット"
        annotation->set_description(
            "[\345\215\212] \343\202\242\343\203\253\343\203\225\343\202\241"
            "\343\203\231\343\203\203\343\203\210");
        annotation->set_shortcut("3");
      }
      candidate->set_id(2);
    }
    {
      commands::Candidates::Candidate *candidate = candidates->add_candidate();
      candidate->set_index(3);
      // "β"
      candidate->set_value("\316\262");
      {
        commands::Annotation *annotation = candidate->mutable_annotation();
        // "ギリシャ文字(小文字)"
        annotation->set_description(
            "\343\202\256\343\203\252\343\202\267\343\203\243\346\226\207"
            "\345\255\227(\345\260\217\346\226\207\345\255\227)");
        annotation->set_shortcut("4");
      }
      candidate->set_id(3);
    }
    {
      commands::Candidates::Candidate *candidate = candidates->add_candidate();
      candidate->set_index(4);
      // "Β"
      candidate->set_value("\316\222");
      {
        commands::Annotation *annotation = candidate->mutable_annotation();
        // "ギリシャ文字(大文字)"
        annotation->set_description(
            "\343\202\256\343\203\252\343\202\267\343\203\243\346\226\207"
            "\345\255\227(\345\244\247\346\226\207\345\255\227)");
        annotation->set_shortcut("5");
      }
      candidate->set_id(4);
    }
    {
      commands::Candidates::Candidate *candidate = candidates->add_candidate();
      candidate->set_index(5);
      // "㌼"
      candidate->set_value("\343\214\274");
      {
        commands::Annotation *annotation = candidate->mutable_annotation();
        // "<機種依存文字>"
        annotation->set_description("<\346\251\237\347\250\256\344\276\235"
                                    "\345\255\230\346\226\207\345\255\227>");
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
        // "[半] アルファベット"
        annotation->set_description(
            "[\345\215\212] \343\202\242\343\203\253\343\203\225\343\202\241"
            "\343\203\231\343\203\203\343\203\210");
        annotation->set_shortcut("7");
      }
      candidate->set_id(6);
    }
    {
      commands::Candidates::Candidate *candidate = candidates->add_candidate();
      candidate->set_index(7);
      // "べーた"
      candidate->set_value("\343\201\271\343\203\274\343\201\237");
      {
        commands::Annotation *annotation = candidate->mutable_annotation();
        // "ひらがな"
        annotation->set_description(
            "\343\201\262\343\202\211\343\201\214\343\201\252");
        annotation->set_shortcut("8");
      }
      candidate->set_id(7);
    }
    {
      commands::Candidates::Candidate *candidate = candidates->add_candidate();
      candidate->set_index(8);
      // "そのほかの文字種"
      candidate->set_value("\343\201\235\343\201\256\343\201\273\343\201\213"
                           "\343\201\256\346\226\207\345\255\227\347\250\256");
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
          // "べーた"
          candidate->set_value("\343\201\271\343\203\274\343\201\237");
          {
            commands::Annotation *annotation = candidate->mutable_annotation();
            // "ひらがな"
            annotation->set_description(
                "\343\201\262\343\202\211\343\201\214\343\201\252");
          }
          candidate->set_id(-1);
        }
        {
          commands::Candidates::Candidate *candidate =
              sub_candidates->add_candidate();
          candidate->set_index(1);
          // "ベータ"
          candidate->set_value("\343\203\231\343\203\274\343\202\277");
          {
            commands::Annotation *annotation = candidate->mutable_annotation();
            // "[全] カタカナ"
            annotation->set_description(
                "[\345\215\212] "
                "\343\202\253\343\202\277\343\202\253\343\203\212");
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
            // "[半]"
            annotation->set_description("[\345\215\212]");
          }
          candidate->set_id(-3);
        }
        {
          commands::Candidates::Candidate *candidate =
              sub_candidates->add_candidate();
          candidate->set_index(3);
          // "ｂｅ－ｔａ"
          candidate->set_value(
              "\357\275\202\357\275\205\357\274\215\357\275\224\357\275\201");
          {
            commands::Annotation *annotation = candidate->mutable_annotation();
            // "[全]"
            annotation->set_description("[\345\205\250]");
          }
          candidate->set_id(-7);
        }
        {
          commands::Candidates::Candidate *candidate =
              sub_candidates->add_candidate();
          candidate->set_index(4);
          // "ﾍﾞｰﾀ"
          candidate->set_value(
              "\357\276\215\357\276\236\357\275\260\357\276\200");
          {
            commands::Annotation *annotation = candidate->mutable_annotation();
            // "[半] カタカナ"
            annotation->set_description(
                "[\345\215\212] "
                "\343\202\253\343\202\277\343\202\253\343\203\212");
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
  output->set_elapsed_time(1000);
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
}  // anonymous namespace

TEST(ImeCompositionStringTest, StartCompositionTest) {
  CompositionString compstr;
  EXPECT_TRUE(compstr.Initialize());

  commands::Output output;
  commands::Preedit *preedit = output.mutable_preedit();

  commands::Preedit::Segment *segment = preedit->add_segment();
  preedit->set_cursor(1);

  segment->set_annotation(commands::Preedit::Segment::UNDERLINE);
  // "が"
  segment->set_key("\xE3\x81\x8C");
  // "が"
  segment->set_value("\xE3\x81\x8C");
  segment->set_value_length(1);

  vector<UIMessage> messages;
  EXPECT_TRUE(compstr.Update(output, &messages));
  EXPECT_EQ(2, messages.size());

  EXPECT_EQ(WM_IME_STARTCOMPOSITION, messages[0].message());
  EXPECT_EQ(0, messages[0].wparam());
  EXPECT_EQ(0, messages[0].lparam());

  EXPECT_EQ(WM_IME_COMPOSITION, messages[1].message());
  EXPECT_EQ(0, messages[1].wparam());
  // When the preedit is updated, the following flags should be sent
  // regardless of which field is actually updated. Otherwise, some
  // applications such as wordpad OOo Writer 3.0 will not update composition
  // window and caret state properly.
  EXPECT_EQ((GCS_COMPREADSTR | GCS_COMPREADATTR | GCS_COMPREADCLAUSE |
             GCS_COMPSTR | GCS_COMPATTR | GCS_COMPCLAUSE | GCS_CURSORPOS |
             GCS_DELTASTART),
            messages[1].lparam());

  EXPECT_EQ(0, compstr.focused_character_index_);
  EXPECT_EQ(2, compstr.info.dwCompReadAttrLen);
  EXPECT_EQ(8, compstr.info.dwCompReadClauseLen);
  EXPECT_EQ(2, compstr.info.dwCompReadStrLen);
  EXPECT_EQ(1, compstr.info.dwCompAttrLen);
  EXPECT_EQ(8, compstr.info.dwCompClauseLen);
  EXPECT_EQ(1, compstr.info.dwCompStrLen);
  EXPECT_EQ(1, compstr.info.dwCursorPos);
  EXPECT_EQ(0, compstr.info.dwDeltaStart);
  EXPECT_EQ(0, compstr.info.dwResultReadClauseLen);
  EXPECT_EQ(0, compstr.info.dwResultReadStrLen);
  EXPECT_EQ(0, compstr.info.dwResultClauseLen);
  EXPECT_EQ(0, compstr.info.dwResultStrLen);

  // "ｶﾞ"
  EXPECT_EQ("\xEF\xBD\xB6\xEF\xBE\x9E", GET_STRING(compstr, CompReadStr));

  EXPECT_EQ(ATTR_INPUT, GET_ATTRIBUTE(compstr, CompReadAttr, 0));
  EXPECT_EQ(ATTR_INPUT, GET_ATTRIBUTE(compstr, CompReadAttr, 1));

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
  output.set_elapsed_time(1000);
  output.set_mode(mozc::commands::HIRAGANA);
  output.mutable_status()->set_activated(true);
  output.mutable_status()->set_mode(mozc::commands::HIRAGANA);

  vector<UIMessage> messages;
  EXPECT_TRUE(compstr.Update(output, &messages));
  EXPECT_EQ(2, messages.size());

  EXPECT_EQ(WM_IME_COMPOSITION, messages[0].message());
  EXPECT_EQ(0, messages[0].wparam());
  EXPECT_EQ(0, messages[0].lparam());

  EXPECT_EQ(WM_IME_ENDCOMPOSITION, messages[1].message());
  EXPECT_EQ(0, messages[1].wparam());
  EXPECT_EQ(0, messages[1].lparam());

  EXPECT_EQ(0, compstr.focused_character_index_);
  EXPECT_EQ(0, compstr.info.dwCompReadAttrLen);
  EXPECT_EQ(0, compstr.info.dwCompReadClauseLen);
  EXPECT_EQ(0, compstr.info.dwCompReadStrLen);
  EXPECT_EQ(0, compstr.info.dwCompAttrLen);
  EXPECT_EQ(0, compstr.info.dwCompClauseLen);
  EXPECT_EQ(0, compstr.info.dwCompStrLen);
  EXPECT_EQ(-1, compstr.info.dwCursorPos);
  EXPECT_EQ(0, compstr.info.dwDeltaStart);
  EXPECT_EQ(0, compstr.info.dwResultReadClauseLen);
  EXPECT_EQ(0, compstr.info.dwResultReadStrLen);
  EXPECT_EQ(0, compstr.info.dwResultClauseLen);
  EXPECT_EQ(0, compstr.info.dwResultStrLen);
}

TEST(ImeCompositionStringTest, EndCompositionWhenCompositionIsCommited) {
  // WM_IME_COMPOSITION should be sent up to once.
  // Otherwise, the result string will be commited twice in wordpad.exe.

  CompositionString compstr;
  EXPECT_TRUE(compstr.Initialize());
  compstr.info.dwCompStrLen = 1;
  compstr.info.dwCompReadStrLen = 1;

  commands::Output output;
  output.set_consumed(true);
  output.set_elapsed_time(1000);
  output.set_mode(mozc::commands::HIRAGANA);
  output.mutable_status()->set_activated(true);
  output.mutable_status()->set_mode(mozc::commands::HIRAGANA);

  commands::Result *result = output.mutable_result();

  // "が"
  result->set_key("\xE3\x81\x8C");
  // "が"
  result->set_value("\xE3\x81\x8C");
  result->set_type(mozc::commands::Result::STRING);

  vector<UIMessage> messages;
  EXPECT_TRUE(compstr.Update(output, &messages));
  EXPECT_EQ(2, messages.size());

  EXPECT_EQ(WM_IME_COMPOSITION, messages[0].message());
  EXPECT_EQ(0, messages[0].wparam());
  // When the result string is updated, the following flags should be sent
  // regardless of which field is actually updated. Otherwise, some
  // applications such as wordpad OOo Writer 3.0 will not update composition
  // window and caret state properly.
  EXPECT_EQ((GCS_RESULTREADSTR | GCS_RESULTREADCLAUSE | GCS_RESULTSTR |
             GCS_RESULTCLAUSE),
            messages[0].lparam());

  EXPECT_EQ(WM_IME_ENDCOMPOSITION, messages[1].message());
  EXPECT_EQ(0, messages[1].wparam());
  EXPECT_EQ(0, messages[1].lparam());

  EXPECT_EQ(0, compstr.focused_character_index_);
  EXPECT_EQ(0, compstr.info.dwCompReadAttrLen);
  EXPECT_EQ(0, compstr.info.dwCompReadClauseLen);
  EXPECT_EQ(0, compstr.info.dwCompReadStrLen);
  EXPECT_EQ(0, compstr.info.dwCompAttrLen);
  EXPECT_EQ(0, compstr.info.dwCompClauseLen);
  EXPECT_EQ(0, compstr.info.dwCompStrLen);
  EXPECT_EQ(-1, compstr.info.dwCursorPos);
  EXPECT_EQ(0, compstr.info.dwDeltaStart);
  EXPECT_EQ(8, compstr.info.dwResultReadClauseLen);
  EXPECT_EQ(2, compstr.info.dwResultReadStrLen);
  EXPECT_EQ(8, compstr.info.dwResultClauseLen);
  EXPECT_EQ(1, compstr.info.dwResultStrLen);

  // "ｶﾞ"
  EXPECT_EQ("\xEF\xBD\xB6\xEF\xBE\x9E", GET_STRING(compstr, ResultReadStr));
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
  output.mutable_result()->set_value("\343\200\200");  // Full-width space
  output.mutable_result()->set_type(mozc::commands::Result::STRING);
  output.set_elapsed_time(1000);
  output.mutable_status()->set_activated(true);
  output.mutable_status()->set_mode(mozc::commands::HIRAGANA);
  output.set_performed_command("Precomposition_InsertSpace");

  vector<UIMessage> messages;
  EXPECT_TRUE(compstr.Update(output, &messages));
  EXPECT_EQ(3, messages.size());

  EXPECT_EQ(WM_IME_STARTCOMPOSITION, messages[0].message());
  EXPECT_EQ(0, messages[0].wparam());
  EXPECT_EQ(0, messages[0].lparam());

  EXPECT_EQ(WM_IME_COMPOSITION, messages[1].message());
  EXPECT_EQ(0, messages[1].wparam());
  EXPECT_EQ((GCS_RESULTREADSTR | GCS_RESULTREADCLAUSE | GCS_RESULTSTR |
             GCS_RESULTCLAUSE),
            messages[1].lparam());

  EXPECT_EQ(WM_IME_ENDCOMPOSITION, messages[2].message());
  EXPECT_EQ(0, messages[2].wparam());
  EXPECT_EQ(0, messages[2].lparam());

  EXPECT_EQ(0, compstr.focused_character_index_);
  EXPECT_EQ(0, compstr.info.dwCompReadAttrLen);
  EXPECT_EQ(0, compstr.info.dwCompReadClauseLen);
  EXPECT_EQ(0, compstr.info.dwCompReadStrLen);
  EXPECT_EQ(0, compstr.info.dwCompAttrLen);
  EXPECT_EQ(0, compstr.info.dwCompClauseLen);
  EXPECT_EQ(0, compstr.info.dwCompStrLen);
  EXPECT_EQ(-1, compstr.info.dwCursorPos);
  EXPECT_EQ(0, compstr.info.dwDeltaStart);
  EXPECT_EQ(8, compstr.info.dwResultReadClauseLen);
  EXPECT_EQ(1, compstr.info.dwResultReadStrLen);
  EXPECT_EQ(8, compstr.info.dwResultClauseLen);
  EXPECT_EQ(1, compstr.info.dwResultStrLen);

  // "　"
  EXPECT_EQ("\343\200\200", GET_STRING(compstr, ResultStr));
  // " "
  EXPECT_EQ(" ", GET_STRING(compstr, ResultReadStr));
}

TEST(ImeCompositionStringTest,
     EndCompositionWhenCompositionIsCommitedWithPreedit) {
  // WM_IME_COMPOSITION should be sent up to once.
  // Otherwise, the result string will be commited twice in wordpad.exe.

  CompositionString compstr;
  EXPECT_TRUE(compstr.Initialize());
  compstr.info.dwCompStrLen = 1;
  compstr.info.dwCompReadStrLen = 1;

  commands::Output output;
  output.set_consumed(true);
  output.set_elapsed_time(1000);
  output.set_mode(mozc::commands::HIRAGANA);
  output.mutable_status()->set_activated(true);
  output.mutable_status()->set_mode(mozc::commands::HIRAGANA);

  commands::Result *result = output.mutable_result();

  // "が"
  result->set_key("\xE3\x81\x8C");
  // "が"
  result->set_value("\xE3\x81\x8C");
  result->set_type(mozc::commands::Result::STRING);

  commands::Preedit *preedit = output.mutable_preedit();
  preedit->set_cursor(1);

  commands::Preedit::Segment *segment = preedit->add_segment();
  segment->set_annotation(commands::Preedit::Segment::UNDERLINE);
  // "が"
  segment->set_key("\xE3\x81\x8C");
  // "が"
  segment->set_value("\xE3\x81\x8C");
  segment->set_value_length(1);

  vector<UIMessage> messages;
  EXPECT_TRUE(compstr.Update(output, &messages));
  EXPECT_EQ(1, messages.size());

  EXPECT_EQ(WM_IME_COMPOSITION, messages[0].message());
  EXPECT_EQ(0, messages[0].wparam());
  // When both the preedit and result string is updated, the following flags
  // should be sent regardless of which field is actually updated. Otherwise,
  // some applications such as wordpad OOo Writer 3.0 will not update
  // composition window and caret state properly.
  EXPECT_EQ((GCS_COMPREADSTR | GCS_COMPREADATTR | GCS_COMPREADCLAUSE |
             GCS_COMPSTR | GCS_COMPATTR | GCS_COMPCLAUSE | GCS_CURSORPOS |
             GCS_DELTASTART) |
            (GCS_RESULTREADSTR | GCS_RESULTREADCLAUSE | GCS_RESULTSTR |
             GCS_RESULTCLAUSE),
            messages[0].lparam());

  EXPECT_EQ(0, compstr.focused_character_index_);
  EXPECT_EQ(2, compstr.info.dwCompReadAttrLen);
  EXPECT_EQ(8, compstr.info.dwCompReadClauseLen);
  EXPECT_EQ(2, compstr.info.dwCompReadStrLen);
  EXPECT_EQ(1, compstr.info.dwCompAttrLen);
  EXPECT_EQ(8, compstr.info.dwCompClauseLen);
  EXPECT_EQ(1, compstr.info.dwCompStrLen);
  EXPECT_EQ(1, compstr.info.dwCursorPos);
  EXPECT_EQ(0, compstr.info.dwDeltaStart);
  EXPECT_EQ(8, compstr.info.dwResultReadClauseLen);
  EXPECT_EQ(2, compstr.info.dwResultReadStrLen);
  EXPECT_EQ(8, compstr.info.dwResultClauseLen);
  EXPECT_EQ(1, compstr.info.dwResultStrLen);

  // "ｶﾞ"
  EXPECT_EQ("\xEF\xBD\xB6\xEF\xBE\x9E", GET_STRING(compstr, CompReadStr));
  // "ｶﾞ"
  EXPECT_EQ("\xEF\xBD\xB6\xEF\xBE\x9E", GET_STRING(compstr, ResultReadStr));

  EXPECT_EQ(ATTR_INPUT, GET_ATTRIBUTE(compstr, CompReadAttr, 0));
  EXPECT_EQ(ATTR_INPUT, GET_ATTRIBUTE(compstr, CompReadAttr, 1));
}

TEST(ImeCompositionStringTest, Suggest) {
  commands::Output output;
  FillOutputForSuggestion(&output);

  CompositionString compstr;
  EXPECT_TRUE(compstr.Initialize());

  vector<UIMessage> messages;
  EXPECT_TRUE(compstr.Update(output, &messages));
  EXPECT_EQ(2, messages.size());

  EXPECT_EQ(WM_IME_STARTCOMPOSITION, messages[0].message());
  EXPECT_EQ(0, messages[0].wparam());
  EXPECT_EQ(0, messages[0].lparam());

  EXPECT_EQ(WM_IME_COMPOSITION, messages[1].message());
  EXPECT_EQ(0, messages[1].wparam());
  // When the preedit is updated, the following flags should be sent
  // regardless of which field is actually updated. Otherwise, some
  // applications such as wordpad OOo Writer 3.0 will not update composition
  // window and caret state properly.
  EXPECT_EQ((GCS_COMPREADSTR | GCS_COMPREADATTR | GCS_COMPREADCLAUSE |
             GCS_COMPSTR | GCS_COMPATTR | GCS_COMPCLAUSE | GCS_CURSORPOS |
             GCS_DELTASTART),
            messages[1].lparam());

  EXPECT_EQ(0, compstr.focused_character_index_);
  EXPECT_EQ(4, compstr.info.dwCompReadAttrLen);
  EXPECT_EQ(8, compstr.info.dwCompReadClauseLen);
  EXPECT_EQ(4, compstr.info.dwCompReadStrLen);
  EXPECT_EQ(4, compstr.info.dwCompAttrLen);
  EXPECT_EQ(8, compstr.info.dwCompClauseLen);
  EXPECT_EQ(4, compstr.info.dwCompStrLen);
  EXPECT_EQ(4, compstr.info.dwCursorPos);
  EXPECT_EQ(0, compstr.info.dwDeltaStart);
  EXPECT_EQ(0, compstr.info.dwResultReadClauseLen);
  EXPECT_EQ(0, compstr.info.dwResultReadStrLen);
  EXPECT_EQ(0, compstr.info.dwResultClauseLen);
  EXPECT_EQ(0, compstr.info.dwResultStrLen);

  // "ｱﾙﾌｧ"
  EXPECT_EQ("\357\275\261\357\276\231\357\276\214\357\275\247",
            GET_STRING(compstr, CompReadStr));
  EXPECT_EQ("\343\201\202\343\202\213\343\201\265\343\201\201",
            GET_STRING(compstr, CompStr));

  EXPECT_EQ(ATTR_INPUT, GET_ATTRIBUTE(compstr, CompReadAttr, 0));
  EXPECT_EQ(ATTR_INPUT, GET_ATTRIBUTE(compstr, CompReadAttr, 1));
  EXPECT_EQ(ATTR_INPUT, GET_ATTRIBUTE(compstr, CompReadAttr, 2));
  EXPECT_EQ(ATTR_INPUT, GET_ATTRIBUTE(compstr, CompReadAttr, 3));

  EXPECT_EQ(ATTR_INPUT, GET_ATTRIBUTE(compstr, CompAttr, 0));
  EXPECT_EQ(ATTR_INPUT, GET_ATTRIBUTE(compstr, CompAttr, 1));
  EXPECT_EQ(ATTR_INPUT, GET_ATTRIBUTE(compstr, CompAttr, 2));
  EXPECT_EQ(ATTR_INPUT, GET_ATTRIBUTE(compstr, CompAttr, 3));
}

TEST(ImeCompositionStringTest, Predict) {
  commands::Output output;
  FillOutputForPrediction(&output);

  CompositionString compstr;
  EXPECT_TRUE(compstr.Initialize());

  vector<UIMessage> messages;
  EXPECT_TRUE(compstr.Update(output, &messages));
  EXPECT_EQ(2, messages.size());

  EXPECT_EQ(WM_IME_STARTCOMPOSITION, messages[0].message());
  EXPECT_EQ(0, messages[0].wparam());
  EXPECT_EQ(0, messages[0].lparam());

  EXPECT_EQ(WM_IME_COMPOSITION, messages[1].message());
  EXPECT_EQ(0, messages[1].wparam());
  // When the preedit is updated, the following flags should be sent
  // regardless of which field is actually updated. Otherwise, some
  // applications such as wordpad OOo Writer 3.0 will not update composition
  // window and caret state properly.
  EXPECT_EQ((GCS_COMPREADSTR | GCS_COMPREADATTR | GCS_COMPREADCLAUSE |
             GCS_COMPSTR | GCS_COMPATTR | GCS_COMPCLAUSE | GCS_CURSORPOS |
             GCS_DELTASTART),
            messages[1].lparam());

  EXPECT_EQ(0, compstr.focused_character_index_);
  EXPECT_EQ(4, compstr.info.dwCompReadAttrLen);
  EXPECT_EQ(8, compstr.info.dwCompReadClauseLen);
  EXPECT_EQ(4, compstr.info.dwCompReadStrLen);
  EXPECT_EQ(9, compstr.info.dwCompAttrLen);
  EXPECT_EQ(8, compstr.info.dwCompClauseLen);
  EXPECT_EQ(9, compstr.info.dwCompStrLen);
  EXPECT_EQ(9, compstr.info.dwCursorPos);
  EXPECT_EQ(0, compstr.info.dwDeltaStart);
  EXPECT_EQ(0, compstr.info.dwResultReadClauseLen);
  EXPECT_EQ(0, compstr.info.dwResultReadStrLen);
  EXPECT_EQ(0, compstr.info.dwResultClauseLen);
  EXPECT_EQ(0, compstr.info.dwResultStrLen);

  // "ｱﾙﾌｧ"
  EXPECT_EQ("\357\275\261\357\276\231\357\276\214\357\275\247",
            GET_STRING(compstr, CompReadStr));
  EXPECT_EQ("AlphaBeta",  GET_STRING(compstr, CompStr));

  EXPECT_EQ(ATTR_TARGET_CONVERTED, GET_ATTRIBUTE(compstr, CompReadAttr, 0));
  EXPECT_EQ(ATTR_TARGET_CONVERTED, GET_ATTRIBUTE(compstr, CompReadAttr, 1));
  EXPECT_EQ(ATTR_TARGET_CONVERTED, GET_ATTRIBUTE(compstr, CompReadAttr, 2));
  EXPECT_EQ(ATTR_TARGET_CONVERTED, GET_ATTRIBUTE(compstr, CompReadAttr, 3));

  EXPECT_EQ(ATTR_TARGET_CONVERTED, GET_ATTRIBUTE(compstr, CompAttr, 0));
  EXPECT_EQ(ATTR_TARGET_CONVERTED, GET_ATTRIBUTE(compstr, CompAttr, 1));
  EXPECT_EQ(ATTR_TARGET_CONVERTED, GET_ATTRIBUTE(compstr, CompAttr, 2));
  EXPECT_EQ(ATTR_TARGET_CONVERTED, GET_ATTRIBUTE(compstr, CompAttr, 3));
  EXPECT_EQ(ATTR_TARGET_CONVERTED, GET_ATTRIBUTE(compstr, CompAttr, 4));
  EXPECT_EQ(ATTR_TARGET_CONVERTED, GET_ATTRIBUTE(compstr, CompAttr, 5));
  EXPECT_EQ(ATTR_TARGET_CONVERTED, GET_ATTRIBUTE(compstr, CompAttr, 6));
  EXPECT_EQ(ATTR_TARGET_CONVERTED, GET_ATTRIBUTE(compstr, CompAttr, 7));
  EXPECT_EQ(ATTR_TARGET_CONVERTED, GET_ATTRIBUTE(compstr, CompAttr, 8));
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
    vector<UIMessage> messages;
    EXPECT_TRUE(compstr.Update(output, &messages));
    EXPECT_EQ(2, messages.size());

    EXPECT_EQ(WM_IME_STARTCOMPOSITION, messages[0].message());
    EXPECT_EQ(0, messages[0].wparam());
    EXPECT_EQ(0, messages[0].lparam());

    EXPECT_EQ(WM_IME_COMPOSITION, messages[1].message());
    EXPECT_EQ(0, messages[1].wparam());
    // When the preedit is updated, the following flags should be sent
    // regardless of which field is actually updated. Otherwise, some
    // applications such as wordpad OOo Writer 3.0 will not update composition
    // window and caret state properly.
    EXPECT_EQ((GCS_COMPREADSTR | GCS_COMPREADATTR | GCS_COMPREADCLAUSE |
               GCS_COMPSTR | GCS_COMPATTR | GCS_COMPCLAUSE | GCS_CURSORPOS |
               GCS_DELTASTART),
              messages[1].lparam());

    EXPECT_EQ(5, compstr.focused_character_index_);
    EXPECT_EQ(8, compstr.info.dwCompReadAttrLen);
    EXPECT_EQ(12, compstr.info.dwCompReadClauseLen);
    EXPECT_EQ(8, compstr.info.dwCompReadStrLen);
    EXPECT_EQ(9, compstr.info.dwCompAttrLen);
    EXPECT_EQ(12, compstr.info.dwCompClauseLen);
    EXPECT_EQ(9, compstr.info.dwCompStrLen);
    EXPECT_EQ(5, compstr.info.dwCursorPos);
    EXPECT_EQ(0, compstr.info.dwDeltaStart);
    EXPECT_EQ(0, compstr.info.dwResultReadClauseLen);
    EXPECT_EQ(0, compstr.info.dwResultReadStrLen);
    EXPECT_EQ(0, compstr.info.dwResultClauseLen);
    EXPECT_EQ(0, compstr.info.dwResultStrLen);

    // "ｱﾙﾌｧﾍﾞｰﾀ"
    EXPECT_EQ("\357\275\261\357\276\231\357\276\214\357\275\247"
              "\357\276\215\357\276\236\357\275\260\357\276\200",
              GET_STRING(compstr, CompReadStr));
    EXPECT_EQ("AlphaBeta",  GET_STRING(compstr, CompStr));

    EXPECT_EQ(ATTR_CONVERTED, GET_ATTRIBUTE(compstr, CompReadAttr, 0));
    EXPECT_EQ(ATTR_CONVERTED, GET_ATTRIBUTE(compstr, CompReadAttr, 1));
    EXPECT_EQ(ATTR_CONVERTED, GET_ATTRIBUTE(compstr, CompReadAttr, 2));
    EXPECT_EQ(ATTR_CONVERTED, GET_ATTRIBUTE(compstr, CompReadAttr, 3));
    EXPECT_EQ(ATTR_TARGET_CONVERTED, GET_ATTRIBUTE(compstr, CompReadAttr, 4));
    EXPECT_EQ(ATTR_TARGET_CONVERTED, GET_ATTRIBUTE(compstr, CompReadAttr, 5));
    EXPECT_EQ(ATTR_TARGET_CONVERTED, GET_ATTRIBUTE(compstr, CompReadAttr, 6));
    EXPECT_EQ(ATTR_TARGET_CONVERTED, GET_ATTRIBUTE(compstr, CompReadAttr, 7));

    EXPECT_EQ(ATTR_CONVERTED, GET_ATTRIBUTE(compstr, CompAttr, 0));
    EXPECT_EQ(ATTR_CONVERTED, GET_ATTRIBUTE(compstr, CompAttr, 1));
    EXPECT_EQ(ATTR_CONVERTED, GET_ATTRIBUTE(compstr, CompAttr, 2));
    EXPECT_EQ(ATTR_CONVERTED, GET_ATTRIBUTE(compstr, CompAttr, 3));
    EXPECT_EQ(ATTR_CONVERTED, GET_ATTRIBUTE(compstr, CompAttr, 4));
    EXPECT_EQ(ATTR_TARGET_CONVERTED, GET_ATTRIBUTE(compstr, CompAttr, 5));
    EXPECT_EQ(ATTR_TARGET_CONVERTED, GET_ATTRIBUTE(compstr, CompAttr, 6));
    EXPECT_EQ(ATTR_TARGET_CONVERTED, GET_ATTRIBUTE(compstr, CompAttr, 7));
    EXPECT_EQ(ATTR_TARGET_CONVERTED, GET_ATTRIBUTE(compstr, CompAttr, 8));
  }

  // Second conversion
  FillOutputForConversion(&output, 1, true);
  {
    vector<UIMessage> messages;
    EXPECT_TRUE(compstr.Update(output, &messages));
    EXPECT_EQ(1, messages.size());

    EXPECT_EQ(WM_IME_COMPOSITION, messages[0].message());
    EXPECT_EQ(0, messages[0].wparam());
    // When the preedit is updated, the following flags should be sent
    // regardless of which field is actually updated. Otherwise, some
    // applications such as wordpad OOo Writer 3.0 will not update composition
    // window and caret state properly.
    EXPECT_EQ((GCS_COMPREADSTR | GCS_COMPREADATTR | GCS_COMPREADCLAUSE |
               GCS_COMPSTR | GCS_COMPATTR | GCS_COMPCLAUSE | GCS_CURSORPOS |
               GCS_DELTASTART),
              messages[0].lparam());

    EXPECT_EQ(5, compstr.focused_character_index_);
    EXPECT_EQ(8, compstr.info.dwCompReadAttrLen);
    EXPECT_EQ(12, compstr.info.dwCompReadClauseLen);
    EXPECT_EQ(8, compstr.info.dwCompReadStrLen);
    EXPECT_EQ(8, compstr.info.dwCompAttrLen);
    EXPECT_EQ(12, compstr.info.dwCompClauseLen);
    EXPECT_EQ(8, compstr.info.dwCompStrLen);
    EXPECT_EQ(5, compstr.info.dwCursorPos);
    EXPECT_EQ(0, compstr.info.dwDeltaStart);
    EXPECT_EQ(0, compstr.info.dwResultReadClauseLen);
    EXPECT_EQ(0, compstr.info.dwResultReadStrLen);
    EXPECT_EQ(0, compstr.info.dwResultClauseLen);
    EXPECT_EQ(0, compstr.info.dwResultStrLen);

    // "ｱﾙﾌｧﾍﾞｰﾀ"
    EXPECT_EQ("\357\275\261\357\276\231\357\276\214\357\275\247"
              "\357\276\215\357\276\236\357\275\260\357\276\200",
              GET_STRING(compstr, CompReadStr));
    // "Alphaベータ"
    EXPECT_EQ("Alpha\343\203\231\343\203\274\343\202\277",
              GET_STRING(compstr, CompStr));

    EXPECT_EQ(ATTR_CONVERTED, GET_ATTRIBUTE(compstr, CompReadAttr, 0));
    EXPECT_EQ(ATTR_CONVERTED, GET_ATTRIBUTE(compstr, CompReadAttr, 1));
    EXPECT_EQ(ATTR_CONVERTED, GET_ATTRIBUTE(compstr, CompReadAttr, 2));
    EXPECT_EQ(ATTR_CONVERTED, GET_ATTRIBUTE(compstr, CompReadAttr, 3));
    EXPECT_EQ(ATTR_TARGET_CONVERTED, GET_ATTRIBUTE(compstr, CompReadAttr, 4));
    EXPECT_EQ(ATTR_TARGET_CONVERTED, GET_ATTRIBUTE(compstr, CompReadAttr, 5));
    EXPECT_EQ(ATTR_TARGET_CONVERTED, GET_ATTRIBUTE(compstr, CompReadAttr, 6));
    EXPECT_EQ(ATTR_TARGET_CONVERTED, GET_ATTRIBUTE(compstr, CompReadAttr, 7));

    EXPECT_EQ(ATTR_CONVERTED, GET_ATTRIBUTE(compstr, CompAttr, 0));
    EXPECT_EQ(ATTR_CONVERTED, GET_ATTRIBUTE(compstr, CompAttr, 1));
    EXPECT_EQ(ATTR_CONVERTED, GET_ATTRIBUTE(compstr, CompAttr, 2));
    EXPECT_EQ(ATTR_CONVERTED, GET_ATTRIBUTE(compstr, CompAttr, 3));
    EXPECT_EQ(ATTR_CONVERTED, GET_ATTRIBUTE(compstr, CompAttr, 4));
    EXPECT_EQ(ATTR_TARGET_CONVERTED, GET_ATTRIBUTE(compstr, CompAttr, 5));
    EXPECT_EQ(ATTR_TARGET_CONVERTED, GET_ATTRIBUTE(compstr, CompAttr, 6));
    EXPECT_EQ(ATTR_TARGET_CONVERTED, GET_ATTRIBUTE(compstr, CompAttr, 7));
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
      // "𠮟る" (U+20B9F)
      segment->set_value("\360\240\256\237\343\202\213");
      segment->set_value_length(2);
      // "しかる"
      segment->set_key("\343\201\227\343\201\213\343\202\213");
    }
    {
      commands::Preedit::Segment *segment = preedit->add_segment();
      segment->set_annotation(commands::Preedit::Segment::UNDERLINE);
      // "と"
      segment->set_value("\343\201\250");
      segment->set_value_length(1);
      // "と"
      segment->set_key("\343\201\250");
    }
    {
      commands::Preedit::Segment *segment = preedit->add_segment();
      segment->set_annotation(commands::Preedit::Segment::HIGHLIGHT);
      // "叱る" (U+53F1)
      segment->set_value("\345\217\261\343\202\213");
      segment->set_value_length(2);
      // "しかる"
      segment->set_key("\343\201\227\343\201\213\343\202\213");
    }
    preedit->set_highlighted_position(3);
  }
  {
    commands::Status *status = output.mutable_status();
    status->set_activated(true);
    status->set_mode(commands::HIRAGANA);
  }

  vector<UIMessage> messages;
  EXPECT_TRUE(compstr.Update(output, &messages));

  // Here, |focused_character_index_| != Preedit::highlighted_position()
  // because of surrogate parir.
  EXPECT_EQ(4, compstr.focused_character_index_);
  EXPECT_EQ(7, compstr.info.dwCompReadAttrLen);
  EXPECT_EQ(16, compstr.info.dwCompReadClauseLen);
  EXPECT_EQ(7, compstr.info.dwCompReadStrLen);
  EXPECT_EQ(6, compstr.info.dwCompAttrLen);
  EXPECT_EQ(16, compstr.info.dwCompClauseLen);
  EXPECT_EQ(6, compstr.info.dwCompStrLen);
  EXPECT_EQ(4, compstr.info.dwCursorPos);
  EXPECT_EQ(0, compstr.info.dwDeltaStart);
  EXPECT_EQ(0, compstr.info.dwResultReadClauseLen);
  EXPECT_EQ(0, compstr.info.dwResultReadStrLen);
  EXPECT_EQ(0, compstr.info.dwResultClauseLen);
  EXPECT_EQ(0, compstr.info.dwResultStrLen);

  // "ｼｶﾙﾄｼｶﾙ"
  EXPECT_EQ("\357\275\274\357\275\266\357\276\231"
            "\357\276\204"
            "\357\275\274\357\275\266\357\276\231",
            GET_STRING(compstr, CompReadStr));

  EXPECT_EQ(ATTR_CONVERTED, GET_ATTRIBUTE(compstr, CompReadAttr, 0));
  EXPECT_EQ(ATTR_CONVERTED, GET_ATTRIBUTE(compstr, CompReadAttr, 1));
  EXPECT_EQ(ATTR_CONVERTED, GET_ATTRIBUTE(compstr, CompReadAttr, 2));

  // TODO(yukawa): Check other fields in |compstr.info|
}
}  // namespace win32
}  // namespace mozc
