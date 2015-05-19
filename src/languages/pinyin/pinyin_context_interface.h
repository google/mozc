// Copyright 2010-2013, Google Inc.
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

// Manage PinYin conversion engine.

#ifndef MOZC_LANGUAGES_PINYIN_PINYIN_CONTEXT_INTERFACE_H_
#define MOZC_LANGUAGES_PINYIN_PINYIN_CONTEXT_INTERFACE_H_

#include <string>
#include <vector>

#include "base/base.h"

namespace mozc {
namespace pinyin {

// TODO(hsumita): Appends candidate type to |Candidate|.
// Candidate type is used to indicates how the candidate is generated.
struct Candidate {
  string text;
};

class PinyinContextInterface {
 public:
  virtual ~PinyinContextInterface() {}

  virtual bool Insert(char ch) = 0;
  // Sets |selected_text| + |unselected_text| to |commit_text| and clears
  // other context.
  // |unselected_text| is pinyin of |conversion text| + |rest_text|.
  virtual void Commit() = 0;
  // Sets |input_text| to |commit_text| and clears other context.
  virtual void CommitPreedit() = 0;
  virtual void Clear() = 0;
  // Clears only commit text.
  virtual void ClearCommitText() = 0;

  virtual bool MoveCursorRight() = 0;
  virtual bool MoveCursorLeft() = 0;
  virtual bool MoveCursorRightByWord() = 0;
  virtual bool MoveCursorLeftByWord() = 0;
  virtual bool MoveCursorToBeginning() = 0;
  virtual bool MoveCursorToEnd() = 0;

  virtual bool SelectCandidate(size_t index) = 0;
  virtual bool FocusCandidate(size_t index) = 0;
  // Clears specified conversion history.
  // Candidate which is introduced by conversion history is also cleared.
  virtual bool ClearCandidateFromHistory(size_t index) = 0;

  virtual bool RemoveCharBefore() = 0;
  virtual bool RemoveCharAfter() = 0;
  virtual bool RemoveWordBefore() = 0;
  virtual bool RemoveWordAfter() = 0;

  // Reloads config of backend with config::PinyinConfig.
  // If config::PinyinConfig::double_pinyin is changed, any informations of
  // the context are cleared.
  virtual void ReloadConfig() = 0;

  // Accessors
  // Commit text.
  virtual const string &commit_text() const = 0;
  // Raw input text.  It is not modified without calling Insert(), Remove*(),
  // Commit*(), or Clear().
  virtual const string &input_text() const = 0;
  // Already selected text using candidate window.
  virtual const string &selected_text() const = 0;
  // Text which is being converted.
  virtual const string &conversion_text() const = 0;
  // Unsegmented and unconverted text.
  virtual const string &rest_text() const = 0;
  // Auxiliary text is shown on candidates window to support user operations.
  virtual const string &auxiliary_text() const = 0;

  virtual size_t cursor() const = 0;
  virtual size_t focused_candidate_index() const = 0;
  // TODO(hsumita): Appends const qualifier to Get/HasCandidate methods.
  virtual bool GetCandidate(size_t index, Candidate *candidate) = 0;
  virtual bool HasCandidate(size_t index) = 0;
  // Takes a required candidates size, and returns a prepared candidates size.
  virtual size_t PrepareCandidates(size_t required_size) = 0;
};

}  // namespace pinyin
}  // namespace mozc

#endif  // MOZC_LANGUAGES_PINYIN_PINYIN_CONTEXT_INTERFACE_H_
