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

#ifndef MOZC_ENGINE_SPELLCHECKER_INTERFACE_H_
#define MOZC_ENGINE_SPELLCHECKER_INTERFACE_H_

#include <optional>
#include <vector>

#include "absl/strings/string_view.h"
#include "composer/query.h"
#include "converter/segments.h"
#include "protocol/commands.pb.h"

namespace mozc::engine {

class SpellcheckerInterface {
 public:
  virtual ~SpellcheckerInterface() = default;

  // Performs spelling correction.
  // `request.text` may contains multiple sentences.
  virtual commands::CheckSpellingResponse CheckSpelling(
      const commands::CheckSpellingRequest &request) const = 0;

  // Performs spelling correction for composition (pre-edit) Hiragana sequence.
  // Both `query` and `context` must be Hiragana input sequence.
  // `request` is passed to determine the keyboard layout.
  // Returns empty result when no correction is required.
  // Returns std::nullopt when the composition spellchecker is not
  // enabled/available.
  virtual std::optional<std::vector<composer::TypeCorrectedQuery>>
  CheckCompositionSpelling(absl::string_view query, absl::string_view context,
                           const commands::Request &request) const = 0;

  // Performs homonym spelling correction.
  virtual void MaybeApplyHomonymCorrection(Segments *segments) const = 0;
};

}  // namespace mozc::engine

#endif  // MOZC_ENGINE_SPELLCHECKER_INTERFACE_H_
