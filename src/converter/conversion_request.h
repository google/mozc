// Copyright 2010-2012, Google Inc.
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

#ifndef MOZC_CONVERTER_CONVERSION_REQUEST_H_
#define MOZC_CONVERTER_CONVERSION_REQUEST_H_

#include <string>

#include "base/base.h"
#include "base/logging.h"

namespace mozc {
namespace composer {
class Composer;
}

// Contains utilizable information for conversion, suggestion and prediction,
// including composition, preceding text, etc.
class ConversionRequest {
 public:
  ConversionRequest() : composer_(NULL) {}
  explicit ConversionRequest(const composer::Composer *c) : composer_(c) {}

  bool has_composer() const { return composer_ != NULL; }
  const composer::Composer &composer() const {
    DCHECK(composer_);
    return *composer_;
  }
  void set_composer(const composer::Composer *c) { composer_ = c; }

  const string &preceding_text() const { return preceding_text_; }
  void set_preceding_text(const string &preceding_text) {
    preceding_text_ = preceding_text;
  }

  // TODO(noriyukit): We may need CopyFrom() to perform undo.

 private:
  // Required field
  // Input composer to generate a key for conversion.
  const composer::Composer *composer_;

  // Optional field
  // If nonempty, utilizes this preceding text for conversion.
  string preceding_text_;

  // TODO(noriyukit): Moves all the members of Segments that are irrelevant to
  // this structure, e.g., Segments::user_history_enabled_ and
  // Segments::request_type_. Also, a key for conversion is eligible to live in
  // this class.
};

}  // namespace mozc

#endif  // MOZC_CONVERTER_CONVERSION_REQUEST_H_
