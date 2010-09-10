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
#include "converter/segments.h"
#include "session/commands.pb.h"
#include "session/internal/candidate_list.h"
#include "session/internal/session_normalizer.h"
#include "session/internal/session_output.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace session {

TEST(SessionOutputTest, FillCandidate) {
  Segment segment;
  Candidate candidate;
  CandidateList candidate_list(true);
  commands::Candidates_Candidate candidate_proto;

  const string kValue13 = "Value only";
  const string kValue42 = "The answer";
  const string kPrefix42 = "prefix";
  const string kSuffix42 = "suffix";
  const string kDescription42 = "description";
  const string kSubcandidateList = "Subcandidates";

  // Make 100 candidates
  for (size_t i = 0; i < 100; ++i) {
    segment.push_back_candidate();
  }
  segment.mutable_candidate(13)->value = kValue13;
  segment.mutable_candidate(42)->value = kValue42;
  segment.mutable_candidate(42)->prefix = kPrefix42;
  segment.mutable_candidate(42)->suffix = kSuffix42;
  segment.mutable_candidate(42)->description = kDescription42;
  candidate_list.set_name(kSubcandidateList);
  static const int kFirstIdInSubList = -123;
  candidate_list.AddCandidate(kFirstIdInSubList, "minus 123");
  candidate_list.AddCandidate(-456, "minus 456");
  candidate_list.AddCandidate(-789, "minus 789");

  candidate.set_id(13);
  SessionOutput::FillCandidate(segment, candidate, &candidate_proto);
  EXPECT_EQ(13, candidate_proto.id());
  EXPECT_EQ(kValue13, candidate_proto.value());
  EXPECT_FALSE(candidate_proto.has_annotation());

  candidate.Clear();
  candidate_proto.Clear();
  candidate.set_id(42);
  SessionOutput::FillCandidate(segment, candidate, &candidate_proto);
  EXPECT_EQ(42, candidate_proto.id());
  EXPECT_EQ(kValue42, candidate_proto.value());
  EXPECT_TRUE(candidate_proto.has_annotation());
  EXPECT_EQ(kPrefix42, candidate_proto.annotation().prefix());
  EXPECT_EQ(kSuffix42, candidate_proto.annotation().suffix());
  EXPECT_EQ(kDescription42, candidate_proto.annotation().description());

  candidate.Clear();
  candidate_proto.Clear();
  candidate.set_subcandidate_list(&candidate_list);
  SessionOutput::FillCandidate(segment, candidate, &candidate_proto);
  EXPECT_TRUE(candidate_proto.has_id());
  EXPECT_EQ(kFirstIdInSubList, candidate_proto.id());
  EXPECT_EQ(kSubcandidateList, candidate_proto.value());
  EXPECT_FALSE(candidate_proto.has_annotation());
}

TEST(SessionOutputTest, FillCandidates) {
  Segment segment;
  Candidate candidate;
  CandidateList candidate_list(true);
  CandidateList subcandidate_list(true);
  commands::Candidates candidates_proto;

  const string kSubcandidateList = "Subcandidates";
  const char* kValues[5] = {"0", "1", "2:sub0", "3:sub1", "4:sub2"};

  // Make 5 candidates
  for (size_t i = 0; i < 5; ++i) {
    segment.push_back_candidate()->value = kValues[i];
  }

  candidate_list.set_focused(true);
  candidate_list.AddCandidate(0, "0");
  candidate_list.AddCandidate(1, "1");
  candidate_list.AddSubCandidateList(&subcandidate_list);
  subcandidate_list.set_focused(true);
  subcandidate_list.set_name(kSubcandidateList);
  subcandidate_list.AddCandidate(2, "2");
  subcandidate_list.AddCandidate(3, "3");
  subcandidate_list.AddCandidate(4, "4");

  // Focused index = 0
  SessionOutput::FillCandidates(segment, candidate_list, 0, &candidates_proto);
  EXPECT_EQ(3, candidates_proto.candidate_size());
  EXPECT_EQ(0, candidates_proto.position());
  EXPECT_TRUE(candidates_proto.has_focused_index());
  EXPECT_EQ(0, candidates_proto.focused_index());
  EXPECT_EQ(kValues[0], candidates_proto.candidate(0).value());
  EXPECT_EQ(kValues[1], candidates_proto.candidate(1).value());
  EXPECT_EQ(kSubcandidateList, candidates_proto.candidate(2).value());
  EXPECT_FALSE(candidates_proto.has_subcandidates());

  // Focused index = 2 with a subcandidate list.
  candidates_proto.Clear();
  candidate_list.MoveToId(3);
  SessionOutput::FillCandidates(segment, candidate_list, 1, &candidates_proto);
  EXPECT_EQ(3, candidates_proto.candidate_size());
  EXPECT_EQ(1, candidates_proto.position());
  EXPECT_TRUE(candidates_proto.has_focused_index());
  EXPECT_EQ(2, candidates_proto.focused_index());
  EXPECT_EQ(kValues[0], candidates_proto.candidate(0).value());
  EXPECT_EQ(kValues[1], candidates_proto.candidate(1).value());
  EXPECT_EQ(kSubcandidateList, candidates_proto.candidate(2).value());
  EXPECT_EQ(0, candidates_proto.candidate(0).index());
  EXPECT_EQ(1, candidates_proto.candidate(1).index());
  EXPECT_EQ(2, candidates_proto.candidate(2).index());

  // Check the values of the subcandidate list.
  EXPECT_TRUE(candidates_proto.has_subcandidates());
  EXPECT_EQ(3, candidates_proto.subcandidates().candidate_size());
  EXPECT_EQ(2, candidates_proto.subcandidates().position());
  EXPECT_TRUE(candidates_proto.subcandidates().has_focused_index());
  EXPECT_EQ(1, candidates_proto.subcandidates().focused_index());
  EXPECT_EQ(kValues[2], candidates_proto.subcandidates().candidate(0).value());
  EXPECT_EQ(kValues[3], candidates_proto.subcandidates().candidate(1).value());
  EXPECT_EQ(kValues[4], candidates_proto.subcandidates().candidate(2).value());

  // Check focused_index.
  candidates_proto.Clear();
  candidate_list.set_focused(false);
  subcandidate_list.set_focused(true);
  SessionOutput::FillCandidates(segment, candidate_list, 0, &candidates_proto);
  EXPECT_FALSE(candidates_proto.has_focused_index());
  EXPECT_TRUE(candidates_proto.subcandidates().has_focused_index());

  candidates_proto.Clear();
  candidate_list.set_focused(false);
  subcandidate_list.set_focused(false);
  SessionOutput::FillCandidates(segment, candidate_list, 0, &candidates_proto);
  EXPECT_FALSE(candidates_proto.has_focused_index());
  EXPECT_FALSE(candidates_proto.subcandidates().has_focused_index());

  candidates_proto.Clear();
  candidate_list.set_focused(true);
  subcandidate_list.set_focused(false);
  SessionOutput::FillCandidates(segment, candidate_list, 0, &candidates_proto);
  EXPECT_TRUE(candidates_proto.has_focused_index());
  EXPECT_FALSE(candidates_proto.subcandidates().has_focused_index());
}

TEST(SessionOutputTest, FillAllCandidateWords) {
  // IDs are ordered by BFS.
  // 
  //  ID|Idx| Candidate list tree
  //   1| 0 | [1:[sub1_1,
  //   5| 1 |    sub1_2:[subsub1_1,
  //   6| 2 |            subsub1_2],
  //   2| 3 |    sub1_3],
  //   0| 4 |  2,
  //   3| 5 |  3:[sub2_1,
  //   4| 6 |     sub2_2]]
  CandidateList main_list(true);
  CandidateList sub1(true);
  CandidateList sub2(true);
  CandidateList subsub1(true);
  commands::CandidateList candidates_proto;

  // Initialize Segment
  Segment segment;
  const char* kNormalKey = "key";
  segment.set_key(kNormalKey);

  const char* kValues[7] =
    {"2", "sub1_1", "sub1_3", "sub2_1", "sub2_2", "subsub1_1", "subsub1_2"};
  const size_t kValueSize = arraysize(kValues);
  for (size_t i = 0; i < kValueSize; ++i) {
    Segment::Candidate *candidate = segment.push_back_candidate();
    candidate->content_key = kNormalKey;
    candidate->value = kValues[i];
  }
  // Set special key to ID:4 / Index:6
  const char* kSpecialKey = "Special Key";
  segment.mutable_candidate(4)->content_key = kSpecialKey;

  // Main
  main_list.AddSubCandidateList(&sub1);
  main_list.AddCandidate(0, kValues[0]);
  main_list.AddSubCandidateList(&sub2);

  // Sub1
  sub1.AddCandidate(1, kValues[1]);
  sub1.AddSubCandidateList(&subsub1);
  sub1.AddCandidate(2, kValues[2]);

  // Sub2
  sub2.AddCandidate(3, kValues[3]);
  sub2.AddCandidate(4, kValues[4]);

  // SubSub1
  subsub1.AddCandidate(5, kValues[5]);
  subsub1.AddCandidate(6, kValues[6]);

  // Set forcus to ID:5 / Index:1
  main_list.set_focused(true);
  sub1.set_focused(true);
  subsub1.set_focused(true);
  main_list.MoveToId(5);
  EXPECT_EQ(5, main_list.focused_id());
  EXPECT_EQ(0, main_list.focused_index());
  EXPECT_EQ(1, sub1.focused_index());
  EXPECT_EQ(0, subsub1.focused_index());
  // End of Initialization


  // Exexcute FillAllCandidateWords
  const commands::Category kCategory = commands::PREDICTION;
  SessionOutput::FillAllCandidateWords(segment, main_list, kCategory,
                                       &candidates_proto);

  // Varidation
  EXPECT_EQ(1, candidates_proto.focused_index());
  EXPECT_EQ(kCategory, candidates_proto.category());
  EXPECT_EQ(kValueSize, candidates_proto.candidates_size());

  EXPECT_EQ(1, candidates_proto.candidates(0).id());
  EXPECT_EQ(5, candidates_proto.candidates(1).id());
  EXPECT_EQ(6, candidates_proto.candidates(2).id());
  EXPECT_EQ(2, candidates_proto.candidates(3).id());
  EXPECT_EQ(0, candidates_proto.candidates(4).id());
  EXPECT_EQ(3, candidates_proto.candidates(5).id());
  EXPECT_EQ(4, candidates_proto.candidates(6).id());

  EXPECT_EQ(0, candidates_proto.candidates(0).index());
  EXPECT_EQ(1, candidates_proto.candidates(1).index());
  EXPECT_EQ(2, candidates_proto.candidates(2).index());
  EXPECT_EQ(3, candidates_proto.candidates(3).index());
  EXPECT_EQ(4, candidates_proto.candidates(4).index());
  EXPECT_EQ(5, candidates_proto.candidates(5).index());
  EXPECT_EQ(6, candidates_proto.candidates(6).index());

  EXPECT_FALSE(candidates_proto.candidates(0).has_key());
  EXPECT_FALSE(candidates_proto.candidates(1).has_key());
  EXPECT_FALSE(candidates_proto.candidates(2).has_key());
  EXPECT_FALSE(candidates_proto.candidates(3).has_key());
  EXPECT_FALSE(candidates_proto.candidates(4).has_key());
  EXPECT_FALSE(candidates_proto.candidates(5).has_key());
  EXPECT_TRUE(candidates_proto.candidates(6).has_key());
  EXPECT_EQ(kSpecialKey, candidates_proto.candidates(6).key());

  EXPECT_EQ(kValues[1], candidates_proto.candidates(0).value());
  EXPECT_EQ(kValues[5], candidates_proto.candidates(1).value());
  EXPECT_EQ(kValues[6], candidates_proto.candidates(2).value());
  EXPECT_EQ(kValues[2], candidates_proto.candidates(3).value());
  EXPECT_EQ(kValues[0], candidates_proto.candidates(4).value());
  EXPECT_EQ(kValues[3], candidates_proto.candidates(5).value());
  EXPECT_EQ(kValues[4], candidates_proto.candidates(6).value());

  EXPECT_FALSE(candidates_proto.candidates(0).has_annotation());
  EXPECT_FALSE(candidates_proto.candidates(1).has_annotation());
  EXPECT_FALSE(candidates_proto.candidates(2).has_annotation());
  EXPECT_FALSE(candidates_proto.candidates(3).has_annotation());
  EXPECT_FALSE(candidates_proto.candidates(4).has_annotation());
  EXPECT_FALSE(candidates_proto.candidates(5).has_annotation());
  EXPECT_FALSE(candidates_proto.candidates(6).has_annotation());
}

TEST(SessionOutputTest, FillUsages) {
  Segment segment;
  CandidateList candidate_list(true);
  commands::Candidates candidates_proto;

  // Make 5 candidates
  static const struct DummySegment {
    const char *value;
    const char *usage_title;
    const char *usage_description;
  } dummy_segments[] = {
    { "val0", "title0", "desc0" },
    { "val1", "", "" },
    { "val2", "title2", "desc2" },
    { "val3", "title3", "desc3" },
    { "val4", "", "" },
  };
  for (size_t i = 0; i < 5; ++i) {
    Segment::Candidate *cand = segment.push_back_candidate();
    candidate_list.AddCandidate(i, dummy_segments[i].value);
    cand->value = dummy_segments[i].value;
    cand->usage_title = dummy_segments[i].usage_title;
    cand->usage_description = dummy_segments[i].usage_description;
  }

  candidate_list.set_focused(true);
  candidate_list.MoveToId(2);

  SessionOutput::FillUsages(segment, candidate_list, &candidates_proto);
  ASSERT_TRUE(candidates_proto.has_usages());
  // The focused id is 2, it should be index:1 on the usages information list.
  EXPECT_EQ(1, candidates_proto.usages().focused_index());
  EXPECT_EQ(3, candidates_proto.usages().information_size());

  // Usage:0 should be dummy_segment:0
  EXPECT_EQ(0, candidates_proto.usages().information(0).id());
  EXPECT_EQ(dummy_segments[0].usage_title,
            candidates_proto.usages().information(0).title());
  EXPECT_EQ(dummy_segments[0].usage_description,
            candidates_proto.usages().information(0).description());

  // Usage:1 should be dummy_segment:2
  EXPECT_EQ(2, candidates_proto.usages().information(1).id());
  EXPECT_EQ(dummy_segments[2].usage_title,
            candidates_proto.usages().information(1).title());
  EXPECT_EQ(dummy_segments[2].usage_description,
            candidates_proto.usages().information(1).description());

  // Usage:2 should be dummy_segment:3
  EXPECT_EQ(3, candidates_proto.usages().information(2).id());
  EXPECT_EQ(dummy_segments[3].usage_title,
            candidates_proto.usages().information(2).title());
  EXPECT_EQ(dummy_segments[3].usage_description,
            candidates_proto.usages().information(2).description());
}


TEST(SessionOutputTest, FillShortcuts) {
  const string kDigits = "123456789";

  commands::Candidates candidates_proto1;
  for (size_t i = 0; i < 10; ++i) {
    candidates_proto1.add_candidate();
  }
  ASSERT_EQ(10, candidates_proto1.candidate_size());

  SessionOutput::FillShortcuts(kDigits, &candidates_proto1);
  EXPECT_EQ(kDigits.substr(0, 1),
            candidates_proto1.candidate(0).annotation().shortcut());
  EXPECT_EQ(kDigits.substr(8, 1),
            candidates_proto1.candidate(8).annotation().shortcut());
  EXPECT_FALSE(candidates_proto1.candidate(9).annotation().has_shortcut());

  commands::Candidates candidates_proto2;
  for (size_t i = 0; i < 3; ++i) {
    candidates_proto2.add_candidate();
  }
  ASSERT_EQ(3, candidates_proto2.candidate_size());

  SessionOutput::FillShortcuts(kDigits, &candidates_proto2);
  EXPECT_EQ(kDigits.substr(0, 1),
            candidates_proto2.candidate(0).annotation().shortcut());
  EXPECT_EQ(kDigits.substr(2, 1),
            candidates_proto2.candidate(2).annotation().shortcut());
}

TEST(SessionOutputTest, FillFooter) {
  commands::Candidates candidates;
  EXPECT_TRUE(SessionOutput::FillFooter(commands::SUGGESTION, &candidates));
  EXPECT_TRUE(candidates.has_footer());

#ifdef CHANNEL_DEV
  EXPECT_FALSE(candidates.footer().has_label());
  EXPECT_TRUE(candidates.footer().has_sub_label());
  EXPECT_EQ(0, candidates.footer().sub_label().find("build "));
#else  // CHANNEL_DEV
  EXPECT_TRUE(candidates.footer().has_label());
  EXPECT_FALSE(candidates.footer().has_sub_label());
  // "Tabキーで選択"
  const char kLabel[] = ("Tab\xE3\x82\xAD\xE3\x83\xBC\xE3\x81\xA7"
                         "\xE9\x81\xB8\xE6\x8A\x9E");
  EXPECT_EQ(kLabel, candidates.footer().label());
#endif  // CHANNEL_DEV

  EXPECT_FALSE(candidates.footer().index_visible());
  EXPECT_FALSE(candidates.footer().logo_visible());

  candidates.Clear();
  EXPECT_TRUE(SessionOutput::FillFooter(commands::PREDICTION, &candidates));
  EXPECT_TRUE(candidates.has_footer());
  EXPECT_FALSE(candidates.footer().has_label());
  EXPECT_TRUE(candidates.footer().index_visible());
  EXPECT_TRUE(candidates.footer().logo_visible());

  candidates.Clear();
  EXPECT_TRUE(SessionOutput::FillFooter(commands::CONVERSION, &candidates));
  EXPECT_TRUE(candidates.has_footer());
  EXPECT_FALSE(candidates.footer().has_label());
  EXPECT_TRUE(candidates.footer().index_visible());
  EXPECT_TRUE(candidates.footer().logo_visible());

  candidates.Clear();
  EXPECT_FALSE(SessionOutput::FillFooter(commands::TRANSLITERATION,
                                         &candidates));
  EXPECT_FALSE(candidates.has_footer());

  candidates.Clear();
  EXPECT_FALSE(SessionOutput::FillFooter(commands::USAGE, &candidates));
  EXPECT_FALSE(candidates.has_footer());
}

TEST(SessionOutputTest, FillSubLabel) {
  commands::Footer footer;
  footer.set_label("to be deleted");
  SessionOutput::FillSubLabel(&footer);
  EXPECT_TRUE(footer.has_sub_label());
  EXPECT_FALSE(footer.has_label());
  EXPECT_GT(footer.sub_label().size(), 6);  // 6 == strlen("build ")
  // sub_label should start with "build ".
  EXPECT_EQ(0, footer.sub_label().find("build "));
}

TEST(SessionOutputTest, AddSegment) {
  commands::Preedit preedit;
  int index = 0;
  {
    // "ゔ" and "〜" are characters to be processed by
    // SessionNormalizer::NormalizePreeditText and
    // SessionNormalizer::NormalizeCandidateText
    const string kKey = "ゔ〜 preedit focused";
    const string kValue = "ゔ〜 PREEDIT FOCUSED";
    const int types = SessionOutput::PREEDIT | SessionOutput::FOCUSED;
    EXPECT_TRUE(SessionOutput::AddSegment(kKey, kValue, types, &preedit));
    EXPECT_EQ(index + 1, preedit.segment_size());
    const commands::Preedit::Segment &segment = preedit.segment(index);

    string normalized_key;
    SessionNormalizer::NormalizePreeditText(kKey, &normalized_key);
    EXPECT_EQ(normalized_key, segment.key());
    string normalized_value;
    SessionNormalizer::NormalizePreeditText(kValue, &normalized_value);
    EXPECT_EQ(normalized_value, segment.value());
    EXPECT_EQ(Util::CharsLen(normalized_value), segment.value_length());
    EXPECT_EQ(commands::Preedit::Segment::UNDERLINE, segment.annotation());
    ++index;
  }

  {
    const string kKey = "ゔ〜 preedit";
    const string kValue = "ゔ〜 PREEDIT";
    const int types = SessionOutput::PREEDIT;
    EXPECT_TRUE(SessionOutput::AddSegment(kKey, kValue, types, &preedit));
    EXPECT_EQ(index + 1, preedit.segment_size());
    const commands::Preedit::Segment &segment = preedit.segment(index);

    string normalized_key;
    SessionNormalizer::NormalizePreeditText(kKey, &normalized_key);
    EXPECT_EQ(normalized_key, segment.key());
    string normalized_value;
    SessionNormalizer::NormalizePreeditText(kValue, &normalized_value);
    EXPECT_EQ(normalized_value, segment.value());
    EXPECT_EQ(Util::CharsLen(normalized_value), segment.value_length());
    EXPECT_EQ(commands::Preedit::Segment::UNDERLINE, segment.annotation());
    ++index;
  }

  {
    const string kKey = "ゔ〜 conversion focused";
    const string kValue = "ゔ〜 CONVERSION FOCUSED";
    const int types = SessionOutput::CONVERSION | SessionOutput::FOCUSED;
    EXPECT_TRUE(SessionOutput::AddSegment(kKey, kValue, types, &preedit));
    EXPECT_EQ(index + 1, preedit.segment_size());
    const commands::Preedit::Segment &segment = preedit.segment(index);

    string normalized_key;
    SessionNormalizer::NormalizePreeditText(kKey, &normalized_key);
    EXPECT_EQ(normalized_key, segment.key());
    string normalized_value;
    SessionNormalizer::NormalizeCandidateText(kValue, &normalized_value);
    EXPECT_EQ(normalized_value, segment.value());
    EXPECT_EQ(Util::CharsLen(normalized_value), segment.value_length());
    EXPECT_EQ(commands::Preedit::Segment::HIGHLIGHT, segment.annotation());
    ++index;
  }

  {
    const string kKey = "ゔ〜 conversion";
    const string kValue = "ゔ〜 CONVERSION";
    const int types = SessionOutput::CONVERSION;
    EXPECT_TRUE(SessionOutput::AddSegment(kKey, kValue, types, &preedit));
    EXPECT_EQ(index + 1, preedit.segment_size());
    const commands::Preedit::Segment &segment = preedit.segment(index);

    string normalized_key;
    SessionNormalizer::NormalizePreeditText(kKey, &normalized_key);
    EXPECT_EQ(normalized_key, segment.key());
    string normalized_value;
    SessionNormalizer::NormalizeCandidateText(kValue, &normalized_value);
    EXPECT_EQ(normalized_value, segment.value());
    EXPECT_EQ(Util::CharsLen(normalized_value), segment.value_length());
    EXPECT_EQ(commands::Preedit::Segment::UNDERLINE, segment.annotation());
    ++index;
  }

  {
    const string kKey = "abc";
    const string kValue = "";  // empty value
    const int types = SessionOutput::CONVERSION;
    EXPECT_FALSE(SessionOutput::AddSegment(kKey, kValue, types, &preedit));
    EXPECT_EQ(index, preedit.segment_size());
  }
}

TEST(SessionOutputTest, FillConversionResult) {
  commands::Result result;
  SessionOutput::FillConversionResult("abc", "ABC", &result);
  EXPECT_EQ(commands::Result::STRING, result.type());
  EXPECT_EQ("abc", result.key());
  EXPECT_EQ("ABC", result.value());
}

TEST(SessionOutputTest, FillPreeditResult) {
  commands::Result result;
  SessionOutput::FillPreeditResult("ABC", &result);
  EXPECT_EQ(commands::Result::STRING, result.type());
  EXPECT_EQ("ABC", result.key());
  EXPECT_EQ("ABC", result.value());
}

}  // namespace session
}  // namespace mozc
