// Copyright 2010, Google Inc.
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

#include "prediction/conversion_predictor.h"

#include <string>
#include "base/base.h"
#include "base/init.h"
#include "base/util.h"
#include "converter/immutable_converter_interface.h"
#include "converter/segments.h"
#include "session/config_handler.h"
#include "session/config.pb.h"

namespace mozc {
namespace {
const size_t kMaxKeySize = 300;   // 300 bytes in UTF8
}

ConversionPredictor::ConversionPredictor()
    : immutable_converter_(
        ImmutableConverterFactory::GetImmutableConverter()) {}

ConversionPredictor::~ConversionPredictor() {}

bool ConversionPredictor::Predict(Segments *segments) const {
  if (segments == NULL) {
    return false;
  }

  if (!GET_CONFIG(use_realtime_conversion)) {
    VLOG(2) << "no realtime conversion";
    return false;
  }

  if (segments->request_type() != Segments::PREDICTION &&
      segments->request_type() != Segments::SUGGESTION) {
    VLOG(2) << "request type is not (PREDICTION|SUGGESTION)";
    return false;
  }

  if (segments->conversion_segments_size() != 1) {
    VLOG(2) << "conversion segment size != 1";
    return false;
  }

  const Segment &segment = segments->conversion_segment(0);
  if (segment.candidates_size() >=
      segments->max_prediction_candidates_size()) {
    return false;
  }

  const string &key = segments->conversion_segment(0).key();
  if (key.empty() || key.size() > kMaxKeySize) {
    return false;
  }

  CHECK(immutable_converter_);

  return immutable_converter_->Convert(segments);
}
}  // namespace mozc
