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

#include "renderer/win32/win32_renderer_util.h"

// clang-format off
#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
#include <atlbase.h>
#include <atlapp.h>
#include <atlgdi.h>
#include <atlmisc.h>
// clang-format on

#include "base/logging.h"
#include "base/mmap.h"
#include "base/util.h"
#include "base/win_font_test_helper.h"
#include "protocol/renderer_command.pb.h"
#include "renderer/win32/win32_font_util.h"
#include "testing/base/public/gunit.h"

// Following functions should be placed in global namespace for Koenig look-up
// trick used in GTest.
void PrintTo(const POINT &point, ::std::ostream *os) {
  *os << "(" << point.x << ", " << point.y << ")";
}
void PrintTo(const RECT &rect, ::std::ostream *os) {
  *os << "(" << rect.left << ", " << rect.top << ", " << rect.right << ", "
      << rect.bottom << ")";
}

namespace WTL {

// These functions should be placed in WTL namespace for Koenig look-up
// trick used in GTest.
void PrintTo(const CPoint &point, ::std::ostream *os) {
  *os << "(" << point.x << ", " << point.y << ")";
}
void PrintTo(const CRect &rect, ::std::ostream *os) {
  *os << "(" << rect.left << ", " << rect.top << ", " << rect.right << ", "
      << rect.bottom << ")";
}

}  // namespace WTL

namespace mozc {
namespace renderer {
namespace win32 {
typedef mozc::commands::Annotation Annotation;
typedef mozc::commands::CandidateList CandidateList;
typedef mozc::commands::CandidateWord CandidateWord;
typedef mozc::commands::Candidates Candidates;
typedef mozc::commands::Candidates::Candidate Candidate;
typedef mozc::commands::Footer Footer;
typedef mozc::commands::Output Output;
typedef mozc::commands::Preedit Preedit;
typedef mozc::commands::Preedit_Segment Segment;
typedef mozc::commands::RendererCommand RendererCommand;
typedef mozc::commands::RendererCommand_ApplicationInfo ApplicationInfo;
typedef mozc::commands::RendererCommand_CandidateForm CandidateForm;
typedef mozc::commands::RendererCommand_CaretInfo CaretInfo;
typedef mozc::commands::RendererCommand_CharacterPosition CharacterPosition;
typedef mozc::commands::RendererCommand_CompositionForm CompositionForm;
typedef mozc::commands::RendererCommand_Point Point;
typedef mozc::commands::RendererCommand_Rectangle Rectangle;
typedef mozc::commands::RendererCommand_WinLogFont WinLogFont;
typedef mozc::commands::Status Status;

namespace {

using WTL::CDC;
using WTL::CFont;
using WTL::CFontHandle;
using WTL::CLogFont;
using WTL::CPoint;
using WTL::CRect;
using WTL::CSize;
using WTL::PrintTo;

const int kDefaultFontHeightInPixel = 18;
const wchar_t kWindowClassName[] = L"Mozc: Default Window Class Name";

#define EXPECT_COMPOSITION_WINDOW_LAYOUT(                                      \
    window_pos_left, window_pos_top, window_pos_right, window_pos_bottom,      \
    text_left, text_top, text_right, text_bottom, base_x, base_y, caret_left,  \
    caret_top, caret_right, caret_bottom, font, window_layout)                 \
  do {                                                                         \
    EXPECT_EQ(CRect((window_pos_left), (window_pos_top), (window_pos_right),   \
                    (window_pos_bottom)),                                      \
              (window_layout).window_position_in_screen_coordinate);           \
    EXPECT_EQ((font), layout.log_font);                                        \
    EXPECT_EQ(CRect((text_left), (text_top), (text_right), (text_bottom)),     \
              (window_layout).text_area);                                      \
    EXPECT_EQ(CPoint((base_x), (base_y)), (window_layout).base_position);      \
    EXPECT_EQ(CRect((caret_left), (caret_top), (caret_right), (caret_bottom)), \
              (window_layout).caret_rect);                                     \
  } while (false)

#define EXPECT_NON_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(target_x, target_y, layout) \
  do {                                                                         \
    EXPECT_TRUE((layout).initialized());                                       \
    EXPECT_FALSE((layout).has_exclude_region());                               \
    EXPECT_EQ(CPoint((target_x), (target_y)), (layout).position());            \
  } while (false)

#define EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(                     \
    target_x, target_y, exclude_rect_left, exclude_rect_top,        \
    exclude_rect_right, exclude_rect_bottom, layout)                \
  do {                                                              \
    EXPECT_TRUE((layout).initialized());                            \
    EXPECT_TRUE((layout).has_exclude_region());                     \
    EXPECT_EQ(CPoint((target_x), (target_y)), (layout).position()); \
    EXPECT_EQ(CRect((exclude_rect_left), (exclude_rect_top),        \
                    (exclude_rect_right), (exclude_rect_bottom)),   \
              (layout).exclude_region());                           \
  } while (false)

WindowPositionEmulator *CreateWindowEmulator(const std::wstring &class_name,
                                             const RECT &window_rect,
                                             const POINT &client_area_offset,
                                             const SIZE &client_area_size,
                                             double scale_factor, HWND *hwnd) {
  WindowPositionEmulator *emulator = WindowPositionEmulator::Create();
  *hwnd = emulator->RegisterWindow(class_name, window_rect, client_area_offset,
                                   client_area_size, scale_factor);
  return emulator;
}

WindowPositionEmulator *CreateWindowEmulatorWithDPIScaling(double scale_factor,
                                                           HWND *hwnd) {
  const CPoint kClientOffset(8, 42);
  const CSize kClientSize(2000, 1000);
  const CRect kWindowRect(500, 500, 2516, 1550);

  return CreateWindowEmulator(kWindowClassName, kWindowRect, kClientOffset,
                              kClientSize, scale_factor, hwnd);
}

WindowPositionEmulator *CreateWindowEmulatorWithClassName(
    const std::wstring &class_name, HWND *hwnd) {
  const CPoint kClientOffset(8, 42);
  const CSize kClientSize(2000, 1000);
  const CRect kWindowRect(500, 500, 2516, 1550);
  const double kScaleFactor = 1.0;

  return CreateWindowEmulator(class_name, kWindowRect, kClientOffset,
                              kClientSize, kScaleFactor, hwnd);
}

class AppInfoUtil {
 public:
  static void SetBasicApplicationInfo(ApplicationInfo *app_info, HWND hwnd,
                                      int visibility) {
    app_info->set_ui_visibilities(visibility);
    app_info->set_process_id(1234);
    app_info->set_thread_id(5678);
    app_info->set_target_window_handle(reinterpret_cast<uint32>(hwnd));
    app_info->set_input_framework(ApplicationInfo::IMM32);
  }

  static void SetCompositionFont(ApplicationInfo *app_info, int height,
                                 int width, int escapement, int orientation,
                                 int weight, int char_set, int out_precision,
                                 int clip_precision, int quality,
                                 int pitch_and_family, const char *face_name) {
    WinLogFont *font = app_info->mutable_composition_font();
    font->set_height(height);
    font->set_width(width);
    font->set_escapement(escapement);
    font->set_orientation(orientation);
    font->set_weight(weight);
    font->set_italic(false);
    font->set_underline(false);
    font->set_strike_out(false);
    font->set_char_set(char_set);
    font->set_out_precision(out_precision);
    font->set_clip_precision(clip_precision);
    font->set_quality(quality);
    font->set_pitch_and_family(pitch_and_family);
    font->set_face_name(face_name);
  }

  static void SetCompositionForm(ApplicationInfo *app_info, uint32 style_bits,
                                 int x, int y, int left, int top, int right,
                                 int bottom) {
    CompositionForm *form = app_info->mutable_composition_form();
    form->set_style_bits(style_bits);
    Point *current_position = form->mutable_current_position();
    Rectangle *area = form->mutable_area();
    current_position->set_x(x);
    current_position->set_y(y);
    area->set_left(left);
    area->set_top(top);
    area->set_right(right);
    area->set_bottom(bottom);
  }

  static void SetCandidateForm(ApplicationInfo *app_info, uint32 style_bits,
                               int x, int y, int left, int top, int right,
                               int bottom) {
    CandidateForm *form = app_info->mutable_candidate_form();
    form->set_style_bits(style_bits);
    Point *current_pos = form->mutable_current_position();
    current_pos->set_x(x);
    current_pos->set_y(y);
    Rectangle *area = form->mutable_area();
    area->set_left(left);
    area->set_top(top);
    area->set_right(right);
    area->set_bottom(bottom);
  }

  static void SetCaretInfo(ApplicationInfo *app_info, bool blinking, int left,
                           int top, int right, int bottom,
                           HWND target_window_handle) {
    CaretInfo *info = app_info->mutable_caret_info();
    info->set_blinking(blinking);
    info->set_target_window_handle(
        reinterpret_cast<uint32>(target_window_handle));
    Rectangle *rect = info->mutable_caret_rect();
    rect->set_left(left);
    rect->set_top(top);
    rect->set_right(right);
    rect->set_bottom(bottom);
  }

  static void SetCompositionTarget(ApplicationInfo *app_info, int position,
                                   int x, int y, uint32 line_height, int left,
                                   int top, int right, int bottom) {
    CharacterPosition *char_pos = app_info->mutable_composition_target();
    char_pos->set_position(position);
    char_pos->mutable_top_left()->set_x(x);
    char_pos->mutable_top_left()->set_y(y);
    char_pos->set_line_height(line_height);
    Rectangle *area = char_pos->mutable_document_area();
    area->set_left(left);
    area->set_top(top);
    area->set_right(right);
    area->set_bottom(bottom);
  }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(AppInfoUtil);
};

}  // namespace

class Win32RendererUtilTest : public testing::Test {
 public:
  static string GetMonospacedFontFaceForTest() {
    return WinFontTestHelper::GetIPAexGothicFontName();
  }

  static string GetPropotionalFontFaceForTest() {
    return WinFontTestHelper::GetIPAexMinchoFontName();
  }

  static CLogFont GetFont(bool is_proportional, bool is_vertical) {
    std::wstring font_face;
    Util::UTF8ToWide((is_proportional ? GetPropotionalFontFaceForTest()
                                      : GetMonospacedFontFaceForTest()),
                     &font_face);
    if (is_vertical) {
      font_face = L"@" + font_face;
    }

    CLogFont font;
    font.lfWeight = FW_NORMAL;
    font.lfCharSet = DEFAULT_CHARSET;

    // We use negative value here to specify absolute font height in pixel,
    // assuming the mapping mode is MM_TEXT.
    // http://msdn.microsoft.com/en-us/library/ms901140.aspx
    font.lfHeight = -kDefaultFontHeightInPixel;

    const errno_t error = wcscpy_s(font.lfFaceName, font_face.c_str());
    CHECK_EQ(0, error) << "wcscpy_s failed";

    if (is_vertical) {
      // 2700 means the text grows from top to bottom.
      font.lfEscapement = 2700;
      font.lfOrientation = 2700;
    }

    return font;
  }

  static SystemPreferenceInterface *CreateDefaultGUIFontEmulator() {
    CLogFont font = GetFont(true, false);
    font.lfHeight = 18;
    font.lfWidth = 0;
    return SystemPreferenceFactory::CreateMock(font);
  }

  static std::wstring GetTestMessageWithCompositeGlyph(int num_repeat) {
    std::wstring message;
    for (size_t i = 0; i < num_repeat; ++i) {
      // "ぱ"
      message += L'\u3071';
      message += L'\u309a';
    }
    return message;
  }

  static std::wstring GetTestMessageForMonospaced() {
    std::wstring w_path;
    const char kMessage[] =
        "熊本県阿蘇郡南阿蘇村大字中松南阿蘇水の生まれる里白水高原駅";
    std::wstring w_message;
    Util::UTF8ToWide(kMessage, &w_message);
    return w_message;
  }

  static std::wstring GetTestMessageForProportional() {
    std::wstring w_path;
    const char kMessage[] =
        "This open-source project originates from Google 日本語入力.";
    std::wstring w_message;
    Util::UTF8ToWide(kMessage, &w_message);
    return w_message;
  }

  // Initializes |command| for unit test.  Parameters to be set are based on
  // an actual application which supports both horizontal and vertical writing.
  static void SetRenderereCommandForTest(bool use_proportional_font,
                                         bool has_candidates, bool is_vertical,
                                         int cursor_offset, HWND hwnd,
                                         RendererCommand *command) {
    command->Clear();
    command->set_type(RendererCommand::UPDATE);
    command->set_visible(true);
    {
      Output *output = command->mutable_output();
      output->set_id(123456789);
      output->set_mode(commands::HIRAGANA);
      output->set_consumed(true);
      Preedit *preedit = output->mutable_preedit();
      preedit->set_cursor(22);
      {
        Segment *segment = preedit->add_segment();
        segment->set_annotation(Segment::UNDERLINE);
        segment->set_value("これは");
        segment->set_value_length(3);
        segment->set_key("これは");
      }
      {
        Segment *segment = preedit->add_segment();
        segment->set_annotation(Segment::UNDERLINE);
        segment->set_value("、");
        segment->set_value_length(1);
        segment->set_key("、");
      }
      {
        Segment *segment = preedit->add_segment();
        segment->set_annotation(Segment::HIGHLIGHT);
        segment->set_value("Google");
        segment->set_value_length(6);
        segment->set_key("Google");
      }
      {
        Segment *segment = preedit->add_segment();
        segment->set_annotation(Segment::UNDERLINE);
        segment->set_value("日本語入力の");
        segment->set_value_length(6);
        segment->set_key("にほんごにゅうりょくの");
      }
      {
        Segment *segment = preedit->add_segment();
        segment->set_annotation(Segment::UNDERLINE);
        segment->set_value("Testです");
        segment->set_value_length(6);
        segment->set_key("Testです");
      }
      preedit->set_highlighted_position(3);

      if (has_candidates) {
        Candidates *candidates = output->mutable_candidates();
        candidates->set_focused_index(0);
        candidates->set_size(2);
        {
          Candidate *candidate = candidates->add_candidate();
          candidate->set_index(0);
          candidate->set_value("Google");
          Annotation *annotation = candidate->mutable_annotation();
          annotation->set_description("[半] アルファベット");
          annotation->set_shortcut("1");
          candidate->set_id(0);
        }
        {
          Candidate *candidate = candidates->add_candidate();
          candidate->set_index(1);
          candidate->set_value("そのほかの文字種");
          Annotation *annotation = candidate->mutable_annotation();
          annotation->set_shortcut("2");
          candidate->set_id(-11);
        }
        candidates->set_position(4);
        candidates->set_category(commands::CONVERSION);
        candidates->set_display_type(commands::MAIN);
        Footer *footer = candidates->mutable_footer();
        footer->set_index_visible(true);
        footer->set_logo_visible(true);
        footer->set_sub_label("build 000");
      }
    }

    SetApplicationInfoForTest(use_proportional_font, is_vertical, cursor_offset,
                              hwnd, command);
  }

  // Initializes |command| for unit test.  Parameters to be set are based on
  // an actual application which supports both horizontal and vertical writing.
  static void SetRenderereCommandForSuggestTest(bool use_proportional_font,
                                                bool is_vertical,
                                                int cursor_offset, HWND hwnd,
                                                RendererCommand *command) {
    command->Clear();
    command->set_type(RendererCommand::UPDATE);
    command->set_visible(true);
    {
      Output *output = command->mutable_output();
      output->set_id(123456789);
      output->set_mode(commands::HIRAGANA);
      output->set_consumed(true);
      {
        Preedit *preedit = output->mutable_preedit();
        preedit->set_cursor(7);
        {
          Segment *segment = preedit->add_segment();
          segment->set_annotation(Segment::UNDERLINE);
          segment->set_value("ねこをかいたい");
          segment->set_value_length(7);
          segment->set_key("ねこをかいたい");
        }
      }
      {
        Candidates *candidates = output->mutable_candidates();
        candidates->set_size(1);
        {
          Candidate *candidate = candidates->add_candidate();
          candidate->set_index(0);
          candidate->set_value("猫を飼いたい");
          {
            Annotation *annotation = candidate->mutable_annotation();
            annotation->set_description("Real-time Conversion");
            candidate->set_id(0);
          }
        }
        candidates->set_position(0);
        candidates->set_category(commands::SUGGESTION);
        candidates->set_display_type(commands::MAIN);
        {
          Footer *footer = candidates->mutable_footer();
          footer->set_sub_label("build 754");
        }
      }
    }

    SetApplicationInfoForTest(use_proportional_font, is_vertical, cursor_offset,
                              hwnd, command);
  }

  // Initializes |command| for unit tests of caret.  Parameters to be set are
  // based on an actual application which supports both horizontal and vertical
  // writing.
  static void SetRenderereCommandForCaretTest(bool use_proportional_font,
                                              bool is_vertical,
                                              int num_characters,
                                              int cursor_position_in_preedit,
                                              int cursor_offset, HWND hwnd,
                                              RendererCommand *command) {
    command->Clear();
    command->set_type(RendererCommand::UPDATE);
    command->set_visible(true);
    {
      Output *output = command->mutable_output();
      output->set_id(123456789);
      output->set_mode(commands::HIRAGANA);
      output->set_consumed(true);
      Preedit *preedit = output->mutable_preedit();
      preedit->set_cursor(cursor_position_in_preedit);
      {
        Segment *segment = preedit->add_segment();
        segment->set_annotation(Segment::UNDERLINE);
        string value;
        for (size_t i = 0; i < num_characters; ++i) {
          value.append("あ");
        }
        segment->set_value(value);
        segment->set_value_length(num_characters);
        segment->set_key(value);
      }
    }

    SetApplicationInfoForTest(use_proportional_font, is_vertical, cursor_offset,
                              hwnd, command);
  }

  // Initializes |command| for unit tests of caret.  Parameters to be set are
  // based on an actual application which supports both horizontal and vertical
  // writing.
  static void SetRenderereCommandForSurrogatePair(bool use_proportional_font,
                                                  bool is_vertical,
                                                  int cursor_offset, HWND hwnd,
                                                  RendererCommand *command) {
    command->Clear();
    command->set_type(RendererCommand::UPDATE);
    command->set_visible(true);
    {
      Output *output = command->mutable_output();
      output->set_id(123456789);
      output->set_mode(commands::HIRAGANA);
      output->set_consumed(true);
      {
        Preedit *preedit = output->mutable_preedit();
        preedit->set_cursor(8);
        {
          Segment *segment = preedit->add_segment();
          segment->set_annotation(Segment::UNDERLINE);
          segment->set_value("𠮟咤");
          segment->set_value_length(2);
          segment->set_key("しった");
        }
        {
          Segment *segment = preedit->add_segment();
          segment->set_annotation(Segment::UNDERLINE);
          segment->set_value("𠮟咤");
          segment->set_value_length(2);
          segment->set_key("しった");
        }
        {
          Segment *segment = preedit->add_segment();
          segment->set_annotation(Segment::HIGHLIGHT);
          segment->set_value("𠮟咤");
          segment->set_value_length(2);
          segment->set_key("しった");
        }
        {
          Segment *segment = preedit->add_segment();
          segment->set_annotation(Segment::UNDERLINE);
          segment->set_value("𠮟咤");
          segment->set_value_length(2);
          segment->set_key("しった");
        }
        preedit->set_highlighted_position(4);
      }
      {
        Candidates *candidates = output->mutable_candidates();
        candidates->set_focused_index(0);
        candidates->set_size(5);
        {
          Candidate *candidate = candidates->add_candidate();
          candidate->set_index(0);
          candidate->set_value("𠮟咤");
          {
            Annotation *annotation = candidate->mutable_annotation();
            annotation->set_shortcut("1");
          }
          candidate->set_id(0);
        }
        {
          Candidate *candidate = candidates->add_candidate();
          candidate->set_index(1);
          candidate->set_value("知った");
          {
            Annotation *annotation = candidate->mutable_annotation();
            annotation->set_shortcut("2");
          }
          candidate->set_id(1);
        }
        {
          Candidate *candidate = candidates->add_candidate();
          candidate->set_index(2);
          candidate->set_value("知った");
          {
            Annotation *annotation = candidate->mutable_annotation();
            annotation->set_description("ひらがな");
            annotation->set_shortcut("3");
          }
          candidate->set_id(2);
        }
        {
          Candidate *candidate = candidates->add_candidate();
          candidate->set_index(3);
          candidate->set_value("知った");
          {
            Annotation *annotation = candidate->mutable_annotation();
            annotation->set_description("[全] カタカナ");
            annotation->set_shortcut("4");
          }
          candidate->set_id(4);
        }
        {
          Candidate *candidate = candidates->add_candidate();
          candidate->set_index(4);
          candidate->set_value("そのほかの文字種");
          {
            Annotation *annotation = candidate->mutable_annotation();
            annotation->set_shortcut("5");
          }
          candidate->set_id(-1);
        }
        candidates->set_position(4);
        candidates->set_category(commands::CONVERSION);
        candidates->set_display_type(commands::MAIN);
        {
          Footer *footer = candidates->mutable_footer();
          footer->set_index_visible(true);
          footer->set_logo_visible(true);
          footer->set_sub_label("build 670");
        }
      }
      {
        Status *status = output->mutable_status();
        status->set_activated(true);
        status->set_mode(commands::HIRAGANA);
      }
      {
        CandidateList *all_candidate_words =
            output->mutable_all_candidate_words();
        all_candidate_words->set_focused_index(0);
        {
          CandidateWord *candidate_word = all_candidate_words->add_candidates();
          candidate_word->set_id(0);
          candidate_word->set_index(0);
          candidate_word->set_value("𠮟咤");
        }
        {
          CandidateWord *candidate_word = all_candidate_words->add_candidates();
          candidate_word->set_id(1);
          candidate_word->set_index(1);
          candidate_word->set_value("知った");
        }
        {
          CandidateWord *candidate_word = all_candidate_words->add_candidates();
          candidate_word->set_id(2);
          candidate_word->set_index(2);
          candidate_word->set_key("しっ");
          candidate_word->set_value("しった");
        }
        {
          CandidateWord *candidate_word = all_candidate_words->add_candidates();
          candidate_word->set_id(4);
          candidate_word->set_index(3);
          candidate_word->set_value("シッタ");
        }
        {
          CandidateWord *candidate_word = all_candidate_words->add_candidates();
          candidate_word->set_id(-1);
          candidate_word->set_index(4);
          candidate_word->set_value("しった");
        }
        {
          CandidateWord *candidate_word = all_candidate_words->add_candidates();
          candidate_word->set_id(-2);
          candidate_word->set_index(5);
          candidate_word->set_value("シッタ");
        }
        {
          CandidateWord *candidate_word = all_candidate_words->add_candidates();
          candidate_word->set_id(-3);
          candidate_word->set_index(6);
          candidate_word->set_value("shitta");
        }
        {
          CandidateWord *candidate_word = all_candidate_words->add_candidates();
          candidate_word->set_id(-4);
          candidate_word->set_index(7);
          candidate_word->set_value("SHITTA");
        }
        {
          CandidateWord *candidate_word = all_candidate_words->add_candidates();
          candidate_word->set_id(-6);
          candidate_word->set_index(8);
          candidate_word->set_value("Shitta");
        }
        {
          CandidateWord *candidate_word = all_candidate_words->add_candidates();
          candidate_word->set_id(-7);
          candidate_word->set_index(9);
          candidate_word->set_value("ｓｈｉｔｔａ");
        }
        {
          CandidateWord *candidate_word = all_candidate_words->add_candidates();
          candidate_word->set_id(-8);
          candidate_word->set_index(10);
          candidate_word->set_value("ＳＨＩＴＴＡ");
        }
        {
          CandidateWord *candidate_word = all_candidate_words->add_candidates();
          candidate_word->set_id(-10);
          candidate_word->set_index(11);
          candidate_word->set_value("Ｓｈｉｔｔａ");
        }
        {
          CandidateWord *candidate_word = all_candidate_words->add_candidates();
          candidate_word->set_id(-11);
          candidate_word->set_index(12);
          candidate_word->set_value("ｼｯﾀ");
        }
        all_candidate_words->set_category(commands::CONVERSION);
      }
    }

    SetApplicationInfoForTest(use_proportional_font, is_vertical, cursor_offset,
                              hwnd, command);
  }

 protected:
  static void SetUpTestCase() {
    // On Windows XP, the availability of typical Japanese fonts such are as
    // MS Gothic depends on the language edition and language packs.
    // So we will register a private font for unit test.
    EXPECT_TRUE(WinFontTestHelper::Initialize());
  }

  static void TearDownTestCase() {
    // Free private fonts although the system automatically frees them when
    // this process is terminated.
    WinFontTestHelper::Uninitialize();
  }

 private:
  static void SetApplicationInfoForTest(bool use_proportional_font,
                                        bool is_vertical, int cursor_offset,
                                        HWND hwnd, RendererCommand *command) {
    ApplicationInfo *app = command->mutable_application_info();
    app->set_process_id(1234);
    app->set_thread_id(5678);
    app->set_target_window_handle(reinterpret_cast<uint32>(hwnd));
    WinLogFont *font = app->mutable_composition_font();
    font->set_height(-45);
    font->set_width(0);
    font->set_escapement(0);
    font->set_orientation(0);
    font->set_weight(FW_NORMAL);
    font->set_italic(false);
    font->set_underline(false);
    font->set_strike_out(false);
    font->set_char_set(SHIFTJIS_CHARSET);
    font->set_out_precision(0);
    font->set_clip_precision(0);
    font->set_quality(0);
    if (use_proportional_font) {
      // Use proportional font
      font->set_pitch_and_family(VARIABLE_PITCH | FF_ROMAN | FF_SWISS);
      font->set_face_name(GetPropotionalFontFaceForTest());
    } else {
      // Use monospaced font
      font->set_pitch_and_family(FIXED_PITCH | FF_ROMAN | FF_SWISS);
      font->set_face_name(GetMonospacedFontFaceForTest());
    }

    if (is_vertical) {
      font->set_escapement(2700);
      font->set_face_name("@" + font->face_name());
    }

    app->set_input_framework(ApplicationInfo::IMM32);
    {
      CompositionForm *composition_form = app->mutable_composition_form();
      composition_form->set_style_bits(CompositionForm::RECT);
      Point *current_position = composition_form->mutable_current_position();
      Rectangle *area = composition_form->mutable_area();
      if (is_vertical) {
        current_position->set_x(1526);
        current_position->set_y(385 + cursor_offset);
        area->set_left(567);
        area->set_top(170);
        area->set_right(1540);
        area->set_bottom(563);
      } else {
        current_position->set_x(1360 + cursor_offset);
        current_position->set_y(57);
        area->set_left(685);
        area->set_top(47);
        area->set_right(1523);
        area->set_bottom(580);
      }
    }

    {
      CandidateForm *candidate_layout = app->mutable_candidate_form();
      candidate_layout->set_style_bits(CandidateForm::CANDIDATEPOS);
      Rectangle *area = candidate_layout->mutable_area();
      area->set_left(567);
      area->set_top(67);
      area->set_right(1983755732);
      area->set_bottom(-781021488);
    }
  }
};

TEST_F(Win32RendererUtilTest, GetPointInPhysicalCoordsTest) {
  const CPoint kClientOffset(8, 42);
  const CSize kClientSize(100, 200);
  const CRect kWindowRect(1000, 500, 1116, 750);

  const CPoint kInnerPoint(1100, 600);
  const CPoint kOuterPoint(10, 300);

  // Check DPI scale: 100%
  {
    HWND hwnd = nullptr;
    LayoutManager layout_mgr(
        CreateDefaultGUIFontEmulator(),
        CreateWindowEmulator(kWindowClassName, kWindowRect, kClientOffset,
                             kClientSize, 1.0, &hwnd));

    // Conversion from an outer point should be calculated by emulation.
    CPoint dest;
    layout_mgr.GetPointInPhysicalCoords(hwnd, kOuterPoint, &dest);

    // Should be the same position because DPI scaling is 100%.
    EXPECT_EQ(kOuterPoint, dest);

    // Conversion from an inner point should be calculated by API.
    layout_mgr.GetPointInPhysicalCoords(hwnd, kInnerPoint, &dest);

    // Should be the same position because DPI scaling is 100%.
    EXPECT_EQ(kInnerPoint, dest);
  }

  // Check DPI scale: 200%
  {
    HWND hwnd = nullptr;
    LayoutManager layout_mgr(
        CreateDefaultGUIFontEmulator(),
        CreateWindowEmulator(kWindowClassName, kWindowRect, kClientOffset,
                             kClientSize, 2.0, &hwnd));

    // Conversion from an outer point should be calculated by emulation.
    CPoint dest;
    layout_mgr.GetPointInPhysicalCoords(hwnd, kOuterPoint, &dest);

    // Should be doubled because DPI scaling is 200%.
    EXPECT_EQ(CPoint(20, 600), dest);

    // Conversion from an inner point should be calculated by API.
    layout_mgr.GetPointInPhysicalCoords(hwnd, kInnerPoint, &dest);

    // Should be doubled because DPI scaling is 200%.
    EXPECT_EQ(CPoint(2200, 1200), dest);
  }
}

TEST_F(Win32RendererUtilTest, GetRectInPhysicalCoordsTest) {
  const CPoint kClientOffset(8, 42);
  const CSize kClientSize(100, 200);
  const CRect kWindowRect(1000, 500, 1116, 750);

  const CRect kInnerRect(1100, 600, 1070, 630);
  const CRect kOuterRect(10, 300, 1110, 630);

  // Check DPI scale: 100%
  {
    HWND hwnd = nullptr;
    LayoutManager layout_mgr(
        CreateDefaultGUIFontEmulator(),
        CreateWindowEmulator(kWindowClassName, kWindowRect, kClientOffset,
                             kClientSize, 1.0, &hwnd));

    // Conversion from an outer rectangle should be calculated by emulation.
    CRect dest;
    layout_mgr.GetRectInPhysicalCoords(hwnd, kOuterRect, &dest);

    // Should be the same rectangle because DPI scaling is 100%.
    EXPECT_EQ(kOuterRect, dest);

    // Conversion from an inner rectangle should be calculated by API.
    layout_mgr.GetRectInPhysicalCoords(hwnd, kInnerRect, &dest);

    // Should be the same rectangle because DPI scaling is 100%.
    EXPECT_EQ(kInnerRect, dest);
  }

  // Check DPI scale: 200%
  {
    HWND hwnd = nullptr;
    LayoutManager layout_mgr(
        CreateDefaultGUIFontEmulator(),
        CreateWindowEmulator(kWindowClassName, kWindowRect, kClientOffset,
                             kClientSize, 2.0, &hwnd));

    // Conversion from an outer rectangle should be calculated by emulation.
    CRect dest;
    layout_mgr.GetRectInPhysicalCoords(hwnd, kOuterRect, &dest);

    // Should be doubled because DPI scaling is 200%.
    EXPECT_EQ(CRect(20, 600, 2220, 1260), dest);

    // Conversion from an inner rectangle should be calculated by API.
    layout_mgr.GetRectInPhysicalCoords(hwnd, kInnerRect, &dest);

    // Should be doubled because DPI scaling is 200%.
    EXPECT_EQ(CRect(2200, 1200, 2140, 1260), dest);
  }
}

TEST_F(Win32RendererUtilTest, GetScalingFactorTest) {
  const double kScalingFactor = 1.5;

  {
    const CPoint kClientOffset(0, 0);
    const CSize kClientSize(100, 200);
    const CRect kWindowRect(1000, 500, 1100, 700);
    HWND hwnd = nullptr;
    LayoutManager layout_mgr(
        CreateDefaultGUIFontEmulator(),
        CreateWindowEmulator(kWindowClassName, kWindowRect, kClientOffset,
                             kClientSize, kScalingFactor, &hwnd));

    ASSERT_DOUBLE_EQ(kScalingFactor, layout_mgr.GetScalingFactor(hwnd));
  }

  // Zero Width
  {
    const CPoint kClientOffset(0, 0);
    const CSize kClientSize(0, 200);
    const CRect kWindowRect(1000, 500, 1000, 700);

    HWND hwnd = nullptr;
    LayoutManager layout_mgr(
        CreateDefaultGUIFontEmulator(),
        CreateWindowEmulator(kWindowClassName, kWindowRect, kClientOffset,
                             kClientSize, kScalingFactor, &hwnd));

    ASSERT_DOUBLE_EQ(kScalingFactor, layout_mgr.GetScalingFactor(hwnd));
  }

  // Zero Height
  {
    const CPoint kClientOffset(0, 0);
    const CSize kClientSize(100, 0);
    const CRect kWindowRect(1000, 500, 1100, 500);
    HWND hwnd = nullptr;
    LayoutManager layout_mgr(
        CreateDefaultGUIFontEmulator(),
        CreateWindowEmulator(kWindowClassName, kWindowRect, kClientOffset,
                             kClientSize, kScalingFactor, &hwnd));

    ASSERT_DOUBLE_EQ(kScalingFactor, layout_mgr.GetScalingFactor(hwnd));
  }

  // Zero Size
  {
    const CPoint kClientOffset(0, 0);
    const CSize kClientSize(0, 0);
    const CRect kWindowRect(1000, 500, 1000, 500);
    HWND hwnd = nullptr;
    LayoutManager layout_mgr(
        CreateDefaultGUIFontEmulator(),
        CreateWindowEmulator(kWindowClassName, kWindowRect, kClientOffset,
                             kClientSize, kScalingFactor, &hwnd));

    // If the window size is zero, the result should be fallen back 1.0.
    ASSERT_DOUBLE_EQ(1.0, layout_mgr.GetScalingFactor(hwnd));
  }
}

TEST_F(Win32RendererUtilTest, WindowPositionEmulatorTest) {
  const CPoint kClientOffset(8, 42);
  const CSize kClientSize(100, 200);
  const CRect kWindowRect(1000, 500, 1116, 750);

  const CPoint kInnerPoint(1100, 600);
  const CPoint kOuterPoint(10, 300);
  const CRect kInnerRect(1100, 600, 1070, 630);
  const CRect kOuterRect(10, 300, 1110, 630);

  // Check DPI scale: 100%
  {
    std::unique_ptr<WindowPositionEmulator> emulator(
        WindowPositionEmulator::Create());
    const HWND hwnd = emulator->RegisterWindow(kWindowClassName, kWindowRect,
                                               kClientOffset, kClientSize, 1.0);

    CRect rect;
    CPoint point;

    // You cannot pass nullptr to |window_handle|.
    EXPECT_FALSE(emulator->IsWindow(nullptr));
    EXPECT_FALSE(emulator->GetWindowRect(nullptr, &rect));
    EXPECT_FALSE(emulator->GetClientRect(nullptr, &rect));
    EXPECT_FALSE(emulator->ClientToScreen(nullptr, &point));

    EXPECT_TRUE(emulator->GetWindowRect(hwnd, &rect));
    EXPECT_EQ(kWindowRect, rect);

    EXPECT_TRUE(emulator->GetClientRect(hwnd, &rect));
    EXPECT_EQ(CRect(CPoint(0, 0), kClientSize), rect);

    point = CPoint(0, 0);
    EXPECT_TRUE(emulator->ClientToScreen(hwnd, &point));
    EXPECT_EQ(kWindowRect.TopLeft() + kClientOffset, point);

    std::wstring class_name;
    EXPECT_TRUE(emulator->GetWindowClassName(hwnd, &class_name));
    EXPECT_EQ(kWindowClassName, class_name);
  }

  // Interestingly, the following results are independent of DPI scaling.
  {
    std::unique_ptr<WindowPositionEmulator> emulator(
        WindowPositionEmulator::Create());
    const HWND hwnd = emulator->RegisterWindow(
        kWindowClassName, kWindowRect, kClientOffset, kClientSize, 10.0);

    CRect rect;
    CPoint point;

    // You cannot pass nullptr to |window_handle|.
    EXPECT_FALSE(emulator->IsWindow(nullptr));
    EXPECT_FALSE(emulator->GetWindowRect(nullptr, &rect));
    EXPECT_FALSE(emulator->GetClientRect(nullptr, &rect));
    EXPECT_FALSE(emulator->ClientToScreen(nullptr, &point));

    EXPECT_TRUE(emulator->GetWindowRect(hwnd, &rect));
    EXPECT_EQ(kWindowRect, rect);

    EXPECT_TRUE(emulator->GetClientRect(hwnd, &rect));
    EXPECT_EQ(CRect(CPoint(0, 0), kClientSize), rect);

    point = CPoint(0, 0);
    EXPECT_TRUE(emulator->ClientToScreen(hwnd, &point));
    EXPECT_EQ(kWindowRect.TopLeft() + kClientOffset, point);

    std::wstring class_name;
    EXPECT_TRUE(emulator->GetWindowClassName(hwnd, &class_name));
    EXPECT_EQ(kWindowClassName, class_name);
  }
}

TEST_F(Win32RendererUtilTest, HorizontalProportional) {
  const CLogFont &logfont = GetFont(true, false);

  std::vector<mozc::renderer::win32::LineLayout> line_layouts;
  bool result = true;

  const std::wstring &message = GetTestMessageForProportional();

  // Check if the |initial_offset| works as expected.
  result = LayoutManager::CalcLayoutWithTextWrapping(logfont, message, 200, 100,
                                                     &line_layouts);
  EXPECT_TRUE(result);
  EXPECT_EQ(4, line_layouts.size());
  EXPECT_EQ(line_layouts[0].line_width, line_layouts[1].line_width);
  EXPECT_EQ(line_layouts[1].line_width, line_layouts[2].line_width);
  EXPECT_EQ(line_layouts[2].line_width, line_layouts[3].line_width);

  // Check if the text wrapping occurs in the first line when
  // |initial_offset| > 0.  In this case, the line height of first line is
  // expected to be the same to that of the second line.
  result = LayoutManager::CalcLayoutWithTextWrapping(logfont, message, 200, 199,
                                                     &line_layouts);
  EXPECT_TRUE(result);
  EXPECT_EQ(4, line_layouts.size());
  EXPECT_EQ(0, line_layouts[0].line_length);
  EXPECT_EQ(0, line_layouts[0].text.size());
  EXPECT_EQ(0, line_layouts[0].character_positions.size());
  EXPECT_EQ(line_layouts[0].line_width, line_layouts[1].line_width);
  EXPECT_EQ(line_layouts[1].line_width, line_layouts[2].line_width);
  EXPECT_EQ(line_layouts[2].line_width, line_layouts[3].line_width);

  // Check if this function fails when there is no enough space for text
  // wrapping.
  result = LayoutManager::CalcLayoutWithTextWrapping(logfont, message, 2, 1,
                                                     &line_layouts);
  EXPECT_FALSE(result);

  // Check if an invalid |initial_offset| is detected as expected.
  result = LayoutManager::CalcLayoutWithTextWrapping(logfont, message, 200,
                                                     -100, &line_layouts);
  EXPECT_FALSE(result);

  // Check if an invalid |initial_offset| is detected as expected.
  result = LayoutManager::CalcLayoutWithTextWrapping(logfont, message, 200, 201,
                                                     &line_layouts);
  EXPECT_FALSE(result);

  // Check if an invalid |maximum_line_length| is detected as expected.
  result = LayoutManager::CalcLayoutWithTextWrapping(logfont, message, -1, 0,
                                                     &line_layouts);
  EXPECT_FALSE(result);

  // Check if an invalid |maximum_line_length| is detected as expected.
  result = LayoutManager::CalcLayoutWithTextWrapping(logfont, message, 0, 0,
                                                     &line_layouts);
  EXPECT_FALSE(result);
}

TEST_F(Win32RendererUtilTest, VerticalProportional) {
  const CLogFont &logfont = GetFont(true, true);

  std::vector<mozc::renderer::win32::LineLayout> line_layouts;
  bool result = true;

  const std::wstring &message = GetTestMessageForProportional();

  // Check if the |initial_offset| works as expected.
  result = LayoutManager::CalcLayoutWithTextWrapping(logfont, message, 200, 100,
                                                     &line_layouts);
  EXPECT_TRUE(result);
  EXPECT_EQ(4, line_layouts.size());
  EXPECT_EQ(line_layouts[0].line_width, line_layouts[1].line_width);
  EXPECT_EQ(line_layouts[1].line_width, line_layouts[2].line_width);
  EXPECT_EQ(line_layouts[2].line_width, line_layouts[3].line_width);

  // Check if the text wrapping occurs in the first line when
  // |initial_offset| > 0.  In this case, the line height of first line is
  // expected to be the same to that of the second line.
  result = LayoutManager::CalcLayoutWithTextWrapping(logfont, message, 200, 199,
                                                     &line_layouts);
  EXPECT_TRUE(result);
  EXPECT_EQ(4, line_layouts.size());
  EXPECT_EQ(0, line_layouts[0].line_length);
  EXPECT_EQ(0, line_layouts[0].text.size());
  EXPECT_EQ(0, line_layouts[0].character_positions.size());
  EXPECT_EQ(line_layouts[0].line_width, line_layouts[1].line_width);
  EXPECT_EQ(line_layouts[1].line_width, line_layouts[2].line_width);
  EXPECT_EQ(line_layouts[2].line_width, line_layouts[3].line_width);

  // Check if this function fails when there is no enough space for text
  // wrapping.
  result = LayoutManager::CalcLayoutWithTextWrapping(logfont, message, 2, 1,
                                                     &line_layouts);
  EXPECT_FALSE(result);

  // Check if an invalid |initial_offset| is detected as expected.
  result = LayoutManager::CalcLayoutWithTextWrapping(logfont, message, 200,
                                                     -100, &line_layouts);
  EXPECT_FALSE(result);

  // Check if an invalid |initial_offset| is detected as expected.
  result = LayoutManager::CalcLayoutWithTextWrapping(logfont, message, 200, 201,
                                                     &line_layouts);
  EXPECT_FALSE(result);

  // Check if an invalid |maximum_line_length| is detected as expected.
  result = LayoutManager::CalcLayoutWithTextWrapping(logfont, message, -1, 0,
                                                     &line_layouts);
  EXPECT_FALSE(result);

  // Check if an invalid |maximum_line_length| is detected as expected.
  result = LayoutManager::CalcLayoutWithTextWrapping(logfont, message, 0, 0,
                                                     &line_layouts);
  EXPECT_FALSE(result);
}

TEST_F(Win32RendererUtilTest, HorizontalMonospaced) {
  const CLogFont &logfont = GetFont(false, false);

  std::vector<mozc::renderer::win32::LineLayout> line_layouts;
  bool result = true;

  const std::wstring &message = GetTestMessageForMonospaced();

  // Check if the |initial_offset| works as expected.
  result = LayoutManager::CalcLayoutWithTextWrapping(logfont, message, 200, 100,
                                                     &line_layouts);
  EXPECT_TRUE(result);
  EXPECT_EQ(4, line_layouts.size());
  EXPECT_EQ(line_layouts[0].line_width, line_layouts[1].line_width);
  EXPECT_EQ(line_layouts[1].line_width, line_layouts[2].line_width);
  EXPECT_EQ(line_layouts[2].line_width, line_layouts[3].line_width);

  // Check if the text wrapping occurs in the first line when
  // |initial_offset| > 0.  In this case, the line height of first line is
  // expected to be the same to that of the second line.
  result = LayoutManager::CalcLayoutWithTextWrapping(logfont, message, 200, 199,
                                                     &line_layouts);
  EXPECT_TRUE(result);
  EXPECT_EQ(4, line_layouts.size());
  EXPECT_EQ(0, line_layouts[0].line_length);
  EXPECT_EQ(0, line_layouts[0].text.size());
  EXPECT_EQ(0, line_layouts[0].character_positions.size());
  EXPECT_EQ(line_layouts[0].line_width, line_layouts[1].line_width);
  EXPECT_EQ(line_layouts[1].line_width, line_layouts[2].line_width);
  EXPECT_EQ(line_layouts[2].line_width, line_layouts[3].line_width);

  // Check if this function fails when there is no enough space for text
  // wrapping.
  result = LayoutManager::CalcLayoutWithTextWrapping(logfont, message, 2, 1,
                                                     &line_layouts);
  EXPECT_FALSE(result);

  // Check if an invalid |initial_offset| is detected as expected.
  result = LayoutManager::CalcLayoutWithTextWrapping(logfont, message, 200,
                                                     -100, &line_layouts);
  EXPECT_FALSE(result);

  // Check if an invalid |initial_offset| is detected as expected.
  result = LayoutManager::CalcLayoutWithTextWrapping(logfont, message, 200, 201,
                                                     &line_layouts);
  EXPECT_FALSE(result);

  // Check if an invalid |maximum_line_length| is detected as expected.
  result = LayoutManager::CalcLayoutWithTextWrapping(logfont, message, -1, 0,
                                                     &line_layouts);
  EXPECT_FALSE(result);

  // Check if an invalid |maximum_line_length| is detected as expected.
  result = LayoutManager::CalcLayoutWithTextWrapping(logfont, message, 0, 0,
                                                     &line_layouts);
  EXPECT_FALSE(result);
}

TEST_F(Win32RendererUtilTest, VerticalMonospaced) {
  const CLogFont &logfont = GetFont(false, true);

  std::vector<mozc::renderer::win32::LineLayout> line_layouts;
  bool result = true;

  const std::wstring &message = GetTestMessageForMonospaced();

  // Check if the |initial_offset| works as expected.
  result = LayoutManager::CalcLayoutWithTextWrapping(logfont, message, 200, 100,
                                                     &line_layouts);
  EXPECT_TRUE(result);
  EXPECT_EQ(4, line_layouts.size());
  EXPECT_EQ(line_layouts[0].line_width, line_layouts[1].line_width);
  EXPECT_EQ(line_layouts[1].line_width, line_layouts[2].line_width);
  EXPECT_EQ(line_layouts[2].line_width, line_layouts[3].line_width);

  // Check if the text wrapping occurs in the first line when
  // |initial_offset| > 0.  In this case, the line height of first line is
  // expected to be the same to that of the second line.
  result = LayoutManager::CalcLayoutWithTextWrapping(logfont, message, 200, 199,
                                                     &line_layouts);
  EXPECT_TRUE(result);
  EXPECT_EQ(4, line_layouts.size());
  EXPECT_EQ(0, line_layouts[0].line_length);
  EXPECT_EQ(0, line_layouts[0].text.size());
  EXPECT_EQ(0, line_layouts[0].character_positions.size());
  EXPECT_EQ(line_layouts[0].line_width, line_layouts[1].line_width);
  EXPECT_EQ(line_layouts[1].line_width, line_layouts[2].line_width);
  EXPECT_EQ(line_layouts[2].line_width, line_layouts[3].line_width);

  // Check if this function fails when there is no enough space for text
  // wrapping.
  result = LayoutManager::CalcLayoutWithTextWrapping(logfont, message, 2, 1,
                                                     &line_layouts);
  EXPECT_FALSE(result);

  // Check if an invalid |initial_offset| is detected as expected.
  result = LayoutManager::CalcLayoutWithTextWrapping(logfont, message, 200,
                                                     -100, &line_layouts);
  EXPECT_FALSE(result);

  // Check if an invalid |initial_offset| is detected as expected.
  result = LayoutManager::CalcLayoutWithTextWrapping(logfont, message, 200, 201,
                                                     &line_layouts);
  EXPECT_FALSE(result);

  // Check if an invalid |maximum_line_length| is detected as expected.
  result = LayoutManager::CalcLayoutWithTextWrapping(logfont, message, -1, 0,
                                                     &line_layouts);
  EXPECT_FALSE(result);

  // Check if an invalid |maximum_line_length| is detected as expected.
  result = LayoutManager::CalcLayoutWithTextWrapping(logfont, message, 0, 0,
                                                     &line_layouts);
  EXPECT_FALSE(result);
}

TEST_F(Win32RendererUtilTest, HorizontalProportionalCompositeGlyph) {
  const CLogFont &logfont = GetFont(true, false);

  std::vector<mozc::renderer::win32::LineLayout> line_layouts;
  bool result = true;

  const std::wstring &message = GetTestMessageWithCompositeGlyph(1);

  result = LayoutManager::CalcLayoutWithTextWrapping(logfont, message, 200, 100,
                                                     &line_layouts);
  EXPECT_TRUE(result);
  EXPECT_EQ(1, line_layouts.size());

  // CalcLayoutWithTextWrapping does not support composition glyph.
  EXPECT_GT(line_layouts[0].character_positions[0].length, 0);
  EXPECT_EQ(line_layouts[0].character_positions[1].begin +
                line_layouts[0].character_positions[1].length,
            line_layouts[0].line_length);
}

TEST_F(Win32RendererUtilTest, VerticalProportionalCompositeGlyph) {
  const CLogFont &logfont = GetFont(true, true);

  std::vector<mozc::renderer::win32::LineLayout> line_layouts;
  bool result = true;

  const std::wstring &message = GetTestMessageWithCompositeGlyph(1);
  result = LayoutManager::CalcLayoutWithTextWrapping(logfont, message, 200, 100,
                                                     &line_layouts);
  EXPECT_TRUE(result);
  EXPECT_EQ(1, line_layouts.size());

  // CalcLayoutWithTextWrapping does not support composition glyph.
  EXPECT_GT(line_layouts[0].character_positions[0].length, 0);
  EXPECT_EQ(line_layouts[0].character_positions[1].begin +
                line_layouts[0].character_positions[1].length,
            line_layouts[0].line_length);
}

TEST_F(Win32RendererUtilTest, HorizontalMonospacedCompositeGlyph) {
  const CLogFont &logfont = GetFont(false, false);

  std::vector<mozc::renderer::win32::LineLayout> line_layouts;
  bool result = true;

  const std::wstring &message = GetTestMessageWithCompositeGlyph(1);

  result = LayoutManager::CalcLayoutWithTextWrapping(logfont, message, 200, 100,
                                                     &line_layouts);
  EXPECT_TRUE(result);
  EXPECT_EQ(1, line_layouts.size());

  // CalcLayoutWithTextWrapping does not support composition glyph.
  EXPECT_GT(line_layouts[0].character_positions[0].length, 0);
  EXPECT_EQ(line_layouts[0].character_positions[1].begin +
                line_layouts[0].character_positions[1].length,
            line_layouts[0].line_length);
}

TEST_F(Win32RendererUtilTest, VerticalMonospacedCompositeGlyph) {
  const CLogFont &logfont = GetFont(false, true);

  std::vector<mozc::renderer::win32::LineLayout> line_layouts;
  bool result = true;

  const std::wstring &message = GetTestMessageWithCompositeGlyph(1);

  result = LayoutManager::CalcLayoutWithTextWrapping(logfont, message, 200, 100,
                                                     &line_layouts);
  EXPECT_TRUE(result);
  EXPECT_EQ(1, line_layouts.size());

  // CalcLayoutWithTextWrapping does not support composition glyph.
  EXPECT_GT(line_layouts[0].character_positions[0].length, 0);
  EXPECT_EQ(line_layouts[0].character_positions[1].begin +
                line_layouts[0].character_positions[1].length,
            line_layouts[0].line_length);
}

TEST_F(Win32RendererUtilTest,
       CompositionHorizontalNoAdditionalSegmentationWithMonospacedFont) {
  const int kCursorOffsetX = 0;

  RendererCommand command;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(CreateDefaultGUIFontEmulator(),
                           CreateWindowEmulatorWithDPIScaling(1.0, &hwnd));
  std::vector<CompositionWindowLayout> layouts;
  CandidateWindowLayout candidate_layout;

  CLogFont logfont;

  bool result = false;

  // w/ candidates, monospaced, horizontal
  SetRenderereCommandForTest(false, true, false, kCursorOffsetX, hwnd,
                             &command);
  EXPECT_TRUE(mozc::win32::FontUtil::ToLOGFONT(
      command.application_info().composition_font(), &logfont));
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_TRUE(result);

  ASSERT_EQ(2, layouts.size());

  // The first line
  {
    const CompositionWindowLayout &layout = layouts.at(0);
    EXPECT_COMPOSITION_WINDOW_LAYOUT(1868, 599, 2003, 648, 0, 0, 135, 49, 0, 0,
                                     0, 0, 0, 0, logfont, layout);
    {
      const char kMsg[] = "これは";
      std::wstring msg;
      mozc::Util::UTF8ToWide(kMsg, &msg);
      EXPECT_EQ(msg, layout.text);
    }
    ASSERT_EQ(1, layout.marker_layouts.size());

    EXPECT_EQ(CPoint(0, 48), layout.marker_layouts[0].from);
    EXPECT_EQ(CPoint(126, 48), layout.marker_layouts[0].to);
    EXPECT_FALSE(layout.marker_layouts[0].highlighted);
  }

  // The second line
  {
    const CompositionWindowLayout &layout = layouts.at(1);
    EXPECT_COMPOSITION_WINDOW_LAYOUT(1193, 648, 1840, 697, 0, 0, 646, 49, 0, 0,
                                     646, 0, 647, 49, logfont, layout);
    {
      const char kMsg[] = "、Google日本語入力のTestです";
      std::wstring msg;
      mozc::Util::UTF8ToWide(kMsg, &msg);
      EXPECT_EQ(msg, layout.text);
    }
    ASSERT_EQ(4, layout.marker_layouts.size());

    EXPECT_EQ(CPoint(0, 48), layout.marker_layouts[0].from);
    EXPECT_EQ(CPoint(36, 48), layout.marker_layouts[0].to);
    EXPECT_FALSE(layout.marker_layouts[0].highlighted);
    EXPECT_EQ(CPoint(45, 48), layout.marker_layouts[1].from);
    EXPECT_EQ(CPoint(190, 48), layout.marker_layouts[1].to);
    EXPECT_TRUE(layout.marker_layouts[1].highlighted);
    EXPECT_EQ(CPoint(196, 48), layout.marker_layouts[2].from);
    EXPECT_EQ(CPoint(457, 48), layout.marker_layouts[2].to);
    EXPECT_FALSE(layout.marker_layouts[2].highlighted);
    EXPECT_EQ(CPoint(466, 48), layout.marker_layouts[3].from);
    EXPECT_EQ(CPoint(646, 48), layout.marker_layouts[3].to);
    EXPECT_FALSE(layout.marker_layouts[3].highlighted);
  }
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1238, 697, 1238, 648, 1839, 697,
                                         candidate_layout);

  // Check other candidate positions.
  command.mutable_output()->mutable_candidates()->set_position(0);
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1868, 648, 1868, 599, 2003, 648,
                                         candidate_layout);

  command.mutable_output()->mutable_candidates()->set_position(3);
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1193, 697, 1193, 648, 1839, 697,
                                         candidate_layout);

  command.mutable_output()->mutable_candidates()->set_position(10);
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1389, 697, 1389, 648, 1839, 697,
                                         candidate_layout);

  command.mutable_output()->mutable_candidates()->set_position(16);
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1659, 697, 1659, 648, 1839, 697,
                                         candidate_layout);

  // w/o candidates, monospaced, horizontal
  SetRenderereCommandForTest(false, false, false, 0, hwnd, &command);
  EXPECT_TRUE(mozc::win32::FontUtil::ToLOGFONT(
      command.application_info().composition_font(), &logfont));
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_TRUE(result);
  EXPECT_FALSE(candidate_layout.initialized());
}

TEST_F(Win32RendererUtilTest,
       CompositionHorizontalAdditionalSegmentationWithMonospacedFont) {
  const int kCursorOffsetX = -90;

  RendererCommand command;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(CreateDefaultGUIFontEmulator(),
                           CreateWindowEmulatorWithDPIScaling(1.0, &hwnd));
  std::vector<CompositionWindowLayout> layouts;
  CandidateWindowLayout candidate_layout;
  CLogFont logfont;

  bool result = false;

  // w/ candidates, monospaced, horizontal
  SetRenderereCommandForTest(false, true, false, kCursorOffsetX, hwnd,
                             &command);
  EXPECT_TRUE(mozc::win32::FontUtil::ToLOGFONT(
      command.application_info().composition_font(), &logfont));
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_TRUE(result);

  ASSERT_EQ(2, layouts.size());

  // The first line
  {
    const CompositionWindowLayout &layout = layouts.at(0);
    EXPECT_COMPOSITION_WINDOW_LAYOUT(1778, 599, 2019, 648, 0, 0, 241, 49, 0, 0,
                                     0, 0, 0, 0, logfont, layout);
    {
      const char kMsg[] = "これは、Go";
      std::wstring msg;
      mozc::Util::UTF8ToWide(kMsg, &msg);
      EXPECT_EQ(msg, layout.text);
    }
    ASSERT_EQ(3, layout.marker_layouts.size());

    EXPECT_EQ(CPoint(0, 48), layout.marker_layouts[0].from);
    EXPECT_EQ(CPoint(126, 48), layout.marker_layouts[0].to);
    EXPECT_FALSE(layout.marker_layouts[0].highlighted);
    EXPECT_EQ(CPoint(135, 48), layout.marker_layouts[1].from);
    EXPECT_EQ(CPoint(171, 48), layout.marker_layouts[1].to);
    EXPECT_FALSE(layout.marker_layouts[1].highlighted);
    EXPECT_EQ(CPoint(180, 48), layout.marker_layouts[2].from);
    EXPECT_EQ(CPoint(241, 48), layout.marker_layouts[2].to);
    EXPECT_TRUE(layout.marker_layouts[2].highlighted);
  }

  // The second line
  {
    const CompositionWindowLayout &layout = layouts.at(1);
    EXPECT_COMPOSITION_WINDOW_LAYOUT(1193, 648, 1734, 697, 0, 0, 540, 49, 0, 0,
                                     540, 0, 541, 49, logfont, layout);
    {
      const char kMsg[] = "ogle日本語入力のTestです";
      std::wstring msg;
      mozc::Util::UTF8ToWide(kMsg, &msg);
      EXPECT_EQ(msg, layout.text);
    }
    ASSERT_EQ(3, layout.marker_layouts.size());

    EXPECT_EQ(CPoint(0, 48), layout.marker_layouts[0].from);
    EXPECT_EQ(CPoint(84, 48), layout.marker_layouts[0].to);
    EXPECT_TRUE(layout.marker_layouts[0].highlighted);
    EXPECT_EQ(CPoint(90, 48), layout.marker_layouts[1].from);
    EXPECT_EQ(CPoint(351, 48), layout.marker_layouts[1].to);
    EXPECT_FALSE(layout.marker_layouts[1].highlighted);
    EXPECT_EQ(CPoint(360, 48), layout.marker_layouts[2].from);
    EXPECT_EQ(CPoint(540, 48), layout.marker_layouts[2].to);
    EXPECT_FALSE(layout.marker_layouts[2].highlighted);
  }

  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1958, 648, 1958, 599, 2019, 648,
                                         candidate_layout);

  // Check other candidate positions.
  command.mutable_output()->mutable_candidates()->set_position(0);
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1778, 648, 1778, 599, 2019, 648,
                                         candidate_layout);

  command.mutable_output()->mutable_candidates()->set_position(3);
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1913, 648, 1913, 599, 2019, 648,
                                         candidate_layout);

  command.mutable_output()->mutable_candidates()->set_position(10);
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1283, 697, 1283, 648, 1733, 697,
                                         candidate_layout);

  command.mutable_output()->mutable_candidates()->set_position(16);
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1553, 697, 1553, 648, 1733, 697,
                                         candidate_layout);

  // w/o candidates, monospaced, horizontal
  SetRenderereCommandForTest(false, false, false, 0, hwnd, &command);
  EXPECT_TRUE(mozc::win32::FontUtil::ToLOGFONT(
      command.application_info().composition_font(), &logfont));
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_TRUE(result);
  EXPECT_FALSE(candidate_layout.initialized());
}

TEST_F(Win32RendererUtilTest,
       CompositionVerticalNoAdditionalSegmentationWithMonospacedFont) {
  const int kCursorOffsetY = 0;

  RendererCommand command;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(CreateDefaultGUIFontEmulator(),
                           CreateWindowEmulatorWithDPIScaling(1.0, &hwnd));
  std::vector<CompositionWindowLayout> layouts;
  CandidateWindowLayout candidate_layout;
  CLogFont logfont;

  bool result = false;

  // w/ candidates, monospaced, vertical
  SetRenderereCommandForTest(false, true, true, kCursorOffsetY, hwnd, &command);
  EXPECT_TRUE(mozc::win32::FontUtil::ToLOGFONT(
      command.application_info().composition_font(), &logfont));
  logfont.lfOrientation = 2700;

  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_TRUE(result);

  ASSERT_EQ(3, layouts.size());

  // The first line
  {
    const CompositionWindowLayout &layout = layouts.at(0);
    EXPECT_COMPOSITION_WINDOW_LAYOUT(1983, 927, 2034, 1062, 0, 0, 51, 135, 51,
                                     0, 0, 0, 0, 0, logfont, layout);
    {
      const char kMsg[] = "これは";
      std::wstring msg;
      mozc::Util::UTF8ToWide(kMsg, &msg);
      EXPECT_EQ(msg, layout.text);
    }
    ASSERT_EQ(1, layout.marker_layouts.size());

    EXPECT_EQ(CPoint(50, 0), layout.marker_layouts[0].from);
    EXPECT_EQ(CPoint(50, 126), layout.marker_layouts[0].to);
    EXPECT_FALSE(layout.marker_layouts[0].highlighted);
  }

  // The second line
  {
    const CompositionWindowLayout &layout = layouts.at(1);
    EXPECT_COMPOSITION_WINDOW_LAYOUT(1932, 712, 1983, 1088, 0, 0, 51, 376, 51,
                                     0, 0, 0, 0, 0, logfont, layout);
    {
      const char kMsg[] = "、Google日本語入";
      std::wstring msg;
      mozc::Util::UTF8ToWide(kMsg, &msg);
      EXPECT_EQ(msg, layout.text);
    }
    ASSERT_EQ(3, layout.marker_layouts.size());

    EXPECT_EQ(CPoint(50, 0), layout.marker_layouts[0].from);
    EXPECT_EQ(CPoint(50, 36), layout.marker_layouts[0].to);
    EXPECT_FALSE(layout.marker_layouts[0].highlighted);
    EXPECT_EQ(CPoint(50, 45), layout.marker_layouts[1].from);
    EXPECT_EQ(CPoint(50, 190), layout.marker_layouts[1].to);
    EXPECT_TRUE(layout.marker_layouts[1].highlighted);
    EXPECT_EQ(CPoint(50, 196), layout.marker_layouts[2].from);
    EXPECT_EQ(CPoint(50, 376), layout.marker_layouts[2].to);
    EXPECT_FALSE(layout.marker_layouts[2].highlighted);
  }

  // The third line
  {
    const CompositionWindowLayout &layout = layouts.at(2);
    EXPECT_COMPOSITION_WINDOW_LAYOUT(1881, 712, 1932, 983, 0, 0, 51, 270, 51, 0,
                                     0, 270, 51, 271, logfont, layout);
    {
      const char kMsg[] = "力のTestです";
      std::wstring msg;
      mozc::Util::UTF8ToWide(kMsg, &msg);
      EXPECT_EQ(msg, layout.text);
    }
    ASSERT_EQ(2, layout.marker_layouts.size());

    EXPECT_EQ(CPoint(50, 0), layout.marker_layouts[0].from);
    EXPECT_EQ(CPoint(50, 81), layout.marker_layouts[0].to);
    EXPECT_FALSE(layout.marker_layouts[0].highlighted);
    EXPECT_EQ(CPoint(50, 90), layout.marker_layouts[1].from);
    EXPECT_EQ(CPoint(50, 270), layout.marker_layouts[1].to);
    EXPECT_FALSE(layout.marker_layouts[1].highlighted);
  }

  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1932, 757, 1932, 757, 1983, 1088,
                                         candidate_layout);

  // Check other candidate positions.
  command.mutable_output()->mutable_candidates()->set_position(0);
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1983, 927, 1983, 927, 2034, 1062,
                                         candidate_layout);

  command.mutable_output()->mutable_candidates()->set_position(3);
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1932, 712, 1932, 712, 1983, 1088,
                                         candidate_layout);

  command.mutable_output()->mutable_candidates()->set_position(10);
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1932, 908, 1932, 908, 1983, 1088,
                                         candidate_layout);

  command.mutable_output()->mutable_candidates()->set_position(16);
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1881, 802, 1881, 802, 1932, 982,
                                         candidate_layout);

  // w/o candidates, monospaced, vertical
  SetRenderereCommandForTest(false, false, true, 0, hwnd, &command);
  EXPECT_TRUE(mozc::win32::FontUtil::ToLOGFONT(
      command.application_info().composition_font(), &logfont));
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_TRUE(result);
  EXPECT_FALSE(candidate_layout.initialized());
}

TEST_F(Win32RendererUtilTest,
       CompositionVerticalAdditionalSegmentationWithMonospacedFont) {
  const int kCursorOffsetY = -90;

  RendererCommand command;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(CreateDefaultGUIFontEmulator(),
                           CreateWindowEmulatorWithDPIScaling(1.0, &hwnd));
  std::vector<CompositionWindowLayout> layouts;
  CandidateWindowLayout candidate_layout;
  CLogFont logfont;

  bool result = false;

  // w/ candidates, monospaced, vertical
  SetRenderereCommandForTest(false, true, true, kCursorOffsetY, hwnd, &command);
  EXPECT_TRUE(mozc::win32::FontUtil::ToLOGFONT(
      command.application_info().composition_font(), &logfont));
  logfont.lfOrientation = 2700;

  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_TRUE(result);

  ASSERT_EQ(3, layouts.size());

  // The first line
  {
    const CompositionWindowLayout &layout = layouts.at(0);
    EXPECT_COMPOSITION_WINDOW_LAYOUT(1983, 837, 2034, 1105, 0, 0, 51, 268, 51,
                                     0, 0, 0, 0, 0, logfont, layout);
    {
      const char kMsg[] = "これは、Goo";
      std::wstring msg;
      mozc::Util::UTF8ToWide(kMsg, &msg);
      EXPECT_EQ(msg, layout.text);
    }
    ASSERT_EQ(3, layout.marker_layouts.size());

    EXPECT_EQ(CPoint(50, 0), layout.marker_layouts[0].from);
    EXPECT_EQ(CPoint(50, 126), layout.marker_layouts[0].to);
    EXPECT_FALSE(layout.marker_layouts[0].highlighted);
    EXPECT_EQ(CPoint(50, 135), layout.marker_layouts[1].from);
    EXPECT_EQ(CPoint(50, 171), layout.marker_layouts[1].to);
    EXPECT_FALSE(layout.marker_layouts[1].highlighted);
    EXPECT_EQ(CPoint(50, 180), layout.marker_layouts[2].from);
    EXPECT_EQ(CPoint(50, 268), layout.marker_layouts[2].to);
    EXPECT_TRUE(layout.marker_layouts[2].highlighted);
  }

  // The second line
  {
    const CompositionWindowLayout &layout = layouts.at(1);
    EXPECT_COMPOSITION_WINDOW_LAYOUT(1932, 712, 1983, 1098, 0, 0, 51, 386, 51,
                                     0, 0, 0, 0, 0, logfont, layout);
    {
      const char kMsg[] = "gle日本語入力のTe";
      std::wstring msg;
      mozc::Util::UTF8ToWide(kMsg, &msg);
      EXPECT_EQ(msg, layout.text);
    }
    ASSERT_EQ(3, layout.marker_layouts.size());

    EXPECT_EQ(CPoint(50, 0), layout.marker_layouts[0].from);
    EXPECT_EQ(CPoint(50, 57), layout.marker_layouts[0].to);
    EXPECT_TRUE(layout.marker_layouts[0].highlighted);
    EXPECT_EQ(CPoint(50, 63), layout.marker_layouts[1].from);
    EXPECT_EQ(CPoint(50, 324), layout.marker_layouts[1].to);
    EXPECT_FALSE(layout.marker_layouts[1].highlighted);
    EXPECT_EQ(CPoint(50, 333), layout.marker_layouts[2].from);
    EXPECT_EQ(CPoint(50, 386), layout.marker_layouts[2].to);
    EXPECT_FALSE(layout.marker_layouts[2].highlighted);
  }

  // The third line
  {
    const CompositionWindowLayout &layout = layouts.at(2);
    EXPECT_COMPOSITION_WINDOW_LAYOUT(1881, 712, 1932, 840, 0, 0, 51, 127, 51, 0,
                                     0, 127, 51, 128, logfont, layout);
    {
      const char kMsg[] = "stです";
      std::wstring msg;
      mozc::Util::UTF8ToWide(kMsg, &msg);
      EXPECT_EQ(msg, layout.text);
    }
    ASSERT_EQ(1, layout.marker_layouts.size());

    EXPECT_EQ(CPoint(50, 0), layout.marker_layouts[0].from);
    EXPECT_EQ(CPoint(50, 127), layout.marker_layouts[0].to);
    EXPECT_FALSE(layout.marker_layouts[0].highlighted);
  }

  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1983, 1017, 1983, 1017, 2034, 1105,
                                         candidate_layout);

  // Check other candidate positions.
  command.mutable_output()->mutable_candidates()->set_position(0);
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1983, 837, 1983, 837, 2034, 1105,
                                         candidate_layout);

  command.mutable_output()->mutable_candidates()->set_position(3);
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1983, 972, 1983, 972, 2034, 1105,
                                         candidate_layout);

  command.mutable_output()->mutable_candidates()->set_position(10);
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1932, 775, 1932, 775, 1983, 1098,
                                         candidate_layout);

  command.mutable_output()->mutable_candidates()->set_position(16);
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1932, 1045, 1932, 1045, 1983, 1098,
                                         candidate_layout);

  // w/o candidates, monospaced, vertical
  SetRenderereCommandForTest(false, false, true, 0, hwnd, &command);
  EXPECT_TRUE(mozc::win32::FontUtil::ToLOGFONT(
      command.application_info().composition_font(), &logfont));
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_TRUE(result);
  EXPECT_FALSE(candidate_layout.initialized());
}

TEST_F(Win32RendererUtilTest,
       CompositionHorizontalNoAdditionalSegmentationWithProportionalFont) {
  const int kCursorOffsetX = 0;

  RendererCommand command;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(CreateDefaultGUIFontEmulator(),
                           CreateWindowEmulatorWithDPIScaling(1.0, &hwnd));
  std::vector<CompositionWindowLayout> layouts;
  CandidateWindowLayout candidate_layout;
  CLogFont logfont;

  bool result = false;

  // w/ candidates, proportional, horizontal
  SetRenderereCommandForTest(true, true, false, kCursorOffsetX, hwnd, &command);
  EXPECT_TRUE(mozc::win32::FontUtil::ToLOGFONT(
      command.application_info().composition_font(), &logfont));
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_TRUE(result);

  ASSERT_EQ(2, layouts.size());

  // The first line
  {
    const CompositionWindowLayout &layout = layouts.at(0);
    EXPECT_COMPOSITION_WINDOW_LAYOUT(1868, 599, 2003, 653, 0, 0, 135, 54, 0, 0,
                                     0, 0, 0, 0, logfont, layout);
    {
      const char kMsg[] = "これは";
      std::wstring msg;
      mozc::Util::UTF8ToWide(kMsg, &msg);
      EXPECT_EQ(msg, layout.text);
    }
    ASSERT_EQ(1, layout.marker_layouts.size());

    EXPECT_EQ(CPoint(0, 53), layout.marker_layouts[0].from);
    EXPECT_EQ(CPoint(126, 53), layout.marker_layouts[0].to);
    EXPECT_FALSE(layout.marker_layouts[0].highlighted);
  }

  // The second line
  {
    const CompositionWindowLayout &layout = layouts.at(1);
    EXPECT_COMPOSITION_WINDOW_LAYOUT(1193, 653, 1840, 707, 0, 0, 646, 54, 0, 0,
                                     646, 0, 647, 54, logfont, layout);
    {
      const char kMsg[] = "、Google日本語入力のTestです";
      std::wstring msg;
      mozc::Util::UTF8ToWide(kMsg, &msg);
      EXPECT_EQ(msg, layout.text);
    }
    ASSERT_EQ(4, layout.marker_layouts.size());

    EXPECT_EQ(CPoint(0, 53), layout.marker_layouts[0].from);
    EXPECT_EQ(CPoint(36, 53), layout.marker_layouts[0].to);
    EXPECT_FALSE(layout.marker_layouts[0].highlighted);
    EXPECT_EQ(CPoint(45, 53), layout.marker_layouts[1].from);
    EXPECT_EQ(CPoint(192, 53), layout.marker_layouts[1].to);
    EXPECT_TRUE(layout.marker_layouts[1].highlighted);
    EXPECT_EQ(CPoint(197, 53), layout.marker_layouts[2].from);
    EXPECT_EQ(CPoint(458, 53), layout.marker_layouts[2].to);
    EXPECT_FALSE(layout.marker_layouts[2].highlighted);
    EXPECT_EQ(CPoint(467, 53), layout.marker_layouts[3].from);
    EXPECT_EQ(CPoint(646, 53), layout.marker_layouts[3].to);
    EXPECT_FALSE(layout.marker_layouts[3].highlighted);
  }

  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1238, 707, 1238, 653, 1839, 707,
                                         candidate_layout);

  // Check other candidate positions.
  command.mutable_output()->mutable_candidates()->set_position(0);
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1868, 653, 1868, 599, 2003, 653,
                                         candidate_layout);

  command.mutable_output()->mutable_candidates()->set_position(3);
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1193, 707, 1193, 653, 1839, 707,
                                         candidate_layout);

  command.mutable_output()->mutable_candidates()->set_position(10);
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1390, 707, 1390, 653, 1839, 707,
                                         candidate_layout);

  command.mutable_output()->mutable_candidates()->set_position(16);
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1660, 707, 1660, 653, 1839, 707,
                                         candidate_layout);

  // w/o candidates, proportional, horizontal
  SetRenderereCommandForTest(true, false, false, 0, hwnd, &command);
  EXPECT_TRUE(mozc::win32::FontUtil::ToLOGFONT(
      command.application_info().composition_font(), &logfont));
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_TRUE(result);
  EXPECT_FALSE(candidate_layout.initialized());
}

TEST_F(Win32RendererUtilTest,
       CompositionHorizontalAdditionalSegmentationWithProportionalFont) {
  const int kCursorOffsetX = -90;

  RendererCommand command;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(CreateDefaultGUIFontEmulator(),
                           CreateWindowEmulatorWithDPIScaling(1.0, &hwnd));
  std::vector<CompositionWindowLayout> layouts;
  CandidateWindowLayout candidate_layout;
  CLogFont logfont;

  bool result = false;

  // w/ candidates, proportional, horizontal
  SetRenderereCommandForTest(true, true, false, kCursorOffsetX, hwnd, &command);
  EXPECT_TRUE(mozc::win32::FontUtil::ToLOGFONT(
      command.application_info().composition_font(), &logfont));
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_TRUE(result);

  ASSERT_EQ(2, layouts.size());

  // The first line
  {
    const CompositionWindowLayout &layout = layouts.at(0);
    EXPECT_COMPOSITION_WINDOW_LAYOUT(1778, 599, 2020, 653, 0, 0, 242, 54, 0, 0,
                                     0, 0, 0, 0, logfont, layout);
    {
      const char kMsg[] = "これは、Go";
      std::wstring msg;
      mozc::Util::UTF8ToWide(kMsg, &msg);
      EXPECT_EQ(msg, layout.text);
    }
    ASSERT_EQ(3, layout.marker_layouts.size());

    EXPECT_EQ(CPoint(0, 53), layout.marker_layouts[0].from);
    EXPECT_EQ(CPoint(126, 53), layout.marker_layouts[0].to);
    EXPECT_FALSE(layout.marker_layouts[0].highlighted);
    EXPECT_EQ(CPoint(135, 53), layout.marker_layouts[1].from);
    EXPECT_EQ(CPoint(171, 53), layout.marker_layouts[1].to);
    EXPECT_FALSE(layout.marker_layouts[1].highlighted);
    EXPECT_EQ(CPoint(180, 53), layout.marker_layouts[2].from);
    EXPECT_EQ(CPoint(242, 53), layout.marker_layouts[2].to);
    EXPECT_TRUE(layout.marker_layouts[2].highlighted);
  }

  // The second line
  {
    const CompositionWindowLayout &layout = layouts.at(1);
    EXPECT_COMPOSITION_WINDOW_LAYOUT(1193, 653, 1733, 707, 0, 0, 539, 54, 0, 0,
                                     539, 0, 540, 54, logfont, layout);
    {
      const char kMsg[] = "ogle日本語入力のTestです";
      std::wstring msg;
      mozc::Util::UTF8ToWide(kMsg, &msg);
      EXPECT_EQ(msg, layout.text);
    }
    ASSERT_EQ(3, layout.marker_layouts.size());

    EXPECT_EQ(CPoint(0, 53), layout.marker_layouts[0].from);
    EXPECT_EQ(CPoint(85, 53), layout.marker_layouts[0].to);
    EXPECT_TRUE(layout.marker_layouts[0].highlighted);
    EXPECT_EQ(CPoint(90, 53), layout.marker_layouts[1].from);
    EXPECT_EQ(CPoint(351, 53), layout.marker_layouts[1].to);
    EXPECT_FALSE(layout.marker_layouts[1].highlighted);
    EXPECT_EQ(CPoint(360, 53), layout.marker_layouts[2].from);
    EXPECT_EQ(CPoint(539, 53), layout.marker_layouts[2].to);
    EXPECT_FALSE(layout.marker_layouts[2].highlighted);
  }

  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1958, 653, 1958, 599, 2020, 653,
                                         candidate_layout);

  // Check other candidate positions.
  command.mutable_output()->mutable_candidates()->set_position(0);
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1778, 653, 1778, 599, 2020, 653,
                                         candidate_layout);

  command.mutable_output()->mutable_candidates()->set_position(3);
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1913, 653, 1913, 599, 2020, 653,
                                         candidate_layout);

  command.mutable_output()->mutable_candidates()->set_position(10);
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1283, 707, 1283, 653, 1732, 707,
                                         candidate_layout);

  command.mutable_output()->mutable_candidates()->set_position(16);
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1553, 707, 1553, 653, 1732, 707,
                                         candidate_layout);

  // w/o candidates, proportional, horizontal
  SetRenderereCommandForTest(true, false, false, 0, hwnd, &command);
  EXPECT_TRUE(mozc::win32::FontUtil::ToLOGFONT(
      command.application_info().composition_font(), &logfont));
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_TRUE(result);
  EXPECT_FALSE(candidate_layout.initialized());
}

TEST_F(Win32RendererUtilTest,
       CompositionVerticalNoAdditionalSegmentationWithProportionalFont) {
  const int kCursorOffsetY = 0;

  RendererCommand command;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(CreateDefaultGUIFontEmulator(),
                           CreateWindowEmulatorWithDPIScaling(1.0, &hwnd));
  std::vector<CompositionWindowLayout> layouts;
  CandidateWindowLayout candidate_layout;
  CLogFont logfont;

  bool result = false;

  // w/ candidates, proportional, vertical
  SetRenderereCommandForTest(true, true, true, kCursorOffsetY, hwnd, &command);
  EXPECT_TRUE(mozc::win32::FontUtil::ToLOGFONT(
      command.application_info().composition_font(), &logfont));
  logfont.lfOrientation = 2700;

  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_TRUE(result);

  ASSERT_EQ(3, layouts.size());

  // The first line
  {
    const CompositionWindowLayout &layout = layouts.at(0);
    EXPECT_COMPOSITION_WINDOW_LAYOUT(1978, 927, 2034, 1062, 0, 0, 56, 135, 56,
                                     0, 0, 0, 0, 0, logfont, layout);
    {
      const char kMsg[] = "これは";
      std::wstring msg;
      mozc::Util::UTF8ToWide(kMsg, &msg);
      EXPECT_EQ(msg, layout.text);
    }
    ASSERT_EQ(1, layout.marker_layouts.size());

    EXPECT_EQ(CPoint(55, 0), layout.marker_layouts[0].from);
    EXPECT_EQ(CPoint(55, 126), layout.marker_layouts[0].to);
    EXPECT_FALSE(layout.marker_layouts[0].highlighted);
  }

  // The second line
  {
    const CompositionWindowLayout &layout = layouts.at(1);
    EXPECT_COMPOSITION_WINDOW_LAYOUT(1922, 712, 1978, 1089, 0, 0, 56, 377, 56,
                                     0, 0, 0, 0, 0, logfont, layout);
    {
      const char kMsg[] = "、Google日本語入";
      std::wstring msg;
      mozc::Util::UTF8ToWide(kMsg, &msg);
      EXPECT_EQ(msg, layout.text);
    }
    ASSERT_EQ(3, layout.marker_layouts.size());

    EXPECT_EQ(CPoint(55, 0), layout.marker_layouts[0].from);
    EXPECT_EQ(CPoint(55, 36), layout.marker_layouts[0].to);
    EXPECT_FALSE(layout.marker_layouts[0].highlighted);
    EXPECT_EQ(CPoint(55, 45), layout.marker_layouts[1].from);
    EXPECT_EQ(CPoint(55, 192), layout.marker_layouts[1].to);
    EXPECT_TRUE(layout.marker_layouts[1].highlighted);
    EXPECT_EQ(CPoint(55, 197), layout.marker_layouts[2].from);
    EXPECT_EQ(CPoint(55, 377), layout.marker_layouts[2].to);
    EXPECT_FALSE(layout.marker_layouts[2].highlighted);
  }

  // The third line
  {
    const CompositionWindowLayout &layout = layouts.at(2);
    EXPECT_COMPOSITION_WINDOW_LAYOUT(1866, 712, 1922, 982, 0, 0, 56, 269, 56, 0,
                                     0, 269, 56, 270, logfont, layout);
    {
      const char kMsg[] = "力のTestです";
      std::wstring msg;
      mozc::Util::UTF8ToWide(kMsg, &msg);
      EXPECT_EQ(msg, layout.text);
    }
    ASSERT_EQ(2, layout.marker_layouts.size());

    EXPECT_EQ(CPoint(55, 0), layout.marker_layouts[0].from);
    EXPECT_EQ(CPoint(55, 81), layout.marker_layouts[0].to);
    EXPECT_FALSE(layout.marker_layouts[0].highlighted);
    EXPECT_EQ(CPoint(55, 90), layout.marker_layouts[1].from);
    EXPECT_EQ(CPoint(55, 269), layout.marker_layouts[1].to);
    EXPECT_FALSE(layout.marker_layouts[1].highlighted);
  }

  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1922, 757, 1922, 757, 1978, 1089,
                                         candidate_layout);

  // Check other candidate positions.
  command.mutable_output()->mutable_candidates()->set_position(0);
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1978, 927, 1978, 927, 2034, 1062,
                                         candidate_layout);

  command.mutable_output()->mutable_candidates()->set_position(3);
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1922, 712, 1922, 712, 1978, 1089,
                                         candidate_layout);

  command.mutable_output()->mutable_candidates()->set_position(10);
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1922, 909, 1922, 909, 1978, 1089,
                                         candidate_layout);

  command.mutable_output()->mutable_candidates()->set_position(16);
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1866, 802, 1866, 802, 1922, 981,
                                         candidate_layout);

  // w/o candidates, proportional, vertical
  SetRenderereCommandForTest(true, false, true, 0, hwnd, &command);
  EXPECT_TRUE(mozc::win32::FontUtil::ToLOGFONT(
      command.application_info().composition_font(), &logfont));
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_TRUE(result);
  EXPECT_FALSE(candidate_layout.initialized());
}

TEST_F(Win32RendererUtilTest,
       CompositionVerticalAdditionalSegmentationWithProportionalFont) {
  const int kCursorOffsetY = -90;

  RendererCommand command;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(CreateDefaultGUIFontEmulator(),
                           CreateWindowEmulatorWithDPIScaling(1.0, &hwnd));
  std::vector<CompositionWindowLayout> layouts;
  CandidateWindowLayout candidate_layout;
  CLogFont logfont;

  bool result = false;

  // w/ candidates, proportional, vertical
  SetRenderereCommandForTest(true, true, true, kCursorOffsetY, hwnd, &command);
  EXPECT_TRUE(mozc::win32::FontUtil::ToLOGFONT(
      command.application_info().composition_font(), &logfont));
  logfont.lfOrientation = 2700;

  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_TRUE(result);

  ASSERT_EQ(3, layouts.size());

  // The first line
  {
    const CompositionWindowLayout &layout = layouts.at(0);
    EXPECT_COMPOSITION_WINDOW_LAYOUT(1978, 837, 2034, 1079, 0, 0, 56, 242, 56,
                                     0, 0, 0, 0, 0, logfont, layout);
    {
      const char kMsg[] = "これは、Go";
      std::wstring msg;
      mozc::Util::UTF8ToWide(kMsg, &msg);
      EXPECT_EQ(msg, layout.text);
    }
    ASSERT_EQ(3, layout.marker_layouts.size());

    EXPECT_EQ(CPoint(55, 0), layout.marker_layouts[0].from);
    EXPECT_EQ(CPoint(55, 126), layout.marker_layouts[0].to);
    EXPECT_FALSE(layout.marker_layouts[0].highlighted);
    EXPECT_EQ(CPoint(55, 135), layout.marker_layouts[1].from);
    EXPECT_EQ(CPoint(55, 171), layout.marker_layouts[1].to);
    EXPECT_FALSE(layout.marker_layouts[1].highlighted);
    EXPECT_EQ(CPoint(55, 180), layout.marker_layouts[2].from);
    EXPECT_EQ(CPoint(55, 242), layout.marker_layouts[2].to);
    EXPECT_TRUE(layout.marker_layouts[2].highlighted);
  }

  // The second line
  {
    const CompositionWindowLayout &layout = layouts.at(1);
    EXPECT_COMPOSITION_WINDOW_LAYOUT(1922, 712, 1978, 1100, 0, 0, 56, 388, 56,
                                     0, 0, 0, 0, 0, logfont, layout);
    {
      const char kMsg[] = "ogle日本語入力のT";
      std::wstring msg;
      mozc::Util::UTF8ToWide(kMsg, &msg);
      EXPECT_EQ(msg, layout.text);
    }
    ASSERT_EQ(3, layout.marker_layouts.size());

    EXPECT_EQ(CPoint(55, 0), layout.marker_layouts[0].from);
    EXPECT_EQ(CPoint(55, 85), layout.marker_layouts[0].to);
    EXPECT_TRUE(layout.marker_layouts[0].highlighted);
    EXPECT_EQ(CPoint(55, 90), layout.marker_layouts[1].from);
    EXPECT_EQ(CPoint(55, 351), layout.marker_layouts[1].to);
    EXPECT_FALSE(layout.marker_layouts[1].highlighted);
    EXPECT_EQ(CPoint(55, 360), layout.marker_layouts[2].from);
    EXPECT_EQ(CPoint(55, 388), layout.marker_layouts[2].to);
    EXPECT_FALSE(layout.marker_layouts[2].highlighted);
  }

  // The third line
  {
    const CompositionWindowLayout &layout = layouts.at(2);
    EXPECT_COMPOSITION_WINDOW_LAYOUT(1866, 712, 1922, 864, 0, 0, 56, 151, 56, 0,
                                     0, 151, 56, 152, logfont, layout);
    {
      const char kMsg[] = "estです";
      std::wstring msg;
      mozc::Util::UTF8ToWide(kMsg, &msg);
      EXPECT_EQ(msg, layout.text);
    }
    ASSERT_EQ(1, layout.marker_layouts.size());

    EXPECT_EQ(CPoint(55, 0), layout.marker_layouts[0].from);
    EXPECT_EQ(CPoint(55, 151), layout.marker_layouts[0].to);
    EXPECT_FALSE(layout.marker_layouts[0].highlighted);
  }

  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1978, 1017, 1978, 1017, 2034, 1079,
                                         candidate_layout);

  // Check other candidate positions.
  command.mutable_output()->mutable_candidates()->set_position(0);
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1978, 837, 1978, 837, 2034, 1079,
                                         candidate_layout);

  command.mutable_output()->mutable_candidates()->set_position(3);
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1978, 972, 1978, 972, 2034, 1079,
                                         candidate_layout);

  command.mutable_output()->mutable_candidates()->set_position(10);
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1922, 802, 1922, 802, 1978, 1100,
                                         candidate_layout);

  command.mutable_output()->mutable_candidates()->set_position(16);
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1922, 1072, 1922, 1072, 1978, 1100,
                                         candidate_layout);

  // w/o candidates, proportional, vertical
  SetRenderereCommandForTest(false, false, true, 0, hwnd, &command);
  EXPECT_TRUE(mozc::win32::FontUtil::ToLOGFONT(
      command.application_info().composition_font(), &logfont));
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_TRUE(result);
  EXPECT_FALSE(candidate_layout.initialized());
}

TEST_F(Win32RendererUtilTest,
       CompositionHorizontalFirstLineIsEmptyWithMonospacedFont) {
  const int kCursorOffsetX = 120;

  RendererCommand command;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(CreateDefaultGUIFontEmulator(),
                           CreateWindowEmulatorWithDPIScaling(1.0, &hwnd));
  std::vector<CompositionWindowLayout> layouts;
  CandidateWindowLayout candidate_layout;
  CLogFont logfont;

  bool result = false;

  // w/ candidates, monospaced, horizontal
  SetRenderereCommandForTest(false, true, false, kCursorOffsetX, hwnd,
                             &command);
  EXPECT_TRUE(mozc::win32::FontUtil::ToLOGFONT(
      command.application_info().composition_font(), &logfont));
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_TRUE(result);

  ASSERT_EQ(1, layouts.size());

  // The first line
  {
    const CompositionWindowLayout &layout = layouts.at(0);
    EXPECT_COMPOSITION_WINDOW_LAYOUT(1193, 648, 1975, 697, 0, 0, 781, 49, 0, 0,
                                     781, 0, 782, 49, logfont, layout);
    {
      const char kMsg[] = "これは、Google日本語入力のTestです";
      std::wstring msg;
      mozc::Util::UTF8ToWide(kMsg, &msg);
      EXPECT_EQ(msg, layout.text);
    }
    ASSERT_EQ(5, layout.marker_layouts.size());

    EXPECT_EQ(CPoint(0, 48), layout.marker_layouts[0].from);
    EXPECT_EQ(CPoint(126, 48), layout.marker_layouts[0].to);
    EXPECT_FALSE(layout.marker_layouts[0].highlighted);
    EXPECT_EQ(CPoint(135, 48), layout.marker_layouts[1].from);
    EXPECT_EQ(CPoint(171, 48), layout.marker_layouts[1].to);
    EXPECT_FALSE(layout.marker_layouts[1].highlighted);
    EXPECT_EQ(CPoint(180, 48), layout.marker_layouts[2].from);
    EXPECT_EQ(CPoint(325, 48), layout.marker_layouts[2].to);
    EXPECT_TRUE(layout.marker_layouts[2].highlighted);
    EXPECT_EQ(CPoint(331, 48), layout.marker_layouts[3].from);
    EXPECT_EQ(CPoint(592, 48), layout.marker_layouts[3].to);
    EXPECT_FALSE(layout.marker_layouts[3].highlighted);
    EXPECT_EQ(CPoint(601, 48), layout.marker_layouts[4].from);
    EXPECT_EQ(CPoint(781, 48), layout.marker_layouts[4].to);
    EXPECT_FALSE(layout.marker_layouts[4].highlighted);
  }
}

TEST_F(Win32RendererUtilTest,
       CompositionHorizontalFirstLineIsEmptyWithProportionalFont) {
  const int kCursorOffsetX = 120;

  RendererCommand command;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(CreateDefaultGUIFontEmulator(),
                           CreateWindowEmulatorWithDPIScaling(1.0, &hwnd));
  std::vector<CompositionWindowLayout> layouts;
  CandidateWindowLayout candidate_layout;
  CLogFont logfont;

  bool result = false;

  // w/ candidates, proportional, horizontal
  SetRenderereCommandForTest(true, true, false, kCursorOffsetX, hwnd, &command);
  EXPECT_TRUE(mozc::win32::FontUtil::ToLOGFONT(
      command.application_info().composition_font(), &logfont));
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_TRUE(result);

  ASSERT_EQ(1, layouts.size());

  // The first line
  {
    const CompositionWindowLayout &layout = layouts.at(0);
    EXPECT_COMPOSITION_WINDOW_LAYOUT(1193, 653, 1975, 707, 0, 0, 781, 54, 0, 0,
                                     781, 0, 782, 54, logfont, layout);
    {
      const char kMsg[] = "これは、Google日本語入力のTestです";
      std::wstring msg;
      mozc::Util::UTF8ToWide(kMsg, &msg);
      EXPECT_EQ(msg, layout.text);
    }
    ASSERT_EQ(5, layout.marker_layouts.size());

    EXPECT_EQ(CPoint(0, 53), layout.marker_layouts[0].from);
    EXPECT_EQ(CPoint(126, 53), layout.marker_layouts[0].to);
    EXPECT_FALSE(layout.marker_layouts[0].highlighted);
    EXPECT_EQ(CPoint(135, 53), layout.marker_layouts[1].from);
    EXPECT_EQ(CPoint(171, 53), layout.marker_layouts[1].to);
    EXPECT_FALSE(layout.marker_layouts[1].highlighted);
    EXPECT_EQ(CPoint(180, 53), layout.marker_layouts[2].from);
    EXPECT_EQ(CPoint(327, 53), layout.marker_layouts[2].to);
    EXPECT_TRUE(layout.marker_layouts[2].highlighted);
    EXPECT_EQ(CPoint(332, 53), layout.marker_layouts[3].from);
    EXPECT_EQ(CPoint(593, 53), layout.marker_layouts[3].to);
    EXPECT_FALSE(layout.marker_layouts[3].highlighted);
    EXPECT_EQ(CPoint(602, 53), layout.marker_layouts[4].from);
    EXPECT_EQ(CPoint(781, 53), layout.marker_layouts[4].to);
    EXPECT_FALSE(layout.marker_layouts[4].highlighted);
  }
}

TEST_F(Win32RendererUtilTest,
       CompositionVerticalFirstLineIsEmptyWithMonospacedFont) {
  const int kCursorOffsetY = 170;

  RendererCommand command;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(CreateDefaultGUIFontEmulator(),
                           CreateWindowEmulatorWithDPIScaling(1.0, &hwnd));
  std::vector<CompositionWindowLayout> layouts;
  CandidateWindowLayout candidate_layout;
  CLogFont logfont;

  bool result = false;

  // w/ candidates, monospaced, vertical
  SetRenderereCommandForTest(false, true, true, kCursorOffsetY, hwnd, &command);
  EXPECT_TRUE(mozc::win32::FontUtil::ToLOGFONT(
      command.application_info().composition_font(), &logfont));
  logfont.lfOrientation = 2700;

  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_TRUE(result);

  ASSERT_EQ(3, layouts.size());

  // The first line
  {
    const CompositionWindowLayout &layout = layouts.at(0);
    EXPECT_COMPOSITION_WINDOW_LAYOUT(1932, 712, 1983, 1088, 0, 0, 51, 376, 51,
                                     0, 0, 0, 0, 0, logfont, layout);
    {
      const char kMsg[] = "これは、Google日";
      std::wstring msg;
      mozc::Util::UTF8ToWide(kMsg, &msg);
      EXPECT_EQ(msg, layout.text);
    }
    ASSERT_EQ(4, layout.marker_layouts.size());

    EXPECT_EQ(CPoint(50, 0), layout.marker_layouts[0].from);
    EXPECT_EQ(CPoint(50, 126), layout.marker_layouts[0].to);
    EXPECT_FALSE(layout.marker_layouts[0].highlighted);
    EXPECT_EQ(CPoint(50, 135), layout.marker_layouts[1].from);
    EXPECT_EQ(CPoint(50, 171), layout.marker_layouts[1].to);
    EXPECT_FALSE(layout.marker_layouts[1].highlighted);
    EXPECT_EQ(CPoint(50, 180), layout.marker_layouts[2].from);
    EXPECT_EQ(CPoint(50, 325), layout.marker_layouts[2].to);
    EXPECT_TRUE(layout.marker_layouts[2].highlighted);
    EXPECT_EQ(CPoint(50, 331), layout.marker_layouts[3].from);
    EXPECT_EQ(CPoint(50, 376), layout.marker_layouts[3].to);
    EXPECT_FALSE(layout.marker_layouts[3].highlighted);
  }

  // The second line
  {
    const CompositionWindowLayout &layout = layouts.at(1);
    EXPECT_COMPOSITION_WINDOW_LAYOUT(1881, 712, 1932, 1072, 0, 0, 51, 360, 51,
                                     0, 0, 0, 0, 0, logfont, layout);
    {
      const char kMsg[] = "本語入力のTestで";
      std::wstring msg;
      mozc::Util::UTF8ToWide(kMsg, &msg);
      EXPECT_EQ(msg, layout.text);
    }
    ASSERT_EQ(2, layout.marker_layouts.size());

    EXPECT_EQ(CPoint(50, 0), layout.marker_layouts[0].from);
    EXPECT_EQ(CPoint(50, 216), layout.marker_layouts[0].to);
    EXPECT_FALSE(layout.marker_layouts[0].highlighted);
    EXPECT_EQ(CPoint(50, 225), layout.marker_layouts[1].from);
    EXPECT_EQ(CPoint(50, 360), layout.marker_layouts[1].to);
    EXPECT_FALSE(layout.marker_layouts[1].highlighted);
  }

  // The third line
  {
    const CompositionWindowLayout &layout = layouts.at(2);
    EXPECT_COMPOSITION_WINDOW_LAYOUT(1830, 712, 1881, 758, 0, 0, 51, 45, 51, 0,
                                     0, 45, 51, 46, logfont, layout);
    {
      const char kMsg[] = "す";
      std::wstring msg;
      mozc::Util::UTF8ToWide(kMsg, &msg);
      EXPECT_EQ(msg, layout.text);
    }
    ASSERT_EQ(1, layout.marker_layouts.size());

    EXPECT_EQ(CPoint(50, 0), layout.marker_layouts[0].from);
    EXPECT_EQ(CPoint(50, 45), layout.marker_layouts[0].to);
    EXPECT_FALSE(layout.marker_layouts[0].highlighted);
  }
}

TEST_F(Win32RendererUtilTest,
       CompositionVerticalFirstLineIsEmptyWithProportionalFont) {
  const int kCursorOffsetY = 170;

  RendererCommand command;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(CreateDefaultGUIFontEmulator(),
                           CreateWindowEmulatorWithDPIScaling(1.0, &hwnd));
  std::vector<CompositionWindowLayout> layouts;
  CandidateWindowLayout candidate_layout;
  CLogFont logfont;

  bool result = false;

  // w/ candidates, proportional, vertical
  SetRenderereCommandForTest(true, true, true, kCursorOffsetY, hwnd, &command);
  EXPECT_TRUE(mozc::win32::FontUtil::ToLOGFONT(
      command.application_info().composition_font(), &logfont));
  logfont.lfOrientation = 2700;

  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_TRUE(result);

  ASSERT_EQ(3, layouts.size());

  // The first line
  {
    const CompositionWindowLayout &layout = layouts.at(0);
    EXPECT_COMPOSITION_WINDOW_LAYOUT(1922, 712, 1978, 1089, 0, 0, 56, 377, 56,
                                     0, 0, 0, 0, 0, logfont, layout);
    {
      const char kMsg[] = "これは、Google日";
      std::wstring msg;
      mozc::Util::UTF8ToWide(kMsg, &msg);
      EXPECT_EQ(msg, layout.text);
    }
    ASSERT_EQ(4, layout.marker_layouts.size());

    EXPECT_EQ(CPoint(55, 0), layout.marker_layouts[0].from);
    EXPECT_EQ(CPoint(55, 126), layout.marker_layouts[0].to);
    EXPECT_FALSE(layout.marker_layouts[0].highlighted);
    EXPECT_EQ(CPoint(55, 135), layout.marker_layouts[1].from);
    EXPECT_EQ(CPoint(55, 171), layout.marker_layouts[1].to);
    EXPECT_FALSE(layout.marker_layouts[1].highlighted);
    EXPECT_EQ(CPoint(55, 180), layout.marker_layouts[2].from);
    EXPECT_EQ(CPoint(55, 327), layout.marker_layouts[2].to);
    EXPECT_TRUE(layout.marker_layouts[2].highlighted);
    EXPECT_EQ(CPoint(55, 332), layout.marker_layouts[3].from);
    EXPECT_EQ(CPoint(55, 377), layout.marker_layouts[3].to);
    EXPECT_FALSE(layout.marker_layouts[3].highlighted);
  }

  // The second line
  {
    const CompositionWindowLayout &layout = layouts.at(1);
    EXPECT_COMPOSITION_WINDOW_LAYOUT(1866, 712, 1922, 1071, 0, 0, 56, 359, 56,
                                     0, 0, 0, 0, 0, logfont, layout);
    {
      const char kMsg[] = "本語入力のTestで";
      std::wstring msg;
      mozc::Util::UTF8ToWide(kMsg, &msg);
      EXPECT_EQ(msg, layout.text);
    }
    ASSERT_EQ(2, layout.marker_layouts.size());

    EXPECT_EQ(CPoint(55, 0), layout.marker_layouts[0].from);
    EXPECT_EQ(CPoint(55, 216), layout.marker_layouts[0].to);
    EXPECT_FALSE(layout.marker_layouts[0].highlighted);
    EXPECT_EQ(CPoint(55, 225), layout.marker_layouts[1].from);
    EXPECT_EQ(CPoint(55, 359), layout.marker_layouts[1].to);
    EXPECT_FALSE(layout.marker_layouts[1].highlighted);
  }

  // The third line
  {
    const CompositionWindowLayout &layout = layouts.at(2);
    EXPECT_COMPOSITION_WINDOW_LAYOUT(1810, 712, 1866, 758, 0, 0, 56, 45, 56, 0,
                                     0, 45, 56, 46, logfont, layout);
    {
      const char kMsg[] = "す";
      std::wstring msg;
      mozc::Util::UTF8ToWide(kMsg, &msg);
      EXPECT_EQ(msg, layout.text);
    }
    ASSERT_EQ(1, layout.marker_layouts.size());

    EXPECT_EQ(CPoint(55, 0), layout.marker_layouts[0].from);
    EXPECT_EQ(CPoint(55, 45), layout.marker_layouts[0].to);
    EXPECT_FALSE(layout.marker_layouts[0].highlighted);
  }
}

TEST_F(Win32RendererUtilTest, CheckCaretPosInHorizontalComposition) {
  // Check the caret points the first character.
  {
    const int kCursorOffsetX = -300;
    HWND hwnd = nullptr;
    LayoutManager layout_mgr(CreateDefaultGUIFontEmulator(),
                             CreateWindowEmulatorWithDPIScaling(1.0, &hwnd));
    std::vector<CompositionWindowLayout> layouts;

    RendererCommand command;
    CandidateWindowLayout candidate_layout;
    CLogFont logfont;
    bool result = false;
    SetRenderereCommandForCaretTest(false, false, 10, 0, kCursorOffsetX, hwnd,
                                    &command);
    EXPECT_TRUE(mozc::win32::FontUtil::ToLOGFONT(
        command.application_info().composition_font(), &logfont));
    layouts.clear();
    candidate_layout.Clear();
    result = layout_mgr.LayoutCompositionWindow(command, &layouts,
                                                &candidate_layout);
    EXPECT_TRUE(result);

    ASSERT_EQ(1, layouts.size());

    {
      const CompositionWindowLayout &layout = layouts.at(0);
      EXPECT_COMPOSITION_WINDOW_LAYOUT(1568, 599, 2018, 648, 0, 0, 450, 49, 0,
                                       0, 0, 0, 1, 49, logfont, layout);
    }
  }

  // Check the caret points the middle character.
  {
    const int kCursorOffsetX = -300;
    HWND hwnd = nullptr;
    LayoutManager layout_mgr(CreateDefaultGUIFontEmulator(),
                             CreateWindowEmulatorWithDPIScaling(1.0, &hwnd));
    std::vector<CompositionWindowLayout> layouts;

    RendererCommand command;
    CandidateWindowLayout candidate_layout;
    CLogFont logfont;
    bool result = false;
    SetRenderereCommandForCaretTest(false, false, 10, 5, kCursorOffsetX, hwnd,
                                    &command);
    EXPECT_TRUE(mozc::win32::FontUtil::ToLOGFONT(
        command.application_info().composition_font(), &logfont));
    layouts.clear();
    candidate_layout.Clear();
    result = layout_mgr.LayoutCompositionWindow(command, &layouts,
                                                &candidate_layout);
    EXPECT_TRUE(result);

    ASSERT_EQ(1, layouts.size());

    {
      const CompositionWindowLayout &layout = layouts.at(0);
      EXPECT_COMPOSITION_WINDOW_LAYOUT(1568, 599, 2018, 648, 0, 0, 450, 49, 0,
                                       0, 225, 0, 226, 49, logfont, layout);
    }
  }

  // Check the caret points the next to the last character.
  // In this case, composition window should have an extra space to draw the
  // caret except that there is no room to extend.
  {
    const int kCursorOffsetX = -300;
    HWND hwnd = nullptr;
    LayoutManager layout_mgr(CreateDefaultGUIFontEmulator(),
                             CreateWindowEmulatorWithDPIScaling(1.0, &hwnd));
    std::vector<CompositionWindowLayout> layouts;

    RendererCommand command;
    CandidateWindowLayout candidate_layout;
    CLogFont logfont;
    bool result = false;
    SetRenderereCommandForCaretTest(false, false, 10, 10, kCursorOffsetX, hwnd,
                                    &command);
    EXPECT_TRUE(mozc::win32::FontUtil::ToLOGFONT(
        command.application_info().composition_font(), &logfont));
    layouts.clear();
    candidate_layout.Clear();
    result = layout_mgr.LayoutCompositionWindow(command, &layouts,
                                                &candidate_layout);
    EXPECT_TRUE(result);

    ASSERT_EQ(1, layouts.size());

    {
      const CompositionWindowLayout &layout = layouts.at(0);
      EXPECT_COMPOSITION_WINDOW_LAYOUT(1568, 599, 2019, 648, 0, 0, 450, 49, 0,
                                       0, 450, 0, 451, 49, logfont, layout);
    }
  }

  // To emulate built-in edit control, we will adjust caret position to be
  // inside of the line if it exceeds the end of line.
  {
    const int kCursorOffsetX = -287;
    HWND hwnd = nullptr;
    LayoutManager layout_mgr(CreateDefaultGUIFontEmulator(),
                             CreateWindowEmulatorWithDPIScaling(1.0, &hwnd));
    std::vector<CompositionWindowLayout> layouts;

    RendererCommand command;
    CandidateWindowLayout candidate_layout;
    CLogFont logfont;
    bool result = false;
    SetRenderereCommandForCaretTest(false, false, 10, 10, kCursorOffsetX, hwnd,
                                    &command);
    EXPECT_TRUE(mozc::win32::FontUtil::ToLOGFONT(
        command.application_info().composition_font(), &logfont));
    layouts.clear();
    candidate_layout.Clear();
    result = layout_mgr.LayoutCompositionWindow(command, &layouts,
                                                &candidate_layout);
    EXPECT_TRUE(result);

    ASSERT_EQ(1, layouts.size());

    {
      const CompositionWindowLayout &layout = layouts.at(0);
      EXPECT_COMPOSITION_WINDOW_LAYOUT(1581, 599, 2031, 648, 0, 0, 450, 49, 0,
                                       0, 449, 0, 450, 49, logfont, layout);
    }
  }

  // If there exists other characters in the next line, caret position should
  // not be adjusted.
  {
    const int kCursorOffsetX = -287;
    HWND hwnd = nullptr;
    LayoutManager layout_mgr(CreateDefaultGUIFontEmulator(),
                             CreateWindowEmulatorWithDPIScaling(1.0, &hwnd));
    std::vector<CompositionWindowLayout> layouts;

    RendererCommand command;
    CandidateWindowLayout candidate_layout;
    CLogFont logfont;
    bool result = false;
    SetRenderereCommandForCaretTest(false, false, 11, 10, kCursorOffsetX, hwnd,
                                    &command);
    EXPECT_TRUE(mozc::win32::FontUtil::ToLOGFONT(
        command.application_info().composition_font(), &logfont));
    layouts.clear();
    candidate_layout.Clear();
    result = layout_mgr.LayoutCompositionWindow(command, &layouts,
                                                &candidate_layout);
    EXPECT_TRUE(result);

    ASSERT_EQ(2, layouts.size());

    {
      const CompositionWindowLayout &layout = layouts.at(0);
      EXPECT_COMPOSITION_WINDOW_LAYOUT(1581, 599, 2031, 648, 0, 0, 450, 49, 0,
                                       0, 0, 0, 0, 0, logfont, layout);
    }

    {
      const CompositionWindowLayout &layout = layouts.at(1);
      EXPECT_COMPOSITION_WINDOW_LAYOUT(1193, 648, 1238, 697, 0, 0, 45, 49, 0, 0,
                                       0, 0, 1, 49, logfont, layout);
    }
  }
}

TEST_F(Win32RendererUtilTest, CheckCaretPosInVerticalComposition) {
  // Check the caret points the first character.
  {
    const int kCursorOffsetY = -10;
    HWND hwnd = nullptr;
    LayoutManager layout_mgr(CreateDefaultGUIFontEmulator(),
                             CreateWindowEmulatorWithDPIScaling(1.0, &hwnd));
    std::vector<CompositionWindowLayout> layouts;

    RendererCommand command;
    CandidateWindowLayout candidate_layout;
    CLogFont logfont;
    bool result = false;
    SetRenderereCommandForCaretTest(false, true, 4, 0, kCursorOffsetY, hwnd,
                                    &command);
    EXPECT_TRUE(mozc::win32::FontUtil::ToLOGFONT(
        command.application_info().composition_font(), &logfont));
    logfont.lfOrientation = 2700;

    layouts.clear();
    candidate_layout.Clear();
    result = layout_mgr.LayoutCompositionWindow(command, &layouts,
                                                &candidate_layout);
    EXPECT_TRUE(result);

    ASSERT_EQ(1, layouts.size());

    {
      const CompositionWindowLayout &layout = layouts.at(0);
      EXPECT_COMPOSITION_WINDOW_LAYOUT(1983, 917, 2034, 1097, 0, 0, 51, 180, 51,
                                       0, 0, 0, 51, 1, logfont, layout);
    }
  }

  // Check the caret points the middle character.
  {
    const int kCursorOffsetY = -10;
    HWND hwnd = nullptr;
    LayoutManager layout_mgr(CreateDefaultGUIFontEmulator(),
                             CreateWindowEmulatorWithDPIScaling(1.0, &hwnd));
    std::vector<CompositionWindowLayout> layouts;

    RendererCommand command;
    CandidateWindowLayout candidate_layout;
    CLogFont logfont;
    bool result = false;
    SetRenderereCommandForCaretTest(false, true, 4, 2, kCursorOffsetY, hwnd,
                                    &command);
    EXPECT_TRUE(mozc::win32::FontUtil::ToLOGFONT(
        command.application_info().composition_font(), &logfont));
    logfont.lfOrientation = 2700;

    layouts.clear();
    candidate_layout.Clear();
    result = layout_mgr.LayoutCompositionWindow(command, &layouts,
                                                &candidate_layout);
    EXPECT_TRUE(result);

    ASSERT_EQ(1, layouts.size());

    {
      const CompositionWindowLayout &layout = layouts.at(0);
      EXPECT_COMPOSITION_WINDOW_LAYOUT(1983, 917, 2034, 1097, 0, 0, 51, 180, 51,
                                       0, 0, 90, 51, 91, logfont, layout);
    }
  }

  // Check the caret points the next to the last character.
  // In this case, composition window should have an extra space to draw the
  // caret except that there is no room to extend.
  {
    const int kCursorOffsetY = -10;
    HWND hwnd = nullptr;
    LayoutManager layout_mgr(CreateDefaultGUIFontEmulator(),
                             CreateWindowEmulatorWithDPIScaling(1.0, &hwnd));
    std::vector<CompositionWindowLayout> layouts;

    RendererCommand command;
    CandidateWindowLayout candidate_layout;
    CLogFont logfont;
    bool result = false;
    SetRenderereCommandForCaretTest(false, true, 4, 4, kCursorOffsetY, hwnd,
                                    &command);
    EXPECT_TRUE(mozc::win32::FontUtil::ToLOGFONT(
        command.application_info().composition_font(), &logfont));
    logfont.lfOrientation = 2700;

    layouts.clear();
    candidate_layout.Clear();
    result = layout_mgr.LayoutCompositionWindow(command, &layouts,
                                                &candidate_layout);
    EXPECT_TRUE(result);

    ASSERT_EQ(1, layouts.size());

    {
      const CompositionWindowLayout &layout = layouts.at(0);
      EXPECT_COMPOSITION_WINDOW_LAYOUT(1983, 917, 2034, 1098, 0, 0, 51, 180, 51,
                                       0, 0, 180, 51, 181, logfont, layout);
    }
  }

  // To emulate built-in edit control, we will adjust caret position to be
  // inside of the line if it exceeds the end of line.
  {
    const int kCursorOffsetY = -2;
    HWND hwnd = nullptr;
    LayoutManager layout_mgr(CreateDefaultGUIFontEmulator(),
                             CreateWindowEmulatorWithDPIScaling(1.0, &hwnd));
    std::vector<CompositionWindowLayout> layouts;

    RendererCommand command;
    CandidateWindowLayout candidate_layout;
    CLogFont logfont;
    bool result = false;
    SetRenderereCommandForCaretTest(false, true, 4, 4, kCursorOffsetY, hwnd,
                                    &command);
    EXPECT_TRUE(mozc::win32::FontUtil::ToLOGFONT(
        command.application_info().composition_font(), &logfont));
    logfont.lfOrientation = 2700;

    layouts.clear();
    candidate_layout.Clear();
    result = layout_mgr.LayoutCompositionWindow(command, &layouts,
                                                &candidate_layout);
    EXPECT_TRUE(result);

    ASSERT_EQ(1, layouts.size());

    {
      const CompositionWindowLayout &layout = layouts.at(0);
      EXPECT_COMPOSITION_WINDOW_LAYOUT(1983, 925, 2034, 1105, 0, 0, 51, 180, 51,
                                       0, 0, 179, 51, 180, logfont, layout);
    }
  }

  // If there exists other characters in the next line, caret position should
  // not be adjusted.
  {
    const int kCursorOffsetY = -2;
    HWND hwnd = nullptr;
    LayoutManager layout_mgr(CreateDefaultGUIFontEmulator(),
                             CreateWindowEmulatorWithDPIScaling(1.0, &hwnd));
    std::vector<CompositionWindowLayout> layouts;

    RendererCommand command;
    CandidateWindowLayout candidate_layout;
    CLogFont logfont;
    bool result = false;
    SetRenderereCommandForCaretTest(false, true, 5, 4, kCursorOffsetY, hwnd,
                                    &command);
    EXPECT_TRUE(mozc::win32::FontUtil::ToLOGFONT(
        command.application_info().composition_font(), &logfont));
    logfont.lfOrientation = 2700;

    layouts.clear();
    candidate_layout.Clear();
    result = layout_mgr.LayoutCompositionWindow(command, &layouts,
                                                &candidate_layout);
    EXPECT_TRUE(result);

    ASSERT_EQ(2, layouts.size());

    {
      const CompositionWindowLayout &layout = layouts.at(0);
      EXPECT_COMPOSITION_WINDOW_LAYOUT(1983, 925, 2034, 1105, 0, 0, 51, 180, 51,
                                       0, 0, 0, 0, 0, logfont, layout);
    }

    {
      const CompositionWindowLayout &layout = layouts.at(1);
      EXPECT_COMPOSITION_WINDOW_LAYOUT(1932, 712, 1983, 757, 0, 0, 51, 45, 51,
                                       0, 0, 0, 51, 1, logfont, layout);
    }
  }
}

// Check if suggest window does not hide preedit.
// See b/4317753 for details.
TEST_F(Win32RendererUtilTest, SuggestWindowNeverHidesHorizontalPreedit) {
  const int kCursorOffsetX = 0;

  RendererCommand command;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(CreateDefaultGUIFontEmulator(),
                           CreateWindowEmulatorWithDPIScaling(1.0, &hwnd));
  std::vector<CompositionWindowLayout> layouts;
  CandidateWindowLayout candidate_layout;
  CLogFont logfont;

  bool result = false;

  // w/ candidates, proportional, horizontal
  SetRenderereCommandForSuggestTest(true, false, kCursorOffsetX, hwnd,
                                    &command);
  EXPECT_TRUE(mozc::win32::FontUtil::ToLOGFONT(
      command.application_info().composition_font(), &logfont));
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_TRUE(result);

  // Suggest window should be aligned to the last composition window.
  EXPECT_EQ(layouts.rbegin()->window_position_in_screen_coordinate.left,
            candidate_layout.position().x);
  EXPECT_EQ(layouts.rbegin()->window_position_in_screen_coordinate.bottom,
            candidate_layout.position().y);
  EXPECT_EQ(CRect(1193, 599, 2003, 707), candidate_layout.exclude_region());
}

// Check if suggest window does not hide preedit.
// See b/4317753 for details.
TEST_F(Win32RendererUtilTest, SuggestWindowNeverHidesVerticalPreedit) {
  const int kCursorOffsetY = 0;

  RendererCommand command;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(CreateDefaultGUIFontEmulator(),
                           CreateWindowEmulatorWithDPIScaling(1.0, &hwnd));
  std::vector<CompositionWindowLayout> layouts;
  CandidateWindowLayout candidate_layout;
  CLogFont logfont;

  bool result = false;

  // w/ candidates, proportional, horizontal
  SetRenderereCommandForSuggestTest(true, true, kCursorOffsetY, hwnd, &command);
  EXPECT_TRUE(mozc::win32::FontUtil::ToLOGFONT(
      command.application_info().composition_font(), &logfont));
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_TRUE(result);

  // Suggest window should be aligned to the first composition window.
  // TODO(yukawa): Use the last composition window when vertical candidate
  //   window is implemented.
  EXPECT_EQ(layouts.begin()->window_position_in_screen_coordinate.left,
            candidate_layout.position().x);
  EXPECT_EQ(layouts.begin()->window_position_in_screen_coordinate.top,
            candidate_layout.position().y);
  EXPECT_EQ(CRect(1978, 927, 2034, 1062), candidate_layout.exclude_region());
}

TEST_F(Win32RendererUtilTest, RemoveUnderlineFromFont_Issue2935480) {
  const int kCursorOffsetY = 0;
  HWND hwnd = nullptr;
  LayoutManager layout_mgr(CreateDefaultGUIFontEmulator(),
                           CreateWindowEmulatorWithDPIScaling(1.0, &hwnd));
  std::vector<CompositionWindowLayout> layouts;

  RendererCommand command;
  CandidateWindowLayout candidate_layout;
  CLogFont logfont;
  bool result = false;
  SetRenderereCommandForCaretTest(false, true, 4, 0, kCursorOffsetY, hwnd,
                                  &command);
  EXPECT_TRUE(mozc::win32::FontUtil::ToLOGFONT(
      command.application_info().composition_font(), &logfont));
  logfont.lfOrientation = 0;
  // Assume underline is enabled in the application.
  logfont.lfUnderline = 1;

  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_TRUE(result);

  // Underline should be stripped.
  ASSERT_EQ(2, layouts.size());
  EXPECT_EQ(0, layouts[0].log_font.lfUnderline);
  EXPECT_EQ(0, layouts[1].log_font.lfUnderline);
}

// Some applications such as MIEFS use CompositionForm::RECT as a bit flag.
// We should consider the case where two or more style bits are specified
// at the same time.
TEST_F(Win32RendererUtilTest, CompositionFormRECTAsBitFlag_Issue3200425) {
  const uint32 kStyleBit = CompositionForm::RECT | CompositionForm::POINT;

  const int kCursorOffsetX = 0;

  RendererCommand command;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(CreateDefaultGUIFontEmulator(),
                           CreateWindowEmulatorWithDPIScaling(1.0, &hwnd));
  std::vector<CompositionWindowLayout> layouts;
  CandidateWindowLayout candidate_layout;

  CLogFont logfont;

  bool result = false;

  // w/ candidates, monospaced, horizontal
  SetRenderereCommandForTest(false, true, false, kCursorOffsetX, hwnd,
                             &command);
  command.mutable_application_info()
      ->mutable_composition_form()
      ->set_style_bits(kStyleBit);

  EXPECT_TRUE(mozc::win32::FontUtil::ToLOGFONT(
      command.application_info().composition_font(), &logfont));
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_TRUE(result);

  ASSERT_EQ(2, layouts.size());

  // The first line
  {
    const CompositionWindowLayout &layout = layouts.at(0);
    EXPECT_COMPOSITION_WINDOW_LAYOUT(1868, 599, 2003, 648, 0, 0, 135, 49, 0, 0,
                                     0, 0, 0, 0, logfont, layout);
    {
      const char kMsg[] = "これは";
      std::wstring msg;
      mozc::Util::UTF8ToWide(kMsg, &msg);
      EXPECT_EQ(msg, layout.text);
    }
    ASSERT_EQ(1, layout.marker_layouts.size());

    EXPECT_EQ(CPoint(0, 48), layout.marker_layouts[0].from);
    EXPECT_EQ(CPoint(126, 48), layout.marker_layouts[0].to);
    EXPECT_FALSE(layout.marker_layouts[0].highlighted);
  }

  // The second line
  {
    const CompositionWindowLayout &layout = layouts.at(1);
    EXPECT_COMPOSITION_WINDOW_LAYOUT(1193, 648, 1840, 697, 0, 0, 646, 49, 0, 0,
                                     646, 0, 647, 49, logfont, layout);
    {
      const char kMsg[] = "、Google日本語入力のTestです";
      std::wstring msg;
      mozc::Util::UTF8ToWide(kMsg, &msg);
      EXPECT_EQ(msg, layout.text);
    }
    ASSERT_EQ(4, layout.marker_layouts.size());

    EXPECT_EQ(CPoint(0, 48), layout.marker_layouts[0].from);
    EXPECT_EQ(CPoint(36, 48), layout.marker_layouts[0].to);
    EXPECT_FALSE(layout.marker_layouts[0].highlighted);
    EXPECT_EQ(CPoint(45, 48), layout.marker_layouts[1].from);
    EXPECT_EQ(CPoint(190, 48), layout.marker_layouts[1].to);
    EXPECT_TRUE(layout.marker_layouts[1].highlighted);
    EXPECT_EQ(CPoint(196, 48), layout.marker_layouts[2].from);
    EXPECT_EQ(CPoint(457, 48), layout.marker_layouts[2].to);
    EXPECT_FALSE(layout.marker_layouts[2].highlighted);
    EXPECT_EQ(CPoint(466, 48), layout.marker_layouts[3].from);
    EXPECT_EQ(CPoint(646, 48), layout.marker_layouts[3].to);
    EXPECT_FALSE(layout.marker_layouts[3].highlighted);
  }
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1238, 697, 1238, 648, 1839, 697,
                                         candidate_layout);

  // Check other candidate positions.
  command.mutable_output()->mutable_candidates()->set_position(0);
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1868, 648, 1868, 599, 2003, 648,
                                         candidate_layout);

  command.mutable_output()->mutable_candidates()->set_position(3);
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1193, 697, 1193, 648, 1839, 697,
                                         candidate_layout);

  command.mutable_output()->mutable_candidates()->set_position(10);
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1389, 697, 1389, 648, 1839, 697,
                                         candidate_layout);

  command.mutable_output()->mutable_candidates()->set_position(16);
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1659, 697, 1659, 648, 1839, 697,
                                         candidate_layout);

  // w/o candidates, monospaced, horizontal
  SetRenderereCommandForTest(false, false, false, 0, hwnd, &command);
  EXPECT_TRUE(mozc::win32::FontUtil::ToLOGFONT(
      command.application_info().composition_font(), &logfont));
  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_TRUE(result);
  EXPECT_FALSE(candidate_layout.initialized());
}

// Evernote Windows Client 4.0.0.2880 (107102) / Editor component
TEST_F(Win32RendererUtilTest, EvernoteEditorComposition) {
  const wchar_t kClassName[] = L"WebViewHost";
  const UINT kClassStyle = CS_DBLCLKS;
  const DWORD kWindowStyle =
      WS_CHILDWINDOW | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
  static_assert(kWindowStyle == 0x56000000, "Check actual value");
  const DWORD kWindowExStyle =
      WS_EX_LEFT | WS_EX_LTRREADING | WS_EX_RIGHTSCROLLBAR;
  static_assert(kWindowExStyle == 0, "Check actual value");

  const CRect kWindowRect(1548, 879, 1786, 1416);
  const CPoint kClientOffset(0, 0);
  const CSize kClientSize(238, 537);
  const double kScaleFactor = 1.0;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(
      CreateDefaultGUIFontEmulator(),
      CreateWindowEmulator(kClassName, kWindowRect, kClientOffset, kClientSize,
                           kScaleFactor, &hwnd));

  RendererCommand command;
  SetRenderereCommandForTest(false, true, false, 0, hwnd, &command);

  // Clear the default ApplicationInfo and update it for Evernote.
  command.clear_application_info();
  AppInfoUtil::SetBasicApplicationInfo(
      command.mutable_application_info(), hwnd,
      (ApplicationInfo::ShowCandidateWindow |
       ApplicationInfo::ShowSuggestWindow |
       ApplicationInfo::ShowCompositionWindow));

  AppInfoUtil::SetCaretInfo(command.mutable_application_info(), false, 0, 0, 0,
                            0, hwnd);

  CandidateWindowLayout candidate_layout;
  std::vector<CompositionWindowLayout> layouts;
  bool result = false;

  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_TRUE(result);

  // Default GUI font should be selected.
  CLogFont default_font = GetFont(true, false);
  default_font.lfHeight = 18;
  default_font.lfWidth = 0;

  ASSERT_EQ(2, layouts.size());

  // The first line
  {
    const CompositionWindowLayout &layout = layouts.at(0);
    EXPECT_COMPOSITION_WINDOW_LAYOUT(1548, 1416, 1777, 1434, 0, 0, 229, 18, 0,
                                     0, 0, 0, 0, 0, default_font, layout);
    {
      const char kMsg[] = "これは、Google日本語入力のTest";
      std::wstring msg;
      mozc::Util::UTF8ToWide(kMsg, &msg);
      EXPECT_EQ(msg, layout.text);
    }
    ASSERT_EQ(5, layout.marker_layouts.size());

    EXPECT_EQ(CPoint(0, 17), layout.marker_layouts[0].from);
    EXPECT_EQ(CPoint(42, 17), layout.marker_layouts[0].to);
    EXPECT_FALSE(layout.marker_layouts[0].highlighted);
    EXPECT_EQ(CPoint(45, 17), layout.marker_layouts[1].from);
    EXPECT_EQ(CPoint(57, 17), layout.marker_layouts[1].to);
    EXPECT_FALSE(layout.marker_layouts[1].highlighted);
    EXPECT_EQ(CPoint(60, 17), layout.marker_layouts[2].from);
    EXPECT_EQ(CPoint(108, 17), layout.marker_layouts[2].to);
    EXPECT_TRUE(layout.marker_layouts[2].highlighted);
    EXPECT_EQ(CPoint(110, 17), layout.marker_layouts[3].from);
    EXPECT_EQ(CPoint(197, 17), layout.marker_layouts[3].to);
    EXPECT_FALSE(layout.marker_layouts[3].highlighted);
    EXPECT_EQ(CPoint(200, 17), layout.marker_layouts[4].from);
    EXPECT_EQ(CPoint(229, 17), layout.marker_layouts[4].to);
    EXPECT_FALSE(layout.marker_layouts[4].highlighted);
  }

  // The second line
  {
    const CompositionWindowLayout &layout = layouts.at(1);
    EXPECT_COMPOSITION_WINDOW_LAYOUT(1548, 1434, 1579, 1452, 0, 0, 30, 18, 0, 0,
                                     30, 0, 31, 18, default_font, layout);
    {
      const char kMsg[] = "です";
      std::wstring msg;
      mozc::Util::UTF8ToWide(kMsg, &msg);
      EXPECT_EQ(msg, layout.text);
    }
    ASSERT_EQ(1, layout.marker_layouts.size());

    EXPECT_EQ(CPoint(0, 17), layout.marker_layouts[0].from);
    EXPECT_EQ(CPoint(30, 17), layout.marker_layouts[0].to);
    EXPECT_FALSE(layout.marker_layouts[0].highlighted);
  }

  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1608, 1434, 1608, 1416, 1777, 1434,
                                         candidate_layout);
}

// Crescent Eve 0.82a / Apr 24 2010.
// Crescent Eve sets larger composition form area than its client area.
// DPI virtualization API may fail in this case.  See b/3239031.
TEST_F(Win32RendererUtilTest, CrescentEveComposition_Issue3239031) {
  const wchar_t kClassName[] = L"CrescentEditer";
  const UINT kClassStyle = CS_DBLCLKS | CS_BYTEALIGNCLIENT;
  static_assert(kClassStyle == 0x00001008, "Check actual value");
  const DWORD kWindowStyle = WS_CHILDWINDOW | WS_VISIBLE | WS_VSCROLL;
  static_assert(kWindowStyle == 0x50200000, "Check actual value");
  const DWORD kWindowExStyle = WS_EX_LEFT | WS_EX_LTRREADING |
                               WS_EX_RIGHTSCROLLBAR | WS_EX_ACCEPTFILES |
                               WS_EX_CLIENTEDGE;
  static_assert(kWindowExStyle == 0x00000210, "Check actual value");

  const CRect kWindowRect(184, 192, 1312, 1426);
  const CPoint kClientOffset(2, 2);
  const CSize kClientSize(1107, 1230);
  const double kScaleFactor = 1.0;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(
      CreateDefaultGUIFontEmulator(),
      CreateWindowEmulator(kClassName, kWindowRect, kClientOffset, kClientSize,
                           kScaleFactor, &hwnd));

  RendererCommand command;
  SetRenderereCommandForTest(false, true, false, 0, hwnd, &command);

  // Replace the default values with those of Crescent Eve.
  command.clear_application_info();
  AppInfoUtil::SetBasicApplicationInfo(
      command.mutable_application_info(), hwnd,
      (ApplicationInfo::ShowCandidateWindow |
       ApplicationInfo::ShowSuggestWindow |
       ApplicationInfo::ShowCompositionWindow));

  AppInfoUtil::SetCompositionForm(
      command.mutable_application_info(),
      CompositionForm::POINT | CompositionForm::RECT, 35, 0, 35, 0, 1106, 1624);

  AppInfoUtil::SetCaretInfo(command.mutable_application_info(), false, 34, 0,
                            36, 14, hwnd);

  CandidateWindowLayout candidate_layout;
  std::vector<CompositionWindowLayout> layouts;
  bool result = false;

  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_TRUE(result);

  // Default GUI font should be selected.
  CLogFont default_font = GetFont(true, false);
  default_font.lfHeight = 18;
  default_font.lfWidth = 0;

  ASSERT_EQ(1, layouts.size());

  // The first line
  {
    const CompositionWindowLayout &layout = layouts.at(0);
    EXPECT_COMPOSITION_WINDOW_LAYOUT(221, 194, 481, 212, 0, 0, 259, 18, 0, 0,
                                     259, 0, 260, 18, default_font, layout);
    {
      const char kMsg[] = "これは、Google日本語入力のTestです";
      std::wstring msg;
      mozc::Util::UTF8ToWide(kMsg, &msg);
      EXPECT_EQ(msg, layout.text);
    }
    ASSERT_EQ(5, layout.marker_layouts.size());

    EXPECT_EQ(CPoint(0, 17), layout.marker_layouts[0].from);
    EXPECT_EQ(CPoint(42, 17), layout.marker_layouts[0].to);
    EXPECT_FALSE(layout.marker_layouts[0].highlighted);
    EXPECT_EQ(CPoint(45, 17), layout.marker_layouts[1].from);
    EXPECT_EQ(CPoint(57, 17), layout.marker_layouts[1].to);
    EXPECT_FALSE(layout.marker_layouts[1].highlighted);
    EXPECT_EQ(CPoint(60, 17), layout.marker_layouts[2].from);
    EXPECT_EQ(CPoint(108, 17), layout.marker_layouts[2].to);
    EXPECT_TRUE(layout.marker_layouts[2].highlighted);
    EXPECT_EQ(CPoint(110, 17), layout.marker_layouts[3].from);
    EXPECT_EQ(CPoint(197, 17), layout.marker_layouts[3].to);
    EXPECT_FALSE(layout.marker_layouts[3].highlighted);
    EXPECT_EQ(CPoint(200, 17), layout.marker_layouts[4].from);
    EXPECT_EQ(CPoint(259, 17), layout.marker_layouts[4].to);
    EXPECT_FALSE(layout.marker_layouts[4].highlighted);
  }

  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(281, 212, 281, 194, 480, 212,
                                         candidate_layout);
}

// MSInfo32.exe 6.1.7600 on Windows 7.  (b/3433099)
// The composition window and candidate window must be shown even when the
// client sets Composition/CandidateForm outside of the top-level window.
// Note that LogicalToPhysicalPoint API may return FALSE in this situation.
TEST_F(Win32RendererUtilTest, MSInfo32Composition_Issue3433099) {
  const double kScaleFactor = 1.0;

  WindowPositionEmulator *window_emulator = nullptr;
  HWND root_window = nullptr;
  HWND child_window = nullptr;

  {
    const wchar_t kRootClassName[] = L"#32770 (Dialog)";
    const UINT kRootClassStyle = CS_DBLCLKS | CS_SAVEBITS;
    const DWORD kRootWindowStyle = 0x96CF0044;
    const DWORD kRootWindowExStyle = 0x00010100;
    const CRect kRootWindowRect(838, 651, 1062, 1157);
    const CPoint kRootClientOffset(8, 71);
    const CSize kRootClientSize(208, 427);
    window_emulator =
        CreateWindowEmulator(kRootClassName, kRootWindowRect, kRootClientOffset,
                             kRootClientSize, kScaleFactor, &root_window);
  }
  {
    const wchar_t kChildClassName[] = L"Edit";
    const UINT kChildClassStyle = CS_DBLCLKS | CS_PARENTDC | CS_GLOBALCLASS;
    const DWORD kChildWindowStyle = 0x50010080;
    const DWORD kChildWindowExStyle = 0x00000204;
    const CRect kChildWindowRect(951, 1071, 1072, 1098);
    const CPoint kChildClientOffset(2, 2);
    const CSize kChildClientSize(117, 23);
    child_window = window_emulator->RegisterWindow(
        kChildClassName, kChildWindowRect, kChildClientOffset, kChildClientSize,
        kScaleFactor);

    window_emulator->SetRoot(child_window, root_window);
  }

  LayoutManager layout_mgr(CreateDefaultGUIFontEmulator(), window_emulator);

  ApplicationInfo app_info;

  RendererCommand command;
  SetRenderereCommandForTest(false, true, false, 0, child_window, &command);

  // Replace the default values with those of MSInfo32.
  command.clear_application_info();
  AppInfoUtil::SetBasicApplicationInfo(
      command.mutable_application_info(), child_window,
      (ApplicationInfo::ShowCandidateWindow |
       ApplicationInfo::ShowSuggestWindow |
       ApplicationInfo::ShowCompositionWindow));

  AppInfoUtil::SetCompositionForm(command.mutable_application_info(),
                                  CompositionForm::POINT, 2, 1, 0, 0, 0, 0);

  AppInfoUtil::SetCaretInfo(command.mutable_application_info(), true, 2, 1, 3,
                            19, child_window);

  CandidateWindowLayout candidate_layout;
  std::vector<CompositionWindowLayout> layouts;
  bool result = false;

  layouts.clear();
  candidate_layout.Clear();
  result =
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout);
  EXPECT_TRUE(result);

  // Default GUI font should be selected.
  CLogFont default_font = GetFont(true, false);
  default_font.lfHeight = 18;
  default_font.lfWidth = 0;

  ASSERT_EQ(3, layouts.size());

  // The first line
  {
    const CompositionWindowLayout &layout = layouts.at(0);
    EXPECT_COMPOSITION_WINDOW_LAYOUT(955, 1074, 1065, 1092, 0, 0, 110, 18, 0, 0,
                                     0, 0, 0, 0, default_font, layout);
    {
      const char kMsg[] = "これは、Google";
      std::wstring msg;
      mozc::Util::UTF8ToWide(kMsg, &msg);
      EXPECT_EQ(msg, layout.text);
    }
    ASSERT_EQ(3, layout.marker_layouts.size());

    EXPECT_EQ(CPoint(0, 17), layout.marker_layouts[0].from);
    EXPECT_EQ(CPoint(42, 17), layout.marker_layouts[0].to);
    EXPECT_FALSE(layout.marker_layouts[0].highlighted);
    EXPECT_EQ(CPoint(45, 17), layout.marker_layouts[1].from);
    EXPECT_EQ(CPoint(57, 17), layout.marker_layouts[1].to);
    EXPECT_FALSE(layout.marker_layouts[1].highlighted);
    EXPECT_EQ(CPoint(60, 17), layout.marker_layouts[2].from);
    EXPECT_EQ(CPoint(108, 17), layout.marker_layouts[2].to);
    EXPECT_TRUE(layout.marker_layouts[2].highlighted);
  }

  // The second line
  {
    const CompositionWindowLayout &layout = layouts.at(1);
    EXPECT_COMPOSITION_WINDOW_LAYOUT(953, 1092, 1067, 1110, 0, 0, 114, 18, 0, 0,
                                     0, 0, 0, 0, default_font, layout);
    {
      const char kMsg[] = "日本語入力のTes";
      std::wstring msg;
      mozc::Util::UTF8ToWide(kMsg, &msg);
      EXPECT_EQ(msg, layout.text);
    }
    ASSERT_EQ(2, layout.marker_layouts.size());

    EXPECT_EQ(CPoint(0, 17), layout.marker_layouts[0].from);
    EXPECT_EQ(CPoint(87, 17), layout.marker_layouts[0].to);
    EXPECT_FALSE(layout.marker_layouts[0].highlighted);
    EXPECT_EQ(CPoint(90, 17), layout.marker_layouts[1].from);
    EXPECT_EQ(CPoint(114, 17), layout.marker_layouts[1].to);
    EXPECT_FALSE(layout.marker_layouts[1].highlighted);
  }

  // The third line
  {
    const CompositionWindowLayout &layout = layouts.at(2);
    EXPECT_COMPOSITION_WINDOW_LAYOUT(953, 1110, 989, 1128, 0, 0, 35, 18, 0, 0,
                                     35, 0, 36, 18, default_font, layout);
    {
      const char kMsg[] = "tです";
      std::wstring msg;
      mozc::Util::UTF8ToWide(kMsg, &msg);
      EXPECT_EQ(msg, layout.text);
    }
    ASSERT_EQ(1, layout.marker_layouts.size());

    EXPECT_EQ(CPoint(0, 17), layout.marker_layouts[0].from);
    EXPECT_EQ(CPoint(35, 17), layout.marker_layouts[0].to);
    EXPECT_FALSE(layout.marker_layouts[0].highlighted);
  }

  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1015, 1092, 1015, 1074, 1065, 1092,
                                         candidate_layout);
}

// Check if LayoutManager can handle preedits which contains surrogate pair.
// See b/4159275 for details.
TEST_F(Win32RendererUtilTest,
       CheckSurrogatePairInHorizontalComposition_Issue4159275) {
  const int kCursorOffsetX = 150;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(CreateDefaultGUIFontEmulator(),
                           CreateWindowEmulatorWithDPIScaling(1.0, &hwnd));

  RendererCommand command;
  SetRenderereCommandForSurrogatePair(false, false, kCursorOffsetX, hwnd,
                                      &command);

  CLogFont logfont;
  EXPECT_TRUE(mozc::win32::FontUtil::ToLOGFONT(
      command.application_info().composition_font(), &logfont));

  std::vector<CompositionWindowLayout> layouts;
  CandidateWindowLayout candidate_layout;
  EXPECT_TRUE(
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout));

  ASSERT_EQ(1, layouts.size());

  // The first line
  {
    const CompositionWindowLayout &layout = layouts.at(0);
    EXPECT_COMPOSITION_WINDOW_LAYOUT(1193, 648, 1554, 697, 0, 0, 360, 49, 0, 0,
                                     360, 0, 361, 49, logfont, layout);
    {
      const char kMsg[] = "𠮟咤𠮟咤𠮟咤𠮟咤";
      std::wstring msg;
      mozc::Util::UTF8ToWide(kMsg, &msg);
      EXPECT_EQ(msg, layout.text);
    }
    ASSERT_EQ(4, layout.marker_layouts.size());

    EXPECT_EQ(CPoint(0, 48), layout.marker_layouts[0].from);
    EXPECT_EQ(CPoint(81, 48), layout.marker_layouts[0].to);
    EXPECT_FALSE(layout.marker_layouts[0].highlighted);
    EXPECT_EQ(CPoint(90, 48), layout.marker_layouts[1].from);
    EXPECT_EQ(CPoint(171, 48), layout.marker_layouts[1].to);
    EXPECT_FALSE(layout.marker_layouts[1].highlighted);
    EXPECT_EQ(CPoint(180, 48), layout.marker_layouts[2].from);
    EXPECT_EQ(CPoint(261, 48), layout.marker_layouts[2].to);
    EXPECT_TRUE(layout.marker_layouts[2].highlighted);
    EXPECT_EQ(CPoint(270, 48), layout.marker_layouts[3].from);
    EXPECT_EQ(CPoint(360, 48), layout.marker_layouts[3].to);
    EXPECT_FALSE(layout.marker_layouts[3].highlighted);
  }

  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1373, 697, 1373, 648, 1553, 697,
                                         candidate_layout);
}

// Check if LayoutManager can handle preedits which contains surrogate pair.
// See b/4159275 for details.
TEST_F(Win32RendererUtilTest,
       CheckSurrogatePairInVerticalComposition_Issue4159275) {
  const int kCursorOffsetY = 175;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(CreateDefaultGUIFontEmulator(),
                           CreateWindowEmulatorWithDPIScaling(1.0, &hwnd));

  RendererCommand command;
  SetRenderereCommandForSurrogatePair(false, true, kCursorOffsetY, hwnd,
                                      &command);

  CLogFont logfont;
  EXPECT_TRUE(mozc::win32::FontUtil::ToLOGFONT(
      command.application_info().composition_font(), &logfont));
  logfont.lfOrientation = 2700;

  std::vector<CompositionWindowLayout> layouts;
  CandidateWindowLayout candidate_layout;
  EXPECT_TRUE(
      layout_mgr.LayoutCompositionWindow(command, &layouts, &candidate_layout));

  ASSERT_EQ(1, layouts.size());

  // The first line
  {
    const CompositionWindowLayout &layout = layouts.at(0);
    EXPECT_COMPOSITION_WINDOW_LAYOUT(1932, 712, 1983, 1073, 0, 0, 51, 360, 51,
                                     0, 0, 360, 51, 361, logfont, layout);
    {
      const char kMsg[] = "𠮟咤𠮟咤𠮟咤𠮟咤";
      std::wstring msg;
      mozc::Util::UTF8ToWide(kMsg, &msg);
      EXPECT_EQ(msg, layout.text);
    }
    ASSERT_EQ(4, layout.marker_layouts.size());

    EXPECT_EQ(CPoint(50, 0), layout.marker_layouts[0].from);
    EXPECT_EQ(CPoint(50, 81), layout.marker_layouts[0].to);
    EXPECT_FALSE(layout.marker_layouts[0].highlighted);
    EXPECT_EQ(CPoint(50, 90), layout.marker_layouts[1].from);
    EXPECT_EQ(CPoint(50, 171), layout.marker_layouts[1].to);
    EXPECT_FALSE(layout.marker_layouts[1].highlighted);
    EXPECT_EQ(CPoint(50, 180), layout.marker_layouts[2].from);
    EXPECT_EQ(CPoint(50, 261), layout.marker_layouts[2].to);
    EXPECT_TRUE(layout.marker_layouts[2].highlighted);
    EXPECT_EQ(CPoint(50, 270), layout.marker_layouts[3].from);
    EXPECT_EQ(CPoint(50, 360), layout.marker_layouts[3].to);
    EXPECT_FALSE(layout.marker_layouts[3].highlighted);
  }

  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1932, 892, 1932, 892, 1983, 1072,
                                         candidate_layout);
}

TEST_F(Win32RendererUtilTest, GetWritingDirectionTest) {
  RendererCommand command;

  // Horizontal
  SetRenderereCommandForTest(false, true, false, 0, nullptr, &command);
  EXPECT_EQ(LayoutManager::HORIZONTAL_WRITING,
            LayoutManager::GetWritingDirection(command.application_info()));

  // Vertical
  SetRenderereCommandForTest(false, true, true, 0, nullptr, &command);
  EXPECT_EQ(LayoutManager::VERTICAL_WRITING,
            LayoutManager::GetWritingDirection(command.application_info()));

  // Unspecified
  command.mutable_application_info()
      ->mutable_composition_font()
      ->clear_escapement();
  EXPECT_EQ(LayoutManager::WRITING_DIRECTION_UNSPECIFIED,
            LayoutManager::GetWritingDirection(command.application_info()));

  // Unspecified
  command.mutable_application_info()->clear_composition_font();
  EXPECT_EQ(LayoutManager::WRITING_DIRECTION_UNSPECIFIED,
            LayoutManager::GetWritingDirection(command.application_info()));
}

// Hidemaru 8.01a True-Inline
TEST_F(Win32RendererUtilTest, Hidemaru_Horizontal_Suggest) {
  const wchar_t kClassName[] = L"HM32CLIENT";
  const CRect kWindowRect(0, 20, 2016, 1050);
  const CPoint kClientOffset(8, 42);
  const CSize kClientSize(2000, 1000);
  const double kScaleFactor = 1.0;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(
      CreateDefaultGUIFontEmulator(),
      CreateWindowEmulator(kClassName, kWindowRect, kClientOffset, kClientSize,
                           kScaleFactor, &hwnd));

  ApplicationInfo app_info;
  AppInfoUtil::SetBasicApplicationInfo(&app_info, hwnd,
                                       ApplicationInfo::ShowSuggestWindow);

  AppInfoUtil::SetCompositionFont(&app_info, -15, 0, 0, 0, FW_NORMAL,
                                  SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS,
                                  CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                  FF_MODERN | FIXED_PITCH, "ＭＳ ゴシック");

  AppInfoUtil::SetCompositionForm(&app_info, CompositionForm::RECT, 112, 25, 48,
                                  0, 1408, 552);

  AppInfoUtil::SetCandidateForm(&app_info, CandidateForm::EXCLUDE, 112, 42, 112,
                                25, 752, 42);

  AppInfoUtil::SetCaretInfo(&app_info, true, 160, 25, 162, 40, hwnd);

  CandidateWindowLayout layout;
  EXPECT_TRUE(layout_mgr.LayoutCandidateWindowForSuggestion(app_info, &layout));
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(168, 102, 168, 87, 170, 102, layout);
}

// Hidemaru 8.01a True-Inline
TEST_F(Win32RendererUtilTest, Hidemaru_Horizontal_Convert) {
  const wchar_t kClassName[] = L"HM32CLIENT";
  const CRect kWindowRect(0, 20, 2016, 1050);
  const CPoint kClientOffset(8, 42);
  const CSize kClientSize(2000, 1000);
  const double kScaleFactor = 1.0;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(
      CreateDefaultGUIFontEmulator(),
      CreateWindowEmulator(kClassName, kWindowRect, kClientOffset, kClientSize,
                           kScaleFactor, &hwnd));

  ApplicationInfo app_info;

  AppInfoUtil::SetBasicApplicationInfo(&app_info, hwnd,
                                       ApplicationInfo::ShowCandidateWindow);

  AppInfoUtil::SetCompositionFont(&app_info, -15, 0, 0, 0, FW_NORMAL,
                                  SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS,
                                  CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                  FF_MODERN | FIXED_PITCH, "ＭＳ ゴシック");

  AppInfoUtil::SetCompositionForm(&app_info, CompositionForm::RECT, 112, 25, 48,
                                  0, 1408, 552);

  AppInfoUtil::SetCandidateForm(&app_info, CandidateForm::EXCLUDE, 128, 25, 128,
                                25, 144, 42);

  AppInfoUtil::SetCaretInfo(&app_info, true, 160, 25, 162, 40, hwnd);

  CandidateWindowLayout layout;
  EXPECT_TRUE(layout_mgr.LayoutCandidateWindowForConversion(app_info, &layout));
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(136, 87, 136, 87, 152, 104, layout);
}

// Hidemaru 8.01a True-Inline
TEST_F(Win32RendererUtilTest, Hidemaru_Vertical_Suggest) {
  const wchar_t kClassName[] = L"HM32CLIENT";
  const CRect kWindowRect(0, 20, 2016, 1050);
  const CPoint kClientOffset(8, 42);
  const CSize kClientSize(2000, 1000);
  const double kScaleFactor = 1.0;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(
      CreateDefaultGUIFontEmulator(),
      CreateWindowEmulator(kClassName, kWindowRect, kClientOffset, kClientSize,
                           kScaleFactor, &hwnd));

  ApplicationInfo app_info;

  AppInfoUtil::SetBasicApplicationInfo(&app_info, hwnd,
                                       ApplicationInfo::ShowSuggestWindow);

  AppInfoUtil::SetCompositionFont(&app_info, -15, 0, 2700, 0, FW_NORMAL,
                                  SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS,
                                  CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                  FF_MODERN | FIXED_PITCH, "@ＭＳ ゴシック");

  AppInfoUtil::SetCompositionForm(&app_info, CompositionForm::RECT, 660, 48, 0,
                                  48, 688, 397);

  AppInfoUtil::SetCandidateForm(&app_info, CandidateForm::EXCLUDE, 660, 67, 641,
                                48, 660, 400);

  AppInfoUtil::SetCaretInfo(&app_info, true, 644, 96, 661, 98, hwnd);

  CandidateWindowLayout layout;
  EXPECT_TRUE(layout_mgr.LayoutCandidateWindowForSuggestion(app_info, &layout));
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(652, 158, 652, 158, 669, 160, layout);
}

// Hidemaru 8.01a True-Inline
TEST_F(Win32RendererUtilTest, Hidemaru_Vertical_Convert) {
  const wchar_t kClassName[] = L"HM32CLIENT";
  const CRect kWindowRect(0, 20, 2016, 1050);
  const CPoint kClientOffset(8, 42);
  const CSize kClientSize(2000, 1000);
  const double kScaleFactor = 1.0;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(
      CreateDefaultGUIFontEmulator(),
      CreateWindowEmulator(kClassName, kWindowRect, kClientOffset, kClientSize,
                           kScaleFactor, &hwnd));

  ApplicationInfo app_info;

  AppInfoUtil::SetBasicApplicationInfo(&app_info, hwnd,
                                       ApplicationInfo::ShowCandidateWindow |
                                           ApplicationInfo::ShowSuggestWindow);

  AppInfoUtil::SetCompositionFont(&app_info, -15, 0, 2700, 0, FW_NORMAL,
                                  SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS,
                                  CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                  FF_MODERN | FIXED_PITCH, "@ＭＳ ゴシック");

  AppInfoUtil::SetCompositionForm(&app_info, CompositionForm::RECT, 660, 48, 0,
                                  48, 668, 397);

  AppInfoUtil::SetCandidateForm(&app_info, CandidateForm::EXCLUDE, 644, 63, 644,
                                63, 661, 80);

  AppInfoUtil::SetCaretInfo(&app_info, true, 644, 96, 661, 98, hwnd);

  CandidateWindowLayout layout;
  EXPECT_TRUE(layout_mgr.LayoutCandidateWindowForConversion(app_info, &layout));
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(652, 125, 652, 125, 669, 142, layout);
}

// Open Office Writer 3.01
TEST_F(Win32RendererUtilTest, OOo_Suggest) {
  const wchar_t kClassName[] = L"SALFRAME";
  const CRect kWindowRect(0, 20, 2016, 1050);
  const CPoint kClientOffset(8, 42);
  const CSize kClientSize(2000, 1000);
  const double kScaleFactor = 1.0;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(
      CreateDefaultGUIFontEmulator(),
      CreateWindowEmulator(kClassName, kWindowRect, kClientOffset, kClientSize,
                           kScaleFactor, &hwnd));

  ApplicationInfo app_info;

  AppInfoUtil::SetBasicApplicationInfo(&app_info, hwnd,
                                       ApplicationInfo::ShowSuggestWindow);

  AppInfoUtil::SetCompositionFont(
      &app_info, -16, 0, 0, 0, FW_DONTCARE, ANSI_CHARSET, OUT_TT_PRECIS,
      CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, 23, "Times New Roman");

  AppInfoUtil::SetCompositionForm(&app_info, CompositionForm::POINT, 292, 253,
                                  0, 0, 0, 0);

  AppInfoUtil::SetCaretInfo(&app_info, true, 292, 253, 294, 273, hwnd);

  CandidateWindowLayout layout;
  EXPECT_TRUE(layout_mgr.LayoutCandidateWindowForSuggestion(app_info, &layout));
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(300, 335, 300, 315, 302, 335, layout);
}

// Open Office Writer 3.01
TEST_F(Win32RendererUtilTest, OOo_Convert) {
  const wchar_t kClassName[] = L"SALFRAME";
  const CRect kWindowRect(0, 20, 2016, 1050);
  const CPoint kClientOffset(8, 42);
  const CSize kClientSize(2000, 1000);
  const double kScaleFactor = 1.0;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(
      CreateDefaultGUIFontEmulator(),
      CreateWindowEmulator(kClassName, kWindowRect, kClientOffset, kClientSize,
                           kScaleFactor, &hwnd));

  ApplicationInfo app_info;

  AppInfoUtil::SetBasicApplicationInfo(&app_info, hwnd,
                                       ApplicationInfo::ShowCandidateWindow |
                                           ApplicationInfo::ShowSuggestWindow);

  AppInfoUtil::SetCompositionFont(
      &app_info, -16, 0, 0, 0, FW_DONTCARE, ANSI_CHARSET, OUT_TT_PRECIS,
      CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, 23, "Times New Roman");

  AppInfoUtil::SetCompositionForm(&app_info, CompositionForm::POINT, 264, 253,
                                  0, 0, 0, 0);

  AppInfoUtil::SetCandidateForm(&app_info, CandidateForm::EXCLUDE, 250, 258,
                                250, 257, 253, 275);

  AppInfoUtil::SetCaretInfo(&app_info, true, 264, 253, 266, 273, hwnd);

  CandidateWindowLayout layout;
  EXPECT_TRUE(layout_mgr.LayoutCandidateWindowForConversion(app_info, &layout));
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(258, 320, 258, 319, 261, 337, layout);
}

// Pidgin 2.6.1
TEST_F(Win32RendererUtilTest, Pidgin_Indicator) {
  const wchar_t kClassName[] = L"gdkWindowToplevel";
  const CRect kWindowRect(0, 20, 2016, 1050);
  const CPoint kClientOffset(8, 42);
  const CSize kClientSize(2000, 1000);
  const double kScaleFactor = 1.0;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(
      CreateDefaultGUIFontEmulator(),
      CreateWindowEmulator(kClassName, kWindowRect, kClientOffset, kClientSize,
                           kScaleFactor, &hwnd));

  ApplicationInfo app_info;

  AppInfoUtil::SetBasicApplicationInfo(&app_info, hwnd,
                                       ApplicationInfo::ShowCandidateWindow |
                                           ApplicationInfo::ShowSuggestWindow);

  AppInfoUtil::SetCompositionFont(
      &app_info, -16, 0, 0, 0, FW_NORMAL, SHIFTJIS_CHARSET, OUT_STROKE_PRECIS,
      CLIP_STROKE_PRECIS, DRAFT_QUALITY, 50, "メイリオ");

  AppInfoUtil::SetCompositionForm(&app_info, CompositionForm::POINT, 48, 589,
                                  96504880, 2617504, 97141432, 2617480);

  AppInfoUtil::SetCandidateForm(&app_info, CandidateForm::CANDIDATEPOS, 32, 636,
                                40706080, 96552944, 2615824, 1815374140);

  AppInfoUtil::SetCaretInfo(&app_info, false, 0, 0, 0, 0, nullptr);

  IndicatorWindowLayout layout;
  EXPECT_TRUE(layout_mgr.LayoutIndicatorWindow(app_info, &layout));
  EXPECT_EQ(CRect(56, 651, 57, 667), layout.window_rect);
  EXPECT_FALSE(layout.is_vertical);
}

// Pidgin 2.6.1
TEST_F(Win32RendererUtilTest, Pidgin_Suggest) {
  const wchar_t kClassName[] = L"gdkWindowToplevel";
  const CRect kWindowRect(0, 20, 2016, 1050);
  const CPoint kClientOffset(8, 42);
  const CSize kClientSize(2000, 1000);
  const double kScaleFactor = 1.0;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(
      CreateDefaultGUIFontEmulator(),
      CreateWindowEmulator(kClassName, kWindowRect, kClientOffset, kClientSize,
                           kScaleFactor, &hwnd));

  ApplicationInfo app_info;

  AppInfoUtil::SetBasicApplicationInfo(&app_info, hwnd,
                                       ApplicationInfo::ShowSuggestWindow);

  AppInfoUtil::SetCompositionFont(
      &app_info, -16, 0, 0, 0, FW_NORMAL, SHIFTJIS_CHARSET, OUT_STROKE_PRECIS,
      CLIP_STROKE_PRECIS, DRAFT_QUALITY, 50, "メイリオ");

  AppInfoUtil::SetCompositionForm(&app_info, CompositionForm::POINT, 48, 589,
                                  96504880, 2617504, 97141432, 2617480);

  AppInfoUtil::SetCandidateForm(&app_info, CandidateForm::CANDIDATEPOS, 48, 636,
                                40706080, 96552944, 2615824, 1815374140);

  AppInfoUtil::SetCaretInfo(&app_info, false, 0, 0, 0, 0, nullptr);

  CandidateWindowLayout layout;
  EXPECT_TRUE(layout_mgr.LayoutCandidateWindowForSuggestion(app_info, &layout));
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(56, 667, 56, 651, 57, 667, layout);
}

// Pidgin 2.6.1
TEST_F(Win32RendererUtilTest, Pidgin_Convert) {
  const wchar_t kClassName[] = L"gdkWindowToplevel";
  const CRect kWindowRect(0, 20, 2016, 1050);
  const CPoint kClientOffset(8, 42);
  const CSize kClientSize(2000, 1000);
  const double kScaleFactor = 1.0;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(
      CreateDefaultGUIFontEmulator(),
      CreateWindowEmulator(kClassName, kWindowRect, kClientOffset, kClientSize,
                           kScaleFactor, &hwnd));

  ApplicationInfo app_info;

  AppInfoUtil::SetBasicApplicationInfo(&app_info, hwnd,
                                       ApplicationInfo::ShowCandidateWindow |
                                           ApplicationInfo::ShowSuggestWindow);

  AppInfoUtil::SetCompositionFont(
      &app_info, -16, 0, 0, 0, FW_NORMAL, SHIFTJIS_CHARSET, OUT_STROKE_PRECIS,
      CLIP_STROKE_PRECIS, DRAFT_QUALITY, 50, "メイリオ");

  AppInfoUtil::SetCompositionForm(&app_info, CompositionForm::POINT, 48, 589,
                                  96504880, 2617504, 97141432, 2617480);

  AppInfoUtil::SetCandidateForm(&app_info, CandidateForm::CANDIDATEPOS, 32, 636,
                                40706080, 96552944, 2615824, 1815374140);

  AppInfoUtil::SetCaretInfo(&app_info, false, 0, 0, 0, 0, nullptr);

  CandidateWindowLayout layout;
  EXPECT_TRUE(layout_mgr.LayoutCandidateWindowForConversion(app_info, &layout));
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(32, 656, 32, 640, 33, 656, layout);
}

// V2C 2.1.6 on JRE 1.6.0.21 (32-bit)
TEST_F(Win32RendererUtilTest, V2C_Indicator) {
  const wchar_t kClassName[] = L"SunAwtFrame";
  const CRect kWindowRect(977, 446, 2042, 1052);
  const CPoint kClientOffset(8, 8);
  const CSize kClientSize(1049, 569);
  const double kScaleFactor = 1.0;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(
      CreateDefaultGUIFontEmulator(),
      CreateWindowEmulator(kClassName, kWindowRect, kClientOffset, kClientSize,
                           kScaleFactor, &hwnd));

  ApplicationInfo app_info;

  AppInfoUtil::SetBasicApplicationInfo(&app_info, hwnd,
                                       ApplicationInfo::ShowSuggestWindow);

  // V2C occasionally creates zero-initialized CANDIDATEFORM and maintains
  // it regardless of the actual position of the composition.
  AppInfoUtil::SetCompositionForm(&app_info, CompositionForm::DEFAULT, 0, 0, 0,
                                  0, 0, 0);

  AppInfoUtil::SetCaretInfo(&app_info, false, 0, 0, 0, 0, nullptr);

  IndicatorWindowLayout layout;
  EXPECT_FALSE(layout_mgr.LayoutIndicatorWindow(app_info, &layout));
}

// V2C 2.1.6 on JRE 1.6.0.21 (32-bit)
TEST_F(Win32RendererUtilTest, V2C_Suggest) {
  const wchar_t kClassName[] = L"SunAwtFrame";
  const CRect kWindowRect(977, 446, 2042, 1052);
  const CPoint kClientOffset(8, 8);
  const CSize kClientSize(1049, 569);
  const double kScaleFactor = 1.0;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(
      CreateDefaultGUIFontEmulator(),
      CreateWindowEmulator(kClassName, kWindowRect, kClientOffset, kClientSize,
                           kScaleFactor, &hwnd));

  ApplicationInfo app_info;

  AppInfoUtil::SetBasicApplicationInfo(&app_info, hwnd,
                                       ApplicationInfo::ShowSuggestWindow);

  // V2C occasionally creates zero-initialized CANDIDATEFORM and maintains
  // it regardless of the actual position of the composition.
  AppInfoUtil::SetCompositionForm(&app_info, CompositionForm::DEFAULT, 0, 0, 0,
                                  0, 0, 0);

  AppInfoUtil::SetCaretInfo(&app_info, false, 0, 0, 0, 0, nullptr);

  CandidateWindowLayout layout;
  EXPECT_TRUE(layout_mgr.LayoutCandidateWindowForSuggestion(app_info, &layout));
  EXPECT_NON_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(985, 1023, layout);
}

// V2C 2.1.6 on JRE 1.6.0.21 (32-bit)
TEST_F(Win32RendererUtilTest, V2C_Convert) {
  const wchar_t kClassName[] = L"SunAwtFrame";
  const CRect kWindowRect(977, 446, 2042, 1052);
  const CPoint kClientOffset(8, 8);
  const CSize kClientSize(1049, 569);
  const double kScaleFactor = 1.0;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(
      CreateDefaultGUIFontEmulator(),
      CreateWindowEmulator(kClassName, kWindowRect, kClientOffset, kClientSize,
                           kScaleFactor, &hwnd));

  ApplicationInfo app_info;

  AppInfoUtil::SetBasicApplicationInfo(&app_info, hwnd,
                                       ApplicationInfo::ShowCandidateWindow |
                                           ApplicationInfo::ShowSuggestWindow);

  // V2C occasionally creates zero-initialized CANDIDATEFORM and maintains
  // it regardless of the actual position of the composition.
  AppInfoUtil::SetCompositionForm(&app_info, CompositionForm::DEFAULT, 0, 0, 0,
                                  0, 0, 0);

  AppInfoUtil::SetCandidateForm(&app_info, CandidateForm::CANDIDATEPOS, 234,
                                523, 1272967816, 1974044135, -348494668, -2);

  AppInfoUtil::SetCaretInfo(&app_info, false, 0, 0, 0, 0, nullptr);

  CandidateWindowLayout layout;
  EXPECT_TRUE(layout_mgr.LayoutCandidateWindowForConversion(app_info, &layout));
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1211, 969, 1211, 951, 1212, 969,
                                         layout);
}

// Qt 4.6.3
TEST_F(Win32RendererUtilTest, Qt_Suggest) {
  const wchar_t kClassName[] = L"QWidget";
  const CRect kWindowRect(0, 20, 2016, 1050);
  const CPoint kClientOffset(8, 42);
  const CSize kClientSize(2000, 1000);
  const double kScaleFactor = 1.0;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(
      CreateDefaultGUIFontEmulator(),
      CreateWindowEmulator(kClassName, kWindowRect, kClientOffset, kClientSize,
                           kScaleFactor, &hwnd));

  ApplicationInfo app_info;

  AppInfoUtil::SetBasicApplicationInfo(&app_info, hwnd,
                                       ApplicationInfo::ShowSuggestWindow);

  AppInfoUtil::SetCompositionFont(
      &app_info, -12, 0, 0, 0, FW_DONTCARE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
      CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, 0, "メイリオ");

  AppInfoUtil::SetCompositionForm(&app_info, CompositionForm::FORCE_POSITION,
                                  211, 68, 18901544, 103737984, 4247412,
                                  19851904);

  AppInfoUtil::SetCandidateForm(&app_info, CandidateForm::EXCLUDE, 211, 87, 211,
                                68, 221, 87);

  AppInfoUtil::SetCaretInfo(&app_info, false, 211, 68, 212, 69, hwnd);

  CandidateWindowLayout layout;
  EXPECT_TRUE(layout_mgr.LayoutCandidateWindowForSuggestion(app_info, &layout));
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(219, 149, 219, 130, 229, 149, layout);
}

// Qt 4.6.3
TEST_F(Win32RendererUtilTest, Qt_Convert) {
  const wchar_t kClassName[] = L"QWidget";
  const CRect kWindowRect(0, 20, 2016, 1050);
  const CPoint kClientOffset(8, 42);
  const CSize kClientSize(2000, 1000);
  const double kScaleFactor = 1.0;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(
      CreateDefaultGUIFontEmulator(),
      CreateWindowEmulator(kClassName, kWindowRect, kClientOffset, kClientSize,
                           kScaleFactor, &hwnd));

  ApplicationInfo app_info;

  AppInfoUtil::SetBasicApplicationInfo(&app_info, hwnd,
                                       ApplicationInfo::ShowCandidateWindow |
                                           ApplicationInfo::ShowSuggestWindow);

  AppInfoUtil::SetCompositionFont(
      &app_info, -12, 0, 0, 0, FW_DONTCARE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
      CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, 0, "メイリオ");

  AppInfoUtil::SetCompositionForm(&app_info, CompositionForm::FORCE_POSITION,
                                  187, 68, 18901544, 103737984, 4247412,
                                  19851904);

  AppInfoUtil::SetCandidateForm(&app_info, CandidateForm::EXCLUDE, 187, 87, 187,
                                68, 197, 87);

  AppInfoUtil::SetCaretInfo(&app_info, false, 187, 68, 188, 69, hwnd);

  CandidateWindowLayout layout;
  EXPECT_TRUE(layout_mgr.LayoutCandidateWindowForConversion(app_info, &layout));
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(195, 149, 195, 130, 205, 149, layout);
}

// Wordpad x86 on Vista SP1
TEST_F(Win32RendererUtilTest, Wordpad_Vista_Indicator) {
  const wchar_t kClassName[] = L"RICHEDIT50W";
  const CRect kWindowRect(617, 573, 1319, 881);
  const CPoint kClientOffset(2, 22);
  const CSize kClientSize(698, 304);
  const double kScaleFactor = 1.0;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(
      CreateDefaultGUIFontEmulator(),
      CreateWindowEmulator(kClassName, kWindowRect, kClientOffset, kClientSize,
                           kScaleFactor, &hwnd));

  ApplicationInfo app_info;

  AppInfoUtil::SetBasicApplicationInfo(&app_info, hwnd,
                                       ApplicationInfo::ShowCandidateWindow |
                                           ApplicationInfo::ShowSuggestWindow);

  AppInfoUtil::SetCompositionFont(
      &app_info, 10, 0, 0, 0, FW_DONTCARE, SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS,
      CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, 17, "ＭＳ Ｐゴシック");

  AppInfoUtil::SetCandidateForm(&app_info, CandidateForm::EXCLUDE, 62, 42, 62,
                                21, 64, 42);

  AppInfoUtil::SetCompositionTarget(&app_info, 1, 693, 596, 17, 625, 579, 1317,
                                    879);

  AppInfoUtil::SetCaretInfo(&app_info, false, 74, 21, 75, 38, hwnd);

  IndicatorWindowLayout layout;
  EXPECT_TRUE(layout_mgr.LayoutIndicatorWindow(app_info, &layout));
  EXPECT_EQ(CRect(693, 596, 694, 613), layout.window_rect);
  EXPECT_FALSE(layout.is_vertical);
}

// Wordpad x86 on Vista SP1
TEST_F(Win32RendererUtilTest, Wordpad_Vista_Suggest) {
  const wchar_t kClassName[] = L"RICHEDIT50W";
  const CRect kWindowRect(617, 573, 1319, 881);
  const CPoint kClientOffset(2, 22);
  const CSize kClientSize(698, 304);
  const double kScaleFactor = 1.0;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(
      CreateDefaultGUIFontEmulator(),
      CreateWindowEmulator(kClassName, kWindowRect, kClientOffset, kClientSize,
                           kScaleFactor, &hwnd));

  ApplicationInfo app_info;

  AppInfoUtil::SetBasicApplicationInfo(&app_info, hwnd,
                                       ApplicationInfo::ShowSuggestWindow);

  AppInfoUtil::SetCompositionFont(
      &app_info, 10, 0, 0, 0, FW_DONTCARE, SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS,
      CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, 17, "ＭＳ Ｐゴシック");

  AppInfoUtil::SetCompositionTarget(&app_info, 0, 681, 596, 17, 625, 579, 1317,
                                    879);

  AppInfoUtil::SetCaretInfo(&app_info, false, 98, 21, 99, 38, hwnd);

  CandidateWindowLayout layout;
  EXPECT_TRUE(layout_mgr.LayoutCandidateWindowForSuggestion(app_info, &layout));
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(681, 613, 681, 596, 682, 613, layout);
}

// Wordpad x86 on Vista SP1
TEST_F(Win32RendererUtilTest, Wordpad_Vista_Convert) {
  const wchar_t kClassName[] = L"RICHEDIT50W";
  const CRect kWindowRect(617, 573, 1319, 881);
  const CPoint kClientOffset(2, 22);
  const CSize kClientSize(698, 304);
  const double kScaleFactor = 1.0;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(
      CreateDefaultGUIFontEmulator(),
      CreateWindowEmulator(kClassName, kWindowRect, kClientOffset, kClientSize,
                           kScaleFactor, &hwnd));

  ApplicationInfo app_info;

  AppInfoUtil::SetBasicApplicationInfo(&app_info, hwnd,
                                       ApplicationInfo::ShowCandidateWindow |
                                           ApplicationInfo::ShowSuggestWindow);

  AppInfoUtil::SetCompositionFont(
      &app_info, 10, 0, 0, 0, FW_DONTCARE, SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS,
      CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, 17, "ＭＳ Ｐゴシック");

  AppInfoUtil::SetCandidateForm(&app_info, CandidateForm::EXCLUDE, 62, 42, 62,
                                21, 64, 42);

  AppInfoUtil::SetCompositionTarget(&app_info, 1, 693, 596, 17, 625, 579, 1317,
                                    879);

  AppInfoUtil::SetCaretInfo(&app_info, false, 74, 21, 75, 38, hwnd);

  CandidateWindowLayout layout;
  EXPECT_TRUE(layout_mgr.LayoutCandidateWindowForConversion(app_info, &layout));
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(693, 613, 681, 616, 683, 637, layout);
}

// MS Word 2010 x64, True Inline, Horizontal
TEST_F(Win32RendererUtilTest, MSWord2010_Horizontal_Suggest) {
  const wchar_t kClassName[] = L"_WwG";
  const CRect kWindowRect(434, 288, 1275, 841);
  const CPoint kClientOffset(0, 0);
  const CSize kClientSize(841, 553);
  const double kScaleFactor = 1.0;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(
      CreateDefaultGUIFontEmulator(),
      CreateWindowEmulator(kClassName, kWindowRect, kClientOffset, kClientSize,
                           kScaleFactor, &hwnd));

  ApplicationInfo app_info;

  AppInfoUtil::SetBasicApplicationInfo(&app_info, hwnd,
                                       ApplicationInfo::ShowSuggestWindow);

  AppInfoUtil::SetCompositionFont(&app_info, -14, 0, 0, 0, FW_NORMAL,
                                  SHIFTJIS_CHARSET, OUT_SCREEN_OUTLINE_PRECIS,
                                  CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, 17,
                                  "ＭＳ 明朝");

  AppInfoUtil::SetCandidateForm(&app_info, CandidateForm::EXCLUDE, 234, 176,
                                136, 176, 703, 193);

  AppInfoUtil::SetCompositionTarget(&app_info, 0, 626, 464, 17, 570, 288, 1137,
                                    841);

  AppInfoUtil::SetCaretInfo(&app_info, false, 220, 176, 221, 194, hwnd);

  CandidateWindowLayout layout;
  EXPECT_TRUE(layout_mgr.LayoutCandidateWindowForSuggestion(app_info, &layout));
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(626, 481, 626, 464, 627, 481, layout);
}

// MS Word 2010 x64, True Inline, Horizontal
TEST_F(Win32RendererUtilTest, MSWord2010_Horizontal_Convert) {
  const wchar_t kClassName[] = L"_WwG";
  const CRect kWindowRect(434, 288, 1275, 841);
  const CPoint kClientOffset(0, 0);
  const CSize kClientSize(841, 553);
  const double kScaleFactor = 1.0;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(
      CreateDefaultGUIFontEmulator(),
      CreateWindowEmulator(kClassName, kWindowRect, kClientOffset, kClientSize,
                           kScaleFactor, &hwnd));

  ApplicationInfo app_info;

  AppInfoUtil::SetBasicApplicationInfo(&app_info, hwnd,
                                       ApplicationInfo::ShowCandidateWindow |
                                           ApplicationInfo::ShowSuggestWindow);

  AppInfoUtil::SetCompositionFont(&app_info, -14, 0, 0, 0, FW_NORMAL,
                                  SHIFTJIS_CHARSET, OUT_SCREEN_OUTLINE_PRECIS,
                                  CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, 17,
                                  "ＭＳ 明朝");

  AppInfoUtil::SetCandidateForm(&app_info, CandidateForm::EXCLUDE, 206, 178,
                                136, 178, 703, 194);

  AppInfoUtil::SetCompositionTarget(&app_info, 1, 640, 466, 16, 570, 288, 1137,
                                    841);

  AppInfoUtil::SetCaretInfo(&app_info, false, 192, 179, 193, 197, hwnd);

  CandidateWindowLayout layout;
  EXPECT_TRUE(layout_mgr.LayoutCandidateWindowForConversion(app_info, &layout));
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(640, 482, 570, 466, 1137, 482, layout);
}

// MS Word 2010 x64, True Inline, Vertical
TEST_F(Win32RendererUtilTest, MSWord2010_Vertical_Suggest) {
  const wchar_t kClassName[] = L"_WwG";
  const CRect kWindowRect(434, 288, 1275, 824);
  const CPoint kClientOffset(0, 0);
  const CSize kClientSize(841, 536);
  const double kScaleFactor = 1.0;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(
      CreateDefaultGUIFontEmulator(),
      CreateWindowEmulator(kClassName, kWindowRect, kClientOffset, kClientSize,
                           kScaleFactor, &hwnd));

  ApplicationInfo app_info;

  AppInfoUtil::SetBasicApplicationInfo(&app_info, hwnd,
                                       ApplicationInfo::ShowSuggestWindow);

  AppInfoUtil::SetCompositionFont(&app_info, -14, 0, 2700, 2700, FW_NORMAL,
                                  SHIFTJIS_CHARSET, OUT_SCREEN_OUTLINE_PRECIS,
                                  CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, 17,
                                  "@ＭＳ 明朝");

  AppInfoUtil::SetCandidateForm(&app_info, CandidateForm::EXCLUDE, 662, 228,
                                644, 130, 662, 697);

  AppInfoUtil::SetCompositionTarget(&app_info, 0, 1096, 474, 18, 434, 418, 1275,
                                    985);

  AppInfoUtil::SetCaretInfo(&app_info, false, 644, 214, 645, 235, hwnd);

  CandidateWindowLayout layout;
  EXPECT_TRUE(layout_mgr.LayoutCandidateWindowForSuggestion(app_info, &layout));
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1078, 474, 1078, 474, 1096, 475,
                                         layout);
}

// MS Word 2010 x64, True Inline, Vertical
TEST_F(Win32RendererUtilTest, MSWord2010_Vertical_Convert) {
  const wchar_t kClassName[] = L"_WwG";
  const CRect kWindowRect(434, 288, 1275, 824);
  const CPoint kClientOffset(0, 0);
  const CSize kClientSize(841, 536);
  const double kScaleFactor = 1.0;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(
      CreateDefaultGUIFontEmulator(),
      CreateWindowEmulator(kClassName, kWindowRect, kClientOffset, kClientSize,
                           kScaleFactor, &hwnd));

  ApplicationInfo app_info;

  AppInfoUtil::SetBasicApplicationInfo(&app_info, hwnd,
                                       ApplicationInfo::ShowCandidateWindow |
                                           ApplicationInfo::ShowSuggestWindow);

  AppInfoUtil::SetCompositionFont(&app_info, -14, 0, 2700, 2700, FW_NORMAL,
                                  SHIFTJIS_CHARSET, OUT_SCREEN_OUTLINE_PRECIS,
                                  CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, 17,
                                  "@ＭＳ 明朝");

  AppInfoUtil::SetCandidateForm(&app_info, CandidateForm::EXCLUDE, 661, 200,
                                643, 130, 661, 697);

  AppInfoUtil::SetCompositionTarget(&app_info, 1, 1095, 488, 18, 434, 418, 1275,
                                    985);

  AppInfoUtil::SetCaretInfo(&app_info, false, 643, 200, 644, 221, hwnd);

  CandidateWindowLayout layout;
  EXPECT_TRUE(layout_mgr.LayoutCandidateWindowForConversion(app_info, &layout));
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(1077, 488, 1077, 418, 1095, 985,
                                         layout);
}

// Firefox 3.6.10 on Vista SP1 / textarea
TEST_F(Win32RendererUtilTest, Firefox_textarea_Suggest) {
  const wchar_t kClassName[] = L"MozillaWindowClass";
  const CRect kWindowRect(198, 329, 1043, 1133);
  const CPoint kClientOffset(0, 0);
  const CSize kClientSize(845, 804);
  const double kScaleFactor = 1.0;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(
      CreateDefaultGUIFontEmulator(),
      CreateWindowEmulator(kClassName, kWindowRect, kClientOffset, kClientSize,
                           kScaleFactor, &hwnd));

  ApplicationInfo app_info;

  AppInfoUtil::SetBasicApplicationInfo(&app_info, hwnd,
                                       ApplicationInfo::ShowSuggestWindow);

  AppInfoUtil::SetCandidateForm(&app_info, CandidateForm::EXCLUDE, 44, 378, 44,
                                378, 44, 398);

  AppInfoUtil::SetCompositionTarget(&app_info, 0, 242, 707, 20, 198, 329, 1043,
                                    1133);

  AppInfoUtil::SetCaretInfo(&app_info, false, 89, 378, 90, 398, hwnd);

  CandidateWindowLayout layout;
  EXPECT_TRUE(layout_mgr.LayoutCandidateWindowForSuggestion(app_info, &layout));
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(242, 727, 242, 707, 242, 727, layout);
}

// Firefox 3.6.10 on Vista SP1 / textarea
TEST_F(Win32RendererUtilTest, Firefox_textarea_Convert) {
  const wchar_t kClassName[] = L"MozillaWindowClass";
  const CRect kWindowRect(198, 329, 1043, 1133);
  const CPoint kClientOffset(0, 0);
  const CSize kClientSize(845, 804);
  const double kScaleFactor = 1.0;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(
      CreateDefaultGUIFontEmulator(),
      CreateWindowEmulator(kClassName, kWindowRect, kClientOffset, kClientSize,
                           kScaleFactor, &hwnd));

  ApplicationInfo app_info;

  AppInfoUtil::SetBasicApplicationInfo(&app_info, hwnd,
                                       ApplicationInfo::ShowCandidateWindow |
                                           ApplicationInfo::ShowSuggestWindow);

  AppInfoUtil::SetCandidateForm(&app_info, CandidateForm::EXCLUDE, 59, 378, 59,
                                378, 59, 398);

  AppInfoUtil::SetCompositionTarget(&app_info, 1, 257, 707, 20, 198, 329, 1043,
                                    1133);

  AppInfoUtil::SetCaretInfo(&app_info, false, 60, 378, 61, 398, hwnd);

  CandidateWindowLayout layout;
  EXPECT_TRUE(layout_mgr.LayoutCandidateWindowForConversion(app_info, &layout));
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(257, 727, 257, 707, 257, 727, layout);
}

// Chrome 6.0.472.63 on Vista SP1 / textarea
TEST_F(Win32RendererUtilTest, Chrome_textarea_Suggest) {
  const wchar_t kClassName[] = L"Chrome_RenderWidgetHostHWND";
  const CRect kWindowRect(153, 190, 891, 906);
  const CPoint kClientOffset(0, 0);
  const CSize kClientSize(738, 716);
  const double kScaleFactor = 1.0;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(
      CreateDefaultGUIFontEmulator(),
      CreateWindowEmulator(kClassName, kWindowRect, kClientOffset, kClientSize,
                           kScaleFactor, &hwnd));

  ApplicationInfo app_info;

  AppInfoUtil::SetBasicApplicationInfo(&app_info, hwnd,
                                       ApplicationInfo::ShowSuggestWindow);

  AppInfoUtil::SetCompositionFont(
      &app_info, 11, 0, 0, 0, FW_DONTCARE, SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS,
      CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, 0, "メイリオ");

  AppInfoUtil::SetCandidateForm(&app_info, CandidateForm::EXCLUDE, 84, 424, 84,
                                424, 85, 444);

  AppInfoUtil::SetCaretInfo(&app_info, false, 84, 444, 85, 445, hwnd);

  CandidateWindowLayout layout;
  EXPECT_TRUE(layout_mgr.LayoutCandidateWindowForSuggestion(app_info, &layout));
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(237, 614, 237, 614, 238, 634, layout);
}

// Chrome 6.0.472.63 on Vista SP1 / textarea
TEST_F(Win32RendererUtilTest, Chrome_textarea_Convert) {
  const wchar_t kClassName[] = L"Chrome_RenderWidgetHostHWND";
  const CRect kWindowRect(153, 190, 891, 906);
  const CPoint kClientOffset(0, 0);
  const CSize kClientSize(738, 716);
  const double kScaleFactor = 1.0;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(
      CreateDefaultGUIFontEmulator(),
      CreateWindowEmulator(kClassName, kWindowRect, kClientOffset, kClientSize,
                           kScaleFactor, &hwnd));

  ApplicationInfo app_info;

  AppInfoUtil::SetBasicApplicationInfo(&app_info, hwnd,
                                       ApplicationInfo::ShowCandidateWindow |
                                           ApplicationInfo::ShowSuggestWindow);

  AppInfoUtil::SetCompositionFont(
      &app_info, 11, 0, 0, 0, FW_DONTCARE, SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS,
      CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, 0, "メイリオ");

  AppInfoUtil::SetCandidateForm(&app_info, CandidateForm::EXCLUDE, 58, 424, 58,
                                424, 59, 444);

  AppInfoUtil::SetCaretInfo(&app_info, false, 58, 444, 59, 445, hwnd);

  CandidateWindowLayout layout;
  EXPECT_TRUE(layout_mgr.LayoutCandidateWindowForConversion(app_info, &layout));
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(211, 614, 211, 614, 212, 634, layout);
}

// Internet Explorer 8.0.6001.18943 on Vista SP1 / textarea
TEST_F(Win32RendererUtilTest, IE8_textarea_Suggest) {
  const wchar_t kClassName[] = L"Internet Explorer_Server";
  const CRect kWindowRect(304, 349, 1360, 1067);
  const CPoint kClientOffset(0, 0);
  const CSize kClientSize(1056, 718);
  const double kScaleFactor = 1.0;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(
      CreateDefaultGUIFontEmulator(),
      CreateWindowEmulator(kClassName, kWindowRect, kClientOffset, kClientSize,
                           kScaleFactor, &hwnd));

  ApplicationInfo app_info;

  AppInfoUtil::SetBasicApplicationInfo(&app_info, hwnd,
                                       ApplicationInfo::ShowSuggestWindow);

  AppInfoUtil::SetCandidateForm(&app_info, CandidateForm::EXCLUDE, 105, 376,
                                105, 356, 107, 376);

  AppInfoUtil::SetCaretInfo(&app_info, false, 105, 368, 106, 384, hwnd);

  CandidateWindowLayout layout;
  EXPECT_TRUE(layout_mgr.LayoutCandidateWindowForSuggestion(app_info, &layout));
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(409, 735, 409, 717, 410, 735, layout);
}

// Internet Explorer 8.0.6001.18943 on Vista SP1 / textarea
TEST_F(Win32RendererUtilTest, IE8_textarea_Convert) {
  const wchar_t kClassName[] = L"Internet Explorer_Server";
  const CRect kWindowRect(304, 349, 1360, 1067);
  const CPoint kClientOffset(0, 0);
  const CSize kClientSize(1056, 718);
  const double kScaleFactor = 1.0;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(
      CreateDefaultGUIFontEmulator(),
      CreateWindowEmulator(kClassName, kWindowRect, kClientOffset, kClientSize,
                           kScaleFactor, &hwnd));

  ApplicationInfo app_info;

  AppInfoUtil::SetBasicApplicationInfo(&app_info, hwnd,
                                       ApplicationInfo::ShowCandidateWindow |
                                           ApplicationInfo::ShowSuggestWindow);

  AppInfoUtil::SetCandidateForm(&app_info, CandidateForm::EXCLUDE, 91, 387, 91,
                                367, 93, 387);

  AppInfoUtil::SetCaretInfo(&app_info, false, 91, 379, 92, 380, hwnd);

  CandidateWindowLayout layout;
  EXPECT_TRUE(layout_mgr.LayoutCandidateWindowForConversion(app_info, &layout));
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(395, 736, 395, 716, 397, 736, layout);
}

// Fudemame 21.  See b/3067011.
// It provides no positional information for suggestion. See b/3067011.
TEST_F(Win32RendererUtilTest, Fudemame21_Suggest) {
  const wchar_t kClassName[] = L"MrnDirectEdit4";
  const CRect kWindowRect(507, 588, 1024, 698);
  const CPoint kClientOffset(0, 0);
  const CSize kClientSize(517, 110);
  const double kScaleFactor = 1.0;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(
      CreateDefaultGUIFontEmulator(),
      CreateWindowEmulator(kClassName, kWindowRect, kClientOffset, kClientSize,
                           kScaleFactor, &hwnd));

  ApplicationInfo app_info;

  AppInfoUtil::SetBasicApplicationInfo(&app_info, hwnd,
                                       ApplicationInfo::ShowSuggestWindow);

  AppInfoUtil::SetCaretInfo(&app_info, false, 0, 0, 0, 0, nullptr);

  CandidateWindowLayout layout;
  EXPECT_TRUE(layout_mgr.LayoutCandidateWindowForSuggestion(app_info, &layout));
  EXPECT_NON_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(507, 698, layout);
}

// Fudemame 21.  See b/3067011.
TEST_F(Win32RendererUtilTest, Fudemame19_Convert) {
  const wchar_t kClassName[] = L"MrnDirectEdit4";
  const CRect kWindowRect(507, 588, 1024, 698);
  const CPoint kClientOffset(0, 0);
  const CSize kClientSize(517, 110);
  const double kScaleFactor = 1.0;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(
      CreateDefaultGUIFontEmulator(),
      CreateWindowEmulator(kClassName, kWindowRect, kClientOffset, kClientSize,
                           kScaleFactor, &hwnd));

  ApplicationInfo app_info;

  AppInfoUtil::SetBasicApplicationInfo(&app_info, hwnd,
                                       ApplicationInfo::ShowCandidateWindow |
                                           ApplicationInfo::ShowSuggestWindow);

  AppInfoUtil::SetCandidateForm(&app_info, CandidateForm::CANDIDATEPOS, 87, 87,
                                0, 0, 0, 0);

  AppInfoUtil::SetCaretInfo(&app_info, false, 0, 0, 0, 0, nullptr);

  CandidateWindowLayout layout;
  EXPECT_TRUE(layout_mgr.LayoutCandidateWindowForConversion(app_info, &layout));
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(594, 675, 594, 657, 595, 675, layout);
}

// Opera 10.63 (build 3516) / Textarea
TEST_F(Win32RendererUtilTest, Opera10_Suggest) {
  const wchar_t kClassName[] = L"OperaWindowClass";
  const UINT kClassStyle = CS_DBLCLKS;
  const DWORD kWindowStyle = WS_CAPTION | WS_VISIBLE | WS_CLIPSIBLINGS |
                             WS_CLIPCHILDREN | WS_SYSMENU | WS_THICKFRAME |
                             WS_OVERLAPPED | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
  static_assert(kWindowStyle == 0x16cf0000, "Check actual value");
  const DWORD kWindowExStyle = WS_EX_LEFT | WS_EX_LTRREADING |
                               WS_EX_RIGHTSCROLLBAR | WS_EX_ACCEPTFILES |
                               WS_EX_WINDOWEDGE;
  static_assert(kWindowExStyle == 0x00000110, "Check actual value");

  const CRect kWindowRect(538, 229, 2114, 1271);
  const CPoint kClientOffset(8, 0);
  const CSize kClientSize(1560, 1034);
  const double kScaleFactor = 1.0;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(
      CreateDefaultGUIFontEmulator(),
      CreateWindowEmulator(kClassName, kWindowRect, kClientOffset, kClientSize,
                           kScaleFactor, &hwnd));

  ApplicationInfo app_info;

  AppInfoUtil::SetBasicApplicationInfo(&app_info, hwnd,
                                       ApplicationInfo::ShowSuggestWindow);

  AppInfoUtil::SetCandidateForm(&app_info, CandidateForm::EXCLUDE, 44, 444, 44,
                                444, 44, 459);

  AppInfoUtil::SetCaretInfo(&app_info, false, 44, 444, 667, 750, hwnd);

  CandidateWindowLayout layout;
  EXPECT_TRUE(layout_mgr.LayoutCandidateWindowForSuggestion(app_info, &layout));
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(590, 673, 590, 673, 590, 688, layout);
}

// Opera 10.63 (build 3516) / Textarea
TEST_F(Win32RendererUtilTest, Opera10_Convert) {
  const wchar_t kClassName[] = L"OperaWindowClass";
  const UINT kClassStyle = CS_DBLCLKS;
  const DWORD kWindowStyle = WS_CAPTION | WS_VISIBLE | WS_CLIPSIBLINGS |
                             WS_CLIPCHILDREN | WS_SYSMENU | WS_THICKFRAME |
                             WS_OVERLAPPED | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
  static_assert(kWindowStyle == 0x16cf0000, "Check actual value");
  const DWORD kWindowExStyle = WS_EX_LEFT | WS_EX_LTRREADING |
                               WS_EX_RIGHTSCROLLBAR | WS_EX_ACCEPTFILES |
                               WS_EX_WINDOWEDGE;
  static_assert(kWindowExStyle == 0x00000110, "Check actual value");

  const CRect kWindowRect(538, 229, 2114, 1271);
  const CPoint kClientOffset(8, 0);
  const CSize kClientSize(1560, 1034);
  const double kScaleFactor = 1.0;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(
      CreateDefaultGUIFontEmulator(),
      CreateWindowEmulator(kClassName, kWindowRect, kClientOffset, kClientSize,
                           kScaleFactor, &hwnd));

  ApplicationInfo app_info;

  AppInfoUtil::SetBasicApplicationInfo(&app_info, hwnd,
                                       ApplicationInfo::ShowCandidateWindow |
                                           ApplicationInfo::ShowSuggestWindow);

  AppInfoUtil::SetCandidateForm(&app_info, CandidateForm::EXCLUDE, 22, 444, 22,
                                444, 22, 459);

  AppInfoUtil::SetCaretInfo(&app_info, false, 22, 444, 645, 750, hwnd);

  CandidateWindowLayout layout;
  EXPECT_TRUE(layout_mgr.LayoutCandidateWindowForConversion(app_info, &layout));
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(568, 673, 568, 673, 568, 688, layout);
}

// NTEmacs22 / GNU Emacs 22.2.1
// Issue 5824433
TEST_F(Win32RendererUtilTest, Emacs22) {
  const wchar_t kClassName[] = L"Emacs";
  const UINT kClassStyle = CS_VREDRAW | CS_HREDRAW;
  const DWORD kWindowStyle = WS_CAPTION | WS_VISIBLE | WS_CLIPSIBLINGS |
                             WS_CLIPCHILDREN | WS_SYSMENU | WS_THICKFRAME |
                             WS_OVERLAPPED | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
  static_assert(kWindowStyle == 0x16cf0000, "Check actual value");
  const DWORD kWindowExStyle = WS_EX_LEFT | WS_EX_LTRREADING |
                               WS_EX_RIGHTSCROLLBAR | WS_EX_ACCEPTFILES |
                               WS_EX_OVERLAPPEDWINDOW;
  static_assert(kWindowExStyle == 0x00000310, "Check actual value");

  const CRect kWindowRect(175, 175, 797, 924);
  const CPoint kClientOffset(10, 53);
  const CSize kClientSize(602, 686);
  const double kScaleFactor = 1.0;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(
      CreateDefaultGUIFontEmulator(),
      CreateWindowEmulator(kClassName, kWindowRect, kClientOffset, kClientSize,
                           kScaleFactor, &hwnd));

  ApplicationInfo app_info;

  AppInfoUtil::SetBasicApplicationInfo(
      &app_info, hwnd,
      ApplicationInfo::ShowCompositionWindow |
          ApplicationInfo::ShowCandidateWindow |
          ApplicationInfo::ShowSuggestWindow);

  AppInfoUtil::SetCompositionFont(
      &app_info, -14, 0, 0, 0, FW_NORMAL, ANSI_CHARSET, OUT_DEFAULT_PRECIS,
      CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH, "Courier New");

  AppInfoUtil::SetCompositionForm(&app_info, CompositionForm::RECT, 66, 58, 10,
                                  42, 570, 658);

  AppInfoUtil::SetCaretInfo(&app_info, false, 66, 58, 67, 74, hwnd);

  CandidateWindowLayout layout;
  EXPECT_TRUE(layout_mgr.LayoutCandidateWindowForConversion(app_info, &layout));
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(251, 302, 251, 286, 252, 302, layout);

  // This application automatically and frequently generates
  // WM_IME_CONTROL/IMC_SETCOMPOSITIONWINDOW even when a user is not
  // typing. So we need to show InfoList without delay.  b/5824433.
  const int mode = layout_mgr.GetCompatibilityMode(app_info);
  EXPECT_EQ(SHOW_INFOLIST_IMMEDIATELY, mode & SHOW_INFOLIST_IMMEDIATELY);
}

// Meadow 3.0 / GNU Emacs 22.3.1
// Issue 5824433
TEST_F(Win32RendererUtilTest, Meadow3) {
  const wchar_t kClassName[] = L"MEADOW";
  const UINT kClassStyle = CS_VREDRAW | CS_HREDRAW;
  const DWORD kWindowStyle = WS_CAPTION | WS_VISIBLE | WS_CLIPSIBLINGS |
                             WS_CLIPCHILDREN | WS_SYSMENU | WS_THICKFRAME |
                             WS_OVERLAPPED | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
  static_assert(kWindowStyle == 0x16cf0000, "Check actual value");
  const DWORD kWindowExStyle = WS_EX_LEFT | WS_EX_LTRREADING |
                               WS_EX_RIGHTSCROLLBAR | WS_EX_ACCEPTFILES |
                               WS_EX_OVERLAPPEDWINDOW;
  static_assert(kWindowExStyle == 0x00000310, "Check actual value");

  const CRect kWindowRect(175, 175, 797, 928);
  const CPoint kClientOffset(10, 53);
  const CSize kClientSize(602, 690);
  const double kScaleFactor = 1.0;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(
      CreateDefaultGUIFontEmulator(),
      CreateWindowEmulator(kClassName, kWindowRect, kClientOffset, kClientSize,
                           kScaleFactor, &hwnd));

  ApplicationInfo app_info;

  AppInfoUtil::SetBasicApplicationInfo(
      &app_info, hwnd,
      ApplicationInfo::ShowCompositionWindow |
          ApplicationInfo::ShowCandidateWindow |
          ApplicationInfo::ShowSuggestWindow);

  AppInfoUtil::SetCompositionForm(&app_info, CompositionForm::RECT, 73, 65, 9,
                                  49, 577, 657);

  AppInfoUtil::SetCaretInfo(&app_info, false, 0, 0, 0, 0, hwnd);

  CandidateWindowLayout layout;
  EXPECT_TRUE(layout_mgr.LayoutCandidateWindowForConversion(app_info, &layout));
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(258, 311, 258, 293, 259, 311, layout);

  // This application automatically and frequently generates
  // WM_IME_CONTROL/IMC_SETCOMPOSITIONWINDOW even when a user is not
  // typing. So we need to show InfoList without delay.  b/5824433.
  const int mode = layout_mgr.GetCompatibilityMode(app_info);
  EXPECT_EQ(SHOW_INFOLIST_IMMEDIATELY, mode & SHOW_INFOLIST_IMMEDIATELY);
}

// Firefox 47.0a1 (2016-02-28)
TEST_F(Win32RendererUtilTest, Firefox_ExcludeRect_Suggest) {
  const wchar_t kClassName[] = L"MozillaWindowClass";
  const UINT kClassStyle = CS_DBLCLKS;
  const DWORD kWindowStyle =
      WS_POPUP | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
  static_assert(kWindowStyle == 0x96000000, "Check actual value");
  const DWORD kWindowExStyle =
      WS_EX_LEFT | WS_EX_LTRREADING | WS_EX_RIGHTSCROLLBAR;
  static_assert(kWindowExStyle == 0x00000000, "Check actual value");

  const CRect kWindowRect(58, 22, 1210, 622);
  const CPoint kClientOffset(6, 0);
  const CSize kClientSize(1140, 594);
  const double kScaleFactor = 1.0;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(
      CreateDefaultGUIFontEmulator(),
      CreateWindowEmulator(kClassName, kWindowRect, kClientOffset, kClientSize,
                           kScaleFactor, &hwnd));

  ApplicationInfo app_info;

  AppInfoUtil::SetBasicApplicationInfo(&app_info, hwnd,
                                       ApplicationInfo::ShowSuggestWindow);

  AppInfoUtil::SetCandidateForm(&app_info, CandidateForm::EXCLUDE, 22, 100, 22,
                                100, 37, 160);

  AppInfoUtil::SetCompositionTarget(&app_info, 0, 86, 122, 20, 83, 119, 109,
                                    525);

  AppInfoUtil::SetCaretInfo(&app_info, false, 35, 140, 36, 160, hwnd);

  CandidateWindowLayout layout;
  EXPECT_TRUE(layout_mgr.LayoutCandidateWindowForSuggestion(app_info, &layout));
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(86, 142, 86, 122, 101, 182, layout);
}

// Firefox 47.0a1 (2016-02-28)
TEST_F(Win32RendererUtilTest, Firefox_ExcludeRect_Convert) {
  const wchar_t kClassName[] = L"MozillaWindowClass";
  const UINT kClassStyle = CS_DBLCLKS;
  const DWORD kWindowStyle =
      WS_POPUP | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
  static_assert(kWindowStyle == 0x96000000, "Check actual value");
  const DWORD kWindowExStyle =
      WS_EX_LEFT | WS_EX_LTRREADING | WS_EX_RIGHTSCROLLBAR;
  static_assert(kWindowExStyle == 0x00000000, "Check actual value");

  const CRect kWindowRect(58, 22, 1210, 622);
  const CPoint kClientOffset(6, 0);
  const CSize kClientSize(1140, 594);
  const double kScaleFactor = 1.0;

  HWND hwnd = nullptr;
  LayoutManager layout_mgr(
      CreateDefaultGUIFontEmulator(),
      CreateWindowEmulator(kClassName, kWindowRect, kClientOffset, kClientSize,
                           kScaleFactor, &hwnd));

  ApplicationInfo app_info;

  AppInfoUtil::SetBasicApplicationInfo(&app_info, hwnd,
                                       ApplicationInfo::ShowCandidateWindow |
                                           ApplicationInfo::ShowSuggestWindow);

  AppInfoUtil::SetCandidateForm(&app_info, CandidateForm::EXCLUDE, 22, 100, 22,
                                100, 37, 160);

  AppInfoUtil::SetCompositionTarget(&app_info, 0, 86, 122, 20, 83, 119, 109,
                                    525);

  AppInfoUtil::SetCaretInfo(&app_info, false, 35, 140, 36, 160, hwnd);

  CandidateWindowLayout layout;
  EXPECT_TRUE(layout_mgr.LayoutCandidateWindowForConversion(app_info, &layout));
  EXPECT_EXCLUDE_CANDIDATE_WINDOW_LAYOUT(86, 142, 86, 122, 101, 182, layout);
}

}  // namespace win32
}  // namespace renderer
}  // namespace mozc
