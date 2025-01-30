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

#include "engine/candidate_list.h"

#include <memory>
#include <tuple>

#include "testing/gunit.h"

namespace mozc {
namespace engine {

class CandidateListTest : public testing::Test {
 protected:
  void SetUp() override {
    main_list_ = std::make_unique<CandidateList>(true);
    sub_list_2_ = std::make_unique<CandidateList>(true);
    sub_sub_list_2_1_ = std::make_unique<CandidateList>(false);

    main_list_->AddCandidate(0, "0");                           // main0
    main_list_->AddCandidate(1, "1");                           // main1
    sub_list_1_ = main_list_->AllocateSubCandidateList(false);  // main2
    main_list_->AddCandidate(2, "2");                           // main3
    main_list_->AddCandidate(3, "3");                           // main4
    main_list_->AddCandidate(4, "4");                           // main5
    main_list_->AddCandidate(5, "5");                           // main6
    main_list_->AddSubCandidateList(sub_list_2_.get());         // main7
    main_list_->AddCandidate(6, "6");                           // main8
    main_list_->AddCandidate(7, "7");                           // main9
    main_list_->AddCandidate(8, "8");                           // main10
    main_list_->AddCandidate(9, "9");                           // main11
    main_list_->AddCandidate(10, "10");                         // main12

    sub_list_1_->AddCandidate(-1, "-1");  // sub10
    sub_list_1_->AddCandidate(-2, "-2");  // sub11
    sub_list_1_->AddCandidate(-3, "-3");  // sub12
    sub_list_1_->AddCandidate(-4, "-4");  // sub13
    sub_list_1_->AddCandidate(-5, "-5");  // sub14

    sub_list_2_->AddSubCandidateList(sub_sub_list_2_1_.get());  // sub20
    sub_list_2_->AddCandidate(21, "21");                        // sub21
    sub_list_2_->AddCandidate(22, "22");                        // sub22
    sub_list_2_->AddCandidate(23, "23");                        // sub23
    sub_list_2_->AddCandidate(24, "24");                        // sub24
    sub_list_2_->AddCandidate(25, "25");                        // sub25

    sub_sub_list_2_1_->AddCandidate(210, "210");  // subsub210
    sub_sub_list_2_1_->AddCandidate(211, "211");  // subsub211
    sub_sub_list_2_1_->AddCandidate(212, "212");  // subsub212
  }

  std::unique_ptr<CandidateList> main_list_;
  // sub_list_1_ will be initialized on the fly.
  CandidateList *sub_list_1_;
  std::unique_ptr<CandidateList> sub_list_2_;
  std::unique_ptr<CandidateList> sub_sub_list_2_1_;
};

TEST_F(CandidateListTest, MoveToId) {
  // main0
  EXPECT_TRUE(main_list_->MoveToId(0));
  EXPECT_EQ(main_list_->focused_id(), 0);
  EXPECT_EQ(main_list_->focused_index(), 0);

  // main1
  EXPECT_TRUE(main_list_->MoveToId(1));
  EXPECT_EQ(main_list_->focused_id(), 1);
  EXPECT_EQ(main_list_->focused_index(), 1);

  // (main2, sub13)
  EXPECT_TRUE(main_list_->MoveToId(-4));
  EXPECT_EQ(main_list_->focused_id(), -4);
  EXPECT_EQ(main_list_->focused_index(), 2);
  EXPECT_EQ(sub_list_1_->focused_index(), 3);

  // (main7, sub20, subsub210)
  EXPECT_TRUE(main_list_->MoveToId(210));
  EXPECT_EQ(main_list_->focused_id(), 210);
  EXPECT_EQ(main_list_->focused_index(), 7);
  EXPECT_EQ(sub_list_2_->focused_index(), 0);
  EXPECT_EQ(sub_sub_list_2_1_->focused_index(), 0);

  // Invalid IDs
  EXPECT_FALSE(main_list_->MoveToId(13));
  EXPECT_FALSE(main_list_->MoveToId(-6));
  EXPECT_FALSE(main_list_->MoveToId(9999));
}

TEST_F(CandidateListTest, MoveNext) {
  // main0 -> main1
  EXPECT_TRUE(main_list_->MoveToId(0));
  EXPECT_TRUE(main_list_->MoveNext());
  EXPECT_EQ(main_list_->focused_id(), 1);
  EXPECT_EQ(main_list_->focused_index(), 1);

  // main1 -> (main2, sub10)
  EXPECT_TRUE(main_list_->MoveNext());
  EXPECT_EQ(main_list_->focused_id(), -1);
  EXPECT_EQ(main_list_->focused_index(), 2);
  EXPECT_EQ(sub_list_1_->focused_index(), 0);

  // (main2, sub10) -> (main2, sub11)
  EXPECT_TRUE(main_list_->MoveNext());
  EXPECT_EQ(main_list_->focused_id(), -2);
  EXPECT_EQ(main_list_->focused_index(), 2);
  EXPECT_EQ(sub_list_1_->focused_index(), 1);

  // (main2, sub14) -> main3: no rotation
  EXPECT_TRUE(main_list_->MoveToId(-5));
  EXPECT_TRUE(main_list_->MoveNext());
  EXPECT_EQ(main_list_->focused_id(), 2);
  EXPECT_EQ(main_list_->focused_index(), 3);

  // (main7, sub25) -> (main7, sub20, subsub210): rotation
  EXPECT_TRUE(main_list_->MoveToId(25));
  EXPECT_TRUE(main_list_->MoveNext());
  EXPECT_EQ(main_list_->focused_id(), 210);
  EXPECT_EQ(main_list_->focused_index(), 7);
  EXPECT_EQ(sub_list_2_->focused_index(), 0);
  EXPECT_EQ(sub_sub_list_2_1_->focused_index(), 0);

  // (main7, sub20, subsub210) -> (main7, sub20, subsub211)
  EXPECT_TRUE(main_list_->MoveNext());
  EXPECT_EQ(main_list_->focused_id(), 211);
  EXPECT_EQ(main_list_->focused_index(), 7);
  EXPECT_EQ(sub_list_2_->focused_index(), 0);
  EXPECT_EQ(sub_sub_list_2_1_->focused_index(), 1);
}

TEST_F(CandidateListTest, MovePrev) {
  // main1 -> main0
  EXPECT_TRUE(main_list_->MoveToId(1));
  EXPECT_TRUE(main_list_->MovePrev());
  EXPECT_EQ(main_list_->focused_id(), 0);
  EXPECT_EQ(main_list_->focused_index(), 0);

  // (main2, sub10) -> main1: no rotation
  EXPECT_TRUE(main_list_->MoveToId(-1));
  EXPECT_TRUE(main_list_->MovePrev());
  EXPECT_EQ(main_list_->focused_id(), 1);

  // (main7, sub20, subsub210) -> (main7, sub25)
  EXPECT_TRUE(main_list_->MoveToId(210));
  EXPECT_TRUE(main_list_->MovePrev());
  EXPECT_EQ(main_list_->focused_id(), 25);
}

TEST_F(CandidateListTest, MoveNextPage) {
  // main3 -> main9
  EXPECT_TRUE(main_list_->MoveToId(2));
  EXPECT_TRUE(main_list_->MoveNextPage());
  EXPECT_EQ(main_list_->focused_id(), 7);

  // main9 -> main0
  EXPECT_TRUE(main_list_->MoveNextPage());
  EXPECT_EQ(main_list_->focused_id(), 0);

  // (main2, sub10) -> main9: no ratation
  EXPECT_TRUE(main_list_->MoveToId(-1));
  EXPECT_TRUE(main_list_->MoveNextPage());
  EXPECT_EQ(main_list_->focused_id(), 7);

  // (main7, sub20, subsub210) -> (main7, sub20, subsub210)
  EXPECT_TRUE(main_list_->MoveToId(210));
  EXPECT_TRUE(main_list_->MoveNextPage());
  EXPECT_EQ(main_list_->focused_id(), 210);
}

TEST_F(CandidateListTest, MovePrevPage) {
  // main3 -> main9
  EXPECT_TRUE(main_list_->MoveToId(2));
  EXPECT_TRUE(main_list_->MovePrevPage());
  EXPECT_EQ(main_list_->focused_id(), 7);

  // main9 -> main0
  EXPECT_TRUE(main_list_->MovePrevPage());
  EXPECT_EQ(main_list_->focused_id(), 0);

  // (main2, sub10) -> main9: no ratation
  EXPECT_TRUE(main_list_->MoveToId(-1));
  EXPECT_TRUE(main_list_->MovePrevPage());
  EXPECT_EQ(main_list_->focused_id(), 7);

  // (main7, sub20, subsub210) -> (main7, sub20, subsub210)
  EXPECT_TRUE(main_list_->MoveToId(210));
  EXPECT_TRUE(main_list_->MovePrevPage());
  EXPECT_EQ(main_list_->focused_id(), 210);
}

TEST_F(CandidateListTest, MoveToPageIndex) {
  EXPECT_TRUE(main_list_->MoveToId(0));

  // main1
  EXPECT_TRUE(main_list_->MoveToPageIndex(1));
  EXPECT_EQ(main_list_->focused_id(), 1);

  // (main2, sub10)
  EXPECT_TRUE(main_list_->MoveToPageIndex(2));
  EXPECT_EQ(main_list_->focused_id(), -1);

  // main12
  EXPECT_TRUE(main_list_->MoveToId(10));

  // invalid index
  EXPECT_FALSE(main_list_->MoveToPageIndex(7));
}

TEST_F(CandidateListTest, Clear) {
  EXPECT_TRUE(main_list_->MoveToId(0));

  main_list_->Clear();
  EXPECT_FALSE(main_list_->MoveToId(0));
  EXPECT_EQ(main_list_->size(), 0);

  main_list_->AddCandidate(500, "500");
  main_list_->AddCandidate(501, "501");

  EXPECT_TRUE(main_list_->MoveNext());
  EXPECT_EQ(main_list_->focused_id(), 501);
  EXPECT_EQ(main_list_->focused_index(), 1);
}

TEST_F(CandidateListTest, Duplication) {
  CandidateList main_list(true);
  CandidateList sub_list(true);

  main_list.AddCandidate(0, "0");
  main_list.AddCandidate(1, "1");
  main_list.AddCandidate(2, "2");
  main_list.AddCandidate(3, "0");  // dup
  main_list.AddCandidate(4, "0");  // dup
  main_list.AddCandidate(5, "1");  // dup
  main_list.AddSubCandidateList(&sub_list);
  sub_list.AddCandidate(6, "0");  // not dup
  sub_list.AddCandidate(7, "7");
  sub_list.AddCandidate(8, "7");  // dup

  EXPECT_EQ(main_list.size(), 4);
  EXPECT_EQ(sub_list.size(), 2);

  main_list.MoveToId(3);
  EXPECT_EQ(main_list.focused_id(), 0);

  main_list.MoveToId(4);
  EXPECT_EQ(main_list.focused_id(), 0);

  main_list.MoveToId(5);
  EXPECT_EQ(main_list.focused_id(), 1);

  main_list.MoveToId(6);
  EXPECT_EQ(main_list.focused_id(), 6);

  main_list.MoveToId(8);
  EXPECT_EQ(main_list.focused_id(), 7);
}

TEST_F(CandidateListTest, FocusedId) {
  CandidateList empty_list(true);
  EXPECT_EQ(empty_list.focused_id(), 0);
}

TEST_F(CandidateListTest, SetPageSize) {
  EXPECT_EQ(main_list_->page_size(), 9);
  // Move to the 10th item.
  EXPECT_TRUE(main_list_->MoveToId(7));

  // Make sure the default values.
  EXPECT_EQ(main_list_->focused_id(), 7);
  EXPECT_EQ(main_list_->focused_index(), 9);
  auto [begin, end] = main_list_->GetPageRange(main_list_->focused_index());
  EXPECT_EQ(begin, 9);
  EXPECT_EQ(end, 12);  // The last index.

  // Change the page size.
  main_list_->set_page_size(11);
  // The id and index should not be changed.
  EXPECT_EQ(main_list_->focused_id(), 7);
  EXPECT_EQ(main_list_->focused_index(), 9);

  // The begin and end should be changed.
  std::tie(begin, end) = main_list_->GetPageRange(main_list_->focused_index());
  EXPECT_EQ(begin, 0);
  EXPECT_EQ(end, 10);
}

TEST_F(CandidateListTest, Attributes) {
  CandidateList main_list(true);

  main_list.AddCandidateWithAttributes(0, "hiragana", HIRAGANA);
  main_list.AddCandidateWithAttributes(1, "f_katakana", FULL_WIDTH | KATAKANA);
  main_list.AddCandidateWithAttributes(2, "h_ascii",
                                       (HALF_WIDTH | ASCII | LOWER));
  main_list.AddCandidateWithAttributes(3, "H_ASCII",
                                       (HALF_WIDTH | ASCII | UPPER));
  // dup entry
  main_list.AddCandidateWithAttributes(4, "h_ascii",
                                       (HALF_WIDTH | ASCII | LOWER));
  main_list.AddCandidateWithAttributes(5, "H_Ascii", (HALF_WIDTH | ASCII));
  main_list.AddCandidateWithAttributes(6, "f_ascii",
                                       (FULL_WIDTH | ASCII | LOWER));

  EXPECT_EQ(main_list.size(), 6);

  EXPECT_FALSE(main_list.MoveNextAttributes(HALF_WIDTH | KATAKANA));

  // h_ascii
  EXPECT_TRUE(main_list.MoveNextAttributes(HALF_WIDTH | ASCII));
  EXPECT_EQ(main_list.focused_id(), 2);
  EXPECT_EQ(main_list.focused_candidate().attributes(),
            (HALF_WIDTH | ASCII | LOWER));

  // H_ASCII
  EXPECT_TRUE(main_list.MoveNextAttributes(HALF_WIDTH | ASCII));
  EXPECT_EQ(main_list.focused_id(), 3);
  EXPECT_EQ(main_list.focused_candidate().attributes(),
            (HALF_WIDTH | ASCII | UPPER));

  // ID:4 (h_ascii) should be skipped because it's a dup.

  // H_Ascii
  EXPECT_TRUE(main_list.MoveNextAttributes(HALF_WIDTH | ASCII));
  EXPECT_EQ(main_list.focused_id(), 5);
  EXPECT_EQ(main_list.focused_candidate().attributes(), (HALF_WIDTH | ASCII));

  // h_ascii / Looped
  EXPECT_TRUE(main_list.MoveNextAttributes(HALF_WIDTH | ASCII));
  EXPECT_EQ(main_list.focused_id(), 2);
}

TEST_F(CandidateListTest, Attributes2) {
  CandidateList main_list(true);

  main_list.AddCandidate(0, "dvd");
  main_list.AddCandidate(1, "DVD");
  main_list.AddCandidateWithAttributes(2, "f_dvd", HIRAGANA);
  // id#3 is dup
  main_list.AddCandidateWithAttributes(3, "f_dvd", FULL_WIDTH | KATAKANA);
  main_list.AddCandidateWithAttributes(4, "h_dvd", (HALF_WIDTH | ASCII));
  // id#5 is dup
  main_list.AddCandidateWithAttributes(5, "h_dvd",
                                       (HALF_WIDTH | ASCII | LOWER));
  main_list.AddCandidateWithAttributes(6, "h_DVD",
                                       (HALF_WIDTH | ASCII | UPPER));
  main_list.AddCandidateWithAttributes(7, "h_Dvd",
                                       (HALF_WIDTH | ASCII | CAPITALIZED));
  // id#8 is dup
  main_list.AddCandidateWithAttributes(8, "f_dvd", (FULL_WIDTH | ASCII));
  // id#9 is dup
  main_list.AddCandidateWithAttributes(9, "f_dvd",
                                       (FULL_WIDTH | ASCII | LOWER));
  main_list.AddCandidateWithAttributes(10, "f_DVD",
                                       (FULL_WIDTH | ASCII | UPPER));
  main_list.AddCandidateWithAttributes(11, "f_Dvd",
                                       (FULL_WIDTH | ASCII | CAPITALIZED));
  // id#12 is dup
  main_list.AddCandidateWithAttributes(12, "h_dvd", HALF_WIDTH | KATAKANA);

  EXPECT_EQ(main_list.size(), 8);

  EXPECT_TRUE(main_list.MoveNextAttributes(FULL_WIDTH | ASCII));

  EXPECT_EQ(main_list.focused_id(), 2);
  EXPECT_EQ(main_list.focused_candidate().attributes(),
            (HIRAGANA | KATAKANA | FULL_WIDTH | ASCII | LOWER));

  EXPECT_TRUE(main_list.MoveNextAttributes(FULL_WIDTH | ASCII));
  EXPECT_EQ(main_list.focused_id(), 10);
  EXPECT_EQ(main_list.focused_candidate().attributes(),
            (FULL_WIDTH | ASCII | UPPER));

  EXPECT_TRUE(main_list.MoveNextAttributes(FULL_WIDTH | ASCII));
  EXPECT_EQ(main_list.focused_id(), 11);
  EXPECT_EQ(main_list.focused_candidate().attributes(),
            (FULL_WIDTH | ASCII | CAPITALIZED));

  EXPECT_TRUE(main_list.MoveNextAttributes(FULL_WIDTH | ASCII));
  EXPECT_EQ(main_list.focused_id(), 2);
}

TEST_F(CandidateListTest, AttributesWithSubList) {
  CandidateList main_list(true);
  main_list.AddCandidate(0, "kanji");

  CandidateList *sub_list = main_list.AllocateSubCandidateList(false);
  sub_list->AddCandidateWithAttributes(1, "hiragana", HIRAGANA);
  sub_list->AddCandidateWithAttributes(2, "f_katakana", FULL_WIDTH | KATAKANA);
  sub_list->AddCandidateWithAttributes(3, "h_ascii",
                                       (HALF_WIDTH | ASCII | LOWER));
  sub_list->AddCandidateWithAttributes(4, "H_ASCII",
                                       (HALF_WIDTH | ASCII | UPPER));
  sub_list->AddCandidateWithAttributes(5, "H_Ascii",
                                       (HALF_WIDTH | ASCII | CAPITALIZED));
  EXPECT_EQ(main_list.size(), 2);
  EXPECT_EQ(sub_list->size(), 5);

  // h_ascii
  EXPECT_TRUE(main_list.MoveNextAttributes(HALF_WIDTH | ASCII));
  EXPECT_EQ(main_list.focused_id(), 3);
  EXPECT_EQ(sub_list->focused_candidate().attributes(),
            (HALF_WIDTH | ASCII | LOWER));

  // H_ASCII
  EXPECT_TRUE(main_list.MoveNextAttributes(HALF_WIDTH | ASCII));
  EXPECT_EQ(main_list.focused_id(), 4);
  EXPECT_EQ(sub_list->focused_candidate().attributes(),
            (HALF_WIDTH | ASCII | UPPER));

  // H_Ascii
  EXPECT_TRUE(main_list.MoveNextAttributes(HALF_WIDTH | ASCII));
  EXPECT_EQ(main_list.focused_id(), 5);
  EXPECT_EQ(sub_list->focused_candidate().attributes(),
            (HALF_WIDTH | ASCII | CAPITALIZED));

  // h_ascii / Looped
  EXPECT_TRUE(main_list.MoveNextAttributes(HALF_WIDTH | ASCII));
  EXPECT_EQ(main_list.focused_id(), 3);
  EXPECT_EQ(sub_list->focused_candidate().attributes(),
            (HALF_WIDTH | ASCII | LOWER));
}

TEST_F(CandidateListTest, GetDeepestFocusedCandidate) {
  EXPECT_TRUE(main_list_->MoveToPageIndex(2));
  EXPECT_EQ(main_list_->focused_candidate().id(), 0);
  EXPECT_TRUE(main_list_->focused_candidate().HasSubcandidateList());

  const Candidate &deepest_candidate = main_list_->GetDeepestFocusedCandidate();
  EXPECT_EQ(deepest_candidate.id(), -1);
  EXPECT_FALSE(deepest_candidate.HasSubcandidateList());
}

TEST_F(CandidateListTest, NextAvailableId) {
  EXPECT_EQ(main_list_->next_available_id(), 213);
  EXPECT_EQ(sub_list_1_->next_available_id(), 0);
  EXPECT_EQ(sub_list_2_->next_available_id(), 213);
  EXPECT_EQ(sub_sub_list_2_1_->next_available_id(), 213);

  // Append duplicate candidate.
  // next_available_id should be incremented.
  sub_sub_list_2_1_->AddCandidate(213, "212");
  EXPECT_EQ(main_list_->next_available_id(), 214);
  EXPECT_EQ(sub_sub_list_2_1_->next_available_id(), 214);
}

}  // namespace engine
}  // namespace mozc
