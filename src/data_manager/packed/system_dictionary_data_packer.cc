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

#include "data_manager/packed/system_dictionary_data_packer.h"

#include <string>

#include "base/base.h"
#include "base/codegen_bytearray_stream.h"
#include "base/file_stream.h"
#include "base/protobuf/gzip_stream.h"
#include "base/protobuf/zero_copy_stream_impl.h"
#include "base/logging.h"
#include "base/version.h"
#include "converter/boundary_struct.h"
#include "data_manager/packed/system_dictionary_data.pb.h"
#include "dictionary/suffix_dictionary_token.h"
#include "dictionary/pos_group.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/user_pos.h"
#include "rewriter/correction_rewriter.h"
#include "rewriter/embedded_dictionary.h"
#ifndef NO_USAGE_REWRITER
#include "rewriter/usage_rewriter_data_structs.h"
#endif  // NO_USAGE_REWRITER

namespace mozc {
namespace packed {
namespace {
const int kSystemDictionaryFormatVersion = 1;
}

SystemDictionaryDataPacker::SystemDictionaryDataPacker(const string &version) {
  system_dictionary_.reset(new SystemDictionaryData());
  system_dictionary_->set_product_version(version);
  system_dictionary_->set_format_version(kSystemDictionaryFormatVersion);
}

SystemDictionaryDataPacker::~SystemDictionaryDataPacker() {
}

void SystemDictionaryDataPacker::SetPosTokens(
    const UserPOS::POSToken *pos_token_data,
    size_t token_count) {
  for (size_t i = 0; i < token_count; ++i) {
    SystemDictionaryData::PosToken *pos_token =
        system_dictionary_->add_pos_tokens();
    if (pos_token_data[i].pos) {
      pos_token->set_pos(pos_token_data[i].pos);
    }
    for (size_t j = 0; j < pos_token_data[i].conjugation_size; ++j) {
      SystemDictionaryData::PosToken::ConjugationType *conjugation_form
          = pos_token->add_conjugation_forms();
      if (pos_token_data[i].conjugation_form[j].key_suffix) {
        conjugation_form->set_key_suffix(
            pos_token_data[i].conjugation_form[j].key_suffix);
      }
      if (pos_token_data[i].conjugation_form[j].value_suffix) {
        conjugation_form->set_value_suffix(
            pos_token_data[i].conjugation_form[j].value_suffix);
      }
      conjugation_form->set_id(
          pos_token_data[i].conjugation_form[j].id);
    }
  }
}

void SystemDictionaryDataPacker::SetPosMatcherData(
    const uint16 *rule_id_table,
    size_t rule_id_table_count,
    const POSMatcher::Range *const *range_tables,
    size_t range_tables_count) {
  SystemDictionaryData::PosMatcherData *pos_matcher_data =
      system_dictionary_->mutable_pos_matcher_data();
  for (size_t i = 0; i < rule_id_table_count; ++i) {
    pos_matcher_data->add_rule_id_table(rule_id_table[i]);
  }
  for (size_t i = 0; i < range_tables_count; ++i) {
    SystemDictionaryData::PosMatcherData::RangeTable *range_table =
        pos_matcher_data->add_range_tables();
    for (size_t j = 0;
         range_tables[i][j].lower != static_cast<uint16>(0xFFFF);
         ++j) {
      SystemDictionaryData::PosMatcherData::RangeTable::Range *range
          = range_table->add_ranges();
      range->set_lower(range_tables[i][j].lower);
      range->set_upper(range_tables[i][j].upper);
    }
  }
}

void SystemDictionaryDataPacker::SetBoundaryData(
    const BoundaryData *boundary_data,
    size_t boundary_data_count) {
  for (size_t i = 0; i < boundary_data_count; ++i) {
    SystemDictionaryData::BoundaryData *boundary =
        system_dictionary_->add_boundary_data();
    boundary->set_prefix_penalty(boundary_data[i].prefix_penalty);
    boundary->set_suffix_penalty(boundary_data[i].suffix_penalty);
  }
}

void SystemDictionaryDataPacker::SetLidGroupData(
    const void *lid_group_data,
    size_t lid_group_data_size) {
  system_dictionary_->set_lid_group_data(
    lid_group_data,
    lid_group_data_size);
}

void SystemDictionaryDataPacker::SetSuffixTokens(
    const SuffixToken *suffix_tokens,
    size_t suffix_tokens_count) {
  for (size_t i = 0; i < suffix_tokens_count; ++i) {
    SystemDictionaryData::SuffixToken *suffix_token =
        system_dictionary_->add_suffix_tokens();
    if (suffix_tokens[i].key) {
      suffix_token->set_key(suffix_tokens[i].key);
    }
    if (suffix_tokens[i].value) {
      suffix_token->set_value(suffix_tokens[i].value);
    }
    suffix_token->set_lid(suffix_tokens[i].lid);
    suffix_token->set_rid(suffix_tokens[i].rid);
    suffix_token->set_wcost(suffix_tokens[i].wcost);
  }
}

void SystemDictionaryDataPacker::SetReadingCorretions(
    const ReadingCorrectionItem *reading_corrections,
    size_t reading_corrections_count) {
  for (size_t i = 0; i < reading_corrections_count; ++i) {
    SystemDictionaryData::ReadingCorrectionItem *item =
        system_dictionary_->add_reading_corrections();
    if (reading_corrections[i].value) {
      item->set_value(reading_corrections[i].value);
    }
    if (reading_corrections[i].error) {
      item->set_error(reading_corrections[i].error);
    }
    if (reading_corrections[i].correction) {
      item->set_correction(reading_corrections[i].correction);
    }
  }
}

void SystemDictionaryDataPacker::SetSegmenterData(
    size_t compressed_l_size,
    size_t compressed_r_size,
    const uint16 *compressed_lid_table,
    size_t compressed_lid_table_size,
    const uint16 *compressed_rid_table,
    size_t compressed_rid_table_size,
    const char *segmenter_bit_array_data,
    size_t segmenter_bit_array_data_size) {
  SystemDictionaryData::SegmenterData *segmenter =
      system_dictionary_->mutable_segmenter_data();
  segmenter->set_compressed_l_size(compressed_l_size);
  segmenter->set_compressed_r_size(compressed_r_size);
  for (size_t i = 0; i < compressed_lid_table_size; ++i) {
    segmenter->add_compressed_lid_table(compressed_lid_table[i]);
  }
  for (size_t i = 0; i < compressed_rid_table_size; ++i) {
    segmenter->add_compressed_rid_table(compressed_rid_table[i]);
  }
  segmenter->set_bit_array_data(segmenter_bit_array_data,
                                segmenter_bit_array_data_size);
}

void SystemDictionaryDataPacker::SetSuggestionFilterData(
    const void *suggestion_filter_data,
    size_t suggestion_filter_data_size) {
  system_dictionary_->set_suggestion_filter_data(
    suggestion_filter_data,
    suggestion_filter_data_size);
}

void SystemDictionaryDataPacker::SetConnectionData(
    const void *connection_data,
    size_t connection_data_size) {
  system_dictionary_->set_connection_data(
    connection_data,
    connection_data_size);
}

void SystemDictionaryDataPacker::SetDictionaryData(
    const void *dictionary_data,
    size_t dictionary_data_size) {
  system_dictionary_->set_dictionary_data(
    dictionary_data,
    dictionary_data_size);
}

void SystemDictionaryDataPacker::SetCollocationData(
    const void *collocation_data,
    size_t collocation_data_size) {
  system_dictionary_->set_collocation_data(
    collocation_data,
    collocation_data_size);
}

void SystemDictionaryDataPacker::SetCollocationSuppressionData(
    const void *collocation_suppression_data,
    size_t collocation_suppression_data_size) {
  system_dictionary_->set_collocation_suppression_data(
    collocation_suppression_data,
    collocation_suppression_data_size);
}

void SystemDictionaryDataPacker::SetSymbolRewriterData(
    const mozc::EmbeddedDictionary::Token *token_data,
    size_t token_size) {
  SystemDictionaryData::EmbeddedDictionary *symbol_dictionary =
      system_dictionary_->mutable_symbol_dictionary();
  for (size_t i = 0; i < token_size; ++i) {
    SystemDictionaryData::EmbeddedDictionary::Token *token =
        symbol_dictionary->add_tokens();
    token->set_key(token_data[i].key);
    for (size_t j = 0; j < token_data[i].value_size; ++j) {
      const mozc::EmbeddedDictionary::Value &value_data =
        token_data[i].value[j];
      SystemDictionaryData::EmbeddedDictionary::Value *value =
        token->add_values();
      if (value_data.value) {
        value->set_value(value_data.value);
      }
      if (value_data.description) {
        value->set_description(value_data.description);
      }
      if (value_data.additional_description) {
        value->set_additional_description(value_data.additional_description);
      }
      value->set_lid(value_data.lid);
      value->set_rid(value_data.rid);
      value->set_cost(value_data.cost);
    }
  }
}

#ifndef NO_USAGE_REWRITER
void SystemDictionaryDataPacker::SetUsageRewriterData(
    int conjugation_num,
    const ConjugationSuffix *base_conjugation_suffix,
    const ConjugationSuffix *conjugation_suffix_data,
    const int *conjugation_suffix_data_index,
    size_t usage_data_size,
    const UsageDictItem *usage_data_value) {
  SystemDictionaryData::UsageRewriterData *usage_rewriter_data =
      system_dictionary_->mutable_usage_rewriter_data();
  for (size_t i = 0; i < conjugation_num; ++i) {
    SystemDictionaryData::UsageRewriterData::Conjugation *conjugation =
      usage_rewriter_data->add_conjugations();
    if (base_conjugation_suffix[i].value_suffix) {
      conjugation->mutable_base_suffix()->set_value_suffix(
          base_conjugation_suffix[i].value_suffix);
    }

    if (base_conjugation_suffix[i].key_suffix) {
      conjugation->mutable_base_suffix()->set_key_suffix(
          base_conjugation_suffix[i].key_suffix);
    }
    for (size_t j = conjugation_suffix_data_index[i];
         j < conjugation_suffix_data_index[i + 1];
         ++j) {
      SystemDictionaryData::UsageRewriterData::ConjugationSuffix *suffix =
        conjugation->add_conjugation_suffixes();
      if (conjugation_suffix_data[j].value_suffix) {
        suffix->set_value_suffix(conjugation_suffix_data[j].value_suffix);
      }
      if (conjugation_suffix_data[j].key_suffix) {
        suffix->set_key_suffix(conjugation_suffix_data[j].key_suffix);
      }
    }
  }
  for (size_t i = 0; i < usage_data_size; ++i) {
    SystemDictionaryData::UsageRewriterData::UsageDictItem *item =
      usage_rewriter_data->add_usage_data_values();
    item->set_id(usage_data_value[i].id);
    if (usage_data_value[i].key) {
      item->set_key(usage_data_value[i].key);
    }
    if (usage_data_value[i].value) {
      item->set_value(usage_data_value[i].value);
    }
    item->set_conjugation_id(usage_data_value[i].conjugation_id);
    if (usage_data_value[i].meaning) {
      item->set_meaning(usage_data_value[i].meaning);
    }
  }
}
#endif  // NO_USAGE_REWRITER

bool SystemDictionaryDataPacker::Output(const string &file_path,
                                        bool use_gzip) {
  OutputFileStream output(file_path.c_str(),
                          ios::out | ios::binary | ios::trunc);
  if (use_gzip) {
    protobuf::io::OstreamOutputStream zero_copy_output(&output);
    protobuf::io::GzipOutputStream gzip_stream(&zero_copy_output);
    if (!system_dictionary_->SerializeToZeroCopyStream(&gzip_stream) ||
        !gzip_stream.Close()) {
      LOG(ERROR) << "Serialize to gzip stream failed.";
    }
  } else {
    if (!system_dictionary_->SerializeToOstream(&output)) {
      LOG(ERROR) << "Failed to write data to " << file_path;
      return false;
    }
  }
  return true;
}

bool SystemDictionaryDataPacker::OutputHeader(
    const string &file_path,
    bool use_gzip) {
  scoped_ptr<ostream> output_stream(
      new mozc::OutputFileStream(file_path.c_str(), ios::out | ios::trunc));
  CodeGenByteArrayOutputStream *codegen_stream;
  output_stream.reset(
      codegen_stream = new mozc::CodeGenByteArrayOutputStream(
          output_stream.release(),
          mozc::codegenstream::OWN_STREAM));
  codegen_stream->OpenVarDef("PackedSystemDictionary");
  if (use_gzip) {
    protobuf::io::OstreamOutputStream zero_copy_output(output_stream.get());
    protobuf::io::GzipOutputStream gzip_stream(&zero_copy_output);
    if (!system_dictionary_->SerializeToZeroCopyStream(&gzip_stream) ||
        !gzip_stream.Close()) {
      LOG(ERROR) << "Serialize to gzip stream failed.";
    }
  } else {
    if (!system_dictionary_->SerializeToOstream(output_stream.get())) {
      LOG(ERROR) << "Failed to write data to " << file_path;
      return false;
    }
  }
  return true;
}

}  // namespace packed
}  // namespace mozc
