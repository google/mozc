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

#include "data_manager/packed/packed_data_manager.h"

#include <memory>

#include "base/flags.h"
#include "base/logging.h"
#include "base/mmap.h"
#include "base/protobuf/coded_stream.h"
#include "base/protobuf/gzip_stream.h"
#include "base/protobuf/zero_copy_stream_impl.h"
#include "data_manager/data_manager.h"
#include "data_manager/data_manager_interface.h"
#include "data_manager/packed/system_dictionary_data.pb.h"
#include "data_manager/packed/system_dictionary_format_version.h"

DEFINE_string(dataset,
              "",
              "The dataset tag of the POS data.");

using std::unique_ptr;

namespace mozc {
namespace packed {
namespace {
// Default value of the total bytes limit defined in protobuf library is 64MB.
// Our big dictionary size is about 50MB. So we don't need to change it.
const size_t kDefaultTotalBytesLimit = 64 << 20;

unique_ptr<PackedDataManager> g_data_manager;

}  // namespace

class PackedDataManager::Impl {
 public:
  Impl();
  ~Impl();
  bool Init(const string &system_dictionary_data);
  bool InitWithZippedData(const string &zipped_system_dictionary_data);
  string GetDictionaryVersion();

  void GetUserPOSData(StringPiece *token_array_data,
                      StringPiece *string_array_data) const;
  const uint16 *GetPOSMatcherData() const;
  const uint8 *GetPosGroupData() const;
  void GetConnectorData(const char **data, size_t *size) const;
  void GetSegmenterData(
      size_t *l_num_elements, size_t *r_num_elements,
      const uint16 **l_table, const uint16 **r_table,
      size_t *bitarray_num_bytes, const char **bitarray_data,
      const uint16 **boundary_data) const;
  void GetSystemDictionaryData(const char **data, int *size) const;
  void GetSuffixDictionaryData(StringPiece *key_array, StringPiece *value_array,
                               const uint32 **token_array) const;
  void GetReadingCorrectionData(
      StringPiece *value_array_data, StringPiece *error_array_data,
      StringPiece *correction_array_data) const;
  void GetCollocationData(const char **array, size_t *size) const;
  void GetCollocationSuppressionData(const char **array,
                                     size_t *size) const;
  void GetSuggestionFilterData(const char **data, size_t *size) const;
  void GetSymbolRewriterData(StringPiece *token_array_data,
                             StringPiece *string_array_data) const;
  void GetEmoticonRewriterData(StringPiece *token_array_data,
                               StringPiece *string_array_data) const;
  void GetSingleKanjiRewriterData(
      StringPiece *token_array_data,
      StringPiece *string_array_data,
      StringPiece *variant_type_array_data,
      StringPiece *variant_token_array_data,
      StringPiece *variant_string_array_data,
      StringPiece *noun_prefix_token_array_data,
      StringPiece *noun_prefix_string_array_data) const;

#ifndef NO_USAGE_REWRITER
  void GetUsageRewriterData(StringPiece *base_conjugation_suffix_data,
                            StringPiece *conjugation_suffix_data,
                            StringPiece *conjugation_suffix_index_data,
                            StringPiece *usage_items_data,
                            StringPiece *string_array_data) const;
#endif  // NO_USAGE_REWRITER
  void GetCounterSuffixSortedArray(const char **array, size_t *size) const;
  StringPiece GetMozcData() const;

 private:
  bool InitializeWithSystemDictionaryData();

  unique_ptr<SystemDictionaryData> system_dictionary_data_;
  DataManager manager_;
};

PackedDataManager::Impl::Impl() = default;
PackedDataManager::Impl::~Impl() = default;

bool PackedDataManager::Impl::Init(const string &system_dictionary_data) {
  system_dictionary_data_.reset(new SystemDictionaryData);
  if (!system_dictionary_data_->ParseFromString(system_dictionary_data)) {
    LOG(ERROR) << "System dictionary data protobuf format error!";
    return false;
  }
  return InitializeWithSystemDictionaryData();
}

bool PackedDataManager::Impl::InitWithZippedData(
    const string &zipped_system_dictionary_data) {
  protobuf::io::ArrayInputStream input(zipped_system_dictionary_data.data(),
                                       zipped_system_dictionary_data.size());
  protobuf::io::GzipInputStream gzip_stream(&input);
  protobuf::io::CodedInputStream coded_stream(&gzip_stream);

  // Disables the total bytes warning.
  coded_stream.SetTotalBytesLimit(kDefaultTotalBytesLimit, -1);

  system_dictionary_data_.reset(new SystemDictionaryData);
  if (!system_dictionary_data_->ParseFromCodedStream(&coded_stream)) {
    LOG(ERROR) << "System dictionary data protobuf format error!";
    return false;
  }
  return InitializeWithSystemDictionaryData();
}

string PackedDataManager::Impl::GetDictionaryVersion() {
  return system_dictionary_data_->product_version();
}

bool PackedDataManager::Impl::InitializeWithSystemDictionaryData() {
  // Checks format version.
  if (system_dictionary_data_->format_version() !=
          kSystemDictionaryFormatVersion) {
    LOG(ERROR) << "System dictionary data format version miss match! "
               << " expected:" << kSystemDictionaryFormatVersion
               << " actual:" << system_dictionary_data_->format_version();
    return false;
  }

  // Initialize |manager_| (PackedDataManager for light doesn't have mozc data).
  if (system_dictionary_data_->has_mozc_data() &&
      !manager_.InitFromArray(system_dictionary_data_->mozc_data(),
                              system_dictionary_data_->mozc_data_magic())) {
    VLOG(1) << "Data set is incomplete.  Assume this is user pos manager data.";
    // The data set containing only user pos manager data is used in build
    // tools.
    // TODO(noriyukit): Fix this hard-to-understand behavior by removing
    // PackedDataManager.
    if (!manager_.InitUserPosManagerDataFromArray(
            system_dictionary_data_->mozc_data(),
            system_dictionary_data_->mozc_data_magic())) {
      LOG(ERROR) << "Failed to initialize mozc data";
      return false;
    }
  }
  return true;
}

void PackedDataManager::Impl::GetUserPOSData(
    StringPiece *token_array_data, StringPiece *string_array_data) const {
  manager_.GetUserPOSData(token_array_data, string_array_data);
}

const uint16 *PackedDataManager::Impl::GetPOSMatcherData() const {
  return manager_.GetPOSMatcherData();
}

const uint8 *PackedDataManager::Impl::GetPosGroupData() const {
  return manager_.GetPosGroupData();
}

void PackedDataManager::Impl::GetConnectorData(
    const char **data,
    size_t *size) const {
  manager_.GetConnectorData(data, size);
}

void PackedDataManager::Impl::GetSegmenterData(
    size_t *l_num_elements, size_t *r_num_elements,
    const uint16 **l_table, const uint16 **r_table,
    size_t *bitarray_num_bytes, const char **bitarray_data,
    const uint16 **boundary_data) const {
  manager_.GetSegmenterData(l_num_elements, r_num_elements, l_table, r_table,
                            bitarray_num_bytes, bitarray_data, boundary_data);
}

void PackedDataManager::Impl::GetSystemDictionaryData(
    const char **data,
    int *size) const {
  manager_.GetSystemDictionaryData(data, size);
}

void PackedDataManager::Impl::GetSuffixDictionaryData(
    StringPiece *key_array, StringPiece *value_array,
    const uint32 **token_array) const {
  manager_.GetSuffixDictionaryData(key_array, value_array, token_array);
}

void PackedDataManager::Impl::GetReadingCorrectionData(
    StringPiece *value_array_data, StringPiece *error_array_data,
    StringPiece *correction_array_data) const {
  manager_.GetReadingCorrectionData(value_array_data, error_array_data,
                                    correction_array_data);
}

void PackedDataManager::Impl::GetCollocationData(
    const char **array,
    size_t *size) const {
  manager_.GetCollocationData(array, size);
}

void PackedDataManager::Impl::GetCollocationSuppressionData(
    const char **array,
    size_t *size) const {
  manager_.GetCollocationSuppressionData(array, size);
}

void PackedDataManager::Impl::GetSuggestionFilterData(
    const char **data,
    size_t *size) const {
  manager_.GetSuggestionFilterData(data, size);
}

void PackedDataManager::Impl::GetSymbolRewriterData(
    StringPiece *token_array_data, StringPiece *string_array_data) const {
  manager_.GetSymbolRewriterData(token_array_data, string_array_data);
}

void PackedDataManager::Impl::GetEmoticonRewriterData(
    StringPiece *token_array_data, StringPiece *string_array_data) const {
  manager_.GetEmoticonRewriterData(token_array_data, string_array_data);
}

void PackedDataManager::Impl::GetSingleKanjiRewriterData(
    StringPiece *token_array_data,
    StringPiece *string_array_data,
    StringPiece *variant_type_array_data,
    StringPiece *variant_token_array_data,
    StringPiece *variant_string_array_data,
    StringPiece *noun_prefix_token_array_data,
    StringPiece *noun_prefix_string_array_data) const {
  manager_.GetSingleKanjiRewriterData(token_array_data,
                                      string_array_data,
                                      variant_type_array_data,
                                      variant_token_array_data,
                                      variant_string_array_data,
                                      noun_prefix_token_array_data,
                                      noun_prefix_string_array_data);
}

#ifndef NO_USAGE_REWRITER
void PackedDataManager::Impl::GetUsageRewriterData(
    StringPiece *base_conjugation_suffix_data,
    StringPiece *conjugation_suffix_data,
    StringPiece *conjugation_suffix_index_data,
    StringPiece *usage_items_data,
    StringPiece *string_array_data) const {
  manager_.GetUsageRewriterData(base_conjugation_suffix_data,
                                conjugation_suffix_data,
                                conjugation_suffix_index_data,
                                usage_items_data,
                                string_array_data);
}
#endif  // NO_USAGE_REWRITER

void PackedDataManager::Impl::GetCounterSuffixSortedArray(
    const char **array, size_t *size) const {
  manager_.GetCounterSuffixSortedArray(array, size);
}

StringPiece PackedDataManager::Impl::GetMozcData() const {
  return StringPiece(system_dictionary_data_->mozc_data());
}

PackedDataManager::PackedDataManager() {
}

PackedDataManager::~PackedDataManager() {
}

bool PackedDataManager::Init(const string &system_dictionary_data) {
  manager_impl_.reset(new Impl());
  if (manager_impl_->Init(system_dictionary_data)) {
    return true;
  }
  LOG(ERROR) << "PackedDataManager initialization error";
  manager_impl_.reset();
  return false;
}

bool PackedDataManager::InitWithZippedData(
    const string &zipped_system_dictionary_data) {
  manager_impl_.reset(new Impl());
  if (manager_impl_->InitWithZippedData(zipped_system_dictionary_data)) {
    return true;
  }
  LOG(ERROR) << "PackedDataManager initialization error";
  manager_impl_.reset();
  return false;
}

string PackedDataManager::GetDictionaryVersion() {
  return manager_impl_->GetDictionaryVersion();
}

void PackedDataManager::GetUserPOSData(
    StringPiece *token_array_data, StringPiece *string_array_data) const {
  manager_impl_->GetUserPOSData(token_array_data, string_array_data);
}

PackedDataManager *PackedDataManager::GetUserPosManager() {
  if (!g_data_manager.get()) {
    LOG(INFO) << "PackedDataManager::GetUserPosManager null!";
    LOG(INFO) << "FLAGS_dataset: [" << FLAGS_dataset << "]";
    if (FLAGS_dataset.empty()) {
      LOG(FATAL) << "PackedDataManager::GetUserPosManager ERROR!";
    } else {
      unique_ptr<PackedDataManager> data_manager(new PackedDataManager);
      string buffer;
      {
        Mmap mmap;
        CHECK(mmap.Open(FLAGS_dataset.c_str(), "r"));
        buffer.assign(mmap.begin(), mmap.size());
      }
      if (data_manager->Init(buffer)) {
        RegisterPackedDataManager(data_manager.release());
      }
    }
  }
  CHECK(g_data_manager.get()) << "PackedDataManager::GetUserPosManager ERROR!";
  return g_data_manager.get();
}

const uint16 *PackedDataManager::GetPOSMatcherData() const {
  return manager_impl_->GetPOSMatcherData();
}

const uint8 *PackedDataManager::GetPosGroupData() const {
  return manager_impl_->GetPosGroupData();
}

void PackedDataManager::GetConnectorData(
    const char **data,
    size_t *size) const {
  manager_impl_->GetConnectorData(data, size);
}

void PackedDataManager::GetSegmenterData(
    size_t *l_num_elements, size_t *r_num_elements,
    const uint16 **l_table, const uint16 **r_table,
    size_t *bitarray_num_bytes, const char **bitarray_data,
    const uint16 **boundary_data) const {
  manager_impl_->GetSegmenterData(l_num_elements,
                                  r_num_elements,
                                  l_table,
                                  r_table,
                                  bitarray_num_bytes,
                                  bitarray_data,
                                  boundary_data);
}

void PackedDataManager::GetSystemDictionaryData(
    const char **data,
    int *size) const {
  manager_impl_->GetSystemDictionaryData(data, size);
}

void PackedDataManager::GetSuffixDictionaryData(
    StringPiece *key_array, StringPiece *value_array,
    const uint32 **token_array) const {
  manager_impl_->GetSuffixDictionaryData(key_array, value_array, token_array);
}

void PackedDataManager::GetReadingCorrectionData(
    StringPiece *value_array_data, StringPiece *error_array_data,
    StringPiece *correction_array_data) const {
  manager_impl_->GetReadingCorrectionData(value_array_data, error_array_data,
                                          correction_array_data);
}

void PackedDataManager::GetCollocationData(
  const char **array,
  size_t *size) const {
  manager_impl_->GetCollocationData(array, size);
}

void PackedDataManager::GetCollocationSuppressionData(
    const char **array,
    size_t *size) const {
  manager_impl_->GetCollocationSuppressionData(array, size);
}

void PackedDataManager::GetSuggestionFilterData(
    const char **data,
    size_t *size) const {
  manager_impl_->GetSuggestionFilterData(data, size);
}

void PackedDataManager::GetSymbolRewriterData(
    StringPiece *token_array_data, StringPiece *string_array_data) const {
  manager_impl_->GetSymbolRewriterData(token_array_data, string_array_data);
}

void PackedDataManager::GetEmoticonRewriterData(
    StringPiece *token_array_data, StringPiece *string_array_data) const {
  manager_impl_->GetEmoticonRewriterData(token_array_data, string_array_data);
}

void PackedDataManager::GetSingleKanjiRewriterData(
    StringPiece *token_array_data,
    StringPiece *string_array_data,
    StringPiece *variant_type_array_data,
    StringPiece *variant_token_array_data,
    StringPiece *variant_string_array_data,
    StringPiece *noun_prefix_token_array_data,
    StringPiece *noun_prefix_string_array_data) const {
  manager_impl_->GetSingleKanjiRewriterData(token_array_data,
                                            string_array_data,
                                            variant_type_array_data,
                                            variant_token_array_data,
                                            variant_string_array_data,
                                            noun_prefix_token_array_data,
                                            noun_prefix_string_array_data);
}

#ifndef NO_USAGE_REWRITER
void PackedDataManager::GetUsageRewriterData(
    StringPiece *base_conjugation_suffix_data,
    StringPiece *conjugation_suffix_data,
    StringPiece *conjugation_suffix_index_data,
    StringPiece *usage_items_data,
    StringPiece *string_array_data) const {
  manager_impl_->GetUsageRewriterData(base_conjugation_suffix_data,
                                      conjugation_suffix_data,
                                      conjugation_suffix_index_data,
                                      usage_items_data,
                                      string_array_data);
}
#endif  // NO_USAGE_REWRITER

void PackedDataManager::GetCounterSuffixSortedArray(
    const char **array, size_t *size) const {
  manager_impl_->GetCounterSuffixSortedArray(array, size);
}

StringPiece PackedDataManager::GetMozcData() const {
  return manager_impl_->GetMozcData();
}

void RegisterPackedDataManager(PackedDataManager *packed_data_manager) {
  g_data_manager.reset(packed_data_manager);
}

PackedDataManager *GetPackedDataManager() {
  return g_data_manager.get();
}

}  // namespace packed
}  // namespace mozc
