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

#ifndef MOZC_PREDICTION_ZERO_QUERY_DECODER_H_
#define MOZC_PREDICTION_ZERO_QUERY_DECODER_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "dictionary/dictionary_interface.h"
#include "engine/modules.h"
#include "prediction/result.h"
#include "prediction/zero_query_dict.h"
#include "request/conversion_request.h"

namespace mozc::prediction {

// Decoder specialized for zero-query predictions based on history context,
// preceding text, number counter suffixes, emoticon/emoji dictionaries,
// supplemental model, and suffix dictionary.
//
// Concrete Conversion Examples:
//  1. Number counter suffixes:
//     - History / Preceding text: "12" -> Candidates: "月", "日", "時", "個",
//     "人"
//     - History: "10" -> Candidates: "時", "分", "月"
//  2. General zero-query / context-based suggestions:
//     - History / Preceding text: "あけまして" -> Candidates:
//     "おめでとうございます"
//     - History / Preceding text: "よろしく" -> Candidates: "お願いします"
//  3. Emoji and Emoticon zero-query lookup:
//     - History: "猫" -> Candidates: "😾", "🐱"
//     - History: "ああ" -> Candidates: "( •̀ㅁ•́;)"
//  4. Email domain completion:
//     - History / Preceding text: "user@" -> Candidates: "gmail.com",
//     "google.com"
class ZeroQueryDecoder {
 public:
  explicit ZeroQueryDecoder(const engine::Modules& modules);

  // Decodes and returns zero query results for the given request.
  std::vector<Result> Decode(const ConversionRequest& request) const;

 private:
  // Returns true if we add zero query result.
  bool AggregateNumberZeroQuery(const ConversionRequest& request,
                                std::vector<Result>* results) const;

  // Looks up the given range and appends zero query candidate list for |key|
  // to |results|.
  void GetZeroQueryCandidatesForKey(const ConversionRequest& request,
                                    absl::string_view key,
                                    const ZeroQueryDict& dict, uint16_t lid,
                                    uint16_t rid,
                                    std::vector<Result>* results) const;

  friend class ZeroQueryDecoderTestPeer;

  const engine::Modules& modules_;
  ZeroQueryDict zero_query_dict_;
  ZeroQueryDict zero_query_number_dict_;
  const dictionary::DictionaryInterface& suffix_dictionary_;
  uint16_t counter_suffix_word_id_ = 0;
  uint16_t zip_code_id_ = 0;
  uint16_t unknown_id_ = 0;
};

}  // namespace mozc::prediction

#endif  // MOZC_PREDICTION_ZERO_QUERY_DECODER_H_
