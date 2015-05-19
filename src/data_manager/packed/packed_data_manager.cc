// Copyright 2010-2013, Google Inc.
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

#include "base/logging.h"
#include "base/mmap.h"
#include "base/protobuf/gzip_stream.h"
#include "base/protobuf/zero_copy_stream_impl.h"
#include "converter/boundary_struct.h"
#include "data_manager/data_manager_interface.h"
#include "data_manager/packed/system_dictionary_data.pb.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suffix_dictionary_token.h"
#include "rewriter/correction_rewriter.h"
#include "rewriter/embedded_dictionary.h"
#ifndef NO_USAGE_REWRITER
#include "rewriter/usage_rewriter_data_structs.h"
#endif  // NO_USAGE_REWRITER

DEFINE_string(dataset,
              "",
              "The dataset tag of the POS data.");

namespace mozc {
namespace packed {
namespace {
const int kSystemDictionaryFormatVersion = 1;

class PackedPOSMatcher : public POSMatcher {
 public:
  PackedPOSMatcher(const uint16 *const rule_id_table,
                   const Range *const *const range_table)
      : POSMatcher(rule_id_table, range_table) {
  }
};

scoped_ptr<PackedDataManager> g_data_manager;

}  // namespace

class PackedDataManager::Impl {
 public:
  Impl();
  ~Impl();
  bool Init(const string &system_dictionary_data);
  bool InitWithZippedData(const string &zipped_system_dictionary_data);
  string GetDictionaryVersion();

  const UserPOS::POSToken *GetUserPOSData() const;
  const POSMatcher *GetPOSMatcher() const;
  const uint8 *GetPosGroupData() const;
  void GetConnectorData(const char **data, size_t *size) const;
  void GetSegmenterData(
      size_t *l_num_elements, size_t *r_num_elements,
      const uint16 **l_table, const uint16 **r_table,
      size_t *bitarray_num_bytes, const char **bitarray_data,
      const BoundaryData **boundary_data) const;
  void GetSystemDictionaryData(const char **data, int *size) const;
  void GetSuffixDictionaryData(const SuffixToken **data,
                               size_t *size) const;
  void GetReadingCorrectionData(const ReadingCorrectionItem **array,
                                size_t *size) const;
  void GetCollocationData(const char **array, size_t *size) const;
  void GetCollocationSuppressionData(const char **array,
                                     size_t *size) const;
  void GetSuggestionFilterData(const char **data, size_t *size) const;
  void GetSymbolRewriterData(const EmbeddedDictionary::Token **data,
                             size_t *size) const;
#ifndef NO_USAGE_REWRITER
  void GetUsageRewriterData(
      const ConjugationSuffix **base_conjugation_suffix,
      const ConjugationSuffix **conjugation_suffix_data,
      const int **conjugation_suffix_data_index,
      const UsageDictItem **usage_data_value) const;
#endif  // NO_USAGE_REWRITER
  const uint16 *GetRuleIdTableForTest() const;
  const void *GetRangeTablesForTest() const;

 private:
  // Non-const struct of POSMatcher::Range
  struct Range {
    uint16 lower;
    uint16 upper;
  };
  bool InitializeWithSystemDictionaryData();

  scoped_array<UserPOS::POSToken> pos_token_;
  scoped_array<UserPOS::ConjugationType> conjugation_array_;
  scoped_array<uint16> rule_id_table_;
  scoped_array<POSMatcher::Range *> range_tables_;
  scoped_array<Range> range_table_items_;
  scoped_array<BoundaryData> boundary_data_;
  scoped_array<SuffixToken> suffix_tokens_;
  scoped_array<ReadingCorrectionItem> reading_corrections_;
  size_t compressed_l_size_;
  size_t compressed_r_size_;
  scoped_array<uint16> compressed_lid_table_;
  scoped_array<uint16> compressed_rid_table_;
  scoped_array<EmbeddedDictionary::Value> symbol_data_values_;
  size_t symbol_data_token_size_;
  scoped_array<EmbeddedDictionary::Token> symbol_data_tokens_;
  scoped_ptr<POSMatcher> pos_matcher_;
  scoped_ptr<SystemDictionaryData> system_dictionary_data_;
#ifndef NO_USAGE_REWRITER
  scoped_array<ConjugationSuffix> base_conjugation_suffix_;
  scoped_array<ConjugationSuffix> conjugation_suffix_data_;
  scoped_array<int> conjugation_suffix_data_index_;
  scoped_array<UsageDictItem> usage_data_value_;
#endif  // NO_USAGE_REWRITER
};

PackedDataManager::Impl::Impl()
    : compressed_l_size_(0),
      compressed_r_size_(0),
      symbol_data_token_size_(0) {
}

PackedDataManager::Impl::~Impl() {
}

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
  system_dictionary_data_.reset(new SystemDictionaryData);
  if (!system_dictionary_data_->ParseFromZeroCopyStream(&gzip_stream)) {
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
  // Makes UserPOS data.
  pos_token_.reset(
      new UserPOS::POSToken[system_dictionary_data_->pos_tokens_size()]);
  size_t conjugation_count = 0;
  for (size_t i = 0; i < system_dictionary_data_->pos_tokens_size(); ++i) {
    conjugation_count +=
        system_dictionary_data_->pos_tokens(i).conjugation_forms_size();
  }
  conjugation_array_.reset(new UserPOS::ConjugationType[conjugation_count]);
  size_t conjugation_index = 0;
  for (size_t i = 0; i < system_dictionary_data_->pos_tokens_size(); ++i) {
    const SystemDictionaryData::PosToken &pos_token =
        system_dictionary_data_->pos_tokens(i);
    if (pos_token.has_pos()) {
      pos_token_[i].pos = pos_token.pos().data();
    } else {
      pos_token_[i].pos = NULL;
    }
    pos_token_[i].conjugation_size =
        pos_token.conjugation_forms_size();
    pos_token_[i].conjugation_form = &conjugation_array_[conjugation_index];
    if (pos_token.conjugation_forms_size() == 0) {
      pos_token_[i].conjugation_form = NULL;
    }
    for (size_t j = 0; j < pos_token.conjugation_forms_size(); ++j) {
      const SystemDictionaryData::PosToken::ConjugationType &conjugation_form =
          pos_token.conjugation_forms(j);
      if (conjugation_form.has_key_suffix()) {
        conjugation_array_[conjugation_index].key_suffix =
            conjugation_form.key_suffix().data();
      } else {
        conjugation_array_[conjugation_index].key_suffix = NULL;
      }
      if (conjugation_form.has_value_suffix()) {
        conjugation_array_[conjugation_index].value_suffix =
            conjugation_form.value_suffix().data();
      } else {
        conjugation_array_[conjugation_index].value_suffix = NULL;
      }
      conjugation_array_[conjugation_index].id = conjugation_form.id();
      ++conjugation_index;
    }
  }

  // Makes POSMatcher data.
  rule_id_table_.reset(
      new uint16[
          system_dictionary_data_->pos_matcher_data().rule_id_table_size()]);
  for (size_t i = 0;
       i < system_dictionary_data_->pos_matcher_data().rule_id_table_size();
       ++i) {
    rule_id_table_[i] =
        system_dictionary_data_->pos_matcher_data().rule_id_table(i);
  }
  const SystemDictionaryData::PosMatcherData &pos_matcher_data =
      system_dictionary_data_->pos_matcher_data();
  range_tables_.reset(
      new POSMatcher::Range*[pos_matcher_data.range_tables_size()]);
  size_t range_count = 0;
  for (size_t i = 0; i < pos_matcher_data.range_tables_size(); ++i) {
    range_count += pos_matcher_data.range_tables(i).ranges_size();
  }
  range_table_items_.reset(
      new Range[range_count + pos_matcher_data.range_tables_size()]);
  size_t range_index = 0;
  for (size_t i = 0; i < pos_matcher_data.range_tables_size(); ++i) {
    const SystemDictionaryData::PosMatcherData::RangeTable &table =
        pos_matcher_data.range_tables(i);
    range_tables_[i] =
        reinterpret_cast<POSMatcher::Range *>(&range_table_items_[range_index]);
    for (size_t j = 0; j < table.ranges_size(); ++j) {
      const SystemDictionaryData::PosMatcherData::RangeTable::Range &range =
          table.ranges(j);
      range_table_items_[range_index].lower = range.lower();
      range_table_items_[range_index].upper = range.upper();
      ++range_index;
    }
    range_table_items_[range_index].lower = static_cast<uint16>(0xFFFF);
    range_table_items_[range_index].upper = static_cast<uint16>(0xFFFF);
    ++range_index;
  }

  // Makes boundary data.
  boundary_data_.reset(
      new BoundaryData[system_dictionary_data_->boundary_data_size()]);
  for (size_t i = 0; i < system_dictionary_data_->boundary_data_size(); ++i) {
    const SystemDictionaryData::BoundaryData &boundary_data =
        system_dictionary_data_->boundary_data(i);
    boundary_data_[i].prefix_penalty = boundary_data.prefix_penalty();
    boundary_data_[i].suffix_penalty = boundary_data.suffix_penalty();
  }

  // Makes suffix data.
  suffix_tokens_.reset(
      new SuffixToken[system_dictionary_data_->suffix_tokens_size()]);
  for (size_t i = 0;
       i < system_dictionary_data_->suffix_tokens_size();
       ++i) {
    const SystemDictionaryData::SuffixToken &suffix_token =
        system_dictionary_data_->suffix_tokens(i);
    if (suffix_token.has_key()) {
      suffix_tokens_[i].key = suffix_token.key().data();
    } else {
      suffix_tokens_[i].key = NULL;
    }
    if (suffix_token.has_value()) {
      suffix_tokens_[i].value = suffix_token.value().data();
    } else {
      suffix_tokens_[i].value = NULL;
    }
    suffix_tokens_[i].lid = suffix_token.lid();
    suffix_tokens_[i].rid = suffix_token.rid();
    suffix_tokens_[i].wcost = suffix_token.wcost();
  }

  // Makes reading correction data.
  reading_corrections_.reset(
      new ReadingCorrectionItem[
          system_dictionary_data_->reading_corrections_size()]);
  for (size_t i = 0;
       i < system_dictionary_data_->reading_corrections_size();
       ++i) {
    const SystemDictionaryData::ReadingCorrectionItem &item =
        system_dictionary_data_->reading_corrections(i);
    if (item.has_value()) {
      reading_corrections_[i].value = item.value().data();
    } else {
      reading_corrections_[i].value = NULL;
    }
    if (item.has_error()) {
      reading_corrections_[i].error = item.error().data();
    } else {
      reading_corrections_[i].error = NULL;
    }
    if (item.has_correction()) {
      reading_corrections_[i].correction = item.correction().data();
    } else {
      reading_corrections_[i].correction = NULL;
    }
  }

  // Makes segment data.
  const SystemDictionaryData::SegmenterData &segmenter_data =
      system_dictionary_data_->segmenter_data();
  compressed_l_size_ = segmenter_data.compressed_l_size();
  compressed_r_size_ = segmenter_data.compressed_r_size();
  compressed_lid_table_.reset(
      new uint16[segmenter_data.compressed_lid_table_size()]);
  for (size_t i = 0; i < segmenter_data.compressed_lid_table_size(); ++i) {
    compressed_lid_table_[i] = segmenter_data.compressed_lid_table(i);
  }
  compressed_rid_table_.reset(
      new uint16[segmenter_data.compressed_rid_table_size()]);
  for (size_t i = 0; i < segmenter_data.compressed_rid_table_size(); ++i) {
    compressed_rid_table_[i] = segmenter_data.compressed_rid_table(i);
  }

  // Makes symbol dictionary data.
  const SystemDictionaryData::EmbeddedDictionary &symbol_dictionary =
      system_dictionary_data_->symbol_dictionary();
  size_t symbol_value_count = 0;
  for (size_t i = 0; i < symbol_dictionary.tokens_size(); ++i) {
    symbol_value_count += symbol_dictionary.tokens(i).values_size();
  }
  symbol_data_values_.reset(
      new EmbeddedDictionary::Value[symbol_value_count + 1]);
  symbol_data_token_size_ = symbol_dictionary.tokens_size();
  symbol_data_tokens_.reset(
    new EmbeddedDictionary::Token[symbol_data_token_size_ + 1]);
  EmbeddedDictionary::Value *value_ptr = symbol_data_values_.get();
  EmbeddedDictionary::Token *token_ptr = symbol_data_tokens_.get();
  for (size_t i = 0; i < symbol_dictionary.tokens_size(); ++i) {
    const SystemDictionaryData::EmbeddedDictionary::Token &token =
        symbol_dictionary.tokens(i);
    token_ptr->key = token.key().data();
    token_ptr->value = value_ptr;
    token_ptr->value_size = token.values_size();
    ++token_ptr;
    for (size_t j = 0; j < token.values_size(); ++j) {
      const SystemDictionaryData::EmbeddedDictionary::Value &value =
          token.values(j);
      if (value.has_value()) {
        value_ptr->value = value.value().data();
      } else {
        value_ptr->value = NULL;
      }
      if (value.has_description()) {
        value_ptr->description = value.description().data();
      } else {
        value_ptr->description = NULL;
      }
      if (value.has_additional_description()) {
        value_ptr->additional_description =
            value.additional_description().data();
      } else {
        value_ptr->additional_description = NULL;
      }
      value_ptr->lid = value.lid();
      value_ptr->rid = value.rid();
      value_ptr->cost = value.cost();
      ++value_ptr;
    }
  }
  value_ptr->value = NULL;
  value_ptr->description = NULL;
  value_ptr->additional_description = NULL;
  value_ptr->lid = 0;
  value_ptr->rid = 0;
  value_ptr->cost = 0;
  token_ptr->key = NULL;
  token_ptr->value = symbol_data_values_.get();
  token_ptr->value_size = symbol_value_count;

  // Makes POSMatcher.
  pos_matcher_.reset(
      new PackedPOSMatcher(rule_id_table_.get(), range_tables_.get()));

#ifndef NO_USAGE_REWRITER
  // Makes Usge rewriter data.
  const SystemDictionaryData::UsageRewriterData &usage_rewriter_data =
      system_dictionary_data_->usage_rewriter_data();
  const size_t conjugation_num = usage_rewriter_data.conjugations_size();
  base_conjugation_suffix_.reset(new ConjugationSuffix[conjugation_num]);
  conjugation_suffix_data_index_.reset(new int[conjugation_num + 1]);

  size_t suffix_data_num = 0;
  conjugation_suffix_data_index_[0] = 0;
  for (size_t i = 0; i < conjugation_num; ++i) {
    const SystemDictionaryData::UsageRewriterData::Conjugation &conjugation =
      usage_rewriter_data.conjugations(i);
    base_conjugation_suffix_[i].value_suffix =
        conjugation.base_suffix().value_suffix().data();
    base_conjugation_suffix_[i].key_suffix =
        conjugation.base_suffix().key_suffix().data();
    suffix_data_num +=
        usage_rewriter_data.conjugations(i).conjugation_suffixes_size();
    conjugation_suffix_data_index_[i + 1] = suffix_data_num;
  }
  conjugation_suffix_data_.reset(new ConjugationSuffix[suffix_data_num]);
  size_t conjugation_suffix_id = 0;
  for (size_t i = 0; i < conjugation_num; ++i) {
    const SystemDictionaryData::UsageRewriterData::Conjugation &conjugation =
      usage_rewriter_data.conjugations(i);
    for (size_t j = 0; j < conjugation.conjugation_suffixes_size(); ++j) {
      conjugation_suffix_data_[conjugation_suffix_id].value_suffix =
        conjugation.conjugation_suffixes(j).value_suffix().data();
      conjugation_suffix_data_[conjugation_suffix_id].key_suffix =
        conjugation.conjugation_suffixes(j).key_suffix().data();
      ++conjugation_suffix_id;
    }
  }

  usage_data_value_.reset(
      new UsageDictItem[usage_rewriter_data.usage_data_values_size() + 1]);
  for (size_t i = 0; i < usage_rewriter_data.usage_data_values_size(); ++i) {
    const SystemDictionaryData::UsageRewriterData::UsageDictItem &item =
        usage_rewriter_data.usage_data_values(i);
    usage_data_value_[i].id = item.id();
    usage_data_value_[i].key = item.key().data();
    usage_data_value_[i].value = item.value().data();
    usage_data_value_[i].conjugation_id = item.conjugation_id();
    usage_data_value_[i].meaning = item.meaning().data();
  }
  UsageDictItem *last_item =
      &usage_data_value_[usage_rewriter_data.usage_data_values_size()];
  last_item->id = 0;
  last_item->key = NULL;
  last_item->value = NULL;
  last_item->conjugation_id = 0;
  last_item->meaning = NULL;
#endif  // NO_USAGE_REWRITER

  return true;
}

const UserPOS::POSToken *PackedDataManager::Impl::GetUserPOSData() const {
  return pos_token_.get();
}

const POSMatcher *PackedDataManager::Impl::GetPOSMatcher() const {
  return pos_matcher_.get();
}

const uint8 *PackedDataManager::Impl::GetPosGroupData() const {
  return reinterpret_cast<const uint8 *>(
      system_dictionary_data_->lid_group_data().data());
}

void PackedDataManager::Impl::GetConnectorData(
    const char **data,
    size_t *size) const {
  *data = system_dictionary_data_->connection_data().data();
  *size = system_dictionary_data_->connection_data().size();
}

void PackedDataManager::Impl::GetSegmenterData(
    size_t *l_num_elements, size_t *r_num_elements,
    const uint16 **l_table, const uint16 **r_table,
    size_t *bitarray_num_bytes, const char **bitarray_data,
    const BoundaryData **boundary_data) const {
  *l_num_elements = compressed_l_size_;
  *r_num_elements = compressed_r_size_;
  *l_table = compressed_lid_table_.get();
  *r_table = compressed_rid_table_.get();
  *bitarray_num_bytes =
      system_dictionary_data_->segmenter_data().bit_array_data().size();
  *bitarray_data =
      system_dictionary_data_->segmenter_data().bit_array_data().data();
  *boundary_data = boundary_data_.get();
}

void PackedDataManager::Impl::GetSystemDictionaryData(
    const char **data,
    int *size) const {
  *data = system_dictionary_data_->dictionary_data().data();
  *size = system_dictionary_data_->dictionary_data().size();
}

void PackedDataManager::Impl::GetSuffixDictionaryData(
    const SuffixToken **data,
    size_t *size) const {
  *data = suffix_tokens_.get();
  *size = system_dictionary_data_->suffix_tokens().size();
}

void PackedDataManager::Impl::GetReadingCorrectionData(
    const ReadingCorrectionItem **array,
    size_t *size) const {
  *array = reading_corrections_.get();
  *size = system_dictionary_data_->reading_corrections().size();
}

void PackedDataManager::Impl::GetCollocationData(
  const char **array,
  size_t *size) const {
  *array = system_dictionary_data_->collocation_data().data();
  *size = system_dictionary_data_->collocation_data().size();
}

void PackedDataManager::Impl::GetCollocationSuppressionData(
    const char **array,
    size_t *size) const {
  *array = system_dictionary_data_->collocation_suppression_data().data();
  *size = system_dictionary_data_->collocation_suppression_data().size();
}

void PackedDataManager::Impl::GetSuggestionFilterData(
    const char **data,
    size_t *size) const {
  *data = system_dictionary_data_->suggestion_filter_data().data();
  *size = system_dictionary_data_->suggestion_filter_data().size();
}

void PackedDataManager::Impl::GetSymbolRewriterData(
    const EmbeddedDictionary::Token **data,
    size_t *size) const {
  *data = symbol_data_tokens_.get();
  *size = symbol_data_token_size_;
}

#ifndef NO_USAGE_REWRITER
void PackedDataManager::Impl::GetUsageRewriterData(
    const ConjugationSuffix **base_conjugation_suffix,
    const ConjugationSuffix **conjugation_suffix_data,
    const int **conjugation_suffix_data_index,
    const UsageDictItem **usage_data_value) const {
  *base_conjugation_suffix = base_conjugation_suffix_.get();
  *conjugation_suffix_data = conjugation_suffix_data_.get();
  *conjugation_suffix_data_index = conjugation_suffix_data_index_.get();
  *usage_data_value = usage_data_value_.get();
}
#endif  // NO_USAGE_REWRITER

const uint16 *PackedDataManager::Impl::GetRuleIdTableForTest() const {
  return rule_id_table_.get();
}

const void *PackedDataManager::Impl::GetRangeTablesForTest() const {
  return range_tables_.get();
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

const UserPOS::POSToken *PackedDataManager::GetUserPOSData() const {
  return manager_impl_->GetUserPOSData();
}

PackedDataManager *PackedDataManager::GetUserPosManager() {
  if (!g_data_manager.get()) {
    LOG(INFO) << "PackedDataManager::GetUserPosManager null!";
    LOG(INFO) << "FLAGS_dataset: [" << FLAGS_dataset << "]";
    if (FLAGS_dataset.empty()) {
      LOG(FATAL) << "PackedDataManager::GetUserPosManager ERROR!";
    } else {
      scoped_ptr<PackedDataManager> data_manager(new PackedDataManager);
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

const POSMatcher *PackedDataManager::GetPOSMatcher() const {
  return manager_impl_->GetPOSMatcher();
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
    const BoundaryData **boundary_data) const {
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
    const SuffixToken **data,
    size_t *size) const {
  manager_impl_->GetSuffixDictionaryData(data, size);
}

void PackedDataManager::GetReadingCorrectionData(
    const ReadingCorrectionItem **array,
    size_t *size) const {
  manager_impl_->GetReadingCorrectionData(array, size);
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
    const EmbeddedDictionary::Token **data,
    size_t *size) const {
  manager_impl_->GetSymbolRewriterData(data, size);
}

#ifndef NO_USAGE_REWRITER
void PackedDataManager::GetUsageRewriterData(
    const ConjugationSuffix **base_conjugation_suffix,
    const ConjugationSuffix **conjugation_suffix_data,
    const int **conjugation_suffix_data_index,
    const UsageDictItem **usage_data_value) const {
  manager_impl_->GetUsageRewriterData(base_conjugation_suffix,
                                      conjugation_suffix_data,
                                      conjugation_suffix_data_index,
                                      usage_data_value);
}
#endif  // NO_USAGE_REWRITER


const uint16 *PackedDataManager::GetRuleIdTableForTest() const {
  return manager_impl_->GetRuleIdTableForTest();
}

const void *PackedDataManager::GetRangeTablesForTest() const {
  return manager_impl_->GetRangeTablesForTest();
}

void RegisterPackedDataManager(PackedDataManager *packed_data_manager) {
  g_data_manager.reset(packed_data_manager);
}

PackedDataManager *GetPackedDataManager() {
  return g_data_manager.get();
}

}  // namespace packed
}  // namespace mozc
