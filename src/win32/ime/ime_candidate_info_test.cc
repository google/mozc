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

#include "win32/ime/ime_candidate_info.h"

#include <cstdint>

#include "base/logging.h"
#include "base/util.h"
#include "protocol/commands.pb.h"
#include "testing/googletest.h"
#include "testing/gunit.h"

namespace mozc {
namespace win32 {
namespace {
#define EXPECT_CANDIDATEINFO(size, count, first_list_offset, private_size, \
                             private_offset, info)                         \
  do {                                                                     \
    EXPECT_EQ((info)->dwSize, (size));                                     \
    EXPECT_EQ((info)->dwCount, (count));                                   \
    EXPECT_EQ((info)->dwOffset[0], (first_list_offset));                   \
    EXPECT_EQ((info)->dwOffset[1], 0);                                     \
    EXPECT_EQ((info)->dwOffset[2], 0);                                     \
    EXPECT_EQ((info)->dwOffset[3], 0);                                     \
    EXPECT_EQ((info)->dwOffset[4], 0);                                     \
    EXPECT_EQ((info)->dwOffset[5], 0);                                     \
    EXPECT_EQ((info)->dwOffset[6], 0);                                     \
    EXPECT_EQ((info)->dwOffset[7], 0);                                     \
    EXPECT_EQ((info)->dwOffset[8], 0);                                     \
    EXPECT_EQ((info)->dwOffset[9], 0);                                     \
    EXPECT_EQ((info)->dwOffset[10], 0);                                    \
    EXPECT_EQ((info)->dwOffset[11], 0);                                    \
    EXPECT_EQ((info)->dwOffset[12], 0);                                    \
    EXPECT_EQ((info)->dwOffset[13], 0);                                    \
    EXPECT_EQ((info)->dwOffset[14], 0);                                    \
    EXPECT_EQ((info)->dwOffset[15], 0);                                    \
    EXPECT_EQ((info)->dwOffset[16], 0);                                    \
    EXPECT_EQ((info)->dwOffset[17], 0);                                    \
    EXPECT_EQ((info)->dwOffset[18], 0);                                    \
    EXPECT_EQ((info)->dwOffset[19], 0);                                    \
    EXPECT_EQ((info)->dwOffset[20], 0);                                    \
    EXPECT_EQ((info)->dwOffset[21], 0);                                    \
    EXPECT_EQ((info)->dwOffset[22], 0);                                    \
    EXPECT_EQ((info)->dwOffset[23], 0);                                    \
    EXPECT_EQ((info)->dwOffset[24], 0);                                    \
    EXPECT_EQ((info)->dwOffset[25], 0);                                    \
    EXPECT_EQ((info)->dwOffset[26], 0);                                    \
    EXPECT_EQ((info)->dwOffset[27], 0);                                    \
    EXPECT_EQ((info)->dwOffset[28], 0);                                    \
    EXPECT_EQ((info)->dwOffset[29], 0);                                    \
    EXPECT_EQ((info)->dwOffset[30], 0);                                    \
    EXPECT_EQ((info)->dwOffset[31], 0);                                    \
    EXPECT_EQ((info)->dwOffset[32], 0);                                    \
    EXPECT_EQ((info)->dwPrivateSize, (private_size));                      \
    EXPECT_EQ((info)->dwPrivateOffset, (private_offset));                  \
  } while (false)

#define EXPECT_CANDIDATELIST(size, style, count, selection, page_start, \
                             page_size, info)                           \
  do {                                                                  \
    EXPECT_EQ((info)->dwSize, (size));                                  \
    EXPECT_EQ((info)->dwStyle, (style));                                \
    EXPECT_EQ((info)->dwCount, (count));                                \
    EXPECT_EQ((info)->dwSelection, (selection));                        \
    EXPECT_EQ((info)->dwPageStart, (page_start));                       \
    EXPECT_EQ((info)->dwPageSize, (page_size));                         \
    EXPECT_EQ((info)->dwPageStart, (page_start));                       \
  } while (false)

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

class ScopedCandidateInfoBuffer {
 public:
  ScopedCandidateInfoBuffer(const ScopedCandidateInfoBuffer &) = delete;
  ScopedCandidateInfoBuffer &operator=(const ScopedCandidateInfoBuffer &) =
      delete;
  explicit ScopedCandidateInfoBuffer(size_t size)
      : header_(static_cast<CANDIDATEINFO *>(Allocate(size))) {}
  ~ScopedCandidateInfoBuffer() { ::HeapFree(::GetProcessHeap(), 0, header_); }
  const CANDIDATEINFO *header() const { return header_; }
  CANDIDATEINFO *mutable_header() { return header_; }
  const CANDIDATELIST *GetList(int candidate_list_no) const {
    DCHECK_GE(candidate_list_no, 0);
    DCHECK_LT(candidate_list_no, header_->dwCount);
    return reinterpret_cast<const CANDIDATELIST *>(
        reinterpret_cast<const BYTE *>(header_) +
        header_->dwOffset[candidate_list_no]);
  }
  const std::wstring GetCandidateString(int candidate_list_no,
                                        int candidate_index) const {
    const CANDIDATELIST *list = GetList(candidate_list_no);
    DCHECK_GE(candidate_index, 0);
    DCHECK_LT(candidate_index, list->dwCount);
    return reinterpret_cast<const wchar_t *>(
        reinterpret_cast<const BYTE *>(list) + list->dwOffset[candidate_index]);
  }

 private:
  static void *Allocate(size_t size) {
    return ::HeapAlloc(::GetProcessHeap(), 0, size);
  }
  CANDIDATEINFO *header_;
};

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

std::string ToUTF8(const std::wstring &wstr) {
  std::string result;
  Util::WideToUtf8(wstr, &result);
  return result;
}
}  // namespace

// Some games such as EMIL CHRONICLE ONLINE assumes that
// CANDIDATELIST::dwPageSize never be zero nor grater than 10 despite that
// the WDK document for IMM32 declares this field can be 0.  See b/3033499.
#define EXPECT_SAFE_PAGE_SIZE(pagesize) \
  do {                                  \
    EXPECT_GT((pagesize), 0);           \
    EXPECT_LT((pagesize), 11);          \
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
  EXPECT_EQ(info.candidate_info_size, 0);
  EXPECT_EQ(info.candidate_list_size, 0);
  EXPECT_EQ(info.count, 0);
  EXPECT_EQ(info.selection, 0);
  EXPECT_FALSE(info.show_candidate);
  EXPECT_TRUE(info.offsets.empty());
  EXPECT_TRUE(info.text_buffer.empty());

  // Second conversion.
  FillOutputForConversion(&output, 1, true);
  info.Clear();
  EXPECT_TRUE(CandidateInfoUtil::Convert(output, &info));

  EXPECT_EQ(info.candidate_info_size, 330);
  EXPECT_EQ(info.candidate_list_size, 186);
  EXPECT_EQ(info.count, kNumCandidates);
  EXPECT_EQ(info.selection, 1);
  EXPECT_TRUE(info.show_candidate);
  EXPECT_EQ(info.offsets.size(), kNumCandidates);
  EXPECT_EQ(info.text_buffer.size(), 55);

  // End conversion.
  output.clear_all_candidate_words();
  output.clear_candidates();

  info.Clear();
  EXPECT_TRUE(CandidateInfoUtil::Convert(output, &info));

  EXPECT_EQ(info.candidate_info_size, 0);
  EXPECT_EQ(info.candidate_list_size, 0);
  EXPECT_EQ(info.count, 0);
  EXPECT_EQ(info.selection, 0);
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

  EXPECT_EQ(info.candidate_info_size, 0);
  EXPECT_EQ(info.candidate_list_size, 0);
  EXPECT_EQ(info.count, 0);
  EXPECT_EQ(info.selection, 0);
  EXPECT_FALSE(info.show_candidate);
  EXPECT_TRUE(info.offsets.empty());
  EXPECT_TRUE(info.text_buffer.empty());
}

TEST(CandidateInfoUtilTest, WriteResultTest) {
  commands::Output output;
  FillOutputForConversion(&output, 1, true);

  CandidateInfo info;
  EXPECT_TRUE(CandidateInfoUtil::Convert(output, &info));

  ScopedCandidateInfoBuffer buffer(info.candidate_info_size);
  CandidateInfoUtil::Write(info, buffer.mutable_header());

  EXPECT_CANDIDATEINFO(330, 1, sizeof(CANDIDATEINFO), 0, 0, buffer.header());
  EXPECT_CANDIDATELIST(186, IME_CAND_READ, kNumCandidates, 1, 0, 9,
                       buffer.GetList(0));

  EXPECT_EQ(ToUTF8(buffer.GetCandidateString(0, 0)), kValueList[0]);
  EXPECT_EQ(ToUTF8(buffer.GetCandidateString(0, 1)), kValueList[1]);
  EXPECT_EQ(ToUTF8(buffer.GetCandidateString(0, 2)), kValueList[2]);
  EXPECT_EQ(ToUTF8(buffer.GetCandidateString(0, 3)), kValueList[3]);
  EXPECT_EQ(ToUTF8(buffer.GetCandidateString(0, 4)), kValueList[4]);
  EXPECT_EQ(ToUTF8(buffer.GetCandidateString(0, 5)), kValueList[5]);
  EXPECT_EQ(ToUTF8(buffer.GetCandidateString(0, 6)), kValueList[6]);
  EXPECT_EQ(ToUTF8(buffer.GetCandidateString(0, 7)), kValueList[7]);
  EXPECT_EQ(ToUTF8(buffer.GetCandidateString(0, 8)), kValueList[8]);
  EXPECT_EQ(ToUTF8(buffer.GetCandidateString(0, 9)), kValueList[9]);
  EXPECT_EQ(ToUTF8(buffer.GetCandidateString(0, 10)), kValueList[10]);
  EXPECT_EQ(ToUTF8(buffer.GetCandidateString(0, 11)), kValueList[11]);
  EXPECT_EQ(ToUTF8(buffer.GetCandidateString(0, 12)), kValueList[12]);
}

TEST(CandidateInfoUtilTest, PagingEmulation_Issue4077022) {
  commands::Output output;
  FillOutputForConversion(&output, 11, true);

  CandidateInfo info;
  EXPECT_TRUE(CandidateInfoUtil::Convert(output, &info));

  ScopedCandidateInfoBuffer buffer(info.candidate_info_size);
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

  ScopedCandidateInfoBuffer buffer(info.candidate_info_size);
  CandidateInfoUtil::Write(info, buffer.mutable_header());

  EXPECT_CANDIDATEINFO(sizeof(CANDIDATEINFO) + sizeof(CANDIDATELIST), 1,
                       sizeof(CANDIDATEINFO), 0, 0, buffer.header());
}
}  // namespace win32
}  // namespace mozc
