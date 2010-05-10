// Copyright 2010, Google Inc.
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

// Session class of Mozc server.

#ifndef MOZC_SESSION_SESSION_H_
#define MOZC_SESSION_SESSION_H_

#include <map>
#include <string>
#include <vector>

#include "base/base.h"
#include "transliteration/transliteration.h"
#include "converter/converter_interface.h"
#include "session/commands.pb.h"
#include "session/common.h"
// This should be keymap_interface.h
#include "session/internal/keymap.h"

namespace mozc {
class ConverterInterface;

namespace composer {
class Composer;
class Table;
}  // namespace composer

namespace keymap {
class KeyMapManager;
}  // namespace keymap

namespace session {
class SessionConverterInterface;
}  // namespace session

struct SessionState {
  enum Type {
    NONE = 0,
    DIRECT = 1,
    PRECOMPOSITION = 2,
    COMPOSITION = 4,
    CONVERSION = 8,
  };
};

typedef map<string, commands::KeyEvent> TransformTable;

class Session {
 public:
  Session(const composer::Table *table,
          const ConverterInterface *converter,
          const keymap::KeyMapManager *keymap);
  ~Session();

  bool SendKey(commands::Command *command);

  // Check if the input key event will be consumed by the session.
  bool TestSendKey(commands::Command *command);

  // Perform the SEND_COMMAND command defined commands.proto.
  bool SendCommand(commands::Command *command);

  bool IMEOn(commands::Command *command);
  bool IMEOff(commands::Command *command);
  bool EchoBack(commands::Command *command);
  bool DoNothing(commands::Command *command);
  // Kill itself without any finalization.  It only works on debug builds.
  bool Abort(commands::Command *command);

  bool Revert(commands::Command *command);

  // Return the current status such as a composition string, input mode, etc.
  bool GetStatus(commands::Command *command);

  bool InsertSpace(commands::Command *command);
  bool InsertSpaceToggled(commands::Command *command);
  bool InsertSpaceHalfWidth(commands::Command *command);
  bool InsertSpaceFullWidth(commands::Command *command);
  bool InsertCharacter(commands::Command *command);
  bool Delete(commands::Command *command);
  bool Backspace(commands::Command *command);
  bool EditCancel(commands::Command *command);

  bool MoveCursorRight(commands::Command *command);
  bool MoveCursorLeft(commands::Command *command);
  bool MoveCursorToEnd(commands::Command *command);
  bool MoveCursorToBeginning(commands::Command *command);
  bool Convert(commands::Command *command);
  // Start converion not using user history.  This is used for debugging.
  bool ConvertWithoutHistory(commands::Command *command);
  bool ConvertNext(commands::Command *command);
  bool ConvertPrev(commands::Command *command);
  // Show the next page of candidates.
  bool ConvertNextPage(commands::Command *command);
  // Show the previous page of candidates.
  bool ConvertPrevPage(commands::Command *command);
  bool ConvertCancel(commands::Command *command);
  bool PredictAndConvert(commands::Command *command);
  bool Commit(commands::Command *command);
  bool CommitFirstSuggestion(commands::Command *command);

  // Commit only the first segment.
  bool CommitSegment(commands::Command *command);

  bool SetComposition(const string &composition);

  bool SegmentFocusRight(commands::Command *command);
  bool SegmentFocusLeft(commands::Command *command);
  bool SegmentFocusLast(commands::Command *command);
  bool SegmentFocusLeftEdge(commands::Command *command);
  bool SegmentWidthExpand(commands::Command *command);
  bool SegmentWidthShrink(commands::Command *command);

  // Select the transliteration candidate.  If the current state is
  // composition, candidates will be generated with only translitaration
  // candidates.
  bool ConvertToHiragana(commands::Command *command);
  bool ConvertToFullKatakana(commands::Command *command);
  bool ConvertToHalfKatakana(commands::Command *command);
  bool ConvertToFullASCII(commands::Command *command);
  bool ConvertToHalfASCII(commands::Command *command);
  bool ConvertToHalfWidth(commands::Command *command);
  // Switch the composition to Hiragana, full-width Katakana or
  // half-width Katakana by rotation.
  bool SwitchKanaType(commands::Command *command);

  // Select the transliteration candidate if the current status is
  // conversion.  This is same with the above ConvertTo functions.  If
  // the current state is composition, the display mode is changed to the
  // transliteration and the composition state still remains.
  bool DisplayAsHiragana(commands::Command *command);
  bool DisplayAsFullKatakana(commands::Command *command);
  bool DisplayAsHalfKatakana(commands::Command *command);
  bool TranslateFullASCII(commands::Command *command);
  bool TranslateHalfASCII(commands::Command *command);
  bool TranslateHalfWidth(commands::Command *command);
  bool ToggleAlphanumericMode(commands::Command *command);

  // Switch the input mode.
  bool InputModeHiragana(commands::Command *command);
  bool InputModeFullKatakana(commands::Command *command);
  bool InputModeHalfKatakana(commands::Command *command);
  bool InputModeFullASCII(commands::Command *command);
  bool InputModeHalfASCII(commands::Command *command);

  // finalize the session.
  bool Finish(commands::Command *command);

  void UpdatePreviousStatus(commands::Command *command);

  bool ReportBug(commands::Command *command);

  void ReloadConfig();

  uint64 create_session_time() const {
    return create_session_time_;
  }

  // return static_cast<uint64>(-1) (default value)
  // if no command is executed in this session.
  uint64 last_command_time() const {
    return last_command_time_;
  }

 private:
  // TODO(komatsu): Make SessionInterface abstract class and hide
  // private functions from the upper layers.
  uint64 create_session_time_;
  uint64 last_command_time_;

  scoped_ptr<composer::Composer> composer_;

  scoped_ptr<session::SessionConverterInterface> converter_;

  const keymap::KeyMapManager *keymap_;

  SessionState::Type state_;
  TransformTable transform_table_;

  // Return true if full width space is preferred in the current
  // situation rather than half width space.
  bool IsFullWidthInsertSpace() const;

  bool ConvertToTransliteration(commands::Command *command,
                                transliteration::TransliterationType type);

  // Select a candidate located by input.command.id.  This command
  // would not be used from SendKey but used from SendCommand because
  // it requires the argument id.
  bool SelectCandidate(commands::Command *command);

  // Set the focus to the candidate located by input.command.id.  This
  // command would not be used from SendKey but used from SendCommand
  // because it requires the argument id.  The difference from
  // SelectCandidate is that HighlightCandidate does not close the
  // candidate window while SelectCandidate closes the candidate
  // window.
  bool HighlightCandidate(commands::Command *command);

  // If the command is a shortcut to select a candidate from a list,
  // Process it and return true, otherwise return false.
  bool MaybeSelectCandidate(commands::Command *command);

  void Output(commands::Command *command);
  void OutputMode(commands::Command *command) const;
  void OutputComposition(commands::Command *command) const;
  void OutputKey(commands::Command *command) const;

  bool SendKeyDirectInputState(commands::Command *command);
  bool SendKeyPrecompositionState(commands::Command *command);
  bool SendKeyCompositionState(commands::Command *command);
  bool SendKeyConversionState(commands::Command *command);

  // update last_command_time;
  void UpdateTime();

  // update preferences only affecting this session.
  bool UpdatePreferences(commands::Command *command);

  // ensure session status is not DIRECT.
  // if session status is DIRECT, set the status to PRECOMPOSITION.
  void EnsureIMEIsOn();

  DISALLOW_COPY_AND_ASSIGN(Session);
};

}  // namespace mozc
#endif  // MOZC_SESSION_SESSION_H_
