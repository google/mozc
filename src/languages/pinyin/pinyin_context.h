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

#ifndef MOZC_LANGUAGES_PINYIN_PINYIN_CONTEXT_H_
#define MOZC_LANGUAGES_PINYIN_PINYIN_CONTEXT_H_

#include "languages/pinyin/pinyin_context_interface.h"

#include <pyzy-1.0/PyZyInputContext.h>
#include <string>
#include <vector>

#include "base/scoped_ptr.h"

namespace mozc {
namespace pinyin {
struct SessionConfig;

// InputContext::Observer has only pure virtual methods and static methods.
// So we can inherit it.
class ContextObserverInterface : public PyZy::InputContext::Observer {
 public:
  virtual ~ContextObserverInterface() {}
  virtual const string &commit_text() const = 0;
  virtual void SetCommitText(const string &commit_text) = 0;
  virtual void ClearCommitText() = 0;
};

class PinyinContext : public PinyinContextInterface {
 public:
  explicit PinyinContext(const SessionConfig &session_config);
  virtual ~PinyinContext();

  // Wrapper functions of libpyzy.
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
  void ResetContext();

  // Double pinyin mode or not.
  bool double_pinyin_;
  // We should delete context_ before observer_.
  scoped_ptr<ContextObserverInterface> observer_;
  scoped_ptr<PyZy::InputContext> context_;

  DISALLOW_COPY_AND_ASSIGN(PinyinContext);
};

}  // namespace pinyin
}  // namespace mozc

#endif  // MOZC_LANGUAGES_PINYIN_PINYIN_CONTEXT_H_
