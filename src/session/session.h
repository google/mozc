// Copyright 2010-2011, Google Inc.
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

#include <string>

#include "base/base.h"
#include "base/coordinates.h"
#include "composer/composer.h"
#include "session/commands.pb.h"
#include "session/session_interface.h"
// Need to include it for "ImeContext::State".
#include "session/internal/ime_context.h"
#include "transliteration/transliteration.h"
// for FRIEND_TEST()
#include "testing/base/public/gunit_prod.h"

namespace mozc {
namespace session {
class SessionCursorManageTest;
class Session : public SessionInterface {
 public:
  Session();
  virtual ~Session();

  virtual bool SendKey(commands::Command *command);

  // Check if the input key event will be consumed by the session.
  virtual bool TestSendKey(commands::Command *command);

  // Perform the SEND_COMMAND command defined commands.proto.
  virtual bool SendCommand(commands::Command *command);

  bool IMEOn(commands::Command *command);
  bool IMEOff(commands::Command *command);
  bool EchoBack(commands::Command *command);
  bool EchoBackAndClearUndoContext(commands::Command *command);
  bool DoNothing(commands::Command *command);
  // Kill itself without any finalization.  It only works on debug builds.
  bool Abort(commands::Command *command);

  // Reset the composer and clear conversion segments.
  // History segments will not be cleared.
  // Therefore if a user commits "風"(かぜ) and Revert method is called,
  // preedit "ひいた"  will be converted into "邪引いた".
  bool Revert(commands::Command *command);
  // Reset the composer and clear all the segments (including history segments).
  // Therefore preedit "ひいた"  will *not* be converted into "邪引いた"
  // on the situation described above.
  bool ResetContext(commands::Command *command);

  // Return the current status such as a composition string, input mode, etc.
  bool GetStatus(commands::Command *command);

  // Fill Output::Callback with the CONVERT_REVERSE SessionCommand to
  // ask the client to send back the SessionCommand to the server.
  // This function is called when the key event representing the
  // ConvertReverse keybinding is called.
  bool RequestConvertReverse(commands::Command *command);

  // Begins reverse conversion for the given session.  This function
  // is called when the CONVERT_REVERSE SessionCommand is called.
  bool ConvertReverse(commands::Command *command);

  // Fill Output::Callback with the Undo SessionCommand to ask the
  // client to send back the SessionCommand to the server.
  // This function is called when the key event representing the
  // Undo keybinding is called.
  bool RequestUndo(commands::Command *command);

  // Undo the commitment.  This function is called when the
  // UNDO SessionCommand is called.
  bool Undo(commands::Command *command);

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
  bool MoveCursorTo(commands::Command *command);
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

  // Expands suggestion candidates.
  bool ExpandSuggestion(commands::Command *command);

  // Commit only the first segment.
  bool CommitSegment(commands::Command *command);
  // Commit some characters at the head of the preedit.
  bool CommitHead(size_t count, commands::Command *command);
  // Commit preedit if in password mode.
  bool CommitIfPassword(commands::Command *command);

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
  bool InputModeSwitchKanaType(commands::Command *command);

  // Specify the input field type.
  bool SwitchInputFieldType(commands::Command *command);

  // Let client launch config dialog
  bool LaunchConfigDialog(commands::Command *command);

  // Let client launch dictionary tool
  bool LaunchDictionaryTool(commands::Command *command);

  // Let client launch word register dialog
  bool LaunchWordRegisterDialog(commands::Command *command);

  // Send a command to the composer to append a special string.
  bool SendComposerCommand(
      const composer::Composer::InternalCommand composer_command,
      commands::Command *command);

  // finalize the session.
  bool Finish(commands::Command *command);

  void UpdatePreviousStatus(commands::Command *command);

  bool ReportBug(commands::Command *command);

  virtual void ReloadConfig();

  // Set client capability for this session.  Used by unittest.
  virtual void set_client_capability(const commands::Capability &capability);

  // Set application information for this session.
  virtual void set_application_info(const commands::ApplicationInfo
                                    &application_info);

  // Get application information
  virtual const commands::ApplicationInfo &application_info() const;

  // Return the time when this instance was created.
  virtual uint64 create_session_time() const;

  // return 0 (default value) if no command is executed in this session.
  virtual uint64 last_command_time() const;

  // TODO(komatsu): delete this funciton.
  // For unittest only
  composer::Composer *get_internal_composer_only_for_unittest();

  const ImeContext &context() const;

  // Update config of |context| referring |config|.
  static void UpdateConfig(const config::Config &config, ImeContext *context);

  // Set OperationPreferences on |context| by using (tentative) |config|.
  static void UpdateOperationPreferences(const config::Config &config,
                                         ImeContext *context);

 private:
  FRIEND_TEST(SessionTest, OutputInitialComposition);
  FRIEND_TEST(SessionTest, IsFullWidthInsertSpace);
  FRIEND_TEST(SessionTest, RequestUndo);

  scoped_ptr<ImeContext> context_;
  scoped_ptr<ImeContext> prev_context_;

  void InitContext(ImeContext *context) const;

  void PushUndoContext();
  void PopUndoContext();
  void ClearUndoContext();

  // Set session state to the given state and also update related status.
  void SetSessionState(ImeContext::State state);

  // Return true if full width space is preferred in the current
  // situation rather than half width space.
  bool IsFullWidthInsertSpace() const;

  bool ConvertToTransliteration(commands::Command *command,
                                transliteration::TransliterationType type);

  // Select a candidate located by input.command.id.  This command
  // would not be used from SendKey but used from SendCommand because
  // it requires the argument id.
  bool SelectCandidate(commands::Command *command);

  // Calls SessionConverter::ConmmitFirstSegment() and deletes characters
  // from the composer.
  void CommitFirstSegmentInternal();

  // Set the focus to the candidate located by input.command.id.  This
  // command would not be used from SendKey but used from SendCommand
  // because it requires the argument id.  The difference from
  // SelectCandidate is that HighlightCandidate does not close the
  // candidate window while SelectCandidate closes the candidate
  // window.
  bool HighlightCandidate(commands::Command *command);

  // The internal implementation of both SelectCandidate and HighlightCandidate.
  bool SelectCandidateInternal(commands::Command *command);

  // If the command is a shortcut to select a candidate from a list,
  // Process it and return true, otherwise return false.
  bool MaybeSelectCandidate(commands::Command *command);

  // Fill command's output according to the current state.
  void OutputFromState(commands::Command *command);
  void Output(commands::Command *command);
  void OutputMode(commands::Command *command) const;
  void OutputComposition(commands::Command *command) const;
  void OutputKey(commands::Command *command) const;
  void OutputWindowLocation(commands::Command *command) const;

  bool SendKeyDirectInputState(commands::Command *command);
  bool SendKeyPrecompositionState(commands::Command *command);
  bool SendKeyCompositionState(commands::Command *command);
  bool SendKeyConversionState(commands::Command *command);

  // update last_command_time;
  void UpdateTime();

  // update preferences only affecting this session.
  void UpdatePreferences(commands::Command *command);

  // Modify input of SendKey, TestSendKey, and SendCommand.
  void TransformInput(commands::Input *input);

  // ensure session status is not DIRECT.
  // if session status is DIRECT, set the status to PRECOMPOSITION.
  void EnsureIMEIsOn();

  // return true if |key_event| is a triggering key_event of
  // AutoIMEConversion.
  bool CanStartAutoConversion(const commands::KeyEvent &key_event) const;

  // Expand composition if required for nested calculation.
  void ExpandCompositionForCalculator(commands::Command *command);

  // Stores received caret location into caret_rectangle_.
  bool SetCaretLocation(commands::Command *command);

  // TODO(nona): Move following rectangle state to ImeContext.
  commands::Rectangle composition_rectangle_;
  commands::Rectangle caret_rectangle_;
  DISALLOW_COPY_AND_ASSIGN(Session);
};

}  // namespace session
}  // namespace mozc
#endif  // MOZC_SESSION_SESSION_H_
