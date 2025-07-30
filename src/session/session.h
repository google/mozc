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

// Session class of Mozc server.

#ifndef MOZC_SESSION_SESSION_H_
#define MOZC_SESSION_SESSION_H_

#include <cstddef>
#include <deque>
#include <memory>
#include <utility>

#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "composer/composer.h"
#include "composer/table.h"
#include "engine/engine_interface.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "session/ime_context.h"
#include "session/keymap.h"
#include "transliteration/transliteration.h"

namespace mozc {
namespace session {

class Session {
 public:
  explicit Session(const EngineInterface &engine);
  Session(const Session &) = delete;
  Session &operator=(const Session &) = delete;

  bool SendKey(mozc::commands::Command *command);

  // Check if the input key event will be consumed by the session.
  bool TestSendKey(mozc::commands::Command *command);

  // Perform the SEND_COMMAND command defined commands.proto.
  bool SendCommand(mozc::commands::Command *command);

  // Turn on IME. Do nothing (but the keyevent is consumed) when IME is already
  // turned on.
  bool IMEOn(mozc::commands::Command *command);

  // Turn off IME. Do nothing (but the keyevent is consumed) when IME is already
  // turned off.
  bool IMEOff(mozc::commands::Command *command);

  // Unlike IMEOn/IMEOff, these commands 1) can update composition mode, and
  // 2) are functional even when IME is already turned on/off.
  // TODO(team): Merge these into IMEOn/Off once b/10250883 is fixed.
  bool MakeSureIMEOn(mozc::commands::Command *command);
  bool MakeSureIMEOff(mozc::commands::Command *command);

  bool EchoBack(mozc::commands::Command *command);
  bool EchoBackAndClearUndoContext(mozc::commands::Command *command);
  bool DoNothing(mozc::commands::Command *command);

  // Tries deleting the specified candidate from the user prediction history.
  // The candidate is determined by command.input.command.id, or the current
  // focused candidate if that ....command.id is not specified. If
  // that candidate, as a key value pair, doesn't exist in the user history,
  // nothing happens. Regardless of the result of internal history deletion,
  // invoking this method has the same effect as ConvertCancel() from the
  // viewpoint of session, meaning that the session state gets back to
  // composition.
  bool DeleteCandidateFromHistory(mozc::commands::Command *command);

  // Resets the composer and clear conversion segments.
  // History segments will not be cleared.
  // Therefore if a user commits "風"(かぜ) and Revert method is called,
  // preedit "ひいた"  will be converted into "邪引いた".
  bool Revert(mozc::commands::Command *command);
  // Reset the composer and clear all the segments (including history segments).
  // Therefore preedit "ひいた"  will *not* be converted into "邪引いた"
  // on the situation described above.
  bool ResetContext(mozc::commands::Command *command);

  // Returns the current status such as a composition string, input mode, etc.
  bool GetStatus(mozc::commands::Command *command);

  // Fills Output::Callback with the CONVERT_REVERSE SessionCommand to
  // ask the client to send back the SessionCommand to the server.
  // This function is called when the key event representing the
  // ConvertReverse keybinding is called.
  bool RequestConvertReverse(mozc::commands::Command *command);

  // Begins reverse conversion for the given session.  This function
  // is called when the CONVERT_REVERSE SessionCommand is called.
  bool ConvertReverse(mozc::commands::Command *command);

  // Fills Output::Callback with the Undo SessionCommand to ask the
  // client to send back the SessionCommand to the server.
  // This function is called when the key event representing the
  // Undo keybinding is called.
  bool RequestUndo(mozc::commands::Command *command);

  // Undos the commitment.  This function is called when the
  // UNDO SessionCommand is called.
  bool Undo(mozc::commands::Command *command);

  bool InsertSpace(mozc::commands::Command *command);
  bool InsertSpaceToggled(mozc::commands::Command *command);
  bool InsertSpaceHalfWidth(mozc::commands::Command *command);
  bool InsertSpaceFullWidth(mozc::commands::Command *command);
  bool InsertCharacter(mozc::commands::Command *command);
  bool UpdateComposition(mozc::commands::Command *command);
  bool UpdateCompositionInternal(mozc::commands::Command *command);
  bool Delete(mozc::commands::Command *command);
  bool Backspace(mozc::commands::Command *command);
  bool EditCancel(mozc::commands::Command *command);
  bool EditCancelAndIMEOff(mozc::commands::Command *command);

  bool MoveCursorRight(mozc::commands::Command *command);
  bool MoveCursorLeft(mozc::commands::Command *command);
  bool MoveCursorToEnd(mozc::commands::Command *command);
  bool MoveCursorToBeginning(mozc::commands::Command *command);
  bool MoveCursorTo(mozc::commands::Command *command);
  bool Convert(mozc::commands::Command *command);
  // Starts conversion not using user history.  This is used for debugging.
  bool ConvertWithoutHistory(mozc::commands::Command *command);
  bool ConvertNext(mozc::commands::Command *command);
  bool ConvertPrev(mozc::commands::Command *command);
  // Shows the next page of candidates.
  bool ConvertNextPage(mozc::commands::Command *command);
  // Shows the previous page of candidates.
  bool ConvertPrevPage(mozc::commands::Command *command);
  bool ConvertCancel(mozc::commands::Command *command);
  bool PredictAndConvert(mozc::commands::Command *command);
  // Note: Commit() also triggers zero query suggestion.
  // TODO(team): Rename this method to CommitWithZeroQuerySuggest.
  bool Commit(mozc::commands::Command *command);
  bool CommitNotTriggeringZeroQuerySuggest(commands::Command *command);
  bool CommitFirstSuggestion(mozc::commands::Command *command);
  // Select a candidate located by input.command.id and commit.
  bool CommitCandidate(mozc::commands::Command *command);

  // Commits only the first segment.
  bool CommitSegment(mozc::commands::Command *command);
  // Commits some characters at the head of the preedit.
  bool CommitHead(size_t count, mozc::commands::Command *command);
  // Commits preedit if in password mode.
  bool CommitIfPassword(mozc::commands::Command *command);

  bool SegmentFocusRight(mozc::commands::Command *command);
  bool SegmentFocusLeft(mozc::commands::Command *command);
  bool SegmentFocusLast(mozc::commands::Command *command);
  bool SegmentFocusLeftEdge(mozc::commands::Command *command);
  bool SegmentWidthExpand(mozc::commands::Command *command);
  bool SegmentWidthShrink(mozc::commands::Command *command);

  // Selects the transliteration candidate.  If the current state is
  // composition, candidates will be generated with only translitaration
  // candidates.
  bool ConvertToHiragana(mozc::commands::Command *command);
  bool ConvertToFullKatakana(mozc::commands::Command *command);
  bool ConvertToHalfKatakana(mozc::commands::Command *command);
  bool ConvertToFullASCII(mozc::commands::Command *command);
  bool ConvertToHalfASCII(mozc::commands::Command *command);
  bool ConvertToHalfWidth(mozc::commands::Command *command);
  // Switch the composition to Hiragana, full-width Katakana or
  // half-width Katakana by rotation.
  bool SwitchKanaType(mozc::commands::Command *command);

  // Select the transliteration candidate if the current status is
  // conversion.  This is same with the above ConvertTo functions.  If
  // the current state is composition, the display mode is changed to the
  // transliteration and the composition state still remains.
  bool DisplayAsHiragana(mozc::commands::Command *command);
  bool DisplayAsFullKatakana(mozc::commands::Command *command);
  bool DisplayAsHalfKatakana(mozc::commands::Command *command);
  bool TranslateFullASCII(mozc::commands::Command *command);
  bool TranslateHalfASCII(mozc::commands::Command *command);
  bool TranslateHalfWidth(mozc::commands::Command *command);
  bool ToggleAlphanumericMode(mozc::commands::Command *command);

  // Switch the input mode.
  bool InputModeHiragana(mozc::commands::Command *command);
  bool InputModeFullKatakana(mozc::commands::Command *command);
  bool InputModeHalfKatakana(mozc::commands::Command *command);
  bool InputModeFullASCII(mozc::commands::Command *command);
  bool InputModeHalfASCII(mozc::commands::Command *command);
  bool InputModeSwitchKanaType(mozc::commands::Command *command);

  // Specify the input field type.
  bool SwitchInputFieldType(mozc::commands::Command *command);

  // Let client launch config dialog
  bool LaunchConfigDialog(mozc::commands::Command *command);

  // Let client launch dictionary tool
  bool LaunchDictionaryTool(mozc::commands::Command *command);

  // Let client launch word register dialog
  bool LaunchWordRegisterDialog(mozc::commands::Command *command);

  // Undo if pre-composition is empty. Rewind KANA cycle otherwise.
  bool UndoOrRewind(mozc::commands::Command *command);

  // Stops key toggling in the composer.
  bool StopKeyToggling(mozc::commands::Command *command);

  bool ReportBug(mozc::commands::Command *command);

  void SetConfig(std::shared_ptr<const mozc::config::Config> config);

  void SetKeyMapManager(
      std::shared_ptr<const mozc::keymap::KeyMapManager> key_map_manager);

  void SetRequest(std::shared_ptr<const mozc::commands::Request> request);

  void SetConfig(mozc::config::Config config) {
    SetConfig(std::make_shared<const config::Config>(std::move(config)));
  }

  void SetRequest(mozc::commands::Request request) {
    SetRequest(
        std::make_shared<const mozc::commands::Request>(std::move(request)));
  }

  void SetTable(std::shared_ptr<const mozc::composer::Table> table);

  // Set client capability for this session.  Used by unittest.
  void set_client_capability(mozc::commands::Capability capability);

  // Set application information for this session.
  void set_application_info(mozc::commands::ApplicationInfo application_info);

  // Get application information
  const mozc::commands::ApplicationInfo &application_info() const;

  // Return the time when this instance was created.
  absl::Time create_session_time() const;

  // return 0 (default value) if no command is executed in this session.
  absl::Time last_command_time() const;

  // TODO(komatsu): delete this function.
  // For unittest only
  mozc::composer::Composer *get_internal_composer_only_for_unittest();

  const ImeContext &context() const;

 private:
  friend class SessionTestPeer;

  std::unique_ptr<ImeContext> context_;

  // Undo stack. *begin is the oldest, and *back is the newest.
  std::deque<std::unique_ptr<ImeContext>> undo_contexts_;

  std::unique_ptr<ImeContext> CreateContext(
      const EngineInterface &engine) const;

  void PushUndoContext();
  void PopUndoContext();
  // Clear the undo context.
  // This should be called when the composer's preedit or cursor position
  // is updated by non-undo related operations. This achieves intuitive
  // behavior, which clears the undo context on the user's edit operation.
  void ClearUndoContext();
  bool HasUndoContext() const;
  // Set command.output.status.undo_available to true if HasUndoContext()==true.
  // Otherwise no-op.
  // Some code treats empty Status and default Status differently so
  // we don't want to create a new Status instance if not required.
  void MaybeSetUndoStatus(commands::Command *command) const;

  // Return true if full width space is preferred in the given new input
  // state than half width space. When |input| does not have new input mode,
  // the current mode will be considered.
  bool IsFullWidthInsertSpace(const mozc::commands::Input &input) const;

  bool EditCancelOnPasswordField(mozc::commands::Command *command);

  bool ConvertToTransliteration(
      mozc::commands::Command *command,
      mozc::transliteration::TransliterationType type);

  // Select a candidate located by input.command.id.  This command
  // would not be used from SendKey but used from SendCommand because
  // it requires the argument id.
  bool SelectCandidate(mozc::commands::Command *command);

  // Calls EngineConverter::ConmmitHeadToFocusedSegments()
  // and deletes characters from the composer.
  void CommitHeadToFocusedSegmentsInternal(const commands::Context &context);

  // Commits without EngineConverter.
  void CommitCompositionDirectly(commands::Command *command);
  void CommitSourceTextDirectly(commands::Command *command);
  void CommitRawTextDirectly(commands::Command *command);
  void CommitStringDirectly(absl::string_view key, absl::string_view preedit,
                            commands::Command *command);
  bool CommitInternal(commands::Command *command,
                      bool trigger_zero_query_suggest);

  // Calls EngineConverter::Suggest if the condition is applicable to
  // call it.  True is returned when EngineConverter::Suggest is
  // called and results exist.  False is returned when
  // EngineConverter::Suggest is not called or no results exist.
  bool Suggest(const mozc::commands::Input &input);

  // Commands like EditCancel should restore the original string used for
  // the reverse conversion without any modification.
  // Returns true if the |source_text| is committed to cancel reconversion.
  // Returns false if this function has nothing to do.
  bool TryCancelConvertReverse(mozc::commands::Command *command);

  // Set the focus to the candidate located by input.command.id.  This
  // command would not be used from SendKey but used from SendCommand
  // because it requires the argument id.  The difference from
  // SelectCandidate is that HighlightCandidate does not close the
  // candidate window while SelectCandidate closes the candidate
  // window.
  bool HighlightCandidate(mozc::commands::Command *command);

  // The internal implementation of both SelectCandidate and HighlightCandidate.
  bool SelectCandidateInternal(mozc::commands::Command *command);

  // If the command is a shortcut to select a candidate from a list,
  // Process it and return true, otherwise return false.
  bool MaybeSelectCandidate(mozc::commands::Command *command);

  // Fill command's output according to the current state.
  void OutputFromState(mozc::commands::Command *command);
  void Output(mozc::commands::Command *command);
  void OutputMode(mozc::commands::Command *command) const;
  void OutputComposition(mozc::commands::Command *command) const;
  void OutputKey(mozc::commands::Command *command) const;

  bool SendKeyDirectInputState(mozc::commands::Command *command);
  bool SendKeyPrecompositionState(mozc::commands::Command *command);
  bool SendKeyCompositionState(mozc::commands::Command *command);
  bool SendKeyConversionState(mozc::commands::Command *command);

  bool MoveCursorToEndInternal(mozc::commands::Command *command,
                               bool clear_undo);

  // update last_command_time;
  void UpdateTime();

  // update preferences only affecting this session.
  void UpdatePreferences(mozc::commands::Command *command);

  // Modify input of SendKey, TestSendKey, and SendCommand.
  void TransformInput(mozc::commands::Input *input);

  // ensure session status is not DIRECT.
  // if session status is DIRECT, set the status to PRECOMPOSITION.
  void EnsureIMEIsOn();

  // return true if |key_event| is a triggering key_event of
  // AutoIMEConversion.
  bool CanStartAutoConversion(const mozc::commands::KeyEvent &key_event) const;

  // Handles KeyEvent::activated to support indirect IME on/off.
  bool HandleIndirectImeOnOff(mozc::commands::Command *command);

  // Commits the raw text of the composition.
  bool CommitRawText(commands::Command *command);
};

}  // namespace session
}  // namespace mozc

#endif  // MOZC_SESSION_SESSION_H_
