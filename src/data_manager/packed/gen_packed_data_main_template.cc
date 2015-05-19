// Copyright 2010-2014, Google Inc.
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

#include <string>

#include "base/flags.h"
#include "base/logging.h"
#include "base/version.h"
#include "converter/boundary_struct.h"
#include "data_manager/packed/system_dictionary_data_packer.h"
#include "dictionary/suffix_dictionary_token.h"
#include "dictionary/pos_group.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/user_pos.h"
#include "rewriter/correction_rewriter.h"
#include "rewriter/counter_suffix.h"
#include "rewriter/embedded_dictionary.h"
#ifndef NO_USAGE_REWRITER
#include "rewriter/usage_rewriter_data_structs.h"
#endif  // NO_USAGE_REWRITER

DEFINE_string(output, "", "Output data file name");
DEFINE_string(dictionary_version, "", "dictionary version");
DEFINE_bool(make_header, false, "make header mode");
DEFINE_bool(use_gzip, false, "use gzip");

namespace mozc {
namespace {

#include "data_manager/@DIR@/user_pos_data.h"
#include "data_manager/@DIR@/pos_matcher_data.h"
#include "data_manager/@DIR@/pos_group_data.h"
#include "data_manager/@DIR@/boundary_data.h"
#include "data_manager/@DIR@/suffix_data.h"
#include "data_manager/@DIR@/reading_correction_data.h"
#include "data_manager/@DIR@/segmenter_data.h"
#include "data_manager/@DIR@/embedded_collocation_suppression_data.h"
#include "data_manager/@DIR@/suggestion_filter_data.h"
#include "data_manager/@DIR@/embedded_connection_data.h"
#include "data_manager/@DIR@/embedded_dictionary_data.h"
#include "data_manager/@DIR@/embedded_collocation_data.h"
#include "data_manager/@DIR@/symbol_rewriter_data.h"
#ifndef NO_USAGE_REWRITER
#include "rewriter/usage_rewriter_data.h"
#endif  // NO_USAGE_REWRITER
#include "data_manager/@DIR@/counter_suffix_data.h"

}  // namespace

bool OutputData(const string &file_path) {
  string dictionary_version = Version::GetMozcVersion();
  if (!FLAGS_dictionary_version.empty()) {
    dictionary_version = FLAGS_dictionary_version;
  }
  packed::SystemDictionaryDataPacker packer(dictionary_version);
  packer.SetPosTokens(kPOSToken, arraysize(kPOSToken));
  packer.SetPosMatcherData(kRuleIdTable, arraysize(kRuleIdTable),
                           kRangeTables, arraysize(kRangeTables));
  packer.SetLidGroupData(kLidGroup, arraysize(kLidGroup));
  packer.SetBoundaryData(kBoundaryData, arraysize(kBoundaryData));
  packer.SetSuffixTokens(kSuffixTokens, arraysize(kSuffixTokens));
  packer.SetReadingCorretions(kReadingCorrections,
                              arraysize(kReadingCorrections));
  packer.SetSegmenterData(kCompressedLSize,
                          kCompressedRSize,
                          kCompressedLIDTable,
                          arraysize(kCompressedLIDTable),
                          kCompressedRIDTable,
                          arraysize(kCompressedRIDTable),
                          kSegmenterBitArrayData_data,
                          kSegmenterBitArrayData_size);
  packer.SetSuggestionFilterData(kSuggestionFilterData_data,
                                 kSuggestionFilterData_size);
  packer.SetConnectionData(kConnectionData_data, kConnectionData_size);
  packer.SetDictionaryData(kDictionaryData_data, kDictionaryData_size);
  packer.SetCollocationData(CollocationData::kExistenceFilter_data,
                            CollocationData::kExistenceFilter_size);
  packer.SetCollocationSuppressionData(
      CollocationSuppressionData::kExistenceFilter_data,
      CollocationSuppressionData::kExistenceFilter_size);
  packer.SetSymbolRewriterData(kSymbolData_token_data, kSymbolData_token_size);
#ifndef NO_USAGE_REWRITER
  packer.SetUsageRewriterData(kConjugationNum,
                              kBaseConjugationSuffix,
                              kConjugationSuffixData,
                              kConjugationSuffixDataIndex,
                              kUsageDataSize,
                              kUsageData_value);
#endif  // NO_USAGE_REWRITER
  packer.SetCounterSuffixSortedArray(kCounterSuffixes,
                                     arraysize(kCounterSuffixes));
  if (FLAGS_make_header) {
    return packer.OutputHeader(file_path, FLAGS_use_gzip);
  } else {
    return packer.Output(file_path, FLAGS_use_gzip);
  }
}

}  // namespace mozc

int main(int argc, char **argv) {
  InitGoogle(argv[0], &argc, &argv, false);

  if (FLAGS_output.empty()) {
    LOG(FATAL) << "output flag is needed";
    return 1;
  }
  if (!mozc::OutputData(FLAGS_output)) {
    LOG(FATAL) << "Data output error : "<< FLAGS_output;
    return 1;
  }
  return 0;
}
