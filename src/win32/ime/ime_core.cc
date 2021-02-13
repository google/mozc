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

#include "win32/ime/ime_core.h"

// clang-format off
#include <windows.h>
#include <ime.h>
// clang-format on

#include <memory>

#include "base/logging.h"
#include "base/util.h"
#include "client/client_interface.h"
#include "config/config_handler.h"
#include "protocol/commands.pb.h"
#include "session/output_util.h"
#include "win32/base/conversion_mode_util.h"
#include "win32/base/deleter.h"
#include "win32/base/imm_reconvert_string.h"
#include "win32/base/input_state.h"
#include "win32/base/keyboard.h"
#include "win32/ime/ime_candidate_info.h"
#include "win32/ime/ime_composition_string.h"
#include "win32/ime/ime_private_context.h"
#include "win32/ime/ime_scoped_context.h"
#include "win32/ime/ime_ui_context.h"
#include "win32/ime/ime_ui_visibility_tracker.h"

namespace mozc {
namespace win32 {

using commands::KeyEvent;
using commands::Output;
using commands::SessionCommand;
using std::unique_ptr;

namespace {

const size_t kReconvertStringSizeLimit = 1024 * 64;
// An embedded object in the RichEdit will be replaced with this character.
// See b/3406434 for details.
const wchar_t kObjectReplacementCharacter = L'\uFFFC';

bool GetNextState(HIMC himc, const commands::Output &output,
                  mozc::win32::InputState *next_state) {
  DCHECK(next_state);

  UIContext context(himc);
  bool next_open_status = false;
  DWORD next_logical_mode = 0;
  DWORD next_visible_mode = 0;
  if (!output.has_status()) {
    // |output| does not have |status|.  Preserve the current status.
    next_open_status = context.GetOpenStatus();
    if (!context.GetConversionMode(&next_logical_mode)) {
      return false;
    }
    next_visible_mode = next_logical_mode;
  } else if (!mozc::win32::ConversionModeUtil::ConvertStatusFromMozcToNative(
                 output.status(), context.IsKanaInputPreferred(),
                 &next_open_status, &next_logical_mode, &next_visible_mode)) {
    return false;
  }

  next_state->open = next_open_status;
  next_state->logical_conversion_mode = next_logical_mode;
  next_state->visible_conversion_mode = next_visible_mode;
  return true;
}

bool UpdateInputContext(HIMC himc, const commands::Output &output,
                        bool generate_message) {
  InputState next_state;
  if (!GetNextState(himc, output, &next_state)) {
    return false;
  }
  if (!generate_message) {
    if (!ImeCore::UpdateContext(himc, next_state, output, nullptr)) {
      return false;
    }
    return true;
  }
  MessageQueue message_queue(himc);
  if (!ImeCore::UpdateContext(himc, next_state, output, &message_queue)) {
    return FALSE;
  }
  return message_queue.Send();
}

HIMCC EnsureHIMCCSize(HIMCC himcc, DWORD size) {
  if (himcc == nullptr) {
    return ::ImmCreateIMCC(size);
  }
  const DWORD current_size = ::ImmGetIMCCSize(himcc);
  if (current_size == size) {
    return himcc;
  }
  return ::ImmReSizeIMCC(himcc, size);
}

bool UpdateCompositionString(HIMC himc, const commands::Output &output,
                             std::vector<UIMessage> *messages) {
  ScopedHIMC<InputContext> context(himc);

  // When the string is inserted from Tablet Input Panel, MSCTF shrinks the
  // CompositionString buffer that we allocated in ImeSelect(). Thus we need
  // to resize the buffer when necessary. b/6841008
  // TODO(yukawa): Move this logic into more appropriate place.
  const HIMCC composition_string_handle =
      EnsureHIMCCSize(context->hCompStr, sizeof(CompositionString));
  if (composition_string_handle == nullptr) {
    return false;
  }
  ScopedHIMCC<CompositionString> compstr(composition_string_handle);
  if (!compstr->Update(output, messages)) {
    return false;
  }
  return true;
}

bool UpdateCompositionStringAndPushMessages(HIMC himc,
                                            const commands::Output &output,
                                            MessageQueue *message_queue) {
  ScopedHIMC<InputContext> context(himc);
  ScopedHIMCC<PrivateContext> private_context(context->hPrivate);
  std::vector<UIMessage> messages;

  if (!UpdateCompositionString(himc, output, &messages)) {
    return false;
  }

  const bool generate_message = (message_queue != nullptr);
  if (!generate_message) {
    return true;
  }

  for (std::vector<UIMessage>::const_iterator it = messages.begin();
       it != messages.end(); ++it) {
    if (UIVisibilityTracker::IsVisibilityTestMessageForComposiwionWindow(
            it->message(), it->wparam(), it->lparam())) {
      private_context->ui_visibility_tracker
          ->BeginVisibilityTestForCompositionWindow();
    }
    message_queue->AddMessage(it->message(), it->wparam(), it->lparam());
  }
  return true;
}

bool GetReconvertString(const RECONVERTSTRING *reconvert_string,
                        string *total_composition_in_utf8) {
  DCHECK(total_composition_in_utf8);
  total_composition_in_utf8->clear();

  if (reconvert_string->dwCompStrLen == 0) {
    // There is no text selection. Reconversion cannot be started.
    return false;
  }

  std::wstring preceding_composition;
  std::wstring target_text;
  std::wstring following_composition;
  if (!ReconvertString::Decompose(reconvert_string, nullptr,
                                  &preceding_composition, &target_text,
                                  &following_composition, nullptr)) {
    DLOG(INFO) << "ReconvertString::Decompose failed.";
    return false;
  }

  const std::wstring total_composition =
      preceding_composition + target_text + following_composition;

  // Like other Japanese IMEs (MS-IME, ATOK), Mozc does not support
  // reconversion when the composition string contains any embedded object
  // because it is too complicated to restore the original state when the
  // reconversion is canceled. See b/3406434 for details.
  if (total_composition.find(kObjectReplacementCharacter) !=
      std::wstring::npos) {
    return false;
  }

  if ((Util::WideToUTF8(total_composition, total_composition_in_utf8) == 0) ||
      total_composition_in_utf8->empty()) {
    DLOG(INFO) << "Composition string is empty.";
    return false;
  }

  return true;
}

bool QueryDocumentFeed(HIMC himc, std::wstring *preceding_text,
                       std::wstring *following_text) {
  LRESULT result = ::ImmRequestMessageW(himc, IMR_DOCUMENTFEED, 0);
  if (result == 0) {
    // IMR_DOCUMENTFEED is not supported.
    return false;
  }
  const size_t buffer_size = static_cast<size_t>(result);
  if (buffer_size > kReconvertStringSizeLimit) {
    LOG(ERROR) << "Too large RECONVERTSTRING.";
    return false;
  }

  unique_ptr<BYTE[]> buffer(new BYTE[buffer_size]);

  RECONVERTSTRING *reconvert_string =
      reinterpret_cast<RECONVERTSTRING *>(buffer.get());
  reconvert_string->dwSize = buffer_size;
  reconvert_string->dwVersion = 0;

  result = ::ImmRequestMessageW(himc, IMR_DOCUMENTFEED,
                                reinterpret_cast<LPARAM>(reconvert_string));
  if (result == 0) {
    DLOG(ERROR) << "RECONVERTSTRING is nullptr.";
    return false;
  }

  return ReconvertString::Decompose(reconvert_string, preceding_text, nullptr,
                                    nullptr, nullptr, following_text);
}

}  // namespace

void ImeCore::UpdateContextWithSurroundingText(HIMC himc,
                                               commands::Context *context) {
  if (context == nullptr) {
    return;
  }
  context->clear_preceding_text();
  context->clear_following_text();
  std::wstring preceding_text;
  std::wstring following_text;
  if (!QueryDocumentFeed(himc, &preceding_text, &following_text)) {
    return;
  }
  Util::WideToUTF8(preceding_text, context->mutable_preceding_text());
  Util::WideToUTF8(following_text, context->mutable_following_text());
}

KeyEventHandlerResult ImeCore::ImeProcessKey(
    mozc::client::ClientInterface *client, const VirtualKey &virtual_key,
    const LParamKeyInfo &lparam, const KeyboardStatus &keyboard_status,
    const InputBehavior &behavior, const InputState &initial_state,
    const mozc::commands::Context &context, InputState *next_state,
    commands::Output *output) {
  unique_ptr<Win32KeyboardInterface> keyboard(
      Win32KeyboardInterface::CreateDefault());
  return KeyEventHandler::ImeProcessKey(
      virtual_key, lparam.GetScanCode(), lparam.IsKeyDownInImeProcessKey(),
      keyboard_status, behavior, initial_state, context, client, keyboard.get(),
      next_state, output);
}

KeyEventHandlerResult ImeCore::ImeToAsciiEx(
    mozc::client::ClientInterface *client, const VirtualKey &virtual_key,
    BYTE scan_code, bool is_key_down, const KeyboardStatus &keyboard_status,
    const InputBehavior &behavior, const InputState &initial_state,
    const commands::Context &context, InputState *next_state,
    commands::Output *output) {
  unique_ptr<Win32KeyboardInterface> keyboard(
      Win32KeyboardInterface::CreateDefault());
  return KeyEventHandler::ImeToAsciiEx(
      virtual_key, scan_code, is_key_down, keyboard_status, behavior,
      initial_state, context, client, keyboard.get(), next_state, output);
}

bool ImeCore::OpenIME(mozc::client::ClientInterface *client, DWORD next_mode) {
  commands::CompositionMode mode = commands::DIRECT;
  if (!ConversionModeUtil::GetMozcModeFromNativeMode(next_mode, &mode)) {
    return false;
  }

  SessionCommand command;
  command.set_type(commands::SessionCommand::TURN_ON_IME);
  command.set_composition_mode(mode);
  commands::Output output;
  if (!client->SendCommand(command, &output)) {
    return false;
  }
  if (!output.consumed()) {
    return false;
  }
  return true;
}

bool ImeCore::CloseIME(mozc::client::ClientInterface *client, DWORD next_mode,
                       commands::Output *output) {
  commands::CompositionMode mode = commands::DIRECT;
  if (!ConversionModeUtil::GetMozcModeFromNativeMode(next_mode, &mode)) {
    return false;
  }

  SessionCommand command;
  command.set_type(commands::SessionCommand::TURN_OFF_IME);
  command.set_composition_mode(mode);

  if (!client->SendCommand(command, output)) {
    return false;
  }
  return true;
}

bool ImeCore::SubmitComposition(HIMC himc, bool generate_message) {
  UIContext context(himc);
  mozc::commands::Output output;
  mozc::commands::SessionCommand command;
  command.set_type(mozc::commands::SessionCommand::SUBMIT);
  if (!context.client()->SendCommand(command, &output)) {
    return false;
  }
  return UpdateInputContext(himc, output, generate_message);
}

bool ImeCore::CancelComposition(HIMC himc, bool generate_message) {
  UIContext context(himc);
  mozc::commands::Output output;
  mozc::commands::SessionCommand command;
  command.set_type(mozc::commands::SessionCommand::REVERT);
  if (!context.client()->SendCommand(command, &output)) {
    return false;
  }
  return UpdateInputContext(himc, output, generate_message);
}

bool ImeCore::SwitchInputMode(HIMC himc, DWORD native_mode,
                              bool generate_message) {
  UIContext context(himc);

  const bool open = context.GetOpenStatus();
  if (!open) {
    return true;
  }
  mozc::commands::Output output;
  mozc::commands::SessionCommand command;
  command.set_type(mozc::commands::SessionCommand::SWITCH_INPUT_MODE);

  commands::CompositionMode mozc_mode = commands::HIRAGANA;
  if (!ConversionModeUtil::ToMozcMode(native_mode, &mozc_mode)) {
    return false;
  }

  command.set_composition_mode(mozc_mode);
  if (!context.client()->SendCommand(command, &output)) {
    return false;
  }
  return UpdateInputContext(himc, output, generate_message);
}

DWORD ImeCore::GetSupportableConversionMode(DWORD raw_conversion_mode) {
  // If the initial |fdwConversion| is not a supported combination of flags,
  // we have to update it and then send the IMN_SETCONVERSIONMODE message.
  // See b/2914115 for details.
  const DWORD kHiragana = IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE;
  const DWORD kFullKatakana =
      IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_KATAKANA;
  const DWORD kHalfKatakana = IME_CMODE_NATIVE | IME_CMODE_KATAKANA;
  const DWORD kFullAlpha = IME_CMODE_ALPHANUMERIC | IME_CMODE_FULLSHAPE;
  const DWORD kHalfAlpha = IME_CMODE_ALPHANUMERIC;

  // Separate Roman flag
  DWORD roman_flag = (raw_conversion_mode & IME_CMODE_ROMAN);
  const DWORD original_mode = (raw_conversion_mode & ~IME_CMODE_ROMAN);

  DWORD next_mode = 0;
  switch (original_mode) {
    case kHiragana:
    case kFullKatakana:
    case kHalfKatakana:
    case kFullAlpha:
    case kHalfAlpha:
      // OK, this is one of Well-known modes.
      next_mode = original_mode;
      break;
    default:
      // Unknown combination.
      // TODO(yukawa): use most similar mode instead of always choosing
      //   Roman-Hiragana
      next_mode = kHiragana;
      roman_flag = IME_CMODE_ROMAN;
      break;
  }

  // Restore Roman Flag
  next_mode |= roman_flag;

  return next_mode;
}

DWORD ImeCore::GetSupportableSentenceMode(DWORD raw_sentence_mode) {
  // If the initial |fdwSentence| is not a supported combination of flags,
  // we have to update it and then send the IMN_SETSENTENCEMODE message as we
  // did in b/2914115 for conversion mode.

  // Always returns IME_SMODE_PHRASEPREDICT.
  // See b/2913510, b/2954777, and b/2955175 for details.
  return IME_SMODE_PHRASEPREDICT;
}

bool ImeCore::IsInputContextInitialized(HIMC himc) {
  if (himc == nullptr) {
    return false;
  }
  ScopedHIMC<InputContext> context(himc);
  // For some reason, we fail to lock input context.  See b/3088049 for
  // details.
  if (context.get() == nullptr) {
    return false;
  }
  if (!PrivateContextUtil::IsValidPrivateContext(context->hPrivate)) {
    return false;
  }
  ScopedHIMCC<PrivateContext> private_context(context->hPrivate);
  if (private_context->ime_behavior->disabled) {
    return false;
  }
  return true;
}

void ImeCore::SortIMEMessages(
    const std::vector<UIMessage> &composition_messages,
    const std::vector<UIMessage> &candidate_messages, bool previous_open_status,
    DWORD previous_conversion_mode, bool next_open_status,
    DWORD next_conversion_mode, std::vector<UIMessage> *sorted_messages) {
  DCHECK(sorted_messages);
  sorted_messages->clear();

  const bool open_status_changed = (previous_open_status != next_open_status);
  const bool conversion_mode_changed =
      (previous_conversion_mode != next_conversion_mode);

  // Notify IMN_SETOPENSTATUS for IME-ON.
  if (open_status_changed && next_open_status) {
    sorted_messages->push_back(UIMessage(WM_IME_NOTIFY, IMN_SETOPENSTATUS, 0));
  }

  // Notify IMN_SETCONVERSIONMODE.
  if (conversion_mode_changed) {
    sorted_messages->push_back(
        UIMessage(WM_IME_NOTIFY, IMN_SETCONVERSIONMODE, 0));
  }

  // Notify IMN_CLOSECANDIDATE.
  std::vector<UIMessage> other_candidate_messages;
  for (std::vector<UIMessage>::const_iterator it = candidate_messages.begin();
       it != candidate_messages.end(); ++it) {
    const bool is_close_candidate = ((it->message() == WM_IME_NOTIFY) &&
                                     (it->wparam() == IMN_CLOSECANDIDATE));
    if (!is_close_candidate) {
      other_candidate_messages.push_back(*it);
      continue;
    }
    sorted_messages->push_back(*it);
  }

  // Notify all composition UI messages except for WM_IME_ENDCOMPOSITION.
  // Typically WM_IME_STARTCOMPOSITION / WM_IME_COMPOSITION will be handled.
  std::vector<UIMessage> end_composition_messages;
  for (std::vector<UIMessage>::const_iterator it = composition_messages.begin();
       it != composition_messages.end(); ++it) {
    if (it->message() == WM_IME_ENDCOMPOSITION) {
      end_composition_messages.push_back(*it);
      continue;
    }
    sorted_messages->push_back(*it);
  }

  // Notify all other candidate UI messages.
  // Typically IMN_OPENCANDIDATE and IMN_CHANGECANDIDATE will be handled.
  for (std::vector<UIMessage>::const_iterator it =
           other_candidate_messages.begin();
       it != other_candidate_messages.end(); ++it) {
    DCHECK(!((it->message() == WM_IME_NOTIFY) &&
             (it->wparam() == IMN_CLOSECANDIDATE)));
    sorted_messages->push_back(*it);
  }

  // Notify WM_IME_ENDCOMPOSITION
  for (std::vector<UIMessage>::const_iterator it =
           end_composition_messages.begin();
       it != end_composition_messages.end(); ++it) {
    DCHECK_EQ(WM_IME_ENDCOMPOSITION, it->message());
    sorted_messages->push_back(*it);
  }

  // Notify IMN_SETOPENSTATUS for IME-OFF.
  if (open_status_changed && !next_open_status) {
    sorted_messages->push_back(UIMessage(WM_IME_NOTIFY, IMN_SETOPENSTATUS, 0));
  }

  sorted_messages->push_back(
      UIMessage(WM_IME_NOTIFY, IMN_PRIVATE, kNotifyUpdateUI));
}

bool ImeCore::UpdateContext(HIMC himc, const InputState &next_state,
                            const commands::Output &new_output,
                            MessageQueue *message_queue) {
  if (!IsInputContextInitialized(himc)) {
    return false;
  }

  if (!new_output.has_callback() ||
      !new_output.callback().has_session_command()) {
    // Callback is not requested.
    return UpdateContextMain(himc, next_state, new_output, message_queue);
  }

  // Callback exists.
  const SessionCommand &callback_command =
      new_output.callback().session_command();

  // Callback of CONVERT_REVERSE is an exception.
  // We ignore all other fields in |callback_command| in this case.
  // The UI handler will invoke reconversion later as other Japanese IMEs do.
  if (callback_command.has_type() &&
      callback_command.type() == SessionCommand::CONVERT_REVERSE) {
    if (message_queue != nullptr) {
      message_queue->AddMessage(WM_IME_NOTIFY, IMN_PRIVATE,
                                kNotifyReconvertFromIME);
    }
    return true;
  }

  // Otherwise, use |callback_output| and |callback_state| instead of
  // |new_output| and |next_mode|, respectively.
  UIContext context(himc);
  Output callback_output;
  if (!context.client()->SendCommand(callback_command, &callback_output)) {
    return false;
  }
  InputState callback_state;
  if (!GetNextState(himc, callback_output, &callback_state)) {
    return false;
  }
  return UpdateContextMain(himc, callback_state, callback_output,
                           message_queue);
}

bool ImeCore::UpdateContextMain(HIMC himc, const InputState &next_state,
                                const commands::Output &new_output,
                                MessageQueue *message_queue) {
  DCHECK(IsInputContextInitialized(himc));
  const bool generate_message = (message_queue != nullptr);
  ScopedHIMC<InputContext> context(himc);
  ScopedHIMCC<PrivateContext> private_context(context->hPrivate);

  // If the deletion range matches commands::Capability::DELETE_PRECEDING_TEXT,
  // initialize the deleter.
  if (generate_message && new_output.has_consumed() &&
      new_output.has_deletion_range() &&
      new_output.deletion_range().has_length() &&
      new_output.deletion_range().has_offset() &&
      (new_output.deletion_range().length() ==
       -new_output.deletion_range().offset())) {
    // If there remains an ongoing composition, it should be cleared before
    // VK_BACKs are delivered.  (b/3423449)
    UIContext uicontext(himc);
    if (!uicontext.IsCompositionStringEmpty()) {
      commands::Output empty_output;
      if (!UpdateCompositionStringAndPushMessages(himc, empty_output,
                                                  message_queue)) {
        return false;
      }
    }

    // Make sure the pending output does not have |deletion_range|.
    // Otherwise, an infinite loop will be created.
    commands::Output output;
    output.CopyFrom(new_output);
    output.clear_deletion_range();
    private_context->deleter->BeginDeletion(
        new_output.deletion_range().length(), output, next_state);
    return true;
  }

  if (new_output.has_consumed()) {
    private_context->last_output->CopyFrom(new_output);
  }

  *private_context->ime_state = next_state;
  const bool previous_open = (context->fOpen != FALSE);
  const DWORD previous_conversion = context->fdwConversion;
  const commands::Output &output = *private_context->last_output;

  // Update context.
  context->fOpen = next_state.open ? TRUE : FALSE;
  context->fdwConversion = next_state.logical_conversion_mode;

  std::vector<UIMessage> composition_messages;
  if (!UpdateCompositionString(himc, output, &composition_messages)) {
    return false;
  }

  std::vector<UIMessage> candidate_messages;
  context->hCandInfo = mozc::win32::CandidateInfoUtil::Update(
      context->hCandInfo, output, &candidate_messages);
  if (context->hCandInfo == nullptr) {
    return false;
  }

  if (generate_message) {
    // In order to minimize the risk of application compatibility problem,
    // we might want to send these messages in the same order to MS-IME.
    // See b/3488848 for details.
    std::vector<UIMessage> sorted_messages;
    SortIMEMessages(composition_messages, candidate_messages, previous_open,
                    previous_conversion, next_state.open,
                    next_state.logical_conversion_mode, &sorted_messages);

    // Allow visibility trackers to track if each UI message will be
    UIVisibilityTracker *ui_visibility_tracker =
        private_context->ui_visibility_tracker;
    for (std::vector<UIMessage>::const_iterator it = sorted_messages.begin();
         it != sorted_messages.end(); ++it) {
      if (UIVisibilityTracker::IsVisibilityTestMessageForCandidateWindow(
              it->message(), it->wparam(), it->lparam())) {
        ui_visibility_tracker->BeginVisibilityTestForCandidateWindow();
      }
      if (UIVisibilityTracker::IsVisibilityTestMessageForComposiwionWindow(
              it->message(), it->wparam(), it->lparam())) {
        ui_visibility_tracker->BeginVisibilityTestForCompositionWindow();
      }
      message_queue->AddMessage(it->message(), it->wparam(), it->lparam());
    }
  }

  return true;
}

BOOL ImeCore::IMEOff(HIMC himc, bool generate_message) {
  if (!IsInputContextInitialized(himc)) {
    return FALSE;
  }

  mozc::win32::UIContext context(himc);

  DWORD logical_conversion_mode = 0;
  if (!context.GetLogicalConversionMode(&logical_conversion_mode)) {
    return FALSE;
  }

  commands::Output output;
  if (!ImeCore::CloseIME(context.client(), logical_conversion_mode, &output)) {
    return FALSE;
  }
  bool next_open_status = false;
  DWORD next_logical_mode = 0;
  DWORD next_visible_mode = 0;
  if (!output.has_status()) {
    mozc::win32::UIContext uicontext(himc);
    if (!uicontext.GetConversionMode(&next_logical_mode)) {
      return FALSE;
    }
    next_visible_mode = next_logical_mode;
  } else if (!mozc::win32::ConversionModeUtil::ConvertStatusFromMozcToNative(
                 output.status(), context.IsKanaInputPreferred(),
                 &next_open_status, &next_logical_mode, &next_visible_mode)) {
    return FALSE;
  }
  InputState next_state;
  next_state.open = false;  // We ignore the returned status.
  next_state.logical_conversion_mode = next_logical_mode;
  next_state.visible_conversion_mode = next_visible_mode;
  if (!generate_message) {
    if (!UpdateContext(himc, next_state, output, nullptr)) {
      return FALSE;
    }
    return TRUE;
  }

  MessageQueue message_queue(himc);
  if (!UpdateContext(himc, next_state, output, &message_queue)) {
    return FALSE;
  }
  return (message_queue.Send() ? TRUE : FALSE);
}

BOOL ImeCore::HighlightCandidate(HIMC himc, int32 candidate_index,
                                 bool generate_message) {
  if (!IsInputContextInitialized(himc)) {
    return FALSE;
  }

  UIContext context(himc);
  if (context.IsEmpty()) {
    return FALSE;
  }

  int32 next_candidate_id = 0;
  {
    commands::Output last_output;
    if (!context.GetLastOutput(&last_output)) {
      return FALSE;
    }

    if (!OutputUtil::GetCandidateIdByIndex(last_output, candidate_index,
                                           &next_candidate_id)) {
      return FALSE;
    }

    // Stop sending HIGHLIGHT_CANDIDATE if the given candidate is already
    // selected.  If the |last_output| does not have focused candidate,
    // HIGHLIGHT_CANDIDATE is always be sent.
    int32 focused_candidate_id = 0;
    if (OutputUtil::GetFocusedCandidateId(last_output, &focused_candidate_id) &&
        (next_candidate_id == focused_candidate_id)) {
      // Already highlighted.
      return TRUE;
    }
  }

  commands::Output output;
  // TODO(yukawa, komatsu): Make a function in client dir.
  {
    mozc::commands::SessionCommand command;
    command.set_type(mozc::commands::SessionCommand::HIGHLIGHT_CANDIDATE);
    command.set_id(next_candidate_id);
    if (!context.client()->SendCommand(command, &output)) {
      return FALSE;
    }
  }

  return UpdateInputContext(himc, output, generate_message);
}

BOOL ImeCore::CloseCandidate(HIMC himc, bool generate_message) {
  if (!IsInputContextInitialized(himc)) {
    return FALSE;
  }

  UIContext context(himc);
  if (context.IsEmpty()) {
    return FALSE;
  }

  int32 focused_candidate_id = 0;
  {
    commands::Output last_output;
    if (!context.GetLastOutput(&last_output)) {
      return FALSE;
    }

    if (!last_output.has_all_candidate_words()) {
      // already closed.
      return TRUE;
    }

    // Although we should not handle CloseCandidate when a suggest window is
    // displayed, currently we need this path to support mouse clicking for
    // suggest window.

    if (!OutputUtil::GetFocusedCandidateId(last_output,
                                           &focused_candidate_id)) {
      return FALSE;
    }
  }

  commands::Output output;
  // TODO(yukawa, komatsu): Make a function in client dir.
  {
    mozc::commands::SessionCommand command;
    command.set_type(mozc::commands::SessionCommand::SELECT_CANDIDATE);
    command.set_id(focused_candidate_id);
    if (!context.client()->SendCommand(command, &output)) {
      return FALSE;
    }
  }

  return UpdateInputContext(himc, output, generate_message);
}

bool ImeCore::IsActiveContext(HIMC himc) {
  bool is_active = false;
  const HWND focus_window = ::GetFocus();
  if (focus_window != nullptr && ::IsWindow(focus_window) != FALSE) {
    const HIMC active_himc = ::ImmGetContext(focus_window);
    is_active = (himc == active_himc);
    ::ImmReleaseContext(focus_window, active_himc);
  }
  return is_active;
}

bool ImeCore::TurnOnIMEAndTryToReconvertFromIME(HIMC himc) {
  UIContext context(himc);
  if (context.IsEmpty()) {
    return false;
  }
  if (context.input_context() == nullptr) {
    return false;
  }
  if (!context.IsCompositionStringEmpty()) {
    // TODO(yukawa): Use Mozc server to determine the behavior when any
    //   appropriate protocol becomes available.
    DLOG(INFO) << "Ongoing composition exists.";
    return false;
  }

  const string &text_utf8 = GetTextForReconversionFromIME(himc);
  if (text_utf8.empty()) {
    if (context.GetOpenStatus()) {
      return true;
    }
    // Currently Mozc server will not turn on IME when |text_utf8| is empty but
    // people expect IME will be turned on even when the reconversion does
    // nothing.  b/4225148.
    return (::ImmSetOpenStatus(himc, TRUE) != FALSE);
  }

  commands::Output output;
  {
    mozc::commands::SessionCommand command;
    command.set_type(mozc::commands::SessionCommand::CONVERT_REVERSE);
    command.set_text(text_utf8);
    if (!context.client()->SendCommand(command, &output)) {
      LOG(ERROR) << "SendCommand failed.";
      return false;
    }
  }

  return UpdateInputContext(himc, output, true);
}

string ImeCore::GetTextForReconversionFromIME(HIMC himc) {
  // Implementation Note:
  // In order to implement IMM32 reconversion, IME is responsible to update
  // the following fields in RECONVERTSTRING.
  // - dwCompStrLen
  // - dwCompStrOffset
  // - dwTargetStrOffset
  // - dwTargetStrLen
  // However, current Mozc server supports only "pre-segmented" reconversion.
  // So the IME module assumes that the entire range pointed by
  // |RECONVERTSTRING::dwTargetStrOffset| and |RECONVERTSTRING::dwTargetStrLen|
  // is to be reconverted.  Technically most of the following processes should
  // be done at the server-side.
  LRESULT result = ::ImmRequestMessageW(himc, IMR_RECONVERTSTRING, 0);
  if (result == 0) {
    DLOG(INFO) << "IMR_RECONVERTSTRING is not supported.";
    return "";
  }

  const size_t buffer_size = static_cast<size_t>(result);
  if (buffer_size > kReconvertStringSizeLimit) {
    LOG(ERROR) << "Too large RECONVERTSTRING.";
    return "";
  }

  unique_ptr<BYTE[]> buffer(new BYTE[buffer_size]);

  RECONVERTSTRING *reconvert_string =
      reinterpret_cast<RECONVERTSTRING *>(buffer.get());
  reconvert_string->dwSize = buffer_size;
  reconvert_string->dwVersion = 0;

  result = ::ImmRequestMessageW(himc, IMR_RECONVERTSTRING,
                                reinterpret_cast<LPARAM>(reconvert_string));
  if (result == 0) {
    DLOG(ERROR) << "RECONVERTSTRING is nullptr.";
    return "";
  }

  unique_ptr<BYTE[]> copied_buffer(new BYTE[buffer_size]);
  for (size_t i = 0; i < buffer_size; ++i) {
    copied_buffer[i] = buffer[i];
  }
  RECONVERTSTRING *expanded_reconvert_string =
      reinterpret_cast<RECONVERTSTRING *>(copied_buffer.get());

  // Expand the composition range if necessary.
  if (!ReconvertString::EnsureCompositionIsNotEmpty(
          expanded_reconvert_string)) {
    return "";
  }
  result =
      ::ImmRequestMessageW(himc, IMR_CONFIRMRECONVERTSTRING,
                           reinterpret_cast<LPARAM>(expanded_reconvert_string));
  if (result != FALSE) {
    // The application accepted |expanded_reconvert_string|.
    reconvert_string = expanded_reconvert_string;
  }

  string total_composition_utf8;
  if (!GetReconvertString(reconvert_string, &total_composition_utf8)) {
    return "";
  }

  return total_composition_utf8;
}

bool ImeCore::QueryReconversionFromApplication(
    HIMC himc, RECONVERTSTRING *composition_info,
    RECONVERTSTRING *reading_info) {
  // Currently, we are ignoring |reading_info|.
  // TODO(yukawa): Support |reading_info|.

  if (!ReconvertString::EnsureCompositionIsNotEmpty(composition_info)) {
    return false;
  }

  string total_composition_utf8;
  if (!GetReconvertString(composition_info, &total_composition_utf8)) {
    return false;
  }

  return true;
}

bool ImeCore::ReconversionFromApplication(
    HIMC himc, const RECONVERTSTRING *composition_info,
    const RECONVERTSTRING *reading_info) {
  // Currently, we are ignoring |reading_info|.
  // TODO(yukawa): Support |reading_info|.

  UIContext context(himc);
  if (context.IsEmpty()) {
    return false;
  }
  if (context.input_context() == nullptr) {
    return false;
  }

  if (!context.IsCompositionStringEmpty()) {
    // TODO(yukawa): Use Mozc server to determine the behavior when any
    //   appropriate protocol becomes available.
    DLOG(INFO) << "Ongoing composition exists.";
    return false;
  }

  if (composition_info->dwCompStrLen == 0) {
    // There is no text selection. Reconversion cannot be started.
    return false;
  }

  string total_composition_utf8;
  if (!GetReconvertString(composition_info, &total_composition_utf8)) {
    return false;
  }

  commands::Output output;
  mozc::commands::SessionCommand command;
  command.set_type(mozc::commands::SessionCommand::CONVERT_REVERSE);
  command.set_text(total_composition_utf8);
  if (!context.client()->SendCommand(command, &output)) {
    LOG(ERROR) << "SendCommand failed.";
    return false;
  }

  return UpdateInputContext(himc, output, true);
}

}  // namespace win32
}  // namespace mozc
