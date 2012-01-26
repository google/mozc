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

#include "session/commands.pb.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "win32/ime/output_util.h"

namespace mozc {
namespace {
// TODO(yukawa): Add a deserialize method to OutputUtil so that we can use
//   text representation of the commands::Output to set up test data.
void SetTestDataForConversion(commands::Output *output) {
  output->set_mode(commands::HIRAGANA);
  output->set_consumed(true);
  {
    commands::Preedit *preedit = output->mutable_preedit();
    preedit->set_cursor(10);
    {
      commands::Preedit::Segment *segment = preedit->add_segment();
      segment->set_annotation(commands::Preedit::Segment::UNDERLINE);
      segment->set_value("Alpha");
      segment->set_value_length(5);
      // "あるふぁ"
      segment->set_key("\343\201\202\343\202\213\343\201\265\343\201\201");
    }
    {
      commands::Preedit::Segment *segment = preedit->add_segment();
      segment->set_annotation(commands::Preedit::Segment::HIGHLIGHT);
      segment->set_value("be-ta");
      segment->set_value_length(5);
      // "べーた"
      segment->set_key("\343\201\271\343\203\274\343\201\237");
    }
    preedit->set_highlighted_position(5);
  }
  {
    commands::Candidates *candidates = output->mutable_candidates();
    candidates->set_focused_index(8);
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
      // "ベータ"
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
    candidate_list->set_focused_index(10);
    {
      commands::CandidateWord *candidate = candidate_list->add_candidates();
      candidate->set_id(0);
      candidate->set_index(0);
      candidate->set_value("Beta");
    }
    {
      commands::CandidateWord *candidate = candidate_list->add_candidates();
      candidate->set_id(1);
      candidate->set_index(1);
      // "ベータ"
      candidate->set_value("\343\203\231\343\203\274\343\202\277");
    }
    {
      commands::CandidateWord *candidate = candidate_list->add_candidates();
      candidate->set_id(2);
      candidate->set_index(2);
      candidate->set_value("BETA");
    }
    {
      commands::CandidateWord *candidate = candidate_list->add_candidates();
      candidate->set_id(3);
      candidate->set_index(3);
      candidate->set_value("beta");
    }
    {
      commands::CandidateWord *candidate = candidate_list->add_candidates();
      candidate->set_id(4);
      candidate->set_index(4);
      // "β"
      candidate->set_value("\316\262");
    }
    {
      commands::CandidateWord *candidate = candidate_list->add_candidates();
      candidate->set_id(5);
      candidate->set_index(5);
      // "Β"
      candidate->set_value("\316\222");
    }
    {
      commands::CandidateWord *candidate = candidate_list->add_candidates();
      candidate->set_id(6);
      candidate->set_index(6);
      // "㌼"
      candidate->set_value("\343\214\274");
    }
    {
      commands::CandidateWord *candidate = candidate_list->add_candidates();
      candidate->set_id(7);
      candidate->set_index(7);
      // "べーた"
      candidate->set_value("\343\201\271\343\203\274\343\201\237");
    }
    {
      commands::CandidateWord *candidate = candidate_list->add_candidates();
      candidate->set_id(-1);
      candidate->set_index(8);
      // "べーた"
      candidate->set_value("\343\201\271\343\203\274\343\201\237");
    }
    {
      commands::CandidateWord *candidate = candidate_list->add_candidates();
      candidate->set_id(-2);
      candidate->set_index(9);
      // "ベータ"
      candidate->set_value("\343\203\231\343\203\274\343\202\277");
    }
    {
      commands::CandidateWord *candidate = candidate_list->add_candidates();
      candidate->set_id(-3);
      candidate->set_index(10);
      candidate->set_value("be-ta");
    }
    {
      commands::CandidateWord *candidate = candidate_list->add_candidates();
      candidate->set_id(-7);
      candidate->set_index(11);
      // "ｂｅ－ｔａ"
      candidate->set_value(
          "\357\275\202\357\275\205\357\274\215\357\275\224\357\275\201");
    }
    {
      commands::CandidateWord *candidate = candidate_list->add_candidates();
      candidate->set_id(-11);
      candidate->set_index(12);
      // "ﾍﾞｰﾀ"
      candidate->set_value("\357\276\215\357\276\236\357\275\260\357\276\200");
    }
    candidate_list->set_category(commands::CONVERSION);
  }
}
}  // anonymous namespace

TEST(OutputUtilTest, GetCandidateIndexById) {
  commands::Output output;
  SetTestDataForConversion(&output);

  // Existing ID
  int32 candidate_index = 0;
  EXPECT_TRUE(OutputUtil::GetCandidateIndexById(output, -2, &candidate_index));
  EXPECT_EQ(9, candidate_index);

  // Not existing ID.
  candidate_index = 0;
  EXPECT_FALSE(OutputUtil::GetCandidateIndexById(
      output, 100, &candidate_index));
}

TEST(OutputUtilTest, GetCandidateIdByIndex) {
  commands::Output output;
  SetTestDataForConversion(&output);

  // Existing index
  int32 candidate_id = 0;
  EXPECT_TRUE(OutputUtil::GetCandidateIdByIndex(output, 9, &candidate_id));
  EXPECT_EQ(-2, candidate_id);

  // Not existing index.
  candidate_id = 0;
  EXPECT_FALSE(OutputUtil::GetCandidateIdByIndex(
      output, 100, &candidate_id));
}

TEST(OutputUtilTest, GetFocusedCandidateId) {
  commands::Output output;
  SetTestDataForConversion(&output);

  int32 candidate_id = 0;
  EXPECT_TRUE(OutputUtil::GetFocusedCandidateId(output, &candidate_id));
  EXPECT_EQ(-3, candidate_id);
}
}  // namespace mozc
