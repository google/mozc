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

#ifndef MOZC_PREDICTION_HANDWRITING_DECODER_H_
#define MOZC_PREDICTION_HANDWRITING_DECODER_H_

#include <optional>
#include <string>
#include <vector>

#include "dictionary/dictionary_interface.h"
#include "engine/modules.h"
#include "prediction/realtime_decoder.h"
#include "prediction/result.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"

namespace mozc::prediction {

// Decoder specialized for handwriting composition recognition events.
// It generates:
//  1. As-is recognition string candidates directly from stroke recognition.
//  2. Reverse-converted dictionary lookup candidates matching non-Hiragana
//  constraints.
//
// Concrete Conversion Examples:
//  1. Direct handwriting recognition result:
//     - Handwriting composition: "かん字じ典" (prob: 0.99)
//       -> Candidate: "かん字じ典" (as-is top candidate, wcost = 0)
//     - Handwriting composition: "かlv字じ典" (prob: 0.01)
//       -> Candidate: "かlv字じ典" (as-is candidate with penalty)
//  2. Reverse conversion + exact dictionary lookup:
//     - Handwriting composition: "かん字じ典"
//       -> ReverseDecode reading: "かんじじてん"
//       -> Constraint: contains "字"
//       -> Dictionary Exact Lookup: "漢字辞典", "漢字字典", "換字字典"
//  3. Transliteration (T13N) constraint matching:
//     - Handwriting composition: "キた"
//       -> ReverseDecode reading: "きた"
//       -> Constraint: contains "キ"
//       -> Matches "キた" (avoids over-generating unconstrained "北" or "来た")
//  4. Non-Hiragana composition skip:
//     - Handwriting composition: "南" (pure Kanji, no Hiragana)
//       -> Candidate: "南" (as-is result only, skips ReverseDecode to save CPU)
class HandwritingDecoder {
 public:
  struct HandwritingQueryInfo {
    std::string query;
    std::vector<std::string> constraints;
  };

  HandwritingDecoder(const engine::Modules& modules,
                     const RealtimeDecoder& realtime_decoder);

  // Decodes handwriting candidates for the given request.
  std::vector<Result> Decode(const ConversionRequest& request) const;

  // Generates `HandwritingQueryInfo` for the given composition event.
  std::optional<HandwritingQueryInfo> GenerateQueryForHandwriting(
      const ConversionRequest& request,
      const commands::SessionCommand::CompositionEvent& composition_event)
      const;

 private:
  const dictionary::DictionaryInterface& dictionary_;
  const RealtimeDecoder& realtime_decoder_;
};

}  // namespace mozc::prediction

#endif  // MOZC_PREDICTION_HANDWRITING_DECODER_H_
