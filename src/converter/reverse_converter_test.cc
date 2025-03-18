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

#include "converter/reverse_converter.h"

#include <memory>
#include <string>

#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "converter/immutable_converter.h"
#include "converter/segments.h"
#include "data_manager/testing/mock_data_manager.h"
#include "engine/modules.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "protocol/user_dictionary_storage.pb.h"
#include "testing/gunit.h"

namespace mozc {

TEST(ReverseConverterTest, ReverseConvert) {
  std::unique_ptr<const engine::Modules> modules =
      engine::Modules::Create(std::make_unique<testing::MockDataManager>())
          .value();
  const ImmutableConverter immutable_converter(*modules);
  const converter::ReverseConverter reverse_converter(immutable_converter);

  constexpr absl::string_view kHonKanji = "本";
  constexpr absl::string_view kHonHiragana = "ほん";
  constexpr absl::string_view kMuryouKanji = "無料";
  constexpr absl::string_view kMuryouHiragana = "むりょう";
  constexpr absl::string_view kFullWidthSpace = "　";  // full-width space
  {
    // Test for single Kanji character.
    constexpr absl::string_view kInput = kHonKanji;
    Segments segments;
    segments.add_segment()->set_key(kInput);
    EXPECT_TRUE(reverse_converter.ReverseConvert(kInput, &segments));
    ASSERT_EQ(segments.segments_size(), 1);
    ASSERT_LE(1, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, kHonHiragana);
  }
  {
    // Test for multi-Kanji character.
    constexpr absl::string_view kInput = kMuryouKanji;
    Segments segments;
    segments.add_segment()->set_key(kInput);
    EXPECT_TRUE(reverse_converter.ReverseConvert(kInput, &segments));
    ASSERT_EQ(segments.segments_size(), 1);
    ASSERT_LE(1, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value,
              kMuryouHiragana);
  }
  {
    // Test for multi terms separated by a space.
    const std::string kInput = absl::StrCat(kHonKanji, " ", kMuryouKanji);
    Segments segments;
    segments.add_segment()->set_key(kInput);
    EXPECT_TRUE(reverse_converter.ReverseConvert(kInput, &segments));
    ASSERT_EQ(segments.segments_size(), 3);
    ASSERT_LT(0, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, kHonHiragana);
    ASSERT_LT(0, segments.conversion_segment(1).candidates_size());
    EXPECT_EQ(segments.conversion_segment(1).candidate(0).value, " ");
    ASSERT_LT(0, segments.conversion_segment(2).candidates_size());
    EXPECT_EQ(segments.conversion_segment(2).candidate(0).value,
              kMuryouHiragana);
  }
  {
    // Test for multi terms separated by multiple spaces.
    const std::string kInput = absl::StrCat(kHonKanji, "   ", kMuryouKanji);
    Segments segments;
    segments.add_segment()->set_key(kInput);
    EXPECT_TRUE(reverse_converter.ReverseConvert(kInput, &segments));
    ASSERT_EQ(segments.segments_size(), 3);
    ASSERT_LT(0, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, kHonHiragana);
    ASSERT_LT(0, segments.conversion_segment(1).candidates_size());
    EXPECT_EQ(segments.conversion_segment(1).candidate(0).value, "   ");
    ASSERT_LT(0, segments.conversion_segment(2).candidates_size());
    EXPECT_EQ(segments.conversion_segment(2).candidate(0).value,
              kMuryouHiragana);
  }
  {
    // Test for leading white spaces.
    const std::string kInput = absl::StrCat("  ", kHonKanji);
    Segments segments;
    segments.add_segment()->set_key(kInput);
    EXPECT_TRUE(reverse_converter.ReverseConvert(kInput, &segments));
    ASSERT_EQ(segments.segments_size(), 2);
    ASSERT_LT(0, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, "  ");
    ASSERT_LT(0, segments.conversion_segment(1).candidates_size());
    EXPECT_EQ(segments.conversion_segment(1).candidate(0).value, kHonHiragana);
  }
  {
    // Test for trailing white spaces.
    const std::string kInput = absl::StrCat(kMuryouKanji, "  ");
    Segments segments;
    segments.add_segment()->set_key(kInput);
    EXPECT_TRUE(reverse_converter.ReverseConvert(kInput, &segments));
    ASSERT_EQ(segments.segments_size(), 2);
    ASSERT_LT(0, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value,
              kMuryouHiragana);
    ASSERT_LT(0, segments.conversion_segment(1).candidates_size());
    EXPECT_EQ(segments.conversion_segment(1).candidate(0).value, "  ");
  }
  {
    // Test for multi terms separated by a full-width space.
    const std::string kInput =
        absl::StrCat(kHonKanji, kFullWidthSpace, kMuryouKanji);
    Segments segments;
    segments.add_segment()->set_key(kInput);
    EXPECT_TRUE(reverse_converter.ReverseConvert(kInput, &segments));
    ASSERT_EQ(segments.segments_size(), 3);
    ASSERT_LT(0, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, kHonHiragana);
    ASSERT_LT(0, segments.conversion_segment(1).candidates_size());
    EXPECT_EQ(segments.conversion_segment(1).candidate(0).value,
              kFullWidthSpace);
    ASSERT_LT(0, segments.conversion_segment(2).candidates_size());
    EXPECT_EQ(segments.conversion_segment(2).candidate(0).value,
              kMuryouHiragana);
  }
  {
    // Test for multi terms separated by two full-width spaces.
    const std::string kFullWidthSpace2 =
        absl::StrCat(kFullWidthSpace, kFullWidthSpace);
    const std::string kInput =
        absl::StrCat(kHonKanji, kFullWidthSpace2, kMuryouKanji);
    Segments segments;
    segments.add_segment()->set_key(kInput);
    EXPECT_TRUE(reverse_converter.ReverseConvert(kInput, &segments));
    ASSERT_EQ(segments.segments_size(), 3);
    ASSERT_LT(0, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, kHonHiragana);
    ASSERT_LT(0, segments.conversion_segment(1).candidates_size());
    EXPECT_EQ(segments.conversion_segment(1).candidate(0).value,
              kFullWidthSpace2);
    ASSERT_LT(0, segments.conversion_segment(2).candidates_size());
    EXPECT_EQ(segments.conversion_segment(2).candidate(0).value,
              kMuryouHiragana);
  }
  {
    // Test for multi terms separated by the mix of full- and half-width spaces.
    const std::string kFullWidthSpace2 = absl::StrCat(kFullWidthSpace, " ");
    const std::string kInput =
        absl::StrCat(kHonKanji, kFullWidthSpace2, kMuryouKanji);
    Segments segments;
    segments.add_segment()->set_key(kInput);
    EXPECT_TRUE(reverse_converter.ReverseConvert(kInput, &segments));
    ASSERT_EQ(segments.segments_size(), 3);
    ASSERT_LT(0, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, kHonHiragana);
    ASSERT_LT(0, segments.conversion_segment(1).candidates_size());
    EXPECT_EQ(segments.conversion_segment(1).candidate(0).value,
              kFullWidthSpace2);
    ASSERT_LT(0, segments.conversion_segment(2).candidates_size());
    EXPECT_EQ(segments.conversion_segment(2).candidate(0).value,
              kMuryouHiragana);
  }
  {
    // Test for math expressions; see b/9398304.
    constexpr absl::string_view kInputHalf = "365*24*60*60*1000=";
    Segments segments;
    segments.add_segment()->set_key(kInputHalf);
    EXPECT_TRUE(reverse_converter.ReverseConvert(kInputHalf, &segments));
    ASSERT_EQ(segments.segments_size(), 1);
    ASSERT_EQ(segments.conversion_segment(0).candidates_size(), 1);
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, kInputHalf);

    // Test for full-width characters.
    constexpr absl::string_view kInputFull =
        "３６５＊２４＊６０＊６０＊１０００＝";
    segments.Clear();
    segments.add_segment()->set_key(kInputFull);
    EXPECT_TRUE(reverse_converter.ReverseConvert(kInputFull, &segments));
    ASSERT_EQ(segments.segments_size(), 1);
    ASSERT_EQ(segments.conversion_segment(0).candidates_size(), 1);
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, kInputHalf);
  }
}

}  // namespace mozc
