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

// This module provides utilities similar to those in base/number_util.h.
// However, this module is separated from it because of the dependency on
// rewriter/counter_suffix.h.

#ifndef MOZC_REWRITER_NUMBER_COMPOUND_UTIL_H_
#define MOZC_REWRITER_NUMBER_COMPOUND_UTIL_H_

#include <cstdint>

#include "absl/strings/string_view.h"
#include "base/container/serialized_string_array.h"
#include "converter/candidate.h"

namespace mozc {

namespace dictionary {
class PosMatcher;
}

namespace number_compound_util {

enum NumberScriptType {
  NONE = 0,
  HALFWIDTH_ARABIC = 1,
  FULLWIDTH_ARABIC = 2,
  KANJI = 4,
  OLD_KANJI = 8,
};

// Splits a string to number and counter suffix if possible.  For example, input
// "一階" can be split as "一" + "階".  At the same time, script type of
// number can be obtained.  A sorted array of counter suffix needs to be
// provided, which can be obtained using data manager; see
// data_manager/data_manager.h.  Returns false if the input cannot be
// split.
bool SplitStringIntoNumberAndCounterSuffix(
    const SerializedStringArray &suffix_array, absl::string_view input,
    absl::string_view *number, absl::string_view *counter_suffix,
    uint32_t *script_type);

// Checks if the given candidate is number, where candidate is considered as a
// number when satisfying one of the following conditions:
//   1) lid is number
//   2) lid is Kanji number
//   3) lid is general nound and content value consists of number and counter
//      suffix, where counter suffix needs to be provided as a sorted array.
bool IsNumber(const SerializedStringArray &suffix_array,
              const dictionary::PosMatcher &pos_matcher,
              const converter::Candidate &cand);

}  // namespace number_compound_util
}  // namespace mozc

#endif  // MOZC_REWRITER_NUMBER_COMPOUND_UTIL_H_
