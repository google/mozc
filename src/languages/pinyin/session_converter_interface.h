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

#ifndef MOZC_LANGUAGES_PINYIN_SESSION_CONVERTER_INTERFACE_H_
#define MOZC_LANGUAGES_PINYIN_SESSION_CONVERTER_INTERFACE_H_

#include "base/base.h"
#include "languages/pinyin/pinyin_constant.h"

namespace mozc {
namespace commands {
class KeyEvent;
class Output;
class Result;
}  // namespace commands

namespace pinyin {
class PinyinContextInterface;

class SessionConverterInterface {
 public:
  SessionConverterInterface() {}
  virtual ~SessionConverterInterface() {}

  // Indicates if the conversion session is active or not.  In general,
  // Insert function makes it active, Reset, Commit and CommitPreedit
  // functions make it deactive, and SelectCandidate may make it diactive.
  virtual bool IsConverterActive() const = 0;

  virtual bool Insert(const commands::KeyEvent &key_event) = 0;

  virtual void Clear() = 0;

  // Fixes the conversion with the current status.
  // If there are unselected text (conversion_text and rest_text),
  // This function commits it as preedit text.
  virtual void Commit() = 0;

  // Commits the preedit string.
  virtual void CommitPreedit() = 0;

  // Selects the candidate.
  // If all candidates are selected, this function calls Commit().
  virtual bool SelectCandidateOnPage(size_t index) = 0;
  virtual bool SelectFocusedCandidate() = 0;

  virtual bool FocusCandidate(size_t index) = 0;
  virtual bool FocusCandidateOnPage(size_t index) = 0;
  virtual bool FocusCandidateNext() = 0;
  virtual bool FocusCandidateNextPage() = 0;
  virtual bool FocusCandidatePrev() = 0;
  virtual bool FocusCandidatePrevPage() = 0;

  virtual bool ClearCandidateFromHistory(size_t index) = 0;

  virtual bool RemoveCharBefore() = 0;
  virtual bool RemoveCharAfter() = 0;
  virtual bool RemoveWordBefore() = 0;
  virtual bool RemoveWordAfter() = 0;

  virtual bool MoveCursorRight() = 0;
  virtual bool MoveCursorLeft() = 0;
  virtual bool MoveCursorRightByWord() = 0;
  virtual bool MoveCursorLeftByWord() = 0;
  virtual bool MoveCursorToBeginning() = 0;
  virtual bool MoveCursorToEnd() = 0;

  // Fills protocol buffers
  // It doesn't have const qualifier because PinyinContextInterface may
  // generate candidates lazily.
  virtual void FillOutput(commands::Output *output) = 0;
  // Fills protocol buffers and updates internal status for a next operation.
  virtual void PopOutput(commands::Output *output) = 0;

  virtual void ReloadConfig() = 0;
  // Switches the context.
  virtual void SwitchContext(ConversionMode mode) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionConverterInterface);
};

}  // namespace pinyin
}  // namespace mozc
#endif  // MOZC_LANGUAGES_PINYIN_SESSION_CONVERTER_INTERFACE_H_
