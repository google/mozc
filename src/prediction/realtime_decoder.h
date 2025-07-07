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

#ifndef MOZC_PREDICTION_REALTIME_DECODER_H_
#define MOZC_PREDICTION_REALTIME_DECODER_H_

#include <functional>
#include <optional>
#include <vector>

#include "absl/strings/string_view.h"
#include "converter/converter_interface.h"
#include "converter/immutable_converter_interface.h"
#include "prediction/result.h"

namespace mozc::prediction {

// Wrapper class to perform realtime decoding with the converter.
// This wrapper hides the Segments, Converter, and ImmutableConverter from
// Predictor.
class RealtimeDecoder {
 public:
  RealtimeDecoder() = default;
  RealtimeDecoder(const ImmutableConverterInterface &immutable_converter,
                  const ConverterInterface &converter)
      : immutable_converter_(std::cref(immutable_converter)),
        converter_(std::cref(converter)) {}
  virtual ~RealtimeDecoder() = default;

  // Decodes `request`. The request type must not be CONVERSION because
  // we assume that Decode doesn't return multiple-segments.
  virtual std::vector<Result> Decode(const ConversionRequest &request) const;

  // value is reading, key is the input.
  virtual std::vector<Result> ReverseDecode(
      const ConversionRequest &request) const;

 private:
  bool PushBackTopConversionResult(const ConversionRequest &request,
                                   std::vector<Result> *results) const;

  const ImmutableConverterInterface &immutable_converter() const {
    DCHECK(immutable_converter_.has_value());
    return immutable_converter_.value().get();
  }

  const ConverterInterface &converter() const {
    DCHECK(converter_.has_value());
    return converter_.value().get();
  }

  // optional<std::reference_wrapper<const T>> allows to store pure reference
  // with an undefined state. It is safer than pointer as the ownership is
  // cleaner. absl::Nonnull cannot be used as we want to use an undefined state
  // for testing.
  std::optional<std::reference_wrapper<const ImmutableConverterInterface>>
      immutable_converter_;
  std::optional<std::reference_wrapper<const ConverterInterface>> converter_;
};

}  // namespace mozc::prediction

#endif  // MOZC_PREDICTION_REALTIME_DECODER_H_
