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

#include "data_manager/data_manager.h"

#include <cstdint>
#include <memory>
#include <sstream>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/bits.h"
#include "base/container/serialized_string_array.h"
#include "base/version.h"
#include "data_manager/dataset_writer.h"
#include "protocol/segmenter_data.pb.h"
#include "testing/gunit.h"

namespace mozc {
namespace {

constexpr absl::string_view kTestMagic = "TEST_MAGIC\r\n";

std::string MakeStringArrayImage(absl::Span<const absl::string_view> strs) {
  std::unique_ptr<uint32_t[]> buf;
  const absl::string_view view =
      SerializedStringArray::SerializeToBuffer(strs, &buf);
  return std::string(view.data(), view.size());
}

std::string PackUint32Values(absl::Span<const uint32_t> values) {
  std::string out(values.size() * sizeof(uint32_t), '\0');
  auto iter = out.begin();
  for (const uint32_t v : values) {
    iter = StoreUnaligned<uint32_t>(v, iter);
  }
  return out;
}

std::string PackUint16Values(absl::Span<const uint16_t> values) {
  std::string out(values.size() * sizeof(uint16_t), '\0');
  auto iter = out.begin();
  for (const uint16_t v : values) {
    iter = StoreUnaligned<uint16_t>(v, iter);
  }
  return out;
}

std::string BuildDataSetImage(
    const absl::flat_hash_map<std::string, std::string>& overrides) {
  const std::string one_string_array = MakeStringArrayImage({"sample"});
  converter::SegmenterDataSizeInfo size_info;
  size_info.set_compressed_lsize(1);
  size_info.set_compressed_rsize(1);

  absl::flat_hash_map<std::string, std::string> entries = {
      {"pos_matcher", PackUint16Values({0})},
      {"user_pos_token", PackUint16Values({0, 0, 0, 0})},
      {"user_pos_string", one_string_array},
      {"conn", std::string(8, '\0')},
      {"dict", std::string(4, '\0')},
      {"sugg", PackUint32Values({0})},
      {"coll", std::string(4, '\0')},
      {"cols", std::string(4, '\0')},
      {"posg", std::string(4, '\0')},
      {"bdry", PackUint16Values({0, 0})},
      {"segmenter_sizeinfo", size_info.SerializeAsString()},
      {"segmenter_ltable", PackUint16Values({0})},
      {"segmenter_rtable", PackUint16Values({0})},
      {"segmenter_bitarray", std::string(4, '\0')},
      {"counter_suffix", one_string_array},
      {"reading_correction_value", one_string_array},
      {"reading_correction_error", one_string_array},
      {"reading_correction_correction", one_string_array},
      {"symbol_token", PackUint32Values({0, 0, 0, 0, 0, 0})},
      {"symbol_string", one_string_array},
      {"emoticon_token", PackUint32Values({0, 0, 0, 0, 0, 0})},
      {"emoticon_string", one_string_array},
      {"emoji_token", PackUint32Values({0, 0, 12, 0, 0, 0, 0})},
      {"emoji_string", one_string_array},
      {"single_kanji_token", PackUint32Values({0, 0})},
      {"single_kanji_string", one_string_array},
      {"single_kanji_variant_type", one_string_array},
      {"single_kanji_variant_token", PackUint32Values({0, 0, 0})},
      {"single_kanji_variant_string", one_string_array},
      {"single_kanji_noun_prefix_token", PackUint32Values({0, 0, 0, 0, 0, 0})},
      {"single_kanji_noun_prefix_string", one_string_array},
      {"zero_query_token_array", PackUint32Values({0, 0, 0, 0})},
      {"zero_query_string_array", one_string_array},
      {"zero_query_number_token_array", PackUint32Values({0, 0, 0, 0})},
      {"zero_query_number_string_array", one_string_array},
      {"usage_base_conjugation_suffix", PackUint32Values({0, 0})},
      {"usage_conjugation_suffix", PackUint32Values({0, 0})},
      {"usage_conjugation_index", PackUint32Values({0, 1})},
      {"usage_item_array", PackUint32Values({0, 0, 0, 0, 0})},
      {"usage_string_array", one_string_array},
      {"version", absl::StrCat(Version::GetMozcEngineVersion(), ".0.0")},
  };

  for (const auto& [key, value] : overrides) {
    entries[key] = value;
  }

  DataSetWriter writer(kTestMagic);
  for (const auto& [key, value] : entries) {
    writer.Add(key, 32, value);
  }
  std::stringstream out;
  writer.Finish(&out);
  return out.str();
}

TEST(DataManagerTest, ValidMinimalDataSetSucceeds) {
  const std::string image = BuildDataSetImage({});
  absl::StatusOr<std::unique_ptr<const DataManager>> dm =
      DataManager::CreateFromArray(image, kTestMagic);
  EXPECT_TRUE(dm.ok()) << dm.status();
}

TEST(DataManagerTest, RejectsInvalidUserPosContainer) {
  {
    const std::string image =
        BuildDataSetImage({{"user_pos_token", std::string(7, '\0')}});
    EXPECT_FALSE(DataManager::CreateFromArray(image, kTestMagic).ok());
    EXPECT_FALSE(
        DataManager::CreateUserPosManagerDataFromArray(image, kTestMagic).ok());
  }
  {
    const std::string image = BuildDataSetImage({{"user_pos_string", "abc"}});
    EXPECT_FALSE(DataManager::CreateFromArray(image, kTestMagic).ok());
    EXPECT_FALSE(
        DataManager::CreateUserPosManagerDataFromArray(image, kTestMagic).ok());
  }
}

TEST(DataManagerTest, RejectsInvalidStringArraySections) {
  constexpr absl::string_view kStringArraySections[] = {
      "counter_suffix",
      "reading_correction_value",
      "emoji_string",
      "single_kanji_string",
      "single_kanji_variant_type",
      "single_kanji_variant_string",
      "zero_query_string_array",
      "zero_query_number_string_array",
      "usage_string_array",
  };
  for (const absl::string_view section : kStringArraySections) {
    const std::string image = BuildDataSetImage({{std::string(section), "ab"}});
    absl::StatusOr<std::unique_ptr<const DataManager>> dm =
        DataManager::CreateFromArray(image, kTestMagic);
    EXPECT_FALSE(dm.ok()) << "Section accepted malformed string array: "
                          << section;
  }
}

TEST(DataManagerTest, RejectsInvalidSerializedDictionarySections) {
  // Out-of-bounds string index (1 >= string_array.size() == 1).
  const std::string bad_token = PackUint32Values({1, 0, 0, 0, 0, 0});
  constexpr absl::string_view kDictSections[] = {
      "symbol_token",
      "emoticon_token",
      "single_kanji_noun_prefix_token",
  };
  for (const absl::string_view section : kDictSections) {
    const std::string image =
        BuildDataSetImage({{std::string(section), bad_token}});
    absl::StatusOr<std::unique_ptr<const DataManager>> dm =
        DataManager::CreateFromArray(image, kTestMagic);
    EXPECT_FALSE(dm.ok()) << "Section accepted invalid dictionary tokens: "
                          << section;
  }
}

}  // namespace
}  // namespace mozc
