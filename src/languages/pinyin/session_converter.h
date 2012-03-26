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

#ifndef MOZC_LANGUAGES_PINYIN_SESSION_CONVERTER_H_
#define MOZC_LANGUAGES_PINYIN_SESSION_CONVERTER_H_

#include "languages/pinyin/session_converter_interface.h"

#include <string>
#include "base/scoped_ptr.h"

namespace mozc {
namespace commands {
class Candidates;
class KeyEvent;
class Output;
class Preedit;
class Result;
}  // namespace commands

namespace pinyin {
class PinyinContextInterface;

class SessionConverter : public SessionConverterInterface {
 public:
  // This class takes the ownership of *context.
  explicit SessionConverter(PinyinContextInterface *context);
  virtual ~SessionConverter();

  bool IsConverterActive() const;
  bool Insert(const commands::KeyEvent &key_event);
  void Clear();
  void Commit();
  void CommitPreedit();

  bool SelectCandidateOnPage(size_t index);
  bool SelectFocusedCandidate();

  bool FocusCandidate(size_t index);
  bool FocusCandidateOnPage(size_t index);
  bool FocusCandidateNext();
  bool FocusCandidateNextPage();
  bool FocusCandidatePrev();
  bool FocusCandidatePrevPage();

  bool ClearCandidateFromHistory(size_t index);

  bool MoveCursorRight();
  bool MoveCursorLeft();
  bool MoveCursorRightByWord();
  bool MoveCursorLeftByWord();
  bool MoveCursorToBeginning();
  bool MoveCursorToEnd();

  bool RemoveCharBefore();
  bool RemoveCharAfter();
  bool RemoveWordBefore();
  bool RemoveWordAfter();

  void FillOutput(commands::Output *output) const;
  void PopOutput(commands::Output *output);

  void ReloadConfig();

 private:
  friend class PinyinSessionTest;
  friend class SessionConverterTest;

  bool IsCandidateListVisible() const;
  bool IsConversionTextVisible() const;

  // Fills data. We may need to update data before call these.
  void FillConversion(commands::Preedit *preedit) const;
  void FillResult(commands::Result *result) const;
  void FillCandidates(commands::Candidates *candidates) const;

  // Converts relative index to absolute index.
  // Absolute index is an index from the beginning of candidates, and
  // relative index is an index from the beginning of a candidates page.
  bool GetAbsoluteIndex(size_t relative_index, size_t *absolute_index) const;

  scoped_ptr<PinyinContextInterface> context_;

  DISALLOW_COPY_AND_ASSIGN(SessionConverter);
};

}  // namespace pinyin
}  // namespace mozc

#endif  // MOZC_LANGUAGES_PINYIN_SESSION_CONVERTER_H_
