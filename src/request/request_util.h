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

#ifndef MOZC_REQUEST_REQUEST_UTIL_H_
#define MOZC_REQUEST_REQUEST_UTIL_H_

#include "composer/composer.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"

namespace mozc {
namespace request_util {

inline bool IsHandwriting(const ConversionRequest &conversion_request) {
  return conversion_request.request().is_handwriting() ||
         !conversion_request.composer().GetHandwritingCompositions().empty();
}

inline bool IsAutoPartialSuggestionEnabled(
    const ConversionRequest &conversion_request) {
  return conversion_request.request().auto_partial_suggestion();
}

inline bool IsFindabilityOrientedOrderEnabled(
    const commands::Request &request) {
  return (
      request.auto_partial_suggestion() && request.mixed_conversion() &&
      request.decoder_experiment_params().enable_findability_oriented_order());
}

inline bool IsFindabilityOrientedOrderEnabled(
    const ConversionRequest &conversion_request) {
  return IsFindabilityOrientedOrderEnabled(conversion_request.request());
}

inline bool ShouldFilterNoisyNumberCandidate(
    const ConversionRequest &conversion_request) {
  return conversion_request.create_partial_candidates();
}

}  // namespace request_util
}  // namespace mozc

#endif  // MOZC_REQUEST_REQUEST_UTIL_H_
