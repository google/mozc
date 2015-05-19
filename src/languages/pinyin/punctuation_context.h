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

// Punctuation mode context for pinyin IME.

#ifndef MOZC_LANGUAGES_PINYIN_PUNCTUATION_CONTEXT_H_
#define MOZC_LANGUAGES_PINYIN_PUNCTUATION_CONTEXT_H_

#include <string>
#include <vector>

#include "base/port.h"
#include "languages/pinyin/pinyin_context_interface.h"

// Suggests punctuation.
// If special key '`' is input first, punctuation mode is turned on, and some
// candidates related to input will be generated.
// If some key input after special key, first key is cleared from the context.
// If other punctuation key is input first, this class directly commits some
// punctuation.

namespace mozc {
namespace pinyin {
struct SessionConfig;

namespace punctuation {
class PunctuationTableInterface;

class PunctuationContext : public PinyinContextInterface {
 public:
  explicit PunctuationContext(const SessionConfig &session_config);
  virtual ~PunctuationContext();

  bool Insert(char ch);
  void Commit();
  void CommitPreedit();
  void Clear();
  void ClearCommitText();

  bool MoveCursorRight();
  bool MoveCursorLeft();
  bool MoveCursorRightByWord();
  bool MoveCursorLeftByWord();
  bool MoveCursorToBeginning();
  bool MoveCursorToEnd();

  bool SelectCandidate(size_t index);
  bool FocusCandidate(size_t index);
  bool FocusCandidatePrev();
  bool FocusCandidateNext();
  bool ClearCandidateFromHistory(size_t index);

  bool RemoveCharBefore();
  bool RemoveCharAfter();
  bool RemoveWordBefore();
  bool RemoveWordAfter();

  void ReloadConfig();

  const string &commit_text() const;
  const string &input_text() const;
  const string &selected_text() const;
  const string &conversion_text() const;
  const string &rest_text() const;
  const string &auxiliary_text() const;

  size_t cursor() const;
  size_t focused_candidate_index() const;
  size_t candidates_size() const;
  void GetCandidates(vector<string> *candidates) const;

 private:
  friend class PunctuationContextTest;

  bool DirectCommit(char ch);
  void UpdateAuxiliaryText();
  void UpdateCandidates();
  bool MoveCursorInternal(size_t index);

  bool is_initial_state_;
  string commit_text_;
  // input_text contains only ASCII cahracters.
  string input_text_;
  string selected_text_;
  string rest_text_;
  string auxiliary_text_;
  size_t cursor_;
  size_t focused_candidate_index_;
  vector<string> candidates_;
  const string empty_text_;
  const PunctuationTableInterface *table_;
  const SessionConfig &session_config_;

  DISALLOW_COPY_AND_ASSIGN(PunctuationContext);
};

}  // namespace punctuation
}  // namespace pinyin
}  // namespace mozc

#endif  // MOZC_LANGUAGES_PINYIN_PUNCTUATION_CONTEXT_H_
