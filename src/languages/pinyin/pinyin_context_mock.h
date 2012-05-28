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

#ifndef MOZC_LANGUAGES_PINYIN_PINYIN_CONTEXT_MOCK_H_
#define MOZC_LANGUAGES_PINYIN_PINYIN_CONTEXT_MOCK_H_

#include "languages/pinyin/pinyin_context_interface.h"

#include <string>
#include <vector>

namespace mozc {
namespace pinyin {

// This class converts an alphabet-sequence to full-width and upper case.
// Candidates consist of all prefix of input text orderd by length.
// e.g.) input "abc" -> candidates "ＡＢＣ", "ＡＢ", and "Ａ".

// The differences of actual PinyinContext and the mock is here.
// - Can't convert multi-characters to one character.
// - Assumes that word boundaries exist on (i % 3 == 0).
//   e.g.) input "abcdefghijk" -> boundaries "abc def ghi jk".
//   This boundary is used in MoveCursor*ByWord(), and RemoveWord*(),
//   and isn't used in conversion process.
// - Can't manage "'" on Insert(). It is used to specify boundaries.
// - Content of auxiliary text is "auxiliary_text_" +
//   tolower(to_half_width(candidates_[0])).
// - ClearCandidateFromHistory() removes specified candidate even if it is
//   not a candidate from a history.

class PinyinContextMock : public PinyinContextInterface {
 public:
  PinyinContextMock();
  virtual ~PinyinContextMock();

  // Insert() returns false if ch is not an alphabet.
  virtual bool Insert(char ch);
  virtual void Commit();
  virtual void CommitPreedit();
  virtual void Clear();
  virtual void ClearCommitText();

  virtual bool MoveCursorRight();
  virtual bool MoveCursorLeft();
  virtual bool MoveCursorRightByWord();
  virtual bool MoveCursorLeftByWord();
  virtual bool MoveCursorToBeginning();
  virtual bool MoveCursorToEnd();

  virtual bool SelectCandidate(size_t index);
  virtual bool FocusCandidate(size_t index);
  virtual bool FocusCandidatePrev();
  virtual bool FocusCandidateNext();
  virtual bool ClearCandidateFromHistory(size_t index);

  virtual bool RemoveCharBefore();
  virtual bool RemoveCharAfter();
  virtual bool RemoveWordBefore();
  virtual bool RemoveWordAfter();

  virtual void ReloadConfig();

  virtual const string &commit_text() const;
  virtual const string &input_text() const;
  virtual const string &selected_text() const;
  virtual const string &conversion_text() const;
  virtual const string &rest_text() const;
  virtual const string &auxiliary_text() const;

  virtual size_t cursor() const;
  virtual size_t focused_candidate_index() const;
  virtual size_t candidates_size() const;
  virtual bool HasCandidate(size_t index);
  virtual bool GetCandidate(size_t index, Candidate *candidate);
  virtual size_t PrepareCandidates(size_t index);

 private:
  // Finds a word boundary around cursor_.
  virtual size_t BoundaryNext() const;
  virtual size_t BoundaryPrev() const;

  virtual void Update();
  // Converts an alphabet-sequence to full-width and upper case
  virtual void UpdateCandidates();
  virtual void UpdateConversion();
  virtual bool MoveCursorInternal(size_t pos);

  // This mock expects that
  // - input_text_ contains only ASCII characters.
  // - commit_text_, selected_text_, conversion_text_, rest_text_, and
  //   candidates_ contains ASCII characters or UTF-8 characters.
  string commit_text_;
  string input_text_;
  string selected_text_;
  string conversion_text_;
  string rest_text_;
  string auxiliary_text_;
  size_t cursor_;
  size_t focused_candidate_index_;
  vector<string> candidates_;

  bool double_pinyin_;

  DISALLOW_COPY_AND_ASSIGN(PinyinContextMock);
};

}  // namespace pinyin
}  // namespace mozc

#endif  // MOZC_LANGUAGES_PINYIN_PINYIN_CONTEXT_MOCK_H_
