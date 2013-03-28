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

#include "win32/tip/tip_command_handler.h"

#include <Windows.h>

#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
// Workaround against KB813540
#include <atlbase_mozc.h>
#include <atlcom.h>

#include <string>

#include "base/util.h"
#include "client/client_interface.h"
#include "session/commands.pb.h"
#include "win32/base/conversion_mode_util.h"
#include "win32/base/deleter.h"
#include "win32/base/input_state.h"
#include "win32/tip/tip_composition_util.h"
#include "win32/tip/tip_edit_session.h"
#include "win32/tip/tip_private_context.h"
#include "win32/tip/tip_status.h"
#include "win32/tip/tip_surrounding_text.h"
#include "win32/tip/tip_text_service.h"

namespace mozc {
namespace win32 {
namespace tsf {

namespace {

using ATL::CComPtr;
using commands::DeletionRange;
using commands::KeyEvent;
using commands::Output;
using commands::SessionCommand;
typedef commands::KeyEvent_SpecialKey SpecialKey;

bool UpdateOpenStateInternal(TipTextService *text_service,
                             ITfContext *context,
                             bool open) {
  TipPrivateContext *private_context =
      text_service->GetPrivateContext(context);
  if (!private_context) {
    // This is an unmanaged context. It's OK. Nothing to do.
    return true;
  }

  if (private_context->input_state().open == open) {
    // State is already consistent. Nothing to do.
    return true;
  }

  // State is not consistent. Send a special key event to the server to
  // resolve this inconsistency.
  KeyEvent key_event;
  key_event.set_special_key(open ? commands::KeyEvent_SpecialKey_ON
                                 : commands::KeyEvent_SpecialKey_OFF);
  DWORD native_mode = 0;
  if (TipStatus::GetInputModeConversion(text_service->GetThreadManager(),
                                        text_service->GetClientID(),
                                        &native_mode)) {
    commands::CompositionMode mode = commands::HIRAGANA;
    if (ConversionModeUtil::ToMozcMode(native_mode, &mode)) {
      key_event.set_mode(mode);
    }
  }

  Output output;
  if (!private_context->GetClient()->SendKey(key_event, &output)) {
    return false;
  }

  return TipCommandHandler::OnCommandReceived(
      text_service, context, output);
}

bool TurnOnImeAndTryToReconvertFromIme(TipTextService *text_service,
                                       ITfContext *context) {
  if (context == nullptr) {
    return false;
  }

  TipSurroundingTextInfo info;
  if (!TipSurroundingText::PrepareForReconversion(
          text_service, context, &info)) {
    return false;
  }

  // Currently this is not supported.
  if (info.in_composition) {
    return false;
  }

  string text_utf8;
  Util::WideToUTF8(info.selected_text, &text_utf8);
  if (text_utf8.empty()) {
    if (TipStatus::IsOpen(text_service->GetThreadManager())) {
      return true;
    }
    // Currently Mozc server will not turn on IME when |text_utf8| is empty but
    // people expect IME will be turned on even when the reconversion does
    // nothing.  b/4225148.
    return UpdateOpenStateInternal(text_service, context, true);
  }

  TipPrivateContext *private_context =
      text_service->GetPrivateContext(context);
  if (!private_context) {
    // This is an unmanaged context. It's OK. Nothing to do.
    return true;
  }

  Output output;
  {
    SessionCommand command;
    command.set_type(SessionCommand::CONVERT_REVERSE);
    command.set_text(text_utf8);
    if (!private_context->GetClient()->SendCommand(command, &output)) {
      return false;
    }
  }

  if (output.has_callback() &&
      output.callback().has_session_command() &&
      output.callback().session_command().has_type()) {
    // do not allow recursive call.
    return false;
  }

  return TipCommandHandler::OnCommandReceived(text_service, context, output);
}

bool UndoCommint(TipTextService *text_service, ITfContext *context) {
  if (context == nullptr) {
    return false;
  }

  TipPrivateContext *private_context =
      text_service->GetPrivateContext(context);
  if (!private_context) {
    // This is an unmanaged context. It's OK. Nothing to do.
    return true;
  }

  Output output;
  {
    SessionCommand command;
    command.set_type(SessionCommand::UNDO);
    if (!private_context->GetClient()->SendCommand(command, &output)) {
      return false;
    }
  }

  if (!output.has_deletion_range()) {
    return false;
  }

  const DeletionRange &deletion_range = output.deletion_range();
  if (deletion_range.offset() > 0 ||
      -deletion_range.offset() != deletion_range.length()) {
    return false;
  }
  const size_t num_characters_to_be_deleted_ucs4 = -deletion_range.offset();
  if (!TipSurroundingText::DeletePrecedingText(
          text_service, context, num_characters_to_be_deleted_ucs4)) {
    // If TSF-based delete-preceeding-text fails, use backspace forwarding as
    // a fallback.

    // Make sure the pending output does not have |deletion_range|.
    // Otherwise, an infinite loop will be created.
    Output pending_output;
    pending_output.CopyFrom(output);
    pending_output.clear_deletion_range();

    // actually |next_state| will be ignored in TSF Mozc.
    // So it is OK to pass the default value.
    InputState next_state;
    private_context->GetDeleter()->BeginDeletion(
        deletion_range.length(), pending_output, next_state);
    return true;
  }

  if (output.has_callback() &&
      output.callback().has_session_command() &&
      output.callback().session_command().has_type()) {
    // do not allow recursive call.
    return false;
  }
  return TipCommandHandler::OnCommandReceived(text_service, context, output);
}

}  // namespace

bool TipCommandHandler::OnCommandReceived(TipTextService *text_service,
                                          ITfContext *context,
                                          const Output &new_output) {
  if (new_output.has_callback() &&
      new_output.callback().has_session_command() &&
      new_output.callback().session_command().has_type()) {
    // Callback exists.
    const SessionCommand::CommandType &type =
        new_output.callback().session_command().type();
    switch (type) {
      case SessionCommand::CONVERT_REVERSE:
        return TurnOnImeAndTryToReconvertFromIme(text_service, context);
      case SessionCommand::UNDO:
        return UndoCommint(text_service, context);
    }
  }

  // When RequestEditSession fails, it does not maintain the reference count.
  // So we need to ensure that AddRef/Release should be called at least once
  // per oject.
  CComPtr<ITfEditSession> edit_session(TipEditSession::New(
      text_service, context, new_output));

  // TSF is responsible for the lifecycle management of the instance of
  // ITfEditSession object passed.
  HRESULT edit_session_result = S_OK;
  const HRESULT hr = context->RequestEditSession(
      text_service->GetClientID(),
      edit_session,
      TF_ES_ASYNCDONTCARE | TF_ES_READWRITE,
      &edit_session_result);
  if (FAILED(hr)) {
    return false;
  }
  return SUCCEEDED(edit_session_result);
}

bool TipCommandHandler::NotifyCompositionReverted(TipTextService *text_service,
                                                  ITfContext *context,
                                                  ITfComposition *composition,
                                                  TfEditCookie cookie) {
  // Ignore any error.
  TipCompositionUtil::ClearProperties(context, composition, cookie);

  TipPrivateContext *private_context =
      text_service->GetPrivateContext(context);
  if (!private_context) {
    return false;
  }

  Output output;
  SessionCommand command;
  command.set_type(SessionCommand::REVERT);
  if (!private_context->GetClient()->SendCommand(command, &output)) {
    return false;
  }

  return true;
}

bool TipCommandHandler::OnCompositionTerminated(TipTextService *text_service,
                                                ITfContext *context,
                                                ITfComposition *composition,
                                                TfEditCookie cookie) {
  if (!composition) {
    return false;
  }
  // Ignore any error.
  TipCompositionUtil::ClearProperties(context, composition, cookie);

  TipPrivateContext *private_context =
      text_service->GetPrivateContext(context);
  if (!private_context) {
    return false;
  }
  Output output;
  SessionCommand command;
  command.set_type(SessionCommand::SUBMIT);
  if (!private_context->GetClient()->SendCommand(command, &output)) {
    return false;
  }
  return OnCommandReceived(text_service, context, output);
}

bool TipCommandHandler::OnOpenCloseChanged(TipTextService *text_service) {
  CComPtr<ITfDocumentMgr> document_manager;
  if (FAILED(text_service->GetThreadManager()->GetFocus(&document_manager))) {
    return false;
  }
  if (document_manager == nullptr) {
    // This is an unmanaged context. It's OK. Nothing to do.
    return true;
  }

  CComPtr<ITfContext> context;
  if (FAILED(document_manager->GetBase(&context))) {
    return false;
  }
  const bool open = TipStatus::IsOpen(text_service->GetThreadManager());
  return UpdateOpenStateInternal(text_service, context, open);
}

bool TipCommandHandler::Submit(TipTextService *text_service,
                               ITfContext *context) {
  TipPrivateContext *private_context =
      text_service->GetPrivateContext(context);
  if (!private_context) {
    // This is an unmanaged context.
    return false;
  }

  SessionCommand command;
  command.set_type(SessionCommand::SUBMIT);
  Output output;
  if (!private_context->GetClient()->SendCommand(command, &output)) {
    return false;
  }
  return OnCommandReceived(text_service, context, output);
}

bool TipCommandHandler::SelectCandidate(TipTextService *text_service,
                                        ITfContext *context,
                                        int candidate_id) {
  TipPrivateContext *private_context =
      text_service->GetPrivateContext(context);
  if (!private_context) {
    // This is an unmanaged context.
    return false;
  }

  SessionCommand command;
  command.set_type(SessionCommand::SELECT_CANDIDATE);
  command.set_id(candidate_id);
  Output output;
  if (!private_context->GetClient()->SendCommand(command, &output)) {
    return false;
  }
  return OnCommandReceived(text_service, context, output);
}

bool TipCommandHandler::ReconvertFromApplication(
    TipTextService *text_service,
    ITfRange *range) {
  if (range == nullptr) {
    return false;
  }
  CComPtr<ITfContext> context;
  if (FAILED(range->GetContext(&context))) {
    return false;
  }
  TipPrivateContext *private_context =
      text_service->GetPrivateContext(context);
  if (!private_context) {
    // This is an unmanaged context.
    return false;
  }

  TipSurroundingTextInfo info;
  if (!TipSurroundingText::Get(text_service, context, &info)) {
    return false;
  }

  if (info.in_composition) {
    // on-going composition is found.
    return false;
  }

  // Stop reconversion when any embedded object is found because we cannot
  // easily restore it. See b/3406434
  if (info.selected_text.find(static_cast<wchar_t>(TS_CHAR_EMBEDDED)) !=
      wstring::npos) {
    // embedded object is found.
    return false;
  }

  SessionCommand command;
  command.set_type(SessionCommand::CONVERT_REVERSE);

  string text_utf8;
  Util::WideToUTF8(info.selected_text, &text_utf8);
  command.set_text(text_utf8);
  Output output;
  if (!private_context->GetClient()->SendCommand(command, &output)) {
    return false;
  }
  return OnCommandReceived(text_service, context, output);
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc
