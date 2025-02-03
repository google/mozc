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

#include "engine/engine_output.h"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string>

#include "absl/strings/string_view.h"
#include "base/text_normalizer.h"
#include "base/util.h"
#include "converter/segments.h"
#include "engine/candidate_list.h"
#include "protocol/candidate_window.pb.h"
#include "protocol/commands.pb.h"
#include "testing/gunit.h"

namespace mozc {
namespace engine {

struct DummySegment {
  const char *value;
  const int32_t usage_id;
  const char *usage_title;
  const char *usage_description;
};

void FillDummySegment(const DummySegment *dummy_segments, const size_t num,
                      Segment *segment, CandidateList *candidate_list) {
  for (size_t i = 0; i < num; ++i) {
    Segment::Candidate *cand = segment->push_back_candidate();
    candidate_list->AddCandidate(i, dummy_segments[i].value);
    cand->value = dummy_segments[i].value;
    cand->usage_id = dummy_segments[i].usage_id;
    cand->usage_title = dummy_segments[i].usage_title;
    cand->usage_description = dummy_segments[i].usage_description;
  }
}

TEST(EngineOutputTest, FillCandidate) {
  Segment segment;
  Candidate candidate;
  CandidateList candidate_list(true);
  commands::CandidateWindow_Candidate candidate_proto;

  const std::string kValue13 = "Value only";
  const std::string kValue42 = "The answer";
  const std::string kPrefix42 = "prefix";
  const std::string kSuffix42 = "suffix";
  const std::string kDescription42 = "description";
  const std::string kSubcandidateList = "Subcandidates";

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
  static constexpr int kFirstIdInSubList = -123;
  candidate_list.AddCandidate(kFirstIdInSubList, "minus 123");
  candidate_list.AddCandidate(-456, "minus 456");
  candidate_list.AddCandidate(-789, "minus 789");

  candidate.set_id(13);
  EngineOutput::FillCandidate(segment, candidate, &candidate_proto);
  EXPECT_EQ(candidate_proto.id(), 13);
  EXPECT_EQ(candidate_proto.value(), kValue13);
  EXPECT_FALSE(candidate_proto.has_annotation());

  candidate.Clear();
  candidate_proto.Clear();
  candidate.set_id(42);
  EngineOutput::FillCandidate(segment, candidate, &candidate_proto);
  EXPECT_EQ(candidate_proto.id(), 42);
  EXPECT_EQ(candidate_proto.value(), kValue42);
  EXPECT_TRUE(candidate_proto.has_annotation());
  EXPECT_EQ(candidate_proto.annotation().prefix(), kPrefix42);
  EXPECT_EQ(candidate_proto.annotation().suffix(), kSuffix42);
  EXPECT_EQ(candidate_proto.annotation().description(), kDescription42);

  candidate.Clear();
  candidate_proto.Clear();
  candidate.set_subcandidate_list(&candidate_list);
  EngineOutput::FillCandidate(segment, candidate, &candidate_proto);
  EXPECT_TRUE(candidate_proto.has_id());
  EXPECT_EQ(candidate_proto.id(), kFirstIdInSubList);
  EXPECT_EQ(candidate_proto.value(), kSubcandidateList);
  EXPECT_FALSE(candidate_proto.has_annotation());
}

TEST(EngineOutputTest, FillCandidateWindow) {
  Segment segment;
  CandidateList candidate_list(true);
  CandidateList subcandidate_list(true);
  commands::CandidateWindow candidate_window_proto;

  const std::string kSubcandidateList = "Subcandidates";
  const char *kValues[5] = {"0", "1", "2:sub0", "3:sub1", "4:sub2"};

  // Make 5 candidates
  for (size_t i = 0; i < 5; ++i) {
    segment.push_back_candidate()->value = kValues[i];
  }

  candidate_list.set_focused(true);
  candidate_list.set_page_size(9);
  candidate_list.AddCandidate(0, "0");
  candidate_list.AddCandidate(1, "1");
  candidate_list.AddSubCandidateList(&subcandidate_list);
  subcandidate_list.set_focused(true);
  subcandidate_list.set_name(kSubcandidateList);
  subcandidate_list.AddCandidate(2, "2");
  subcandidate_list.AddCandidate(3, "3");
  subcandidate_list.AddCandidate(4, "4");

  // Focused index = 0. page_size = 9.
  EngineOutput::FillCandidateWindow(segment, candidate_list, 0,
                                    &candidate_window_proto);
  EXPECT_EQ(candidate_window_proto.page_size(), 9);
  EXPECT_EQ(candidate_window_proto.candidate_size(), 3);
  EXPECT_EQ(candidate_window_proto.position(), 0);
  EXPECT_TRUE(candidate_window_proto.has_focused_index());
  EXPECT_EQ(candidate_window_proto.focused_index(), 0);
  EXPECT_EQ(candidate_window_proto.candidate(0).value(), kValues[0]);
  EXPECT_EQ(candidate_window_proto.candidate(1).value(), kValues[1]);
  EXPECT_EQ(candidate_window_proto.candidate(2).value(), kSubcandidateList);
  EXPECT_FALSE(candidate_window_proto.has_sub_candidate_window());

  // Focused index = 2 with a subcandidate list. page_size = 5.
  candidate_window_proto.Clear();
  candidate_list.MoveToId(3);
  candidate_list.set_page_size(5);
  EngineOutput::FillCandidateWindow(segment, candidate_list, 1,
                                    &candidate_window_proto);
  EXPECT_EQ(candidate_window_proto.page_size(), 5);
  EXPECT_EQ(candidate_window_proto.candidate_size(), 3);
  EXPECT_EQ(candidate_window_proto.position(), 1);
  EXPECT_TRUE(candidate_window_proto.has_focused_index());
  EXPECT_EQ(candidate_window_proto.focused_index(), 2);
  EXPECT_EQ(candidate_window_proto.candidate(0).value(), kValues[0]);
  EXPECT_EQ(candidate_window_proto.candidate(1).value(), kValues[1]);
  EXPECT_EQ(candidate_window_proto.candidate(2).value(), kSubcandidateList);
  EXPECT_EQ(candidate_window_proto.candidate(0).index(), 0);
  EXPECT_EQ(candidate_window_proto.candidate(1).index(), 1);
  EXPECT_EQ(candidate_window_proto.candidate(2).index(), 2);

  // Check the values of the subcandidate list.
  EXPECT_TRUE(candidate_window_proto.has_sub_candidate_window());
  EXPECT_EQ(candidate_window_proto.sub_candidate_window().candidate_size(), 3);
  EXPECT_EQ(candidate_window_proto.sub_candidate_window().position(), 2);
  EXPECT_TRUE(
      candidate_window_proto.sub_candidate_window().has_focused_index());
  EXPECT_EQ(candidate_window_proto.sub_candidate_window().focused_index(), 1);
  EXPECT_EQ(candidate_window_proto.sub_candidate_window().candidate(0).value(),
            kValues[2]);
  EXPECT_EQ(candidate_window_proto.sub_candidate_window().candidate(1).value(),
            kValues[3]);
  EXPECT_EQ(candidate_window_proto.sub_candidate_window().candidate(2).value(),
            kValues[4]);

  // Check focused_index.
  candidate_window_proto.Clear();
  candidate_list.set_focused(false);
  subcandidate_list.set_focused(true);
  EngineOutput::FillCandidateWindow(segment, candidate_list, 0,
                                    &candidate_window_proto);
  EXPECT_FALSE(candidate_window_proto.has_focused_index());
  EXPECT_TRUE(
      candidate_window_proto.sub_candidate_window().has_focused_index());

  candidate_window_proto.Clear();
  candidate_list.set_focused(false);
  subcandidate_list.set_focused(false);
  EngineOutput::FillCandidateWindow(segment, candidate_list, 0,
                                    &candidate_window_proto);
  EXPECT_FALSE(candidate_window_proto.has_focused_index());
  EXPECT_FALSE(
      candidate_window_proto.sub_candidate_window().has_focused_index());

  candidate_window_proto.Clear();
  candidate_list.set_focused(true);
  subcandidate_list.set_focused(false);
  EngineOutput::FillCandidateWindow(segment, candidate_list, 0,
                                    &candidate_window_proto);
  EXPECT_TRUE(candidate_window_proto.has_focused_index());
  EXPECT_FALSE(
      candidate_window_proto.sub_candidate_window().has_focused_index());
}

TEST(EngineOutputTest, FillAllCandidateWords) {
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
  const char *kNormalKey = "key";
  segment.set_key(kNormalKey);
  const char *kDescription = "desc";

  const char *kValues[7] = {"2",      "sub1_1",    "sub1_3",   "sub2_1",
                            "sub2_2", "subsub1_1", "subsub1_2"};
  constexpr size_t kValueSize = std::size(kValues);
  for (size_t i = 0; i < kValueSize; ++i) {
    Segment::Candidate *candidate = segment.push_back_candidate();
    candidate->content_key = kNormalKey;
    candidate->value = kValues[i];
    candidate->description = kDescription;
    for (size_t j = 0; j < i; ++j) {
      candidate->PushBackInnerSegmentBoundary(1, 1, 1, 1);
    }
  }
  // Set special key to ID:4 / Index:6
  const char *kSpecialKey = "Special Key";
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

  // Set focus to ID:5 / Index:1
  main_list.set_focused(true);
  sub1.set_focused(true);
  subsub1.set_focused(true);
  main_list.MoveToId(5);
  EXPECT_EQ(main_list.focused_id(), 5);
  EXPECT_EQ(main_list.focused_index(), 0);
  EXPECT_EQ(sub1.focused_index(), 1);
  EXPECT_EQ(subsub1.focused_index(), 0);
  // End of Initialization

  // Execute FillAllCandidateWords
  const commands::Category kCategory = commands::PREDICTION;
  EngineOutput::FillAllCandidateWords(segment, main_list, kCategory,
                                      &candidates_proto);

  // Varidation
  EXPECT_EQ(candidates_proto.focused_index(), 1);
  EXPECT_EQ(candidates_proto.category(), kCategory);
  EXPECT_EQ(candidates_proto.candidates_size(), kValueSize);

  EXPECT_EQ(candidates_proto.candidates(0).id(), 1);
  EXPECT_EQ(candidates_proto.candidates(1).id(), 5);
  EXPECT_EQ(candidates_proto.candidates(2).id(), 6);
  EXPECT_EQ(candidates_proto.candidates(3).id(), 2);
  EXPECT_EQ(candidates_proto.candidates(4).id(), 0);
  EXPECT_EQ(candidates_proto.candidates(5).id(), 3);
  EXPECT_EQ(candidates_proto.candidates(6).id(), 4);

  EXPECT_EQ(candidates_proto.candidates(0).index(), 0);
  EXPECT_EQ(candidates_proto.candidates(1).index(), 1);
  EXPECT_EQ(candidates_proto.candidates(2).index(), 2);
  EXPECT_EQ(candidates_proto.candidates(3).index(), 3);
  EXPECT_EQ(candidates_proto.candidates(4).index(), 4);
  EXPECT_EQ(candidates_proto.candidates(5).index(), 5);
  EXPECT_EQ(candidates_proto.candidates(6).index(), 6);

  EXPECT_FALSE(candidates_proto.candidates(0).has_key());
  EXPECT_FALSE(candidates_proto.candidates(1).has_key());
  EXPECT_FALSE(candidates_proto.candidates(2).has_key());
  EXPECT_FALSE(candidates_proto.candidates(3).has_key());
  EXPECT_FALSE(candidates_proto.candidates(4).has_key());
  EXPECT_FALSE(candidates_proto.candidates(5).has_key());
  EXPECT_TRUE(candidates_proto.candidates(6).has_key());
  EXPECT_EQ(candidates_proto.candidates(6).key(), kSpecialKey);

  EXPECT_EQ(candidates_proto.candidates(0).value(), kValues[1]);
  EXPECT_EQ(candidates_proto.candidates(1).value(), kValues[5]);
  EXPECT_EQ(candidates_proto.candidates(2).value(), kValues[6]);
  EXPECT_EQ(candidates_proto.candidates(3).value(), kValues[2]);
  EXPECT_EQ(candidates_proto.candidates(4).value(), kValues[0]);
  EXPECT_EQ(candidates_proto.candidates(5).value(), kValues[3]);
  EXPECT_EQ(candidates_proto.candidates(6).value(), kValues[4]);

  EXPECT_TRUE(candidates_proto.candidates(0).has_annotation());
  EXPECT_TRUE(candidates_proto.candidates(1).has_annotation());
  EXPECT_TRUE(candidates_proto.candidates(2).has_annotation());
  EXPECT_TRUE(candidates_proto.candidates(3).has_annotation());
  EXPECT_TRUE(candidates_proto.candidates(4).has_annotation());
  EXPECT_TRUE(candidates_proto.candidates(5).has_annotation());
  EXPECT_TRUE(candidates_proto.candidates(6).has_annotation());

  EXPECT_EQ(candidates_proto.candidates(0).num_segments_in_candidate(), 1);
  EXPECT_EQ(candidates_proto.candidates(1).num_segments_in_candidate(), 5);
  EXPECT_EQ(candidates_proto.candidates(2).num_segments_in_candidate(), 6);
  EXPECT_EQ(candidates_proto.candidates(3).num_segments_in_candidate(), 2);
  EXPECT_EQ(candidates_proto.candidates(4).num_segments_in_candidate(), 1);
  EXPECT_EQ(candidates_proto.candidates(5).num_segments_in_candidate(), 3);
  EXPECT_EQ(candidates_proto.candidates(6).num_segments_in_candidate(), 4);
}

TEST(EngineOutputTest, FillAllCandidateWords_Attributes) {
  CandidateList candidate_list(true);
  commands::CandidateList candidates_proto;

  // Initialize Segment
  Segment segment;
  const char *kKey = "key";
  segment.set_key(kKey);

  const char *kValues[5] = {"value_0", "value_1", "value_2", "value_3",
                            "value_4"};
  constexpr size_t kValueSize = std::size(kValues);
  for (size_t i = 0; i < kValueSize; ++i) {
    Segment::Candidate *candidate = segment.push_back_candidate();
    candidate->content_key = kKey;
    candidate->value = kValues[i];

    candidate_list.AddCandidate(i, kValues[i]);
  }

  segment.mutable_candidate(1)->attributes =
      Segment::Candidate::Attribute::USER_DICTIONARY;
  segment.mutable_candidate(2)->attributes =
      Segment::Candidate::Attribute::USER_HISTORY_PREDICTION |
      Segment::Candidate::Attribute::NO_VARIANTS_EXPANSION;
  segment.mutable_candidate(3)->attributes =
      Segment::Candidate::Attribute::SPELLING_CORRECTION |
      Segment::Candidate::Attribute::NO_EXTRA_DESCRIPTION;
  segment.mutable_candidate(4)->attributes =
      Segment::Candidate::Attribute::TYPING_CORRECTION |
      Segment::Candidate::Attribute::BEST_CANDIDATE;

  candidate_list.set_focused(true);
  candidate_list.MoveToId(0);
  EXPECT_EQ(candidate_list.focused_id(), 0);
  EXPECT_EQ(candidate_list.focused_index(), 0);
  // End of Initialization

  // Execute FillAllCandidateWords
  const commands::Category kCategory = commands::PREDICTION;
  EngineOutput::FillAllCandidateWords(segment, candidate_list, kCategory,
                                      &candidates_proto);

  // Varidation
  EXPECT_EQ(candidates_proto.focused_index(), 0);
  EXPECT_EQ(candidates_proto.category(), kCategory);
  EXPECT_EQ(candidates_proto.candidates_size(), kValueSize);

  EXPECT_EQ(candidates_proto.candidates(0).attributes_size(), 0);

  EXPECT_EQ(candidates_proto.candidates(1).attributes_size(), 1);
  EXPECT_EQ(candidates_proto.candidates(1).attributes(0),
            commands::CandidateAttribute::USER_DICTIONARY);

  EXPECT_EQ(candidates_proto.candidates(2).attributes_size(), 1);
  EXPECT_EQ(candidates_proto.candidates(2).attributes(0),
            commands::CandidateAttribute::USER_HISTORY);

  EXPECT_EQ(candidates_proto.candidates(3).attributes_size(), 1);
  EXPECT_EQ(candidates_proto.candidates(3).attributes(0),
            commands::CandidateAttribute::SPELLING_CORRECTION);

  EXPECT_EQ(1, candidates_proto.candidates(4).attributes_size());
  EXPECT_EQ(candidates_proto.candidates(4).attributes(0),
            commands::CandidateAttribute::TYPING_CORRECTION);
}

TEST(EngineOutputTest, ShouldShowUsages) {
  {
    Segment segment;
    CandidateList candidate_list(true);
    CandidateList sub(true);
    static const DummySegment dummy_segments[] = {
        {"val0", 0, "", ""}, {"val1", 0, "", ""}, {"val2", 0, "", ""},
        {"val3", 0, "", ""}, {"val4", 0, "", ""},
    };
    FillDummySegment(dummy_segments, 5, &segment, &candidate_list);
    candidate_list.AddSubCandidateList(&sub);
    candidate_list.set_focused(true);
    ASSERT_TRUE(candidate_list.MoveToId(0));
    ASSERT_FALSE(EngineOutput::ShouldShowUsages(segment, candidate_list));
  }
  {
    Segment segment;
    CandidateList candidate_list(true);
    CandidateList sub(true);
    static const DummySegment dummy_segments[] = {
        {"val0", 0, "", ""}, {"val1", 10, "title1", ""}, {"val2", 0, "", ""},
        {"val3", 0, "", ""}, {"val4", 0, "", ""},
    };
    FillDummySegment(dummy_segments, 5, &segment, &candidate_list);
    candidate_list.AddSubCandidateList(&sub);
    candidate_list.set_focused(true);
    ASSERT_TRUE(candidate_list.MoveToId(0));
    ASSERT_TRUE(EngineOutput::ShouldShowUsages(segment, candidate_list));
  }
  {
    Segment segment;
    CandidateList candidate_list(true);
    CandidateList sub(true);
    static const DummySegment dummy_segments[] = {
        {"val00", 10, "title00", ""}, {"val01", 0, "", ""},
        {"val02", 0, "", ""},         {"val03", 0, "", ""},
        {"val04", 0, "", ""},         {"val05", 0, "", ""},
        {"val06", 0, "", ""},         {"val07", 0, "", ""},
        {"val08", 0, "", ""},         {"val09", 0, "", ""},
        {"val10", 20, "title10", ""}, {"val11", 0, "", ""},
        {"val12", 0, "", ""},         {"val13", 30, "title13", ""},
        {"val14", 0, "", ""},         {"val15", 0, "", ""},
        {"val16", 0, "", ""},         {"val17", 0, "", ""},
        {"val18", 0, "", ""},         {"val19", 0, "", ""},
        {"val20", 0, "", ""},         {"val21", 0, "", ""},
        {"val22", 0, "", ""},         {"val23", 0, "", ""},
        {"val24", 0, "", ""},         {"val25", 0, "", ""},
        {"val26", 0, "", ""},         {"val27", 0, "", ""},
        {"val28", 0, "", ""},         {"val29", 0, "", ""},
    };
    FillDummySegment(dummy_segments, 30, &segment, &candidate_list);
    candidate_list.AddSubCandidateList(&sub);
    // pages of candidate_list:
    //  [00-08],[09-17],[18-26],[27-29]+subcandidate
    candidate_list.set_focused(true);
    ASSERT_TRUE(candidate_list.MoveToId(0));
    ASSERT_TRUE(EngineOutput::ShouldShowUsages(segment, candidate_list));
    ASSERT_TRUE(candidate_list.MoveToId(8));
    ASSERT_TRUE(EngineOutput::ShouldShowUsages(segment, candidate_list));
    ASSERT_TRUE(candidate_list.MoveToId(9));
    ASSERT_TRUE(EngineOutput::ShouldShowUsages(segment, candidate_list));
    ASSERT_TRUE(candidate_list.MoveToId(17));
    ASSERT_TRUE(EngineOutput::ShouldShowUsages(segment, candidate_list));
    ASSERT_TRUE(candidate_list.MoveToId(18));
    ASSERT_FALSE(EngineOutput::ShouldShowUsages(segment, candidate_list));
    ASSERT_TRUE(candidate_list.MoveToId(26));
    ASSERT_FALSE(EngineOutput::ShouldShowUsages(segment, candidate_list));
    ASSERT_TRUE(candidate_list.MoveToId(27));
    ASSERT_FALSE(EngineOutput::ShouldShowUsages(segment, candidate_list));
  }
}

TEST(EngineOutputTest, FillUsages) {
  Segment segment;
  CandidateList candidate_list(true);
  CandidateList sub(true);
  commands::CandidateWindow candidate_window_proto;
  static const DummySegment dummy_segments[] = {
      {"val00", 10, "title00", "desc00"},
      {"val01", 0, "", ""},
      {"val02", 0, "", ""},
      {"val03", 0, "", ""},
      {"val04", 20, "title04", "desc04"},
      {"val05", 0, "", ""},
      {"val06", 0, "", ""},
      {"val07", 0, "", ""},
      {"val08", 0, "", ""},
      {"val09", 0, "", ""},
      {"val10", 30, "title10", "desc10"},
      {"val11", 40, "title11", "desc11"},
      {"val12", 50, "title12", "desc12"},
      {"val13", 60, "title13", "desc13"},
      {"val14", 0, "", ""},
      {"val15", 0, "", ""},
      {"val16", 0, "", ""},
      {"val17", 0, "", ""},
      {"val18", 0, "", ""},
      {"val19", 100, "title100", "desc100"},
      {"val20", 110, "title110", "desc110"},
      {"val21", 100, "title100", "desc100"},
      {"val22", 110, "title110", "desc110"},
      {"val23", 0, "", ""},
      {"val24", 0, "", ""},
      {"val25", 0, "", ""},
      {"val26", 0, "", ""},
      {"val27", 0, "", ""},
      {"val28", 0, "", ""},
      {"val29", 0, "", ""},
  };
  FillDummySegment(dummy_segments, 30, &segment, &candidate_list);
  candidate_list.AddSubCandidateList(&sub);

  // pages of candidate_list:
  //  [00-08],[09-17],[18-26],[27-29]+subcandidate

  candidate_list.set_focused(true);

  candidate_list.MoveToId(2);
  candidate_window_proto.Clear();
  EngineOutput::FillUsages(segment, candidate_list, &candidate_window_proto);
  ASSERT_TRUE(candidate_window_proto.has_usages());
  // There is no focused usage.
  EXPECT_FALSE(candidate_window_proto.usages().has_focused_index());
  EXPECT_EQ(candidate_window_proto.usages().information_size(), 2);
  EXPECT_EQ(candidate_window_proto.usages().information(0).id(), 10);
  EXPECT_EQ(candidate_window_proto.usages().information(0).title(),
            dummy_segments[0].usage_title);
  EXPECT_EQ(candidate_window_proto.usages().information(0).description(),
            dummy_segments[0].usage_description);
  EXPECT_EQ(candidate_window_proto.usages().information(1).id(), 20);
  EXPECT_EQ(candidate_window_proto.usages().information(1).title(),
            dummy_segments[4].usage_title);
  EXPECT_EQ(candidate_window_proto.usages().information(1).description(),
            dummy_segments[4].usage_description);

  candidate_list.MoveToId(12);
  candidate_window_proto.Clear();
  EngineOutput::FillUsages(segment, candidate_list, &candidate_window_proto);
  ASSERT_TRUE(candidate_window_proto.has_usages());
  // Focused usage index is 20
  EXPECT_TRUE(candidate_window_proto.usages().has_focused_index());
  EXPECT_EQ(candidate_window_proto.usages().focused_index(), 2);
  EXPECT_EQ(candidate_window_proto.usages().information_size(), 4);
  EXPECT_EQ(candidate_window_proto.usages().information(0).id(), 30);
  EXPECT_EQ(candidate_window_proto.usages().information(0).title(),
            dummy_segments[10].usage_title);
  EXPECT_EQ(candidate_window_proto.usages().information(0).description(),
            dummy_segments[10].usage_description);
  EXPECT_EQ(candidate_window_proto.usages().information(1).id(), 40);
  EXPECT_EQ(candidate_window_proto.usages().information(1).title(),
            dummy_segments[11].usage_title);
  EXPECT_EQ(candidate_window_proto.usages().information(1).description(),
            dummy_segments[11].usage_description);
  EXPECT_EQ(candidate_window_proto.usages().information(2).id(), 50);
  EXPECT_EQ(candidate_window_proto.usages().information(2).title(),
            dummy_segments[12].usage_title);
  EXPECT_EQ(candidate_window_proto.usages().information(2).description(),
            dummy_segments[12].usage_description);
  EXPECT_EQ(candidate_window_proto.usages().information(3).id(), 60);
  EXPECT_EQ(candidate_window_proto.usages().information(3).title(),
            dummy_segments[13].usage_title);
  EXPECT_EQ(candidate_window_proto.usages().information(3).description(),
            dummy_segments[13].usage_description);

  candidate_list.MoveToId(19);
  candidate_window_proto.Clear();
  EngineOutput::FillUsages(segment, candidate_list, &candidate_window_proto);
  ASSERT_TRUE(candidate_window_proto.has_usages());
  EXPECT_TRUE(candidate_window_proto.usages().has_focused_index());
  EXPECT_EQ(candidate_window_proto.usages().focused_index(), 0);
  // usages(id:100) of "val19" and "val21" are merged
  EXPECT_EQ(candidate_window_proto.usages().information_size(), 2);
  EXPECT_EQ(candidate_window_proto.usages().information(0).id(), 100);
  EXPECT_EQ(candidate_window_proto.usages().information(0).title(),
            dummy_segments[19].usage_title);
  EXPECT_EQ(candidate_window_proto.usages().information(0).description(),
            dummy_segments[19].usage_description);
  EXPECT_EQ(candidate_window_proto.usages().information(1).id(), 110);
  EXPECT_EQ(candidate_window_proto.usages().information(1).title(),
            dummy_segments[20].usage_title);
  EXPECT_EQ(candidate_window_proto.usages().information(1).description(),
            dummy_segments[20].usage_description);

  candidate_list.MoveToId(20);
  candidate_window_proto.Clear();
  EngineOutput::FillUsages(segment, candidate_list, &candidate_window_proto);
  ASSERT_TRUE(candidate_window_proto.has_usages());
  EXPECT_TRUE(candidate_window_proto.usages().has_focused_index());
  EXPECT_EQ(candidate_window_proto.usages().focused_index(), 1);

  // usages(id:100) of "val19" and "val21" are merged
  candidate_list.MoveToId(21);
  candidate_window_proto.Clear();
  EngineOutput::FillUsages(segment, candidate_list, &candidate_window_proto);
  ASSERT_TRUE(candidate_window_proto.has_usages());
  EXPECT_TRUE(candidate_window_proto.usages().has_focused_index());
  EXPECT_EQ(candidate_window_proto.usages().focused_index(), 0);

  // usages(id:110) of "val20" and "val22" are merged
  candidate_list.MoveToId(22);
  candidate_window_proto.Clear();
  EngineOutput::FillUsages(segment, candidate_list, &candidate_window_proto);
  ASSERT_TRUE(candidate_window_proto.has_usages());
  EXPECT_TRUE(candidate_window_proto.usages().has_focused_index());
  EXPECT_EQ(candidate_window_proto.usages().focused_index(), 1);

  candidate_list.MoveToId(28);
  candidate_window_proto.Clear();
  EngineOutput::FillUsages(segment, candidate_list, &candidate_window_proto);
  ASSERT_FALSE(candidate_window_proto.has_usages());
}

TEST(EngineOutputTest, FillShortcuts) {
  const std::string kDigits = "123456789";

  commands::CandidateWindow candidate_window_proto1;
  for (size_t i = 0; i < 10; ++i) {
    candidate_window_proto1.add_candidate();
  }
  ASSERT_EQ(candidate_window_proto1.candidate_size(), 10);

  EngineOutput::FillShortcuts(kDigits, &candidate_window_proto1);
  EXPECT_EQ(candidate_window_proto1.candidate(0).annotation().shortcut(),
            kDigits.substr(0, 1));
  EXPECT_EQ(candidate_window_proto1.candidate(8).annotation().shortcut(),
            kDigits.substr(8, 1));
  EXPECT_FALSE(
      candidate_window_proto1.candidate(9).annotation().has_shortcut());

  commands::CandidateWindow candidate_window_proto2;
  for (size_t i = 0; i < 3; ++i) {
    candidate_window_proto2.add_candidate();
  }
  ASSERT_EQ(candidate_window_proto2.candidate_size(), 3);

  EngineOutput::FillShortcuts(kDigits, &candidate_window_proto2);
  EXPECT_EQ(candidate_window_proto2.candidate(0).annotation().shortcut(),
            kDigits.substr(0, 1));
  EXPECT_EQ(candidate_window_proto2.candidate(2).annotation().shortcut(),
            kDigits.substr(2, 1));
}

TEST(EngineOutputTest, FillFooter) {
  commands::CandidateWindow candidate_window;
  EXPECT_TRUE(
      EngineOutput::FillFooter(commands::SUGGESTION, &candidate_window));
  EXPECT_TRUE(candidate_window.has_footer());

#if defined(CHANNEL_DEV) && defined(GOOGLE_JAPANESE_INPUT_BUILD)
  EXPECT_FALSE(candidate_window.footer().has_label());
  EXPECT_TRUE(candidate_window.footer().has_sub_label());
  EXPECT_EQ(candidate_window.footer().sub_label().find("build "), 0);
#else   // CHANNEL_DEV && GOOGLE_JAPANESE_INPUT_BUILD
  EXPECT_TRUE(candidate_window.footer().has_label());
  EXPECT_FALSE(candidate_window.footer().has_sub_label());
  constexpr char kLabel[] = "Tabキーで選択";
  EXPECT_EQ(candidate_window.footer().label(), kLabel);
#endif  // CHANNEL_DEV && GOOGLE_JAPANESE_INPUT_BUILD

  EXPECT_FALSE(candidate_window.footer().index_visible());
  EXPECT_FALSE(candidate_window.footer().logo_visible());

  candidate_window.Clear();
  EXPECT_TRUE(
      EngineOutput::FillFooter(commands::PREDICTION, &candidate_window));
  EXPECT_TRUE(candidate_window.has_footer());
  EXPECT_FALSE(candidate_window.footer().has_label());
  EXPECT_TRUE(candidate_window.footer().index_visible());
  EXPECT_TRUE(candidate_window.footer().logo_visible());

  candidate_window.Clear();
  EXPECT_TRUE(
      EngineOutput::FillFooter(commands::CONVERSION, &candidate_window));
  EXPECT_TRUE(candidate_window.has_footer());
  EXPECT_FALSE(candidate_window.footer().has_label());
  EXPECT_TRUE(candidate_window.footer().index_visible());
  EXPECT_TRUE(candidate_window.footer().logo_visible());

  candidate_window.Clear();
  EXPECT_FALSE(
      EngineOutput::FillFooter(commands::TRANSLITERATION, &candidate_window));
  EXPECT_FALSE(candidate_window.has_footer());

  candidate_window.Clear();
  EXPECT_FALSE(EngineOutput::FillFooter(commands::USAGE, &candidate_window));
  EXPECT_FALSE(candidate_window.has_footer());

  candidate_window.Clear();
  for (int i = 0; i < 20; ++i) {
    commands::CandidateWindow::Candidate *c = candidate_window.add_candidate();
    c->set_index(i);
    c->set_value("dummy");
    c->set_id(i);
    // Candidates with even Id can be deleted.
    c->mutable_annotation()->set_deletable(i % 2 == 0);
  }
  for (int i = 0; i < 20; ++i) {
    candidate_window.clear_footer();
    candidate_window.set_focused_index(i);
    EXPECT_TRUE(
        EngineOutput::FillFooter(commands::PREDICTION, &candidate_window));
    if (i % 2 == 0) {
      ASSERT_TRUE(candidate_window.has_footer());
      ASSERT_TRUE(candidate_window.footer().has_label());
#if defined(__APPLE__)
      constexpr char kDeleteInstruction[] = "control+fn+deleteで履歴から削除";
#elif defined(OS_CHROMEOS)
      constexpr char kDeleteInstruction[] = "ctrl+alt+backspaceで履歴から削除";
#else   // !__APPLE__ && !OS_CHROMEOS
      constexpr char kDeleteInstruction[] = "Ctrl+Delで履歴から削除";
#endif  // __APPLE__ || OS_CHROMEOS
      EXPECT_EQ(candidate_window.footer().label(), kDeleteInstruction);
#if defined(CHANNEL_DEV) && defined(GOOGLE_JAPANESE_INPUT_BUILD)
    } else {
      EXPECT_FALSE(candidate_window.footer().has_label());
      EXPECT_TRUE(candidate_window.footer().has_sub_label());
      EXPECT_EQ(candidate_window.footer().sub_label().find("build "), 0);
#endif  // CHANNEL_DEV && GOOGLE_JAPANESE_INPUT_BUILD
    }
  }
}

TEST(EngineOutputTest, FillSubLabel) {
  commands::Footer footer;
  footer.set_label("to be deleted");
  EngineOutput::FillSubLabel(&footer);
  EXPECT_TRUE(footer.has_sub_label());
  EXPECT_FALSE(footer.has_label());
  EXPECT_GT(footer.sub_label().size(), 6);  // 6 == strlen("build ")
  // sub_label should start with "build ".
  EXPECT_EQ(footer.sub_label().find("build "), 0);
}

TEST(EngineOutputTest, AddSegment) {
  commands::Preedit preedit;
  int index = 0;
  {
    // "〜" is a character to be processed by TextNormalizer::NormalizeText
    const std::string kKey = "ゔ〜 preedit focused";
    const std::string kValue = "ゔ〜 PREEDIT FOCUSED";
    const int types = EngineOutput::PREEDIT | EngineOutput::FOCUSED;
    EXPECT_TRUE(EngineOutput::AddSegment(kKey, kValue, types, &preedit));
    EXPECT_EQ(preedit.segment_size(), index + 1);
    const commands::Preedit::Segment &segment = preedit.segment(index);

    const std::string normalized_key = TextNormalizer::NormalizeText(kKey);
    EXPECT_EQ(segment.key(), normalized_key);
    const std::string normalized_value = TextNormalizer::NormalizeText(kValue);
    EXPECT_EQ(segment.value(), normalized_value);
    EXPECT_EQ(segment.value_length(), Util::CharsLen(normalized_value));
    EXPECT_EQ(segment.annotation(), commands::Preedit::Segment::UNDERLINE);
    ++index;
  }

  {
    const std::string kKey = "ゔ〜 preedit";
    const std::string kValue = "ゔ〜 PREEDIT";
    const int types = EngineOutput::PREEDIT;
    EXPECT_TRUE(EngineOutput::AddSegment(kKey, kValue, types, &preedit));
    EXPECT_EQ(preedit.segment_size(), index + 1);
    const commands::Preedit::Segment &segment = preedit.segment(index);

    const std::string normalized_key = TextNormalizer::NormalizeText(kKey);
    EXPECT_EQ(segment.key(), normalized_key);
    const std::string normalized_value = TextNormalizer::NormalizeText(kValue);
    EXPECT_EQ(segment.value(), normalized_value);
    EXPECT_EQ(segment.value_length(), Util::CharsLen(normalized_value));
    EXPECT_EQ(segment.annotation(), commands::Preedit::Segment::UNDERLINE);
    ++index;
  }

  {
    const std::string kKey = "ゔ〜 conversion focused";
    const std::string kValue = "ゔ〜 CONVERSION FOCUSED";
    const int types = EngineOutput::CONVERSION | EngineOutput::FOCUSED;
    EXPECT_TRUE(EngineOutput::AddSegment(kKey, kValue, types, &preedit));
    EXPECT_EQ(preedit.segment_size(), index + 1);
    const commands::Preedit::Segment &segment = preedit.segment(index);

    const std::string normalized_key = TextNormalizer::NormalizeText(kKey);
    EXPECT_EQ(segment.key(), normalized_key);
    // Normalization is performed in Rewriter.
    absl::string_view normalized_value = kValue;
    EXPECT_EQ(segment.value(), normalized_value);
    EXPECT_EQ(segment.value_length(), Util::CharsLen(normalized_value));
    EXPECT_EQ(segment.annotation(), commands::Preedit::Segment::HIGHLIGHT);
    ++index;
  }

  {
    const std::string kKey = "ゔ〜 conversion";
    const std::string kValue = "ゔ〜 CONVERSION";
    const int types = EngineOutput::CONVERSION;
    EXPECT_TRUE(EngineOutput::AddSegment(kKey, kValue, types, &preedit));
    EXPECT_EQ(preedit.segment_size(), index + 1);
    const commands::Preedit::Segment &segment = preedit.segment(index);

    const std::string normalized_key = TextNormalizer::NormalizeText(kKey);
    EXPECT_EQ(segment.key(), normalized_key);
    // Normalization is performed in Rewriter.
    absl::string_view normalized_value = kValue;
    EXPECT_EQ(segment.value(), normalized_value);
    EXPECT_EQ(segment.value_length(), Util::CharsLen(normalized_value));
    EXPECT_EQ(segment.annotation(), commands::Preedit::Segment::UNDERLINE);
    ++index;
  }

  {
    const std::string kKey = "abc";
    const std::string kValue = "";  // empty value
    const int types = EngineOutput::CONVERSION;
    EXPECT_FALSE(EngineOutput::AddSegment(kKey, kValue, types, &preedit));
    EXPECT_EQ(preedit.segment_size(), index);
  }
}

TEST(EngineOutputTest, FillConversionResultWithoutNormalization) {
  constexpr char kInput[] = "ゔ";

  commands::Result result;
  EngineOutput::FillConversionResultWithoutNormalization(kInput, kInput,
                                                         &result);
  EXPECT_EQ(result.type(), commands::Result::STRING);
  EXPECT_EQ(result.key(), kInput);    // should not be normalized
  EXPECT_EQ(result.value(), kInput);  // should not be normalized
}

TEST(EngineOutputTest, FillConversionResult) {
  commands::Result result;
  EngineOutput::FillConversionResult("abc", "ABC", &result);
  EXPECT_EQ(result.type(), commands::Result::STRING);
  EXPECT_EQ(result.key(), "abc");
  EXPECT_EQ(result.value(), "ABC");
}

TEST(EngineOutputTest, FillCursorOffsetResult) {
  commands::Result result;
  EngineOutput::FillCursorOffsetResult(-1, &result);
  EXPECT_EQ(result.cursor_offset(), -1);
}

TEST(EngineOutputTest, FillPreeditResult) {
  commands::Result result;
  EngineOutput::FillPreeditResult("ABC", &result);
  EXPECT_EQ(result.type(), commands::Result::STRING);
  EXPECT_EQ(result.key(), "ABC");
  EXPECT_EQ(result.value(), "ABC");
}

TEST(EngineOutputTest, FillAllCandidateWords_NonFocused) {
  // Test against b/3059255
  // Even when no candidate was focused, all_candidate_words had focused_index.

  CandidateList main_list(true);
  commands::CandidateList candidates_proto;
  main_list.AddCandidate(0, "key");

  // Initialize Segment
  Segment segment;
  const char *kNormalKey = "key";
  segment.set_key(kNormalKey);

  Segment::Candidate *candidate = segment.push_back_candidate();
  candidate->content_key = "key";
  candidate->value = "value";

  {
    // Execute FillAllCandidateWords
    const commands::Category kCategory = commands::SUGGESTION;
    EngineOutput::FillAllCandidateWords(segment, main_list, kCategory,
                                        &candidates_proto);

    // Varidation
    EXPECT_FALSE(candidates_proto.has_focused_index());
  }
  {
    main_list.set_focused(true);
    // Execute FillAllCandidateWords
    // When the category is SUGGESTION, has_focused_index never return true in
    // real usage. This is just a test case.
    const commands::Category kCategory = commands::SUGGESTION;
    EngineOutput::FillAllCandidateWords(segment, main_list, kCategory,
                                        &candidates_proto);

    // Validation
    // If a candidate is focused, true is expected.
    EXPECT_TRUE(candidates_proto.has_focused_index());
  }
}

TEST(EngineOutputTest, FillRemovedCandidateWords) {
  commands::CandidateList candidates_proto;

  // Initialize Segment
  Segment segment;
  const char *kNormalKey = "key";
  segment.set_key(kNormalKey);

  Segment::Candidate candidate;
  candidate.content_key = "key";
  candidate.value = "value";
  segment.removed_candidates_for_debug_.push_back(candidate);

  // Execute FillAllCandidateWords
  EngineOutput::FillRemovedCandidates(segment, &candidates_proto);
}

}  // namespace engine
}  // namespace mozc
