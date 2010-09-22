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
#include "base/util.h"
#include "converter/converter_interface.h"
#include "converter/converter_mock.h"
#include "composer/composer.h"
#include "composer/table.h"
#include "session/internal/keymap.h"
#include "session/config.pb.h"
#include "session/config_handler.h"
#include "session/session_converter.h"
#include "testing/base/public/gunit.h"
#include "testing/base/public/googletest.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace session {

static const char kChars_Mo[] = "\xE3\x82\x82";
static const char kChars_Mozuku[] = "\xE3\x82\x82\xE3\x81\x9A\xE3\x81\x8F";
static const char kChars_Mozukusu[] =
  "\xE3\x82\x82\xE3\x81\x9A\xE3\x81\x8F\xE3\x81\x99";
static const char kChars_Momonga[] =
  "\xE3\x82\x82\xE3\x82\x82\xE3\x82\x93\xE3\x81\x8C";

class SessionConverterTest : public testing::Test {
 protected:
  virtual void SetUp() {
    convertermock_.reset(new ConverterMock());
    ConverterFactory::SetConverter(convertermock_.get());
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);

    table_.reset(new composer::Table);
    composer_.reset(composer::Composer::Create(table_.get()));

    // "あいうえお"
    aiueo_ = "\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86\xe3\x81\x88\xe3\x81\x8a";
  }

  virtual void TearDown() {
    table_.reset();
    composer_.reset();
  }

  // set result for "あいうえお"
  void SetAiueo(Segments *segments) {
    segments->clear();
    Segment *segment;
    Segment::Candidate *candidate;

    segment = segments->add_segment();
    // "あいうえお"
    segment->set_key(
        "\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86\xe3\x81\x88\xe3\x81\x8a");
    candidate = segment->add_candidate();
    // "あいうえお"
    candidate->value =
      "\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86\xe3\x81\x88\xe3\x81\x8a";
    candidate = segment->add_candidate();
    // "アイウエオ"
    candidate->value =
      "\xe3\x82\xa2\xe3\x82\xa4\xe3\x82\xa6\xe3\x82\xa8\xe3\x82\xaa";
  }

  // set result for "かまぼこのいんぼう"
  void SetKamaboko(Segments *segments) {
    Segment *segment;
    Segment::Candidate *candidate;

    segments->clear();
    segment = segments->add_segment();

    // "かまぼこの"
    segment->set_key(
        "\xe3\x81\x8b\xe3\x81\xbe\xe3\x81\xbc\xe3\x81\x93\xe3\x81\xae");
    candidate = segment->add_candidate();
    // "かまぼこの"
    candidate->value =
      "\xe3\x81\x8b\xe3\x81\xbe\xe3\x81\xbc\xe3\x81\x93\xe3\x81\xae";
    candidate = segment->add_candidate();
    // "カマボコの"
    candidate->value =
      "\xe3\x82\xab\xe3\x83\x9e\xe3\x83\x9c\xe3\x82\xb3\xe3\x81\xae";
    segment = segments->add_segment();
    // "いんぼう"
    segment->set_key("\xe3\x81\x84\xe3\x82\x93\xe3\x81\xbc\xe3\x81\x86");
    candidate = segment->add_candidate();
    // "陰謀"
    candidate->value = "\xe9\x99\xb0\xe8\xac\x80";
    candidate = segment->add_candidate();
    // "印房"
    candidate->value = "\xe5\x8d\xb0\xe6\x88\xbf";
  }

  // set result for "like"
  void InitConverterWithLike(Segments *segments) {
    // "ぃ"
    composer_->InsertCharacterKeyAndPreedit("li", "\xE3\x81\x83");
    // "け"
    composer_->InsertCharacterKeyAndPreedit("ke", "\xE3\x81\x91");

    Segment *segment;
    Segment::Candidate *candidate;

    segments->clear();
    segment = segments->add_segment();

    // "ぃ"
    segment->set_key("\xE3\x81\x83");
    candidate = segment->add_candidate();
    // "ぃ"
    candidate->value = "\xE3\x81\x83";

    candidate = segment->add_candidate();
    // "ィ"
    candidate->value = "\xE3\x82\xA3";

    segment = segments->add_segment();
    // "け"
    segment->set_key("\xE3\x81\x91");
    candidate = segment->add_candidate();
    // "家"
    candidate->value = "\xE5\xAE\xB6";
    candidate = segment->add_candidate();
    // "け"
    candidate->value = "\xE3\x81\x91";

    convertermock_->SetStartConversion(segments, true);
  }

  scoped_ptr<ConverterMock> convertermock_;

  scoped_ptr<composer::Composer> composer_;
  scoped_ptr<composer::Table> table_;

  string aiueo_;
};

TEST_F(SessionConverterTest, Convert) {
  SessionConverter converter(convertermock_.get());
  Segments segments;
  SetAiueo(&segments);
  convertermock_->SetStartConversion(&segments, true);

  composer_->InsertCharacterPreedit(aiueo_);
  EXPECT_TRUE(converter.Convert(composer_.get()));
  ASSERT_TRUE(converter.IsActive());

  commands::Output output;
  converter.FillOutput(&output);
  EXPECT_FALSE(output.has_result());
  EXPECT_TRUE(output.has_preedit());
  EXPECT_FALSE(output.has_candidates());

  const commands::Preedit &conversion = output.preedit();
  EXPECT_EQ(1, conversion.segment_size());
  EXPECT_EQ(commands::Preedit::Segment::HIGHLIGHT,
            conversion.segment(0).annotation());
  EXPECT_EQ(aiueo_, conversion.segment(0).value());
  EXPECT_EQ(aiueo_, conversion.segment(0).key());

  // Converter should be active before submittion
  EXPECT_TRUE(converter.IsActive());
  EXPECT_FALSE(converter.IsCandidateListVisible());

  converter.Commit();
  output.Clear();
  converter.FillOutput(&output);
  EXPECT_TRUE(output.has_result());
  EXPECT_FALSE(output.has_preedit());
  EXPECT_FALSE(output.has_candidates());

  const commands::Result &result = output.result();
  EXPECT_EQ(aiueo_, result.value());
  EXPECT_EQ(aiueo_, result.key());

  // Converter should be inactive after submittion
  EXPECT_FALSE(converter.IsActive());
  EXPECT_FALSE(converter.IsCandidateListVisible());
}

TEST_F(SessionConverterTest, ConvertToTransliteration) {
  SessionConverter converter(convertermock_.get());
  Segments segments;
  SetAiueo(&segments);
  convertermock_->SetStartConversion(&segments, true);

  composer_->InsertCharacterKeyAndPreedit("aiueo", aiueo_);
  EXPECT_TRUE(converter.ConvertToTransliteration(composer_.get(),
                                                 transliteration::HALF_ASCII));
  {  // Check the conversion #1
    commands::Output output;
    converter.FillOutput(&output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(1, conversion.segment_size());
    EXPECT_EQ("aiueo", conversion.segment(0).value());
    EXPECT_FALSE(converter.IsCandidateListVisible());
  }

  EXPECT_TRUE(converter.ConvertToTransliteration(composer_.get(),
                                                 transliteration::HALF_ASCII));
  {  // Check the conversion #2
    commands::Output output;
    converter.FillOutput(&output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(1, conversion.segment_size());
    EXPECT_EQ("AIUEO", conversion.segment(0).value());
    EXPECT_FALSE(converter.IsCandidateListVisible());
  }

  EXPECT_TRUE(converter.ConvertToTransliteration(composer_.get(),
                                                 transliteration::FULL_ASCII));
  {  // Check the conversion #3
    commands::Output output;
    converter.FillOutput(&output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(1, conversion.segment_size());
    EXPECT_EQ("\xEF\xBC\xA1\xEF\xBC\xA9\xEF\xBC\xB5\xEF\xBC\xA5\xEF\xBC\xAF",
              conversion.segment(0).value());
    EXPECT_FALSE(converter.IsCandidateListVisible());
  }
}

TEST_F(SessionConverterTest, ConvertToTransliterationWithMultipleSegments) {
  Segments segments;
  InitConverterWithLike(&segments);
  SessionConverter converter(convertermock_.get());

  // Convert
  EXPECT_TRUE(converter.Convert(composer_.get()));
  {  // Check the conversion #1
    commands::Output output;
    converter.FillOutput(&output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(2, conversion.segment_size());
    // "ぃ"
    EXPECT_EQ("\xE3\x81\x83", conversion.segment(0).value());
    // "家"
    EXPECT_EQ("\xE5\xAE\xB6", conversion.segment(1).value());
    EXPECT_FALSE(converter.IsCandidateListVisible());
  }

  // Convert to half-width alphanumeric.
  EXPECT_TRUE(converter.ConvertToTransliteration(composer_.get(),
                                                 transliteration::HALF_ASCII));
  {  // Check the conversion #2
    commands::Output output;
    converter.FillOutput(&output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(2, conversion.segment_size());
    EXPECT_EQ("li", conversion.segment(0).value());
    EXPECT_FALSE(converter.IsCandidateListVisible());
  }
}

TEST_F(SessionConverterTest, ConvertToTransliterationWithoutCascadigWindow) {
  SessionConverter converter(convertermock_.get());
  Segments segments;
  {
    Segment *segment;
    Segment::Candidate *candidate;
    segment = segments.add_segment();
    segment->set_key("dvd");
    candidate = segment->add_candidate();
    candidate->value = "dvd";
    candidate = segment->add_candidate();
    candidate->value = "DVD";
  }
  convertermock_->SetStartConversion(&segments, true);

  {  // Set config
    config::Config config;
    config::ConfigHandler::GetConfig(&config);
    config.set_use_cascading_window(false);
    config::ConfigHandler::SetConfig(config);
    converter.ReloadConfig();
  }

  // "ｄｖｄ"
  composer_->InsertCharacterKeyAndPreedit("dvd",
                                     "\xEF\xBD\x84\xEF\xBD\x96\xEF\xBD\x84");
  EXPECT_TRUE(converter.ConvertToTransliteration(composer_.get(),
                                                 transliteration::FULL_ASCII));
  {  // Check the conversion #1
    commands::Output output;
    converter.FillOutput(&output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(1, conversion.segment_size());
    // "ｄｖｄ"
    EXPECT_EQ("\xEF\xBD\x84\xEF\xBD\x96\xEF\xBD\x84",
              conversion.segment(0).value());
    EXPECT_FALSE(converter.IsCandidateListVisible());
  }

  EXPECT_TRUE(converter.ConvertToTransliteration(composer_.get(),
                                                 transliteration::FULL_ASCII));
  {  // Check the conversion #2
    commands::Output output;
    converter.FillOutput(&output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(1, conversion.segment_size());
    // "ＤＶＤ"
    EXPECT_EQ("\xEF\xBC\xA4\xEF\xBC\xB6\xEF\xBC\xA4",
              conversion.segment(0).value());
    EXPECT_FALSE(converter.IsCandidateListVisible());
  }

  EXPECT_TRUE(converter.ConvertToTransliteration(composer_.get(),
                                                 transliteration::FULL_ASCII));
  {  // Check the conversion #3
    commands::Output output;
    converter.FillOutput(&output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(1, conversion.segment_size());
    // "Ｄｖｄ"
    EXPECT_EQ("\xEF\xBC\xA4\xEF\xBD\x96\xEF\xBD\x84",
              conversion.segment(0).value());
    EXPECT_FALSE(converter.IsCandidateListVisible());
  }
}

TEST_F(SessionConverterTest, MultiSegmentsConversion) {
  SessionConverter converter(convertermock_.get());
  Segments segments;
  SetKamaboko(&segments);
  convertermock_->SetStartConversion(&segments, true);

  const string kKamabokono =
    "\xe3\x81\x8b\xe3\x81\xbe\xe3\x81\xbc\xe3\x81\x93\xe3\x81\xae";
  const string kInbou =
    "\xe3\x81\x84\xe3\x82\x93\xe3\x81\xbc\xe3\x81\x86";

  // "かまぼこのいんぼう"
  composer_->InsertCharacterPreedit(kKamabokono + kInbou);
  EXPECT_TRUE(converter.Convert(composer_.get()));

  // Test for conversion
  {
    commands::Output output;
    converter.FillOutput(&output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(2, conversion.segment_size());
    EXPECT_EQ(commands::Preedit::Segment::HIGHLIGHT,
              conversion.segment(0).annotation());
    EXPECT_EQ(kKamabokono, conversion.segment(0).key());
    EXPECT_EQ(kKamabokono, conversion.segment(0).value());

    EXPECT_EQ(commands::Preedit::Segment::UNDERLINE,
              conversion.segment(1).annotation());
    EXPECT_EQ(kInbou, conversion.segment(1).key());
    // "陰謀"
    EXPECT_EQ("\xe9\x99\xb0\xe8\xac\x80", conversion.segment(1).value());
  }

  EXPECT_FALSE(converter.IsCandidateListVisible());
  converter.CandidateNext();
  EXPECT_TRUE(converter.IsCandidateListVisible());
  converter.CandidatePrev();
  EXPECT_TRUE(converter.IsCandidateListVisible());

  // Test for candidates
  {
    commands::Output output;
    converter.FillOutput(&output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    const commands::Candidates &candidates = output.candidates();
    EXPECT_EQ(0, candidates.focused_index());
    EXPECT_EQ(3, candidates.size());  // two candidates + one t13n sub list.
    EXPECT_EQ(0, candidates.position());
    EXPECT_EQ(kKamabokono, candidates.candidate(0).value());
    // "カマボコの"
    EXPECT_EQ("\xe3\x82\xab\xe3\x83\x9e\xe3\x83\x9c\xe3\x82\xb3\xe3\x81\xae",
              candidates.candidate(1).value());

    // "そのほかの文字種";
    EXPECT_EQ("\xe3\x81\x9d\xe3\x81\xae\xe3\x81\xbb\xe3\x81\x8b\xe3\x81\xae"
              "\xe6\x96\x87\xe5\xad\x97\xe7\xa8\xae",
              candidates.candidate(2).value());
  }

  // Test for segment motion. [SegmentFocusRight]
  {
    converter.SegmentFocusRight();
    EXPECT_FALSE(converter.IsCandidateListVisible());
    converter.SetCandidateListVisible(true);

    commands::Output output;
    converter.FillOutput(&output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    const commands::Candidates &candidates = output.candidates();
    EXPECT_EQ(0, candidates.focused_index());
    EXPECT_EQ(3, candidates.size());  // two candidates + one t13n sub list.
    EXPECT_EQ(5, candidates.position());
    // "陰謀"
    EXPECT_EQ("\xe9\x99\xb0\xe8\xac\x80",
              candidates.candidate(0).value());
    // "印房"
    EXPECT_EQ("\xe5\x8d\xb0\xe6\x88\xbf",
              candidates.candidate(1).value());

    // "そのほかの文字種";
    EXPECT_EQ("\xe3\x81\x9d\xe3\x81\xae\xe3\x81\xbb\xe3\x81\x8b\xe3\x81\xae"
              "\xe6\x96\x87\xe5\xad\x97\xe7\xa8\xae",
              candidates.candidate(2).value());
  }

  // Test for candidate motion. [CandidateNext]
  {
    converter.CandidateNext();
    EXPECT_TRUE(converter.IsCandidateListVisible());
    commands::Output output;
    converter.FillOutput(&output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    const commands::Candidates &candidates = output.candidates();
    EXPECT_EQ(1, candidates.focused_index());
    EXPECT_EQ(3, candidates.size());  // two candidates + one t13n sub list.
    EXPECT_EQ(5, candidates.position());
    // "陰謀"
    EXPECT_EQ("\xe9\x99\xb0\xe8\xac\x80", candidates.candidate(0).value());
    // "印房"
    EXPECT_EQ("\xe5\x8d\xb0\xe6\x88\xbf", candidates.candidate(1).value());

    // "そのほかの文字種";
    EXPECT_EQ("\xe3\x81\x9d\xe3\x81\xae\xe3\x81\xbb\xe3\x81\x8b\xe3\x81\xae"
              "\xe6\x96\x87\xe5\xad\x97\xe7\xa8\xae",
              candidates.candidate(2).value());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(kKamabokono, conversion.segment(0).value());
    // "印房"
    EXPECT_EQ("\xe5\x8d\xb0\xe6\x88\xbf", conversion.segment(1).value());
  }

  // Test for segment motion again [SegmentFocusLeftEdge] [SegmentFocusLast]
  // The positions of "陰謀" and "印房" should be swapped.
  {
    Segments fixed_segments;
    SetKamaboko(&fixed_segments);

    // "陰謀"
    ASSERT_EQ("\xe9\x99\xb0\xe8\xac\x80",
              fixed_segments.segment(1).candidate(0).value);
    // "印房"
    ASSERT_EQ("\xe5\x8d\xb0\xe6\x88\xbf",
              fixed_segments.segment(1).candidate(1).value);
    // swap the values.
    fixed_segments.mutable_segment(1)->mutable_candidate(0)->value.swap(
        fixed_segments.mutable_segment(1)->mutable_candidate(1)->value);
    // "印房"
    ASSERT_EQ("\xe5\x8d\xb0\xe6\x88\xbf",
              fixed_segments.segment(1).candidate(0).value);
    // "陰謀"
    ASSERT_EQ("\xe9\x99\xb0\xe8\xac\x80",
              fixed_segments.segment(1).candidate(1).value);
    convertermock_->SetCommitSegmentValue(&fixed_segments, true);

    converter.SegmentFocusLeftEdge();
    EXPECT_FALSE(converter.IsCandidateListVisible());

    converter.SegmentFocusLast();
    EXPECT_FALSE(converter.IsCandidateListVisible());
    converter.SetCandidateListVisible(true);

    commands::Output output;
    converter.FillOutput(&output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    const commands::Candidates &candidates = output.candidates();
    EXPECT_EQ(0, candidates.focused_index());
    EXPECT_EQ(3, candidates.size());  // two candidates + one t13n sub list.
    EXPECT_EQ(5, candidates.position());
    // "印房"
    EXPECT_EQ("\xe5\x8d\xb0\xe6\x88\xbf", candidates.candidate(0).value());

    // "陰謀"
    EXPECT_EQ("\xe9\x99\xb0\xe8\xac\x80", candidates.candidate(1).value());

    // "そのほかの文字種";
    EXPECT_EQ("\xe3\x81\x9d\xe3\x81\xae\xe3\x81\xbb\xe3\x81\x8b\xe3\x81\xae"
              "\xe6\x96\x87\xe5\xad\x97\xe7\xa8\xae",
              candidates.candidate(2).value());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(kKamabokono, conversion.segment(0).value());
    // "印房"
    EXPECT_EQ("\xe5\x8d\xb0\xe6\x88\xbf", conversion.segment(1).value());
  }

  // Check if GetDefaultResult returns the original conversion.
  // "かまぼこの陰謀"
  EXPECT_EQ("\xe3\x81\x8b\xe3\x81\xbe\xe3\x81\xbc\xe3\x81\x93\xe3\x81\xae"
            "\xe9\x99\xb0\xe8\xac\x80",
            converter.GetDefaultResult());

  {
    converter.Commit();
    EXPECT_FALSE(converter.IsCandidateListVisible());

    commands::Output output;
    converter.FillOutput(&output);
    EXPECT_TRUE(output.has_result());
    EXPECT_FALSE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Result &result = output.result();
    // "かまぼこの印房"
    EXPECT_EQ("\xe3\x81\x8b\xe3\x81\xbe\xe3\x81\xbc\xe3\x81\x93\xe3\x81\xae"
              "\xe5\x8d\xb0\xe6\x88\xbf",
              result.value());
    // "かまぼこのいんぼう"
    EXPECT_EQ("\xe3\x81\x8b\xe3\x81\xbe\xe3\x81\xbc\xe3\x81\x93\xe3\x81\xae"
              "\xe3\x81\x84\xe3\x82\x93\xe3\x81\xbc\xe3\x81\x86",
              result.key());
    EXPECT_FALSE(converter.IsActive());

    // Check if GetDefaultResult returns an empty string.
    EXPECT_TRUE(converter.GetDefaultResult().empty());
  }
}

TEST_F(SessionConverterTest, Transliterations) {
  SessionConverter converter(convertermock_.get());
  // "く"
  composer_->InsertCharacterKeyAndPreedit("h", "\xE3\x81\x8F");
  // "ま"
  composer_->InsertCharacterKeyAndPreedit("J", "\xE3\x81\xBE");

  Segments segments;
  {  // Initialize segments.
    Segment *segment = segments.add_segment();
    // "くま"
    segment->set_key("\xE3\x81\x8F\xE3\x81\xBE");
    // "クマー"
    segment->add_candidate()->value = "\xE3\x82\xAF\xE3\x83\x9E\xE3\x83\xBC";
  }
  convertermock_->SetStartConversion(&segments, true);
  EXPECT_TRUE(converter.Convert(composer_.get()));
  EXPECT_FALSE(converter.IsCandidateListVisible());

  // Move to the t13n list.
  converter.CandidateNext();
  EXPECT_TRUE(converter.IsCandidateListVisible());

  commands::Output output;
  converter.FillOutput(&output);
  EXPECT_FALSE(output.has_result());
  EXPECT_TRUE(output.has_preedit());
  EXPECT_TRUE(output.has_candidates());

  const commands::Candidates &candidates = output.candidates();
  EXPECT_EQ(2, candidates.size());  // one candidate + one t13n sub list.
  EXPECT_EQ(1, candidates.focused_index());
  // "そのほかの文字種";
  EXPECT_EQ("\xe3\x81\x9d\xe3\x81\xae\xe3\x81\xbb\xe3\x81\x8b\xe3\x81\xae"
            "\xe6\x96\x87\xe5\xad\x97\xe7\xa8\xae",
            candidates.candidate(1).value());

  vector<string> t13ns;
  composer_->GetTransliterations(&t13ns);

  EXPECT_TRUE(candidates.has_subcandidates());
  EXPECT_EQ(t13ns.size(), candidates.subcandidates().size());
  EXPECT_EQ(9, candidates.subcandidates().candidate_size());

  for (size_t i = 0; i < candidates.subcandidates().candidate_size(); ++i) {
    EXPECT_EQ(t13ns[i], candidates.subcandidates().candidate(i).value());
  }
}

TEST_F(SessionConverterTest, NormalizedTransliterations) {
  SessionConverter converter(convertermock_.get());
  // "らゔ"
  composer_->InsertCharacterPreedit("\xE3\x82\x89\xE3\x82\x94");

  Segments segments;
  {  // Initialize segments.
    Segment *segment = segments.add_segment();
    segment->set_key("LOVE");
    segment->add_candidate()->value = "LOVE";
  }
  convertermock_->SetStartConversion(&segments, true);
  EXPECT_TRUE(converter.ConvertToTransliteration(composer_.get(),
                                                 transliteration::HIRAGANA));
  EXPECT_FALSE(converter.IsCandidateListVisible());

  //  converter.CandidateNext();

  commands::Output output;
  converter.FillOutput(&output);
  EXPECT_TRUE(output.has_preedit());
  EXPECT_EQ(1, output.preedit().segment_size());
  LOG(INFO) << output.DebugString();
  // "らヴ"
  EXPECT_EQ("\xE3\x82\x89\xE3\x83\xB4", output.preedit().segment(0).value());
}

TEST_F(SessionConverterTest, ConvertToHalfWidth) {
  SessionConverter converter(convertermock_.get());
  // "あ"
  composer_->InsertCharacterKeyAndPreedit("a", "\xE3\x81\x82");
  // "ｂ"
  composer_->InsertCharacterKeyAndPreedit("b", "\xEF\xBD\x82");
  // "ｃ"
  composer_->InsertCharacterKeyAndPreedit("c", "\xEF\xBD\x83");

  Segments segments;
  {  // Initialize segments.
    Segment *segment = segments.add_segment();
    // "あｂｃ"
    segment->set_key("\xE3\x81\x82\xEF\xBD\x82\xEF\xBD\x83");
    // "あべし"
    segment->add_candidate()->value = "\xE3\x81\x82\xE3\x81\xB9\xE3\x81\x97";
  }
  convertermock_->SetStartConversion(&segments, true);
  EXPECT_TRUE(converter.ConvertToHalfWidth(composer_.get()));
  EXPECT_FALSE(converter.IsCandidateListVisible());

  {  // Make sure the output
    commands::Output output;
    converter.FillOutput(&output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(1, conversion.segment_size());
    // "ｱbc"
    EXPECT_EQ("\xEF\xBD\xB1\x62\x63", conversion.segment(0).value());
  }

  // Composition will be transliterated to "ａｂｃ".
  EXPECT_TRUE(converter.ConvertToTransliteration(composer_.get(),
                                                 transliteration::FULL_ASCII));
  {  // Make sure the output
    commands::Output output;
    converter.FillOutput(&output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(1, conversion.segment_size());
    // "ａｂｃ"
    EXPECT_EQ("\xEF\xBD\x81\xEF\xBD\x82\xEF\xBD\x83",
              conversion.segment(0).value());
  }

  EXPECT_TRUE(converter.ConvertToHalfWidth(composer_.get()));
  EXPECT_FALSE(converter.IsCandidateListVisible());
  {  // Make sure the output
    commands::Output output;
    converter.FillOutput(&output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(1, conversion.segment_size());
    // "abc"
    EXPECT_EQ("abc", conversion.segment(0).value());
  }
}

TEST_F(SessionConverterTest, SwitchKanaType) {
  {  // From composition mode.
    SessionConverter converter(convertermock_.get());
    // "あ"
    composer_->InsertCharacterKeyAndPreedit("a", "\xE3\x81\x82");
    // "ｂ"
    composer_->InsertCharacterKeyAndPreedit("b", "\xEF\xBD\x82");
    // "ｃ"
    composer_->InsertCharacterKeyAndPreedit("c", "\xEF\xBD\x83");

    Segments segments;
    {  // Initialize segments.
      Segment *segment = segments.add_segment();
      // "あｂｃ"
      segment->set_key("\xE3\x81\x82\xEF\xBD\x82\xEF\xBD\x83");
      // "あべし"
      segment->add_candidate()->value = "\xE3\x81\x82\xE3\x81\xB9\xE3\x81\x97";
    }
    convertermock_->SetStartConversion(&segments, true);
    EXPECT_TRUE(converter.SwitchKanaType(composer_.get()));
    EXPECT_FALSE(converter.IsCandidateListVisible());

    {  // Make sure the output
      commands::Output output;
      converter.FillOutput(&output);
      EXPECT_FALSE(output.has_result());
      EXPECT_TRUE(output.has_preedit());
      EXPECT_FALSE(output.has_candidates());

      const commands::Preedit &conversion = output.preedit();
      EXPECT_EQ(1, conversion.segment_size());
      // "アｂｃ"
      EXPECT_EQ("\xE3\x82\xA2\xEF\xBD\x82\xEF\xBD\x83",
                conversion.segment(0).value());
    }

    EXPECT_TRUE(converter.SwitchKanaType(composer_.get()));
    EXPECT_FALSE(converter.IsCandidateListVisible());
    {
      commands::Output output;
      converter.FillOutput(&output);
      EXPECT_FALSE(output.has_result());
      EXPECT_TRUE(output.has_preedit());
      EXPECT_FALSE(output.has_candidates());

      const commands::Preedit &conversion = output.preedit();
      EXPECT_EQ(1, conversion.segment_size());
      // "ｱbc"
      EXPECT_EQ("\xEF\xBD\xB1\x62\x63", conversion.segment(0).value());
    }

    EXPECT_TRUE(converter.SwitchKanaType(composer_.get()));
    EXPECT_FALSE(converter.IsCandidateListVisible());
    {
      commands::Output output;
      converter.FillOutput(&output);
      EXPECT_FALSE(output.has_result());
      EXPECT_TRUE(output.has_preedit());
      EXPECT_FALSE(output.has_candidates());

      const commands::Preedit &conversion = output.preedit();
      EXPECT_EQ(1, conversion.segment_size());
      // "あｂｃ"
      EXPECT_EQ("\xE3\x81\x82\xEF\xBD\x82\xEF\xBD\x83",
                conversion.segment(0).value());
    }
  }

  {  // From conversion mode
    SessionConverter converter(convertermock_.get());
    composer_->EditErase();
    // "か"
    composer_->InsertCharacterKeyAndPreedit("ka", "\xE3\x81\x8B");
    // "ん"
    composer_->InsertCharacterKeyAndPreedit("n", "\xE3\x82\x93");
    // "じ"
    composer_->InsertCharacterKeyAndPreedit("ji", "\xE3\x81\x98");

    Segments segments;
    {  // Initialize segments.
      Segment *segment = segments.add_segment();
      // "かんじ"
      segment->set_key("\xE3\x81\x8B\xE3\x82\x93\xE3\x81\x98");
      // "漢字"
      segment->add_candidate()->value = "\xE6\xBC\xA2\xE5\xAD\x97";
    }
    convertermock_->SetStartConversion(&segments, true);
    EXPECT_TRUE(converter.Convert(composer_.get()));
    EXPECT_FALSE(converter.IsCandidateListVisible());

    {  // Make sure the output
      commands::Output output;
      converter.FillOutput(&output);
      EXPECT_FALSE(output.has_result());
      EXPECT_TRUE(output.has_preedit());
      EXPECT_FALSE(output.has_candidates());

      const commands::Preedit &conversion = output.preedit();
      EXPECT_EQ(1, conversion.segment_size());
      // "漢字"
      EXPECT_EQ("\xE6\xBC\xA2\xE5\xAD\x97",
                conversion.segment(0).value());
    }

    EXPECT_TRUE(converter.SwitchKanaType(composer_.get()));
    EXPECT_FALSE(converter.IsCandidateListVisible());
    {
      commands::Output output;
      converter.FillOutput(&output);
      EXPECT_FALSE(output.has_result());
      EXPECT_TRUE(output.has_preedit());
      EXPECT_FALSE(output.has_candidates());

      const commands::Preedit &conversion = output.preedit();
      EXPECT_EQ(1, conversion.segment_size());
      // "かんじ"
      EXPECT_EQ("\xE3\x81\x8B\xE3\x82\x93\xE3\x81\x98",
                conversion.segment(0).value());
    }

    EXPECT_TRUE(converter.SwitchKanaType(composer_.get()));
    EXPECT_FALSE(converter.IsCandidateListVisible());
    {
      commands::Output output;
      converter.FillOutput(&output);
      EXPECT_FALSE(output.has_result());
      EXPECT_TRUE(output.has_preedit());
      EXPECT_FALSE(output.has_candidates());

      const commands::Preedit &conversion = output.preedit();
      EXPECT_EQ(1, conversion.segment_size());
      // "カンジ"
      EXPECT_EQ("\xE3\x82\xAB\xE3\x83\xB3\xE3\x82\xB8",
                conversion.segment(0).value());
    }

    EXPECT_TRUE(converter.SwitchKanaType(composer_.get()));
    EXPECT_FALSE(converter.IsCandidateListVisible());
    {
      commands::Output output;
      converter.FillOutput(&output);
      EXPECT_FALSE(output.has_result());
      EXPECT_TRUE(output.has_preedit());
      EXPECT_FALSE(output.has_candidates());

      const commands::Preedit &conversion = output.preedit();
      EXPECT_EQ(1, conversion.segment_size());
      // "ｶﾝｼﾞ"
      EXPECT_EQ("\xEF\xBD\xB6\xEF\xBE\x9D\xEF\xBD\xBC\xEF\xBE\x9E",
                conversion.segment(0).value());
    }

    EXPECT_TRUE(converter.SwitchKanaType(composer_.get()));
    EXPECT_FALSE(converter.IsCandidateListVisible());
    {
      commands::Output output;
      converter.FillOutput(&output);
      EXPECT_FALSE(output.has_result());
      EXPECT_TRUE(output.has_preedit());
      EXPECT_FALSE(output.has_candidates());

      const commands::Preedit &conversion = output.preedit();
      EXPECT_EQ(1, conversion.segment_size());
      // "かんじ"
      EXPECT_EQ("\xE3\x81\x8B\xE3\x82\x93\xE3\x81\x98",
                conversion.segment(0).value());
    }
  }
}

TEST_F(SessionConverterTest, CommitFirstSegment) {
  SessionConverter converter(convertermock_.get());
  Segments segments;
  SetKamaboko(&segments);
  convertermock_->SetStartConversion(&segments, true);

  const string kKamabokono =
    "\xe3\x81\x8b\xe3\x81\xbe\xe3\x81\xbc\xe3\x81\x93\xe3\x81\xae";
  const string kInbou =
    "\xe3\x81\x84\xe3\x82\x93\xe3\x81\xbc\xe3\x81\x86";

  // "かまぼこのいんぼう"
  composer_->InsertCharacterPreedit(kKamabokono + kInbou);
  EXPECT_TRUE(converter.Convert(composer_.get()));
  EXPECT_FALSE(converter.IsCandidateListVisible());

  {  // Check the conversion before CommitFirstSegment.
    commands::Output output;
    converter.FillOutput(&output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    // "かまぼこの"
    EXPECT_EQ(kKamabokono, conversion.segment(0).value());
    // "陰謀"
    EXPECT_EQ("\xe9\x99\xb0\xe8\xac\x80", conversion.segment(1).value());
  }

  {  // Initialization of SetSubmitFirstSegment.
    Segments segments_after_submit;
    Segment *segment = segments_after_submit.add_segment();
    // "いんぼう"
    segment->set_key("\xe3\x81\x84\xe3\x82\x93\xe3\x81\xbc\xe3\x81\x86");
    segment->add_candidate()->value = "\xe9\x99\xb0\xe8\xac\x80";  // "陰謀"
    segment->add_candidate()->value = "\xe5\x8d\xb0\xe6\x88\xbf";  // "印房"
    convertermock_->SetSubmitFirstSegment(&segments_after_submit, true);
  }
  converter.CommitFirstSegment(composer_.get());
  EXPECT_FALSE(converter.IsCandidateListVisible());

  {  // Check the conversion after CommitFirstSegment.
    commands::Output output;
    converter.FillOutput(&output);
    EXPECT_TRUE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    // "陰謀"
    EXPECT_EQ("\xe9\x99\xb0\xe8\xac\x80", conversion.segment(0).value());

    // Check the result
    const commands::Result &result = output.result();
    // "かまぼこの"
    EXPECT_EQ(kKamabokono, result.value());
    // "かまぼこの"
    EXPECT_EQ(kKamabokono, result.key());
  }
  EXPECT_TRUE(converter.IsActive());
}

TEST_F(SessionConverterTest, CommitPreedit) {
  SessionConverter converter(convertermock_.get());
  composer_->InsertCharacterPreedit(aiueo_);
  converter.CommitPreedit(*composer_);
  EXPECT_FALSE(converter.IsCandidateListVisible());

  {  // Check the result
    commands::Output output;
    converter.FillOutput(&output);
    EXPECT_TRUE(output.has_result());
    EXPECT_FALSE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Result &result = output.result();
    EXPECT_EQ(aiueo_, result.value());
    EXPECT_EQ(aiueo_, result.key());
  }
  EXPECT_FALSE(converter.IsActive());
}

TEST_F(SessionConverterTest, CommitSuggestion) {
  SessionConverter converter(convertermock_.get());
  Segments segments;
  {  // Initialize mock segments for suggestion
    segments.set_request_type(Segments::SUGGESTION);
    Segment *segment = segments.add_segment();
    Segment::Candidate *candidate;
    // "も"
    segment->set_key(kChars_Mo);
    candidate = segment->add_candidate();
    // "もずくす"
    candidate->value = kChars_Mozukusu;
    candidate->content_key = kChars_Mozukusu;
    candidate = segment->add_candidate();
    // "ももんが"
    candidate->value = kChars_Momonga;
    candidate->content_key = kChars_Momonga;
  }
  composer_->InsertCharacterPreedit(kChars_Mo);

  // Suggestion
  convertermock_->SetStartSuggestion(&segments, true);
  EXPECT_TRUE(converter.Suggest(composer_.get()));
  EXPECT_TRUE(converter.IsCandidateListVisible());
  EXPECT_TRUE(converter.IsActive());

  {  // Check the candidate list
    commands::Output output;
    converter.FillOutput(&output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    const commands::Preedit &preedit = output.preedit();
    EXPECT_EQ(1, preedit.segment_size());
    EXPECT_EQ(kChars_Mo, preedit.segment(0).value());

    const commands::Candidates &candidates = output.candidates();
    EXPECT_EQ(2, candidates.size());
    EXPECT_EQ(kChars_Mozukusu, candidates.candidate(0).value());
    EXPECT_FALSE(candidates.has_focused_index());
  }

  converter.CommitSuggestion(0);
  EXPECT_FALSE(converter.IsCandidateListVisible());
  EXPECT_FALSE(converter.IsActive());

  {  // Check the result
    commands::Output output;
    converter.FillOutput(&output);
    EXPECT_TRUE(output.has_result());
    EXPECT_FALSE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Result &result = output.result();
    EXPECT_EQ(kChars_Mozukusu, result.value());
    EXPECT_EQ(kChars_Mozukusu, result.key());
  }
}


TEST_F(SessionConverterTest, SuggestAndPredict) {
  SessionConverter converter(convertermock_.get());
  Segments segments;
  {  // Initialize mock segments for suggestion
    segments.set_request_type(Segments::SUGGESTION);
    Segment *segment = segments.add_segment();
    Segment::Candidate *candidate;
    // "も"
    segment->set_key(kChars_Mo);
    candidate = segment->add_candidate();
    // "もずくす"
    candidate->value = kChars_Mozukusu;
    candidate->content_key = kChars_Mozukusu;
    candidate = segment->add_candidate();
    // "ももんが"
    candidate->value = kChars_Momonga;
    candidate->content_key = kChars_Momonga;
  }
  composer_->InsertCharacterPreedit(kChars_Mo);

  // Suggestion
  convertermock_->SetStartSuggestion(&segments, true);
  EXPECT_TRUE(converter.Suggest(composer_.get()));
  EXPECT_TRUE(converter.IsCandidateListVisible());
  EXPECT_TRUE(converter.IsActive());

  {  // Check the candidate list
    commands::Output output;
    converter.FillOutput(&output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());
    EXPECT_TRUE(output.candidates().has_footer());
#ifdef CHANNEL_DEV
    EXPECT_FALSE(output.candidates().footer().has_label());
    EXPECT_TRUE(output.candidates().footer().has_sub_label());
#else  // CHANNEL_DEV
    EXPECT_TRUE(output.candidates().footer().has_label());
    EXPECT_FALSE(output.candidates().footer().has_sub_label());
#endif  //CHANNEL_DEV
    EXPECT_FALSE(output.candidates().footer().index_visible());
    EXPECT_FALSE(output.candidates().footer().logo_visible());

    const commands::Candidates &candidates = output.candidates();
    EXPECT_EQ(2, candidates.size());
    // "もずくす"
    EXPECT_EQ(kChars_Mozukusu, candidates.candidate(0).value());
    EXPECT_FALSE(candidates.has_focused_index());
  }

  segments.Clear();
  {  // Initialize mock segments for prediction
    segments.set_request_type(Segments::PREDICTION);
    Segment *segment = segments.add_segment();
    Segment::Candidate *candidate;
    // "も"
    segment->set_key(kChars_Mo);
    candidate = segment->add_candidate();
    // "もずく"
    candidate->value = kChars_Mozuku;
    candidate->content_key = kChars_Mozuku;
    candidate = segment->add_candidate();
    // "ももんが"
    candidate->value = kChars_Momonga;
    candidate->content_key = kChars_Momonga;
    candidate = segment->add_candidate();
    // "モンドリアン"
    candidate->value = "\xE3\x83\xA2\xE3\x83\xB3\xE3\x83\x89"
                       "\xE3\x83\xAA\xE3\x82\xA2\xE3\x83\xB3";
    // "もんどりあん"
    candidate->content_key = "\xE3\x82\x82\xE3\x82\x93\xE3\x81\xA9"
                             "\xE3\x82\x8A\xE3\x81\x82\xE3\x82\x93";
  }

  // Prediction
  convertermock_->SetStartPrediction(&segments, true);
  EXPECT_TRUE(converter.Predict(composer_.get()));
  EXPECT_TRUE(converter.IsCandidateListVisible());
  EXPECT_TRUE(converter.IsActive());

  // If there are suggestion results, the Prediction is not triggered.
  {
    commands::Output output;
    converter.FillOutput(&output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());
    EXPECT_FALSE(output.candidates().footer().has_label());
    EXPECT_TRUE(output.candidates().footer().index_visible());
    EXPECT_TRUE(output.candidates().footer().logo_visible());

    // Check the conversion
    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(1, conversion.segment_size());
    EXPECT_EQ(kChars_Mozukusu, conversion.segment(0).value());

    // Check the candidate list
    const commands::Candidates &candidates = output.candidates();
    // Candidates should be the same as suggestion
    EXPECT_EQ(2, candidates.size());
    EXPECT_EQ(kChars_Mozukusu, candidates.candidate(0).value());
    EXPECT_EQ(kChars_Momonga, candidates.candidate(1).value());
    EXPECT_TRUE(candidates.has_focused_index());
    EXPECT_EQ(0, candidates.focused_index());
  }

  // Prediction is called
  converter.CandidateNext();
  converter.CandidateNext();

  {  // Check the candidate list
    commands::Output output;
    converter.FillOutput(&output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    const commands::Candidates &candidates = output.candidates();
    // Candidates should be merged with the previous suggestions.
    EXPECT_EQ(4, candidates.size());
    // "もずくす"
    EXPECT_EQ(kChars_Mozukusu, candidates.candidate(0).value());
    // "ももんが"
    EXPECT_EQ(kChars_Momonga, candidates.candidate(1).value());
    // "もずく"
    EXPECT_EQ(kChars_Mozuku, candidates.candidate(2).value());
    // "モンドリアン"
    EXPECT_EQ("\xE3\x83\xA2\xE3\x83\xB3\xE3\x83\x89"
              "\xE3\x83\xAA\xE3\x82\xA2\xE3\x83\xB3",
              candidates.candidate(3).value());
    EXPECT_TRUE(candidates.has_focused_index());
  }

  // Select to "モンドリアン".
  converter.CandidateNext();
  converter.Commit();

  {  // Check the submitted value
    commands::Output output;
    converter.FillOutput(&output);
    EXPECT_TRUE(output.has_result());
    EXPECT_FALSE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Result &result = output.result();
    // "モンドリアン"
    EXPECT_EQ("\xE3\x83\xA2\xE3\x83\xB3\xE3\x83\x89"
              "\xE3\x83\xAA\xE3\x82\xA2\xE3\x83\xB3",
              result.value());
    // "もんどりあん"
    EXPECT_EQ("\xE3\x82\x82\xE3\x82\x93\xE3\x81\xA9"
              "\xE3\x82\x8A\xE3\x81\x82\xE3\x82\x93",
              result.key());
  }

  segments.Clear();
  {  // Initialize mock segments for prediction
    segments.set_request_type(Segments::PREDICTION);
    Segment *segment = segments.add_segment();
    Segment::Candidate *candidate;
    // "も"
    segment->set_key(kChars_Mo);
    candidate = segment->add_candidate();
    // "もずく"
    candidate->value = kChars_Mozuku;
    candidate->content_key = kChars_Mozuku;
    candidate = segment->add_candidate();
    // "ももんが"
    candidate->value = kChars_Momonga;
    candidate->content_key = kChars_Momonga;
    candidate = segment->add_candidate();
    // "モンドリアン"
    candidate->value = "\xE3\x83\xA2\xE3\x83\xB3\xE3\x83\x89"
                       "\xE3\x83\xAA\xE3\x82\xA2\xE3\x83\xB3";
    // "もんどりあん"
    candidate->content_key = "\xE3\x82\x82\xE3\x82\x93\xE3\x81\xA9"
                             "\xE3\x82\x8A\xE3\x81\x82\xE3\x82\x93";
  }

  // Prediction without suggestion.
  convertermock_->SetStartPrediction(&segments, true);
  EXPECT_TRUE(converter.Predict(composer_.get()));
  EXPECT_TRUE(converter.IsActive());

  {
    commands::Output output;
    converter.FillOutput(&output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    // Check the conversion
    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(1, conversion.segment_size());
    // "もずく"
    EXPECT_EQ(kChars_Mozuku, conversion.segment(0).value());

    // Check the candidate list
    const commands::Candidates &candidates = output.candidates();
    // Candidates should NOT be merged with the previous suggestions.
    EXPECT_EQ(3, candidates.size());
    // "もずく"
    EXPECT_EQ(kChars_Mozuku, candidates.candidate(0).value());
    // "ももんが"
    EXPECT_EQ(kChars_Momonga, candidates.candidate(1).value());
    // "モンドリアン"
    EXPECT_EQ("\xE3\x83\xA2\xE3\x83\xB3\xE3\x83\x89"
              "\xE3\x83\xAA\xE3\x82\xA2\xE3\x83\xB3",
              candidates.candidate(2).value());
    EXPECT_TRUE(candidates.has_focused_index());
  }
}

TEST_F(SessionConverterTest, ReloadConfig) {
  SessionConverter converter(convertermock_.get());
  Segments segments;
  SetAiueo(&segments);
  convertermock_->SetStartConversion(&segments, true);

  composer_->InsertCharacterPreedit("aiueo");
  EXPECT_TRUE(converter.Convert(composer_.get()));
  converter.SetCandidateListVisible(true);

  {  // Set config
    config::Config config;
    config::ConfigHandler::GetConfig(&config);
    config.set_selection_shortcut(config::Config::SHORTCUT_123456789);
    config::ConfigHandler::SetConfig(config);
    ASSERT_EQ(config::Config::SHORTCUT_123456789,
              GET_CONFIG(selection_shortcut));
    converter.ReloadConfig();
    EXPECT_TRUE(converter.IsCandidateListVisible());
  }
  {  // Check the config update
    commands::Output output;
    converter.FillOutput(&output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    const commands::Candidates &candidates = output.candidates();
    EXPECT_EQ("1", candidates.candidate(0).annotation().shortcut());
    EXPECT_EQ("2", candidates.candidate(1).annotation().shortcut());
  }

  {  // Set config #2
    config::Config config;
    config::ConfigHandler::GetConfig(&config);
    config.set_selection_shortcut(config::Config::NO_SHORTCUT);
    config::ConfigHandler::SetConfig(config);
    ASSERT_EQ(config::Config::NO_SHORTCUT, GET_CONFIG(selection_shortcut));
    converter.ReloadConfig();
  }
  {  // Check the config update
    commands::Output output;
    converter.FillOutput(&output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    const commands::Candidates &candidates = output.candidates();
    EXPECT_TRUE(candidates.candidate(0).annotation().shortcut().empty());
    EXPECT_TRUE(candidates.candidate(1).annotation().shortcut().empty());
  }
}

TEST_F(SessionConverterTest, OutputAllCandidateWords) {
  SessionConverter converter(convertermock_.get());
  Segments segments;
  SetKamaboko(&segments);
  convertermock_->SetStartConversion(&segments, true);

  // "かまぼこの"
  const string kKamabokono =
    "\xe3\x81\x8b\xe3\x81\xbe\xe3\x81\xbc\xe3\x81\x93\xe3\x81\xae";
  // "いんぼう"
  const string kInbou =
    "\xe3\x81\x84\xe3\x82\x93\xe3\x81\xbc\xe3\x81\x86";
  composer_->InsertCharacterPreedit(kKamabokono + kInbou);
  commands::Output output;

  EXPECT_TRUE(converter.Convert(composer_.get()));
  {
    ASSERT_TRUE(converter.IsActive());
    EXPECT_FALSE(converter.IsCandidateListVisible());

    output.Clear();
    converter.PopOutput(&output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());
    EXPECT_TRUE(output.has_all_candidate_words());

    EXPECT_EQ(0, output.all_candidate_words().focused_index());
    EXPECT_EQ(commands::CONVERSION, output.all_candidate_words().category());
    // [ "かまぼこの", "カマボコの", "カマボコノ" (t13n), "かまぼこの" (t13n),
    //   "ｶﾏﾎﾞｺﾉ" (t13n) ]
    EXPECT_EQ(5, output.all_candidate_words().candidates_size());
  }

  converter.CandidateNext();
  {
    ASSERT_TRUE(converter.IsActive());
    EXPECT_TRUE(converter.IsCandidateListVisible());

    output.Clear();
    converter.PopOutput(&output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());
    EXPECT_TRUE(output.has_all_candidate_words());

    EXPECT_EQ(1, output.all_candidate_words().focused_index());
    EXPECT_EQ(commands::CONVERSION, output.all_candidate_words().category());
    // [ "かまぼこの", "カマボコの", "カマボコノ" (t13n), "かまぼこの" (t13n),
    //   "ｶﾏﾎﾞｺﾉ" (t13n) ]
    EXPECT_EQ(5, output.all_candidate_words().candidates_size());
  }

  converter.SegmentFocusRight();
  {
    ASSERT_TRUE(converter.IsActive());
    EXPECT_FALSE(converter.IsCandidateListVisible());

    output.Clear();
    converter.PopOutput(&output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());
    EXPECT_TRUE(output.has_all_candidate_words());

    EXPECT_EQ(0, output.all_candidate_words().focused_index());
    EXPECT_EQ(commands::CONVERSION, output.all_candidate_words().category());
    // [ "陰謀", "印房", "インボウ" (t13n), "いんぼう" (t13n), "ｲﾝﾎﾞｳ" (t13n) ]
    EXPECT_EQ(5, output.all_candidate_words().candidates_size());
  }
}

// Suggest() in the suggestion state was not accepted.  (http://b/1948334)
TEST_F(SessionConverterTest, Issue1948334) {
  SessionConverter converter(convertermock_.get());
  Segments segments;
  {  // Initialize mock segments for the first suggestion
    segments.set_request_type(Segments::SUGGESTION);
    Segment *segment = segments.add_segment();
    Segment::Candidate *candidate;
    // "も"
    segment->set_key(kChars_Mo);
    candidate = segment->add_candidate();
    // "もずくす"
    candidate->value = kChars_Mozukusu;
    candidate->content_key = kChars_Mozukusu;
    candidate = segment->add_candidate();
    // "ももんが"
    candidate->value = kChars_Momonga;
    candidate->content_key = kChars_Momonga;
  }
  composer_->InsertCharacterPreedit(kChars_Mo);

  // Suggestion
  convertermock_->SetStartSuggestion(&segments, true);
  EXPECT_TRUE(converter.Suggest(composer_.get()));
  EXPECT_TRUE(converter.IsActive());

  segments.Clear();
  {  // Initialize mock segments for the second suggestion
    segments.set_request_type(Segments::SUGGESTION);
    Segment *segment = segments.add_segment();
    Segment::Candidate *candidate;
    // "もず"
    segment->set_key("\xE3\x82\x82\xE3\x81\x9A");
    candidate = segment->add_candidate();
    // "もずくす"
    candidate->value = kChars_Mozukusu;
    candidate->content_key = kChars_Mozukusu;
  }
  composer_->InsertCharacterPreedit("\xE3\x82\x82\xE3\x81\x9A");

  // Suggestion
  convertermock_->SetStartSuggestion(&segments, true);
  EXPECT_TRUE(converter.Suggest(composer_.get()));
  EXPECT_TRUE(converter.IsActive());

  {  // Check the candidate list
    commands::Output output;
    converter.FillOutput(&output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    const commands::Candidates &candidates = output.candidates();
    // Candidates should be merged with the previous suggestions.
    EXPECT_EQ(1, candidates.size());
    // "もずくす"
    EXPECT_EQ(kChars_Mozukusu, candidates.candidate(0).value());
    EXPECT_FALSE(candidates.has_focused_index());
  }
}

TEST_F(SessionConverterTest, Issue1960362) {
  // Testcase against http://b/1960362, a candidate list was not
  // updated when ConvertToTransliteration changed the size of segments.

  table_->AddRule("zyu", "ZYU", "");
  table_->AddRule("jyu", "ZYU", "");
  table_->AddRule("tt", "XTU", "t");
  table_->AddRule("ta", "TA", "");

  composer_->InsertCharacter("j");
  composer_->InsertCharacter("y");
  composer_->InsertCharacter("u");
  composer_->InsertCharacter("t");

  SessionConverter converter(convertermock_.get());

  Segments segments;
  {
    segments.set_request_type(Segments::CONVERSION);
    Segment *segment;
    Segment::Candidate *candidate;

    segment = segments.add_segment();
    segment->set_key("ZYU");
    candidate = segment->add_candidate();
    candidate->value = "[ZYU]";
    candidate->content_key = "[ZYU]";

    segment = segments.add_segment();
    segment->set_key("t");
    candidate = segment->add_candidate();
    candidate->value = "[t]";
    candidate->content_key = "[t]";
  }

  Segments resized_segments;
  {
    resized_segments.set_request_type(Segments::CONVERSION);
    Segment *segment = resized_segments.add_segment();
    Segment::Candidate *candidate;
    segment->set_key("ZYUt");
    candidate = segment->add_candidate();
    candidate->value = "[ZYUt]";
    candidate->content_key = "[ZYUt]";
  }

  convertermock_->SetStartConversion(&segments, true);
  convertermock_->SetResizeSegment1(&resized_segments, true);
  EXPECT_TRUE(converter.ConvertToTransliteration(composer_.get(),
                                                 transliteration::HALF_ASCII));
  EXPECT_FALSE(converter.IsCandidateListVisible());

  commands::Output output;
  converter.FillOutput(&output);
  EXPECT_FALSE(output.has_result());
  EXPECT_TRUE(output.has_preedit());
  EXPECT_FALSE(output.has_candidates());

  const commands::Preedit &conversion = output.preedit();
  EXPECT_EQ("jyut", conversion.segment(0).value());
}

TEST_F(SessionConverterTest, Issue1978201) {
  // This is a unittest against http://b/1978201
  SessionConverter converter(convertermock_.get());
  Segments segments;
  composer_->InsertCharacterPreedit(kChars_Mo);

  {  // Initialize mock segments for prediction
    segments.set_request_type(Segments::PREDICTION);
    Segment *segment = segments.add_segment();
    Segment::Candidate *candidate;
    // "も"
    segment->set_key(kChars_Mo);
    candidate = segment->add_candidate();
    // "もずく"
    candidate->value = kChars_Mozuku;
    candidate->content_key = kChars_Mozuku;
    candidate = segment->add_candidate();
    // "ももんが"
    candidate->value = kChars_Momonga;
    candidate->content_key = kChars_Momonga;
  }

  // Prediction
  convertermock_->SetStartPrediction(&segments, true);
  EXPECT_TRUE(converter.Predict(composer_.get()));
  EXPECT_TRUE(converter.IsActive());

  {  // Check the conversion
    commands::Output output;
    converter.FillOutput(&output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(1, conversion.segment_size());
    EXPECT_EQ(kChars_Mozuku, conversion.segment(0).value());
  }

  // Meaningless segment manipulations.
  converter.SegmentWidthShrink();
  converter.SegmentFocusLeft();
  converter.SegmentFocusLast();

  {  // Check the conversion again
    commands::Output output;
    converter.FillOutput(&output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(1, conversion.segment_size());
    EXPECT_EQ(kChars_Mozuku, conversion.segment(0).value());
  }
}

TEST_F(SessionConverterTest, Issue1981020) {
  SessionConverter converter(convertermock_.get());
  // hiragana "ヴヴヴヴ"
  composer_->InsertCharacterPreedit
      ("\xE3\x82\x94\xE3\x82\x94\xE3\x82\x94\xE3\x82\x94");
  Segments segments;
  convertermock_->SetFinishConversion(&segments, true);
  converter.CommitPreedit(*composer_.get());
  convertermock_->GetFinishConversion(&segments);
  // katakana "ヴヴヴヴ"
  EXPECT_EQ("\xE3\x83\xB4\xE3\x83\xB4\xE3\x83\xB4\xE3\x83\xB4",
            segments.conversion_segment(0).candidate(0).value);
  EXPECT_EQ("\xE3\x83\xB4\xE3\x83\xB4\xE3\x83\xB4\xE3\x83\xB4",
            segments.conversion_segment(0).candidate(0).content_value);
}

TEST_F(SessionConverterTest, Issue2029557) {
  // Unittest against http://b/2029557
  // a<tab><F6> raised a DCHECK error.
  SessionConverter converter(convertermock_.get());
  // Composition (as "a")
  composer_->InsertCharacterPreedit("a");

  // Prediction (as <tab>)
  Segments segments;
  SetAiueo(&segments);
  convertermock_->SetStartPrediction(&segments, true);
  EXPECT_TRUE(converter.Predict(composer_.get()));
  EXPECT_TRUE(converter.IsActive());

  // Transliteration (as <F6>)
  segments.clear();
  Segment *segment = segments.add_segment();
  segment->set_key("a");
  Segment::Candidate *candidate = segment->add_candidate();
  candidate->value = "a";

  convertermock_->SetStartConversion(&segments, true);
  EXPECT_TRUE(converter.ConvertToTransliteration(composer_.get(),
                                                 transliteration::HIRAGANA));
  EXPECT_TRUE(converter.IsActive());
}

TEST_F(SessionConverterTest, Issue2031986) {
  // Unittest against http://b/2031986
  // aaaaa<Shift+Enter> raised a CRT error.
  SessionConverter converter(convertermock_.get());

  {  // Initialize a suggest result triggered by "aaaa".
    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key("aaaa");
    Segment::Candidate *candidate;
    candidate = segment->add_candidate();
    candidate->value = "AAAA";
    candidate = segment->add_candidate();
    candidate->value = "Aaaa";
    convertermock_->SetStartSuggestion(&segments, true);
  }
  // Get suggestion
  composer_->InsertCharacterPreedit("aaaa");
  EXPECT_TRUE(converter.Suggest(composer_.get()));
  EXPECT_TRUE(converter.IsActive());

  {  // Initialize no suggest result triggered by "aaaaa".
    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key("aaaaa");
    convertermock_->SetStartSuggestion(&segments, false);
  }
  // Hide suggestion
  composer_->InsertCharacterPreedit("a");
  EXPECT_FALSE(converter.Suggest(composer_.get()));
  EXPECT_FALSE(converter.IsActive());
}

TEST_F(SessionConverterTest, Issue2040116) {
  // Unittest against http://b/2040116
  //
  // It happens when the first Predict returns results but the next
  // MaybeExpandPrediction does not return any results.  That's a
  // trick by GoogleSuggest.
  SessionConverter converter(convertermock_.get());
  composer_->InsertCharacterPreedit("G");

  {
    // Initialize no predict result.
    Segments segments;
    segments.set_request_type(Segments::PREDICTION);
    Segment *segment = segments.add_segment();
    segment->set_key("G");
    convertermock_->SetStartPrediction(&segments, false);
  }
  // Get prediction
  EXPECT_FALSE(converter.Predict(composer_.get()));
  EXPECT_FALSE(converter.IsActive());

  {
    // Initialize a suggest result triggered by "G".
    Segments segments;
    segments.set_request_type(Segments::PREDICTION);
    Segment *segment = segments.add_segment();
    segment->set_key("G");
    Segment::Candidate *candidate;
    candidate = segment->add_candidate();
    candidate->value = "GoogleSuggest";
    convertermock_->SetStartPrediction(&segments, true);
  }
  // Get prediction again
  EXPECT_TRUE(converter.Predict(composer_.get()));
  EXPECT_TRUE(converter.IsActive());

  {  // Check the conversion.
    commands::Output output;
    converter.FillOutput(&output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(1, conversion.segment_size());
    EXPECT_EQ("GoogleSuggest", conversion.segment(0).value());
  }

  {
    // Initialize no predict result triggered by "G".  It's possible
    // by Google Suggest.
    Segments segments;
    segments.set_request_type(Segments::PREDICTION);
    Segment *segment = segments.add_segment();
    segment->set_key("G");
    convertermock_->SetStartPrediction(&segments, false);
  }
  // Hide prediction
  converter.CandidateNext();
  EXPECT_TRUE(converter.IsActive());

  {  // Check the conversion.
    commands::Output output;
    converter.FillOutput(&output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(1, conversion.segment_size());
    EXPECT_EQ("GoogleSuggest", conversion.segment(0).value());

    const commands::Candidates &candidates = output.candidates();
    EXPECT_EQ(1, candidates.candidate_size());
  }
}

// since History segments are almost hidden from
namespace {
class ConverterMockForReset : public ConverterMock {
 public:
  bool ResetConversion(Segments *segments) const {
    reset_conversion_called_ = true;
    return true;
  }

  bool reset_conversion_called() const {
    return reset_conversion_called_;
  }

  void Reset() {
    reset_conversion_called_ = false;
  }

  ConverterMockForReset() : reset_conversion_called_(false) {}
 private:
  mutable bool reset_conversion_called_;
};

class ConverterMockForRevert : public ConverterMock {
 public:
  bool RevertConversion(Segments *segments) const {
    revert_conversion_called_ = true;
    return true;
  }

  bool revert_conversion_called() const {
    return revert_conversion_called_;
  }

  void Reset() {
    revert_conversion_called_ = false;
  }

  ConverterMockForRevert() : revert_conversion_called_(false) {}
 private:
  mutable bool revert_conversion_called_;
};
} // namespace

TEST(SessionConverterResetTest, Reset) {
  ConverterMockForReset convertermock;
  ConverterFactory::SetConverter(&convertermock);
  SessionConverter converter(&convertermock);
  EXPECT_FALSE(convertermock.reset_conversion_called());
  converter.Reset();
  EXPECT_TRUE(convertermock.reset_conversion_called());
}

TEST(SessionConverterRevertTest, Revert) {
  ConverterMockForRevert convertermock;
  ConverterFactory::SetConverter(&convertermock);
  SessionConverter converter(&convertermock);
  EXPECT_FALSE(convertermock.revert_conversion_called());
  converter.Revert();
  EXPECT_TRUE(convertermock.revert_conversion_called());
}
}  // namespace session
}  // namespace mozc
