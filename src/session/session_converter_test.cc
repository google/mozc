// Copyright 2010-2011, Google Inc.
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

// Tests for session converter.
//
// Note that we have a lot of tests which assume that the converter fills
// T13Ns. If you want to add test case related to T13Ns, please make sure
// you set T13Ns to the result for a mock converter.

#include <set>
#include <string>
#include <vector>

#include "base/base.h"
#include "base/util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/converter_interface.h"
#include "converter/converter_mock.h"
#include "converter/segments.h"
#include "composer/composer.h"
#include "composer/table.h"
#include "session/internal/keymap.h"
#include "session/session_converter.h"
#include "session/internal/candidate_list.h"
#include "testing/base/public/gunit.h"
#include "testing/base/public/googletest.h"
#include "transliteration/transliteration.h"

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
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);

    table_.reset(new composer::Table);
    table_->Initialize();
    composer_.reset(new composer::Composer);
    composer_->SetTable(table_.get());

    // "あいうえお"
    aiueo_ = "\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86\xe3\x81\x88\xe3\x81\x8a";
  }

  virtual void TearDown() {
    table_.reset();
    composer_.reset();
    // just in case, reset the config in test_tmpdir
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);
  }

  // set result for "あいうえお"
  void SetAiueo(Segments *segments) {
    segments->Clear();
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

    segments->Clear();
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

    // Set dummy T13Ns
    vector<Segment::Candidate> *meta_candidates =
        segment->mutable_meta_candidates();
    meta_candidates->resize(transliteration::NUM_T13N_TYPES);
    for (size_t i = 0; i < transliteration::NUM_T13N_TYPES; ++i) {
      meta_candidates->at(i).Init();
      meta_candidates->at(i).value = segment->key();
      meta_candidates->at(i).content_value = segment->key();
      meta_candidates->at(i).content_key = segment->key();
    }
  }

  // set T13N candidates to segments using composer
  void FillT13Ns(Segments *segments, const composer::Composer *composer) {
    size_t composition_pos = 0;
    for (size_t i = 0; i < segments->conversion_segments_size(); ++i) {
      Segment *segment = segments->mutable_conversion_segment(i);
      CHECK(segment);
      const size_t composition_len = Util::CharsLen(segment->key());
      vector<string> t13ns;
      composer->GetSubTransliterations(
          composition_pos, composition_len, &t13ns);
      vector<Segment::Candidate> *meta_candidates =
          segment->mutable_meta_candidates();
      meta_candidates->resize(transliteration::NUM_T13N_TYPES);
      for (size_t j = 0; j < transliteration::NUM_T13N_TYPES; ++j) {
        meta_candidates->at(j).Init();
        meta_candidates->at(j).value = t13ns[j];
        meta_candidates->at(j).content_value = t13ns[j];
        meta_candidates->at(j).content_key = segment->key();
      }
      composition_pos += composition_len;
    }
  }

  // set result for "like"
  void InitConverterWithLike(Segments *segments) {
    // "ぃ"
    composer_->InsertCharacterKeyAndPreedit("li", "\xE3\x81\x83");
    // "け"
    composer_->InsertCharacterKeyAndPreedit("ke", "\xE3\x81\x91");

    Segment *segment;
    Segment::Candidate *candidate;

    segments->Clear();
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

    FillT13Ns(segments, composer_.get());
    convertermock_->SetStartConversionWithComposer(segments, true);
  }

  void InsertASCIISequence(const string text, composer::Composer *composer) {
    for (size_t i = 0; i < text.size(); ++i) {
      commands::KeyEvent key;
      key.set_key_code(text[i]);
      composer->InsertCharacterKeyEvent(key);
    }
  }

  void ExpectSameSessionConverter(const SessionConverter &lhs,
                                  const SessionConverter &rhs) {
    EXPECT_EQ(lhs.IsActive(), rhs.IsActive());
    EXPECT_EQ(lhs.IsCandidateListVisible(), rhs.IsCandidateListVisible());
    EXPECT_EQ(lhs.GetSegmentIndex(), rhs.GetSegmentIndex());

    EXPECT_EQ(lhs.GetOperationPreferences().use_cascading_window,
              rhs.GetOperationPreferences().use_cascading_window);
    EXPECT_EQ(lhs.GetOperationPreferences().candidate_shortcuts,
              rhs.GetOperationPreferences().candidate_shortcuts);
    EXPECT_EQ(lhs.conversion_preferences().use_history,
              rhs.conversion_preferences().use_history);
    EXPECT_EQ(lhs.conversion_preferences().max_history_size,
              rhs.conversion_preferences().max_history_size);
    EXPECT_EQ(lhs.IsCandidateListVisible(),
              rhs.IsCandidateListVisible());

    Segments segments_lhs, segments_rhs;
    lhs.GetSegments(&segments_lhs);
    rhs.GetSegments(&segments_rhs);
    EXPECT_EQ(segments_lhs.segments_size(),
              segments_rhs.segments_size());
    for (size_t i = 0; i < segments_lhs.segments_size(); ++i) {
      Segment segment_lhs, segment_rhs;
      segment_lhs.CopyFrom(segments_lhs.segment(i));
      segment_rhs.CopyFrom(segments_rhs.segment(i));
      EXPECT_EQ(segment_lhs.key(), segment_rhs.key()) << " i=" << i;
      EXPECT_EQ(segment_lhs.segment_type(),
                segment_rhs.segment_type()) << " i=" << i;
      EXPECT_EQ(segment_lhs.candidates_size(), segment_rhs.candidates_size());
    }

    const CandidateList &candidate_list_lhs = lhs.GetCandidateList();
    const CandidateList &candidate_list_rhs = rhs.GetCandidateList();
    EXPECT_EQ(candidate_list_lhs.name(), candidate_list_rhs.name());
    EXPECT_EQ(candidate_list_lhs.page_size(), candidate_list_rhs.page_size());
    EXPECT_EQ(candidate_list_lhs.size(), candidate_list_rhs.size());
    EXPECT_EQ(candidate_list_lhs.last_index(), candidate_list_rhs.last_index());
    EXPECT_EQ(candidate_list_lhs.focused_id(), candidate_list_rhs.focused_id());
    EXPECT_EQ(candidate_list_lhs.focused_index(),
              candidate_list_rhs.focused_index());
    EXPECT_EQ(candidate_list_lhs.focused(), candidate_list_rhs.focused());

    for (int i = 0; i < candidate_list_lhs.size(); ++i) {
      const Candidate &candidate_lhs = candidate_list_lhs.candidate(i);
      const Candidate &candidate_rhs = candidate_list_rhs.candidate(i);
      EXPECT_EQ(candidate_lhs.id(), candidate_rhs.id());
      EXPECT_EQ(candidate_lhs.attributes(), candidate_rhs.attributes());
      EXPECT_EQ(candidate_lhs.IsSubcandidateList(),
                candidate_rhs.IsSubcandidateList());
      if (candidate_lhs.IsSubcandidateList()) {
        EXPECT_EQ(candidate_lhs.subcandidate_list().size(),
                  candidate_rhs.subcandidate_list().size());
      }
    }

    const commands::Result result_lhs = lhs.GetResult();
    const commands::Result result_rhs = rhs.GetResult();
    EXPECT_EQ(result_lhs.type(), result_rhs.type());
    EXPECT_EQ(result_lhs.value(), result_rhs.value());
    EXPECT_EQ(result_lhs.key(), result_rhs.key());
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
  FillT13Ns(&segments, composer_.get());
  convertermock_->SetStartConversionWithComposer(&segments, true);

  composer_->InsertCharacterPreedit(aiueo_);
  EXPECT_TRUE(converter.Convert(*composer_));
  ASSERT_TRUE(converter.IsActive());

  commands::Output output;
  converter.FillOutput(*composer_, &output);
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
  composer_->Reset();
  output.Clear();
  converter.FillOutput(*composer_, &output);
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

TEST_F(SessionConverterTest, ConvertWithSpellingCorrection) {
  SessionConverter converter(convertermock_.get());
  Segments segments;
  SetAiueo(&segments);
  FillT13Ns(&segments, composer_.get());
  segments.mutable_conversion_segment(0)->mutable_candidate(0)->attributes |=
      Segment::Candidate::SPELLING_CORRECTION;
  convertermock_->SetStartConversionWithComposer(&segments, true);

  composer_->InsertCharacterPreedit(aiueo_);
  EXPECT_TRUE(converter.Convert(*composer_));
  ASSERT_TRUE(converter.IsActive());
  EXPECT_TRUE(converter.IsCandidateListVisible());
}

TEST_F(SessionConverterTest, ConvertToTransliteration) {
  SessionConverter converter(convertermock_.get());
  Segments segments;
  SetAiueo(&segments);

  composer_->InsertCharacterKeyAndPreedit("aiueo", aiueo_);
  FillT13Ns(&segments, composer_.get());
  convertermock_->SetStartConversionWithComposer(&segments, true);

  EXPECT_TRUE(converter.ConvertToTransliteration(*composer_,
                                                 transliteration::HALF_ASCII));
  {  // Check the conversion #1
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(1, conversion.segment_size());
    EXPECT_EQ("aiueo", conversion.segment(0).value());
    EXPECT_FALSE(converter.IsCandidateListVisible());
  }

  EXPECT_TRUE(converter.ConvertToTransliteration(*composer_,
                                                 transliteration::HALF_ASCII));
  {  // Check the conversion #2
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(1, conversion.segment_size());
    EXPECT_EQ("AIUEO", conversion.segment(0).value());
    EXPECT_FALSE(converter.IsCandidateListVisible());
  }

  EXPECT_TRUE(converter.ConvertToTransliteration(*composer_,
                                                 transliteration::FULL_ASCII));
  {  // Check the conversion #3
    commands::Output output;
    converter.FillOutput(*composer_, &output);
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
  EXPECT_TRUE(converter.Convert(*composer_));
  {  // Check the conversion #1
    commands::Output output;
    converter.FillOutput(*composer_, &output);
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
  EXPECT_TRUE(converter.ConvertToTransliteration(*composer_,
                                                 transliteration::HALF_ASCII));
  {  // Check the conversion #2
    commands::Output output;
    converter.FillOutput(*composer_, &output);
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
  {  // Set OperationPreferences
    OperationPreferences preferences;
    preferences.use_cascading_window = false;
    preferences.candidate_shortcuts = "";
    converter.SetOperationPreferences(preferences);
  }

  // "ｄｖｄ"
  composer_->InsertCharacterKeyAndPreedit(
      "dvd", "\xEF\xBD\x84\xEF\xBD\x96\xEF\xBD\x84");
  FillT13Ns(&segments, composer_.get());
  convertermock_->SetStartConversionWithComposer(&segments, true);
  EXPECT_TRUE(converter.ConvertToTransliteration(*composer_,
                                                 transliteration::FULL_ASCII));
  {  // Check the conversion #1
    commands::Output output;
    converter.FillOutput(*composer_, &output);
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

  EXPECT_TRUE(converter.ConvertToTransliteration(*composer_,
                                                 transliteration::FULL_ASCII));
  {  // Check the conversion #2
    commands::Output output;
    converter.FillOutput(*composer_, &output);
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

  EXPECT_TRUE(converter.ConvertToTransliteration(*composer_,
                                                 transliteration::FULL_ASCII));
  {  // Check the conversion #3
    commands::Output output;
    converter.FillOutput(*composer_, &output);
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
  const string kKamabokono =
    "\xe3\x81\x8b\xe3\x81\xbe\xe3\x81\xbc\xe3\x81\x93\xe3\x81\xae";
  const string kInbou =
    "\xe3\x81\x84\xe3\x82\x93\xe3\x81\xbc\xe3\x81\x86";

  // "かまぼこのいんぼう"
  composer_->InsertCharacterPreedit(kKamabokono + kInbou);
  FillT13Ns(&segments, composer_.get());
  convertermock_->SetStartConversionWithComposer(&segments, true);
  EXPECT_TRUE(converter.Convert(*composer_));

  // Test for conversion
  {
    commands::Output output;
    converter.FillOutput(*composer_, &output);
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
  converter.CandidateNext(*composer_);
  EXPECT_TRUE(converter.IsCandidateListVisible());
  converter.CandidatePrev();
  EXPECT_TRUE(converter.IsCandidateListVisible());

  // Test for candidates
  {
    commands::Output output;
    converter.FillOutput(*composer_, &output);
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
    converter.FillOutput(*composer_, &output);
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

  // Test for segment motion. [SegmentFocusLeft]
  {
    converter.SegmentFocusLeft();
    EXPECT_FALSE(converter.IsCandidateListVisible());
    converter.SetCandidateListVisible(true);

    commands::Output output;
    converter.FillOutput(*composer_, &output);
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

  // Test for segment motion. [SegmentFocusLeft] at the head of segments.
  // http://b/2990134
  // Focus changing at the tail of segments to right,
  // and at the head of segments to left, should work.
  {
    converter.SegmentFocusLeft();
    EXPECT_FALSE(converter.IsCandidateListVisible());
    converter.SetCandidateListVisible(true);

    commands::Output output;
    converter.FillOutput(*composer_, &output);
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

  // Test for segment motion. [SegmentFocusRight] at the tail of segments.
  // http://b/2990134
  // Focus changing at the tail of segments to right,
  // and at the head of segments to left, should work.
  {
    converter.SegmentFocusRight();
    EXPECT_FALSE(converter.IsCandidateListVisible());
    converter.SetCandidateListVisible(true);

    commands::Output output;
    converter.FillOutput(*composer_, &output);
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

  // Test for candidate motion. [CandidateNext]
  {
    converter.SegmentFocusRight();  // Focus to the last segment.
    converter.CandidateNext(*composer_);
    EXPECT_TRUE(converter.IsCandidateListVisible());
    commands::Output output;
    converter.FillOutput(*composer_, &output);
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
    converter.FillOutput(*composer_, &output);
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

  {
    converter.Commit();
    composer_->Reset();
    EXPECT_FALSE(converter.IsCandidateListVisible());

    commands::Output output;
    converter.FillOutput(*composer_, &output);
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
  FillT13Ns(&segments, composer_.get());
  convertermock_->SetStartConversionWithComposer(&segments, true);
  EXPECT_TRUE(converter.Convert(*composer_));
  EXPECT_FALSE(converter.IsCandidateListVisible());

  // Move to the t13n list.
  converter.CandidateNext(*composer_);
  EXPECT_TRUE(converter.IsCandidateListVisible());

  commands::Output output;
  converter.FillOutput(*composer_, &output);
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

TEST_F(SessionConverterTest, T13NWithResegmentation) {
  SessionConverter converter(convertermock_.get());
  {
    Segments segments;
    Segment *segment = segments.add_segment();
    Segment::Candidate *candidate;
    CHECK(segment);
    // "かまぼこの"
    segment->set_key(
        "\xe3\x81\x8b\xe3\x81\xbe\xe3\x81\xbc\xe3\x81\x93\xe3\x81\xae");
    candidate = segment->add_candidate();
    CHECK(candidate);
    // "かまぼこの"
    candidate->value =
        "\xe3\x81\x8b\xe3\x81\xbe\xe3\x81\xbc\xe3\x81\x93\xe3\x81\xae";

    segment = segments.add_segment();
    CHECK(segment);
    // "いんぼう"
    segment->set_key("\xe3\x81\x84\xe3\x82\x93\xe3\x81\xbc\xe3\x81\x86");
    candidate = segment->add_candidate();
    CHECK(candidate);
    // "いんぼう"
    candidate->value =
        "\xe3\x81\x84\xe3\x82\x93\xe3\x81\xbc\xe3\x81\x86";

    InsertASCIISequence("kamabokonoinbou", composer_.get());
    FillT13Ns(&segments, composer_.get());
    convertermock_->SetStartConversionWithComposer(&segments, true);
  }
  EXPECT_TRUE(converter.Convert(*composer_));
  // Test for segment motion. [SegmentFocusRight]
  converter.SegmentFocusRight();
  // Shrink segment
  {
    Segments segments;
    Segment *segment;
    Segment::Candidate *candidate;

    segments.Clear();
    segment = segments.add_segment();

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

    segment = segments.add_segment();
    // "いんぼ"
    segment->set_key("\xe3\x81\x84\xe3\x82\x93\xe3\x81\xbc");
    candidate = segment->add_candidate();
    // "インボ"
    candidate->value = "\xe3\x82\xa4\xe3\x83\xb3\xe3\x83\x9c";

    segment = segments.add_segment();
    // "う"
    segment->set_key("\xe3\x81\x86");
    candidate = segment->add_candidate();
    // "ウ"
    candidate->value = "\xe3\x82\xa6";

    FillT13Ns(&segments, composer_.get());
    convertermock_->SetResizeSegment1(&segments, true);
  }
  converter.SegmentWidthShrink();

  // Convert to half katakana
  converter.ConvertToTransliteration(*composer_,
                                     transliteration::HALF_KATAKANA);

  {
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    const commands::Preedit &preedit = output.preedit();
    EXPECT_EQ(3, preedit.segment_size());
    // "ｲﾝﾎﾞ"
    EXPECT_EQ("\xef\xbd\xb2\xef\xbe\x9d\xef\xbe\x8e\xef\xbe\x9e",
              preedit.segment(1).value());
  }
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
  FillT13Ns(&segments, composer_.get());
  convertermock_->SetStartConversionWithComposer(&segments, true);
  EXPECT_TRUE(converter.ConvertToHalfWidth(*composer_));
  EXPECT_FALSE(converter.IsCandidateListVisible());

  {  // Make sure the output
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(1, conversion.segment_size());
    // "ｱbc"
    EXPECT_EQ("\xEF\xBD\xB1\x62\x63", conversion.segment(0).value());
  }

  // Composition will be transliterated to "ａｂｃ".
  EXPECT_TRUE(converter.ConvertToTransliteration(*composer_,
                                                 transliteration::FULL_ASCII));
  {  // Make sure the output
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(1, conversion.segment_size());
    // "ａｂｃ"
    EXPECT_EQ("\xEF\xBD\x81\xEF\xBD\x82\xEF\xBD\x83",
              conversion.segment(0).value());
  }

  EXPECT_TRUE(converter.ConvertToHalfWidth(*composer_));
  EXPECT_FALSE(converter.IsCandidateListVisible());
  {  // Make sure the output
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(1, conversion.segment_size());
    // "abc"
    EXPECT_EQ("abc", conversion.segment(0).value());
  }
}

TEST_F(SessionConverterTest, ConvertToHalfWidth_2) {
  // http://b/2517514
  // ConvertToHalfWidth converts punctuations differently w/ or w/o kana.
  SessionConverter converter(convertermock_.get());
  // "ｑ"
  composer_->InsertCharacterKeyAndPreedit("q", "\xef\xbd\x91");
  // "、"
  composer_->InsertCharacterKeyAndPreedit(",", "\xe3\x80\x81");
  // "。"
  composer_->InsertCharacterKeyAndPreedit(".", "\xe3\x80\x82");

  Segments segments;
  {  // Initialize segments.
    Segment *segment = segments.add_segment();
    // "ｑ、。"
    segment->set_key("\xef\xbd\x91\xe3\x80\x81\xe3\x80\x82");
    segment->add_candidate()->value = "q,.";
    // "q､｡"
    segment->add_candidate()->value = "q\xef\xbd\xa4\xef\xbd\xa1";
  }
  FillT13Ns(&segments, composer_.get());
  convertermock_->SetStartConversionWithComposer(&segments, true);
  EXPECT_TRUE(converter.ConvertToHalfWidth(*composer_));
  EXPECT_FALSE(converter.IsCandidateListVisible());

  {  // Make sure the output
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(1, conversion.segment_size());
    // "q､｡"
    EXPECT_EQ("q\xef\xbd\xa4\xef\xbd\xa1", conversion.segment(0).value());
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
    FillT13Ns(&segments, composer_.get());
    convertermock_->SetStartConversionWithComposer(&segments, true);
    EXPECT_TRUE(converter.SwitchKanaType(*composer_));
    EXPECT_FALSE(converter.IsCandidateListVisible());

    {  // Make sure the output
      commands::Output output;
      converter.FillOutput(*composer_, &output);
      EXPECT_FALSE(output.has_result());
      EXPECT_TRUE(output.has_preedit());
      EXPECT_FALSE(output.has_candidates());

      const commands::Preedit &conversion = output.preedit();
      EXPECT_EQ(1, conversion.segment_size());
      // "アｂｃ"
      EXPECT_EQ("\xE3\x82\xA2\xEF\xBD\x82\xEF\xBD\x83",
                conversion.segment(0).value());
    }

    EXPECT_TRUE(converter.SwitchKanaType(*composer_));
    EXPECT_FALSE(converter.IsCandidateListVisible());
    {
      commands::Output output;
      converter.FillOutput(*composer_, &output);
      EXPECT_FALSE(output.has_result());
      EXPECT_TRUE(output.has_preedit());
      EXPECT_FALSE(output.has_candidates());

      const commands::Preedit &conversion = output.preedit();
      EXPECT_EQ(1, conversion.segment_size());
      // "ｱbc"
      EXPECT_EQ("\xEF\xBD\xB1\x62\x63", conversion.segment(0).value());
    }

    EXPECT_TRUE(converter.SwitchKanaType(*composer_));
    EXPECT_FALSE(converter.IsCandidateListVisible());
    {
      commands::Output output;
      converter.FillOutput(*composer_, &output);
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
    FillT13Ns(&segments, composer_.get());
    convertermock_->SetStartConversionWithComposer(&segments, true);
    EXPECT_TRUE(converter.Convert(*composer_));
    EXPECT_FALSE(converter.IsCandidateListVisible());

    {  // Make sure the output
      commands::Output output;
      converter.FillOutput(*composer_, &output);
      EXPECT_FALSE(output.has_result());
      EXPECT_TRUE(output.has_preedit());
      EXPECT_FALSE(output.has_candidates());

      const commands::Preedit &conversion = output.preedit();
      EXPECT_EQ(1, conversion.segment_size());
      // "漢字"
      EXPECT_EQ("\xE6\xBC\xA2\xE5\xAD\x97",
                conversion.segment(0).value());
    }

    EXPECT_TRUE(converter.SwitchKanaType(*composer_));
    EXPECT_FALSE(converter.IsCandidateListVisible());
    {
      commands::Output output;
      converter.FillOutput(*composer_, &output);
      EXPECT_FALSE(output.has_result());
      EXPECT_TRUE(output.has_preedit());
      EXPECT_FALSE(output.has_candidates());

      const commands::Preedit &conversion = output.preedit();
      EXPECT_EQ(1, conversion.segment_size());
      // "かんじ"
      EXPECT_EQ("\xE3\x81\x8B\xE3\x82\x93\xE3\x81\x98",
                conversion.segment(0).value());
    }

    EXPECT_TRUE(converter.SwitchKanaType(*composer_));
    EXPECT_FALSE(converter.IsCandidateListVisible());
    {
      commands::Output output;
      converter.FillOutput(*composer_, &output);
      EXPECT_FALSE(output.has_result());
      EXPECT_TRUE(output.has_preedit());
      EXPECT_FALSE(output.has_candidates());

      const commands::Preedit &conversion = output.preedit();
      EXPECT_EQ(1, conversion.segment_size());
      // "カンジ"
      EXPECT_EQ("\xE3\x82\xAB\xE3\x83\xB3\xE3\x82\xB8",
                conversion.segment(0).value());
    }

    EXPECT_TRUE(converter.SwitchKanaType(*composer_));
    EXPECT_FALSE(converter.IsCandidateListVisible());
    {
      commands::Output output;
      converter.FillOutput(*composer_, &output);
      EXPECT_FALSE(output.has_result());
      EXPECT_TRUE(output.has_preedit());
      EXPECT_FALSE(output.has_candidates());

      const commands::Preedit &conversion = output.preedit();
      EXPECT_EQ(1, conversion.segment_size());
      // "ｶﾝｼﾞ"
      EXPECT_EQ("\xEF\xBD\xB6\xEF\xBE\x9D\xEF\xBD\xBC\xEF\xBE\x9E",
                conversion.segment(0).value());
    }

    EXPECT_TRUE(converter.SwitchKanaType(*composer_));
    EXPECT_FALSE(converter.IsCandidateListVisible());
    {
      commands::Output output;
      converter.FillOutput(*composer_, &output);
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
  FillT13Ns(&segments, composer_.get());
  convertermock_->SetStartConversionWithComposer(&segments, true);

  const string kKamabokono =
    "\xe3\x81\x8b\xe3\x81\xbe\xe3\x81\xbc\xe3\x81\x93\xe3\x81\xae";
  const string kInbou =
    "\xe3\x81\x84\xe3\x82\x93\xe3\x81\xbc\xe3\x81\x86";

  // "かまぼこのいんぼう"
  composer_->InsertCharacterPreedit(kKamabokono + kInbou);
  EXPECT_TRUE(converter.Convert(*composer_));
  EXPECT_FALSE(converter.IsCandidateListVisible());

  {  // Check the conversion before CommitFirstSegment.
    commands::Output output;
    converter.FillOutput(*composer_, &output);
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
  size_t size;
  converter.CommitFirstSegment(&size);
  EXPECT_FALSE(converter.IsCandidateListVisible());
  // "かまぼこの"
  EXPECT_EQ(Util::CharsLen(kKamabokono), size);
  EXPECT_TRUE(converter.IsActive());
}

TEST_F(SessionConverterTest, CommitPreedit) {
  SessionConverter converter(convertermock_.get());
  composer_->InsertCharacterPreedit(aiueo_);
  converter.CommitPreedit(*composer_);
  composer_->Reset();
  EXPECT_FALSE(converter.IsCandidateListVisible());

  {  // Check the result
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_TRUE(output.has_result());
    EXPECT_FALSE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Result &result = output.result();
    EXPECT_EQ(aiueo_, result.value());
    EXPECT_EQ(aiueo_, result.key());
  }
  EXPECT_FALSE(converter.IsActive());
}

TEST_F(SessionConverterTest, CommitSuggestionByIndex) {
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
  convertermock_->SetStartSuggestionWithComposer(&segments, true);
  EXPECT_TRUE(converter.Suggest(*composer_));
  EXPECT_TRUE(converter.IsCandidateListVisible());
  EXPECT_TRUE(converter.IsActive());

  {  // Check the candidate list
    commands::Output output;
    converter.FillOutput(*composer_, &output);
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

  // FinishConversion is expected to return empty Segments.
  convertermock_->SetFinishConversion(
      scoped_ptr<Segments>(new Segments).get(), true);

  size_t committed_key_size = 0;
  converter.CommitSuggestionByIndex(0, *composer_.get(), &committed_key_size);
  composer_->Reset();
  EXPECT_FALSE(converter.IsCandidateListVisible());
  EXPECT_FALSE(converter.IsActive());
  EXPECT_EQ(4, committed_key_size);

  {  // Check the result
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_TRUE(output.has_result());
    EXPECT_FALSE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Result &result = output.result();
    EXPECT_EQ(kChars_Mozukusu, result.value());
    EXPECT_EQ(kChars_Mozukusu, result.key());
    EXPECT_EQ(SessionConverterInterface::COMPOSITION,
              converter.GetState());
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
  convertermock_->SetStartSuggestionWithComposer(&segments, true);
  EXPECT_TRUE(converter.Suggest(*composer_));
  EXPECT_TRUE(converter.IsCandidateListVisible());
  EXPECT_TRUE(converter.IsActive());

  {  // Check the candidate list
    commands::Output output;
    converter.FillOutput(*composer_, &output);
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
#endif  // CHANNEL_DEV
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
  convertermock_->SetStartPredictionWithComposer(&segments, true);
  EXPECT_TRUE(converter.Predict(*composer_));
  EXPECT_TRUE(converter.IsCandidateListVisible());
  EXPECT_TRUE(converter.IsActive());

  // If there are suggestion results, the Prediction is not triggered.
  {
    commands::Output output;
    converter.FillOutput(*composer_, &output);
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
  converter.CandidateNext(*composer_);
  converter.CandidateNext(*composer_);

  {  // Check the candidate list
    commands::Output output;
    converter.FillOutput(*composer_, &output);
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
  converter.CandidateNext(*composer_);
  converter.Commit();
  composer_->Reset();

  {  // Check the submitted value
    commands::Output output;
    converter.FillOutput(*composer_, &output);
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
  convertermock_->SetStartPredictionWithComposer(&segments, true);
  EXPECT_TRUE(converter.Predict(*composer_));
  EXPECT_TRUE(converter.IsActive());

  {
    commands::Output output;
    converter.FillOutput(*composer_, &output);
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

TEST_F(SessionConverterTest, SuppressSuggestionOnPasswordField) {
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
  composer_->SetInputFieldType(commands::SessionCommand::PASSWORD);
  composer_->InsertCharacterPreedit(kChars_Mo);

  // Suggestion
  convertermock_->SetStartSuggestionWithComposer(&segments, true);
  // No candidates should be visible because we are on password field.
  EXPECT_FALSE(converter.Suggest(*composer_));
  EXPECT_FALSE(converter.IsCandidateListVisible());
  EXPECT_FALSE(converter.IsActive());
}


TEST_F(SessionConverterTest, ExpandSuggestion) {
  SessionConverter converter(convertermock_.get());

  const char *kSuggestionValues[] = {
      "S0",
      "S1",
      "S2",
  };
  const char *kPredictionValues[] = {
      "P0",
      "P1",
      "P2",
      // Duplicate entry. Any dupulication should not exist
      // in the candidate list.
      "S1",
      "P3",
  };
  const char kKey[] = "key";
  const int kDupulicationIndex = 3;

  Segments segments;
  {  // Initialize mock segments for suggestion
    segments.set_request_type(Segments::SUGGESTION);
    Segment *segment = segments.add_segment();
    Segment::Candidate *candidate;
    segment->set_key(kKey);
    for (size_t i = 0; i < arraysize(kSuggestionValues); ++i) {
      candidate = segment->add_candidate();
      candidate->value = kSuggestionValues[i];
      candidate->content_key = kKey;
    }
  }
  composer_->InsertCharacterPreedit(kKey);

  // Suggestion
  convertermock_->SetStartSuggestionWithComposer(&segments, true);
  EXPECT_TRUE(converter.Suggest(*composer_));
  EXPECT_TRUE(converter.IsCandidateListVisible());
  EXPECT_TRUE(converter.IsActive());
  {  // Check the candidate list
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    const commands::Candidates &candidates = output.candidates();
    EXPECT_EQ(commands::SUGGESTION, candidates.category());
    EXPECT_EQ(commands::SUGGESTION, output.all_candidate_words().category());
    EXPECT_EQ(arraysize(kSuggestionValues), candidates.size());
    for (size_t i = 0; i < arraysize(kSuggestionValues); ++i) {
      EXPECT_EQ(kSuggestionValues[i], candidates.candidate(i).value());
    }
  }

  segments.Clear();
  {  // Initialize mock segments for prediction (== expanding suggestion)
    segments.set_request_type(Segments::PREDICTION);
    Segment *segment = segments.add_segment();
    Segment::Candidate *candidate;
    segment->set_key(kKey);
    for (size_t i = 0; i < arraysize(kPredictionValues); ++i) {
      candidate = segment->add_candidate();
      candidate->value = kPredictionValues[i];
      candidate->content_key = kKey;
    }
  }
  // Expand suggestion candidate
  convertermock_->SetStartPredictionWithComposer(&segments, true);
  EXPECT_TRUE(converter.ExpandSuggestion(*composer_));
  EXPECT_TRUE(converter.IsCandidateListVisible());
  EXPECT_TRUE(converter.IsActive());
  {  // Check the candidate list
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    const commands::Candidates &candidates = output.candidates();
    EXPECT_EQ(commands::SUGGESTION, candidates.category());
    EXPECT_EQ(commands::SUGGESTION, output.all_candidate_words().category());
    // -1 is for duplicate entry.
    EXPECT_EQ(arraysize(kSuggestionValues) + arraysize(kPredictionValues) - 1,
              candidates.size());
    size_t i;
    for (i = 0; i < arraysize(kSuggestionValues); ++i) {
      EXPECT_EQ(kSuggestionValues[i], candidates.candidate(i).value());
    }

    // -1 is for duplicate entry.
    for (; i < candidates.size(); ++i) {
      size_t index_in_prediction = i - arraysize(kSuggestionValues);
      if (index_in_prediction >= kDupulicationIndex) {
        ++index_in_prediction;
      }
      EXPECT_EQ(kPredictionValues[index_in_prediction],
                candidates.candidate(i).value());
    }
  }
}

TEST_F(SessionConverterTest, AppendCandidateList) {
  SessionConverter converter(convertermock_.get());
  converter.state_ = SessionConverterInterface::CONVERSION;
  converter.operation_preferences_.use_cascading_window = true;
  Segments segments;

  {
    SetAiueo(&segments);
    FillT13Ns(&segments, composer_.get());

    converter.SetSegments(segments);
    converter.AppendCandidateList();
    const CandidateList &candidate_list = converter.GetCandidateList();
    // 3 == hiragana cand, katakana cand and sub candidate list.
    EXPECT_EQ(3, candidate_list.size());
    size_t sub_cand_list_count = 0;
    for (size_t i = 0; i < candidate_list.size(); ++i) {
      if (candidate_list.candidate(i).IsSubcandidateList()) {
        ++sub_cand_list_count;
      }
    }
    // Sub candidate list for T13N.
    EXPECT_EQ(1, sub_cand_list_count);
  }
  {
    Segment *segment = segments.mutable_conversion_segment(0);
    Segment::Candidate *candidate = segment->add_candidate();
    // "あいうえお_2"
    candidate->value =
      "\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86\xe3\x81\x88\xe3\x81\x8a_2";
    // New meta candidates.
    // They should be ignored.
    vector<Segment::Candidate> *meta_candidates =
        segment->mutable_meta_candidates();
    meta_candidates->clear();
    meta_candidates->resize(1);
    meta_candidates->at(0).Init();
    meta_candidates->at(0).value = "t13nValue";
    meta_candidates->at(0).content_value = "t13nValue";
    meta_candidates->at(0).content_key = segment->key();

    converter.SetSegments(segments);
    converter.AppendCandidateList();
    const CandidateList &candidate_list = converter.GetCandidateList();
    // 4 == hiragana cand, katakana cand, hiragana cand2
    // and sub candidate list.
    EXPECT_EQ(4, candidate_list.size());
    size_t sub_cand_list_count = 0;
    set<int> id_set;
    for (size_t i = 0; i < candidate_list.size(); ++i) {
      if (candidate_list.candidate(i).IsSubcandidateList()) {
        ++sub_cand_list_count;
      } else {
        // No duplicate ids are expected.
        int id = candidate_list.candidate(i).id();
        // EXPECT_EQ(iterator, iterator) might cause compile error in specific
        // environment.
        EXPECT_TRUE(id_set.end() == id_set.find(id));
        id_set.insert(id);
      }
    }
    // Sub candidate list shouldn't be duplicated.
    EXPECT_EQ(1, sub_cand_list_count);
  }
}

TEST_F(SessionConverterTest, ReloadConfig) {
  SessionConverter converter(convertermock_.get());
  Segments segments;
  SetAiueo(&segments);
  FillT13Ns(&segments, composer_.get());
  convertermock_->SetStartConversionWithComposer(&segments, true);

  composer_->InsertCharacterPreedit("aiueo");
  EXPECT_TRUE(converter.Convert(*composer_));
  converter.SetCandidateListVisible(true);

  {  // Set OperationPreferences
    const char kShortcut123456789[] = "123456789";
    OperationPreferences preferences;
    preferences.use_cascading_window = false;
    preferences.candidate_shortcuts = kShortcut123456789;
    converter.SetOperationPreferences(preferences);
    EXPECT_TRUE(converter.IsCandidateListVisible());
  }
  {  // Check the config update
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    const commands::Candidates &candidates = output.candidates();
    EXPECT_EQ("1", candidates.candidate(0).annotation().shortcut());
    EXPECT_EQ("2", candidates.candidate(1).annotation().shortcut());
  }

  {  // Set OperationPreferences #2
    OperationPreferences preferences;
    preferences.use_cascading_window = false;
    preferences.candidate_shortcuts = "";
    converter.SetOperationPreferences(preferences);
  }
  {  // Check the config update
    commands::Output output;
    converter.FillOutput(*composer_, &output);
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
  // "かまぼこの"
  const string kKamabokono =
    "\xe3\x81\x8b\xe3\x81\xbe\xe3\x81\xbc\xe3\x81\x93\xe3\x81\xae";
  // "いんぼう"
  const string kInbou =
    "\xe3\x81\x84\xe3\x82\x93\xe3\x81\xbc\xe3\x81\x86";
  composer_->InsertCharacterPreedit(kKamabokono + kInbou);

  FillT13Ns(&segments, composer_.get());
  convertermock_->SetStartConversionWithComposer(&segments, true);

  commands::Output output;

  EXPECT_TRUE(converter.Convert(*composer_));
  {
    ASSERT_TRUE(converter.IsActive());
    EXPECT_FALSE(converter.IsCandidateListVisible());

    output.Clear();
    converter.PopOutput(*composer_, &output);
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

  converter.CandidateNext(*composer_);
  {
    ASSERT_TRUE(converter.IsActive());
    EXPECT_TRUE(converter.IsCandidateListVisible());

    output.Clear();
    converter.PopOutput(*composer_, &output);
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
    converter.PopOutput(*composer_, &output);
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

TEST_F(SessionConverterTest, FillContext) {
  SessionConverter converter(convertermock_.get());
  Segments segments;

  // Set history segments.
  // "車で", "行く"
  const string kHistoryInput[] = {
      "\xE8\xBB\x8A\xE3\x81\xA7",
      "\xE8\xA1\x8C\xE3\x81\x8F"
  };
  for (size_t i = 0; i < arraysize(kHistoryInput); ++i) {
    Segment *segment = segments.add_segment();
    segment->set_segment_type(Segment::HISTORY);
    Segment::Candidate *candidate = segment->add_candidate();
    candidate->value = kHistoryInput[i];
  }
  convertermock_->SetFinishConversion(&segments, true);
  converter.CommitPreedit(*composer_);

  // FillContext must fill concatenation of values of history segments into
  // preceding_text.
  commands::Context context;
  converter.FillContext(&context);
  EXPECT_TRUE(context.has_preceding_text());
  EXPECT_EQ(kHistoryInput[0] + kHistoryInput[1], context.preceding_text());

  // If preceding text has been set already, do not overwrite it.
  // "自動車で行く"
  const char kPrecedingText[] = "\xE8\x87\xAA\xE5\x8B\x95\xE8\xBB\x8A"
                                "\xE3\x81\xA7\xE8\xA1\x8C\xE3\x81\x8F";
  context.set_preceding_text(kPrecedingText);
  converter.FillContext(&context);
  EXPECT_EQ(kPrecedingText, context.preceding_text());
}

TEST_F(SessionConverterTest, GetPreeditAndGetConversion) {
  Segments segments;

  Segment *segment;
  Segment::Candidate *candidate;

  segment = segments.add_segment();
  segment->set_segment_type(Segment::HISTORY);
  segment->set_key("[key:history1]");
  candidate = segment->add_candidate();
  candidate->content_key = "[content_key:history1-1]";
  candidate = segment->add_candidate();
  candidate->content_key = "[content_key:history1-2]";

  segment = segments.add_segment();
  segment->set_segment_type(Segment::FREE);
  segment->set_key("[key:conversion1]");
  candidate = segment->add_candidate();
  candidate->key = "[key:conversion1-1]";
  candidate->content_key = "[content_key:conversion1-1]";
  candidate->value = "[value:conversion1-1]";
  candidate = segment->add_candidate();
  candidate->key = "[key:conversion1-2]";
  candidate->content_key = "[content_key:conversion1-2]";
  candidate->value = "[value:conversion1-2]";
  {
    // PREDICTION
    SessionConverter converter(convertermock_.get());
    convertermock_->SetStartPredictionWithComposer(&segments, true);
    converter.Predict(*composer_);
    converter.CandidateNext(*composer_);
    string preedit;
    converter.GetPreedit(0, 1, &preedit);
    EXPECT_EQ("[content_key:conversion1-2]", preedit);
    string conversion;
    converter.GetConversion(0, 1, &conversion);
    EXPECT_EQ("[value:conversion1-2]", conversion);
  }
  {
    // SUGGESTION
    SessionConverter converter(convertermock_.get());
    convertermock_->SetStartSuggestionWithComposer(&segments, true);
    converter.Suggest(*composer_);
    string preedit;
    converter.GetPreedit(0, 1, &preedit);
    EXPECT_EQ("[content_key:conversion1-1]", preedit);
    string conversion;
    converter.GetConversion(0, 1, &conversion);
    EXPECT_EQ("[value:conversion1-1]", conversion);
  }
  segment = segments.add_segment();
  segment->set_segment_type(Segment::FREE);
  segment->set_key("[key:conversion2]");
  candidate = segment->add_candidate();
  candidate->key = "[key:conversion2-1]";
  candidate->content_key = "[content_key:conversion2-1]";
  candidate->value = "[value:conversion2-1]";
  candidate = segment->add_candidate();
  candidate->key = "[key:conversion2-2]";
  candidate->content_key = "[content_key:conversion2-2]";
  candidate->value = "[value:conversion2-2]";
  {
    // CONVERSION
    SessionConverter converter(convertermock_.get());
    convertermock_->SetStartConversionWithComposer(&segments, true);
    converter.Convert(*composer_);
    converter.CandidateNext(*composer_);
    string preedit;
    converter.GetPreedit(0, 2, &preedit);
    EXPECT_EQ("[key:conversion1][key:conversion2]", preedit);
    string conversion;
    converter.GetConversion(0, 2, &conversion);
    EXPECT_EQ("[value:conversion1-2][value:conversion2-1]", conversion);
  }
}

TEST_F(SessionConverterTest, GetAndSetSegments) {
  SessionConverter converter(convertermock_.get());
  Segments segments;

  // Set history segments.
  // "車で", "行く"
  const string kHistoryInput[] = {
      "\xE8\xBB\x8A\xE3\x81\xA7",
      "\xE8\xA1\x8C\xE3\x81\x8F"
  };
  for (size_t i = 0; i < arraysize(kHistoryInput); ++i) {
    Segment *segment = segments.add_segment();
    segment->set_segment_type(Segment::HISTORY);
    Segment::Candidate *candidate = segment->add_candidate();
    candidate->value = kHistoryInput[i];
  }
  convertermock_->SetFinishConversion(&segments, true);
  converter.CommitPreedit(*composer_);

  Segments src;
  converter.GetSegments(&src);
  ASSERT_EQ(2, src.history_segments_size());
  // "車で"
  EXPECT_EQ("\xE8\xBB\x8A\xE3\x81\xA7",
            src.history_segment(0).candidate(0).value);
  // "行く"
  EXPECT_EQ("\xE8\xA1\x8C\xE3\x81\x8F",
            src.history_segment(1).candidate(0).value);

  // "歩いて"
  src.mutable_history_segment(0)->mutable_candidate(0)->value
      = "\xE6\xAD\xA9\xE3\x81\x84\xE3\x81\xA6";
  Segment *segment = src.add_segment();
  segment->set_segment_type(Segment::FREE);
  Segment::Candidate *candidate = segment->add_candidate();
  candidate->value = "?";

  converter.SetSegments(src);

  Segments dest;
  converter.GetSegments(&dest);

  ASSERT_EQ(2, dest.history_segments_size());
  ASSERT_EQ(1, dest.conversion_segments_size());
  // "歩いて"
  EXPECT_EQ(src.history_segment(0).candidate(0).value,
            dest.history_segment(0).candidate(0).value);
  // "行く"
  EXPECT_EQ(src.history_segment(1).candidate(0).value,
            dest.history_segment(1).candidate(0).value);
  // "?"
  EXPECT_EQ(src.history_segment(2).candidate(0).value,
            dest.history_segment(2).candidate(0).value);
}

TEST_F(SessionConverterTest, CopyFrom) {
  SessionConverter src(convertermock_.get());

  // "かまぼこの"
  const string kKamabokono =
      "\xe3\x81\x8b\xe3\x81\xbe\xe3\x81\xbc\xe3\x81\x93\xe3\x81\xae";
  // "いんぼう"
  const string kInbou =
      "\xe3\x81\x84\xe3\x82\x93\xe3\x81\xbc\xe3\x81\x86";
  // "陰謀"
  const string kInbouKanji = "\xE9\x99\xB0\xE8\xAC\x80";
  const char *kShortcut = "987654321";

  {  // create source converter
    Segments segments;
    SetKamaboko(&segments);

    convertermock_->SetStartConversionWithComposer(&segments, true);

    OperationPreferences operation_preferences;
    operation_preferences.use_cascading_window = false;
    operation_preferences.candidate_shortcuts = kShortcut;
    src.SetOperationPreferences(operation_preferences);
  }

  {  // validation
    // Copy and validate
    SessionConverter dest(convertermock_.get());
    dest.CopyFrom(src);
    ExpectSameSessionConverter(src, dest);

    // Convert source
    EXPECT_TRUE(src.Convert(*composer_));
    EXPECT_TRUE(src.IsActive());

    // Convert destination and validate
    EXPECT_TRUE(dest.Convert(*composer_));
    ExpectSameSessionConverter(src, dest);

    // Copy converted and validate
    dest.CopyFrom(src);
    ExpectSameSessionConverter(src, dest);
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
  convertermock_->SetStartSuggestionWithComposer(&segments, true);
  EXPECT_TRUE(converter.Suggest(*composer_));
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
  convertermock_->SetStartSuggestionWithComposer(&segments, true);
  EXPECT_TRUE(converter.Suggest(*composer_));
  EXPECT_TRUE(converter.IsActive());

  {  // Check the candidate list
    commands::Output output;
    converter.FillOutput(*composer_, &output);
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

  FillT13Ns(&segments, composer_.get());
  convertermock_->SetStartConversionWithComposer(&segments, true);
  FillT13Ns(&resized_segments, composer_.get());
  convertermock_->SetResizeSegment1(&resized_segments, true);
  EXPECT_TRUE(converter.ConvertToTransliteration(*composer_,
                                                 transliteration::HALF_ASCII));
  EXPECT_FALSE(converter.IsCandidateListVisible());

  commands::Output output;
  converter.FillOutput(*composer_, &output);
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
  convertermock_->SetStartPredictionWithComposer(&segments, true);
  EXPECT_TRUE(converter.Predict(*composer_));
  EXPECT_TRUE(converter.IsActive());

  {  // Check the conversion
    commands::Output output;
    converter.FillOutput(*composer_, &output);
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
    converter.FillOutput(*composer_, &output);
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
  convertermock_->SetStartPredictionWithComposer(&segments, true);
  EXPECT_TRUE(converter.Predict(*composer_));
  EXPECT_TRUE(converter.IsActive());

  // Transliteration (as <F6>)
  segments.Clear();
  Segment *segment = segments.add_segment();
  segment->set_key("a");
  Segment::Candidate *candidate = segment->add_candidate();
  candidate->value = "a";

  FillT13Ns(&segments, composer_.get());
  convertermock_->SetStartConversionWithComposer(&segments, true);
  EXPECT_TRUE(converter.ConvertToTransliteration(*composer_,
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
    convertermock_->SetStartSuggestionWithComposer(&segments, true);
  }
  // Get suggestion
  composer_->InsertCharacterPreedit("aaaa");
  EXPECT_TRUE(converter.Suggest(*composer_));
  EXPECT_TRUE(converter.IsActive());

  {  // Initialize no suggest result triggered by "aaaaa".
    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key("aaaaa");
    convertermock_->SetStartSuggestionWithComposer(&segments, false);
  }
  // Hide suggestion
  composer_->InsertCharacterPreedit("a");
  EXPECT_FALSE(converter.Suggest(*composer_));
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
    convertermock_->SetStartPredictionWithComposer(&segments, false);
  }
  // Get prediction
  EXPECT_FALSE(converter.Predict(*composer_));
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
    convertermock_->SetStartPredictionWithComposer(&segments, true);
  }
  // Get prediction again
  EXPECT_TRUE(converter.Predict(*composer_));
  EXPECT_TRUE(converter.IsActive());

  {  // Check the conversion.
    commands::Output output;
    converter.FillOutput(*composer_, &output);
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
    convertermock_->SetStartPredictionWithComposer(&segments, false);
  }
  // Hide prediction
  converter.CandidateNext(*composer_);
  EXPECT_TRUE(converter.IsActive());

  {  // Check the conversion.
    commands::Output output;
    converter.FillOutput(*composer_, &output);
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

TEST_F(SessionConverterTest, GetReadingText) {
  SessionConverter converter(convertermock_.get());

  // "阿伊宇江於"
  const string kanji_aiueo =
      "\xe9\x98\xbf\xe4\xbc\x8a\xe5\xae\x87\xe6\xb1\x9f\xe6\x96\xbc";
  // "あいうえお"
  const string hiragana_aiueo =
      "\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86\xe3\x81\x88\xe3\x81\x8a";
  // Set up Segments for reverse conversion.
  Segments reverse_segments;
  Segment *segment;
  segment = reverse_segments.add_segment();
  segment->set_key(kanji_aiueo);
  Segment::Candidate *candidate;
  candidate = segment->add_candidate();
  // For reverse conversion, key is the original kanji string.
  candidate->key = kanji_aiueo;
  candidate->value = hiragana_aiueo;
  convertermock_->SetStartReverseConversion(&reverse_segments, true);
  // Set up Segments for forward conversion.
  Segments segments;
  segment = segments.add_segment();
  segment->set_key(hiragana_aiueo);
  candidate = segment->add_candidate();
  candidate->key = hiragana_aiueo;
  candidate->value = kanji_aiueo;
  convertermock_->SetStartConversionWithComposer(&segments, true);

  string reading;
  EXPECT_TRUE(converter.GetReadingText(kanji_aiueo, &reading));
  EXPECT_EQ(hiragana_aiueo, reading);
}

TEST_F(SessionConverterTest, ZeroQuerySuggestion) {
  SessionConverter converter(convertermock_.get());

  // Set up a mock suggestion result.
  Segments segments;
  segments.set_request_type(Segments::SUGGESTION);
  Segment *segment;
  segment = segments.add_segment();
  segment->set_key("");
  segment->add_candidate()->value = "search";
  segment->add_candidate()->value = "input";
  convertermock_->SetStartSuggestionWithComposer(&segments, true);

  EXPECT_TRUE(composer_->Empty());
  EXPECT_TRUE(converter.Suggest(*composer_));
  EXPECT_TRUE(converter.IsCandidateListVisible());
  EXPECT_TRUE(converter.IsActive());

  {  // Check the output
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_FALSE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    const commands::Candidates &candidates = output.candidates();
    EXPECT_EQ(2, candidates.size());
    EXPECT_EQ("search", candidates.candidate(0).value());
    EXPECT_EQ("input", candidates.candidate(1).value());
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
}  // namespace

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

TEST_F(SessionConverterTest, CommitHead) {
  SessionConverter converter(convertermock_.get());
  // "あいうえお"
  composer_->InsertCharacterPreedit(aiueo_);

  size_t committed_size;
  converter.CommitHead(1, *composer_, &committed_size);
  EXPECT_EQ(1, committed_size);
  composer_->DeleteAt(0);

  commands::Output output;
  converter.FillOutput(*composer_, &output);
  EXPECT_TRUE(output.has_result());
  EXPECT_FALSE(output.has_candidates());

  const commands::Result &result = output.result();
  EXPECT_EQ("\xe3\x81\x82", result.value());  // "あ"
  EXPECT_EQ("\xe3\x81\x82", result.key());    // "あ"
  string preedit;
  composer_->GetStringForPreedit(&preedit);
  // "いうえお"
  EXPECT_EQ("\xe3\x81\x84\xe3\x81\x86\xe3\x81\x88\xe3\x81\x8a", preedit);

  converter.CommitHead(3, *composer_, &committed_size);
  EXPECT_EQ(3, committed_size);
  composer_->DeleteAt(0);
  composer_->DeleteAt(0);
  composer_->DeleteAt(0);
  converter.FillOutput(*composer_, &output);
  EXPECT_TRUE(output.has_result());
  EXPECT_FALSE(output.has_candidates());

  const commands::Result &result2 = output.result();
  // "いうえ"
  EXPECT_EQ("\xe3\x81\x84\xe3\x81\x86\xe3\x81\x88", result2.value());
  // "いうえ"
  EXPECT_EQ("\xe3\x81\x84\xe3\x81\x86\xe3\x81\x88", result2.key());
  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ("\xe3\x81\x8a", preedit);         // "お"
}

TEST_F(SessionConverterTest, CommandCandidate) {
  SessionConverter converter(convertermock_.get());
  Segments segments;
  SetAiueo(&segments);
  FillT13Ns(&segments, composer_.get());
  // set COMMAND_CANDIDATE.
  segments.mutable_conversion_segment(0)->mutable_candidate(0)->attributes |=
      Segment::Candidate::COMMAND_CANDIDATE;
  convertermock_->SetStartConversionWithComposer(&segments, true);

  composer_->InsertCharacterPreedit(aiueo_);
  EXPECT_TRUE(converter.Convert(*composer_));

  converter.Commit();
  commands::Output output;
  converter.FillOutput(*composer_, &output);
  EXPECT_FALSE(output.has_result());
}

TEST_F(SessionConverterTest, CommandCandidateWithCommitCommands) {
  const string kKamabokono =
      "\xe3\x81\x8b\xe3\x81\xbe\xe3\x81\xbc\xe3\x81\x93\xe3\x81\xae";
  const string kInbou =
      "\xe3\x81\x84\xe3\x82\x93\xe3\x81\xbc\xe3\x81\x86";
  composer_->InsertCharacterPreedit(kKamabokono + kInbou);

  {
    // The first candidate is a command candidate, so
    // CommitFirstSegment resets all conversion.
    SessionConverter converter(convertermock_.get());
    Segments segments;
    SetKamaboko(&segments);
    segments.mutable_conversion_segment(0)->mutable_candidate(0)->attributes =
        Segment::Candidate::COMMAND_CANDIDATE;
    convertermock_->SetStartConversionWithComposer(&segments, true);
    converter.Convert(*composer_);

    size_t committed_size = 0;
    converter.CommitFirstSegment(&committed_size);
    EXPECT_EQ(0, committed_size);

    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(converter.IsActive());
    EXPECT_FALSE(output.has_result());
  }

  {
    // The second candidate is a command candidate, so
    // CommitFirstSegment commits all conversion.
    SessionConverter converter(convertermock_.get());
    Segments segments;
    SetKamaboko(&segments);

    segments.mutable_conversion_segment(1)->mutable_candidate(0)->attributes =
        Segment::Candidate::COMMAND_CANDIDATE;
    convertermock_->SetStartConversionWithComposer(&segments, true);
    converter.Convert(*composer_);

    size_t committed_size = 0;
    converter.CommitFirstSegment(&committed_size);
    EXPECT_EQ(Util::CharsLen(kKamabokono), committed_size);

    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_TRUE(converter.IsActive());
    EXPECT_TRUE(output.has_result());
  }

  {
    // The selected suggestion with Id is a command candidate.
    SessionConverter converter(convertermock_.get());
    Segments segments;
    SetAiueo(&segments);

    segments.mutable_conversion_segment(0)->mutable_candidate(0)->attributes =
        Segment::Candidate::COMMAND_CANDIDATE;
    convertermock_->SetStartSuggestionWithComposer(&segments, true);
    converter.Suggest(*composer_);

    size_t committed_size = 0;
    EXPECT_FALSE(converter.CommitSuggestionById(
        0, *composer_, &committed_size));
    EXPECT_EQ(0, committed_size);
  }

  {
    // The selected suggestion with Index is a command candidate.
    SessionConverter converter(convertermock_.get());
    Segments segments;
    SetAiueo(&segments);

    segments.mutable_conversion_segment(0)->mutable_candidate(1)->attributes =
        Segment::Candidate::COMMAND_CANDIDATE;
    convertermock_->SetStartSuggestionWithComposer(&segments, true);
    converter.Suggest(*composer_);

    size_t committed_size = 0;
    EXPECT_FALSE(converter.CommitSuggestionByIndex(
        1, *composer_, &committed_size));
    EXPECT_EQ(0, committed_size);
  }
}

}  // namespace session
}  // namespace mozc
