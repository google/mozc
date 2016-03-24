// Copyright 2010-2016, Google Inc.
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

#include "base/logging.h"
#include "base/serialized_string_array.h"
#include "data_manager/dataset_reader.h"
#include "protocol/segmenter_data.pb.h"
#include "rewriter/serialized_dictionary.h"

namespace mozc {
namespace {

bool InitUserPosManagerDataFromReader(
    const DataSetReader &reader,
    StringPiece *pos_matcher_data,
    StringPiece *user_pos_token_array_data,
    StringPiece *user_pos_string_array_data) {
  if (!reader.Get("pos_matcher", pos_matcher_data)) {
    LOG(ERROR) << "Cannot find POS matcher rule ID table";
    return false;
  }
  if (!reader.Get("user_pos_token", user_pos_token_array_data)) {
    LOG(ERROR) << "Cannot find a user POS token array";
    return false;
  }
  if (!reader.Get("user_pos_string", user_pos_string_array_data)) {
    LOG(ERROR) << "Cannot find a user POS string array";
    return false;
  }
  if (user_pos_token_array_data->size() % 8 != 0 ||
      !SerializedStringArray::VerifyData(*user_pos_string_array_data)) {
    LOG(ERROR) << "User POS data is broken: token array data size = "
               << user_pos_token_array_data->size() << ", string array size = "
               << user_pos_string_array_data->size();
    return false;
  }
  return true;
}

}  // namespace

DataManager::DataManager() = default;
DataManager::~DataManager() = default;

bool DataManager::InitFromArray(StringPiece array, StringPiece magic) {
  DataSetReader reader;
  if (!reader.Init(array, magic)) {
    LOG(ERROR) << "Binary data of size " << array.size() << " is broken";
    return false;
  }
  if (!InitUserPosManagerDataFromReader(reader,
                                        &pos_matcher_data_,
                                        &user_pos_token_array_data_,
                                        &user_pos_string_array_data_)) {
    LOG(ERROR) << "User POS manager data is broken";
    return false;
  }
  if (!reader.Get("conn", &connection_data_)) {
    LOG(ERROR) << "Cannot find a connection data";
    return false;
  }
  if (!reader.Get("dict", &dictionary_data_)) {
    LOG(ERROR) << "Cannot find a dictionary data";
    return false;
  }
  if (!reader.Get("sugg", &suggestion_filter_data_)) {
    LOG(ERROR) << "Cannot find a suggestion filter data";
    return false;
  }
  if (!reader.Get("coll", &collocation_data_)) {
    LOG(ERROR) << "Cannot find a collocation data";
    return false;
  }
  if (!reader.Get("cols", &collocation_suppression_data_)) {
    LOG(ERROR) << "Cannot find a collocation suprression data";
    return false;
  }
  if (!reader.Get("posg", &pos_group_data_)) {
    LOG(ERROR) << "Cannot find a POS group data";
    return false;
  }
  if (!reader.Get("bdry", &boundary_data_)) {
    LOG(ERROR) << "Cannot find a boundary data";
    return false;
  }
  {
    StringPiece memblock;
    if (!reader.Get("segmenter_sizeinfo", &memblock)) {
      LOG(ERROR) << "Cannot find a segmenter size info";
      return false;
    }
    converter::SegmenterDataSizeInfo sizeinfo;
    if (!sizeinfo.ParseFromArray(memblock.data(), memblock.size())) {
      LOG(ERROR) << "Failed to parse SegmenterDataSizeInfo";
      return false;
    }
    segmenter_compressed_lsize_ = sizeinfo.compressed_lsize();
    segmenter_compressed_rsize_ = sizeinfo.compressed_rsize();
  }
  if (!reader.Get("segmenter_ltable", &segmenter_ltable_)) {
    LOG(ERROR) << "Cannot find a segmenter ltable";
    return false;
  }
  if (!reader.Get("segmenter_rtable", &segmenter_rtable_)) {
    LOG(ERROR) << "Cannot find a segmenter rtable";
    return false;
  }
  if (!reader.Get("segmenter_bitarray", &segmenter_bitarray_)) {
    LOG(ERROR) << "Cannot find a segmenter bit-array";
    return false;
  }
  if (!reader.Get("counter_suffix", &counter_suffix_data_)) {
    LOG(ERROR) << "Cannot find a counter suffix data";
    return false;
  }
  if (!SerializedStringArray::VerifyData(counter_suffix_data_)) {
    LOG(ERROR) << "Counter suffix string array is broken";
    return false;
  }
  if (!reader.Get("suffix_key", &suffix_key_array_data_)) {
    LOG(ERROR) << "Cannot find a suffix key array";
    return false;
  }
  if (!reader.Get("suffix_value", &suffix_value_array_data_)) {
    LOG(ERROR) << "Cannot find a suffix value array";
    return false;
  }
  if (!reader.Get("suffix_token", &suffix_token_array_data_)) {
    LOG(ERROR) << "Cannot find a suffix token array";
    return false;
  }
  {
    SerializedStringArray suffix_keys, suffix_values;
    if (!suffix_keys.Init(suffix_key_array_data_) ||
        !suffix_values.Init(suffix_value_array_data_) ||
        suffix_keys.size() != suffix_values.size() ||
        // Suffix token array is an array of triple (lid, rid, cost) of uint32,
        // so it contains N = 3 * |suffix_keys.size()| uint32 elements.
        // Therefore, its byte length must be 4 * N bytes.
        suffix_token_array_data_.size() != 4 * 3 * suffix_keys.size()) {
      LOG(ERROR) << "Suffix dictionary data is broken";
      return false;
    }
  }
  if (!reader.Get("reading_correction_value",
                  &reading_correction_value_array_data_)) {
    LOG(ERROR) << "Cannot find reading correction value array";
    return false;
  }
  if (!reader.Get("reading_correction_error",
                  &reading_correction_error_array_data_)) {
    LOG(ERROR) << "Cannot find reading correction error array";
    return false;
  }
  if (!reader.Get("reading_correction_correction",
                  &reading_correction_correction_array_data_)) {
    LOG(ERROR) << "Cannot find reading correction correction array";
    return false;
  }
  {
    SerializedStringArray value_array, error_array, correction_array;
    if (!value_array.Init(reading_correction_value_array_data_) ||
        !error_array.Init(reading_correction_error_array_data_) ||
        !correction_array.Init(reading_correction_correction_array_data_) ||
        value_array.size() != error_array.size() ||
        value_array.size() != correction_array.size()) {
      LOG(ERROR) << "Reading correction data is broken";
      return false;
    }
  }
  if (!reader.Get("symbol_token", &symbol_token_array_data_)) {
    LOG(ERROR) << "Cannot find a symbol token array";
    return false;
  }
  if (!reader.Get("symbol_string", &symbol_string_array_data_)) {
    LOG(ERROR) << "Cannot find a symbol string array or data is broken";
    return false;
  }
  if (!SerializedDictionary::VerifyData(symbol_token_array_data_,
                                        symbol_string_array_data_)) {
    LOG(ERROR) << "Symbol dictionary data is broken";
    return false;
  }
  if (!reader.Get("emoticon_token", &emoticon_token_array_data_)) {
    LOG(ERROR) << "Cannot find an emoticon token array";
    return false;
  }
  if (!reader.Get("emoticon_string", &emoticon_string_array_data_)) {
    LOG(ERROR) << "Cannot find an emoticon string array or data is broken";
    return false;
  }
  if (!SerializedDictionary::VerifyData(emoticon_token_array_data_,
                                        emoticon_string_array_data_)) {
    LOG(ERROR) << "Emoticon dictionary data is broken";
    return false;
  }
  if (!reader.Get("emoji_token", &emoji_token_array_data_)) {
    LOG(ERROR) << "Cannot find an emoji token array";
    return false;
  }
  if (!reader.Get("emoji_string", &emoji_string_array_data_)) {
    LOG(ERROR) << "Cannot find an emoji string array or data is broken";
    return false;
  }
  if (!SerializedStringArray::VerifyData(emoji_string_array_data_)) {
    LOG(ERROR) << "Emoji rewriter string array data is broken";
    return false;
  }
  if (!reader.Get("single_kanji_token",
                  &single_kanji_token_array_data_) ||
      !reader.Get("single_kanji_string",
                  &single_kanji_string_array_data_) ||
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
    return false;
  }
  if (!SerializedStringArray::VerifyData(single_kanji_string_array_data_) ||
      !SerializedStringArray::VerifyData(single_kanji_variant_type_data_) ||
      !SerializedStringArray::VerifyData(
          single_kanji_variant_string_array_data_) ||
      !SerializedDictionary::VerifyData(
          single_kanji_noun_prefix_token_array_data_,
          single_kanji_noun_prefix_string_array_data_)) {
    LOG(ERROR) << "Single Kanji data is broken";
    return false;
  }

  if (!reader.Get("usage_item_array", &usage_items_data_)) {
    VLOG(2) << "Usage dictionary is not provided";
    // Usage dictionary is optional, so don't return false here.
  } else {
    if (!reader.Get("usage_base_conjugation_suffix",
                    &usage_base_conjugation_suffix_data_) ||
        !reader.Get("usage_conjugation_suffix",
                    &usage_conjugation_suffix_data_) ||
        !reader.Get("usage_conjugation_index",
                    &usage_conjugation_index_data_) ||
        !reader.Get("usage_string_array",
                    &usage_string_array_data_)) {
      LOG(ERROR) << "Cannot find some usage dictionary data components";
      return false;
    }
    if (!SerializedStringArray::VerifyData(usage_string_array_data_)) {
      LOG(ERROR) << "Usage dictionary's string array is broken";
      return false;
    }
  }
  return true;
}

bool DataManager::InitUserPosManagerDataFromArray(StringPiece array,
                                                  StringPiece magic) {
  DataSetReader reader;
  if (!reader.Init(array, magic)) {
    LOG(ERROR) << "Binary data of size " << array.size() << " is broken";
    return false;
  }
  if (!InitUserPosManagerDataFromReader(reader,
                                        &pos_matcher_data_,
                                        &user_pos_token_array_data_,
                                        &user_pos_string_array_data_)) {
    LOG(ERROR) << "User POS manager data is broken";
    return false;
  }
  return true;
}

bool DataManager::InitUserPosManagerDataFromFile(const string &path,
                                                 StringPiece magic) {
  if (!mmap_.Open(path.c_str(), "r")) {
    LOG(ERROR) << "Failed to mmap " << path;
    return false;
  }
  const StringPiece data(mmap_.begin(), mmap_.size());
  return InitUserPosManagerDataFromArray(data, magic);
}

void DataManager::GetConnectorData(const char **data, size_t *size) const {
  *data = connection_data_.data();
  *size = connection_data_.size();
}

void DataManager::GetSystemDictionaryData(const char **data, int *size) const {
  *data = dictionary_data_.data();
  *size = dictionary_data_.size();
}

void DataManager::GetCollocationData(const char **array, size_t *size) const {
  *array = collocation_data_.data();
  *size = collocation_data_.size();
}

void DataManager::GetCollocationSuppressionData(const char **array,
                                                size_t *size) const {
  *array = collocation_suppression_data_.data();
  *size = collocation_suppression_data_.size();
}

void DataManager::GetSuggestionFilterData(const char **data,
                                          size_t *size) const {
  *data = suggestion_filter_data_.data();
  *size = suggestion_filter_data_.size();
}

void DataManager::GetUserPOSData(StringPiece *token_array_data,
                                 StringPiece *string_array_data) const {
  *token_array_data = user_pos_token_array_data_;
  *string_array_data = user_pos_string_array_data_;
}

const uint16 *DataManager::GetPOSMatcherData() const {
  return reinterpret_cast<const uint16 *>(pos_matcher_data_.data());
}

const uint8 *DataManager::GetPosGroupData() const {
  return reinterpret_cast<const uint8 *>(pos_group_data_.data());
}

void DataManager::GetSegmenterData(
    size_t *l_num_elements, size_t *r_num_elements, const uint16 **l_table,
    const uint16 **r_table, size_t *bitarray_num_bytes,
    const char **bitarray_data, const uint16 **boundary_data) const {
  *l_num_elements = segmenter_compressed_lsize_;
  *r_num_elements = segmenter_compressed_rsize_;
  *l_table = reinterpret_cast<const uint16 *>(segmenter_ltable_.data());
  *r_table = reinterpret_cast<const uint16 *>(segmenter_rtable_.data());
  *bitarray_num_bytes = segmenter_bitarray_.size();
  *bitarray_data = segmenter_bitarray_.data();
  *boundary_data = reinterpret_cast<const uint16 *>(boundary_data_.data());
}

void DataManager::GetSuffixDictionaryData(StringPiece *key_array_data,
                                          StringPiece *value_array_data,
                                          const uint32 **token_array) const {
  *key_array_data = suffix_key_array_data_;
  *value_array_data = suffix_value_array_data_;
  *token_array =
      reinterpret_cast<const uint32 *>(suffix_token_array_data_.data());
}

void DataManager::GetReadingCorrectionData(
    StringPiece *value_array_data, StringPiece *error_array_data,
    StringPiece *correction_array_data) const {
  *value_array_data = reading_correction_value_array_data_;
  *error_array_data = reading_correction_error_array_data_;
  *correction_array_data = reading_correction_correction_array_data_;
}

void DataManager::GetSymbolRewriterData(StringPiece *token_array_data,
                                        StringPiece *string_array_data) const {
  *token_array_data = symbol_token_array_data_;
  *string_array_data = symbol_string_array_data_;
}

void DataManager::GetEmoticonRewriterData(
    StringPiece *token_array_data, StringPiece *string_array_data) const {
  *token_array_data = emoticon_token_array_data_;
  *string_array_data = emoticon_string_array_data_;
}

void DataManager::GetEmojiRewriterData(
    StringPiece *token_array_data, StringPiece *string_array_data) const {
  *token_array_data = emoji_token_array_data_;
  *string_array_data = emoji_string_array_data_;
}

void DataManager::GetSingleKanjiRewriterData(
    StringPiece *token_array_data,
    StringPiece *string_array_data,
    StringPiece *variant_type_array_data,
    StringPiece *variant_token_array_data,
    StringPiece *variant_string_array_data,
    StringPiece *noun_prefix_token_array_data,
    StringPiece *noun_prefix_string_array_data) const {
  *token_array_data = single_kanji_token_array_data_;
  *string_array_data = single_kanji_string_array_data_;
  *variant_type_array_data = single_kanji_variant_type_data_;
  *variant_token_array_data = single_kanji_variant_token_array_data_;
  *variant_string_array_data = single_kanji_variant_string_array_data_;
  *noun_prefix_token_array_data = single_kanji_noun_prefix_token_array_data_;
  *noun_prefix_string_array_data = single_kanji_noun_prefix_string_array_data_;
}

void DataManager::GetCounterSuffixSortedArray(const char **array,
                                              size_t *size) const {
  *array = counter_suffix_data_.data();
  *size = counter_suffix_data_.size();
}

#ifndef NO_USAGE_REWRITER
void DataManager::GetUsageRewriterData(
    StringPiece *base_conjugation_suffix_data,
    StringPiece *conjugation_suffix_data,
    StringPiece *conjugation_index_data,
    StringPiece *usage_items_data,
    StringPiece *string_array_data) const {
  *base_conjugation_suffix_data = usage_base_conjugation_suffix_data_;
  *conjugation_suffix_data = usage_conjugation_suffix_data_;
  *conjugation_index_data = usage_conjugation_index_data_;
  *usage_items_data = usage_items_data_;
  *string_array_data = usage_string_array_data_;
}
#endif  // NO_USAGE_REWRITER

}  // namespace mozc
