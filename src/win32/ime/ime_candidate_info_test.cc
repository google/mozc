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
#include "win32/ime/ime_candidate_info.h"

namespace mozc {
namespace win32 {
namespace {
#define EXPECT_CANDIDATEINFO(                                               \
    size, count, first_list_offset, private_size, private_offset, info)     \
  do {                                                                      \
     EXPECT_EQ((size), (info)->dwSize);                                     \
     EXPECT_EQ((count), (info)->dwCount);                                   \
     EXPECT_EQ((first_list_offset), (info)->dwOffset[0]);                   \
     EXPECT_EQ(0, (info)->dwOffset[1]);                                     \
     EXPECT_EQ(0, (info)->dwOffset[2]);                                     \
     EXPECT_EQ(0, (info)->dwOffset[3]);                                     \
     EXPECT_EQ(0, (info)->dwOffset[4]);                                     \
     EXPECT_EQ(0, (info)->dwOffset[5]);                                     \
     EXPECT_EQ(0, (info)->dwOffset[6]);                                     \
     EXPECT_EQ(0, (info)->dwOffset[7]);                                     \
     EXPECT_EQ(0, (info)->dwOffset[8]);                                     \
     EXPECT_EQ(0, (info)->dwOffset[9]);                                     \
     EXPECT_EQ(0, (info)->dwOffset[10]);                                    \
     EXPECT_EQ(0, (info)->dwOffset[11]);                                    \
     EXPECT_EQ(0, (info)->dwOffset[12]);                                    \
     EXPECT_EQ(0, (info)->dwOffset[13]);                                    \
     EXPECT_EQ(0, (info)->dwOffset[14]);                                    \
     EXPECT_EQ(0, (info)->dwOffset[15]);                                    \
     EXPECT_EQ(0, (info)->dwOffset[16]);                                    \
     EXPECT_EQ(0, (info)->dwOffset[17]);                                    \
     EXPECT_EQ(0, (info)->dwOffset[18]);                                    \
     EXPECT_EQ(0, (info)->dwOffset[19]);                                    \
     EXPECT_EQ(0, (info)->dwOffset[20]);                                    \
     EXPECT_EQ(0, (info)->dwOffset[21]);                                    \
     EXPECT_EQ(0, (info)->dwOffset[22]);                                    \
     EXPECT_EQ(0, (info)->dwOffset[23]);                                    \
     EXPECT_EQ(0, (info)->dwOffset[24]);                                    \
     EXPECT_EQ(0, (info)->dwOffset[25]);                                    \
     EXPECT_EQ(0, (info)->dwOffset[26]);                                    \
     EXPECT_EQ(0, (info)->dwOffset[27]);                                    \
     EXPECT_EQ(0, (info)->dwOffset[28]);                                    \
     EXPECT_EQ(0, (info)->dwOffset[29]);                                    \
     EXPECT_EQ(0, (info)->dwOffset[30]);                                    \
     EXPECT_EQ(0, (info)->dwOffset[31]);                                    \
     EXPECT_EQ(0, (info)->dwOffset[32]);                                    \
     EXPECT_EQ((private_size), (info)->dwPrivateSize);                      \
     EXPECT_EQ((private_offset), (info)->dwPrivateOffset);                  \
  } while (false)                                                           \

#define EXPECT_CANDIDATELIST(                                               \
    size, style, count, selection, page_start, page_size, info)             \
  do {                                                                      \
     EXPECT_EQ((size), (info)->dwSize);                                     \
     EXPECT_EQ((style), (info)->dwStyle);                                   \
     EXPECT_EQ((count), (info)->dwCount);                                   \
     EXPECT_EQ((selection), (info)->dwSelection);                           \
     EXPECT_EQ((page_start), (info)->dwPageStart);                          \
     EXPECT_EQ((page_size), (info)->dwPageSize);                            \
     EXPECT_EQ((page_start), (info)->dwPageStart);                          \
  } while (false)                                                           \

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

class ScopedCanidateInfoBuffer {
 public:
  explicit ScopedCanidateInfoBuffer(size_t size)
      : header_(static_cast<CANDIDATEINFO *>(Allocate(size))) {}
  ~ScopedCanidateInfoBuffer() {
    ::HeapFree(::GetProcessHeap(), 0, header_);
  }
  const CANDIDATEINFO *header() const {
    return header_;
  }
  CANDIDATEINFO *mutable_header() {
    return header_;
  }
  const CANDIDATELIST *GetList(int candidate_list_no) const {
    DCHECK_GE(candidate_list_no, 0);
    DCHECK_LT(candidate_list_no, header_->dwCount);
    return reinterpret_cast<const CANDIDATELIST *>(
        reinterpret_cast<const BYTE *>(header_) +
        header_->dwOffset[candidate_list_no]);
  }
  const wstring GetCandidateString(int candidate_list_no,
                                   int candidate_index) const {
    const CANDIDATELIST *list = GetList(candidate_list_no);
    DCHECK_GE(candidate_index, 0);
    DCHECK_LT(candidate_index, list->dwCount);
    return reinterpret_cast<const wchar_t *>(
        reinterpret_cast<const BYTE *>(list) +
        list->dwOffset[candidate_index]);
  }

 private:
  static void *Allocate(size_t size) {
    return ::HeapAlloc(::GetProcessHeap(), 0, size);
  }
  CANDIDATEINFO *header_;

  DISALLOW_COPY_AND_ASSIGN(ScopedCanidateInfoBuffer);
};

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

string ToUTF8(const wstring &wstr) {
  string result;
  Util::WideToUTF8(wstr, &result);
  return result;
}
}  // anonymous namespace

// Some games such as EMIL CHRONICLE ONLINE assumes that
// CANDIDATELIST::dwPageSize never be zero nor grater than 10 despite that
// the WDK document for IMM32 declares this field can be 0.  See b/3033499.
#define EXPECT_SAFE_PAGE_SIZE(pagesize)   \
  do {                                    \
    EXPECT_GT((pagesize), 0);             \
    EXPECT_LT((pagesize), 11);            \
  } while (false)

TEST(CandidateInfoUtilTest, ConversionTest) {
  commands::Output output;
  CandidateInfo info;

  // First conversion.
  // It is common for traditional IMEs not to display candidate window for the
  // first conversion.  These IMEs do not fill CANDIDATEINFO for the first
  // conversion, too.  See b/2978825 for details.  Mozc server conforms
  // to this behavior by keeping |output.candidates()| empty for the first
  // conversion.
  FillOutputForConversion(&output, 0, false);
  info.Clear();
  EXPECT_TRUE(CandidateInfoUtil::Convert(output, &info));
  EXPECT_EQ(0, info.candidate_info_size);
  EXPECT_EQ(0, info.candidate_list_size);
  EXPECT_EQ(0, info.count);
  EXPECT_EQ(0, info.selection);
  EXPECT_FALSE(info.show_candidate);
  EXPECT_TRUE(info.offsets.empty());
  EXPECT_TRUE(info.text_buffer.empty());

  // Second conversion.
  FillOutputForConversion(&output, 1, true);
  info.Clear();
  EXPECT_TRUE(CandidateInfoUtil::Convert(output, &info));

  EXPECT_EQ(330, info.candidate_info_size);
  EXPECT_EQ(186, info.candidate_list_size);
  EXPECT_EQ(kNumCandidates, info.count);
  EXPECT_EQ(1, info.selection);
  EXPECT_TRUE(info.show_candidate);
  EXPECT_EQ(kNumCandidates, info.offsets.size());
  EXPECT_EQ(55, info.text_buffer.size());

  // End conversion.
  output.clear_all_candidate_words();
  output.clear_candidates();

  info.Clear();
  EXPECT_TRUE(CandidateInfoUtil::Convert(output, &info));

  EXPECT_EQ(0, info.candidate_info_size);
  EXPECT_EQ(0, info.candidate_list_size);
  EXPECT_EQ(0, info.count);
  EXPECT_EQ(0, info.selection);
  EXPECT_FALSE(info.show_candidate);
  EXPECT_TRUE(info.offsets.empty());
  EXPECT_TRUE(info.text_buffer.empty());
}

TEST(CandidateInfoUtilTest, SuggestionTest) {
  commands::Output output;
  FillOutputForSuggestion(&output);

  CandidateInfo info;

  info.Clear();
  EXPECT_TRUE(CandidateInfoUtil::Convert(output, &info));

  EXPECT_EQ(0, info.candidate_info_size);
  EXPECT_EQ(0, info.candidate_list_size);
  EXPECT_EQ(0, info.count);
  EXPECT_EQ(0, info.selection);
  EXPECT_FALSE(info.show_candidate);
  EXPECT_TRUE(info.offsets.empty());
  EXPECT_TRUE(info.text_buffer.empty());
}

TEST(CandidateInfoUtilTest, WriteResultTest) {
  commands::Output output;
  FillOutputForConversion(&output, 1, true);

  CandidateInfo info;
  EXPECT_TRUE(CandidateInfoUtil::Convert(output, &info));

  ScopedCanidateInfoBuffer buffer(info.candidate_info_size);
  CandidateInfoUtil::Write(info, buffer.mutable_header());

  EXPECT_CANDIDATEINFO(330, 1, sizeof(CANDIDATEINFO), 0, 0, buffer.header());
  EXPECT_CANDIDATELIST(186, IME_CAND_READ, kNumCandidates, 1, 0, 9,
                       buffer.GetList(0));

  EXPECT_EQ(kValueList[0], ToUTF8(buffer.GetCandidateString(0, 0)));
  EXPECT_EQ(kValueList[1], ToUTF8(buffer.GetCandidateString(0, 1)));
  EXPECT_EQ(kValueList[2], ToUTF8(buffer.GetCandidateString(0, 2)));
  EXPECT_EQ(kValueList[3], ToUTF8(buffer.GetCandidateString(0, 3)));
  EXPECT_EQ(kValueList[4], ToUTF8(buffer.GetCandidateString(0, 4)));
  EXPECT_EQ(kValueList[5], ToUTF8(buffer.GetCandidateString(0, 5)));
  EXPECT_EQ(kValueList[6], ToUTF8(buffer.GetCandidateString(0, 6)));
  EXPECT_EQ(kValueList[7], ToUTF8(buffer.GetCandidateString(0, 7)));
  EXPECT_EQ(kValueList[8], ToUTF8(buffer.GetCandidateString(0, 8)));
  EXPECT_EQ(kValueList[9], ToUTF8(buffer.GetCandidateString(0, 9)));
  EXPECT_EQ(kValueList[10], ToUTF8(buffer.GetCandidateString(0, 10)));
  EXPECT_EQ(kValueList[11], ToUTF8(buffer.GetCandidateString(0, 11)));
  EXPECT_EQ(kValueList[12], ToUTF8(buffer.GetCandidateString(0, 12)));
}

TEST(CandidateInfoUtilTest, PagingEmulation_Issue4077022) {
  commands::Output output;
  FillOutputForConversion(&output, 11, true);

  CandidateInfo info;
  EXPECT_TRUE(CandidateInfoUtil::Convert(output, &info));

  ScopedCanidateInfoBuffer buffer(info.candidate_info_size);
  CandidateInfoUtil::Write(info, buffer.mutable_header());

  EXPECT_CANDIDATEINFO(330, 1, sizeof(CANDIDATEINFO), 0, 0, buffer.header());
  EXPECT_CANDIDATELIST(186, IME_CAND_READ, kNumCandidates, 11, 9, 9,
                       buffer.GetList(0));
}

TEST(CandidateInfoUtilTest, WriteSafeDefaultTest) {
  commands::Output output;
  FillOutputForConversion(&output, 1, true);

  CandidateInfo info;
  CandidateInfoUtil::SetSafeDefault(&info);

  ScopedCanidateInfoBuffer buffer(info.candidate_info_size);
  CandidateInfoUtil::Write(info, buffer.mutable_header());

  EXPECT_CANDIDATEINFO(
      sizeof(CANDIDATEINFO) + sizeof(CANDIDATELIST), 1,
      sizeof(CANDIDATEINFO), 0, 0, buffer.header());
}
}  // namespace win32
}  // namespace mozc
