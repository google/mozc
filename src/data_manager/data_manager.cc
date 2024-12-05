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

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <optional>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/container/serialized_string_array.h"
#include "base/mmap.h"
#include "base/version.h"
#include "base/vlog.h"
#include "data_manager/dataset_reader.h"
#include "data_manager/serialized_dictionary.h"
#include "protocol/segmenter_data.pb.h"

namespace mozc {
namespace {

#ifdef GOOGLE_JAPANESE_INPUT_BUILD
constexpr absl::string_view kDataSetMagicNumber = "\xEFMOZC\r\n"
#else   // GOOGLE_JAPANESE_INPUT_BUILD
constexpr absl::string_view kDataSetMagicNumber = "\xEFMOZC\r\n";
#endif  // GOOGLE_JAPANESE_INPUT_BUILD

constexpr absl::string_view kDataSetMagicNumberOss = "\xEFMOZC\r\n";

DataManager::Status InitUserPosManagerDataFromReader(
    const DataSetReader &reader, absl::string_view *pos_matcher_data,
    absl::string_view *user_pos_token_array_data,
    absl::string_view *user_pos_string_array_data) {
  if (!reader.Get("pos_matcher", pos_matcher_data)) {
    LOG(ERROR) << "Cannot find POS matcher rule ID table";
    return DataManager::Status::DATA_MISSING;
  }
  if (!reader.Get("user_pos_token", user_pos_token_array_data)) {
    LOG(ERROR) << "Cannot find a user POS token array";
    return DataManager::Status::DATA_MISSING;
  }
  if (!reader.Get("user_pos_string", user_pos_string_array_data)) {
    LOG(ERROR) << "Cannot find a user POS string array";
    return DataManager::Status::DATA_MISSING;
  }
  if (user_pos_token_array_data->size() % 8 != 0 ||
      !SerializedStringArray::VerifyData(*user_pos_string_array_data)) {
    LOG(ERROR) << "User POS data is broken: token array data size = "
               << user_pos_token_array_data->size() << ", string array size = "
               << user_pos_string_array_data->size();
    return DataManager::Status::DATA_BROKEN;
  }
  return DataManager::Status::OK;
}

template <typename T>
absl::Span<const T> MakeSpanFromAlignedBuffer(const absl::string_view buf) {
  return absl::MakeSpan(std::launder(reinterpret_cast<const T *>(buf.data())),
                        buf.size() / sizeof(T));
}

}  // namespace

// static
std::string DataManager::StatusCodeToString(Status code) {
  std::string s;
  switch (code) {
    case Status::OK:
      s.assign("Status::OK");
      break;
    case Status::ENGINE_VERSION_MISMATCH:
      s.assign("Status::ENGINE_VERSION_MISMATCH");
      break;
    case Status::DATA_MISSING:
      s.assign("Status::DATA_MISSING");
      break;
    case Status::DATA_BROKEN:
      s.assign("Status::DATA_BROKEN");
      break;
    case Status::MMAP_FAILURE:
      s.assign("Status::MMAP_FAILURE");
      break;
    default:
      s.assign("Status::UNKNOWN");
      break;
  }
  absl::StrAppendFormat(&s, "(%d)", static_cast<int>(code));
  return s;
}

// static
absl::string_view DataManager::GetDataSetMagicNumber(absl::string_view type) {
  if (type == "oss") {
    return kDataSetMagicNumberOss;
  }
  return kDataSetMagicNumber;
}

absl::StatusOr<std::unique_ptr<DataManager>> DataManager::CreateFromFile(
    const std::string &path) {
  return CreateFromFile(path, kDataSetMagicNumber);
}

absl::StatusOr<std::unique_ptr<DataManager>> DataManager::CreateFromFile(
    const std::string &path, absl::string_view magic) {
  auto data_manager = std::make_unique<DataManager>();
  const Status status = data_manager->InitFromFile(path, magic);
  if (status != DataManager::Status::OK) {
    return absl::InternalError(
        absl::StrFormat("%s: Failed to initialize a data manager from %s",
                        DataManager::StatusCodeToString(status), path));
  }
  return data_manager;
}

DataManager::Status DataManager::InitFromArray(absl::string_view array) {
  return InitFromArray(array, kDataSetMagicNumber);
}

DataManager::Status DataManager::InitFromArray(absl::string_view array,
                                               absl::string_view magic) {
  DataSetReader reader;
  if (!reader.Init(array, magic)) {
    LOG(ERROR) << "Binary data of size " << array.size() << " is broken";
    return DataManager::Status::DATA_BROKEN;
  }
  return InitFromReader(reader);
}

DataManager::Status DataManager::InitFromArray(absl::string_view array,
                                               size_t magic_length) {
  DataSetReader reader;
  if (!reader.Init(array, magic_length)) {
    LOG(ERROR) << "Binary data of size " << array.size() << " is broken";
    return DataManager::Status::DATA_BROKEN;
  }
  return InitFromReader(reader);
}

DataManager::Status DataManager::InitFromReader(const DataSetReader &reader) {
  const Status status = InitUserPosManagerDataFromReader(
      reader, &pos_matcher_data_, &user_pos_token_array_data_,
      &user_pos_string_array_data_);
  if (status != Status::OK) {
    LOG(ERROR) << "User POS manager data is broken";
    return status;
  }
  if (!reader.Get("conn", &connection_data_)) {
    LOG(ERROR) << "Cannot find a connection data";
    return Status::DATA_MISSING;
  }
  if (!reader.Get("dict", &dictionary_data_)) {
    LOG(ERROR) << "Cannot find a dictionary data";
    return Status::DATA_MISSING;
  }
  if (!reader.Get("sugg", &suggestion_filter_data_)) {
    LOG(ERROR) << "Cannot find a suggestion filter data";
    return Status::DATA_MISSING;
  }
  if (!reader.Get("coll", &collocation_data_)) {
    LOG(ERROR) << "Cannot find a collocation data";
    return Status::DATA_MISSING;
  }
  if (!reader.Get("cols", &collocation_suppression_data_)) {
    LOG(ERROR) << "Cannot find a collocation suprression data";
    return Status::DATA_MISSING;
  }
  if (!reader.Get("posg", &pos_group_data_)) {
    LOG(ERROR) << "Cannot find a POS group data";
    return Status::DATA_MISSING;
  }
  if (!reader.Get("bdry", &boundary_data_)) {
    LOG(ERROR) << "Cannot find a boundary data";
    return Status::DATA_MISSING;
  }
  {
    absl::string_view memblock;
    if (!reader.Get("segmenter_sizeinfo", &memblock)) {
      LOG(ERROR) << "Cannot find a segmenter size info";
      return Status::DATA_MISSING;
    }
    converter::SegmenterDataSizeInfo sizeinfo;
    if (!sizeinfo.ParseFromArray(memblock.data(), memblock.size())) {
      LOG(ERROR) << "Failed to parse SegmenterDataSizeInfo";
      return Status::DATA_BROKEN;
    }
    segmenter_compressed_lsize_ = sizeinfo.compressed_lsize();
    segmenter_compressed_rsize_ = sizeinfo.compressed_rsize();
  }
  if (!reader.Get("segmenter_ltable", &segmenter_ltable_)) {
    LOG(ERROR) << "Cannot find a segmenter ltable";
    return Status::DATA_MISSING;
  }
  if (!reader.Get("segmenter_rtable", &segmenter_rtable_)) {
    LOG(ERROR) << "Cannot find a segmenter rtable";
    return Status::DATA_MISSING;
  }
  if (!reader.Get("segmenter_bitarray", &segmenter_bitarray_)) {
    LOG(ERROR) << "Cannot find a segmenter bit-array";
    return Status::DATA_MISSING;
  }
  if (!reader.Get("counter_suffix", &counter_suffix_data_)) {
    LOG(ERROR) << "Cannot find a counter suffix data";
    return Status::DATA_MISSING;
  }
  if (!SerializedStringArray::VerifyData(counter_suffix_data_)) {
    LOG(ERROR) << "Counter suffix string array is broken";
    return Status::DATA_MISSING;
  }
  if (!reader.Get("suffix_key", &suffix_key_array_data_)) {
    LOG(ERROR) << "Cannot find a suffix key array";
    return Status::DATA_MISSING;
  }
  if (!reader.Get("suffix_value", &suffix_value_array_data_)) {
    LOG(ERROR) << "Cannot find a suffix value array";
    return Status::DATA_MISSING;
  }
  if (!reader.Get("suffix_token", &suffix_token_array_data_)) {
    LOG(ERROR) << "Cannot find a suffix token array";
    return Status::DATA_MISSING;
  }
  {
    SerializedStringArray suffix_keys, suffix_values;
    if (!suffix_keys.Init(suffix_key_array_data_) ||
        !suffix_values.Init(suffix_value_array_data_) ||
        suffix_keys.size() != suffix_values.size() ||
        // Suffix token array is an array of triple (lid, rid, cost) of
        // uint32_t, so it contains N = 3 * |suffix_keys.size()| uint32_t
        // elements. Therefore, its byte length must be 4 * N bytes.
        suffix_token_array_data_.size() != 4 * 3 * suffix_keys.size()) {
      LOG(ERROR) << "Suffix dictionary data is broken";
      return Status::DATA_BROKEN;
    }
  }
  if (!reader.Get("reading_correction_value",
                  &reading_correction_value_array_data_)) {
    LOG(ERROR) << "Cannot find reading correction value array";
    return Status::DATA_MISSING;
  }
  if (!reader.Get("reading_correction_error",
                  &reading_correction_error_array_data_)) {
    LOG(ERROR) << "Cannot find reading correction error array";
    return Status::DATA_MISSING;
  }
  if (!reader.Get("reading_correction_correction",
                  &reading_correction_correction_array_data_)) {
    LOG(ERROR) << "Cannot find reading correction correction array";
    return Status::DATA_MISSING;
  }
  {
    SerializedStringArray value_array, error_array, correction_array;
    if (!value_array.Init(reading_correction_value_array_data_) ||
        !error_array.Init(reading_correction_error_array_data_) ||
        !correction_array.Init(reading_correction_correction_array_data_) ||
        value_array.size() != error_array.size() ||
        value_array.size() != correction_array.size()) {
      LOG(ERROR) << "Reading correction data is broken";
      return Status::DATA_BROKEN;
    }
  }
  if (!reader.Get("symbol_token", &symbol_token_array_data_)) {
    LOG(ERROR) << "Cannot find a symbol token array";
    return Status::DATA_MISSING;
  }
  if (!reader.Get("symbol_string", &symbol_string_array_data_)) {
    LOG(ERROR) << "Cannot find a symbol string array or data is broken";
    return Status::DATA_MISSING;
  }
  if (!SerializedDictionary::VerifyData(symbol_token_array_data_,
                                        symbol_string_array_data_)) {
    LOG(ERROR) << "Symbol dictionary data is broken";
    return Status::DATA_BROKEN;
  }
  if (!reader.Get("emoticon_token", &emoticon_token_array_data_)) {
    LOG(ERROR) << "Cannot find an emoticon token array";
    return Status::DATA_MISSING;
  }
  if (!reader.Get("emoticon_string", &emoticon_string_array_data_)) {
    LOG(ERROR) << "Cannot find an emoticon string array or data is broken";
    return Status::DATA_MISSING;
  }
  if (!SerializedDictionary::VerifyData(emoticon_token_array_data_,
                                        emoticon_string_array_data_)) {
    LOG(ERROR) << "Emoticon dictionary data is broken";
    return Status::DATA_BROKEN;
  }
  if (!reader.Get("emoji_token", &emoji_token_array_data_)) {
    LOG(ERROR) << "Cannot find an emoji token array";
    return Status::DATA_MISSING;
  }
  if (!reader.Get("emoji_string", &emoji_string_array_data_)) {
    LOG(ERROR) << "Cannot find an emoji string array or data is broken";
    return Status::DATA_MISSING;
  }
  if (!SerializedStringArray::VerifyData(emoji_string_array_data_)) {
    LOG(ERROR) << "Emoji rewriter string array data is broken";
    return Status::DATA_BROKEN;
  }
  if (!reader.Get("single_kanji_token", &single_kanji_token_array_data_) ||
      !reader.Get("single_kanji_string", &single_kanji_string_array_data_) ||
      !reader.Get("single_kanji_variant_type",
                  &single_kanji_variant_type_data_) ||
      !reader.Get("single_kanji_variant_token",
                  &single_kanji_variant_token_array_data_) ||
      !reader.Get("single_kanji_variant_string",
                  &single_kanji_variant_string_array_data_) ||
      !reader.Get("single_kanji_noun_prefix_token",
                  &single_kanji_noun_prefix_token_array_data_) ||
      !reader.Get("single_kanji_noun_prefix_string",
                  &single_kanji_noun_prefix_string_array_data_)) {
    LOG(ERROR) << "Cannot find single Kanji rewriter data";
    return Status::DATA_MISSING;
  }
  if (!SerializedStringArray::VerifyData(single_kanji_string_array_data_) ||
      !SerializedStringArray::VerifyData(single_kanji_variant_type_data_) ||
      !SerializedStringArray::VerifyData(
          single_kanji_variant_string_array_data_) ||
      !SerializedDictionary::VerifyData(
          single_kanji_noun_prefix_token_array_data_,
          single_kanji_noun_prefix_string_array_data_)) {
    LOG(ERROR) << "Single Kanji data is broken";
    return Status::DATA_BROKEN;
  }
  if (!reader.Get("a11y_description_token",
                  &a11y_description_token_array_data_)) {
    MOZC_VLOG(2) << "A11y description dictionary's token array is not provided";
    a11y_description_token_array_data_ = "";
    // A11y description dictionary is optional, so don't return false here.
  }
  if (!reader.Get("a11y_description_string",
                  &a11y_description_string_array_data_)) {
    MOZC_VLOG(2)
        << "A11y description dictionary's string array is not provided";
    a11y_description_string_array_data_ = "";
    // A11y description dictionary is optional, so don't return false here.
  }
  if (!(a11y_description_token_array_data_.empty() &&
        a11y_description_string_array_data_.empty()) &&
      !SerializedDictionary::VerifyData(a11y_description_token_array_data_,
                                        a11y_description_string_array_data_)) {
    LOG(ERROR) << "A11y description dictionary data is broken";
    return Status::DATA_BROKEN;
  }
  if (!reader.Get("zero_query_token_array", &zero_query_token_array_data_) ||
      !reader.Get("zero_query_string_array", &zero_query_string_array_data_) ||
      !reader.Get("zero_query_number_token_array",
                  &zero_query_number_token_array_data_) ||
      !reader.Get("zero_query_number_string_array",
                  &zero_query_number_string_array_data_)) {
    LOG(ERROR) << "Cannot find zero query data";
    return Status::DATA_MISSING;
  }
  if (!SerializedStringArray::VerifyData(zero_query_string_array_data_) ||
      !SerializedStringArray::VerifyData(
          zero_query_number_string_array_data_)) {
    LOG(ERROR) << "Zero query data is broken";
    return Status::DATA_BROKEN;
  }

  if (!reader.Get("usage_item_array", &usage_items_data_)) {
    MOZC_VLOG(2) << "Usage dictionary is not provided";
    // Usage dictionary is optional, so don't return false here.
  } else {
    if (!reader.Get("usage_base_conjugation_suffix",
                    &usage_base_conjugation_suffix_data_) ||
        !reader.Get("usage_conjugation_suffix",
                    &usage_conjugation_suffix_data_) ||
        !reader.Get("usage_conjugation_index",
                    &usage_conjugation_index_data_) ||
        !reader.Get("usage_string_array", &usage_string_array_data_)) {
      LOG(ERROR) << "Cannot find some usage dictionary data components";
      return Status::DATA_MISSING;
    }
    if (!SerializedStringArray::VerifyData(usage_string_array_data_)) {
      LOG(ERROR) << "Usage dictionary's string array is broken";
      return Status::DATA_BROKEN;
    }
  }

  if (!reader.Get("version", &data_version_)) {
    LOG(ERROR) << "Cannot find data version";
    return Status::DATA_MISSING;
  }
  {
    std::vector<absl::string_view> components =
        absl::StrSplit(data_version_, '.', absl::SkipEmpty());
    if (components.size() != 3) {
      LOG(ERROR) << "Invalid version format: " << data_version_;
      return Status::DATA_BROKEN;
    }
    if (components[0] != Version::GetMozcEngineVersion()) {
      LOG(ERROR) << "Incompatible data. The required engine version is "
                 << Version::GetMozcEngineVersion() << " but tried to load "
                 << components[0] << " (" << data_version_ << ")";
      return Status::ENGINE_VERSION_MISMATCH;
    }
  }
  return Status::OK;
}

DataManager::Status DataManager::InitFromFile(const std::string &path) {
  return InitFromFile(path, kDataSetMagicNumber);
}

DataManager::Status DataManager::InitFromFile(const std::string &path,
                                              absl::string_view magic) {
  absl::StatusOr<Mmap> mmap = Mmap::Map(path, Mmap::READ_ONLY);
  if (!mmap.ok()) {
    LOG(ERROR) << mmap.status();
    return Status::MMAP_FAILURE;
  }
  filename_ = path;
  mmap_ = *std::move(mmap);
  const absl::string_view data(mmap_.begin(), mmap_.size());
  return InitFromArray(data, magic);
}

DataManager::Status DataManager::InitUserPosManagerDataFromArray(
    absl::string_view array, absl::string_view magic) {
  DataSetReader reader;
  if (!reader.Init(array, magic)) {
    LOG(ERROR) << "Binary data of size " << array.size() << " is broken";
    return Status::DATA_BROKEN;
  }
  const Status status = InitUserPosManagerDataFromReader(
      reader, &pos_matcher_data_, &user_pos_token_array_data_,
      &user_pos_string_array_data_);
  LOG_IF(ERROR, status != Status::OK) << "User POS manager data is broken";
  return status;
}

DataManager::Status DataManager::InitUserPosManagerDataFromFile(
    const std::string &path, absl::string_view magic) {
  absl::StatusOr<Mmap> mmap = Mmap::Map(path, Mmap::READ_ONLY);
  if (!mmap.ok()) {
    LOG(ERROR) << mmap.status();
    return Status::MMAP_FAILURE;
  }
  mmap_ = *std::move(mmap);
  const absl::string_view data(mmap_.begin(), mmap_.size());
  return InitUserPosManagerDataFromArray(data, magic);
}

absl::string_view DataManager::GetConnectorData() const {
  return connection_data_;
}

absl::string_view DataManager::GetSystemDictionaryData() const {
  return dictionary_data_;
}

absl::Span<const uint32_t> DataManager::GetCollocationData() const {
  return MakeSpanFromAlignedBuffer<uint32_t>(collocation_data_);
}

absl::Span<const uint32_t> DataManager::GetCollocationSuppressionData() const {
  return MakeSpanFromAlignedBuffer<uint32_t>(collocation_suppression_data_);
}

absl::Span<const uint32_t> DataManager::GetSuggestionFilterData() const {
  return MakeSpanFromAlignedBuffer<uint32_t>(suggestion_filter_data_);
}

void DataManager::GetUserPosData(absl::string_view *token_array_data,
                                 absl::string_view *string_array_data) const {
  *token_array_data = user_pos_token_array_data_;
  *string_array_data = user_pos_string_array_data_;
}

absl::Span<const uint16_t> DataManager::GetPosMatcherData() const {
  return MakeSpanFromAlignedBuffer<uint16_t>(pos_matcher_data_);
}

absl::Span<const uint8_t> DataManager::GetPosGroupData() const {
  return MakeSpanFromAlignedBuffer<uint8_t>(pos_group_data_);
}

void DataManager::GetSegmenterData(
    size_t *l_num_elements, size_t *r_num_elements,
    absl::Span<const uint16_t> *l_table, absl::Span<const uint16_t> *r_table,
    absl::Span<const char> *bitarray_data,
    absl::Span<const uint16_t> *boundary_data) const {
  *l_num_elements = segmenter_compressed_lsize_;
  *r_num_elements = segmenter_compressed_rsize_;
  *l_table = MakeSpanFromAlignedBuffer<uint16_t>(segmenter_ltable_);
  *r_table = MakeSpanFromAlignedBuffer<uint16_t>(segmenter_rtable_);
  *bitarray_data = MakeSpanFromAlignedBuffer<char>(segmenter_bitarray_);
  *boundary_data = MakeSpanFromAlignedBuffer<uint16_t>(boundary_data_);
}

void DataManager::GetSuffixDictionaryData(
    absl::string_view *key_array_data, absl::string_view *value_array_data,
    absl::Span<const uint32_t> *token_array) const {
  *key_array_data = suffix_key_array_data_;
  *value_array_data = suffix_value_array_data_;
  *token_array = MakeSpanFromAlignedBuffer<uint32_t>(suffix_token_array_data_);
}

void DataManager::GetReadingCorrectionData(
    absl::string_view *value_array_data, absl::string_view *error_array_data,
    absl::string_view *correction_array_data) const {
  *value_array_data = reading_correction_value_array_data_;
  *error_array_data = reading_correction_error_array_data_;
  *correction_array_data = reading_correction_correction_array_data_;
}

void DataManager::GetSymbolRewriterData(
    absl::string_view *token_array_data,
    absl::string_view *string_array_data) const {
  *token_array_data = symbol_token_array_data_;
  *string_array_data = symbol_string_array_data_;
}

void DataManager::GetEmoticonRewriterData(
    absl::string_view *token_array_data,
    absl::string_view *string_array_data) const {
  *token_array_data = emoticon_token_array_data_;
  *string_array_data = emoticon_string_array_data_;
}

void DataManager::GetEmojiRewriterData(
    absl::string_view *token_array_data,
    absl::string_view *string_array_data) const {
  *token_array_data = emoji_token_array_data_;
  *string_array_data = emoji_string_array_data_;
}

void DataManager::GetSingleKanjiRewriterData(
    absl::string_view *token_array_data, absl::string_view *string_array_data,
    absl::string_view *variant_type_array_data,
    absl::string_view *variant_token_array_data,
    absl::string_view *variant_string_array_data,
    absl::string_view *noun_prefix_token_array_data,
    absl::string_view *noun_prefix_string_array_data) const {
  *token_array_data = single_kanji_token_array_data_;
  *string_array_data = single_kanji_string_array_data_;
  *variant_type_array_data = single_kanji_variant_type_data_;
  *variant_token_array_data = single_kanji_variant_token_array_data_;
  *variant_string_array_data = single_kanji_variant_string_array_data_;
  *noun_prefix_token_array_data = single_kanji_noun_prefix_token_array_data_;
  *noun_prefix_string_array_data = single_kanji_noun_prefix_string_array_data_;
}

void DataManager::GetA11yDescriptionRewriterData(
    absl::string_view *token_array_data,
    absl::string_view *string_array_data) const {
  *token_array_data = a11y_description_token_array_data_;
  *string_array_data = a11y_description_string_array_data_;
}

absl::string_view DataManager::GetCounterSuffixSortedArray() const {
  return counter_suffix_data_;
}

void DataManager::GetZeroQueryData(
    absl::string_view *zero_query_token_array_data,
    absl::string_view *zero_query_string_array_data,
    absl::string_view *zero_query_number_token_array_data,
    absl::string_view *zero_query_number_string_array_data) const {
  *zero_query_token_array_data = zero_query_token_array_data_;
  *zero_query_string_array_data = zero_query_string_array_data_;
  *zero_query_number_token_array_data = zero_query_number_token_array_data_;
  *zero_query_number_string_array_data = zero_query_number_string_array_data_;
}

#ifndef NO_USAGE_REWRITER
void DataManager::GetUsageRewriterData(
    absl::string_view *base_conjugation_suffix_data,
    absl::string_view *conjugation_suffix_data,
    absl::string_view *conjugation_index_data,
    absl::string_view *usage_items_data,
    absl::string_view *string_array_data) const {
  *base_conjugation_suffix_data = usage_base_conjugation_suffix_data_;
  *conjugation_suffix_data = usage_conjugation_suffix_data_;
  *conjugation_index_data = usage_conjugation_index_data_;
  *usage_items_data = usage_items_data_;
  *string_array_data = usage_string_array_data_;
}
#endif  // NO_USAGE_REWRITER

absl::string_view DataManager::GetDataVersion() const { return data_version_; }

std::optional<std::pair<size_t, size_t>> DataManager::GetOffsetAndSize(
    absl::string_view name) const {
  if (const auto iter = offset_and_size_.find(name);
      iter != offset_and_size_.end()) {
    return iter->second;
  }
  return std::nullopt;
}

std::ostream &operator<<(std::ostream &os, DataManager::Status status) {
  return os << DataManager::StatusCodeToString(status);
}

}  // namespace mozc
