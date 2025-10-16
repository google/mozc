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

#ifndef MOZC_CONVERTER_CANDIDATE_H_
#define MOZC_CONVERTER_CANDIDATE_H_

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <string>
#include <tuple>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/number_util.h"
#include "converter/attribute.h"
#include "converter/inner_segment.h"

#ifndef NDEBUG
#define MOZC_CANDIDATE_DEBUG
#define MOZC_CANDIDATE_LOG(result, message) \
  (result)->Dlog(__FILE__, __LINE__, message)

#else  // NDEBUG
#define MOZC_CANDIDATE_LOG(result, message) \
  {                                         \
  }

#endif  // NDEBUG

namespace mozc {
namespace converter {

class Candidate {
 public:
  Candidate() = default;

  enum Command {
    DEFAULT_COMMAND = 0,
    ENABLE_INCOGNITO_MODE,      // enables "incognito mode".
    DISABLE_INCOGNITO_MODE,     // disables "incognito mode".
    ENABLE_PRESENTATION_MODE,   // enables "presentation mode".
    DISABLE_PRESENTATION_MODE,  // disables "presentation mode".
  };

  enum Category {
    DEFAULT_CATEGORY,  // Realtime conversion, history prediction, etc
    SYMBOL,            // Symbol, emoji
    OTHER,             // Misc candidate
  };

  // LINT.IfChange
  std::string key;    // reading
  std::string value;  // surface form
  std::string content_key;
  std::string content_value;

  size_t consumed_key_size = 0;

  // Meta information
  // TODO(taku): Better to introduce a struct to save heap usage.
  // These fields are mostly empty.
  std::string prefix;
  std::string suffix;
  // Description including description type and message
  std::string description;
  // Description for A11y support (e.g. "あ。ヒラガナ あ")
  std::string a11y_description;
  // Actual value to be displayed. Uses it to encode meta information
  // in the value. e.g., puts "_" to clarify the unrecognizable white spaces.
  std::string display_value;

  // Usage ID
  int32_t usage_id = 0;
  // Title of the usage containing basic form of this candidate.
  std::string usage_title;
  // Content of the usage.
  std::string usage_description;

  // Context "sensitive" candidate cost.
  // Taking adjacent words/nodes into consideration.
  // Basically, candidate is sorted by this cost.
  int32_t cost = 0;
  // Context "free" candidate cost
  // NOT taking adjacent words/nodes into consideration.
  int32_t wcost = 0;
  // (cost without transition cost between left/right boundaries)
  // Cost of only transitions (cost without word cost adjacent context)
  int32_t structure_cost = 0;

  // lid of left-most node
  uint16_t lid = 0;
  // rid of right-most node
  uint16_t rid = 0;

  // Attributes of this candidate. Can set multiple attributes
  // defined in enum |Attribute|.
  uint32_t attributes = 0;

  Category category = DEFAULT_CATEGORY;

  // Candidate style. This is not a bit-field.
  // The style is defined in enum |Style|.
  NumberUtil::NumberString::Style style =
      NumberUtil::NumberString::DEFAULT_STYLE;

  // Command of this candidate. This is not a bit-field.
  // The style is defined in enum |Command|.
  Command command = DEFAULT_COMMAND;

  // Boundary information for real time conversion.  This will be set only for
  // real time conversion result candidates.  Each element is the encoded
  // lengths of key, value, content key and content value.
  InnerSegmentBoundary inner_segment_boundary;
  // LINT.ThenChange(//converter/segments_matchers.h)

  // The original cost before rescoring. Used for debugging purpose.
  int32_t cost_before_rescoring = 0;
#ifdef MOZC_CANDIDATE_DEBUG
  void Dlog(absl::string_view filename, int line,
            absl::string_view message) const;
  mutable std::string log;
#endif  // MOZC_CANDIDATE_DEBUG

  InnerSegments inner_segments() const {
    return InnerSegments(key, value, content_key, content_value,
                         inner_segment_boundary);
  }

  // Clears the Candidate with default values. Note that the default
  // constructor already does the same so you don't need to call Clear
  // explicitly.
  void Clear();

  // Returns functional key.
  // functional_key =
  // key.substr(content_key.size(), key.size() - content_key.size());
  absl::string_view functional_key() const;

  // Returns functional value.
  // functional_value =
  // value.substr(content_value.size(), value.size() - content_value.size());
  absl::string_view functional_value() const;

  std::string DebugString() const;

  friend std::ostream& operator<<(std::ostream& os,
                                  const Candidate& candidate) {
    return os << candidate.DebugString();
  }
};

// Inlining basic accessors here.
inline absl::string_view Candidate::functional_key() const {
  return absl::string_view(key).substr(
      std::min(key.size(), content_key.size()));
}

inline absl::string_view Candidate::functional_value() const {
  return absl::string_view(value).substr(
      std::min(value.size(), content_value.size()));
}

}  // namespace converter
}  // namespace mozc

#endif  // MOZC_CONVERTER_CANDIDATE_H_
