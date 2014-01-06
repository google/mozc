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
#include "dictionary/pos_group.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/user_pos.h"

DEFINE_string(output, "", "Output data file name");

namespace mozc {
namespace {

#include "data_manager/@DIR@/user_pos_data.h"
#include "data_manager/@DIR@/pos_matcher_data.h"

}  // namespace

bool OutputData(const string &file_path) {
  packed::SystemDictionaryDataPacker packer(Version::GetMozcVersion());
  packer.SetPosTokens(kPOSToken, arraysize(kPOSToken));
  packer.SetPosMatcherData(kRuleIdTable, arraysize(kRuleIdTable),
                           kRangeTables, arraysize(kRangeTables));
  return packer.Output(file_path, false);
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
