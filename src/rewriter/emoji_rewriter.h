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

#ifndef MOZC_REWRITER_EMOJI_REWRITER_H_
#define MOZC_REWRITER_EMOJI_REWRITER_H_

#include <cstddef>
#include <utility>

#include "absl/strings/string_view.h"
#include "base/container/serialized_string_array.h"
#include "converter/segments.h"
#include "data_manager/data_manager.h"
#include "data_manager/emoji_data.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"

namespace mozc {

// EmojiRewriter class adds UTF-8 emoji characters in converted candidates of
// given segments, if each segment has a special key to convert.
// Added emoji characters are chosen by Yomi (reading of it) registered in
// a dictionary. If a segment has a key "えもじ", all emoji characters are
// pushed to its candidate list.
//
// Usage:
//
//   mozc::Segments segments;
//   mozc::Segment *segment = segments.add_segment();
//   mozc::Segment::Candidate *candidate = segment->add_candidate();
//   candidate->set_key("えもじ");
//
//   // Use one of data manager from data_manager/.
//   mozc::EmojiRewriter rewriter(data_manager);
//   rewriter.Rewrite(mozc::ConvresionRequest(), &segments);
//
// Here, the first segment of segments is expected to have all emoji
// characters in its candidates' values.  You can see them as such:
//
//   for (size_t i = 0; i < segment->candidate_size(); ++i) {
//     LOG(INFO) << segment->candidate(i).value;
//   }
class EmojiRewriter : public RewriterInterface {
 public:
  static constexpr size_t kEmojiDataByteLength = 28;
  using IteratorRange = std::pair<EmojiDataIterator, EmojiDataIterator>;

  explicit EmojiRewriter(const DataManager &data_manager);
  EmojiRewriter(const EmojiRewriter &) = delete;
  EmojiRewriter &operator=(const EmojiRewriter &) = delete;

  int capability(const ConversionRequest &request) const override;

  // Returns true if emoji candidates are added.  When user settings are set
  // not to use EmojiRewriter, does nothing other than returning false.
  // Otherwise, main process are done in ReriteCandidates().
  // A reference to a ConversionRequest instance is not used, but it is required
  // because of the interface.
  bool Rewrite(const ConversionRequest &request,
               Segments *segments) const override;

  // Returns true if the given candidate includes emoji characters.
  // TODO(peria, hidehiko): Unify this checker and IsEmojiEntry defined in
  //     predictor/user_history_predictor.cc.  If you make similar functions
  //     before the merging in case, put a same note to avoid twisted
  //     dependency.
  static bool IsEmojiCandidate(const Segment::Candidate &candidate);

 private:
  EmojiDataIterator begin() const {
    return EmojiDataIterator(token_array_data_.data());
  }
  EmojiDataIterator end() const {
    return EmojiDataIterator(token_array_data_.data() +
                             token_array_data_.size());
  }

  // Adds emoji candidates on each segment of given segments, if it has a
  // specific string as a key based on a dictionary.  If a segment's value is
  // "えもじ", adds all emoji candidates.
  // Returns true if emoji candidates are added in any segment.
  bool RewriteCandidates(Segments *segments) const;

  IteratorRange LookUpToken(absl::string_view key) const;

  absl::string_view token_array_data_;
  SerializedStringArray string_array_;
};

}  // namespace mozc

#endif  // MOZC_REWRITER_EMOJI_REWRITER_H_
