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

#include "converter/history_reconstructor.h"

#include <cstdint>
#include <string>

#include "converter/segments.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/pos_matcher.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "protocol/user_dictionary_storage.pb.h"
#include "testing/gunit.h"

namespace mozc {
namespace converter {

TEST(HistoryReconstructorTest, GetLastConnectivePart) {
  const testing::MockDataManager data_manager;
  const dictionary::PosMatcher pos_matcher(data_manager.GetPosMatcherData());
  const converter::HistoryReconstructor reconstructor(pos_matcher);

  {
    std::string key;
    std::string value;
    uint16_t id = 0;
    EXPECT_FALSE(reconstructor.GetLastConnectivePart("", &key, &value, &id));
    EXPECT_FALSE(reconstructor.GetLastConnectivePart(" ", &key, &value, &id));
    EXPECT_FALSE(reconstructor.GetLastConnectivePart("  ", &key, &value, &id));
  }

  {
    std::string key;
    std::string value;
    uint16_t id = 0;
    EXPECT_TRUE(reconstructor.GetLastConnectivePart("a", &key, &value, &id));
    EXPECT_EQ(key, "a");
    EXPECT_EQ(value, "a");
    EXPECT_EQ(id, pos_matcher.GetUniqueNounId());

    EXPECT_TRUE(reconstructor.GetLastConnectivePart("a ", &key, &value, &id));
    EXPECT_EQ(key, "a");
    EXPECT_EQ(value, "a");

    EXPECT_FALSE(reconstructor.GetLastConnectivePart("a  ", &key, &value, &id));

    EXPECT_TRUE(reconstructor.GetLastConnectivePart("a ", &key, &value, &id));
    EXPECT_EQ(key, "a");
    EXPECT_EQ(value, "a");

    EXPECT_TRUE(reconstructor.GetLastConnectivePart("a10a", &key, &value, &id));
    EXPECT_EQ(key, "a");
    EXPECT_EQ(value, "a");

    EXPECT_TRUE(reconstructor.GetLastConnectivePart("ａ", &key, &value, &id));
    EXPECT_EQ(key, "a");
    EXPECT_EQ(value, "ａ");
  }

  {
    std::string key;
    std::string value;
    uint16_t id = 0;
    EXPECT_TRUE(reconstructor.GetLastConnectivePart("10", &key, &value, &id));
    EXPECT_EQ(key, "10");
    EXPECT_EQ(value, "10");
    EXPECT_EQ(id, pos_matcher.GetNumberId());

    EXPECT_TRUE(
        reconstructor.GetLastConnectivePart("10a10", &key, &value, &id));
    EXPECT_EQ(key, "10");
    EXPECT_EQ(value, "10");

    EXPECT_TRUE(reconstructor.GetLastConnectivePart("１０", &key, &value, &id));
    EXPECT_EQ(key, "10");
    EXPECT_EQ(value, "１０");
  }

  {
    std::string key;
    std::string value;
    uint16_t id = 0;
    EXPECT_FALSE(reconstructor.GetLastConnectivePart("あ", &key, &value, &id));
  }
}

TEST(HistoryReconstructorTest, ReconstructHistory) {
  const testing::MockDataManager data_manager;
  const dictionary::PosMatcher pos_matcher(data_manager.GetPosMatcherData());
  const converter::HistoryReconstructor reconstructor(pos_matcher);

  constexpr char kTen[] = "１０";

  Segments segments;
  EXPECT_TRUE(reconstructor.ReconstructHistory(kTen, &segments));
  EXPECT_EQ(segments.segments_size(), 1);
  const Segment &segment = segments.segment(0);
  EXPECT_EQ(segment.segment_type(), Segment::HISTORY);
  EXPECT_EQ(segment.key(), "10");
  EXPECT_EQ(segment.candidates_size(), 1);
  const Segment::Candidate &candidate = segment.candidate(0);
  EXPECT_EQ(candidate.attributes, Segment::Candidate::NO_LEARNING);
  EXPECT_EQ(candidate.content_key, "10");
  EXPECT_EQ(candidate.key, "10");
  EXPECT_EQ(candidate.content_value, kTen);
  EXPECT_EQ(candidate.value, kTen);
  EXPECT_NE(candidate.lid, 0);
  EXPECT_NE(candidate.rid, 0);
}

}  // namespace converter
}  // namespace mozc
