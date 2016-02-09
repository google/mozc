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

#include <string>

#include "base/file_stream.h"
#include "base/flags.h"
#include "base/init_mozc.h"
#include "base/logging.h"
#include "base/util.h"
#include "base/version.h"
#include "converter/boundary_struct.h"
#include "data_manager/packed/system_dictionary_data_packer.h"
#include "dictionary/pos_group.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suffix_dictionary_token.h"
#include "dictionary/user_pos.h"
#include "rewriter/correction_rewriter.h"
#include "rewriter/counter_suffix.h"
#include "rewriter/embedded_dictionary.h"
#ifndef NO_USAGE_REWRITER
#include "rewriter/usage_rewriter_data_structs.h"
#endif  // NO_USAGE_REWRITER

DEFINE_string(mozc_data, "", "Data set file to be packed");
DEFINE_string(mozc_data_magic, "", "Magic number for data set file");
DEFINE_string(output, "", "Output data file name");
DEFINE_string(dictionary_version, "", "dictionary version");
DEFINE_bool(make_header, false, "make header mode");
DEFINE_bool(use_gzip, false, "use gzip");

namespace mozc {
namespace {

#include "data_manager/@DIR@/boundary_data.h"
#include "data_manager/@DIR@/pos_group_data.h"
#include "data_manager/@DIR@/pos_matcher_data.h"
#include "data_manager/@DIR@/reading_correction_data.h"
#include "data_manager/@DIR@/segmenter_data.h"
#include "data_manager/@DIR@/suffix_data.h"
#include "data_manager/@DIR@/symbol_rewriter_data.h"
#include "data_manager/@DIR@/user_pos_data.h"
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
  // The following two arrays contain sentinel elements but the packer doesn't
  // expect them.  So pass the shinked ranges of the arrays.  Note that sentinel
  // elements are not required at runtime.
  packer.SetPosMatcherData(kRuleIdTable, arraysize(kRuleIdTable) - 1,
                           kRangeTables, arraysize(kRangeTables) - 1);
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

  string magic;
  CHECK(Util::Unescape(FLAGS_mozc_data_magic, &magic))
      << "Invalid hex-escaped string: " << FLAGS_mozc_data_magic;
  packer.SetMozcData(InputFileStream(FLAGS_mozc_data.c_str(),
                                     ios_base::in | ios_base::binary).Read(),
                     magic);
  if (FLAGS_make_header) {
    return packer.OutputHeader(file_path, FLAGS_use_gzip);
  } else {
    return packer.Output(file_path, FLAGS_use_gzip);
  }
}

}  // namespace mozc

int main(int argc, char **argv) {
  mozc::InitMozc(argv[0], &argc, &argv, false);

  CHECK(!FLAGS_mozc_data.empty()) << "--mozc_data is required";
  CHECK(!FLAGS_output.empty()) << "--output flag is required";
  CHECK(mozc::OutputData(FLAGS_output))
      << "Data output error: "<< FLAGS_output;

  return 0;
}
