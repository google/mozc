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

#include "win32/tip/tip_edit_session.h"

#include <msctf.h>
#include <wil/com.h>
#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "base/win32/com.h"
#include "base/win32/wide_char.h"
#include "client/client_interface.h"
#include "protocol/candidate_window.pb.h"
#include "protocol/commands.pb.h"
#include "win32/base/conversion_mode_util.h"
#include "win32/base/deleter.h"
#include "win32/base/input_state.h"
#include "win32/tip/tip_composition_util.h"
#include "win32/tip/tip_dll_module.h"
#include "win32/tip/tip_edit_session_impl.h"
#include "win32/tip/tip_input_mode_manager.h"
#include "win32/tip/tip_private_context.h"
#include "win32/tip/tip_range_util.h"
#include "win32/tip/tip_status.h"
#include "win32/tip/tip_surrounding_text.h"
#include "win32/tip/tip_text_service.h"
#include "win32/tip/tip_thread_context.h"
#include "win32/tip/tip_ui_handler.h"

namespace mozc {
namespace win32 {
namespace tsf {
namespace {

using ::mozc::commands::CandidateWindow;
using ::mozc::commands::DeletionRange;
using ::mozc::commands::Output;
using ::mozc::commands::SessionCommand;
using Candidate = ::mozc::commands::CandidateWindow_Candidate;
using CompositionMode = ::mozc::commands::CompositionMode;
using SpecialKey = ::mozc::commands::KeyEvent_SpecialKey;
using CommandType = ::mozc::commands::SessionCommand::CommandType;

// This class is an implementation class for the ITfEditSession classes, which
// is an observer for exclusively updating the text store of a TSF thread
// manager.
class AsyncLayoutChangeEditSessionImpl final
    : public TipComImplements<ITfEditSession> {
 public:
  AsyncLayoutChangeEditSessionImpl(
      wil::com_ptr_nothrow<TipTextService> text_service,
      wil::com_ptr_nothrow<ITfContext> context)
      : text_service_(std::move(text_service)), context_(std::move(context)) {}

  // The ITfEditSession interface method.
  // This function is called back by the TSF thread manager when an edit
  // request is granted.
  virtual STDMETHODIMP DoEditSession(TfEditCookie read_cookie) {
    // Ignore the returned code as TipUiHandler::UpdateUI will be called
    // anyway.
    text_service_->GetThreadContext()
        ->GetInputModeManager()
        ->OnMoveFocusedWindow();

    TipEditSessionImpl::UpdateUI(text_service_.get(), context_.get(),
                                 read_cookie);
    return S_OK;
  }

 private:
  wil::com_ptr_nothrow<TipTextService> text_service_;
  wil::com_ptr_nothrow<ITfContext> context_;
};

bool OnLayoutChangedAsyncImpl(TipTextService *text_service,
                              ITfContext *context) {
  if (!text_service || !context) {
    return false;
  }

  auto edit_session =
      MakeComPtr<AsyncLayoutChangeEditSessionImpl>(text_service, context);

  HRESULT edit_session_result = S_OK;
  const HRESULT hr = context->RequestEditSession(
      text_service->GetClientID(), edit_session.get(),
      TF_ES_ASYNCDONTCARE | TF_ES_READ, &edit_session_result);
  return SUCCEEDED(hr) && SUCCEEDED(edit_session_result);
}

// This class is an implementation class for the ITfEditSession classes, which
// is an observer for exclusively updating the text store of a TSF thread
// manager.
class AsyncSetFocusEditSessionImpl final
    : public TipComImplements<ITfEditSession> {
 public:
  AsyncSetFocusEditSessionImpl(
      wil::com_ptr_nothrow<TipTextService> text_service,
      wil::com_ptr_nothrow<ITfContext> context)
      : text_service_(std::move(text_service)), context_(std::move(context)) {}

  // The ITfEditSession interface method.
  // This function is called back by the TSF thread manager when an edit
  // request is granted.
  virtual STDMETHODIMP DoEditSession(TfEditCookie read_cookie) {
    std::vector<InputScope> input_scopes;
    wil::com_ptr_nothrow<ITfRange> selection_range;
    TfActiveSelEnd active_sel_end = TF_AE_NONE;
    if (SUCCEEDED(TipRangeUtil::GetDefaultSelection(
            context_.get(), read_cookie, &selection_range, &active_sel_end))) {
      TipRangeUtil::GetInputScopes(selection_range.get(), read_cookie,
                                   &input_scopes);
    }
    ITfThreadMgr *thread_manager = text_service_->GetThreadManager();
    TipThreadContext *thread_context = text_service_->GetThreadContext();
    DWORD system_input_mode = 0;
    if (!TipStatus::GetInputModeConversion(
            thread_manager, text_service_->GetClientID(), &system_input_mode)) {
      return E_FAIL;
    }
    const auto action = thread_context->GetInputModeManager()->OnSetFocus(
        TipStatus::IsOpen(thread_manager), system_input_mode, input_scopes);
    if (action == TipInputModeManager::kUpdateUI) {
      TipEditSessionImpl::UpdateUI(text_service_.get(), context_.get(),
                                   read_cookie);
    }
    return S_OK;
  }

 private:
  wil::com_ptr_nothrow<TipTextService> text_service_;
  wil::com_ptr_nothrow<ITfContext> context_;
};

bool OnUpdateOnOffModeAsync(TipTextService *text_service, ITfContext *context,
                            bool open) {
  const auto action = text_service->GetThreadContext()
                          ->GetInputModeManager()
                          ->OnChangeOpenClose(open);
  if (action == TipInputModeManager::kUpdateUI) {
    return OnLayoutChangedAsyncImpl(text_service, context);
  }
  return true;
}

// This class is an implementation class for the ITfEditSession classes, which
// is an observer for exclusively updating the text store of a TSF thread
// manager.
class AsyncSwitchInputModeEditSessionImpl final
    : public TipComImplements<ITfEditSession> {
 public:
  AsyncSwitchInputModeEditSessionImpl(
      wil::com_ptr_nothrow<TipTextService> text_service,
      wil::com_ptr_nothrow<ITfContext> context, bool open, uint32_t native_mode)
      : text_service_(std::move(text_service)),
        context_(std::move(context)),
        open_(open),
        native_mode_(native_mode) {}

  // The ITfEditSession interface method.
  // This function is called back by the TSF thread manager when an edit
  // request is granted.
  virtual STDMETHODIMP DoEditSession(TfEditCookie write_cookie) {
    TipPrivateContext *private_context =
        text_service_->GetPrivateContext(context_.get());
    if (!private_context) {
      // This is an unmanaged context. It's OK. Nothing to do.
      return S_OK;
    }

    const TipInputModeManager *input_mode_manager =
        text_service_->GetThreadContext()->GetInputModeManager();

    CompositionMode mozc_mode = commands::HIRAGANA;
    if (!ConversionModeUtil::ToMozcMode(native_mode_, &mozc_mode)) {
      return E_FAIL;
    }
    Output output;
    if (!open_) {
      // The next on/off mode is OFF. Send TURN_OFF_IME to update the converter
      // state.
      SessionCommand command;
      command.set_type(commands::SessionCommand::TURN_OFF_IME);
      command.set_composition_mode(mozc_mode);
      if (!private_context->GetClient()->SendCommand(command, &output)) {
        return E_FAIL;
      }
    } else if (!input_mode_manager->GetEffectiveOpenClose()) {
      // The next on/off mode is ON but the state of input mode manager is
      // OFF. Send TURN_ON_IME to update the converter state.
      SessionCommand command;
      command.set_type(commands::SessionCommand::TURN_ON_IME);
      command.set_composition_mode(mozc_mode);
      if (!private_context->GetClient()->SendCommand(command, &output)) {
        return E_FAIL;
      }
    } else {
      // The next on/off mode and the state of input mode manager is
      // consistent. Send SWITCH_COMPOSITION_MODE to update the converter state.
      SessionCommand command;
      command.set_type(SessionCommand::SWITCH_COMPOSITION_MODE);
      command.set_composition_mode(mozc_mode);
      if (!private_context->GetClient()->SendCommand(command, &output)) {
        return E_FAIL;
      }
    }
    return TipEditSessionImpl::UpdateContext(
        text_service_.get(), context_.get(), write_cookie, output);
  }

 private:
  wil::com_ptr_nothrow<TipTextService> text_service_;
  wil::com_ptr_nothrow<ITfContext> context_;
  bool open_;
  uint32_t native_mode_;
};

bool OnSwitchInputModeAsync(TipTextService *text_service, ITfContext *context,
                            bool open, uint32_t native_mode) {
  auto edit_session = MakeComPtr<AsyncSwitchInputModeEditSessionImpl>(
      text_service, context, open, native_mode);

  HRESULT edit_session_result = S_OK;
  const HRESULT hr = context->RequestEditSession(
      text_service->GetClientID(), edit_session.get(),
      TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &edit_session_result);
  return SUCCEEDED(hr) && SUCCEEDED(edit_session_result);
}

// This class is an implementation class for the ITfEditSession classes, which
// is an observer for exclusively updating the text store of a TSF thread
// manager.
class AsyncSessionCommandEditSessionImpl final
    : public TipComImplements<ITfEditSession> {
 public:
  AsyncSessionCommandEditSessionImpl(
      wil::com_ptr_nothrow<TipTextService> text_service,
      wil::com_ptr_nothrow<ITfContext> context,
      const SessionCommand &session_command)
      : text_service_(std::move(text_service)),
        context_(std::move(context)),
        session_command_(session_command) {}

  // The ITfEditSession interface method.
  // This function is called back by the TSF thread manager when an edit
  // request is granted.
  virtual STDMETHODIMP DoEditSession(TfEditCookie write_cookie) {
    Output output;
    TipPrivateContext *private_context =
        text_service_->GetPrivateContext(context_.get());
    if (private_context == nullptr) {
      return E_FAIL;
    }
    if (!private_context->GetClient()->SendCommand(session_command_, &output)) {
      return E_FAIL;
    }
    return TipEditSessionImpl::UpdateContext(
        text_service_.get(), context_.get(), write_cookie, output);
  }

 private:
  wil::com_ptr_nothrow<TipTextService> text_service_;
  wil::com_ptr_nothrow<ITfContext> context_;
  SessionCommand session_command_;
};

bool OnSessionCommandAsync(TipTextService *text_service, ITfContext *context,
                           const SessionCommand &session_command) {
  auto edit_session = MakeComPtr<AsyncSessionCommandEditSessionImpl>(
      text_service, context, session_command);

  HRESULT edit_session_result = S_OK;
  const HRESULT hr = context->RequestEditSession(
      text_service->GetClientID(), edit_session.get(),
      TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &edit_session_result);
  return SUCCEEDED(hr) && SUCCEEDED(edit_session_result);
}

bool TurnOnImeAndTryToReconvertFromIme(TipTextService *text_service,
                                       ITfContext *context) {
  if (context == nullptr) {
    return false;
  }

  TipSurroundingTextInfo info;
  bool need_async_edit_session = false;
  if (!TipSurroundingText::PrepareForReconversionFromIme(
          text_service, context, &info, &need_async_edit_session)) {
    return false;
  }

  // Currently this is not supported.
  if (info.in_composition) {
    return false;
  }

  std::string text_utf8 = WideToUtf8(info.selected_text);
  if (text_utf8.empty()) {
    const bool open = text_service->GetThreadContext()
                          ->GetInputModeManager()
                          ->GetEffectiveOpenClose();
    if (open) {
      return true;
    }
    // Currently Mozc server will not turn on IME when |text_utf8| is empty but
    // people expect IME will be turned on even when the reconversion does
    // nothing.  b/4225148.
    return OnUpdateOnOffModeAsync(text_service, context, true);
  }

  TipPrivateContext *private_context = text_service->GetPrivateContext(context);
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

  if (output.has_callback() && output.callback().has_session_command() &&
      output.callback().session_command().has_type()) {
    // do not allow recursive call.
    return false;
  }

  if (need_async_edit_session) {
    return TipEditSession::OnOutputReceivedAsync(text_service, context,
                                                 std::move(output));
  } else {
    return TipEditSession::OnOutputReceivedSync(text_service, context,
                                                std::move(output));
  }
}

bool UndoCommint(TipTextService *text_service, ITfContext *context) {
  if (context == nullptr) {
    return false;
  }

  TipPrivateContext *private_context = text_service->GetPrivateContext(context);
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
  const size_t num_characters_to_be_deleted_codepoint =
      -deletion_range.offset();
  if (!TipSurroundingText::DeletePrecedingText(
          text_service, context, num_characters_to_be_deleted_codepoint)) {
    // If TSF-based delete-preceding-text fails, use backspace forwarding as
    // a fall back.

    // Make sure the pending output does not have |deletion_range|.
    // Otherwise, an infinite loop will be created.
    Output pending_output = output;
    pending_output.clear_deletion_range();

    // actually |next_state| will be ignored in TSF Mozc.
    // So it is OK to pass the default value.
    InputState next_state;
    private_context->GetDeleter()->BeginDeletion(deletion_range.length(),
                                                 pending_output, next_state);
    return true;
  }

  if (output.has_callback() && output.callback().has_session_command() &&
      output.callback().session_command().has_type()) {
    // do not allow recursive call.
    return false;
  }

  // Undo commit should be called from and only from the key event handler.
  return TipEditSession::OnOutputReceivedSync(text_service, context,
                                              std::move(output));
}

bool IsCandidateFocused(const Output &output, uint32_t candidate_id) {
  if (!output.has_candidate_window()) {
    return false;
  }
  const CandidateWindow &candidate_window = output.candidate_window();

  if (!candidate_window.has_focused_index()) {
    return false;
  }
  const uint32_t focused_index = candidate_window.focused_index();
  for (size_t i = 0; i < candidate_window.candidate_size(); ++i) {
    const Candidate &candidate = candidate_window.candidate(i);
    if (candidate.index() != focused_index) {
      continue;
    }
    if (candidate.id() == candidate_id) {
      return true;
    }
  }
  return false;
}

// This class is an implementation class for the ITfEditSession classes, which
// is an observer for exclusively updating the text store of a TSF thread
// manager.
class SyncEditSessionImpl final : public TipComImplements<ITfEditSession> {
 public:
  SyncEditSessionImpl(wil::com_ptr_nothrow<TipTextService> text_service,
                      wil::com_ptr_nothrow<ITfContext> context, Output output)
      : text_service_(std::move(text_service)),
        context_(std::move(context)),
        output_(std::move(output)) {}

  // The ITfEditSession interface method.
  // This function is called back by the TSF thread manager when an edit
  // request is granted.
  virtual STDMETHODIMP DoEditSession(TfEditCookie write_cookie) {
    return TipEditSessionImpl::UpdateContext(
        text_service_.get(), context_.get(), write_cookie, output_);
  }

 private:
  wil::com_ptr_nothrow<TipTextService> text_service_;
  wil::com_ptr_nothrow<ITfContext> context_;
  Output output_;
};

enum EditSessionMode {
  kDontCare = 0,
  kAsync,
  kSync,
};

bool OnOutputReceivedImpl(TipTextService *text_service, ITfContext *context,
                          Output new_output, EditSessionMode mode) {
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
      default:
        break;
    }
  }

  auto edit_session = MakeComPtr<SyncEditSessionImpl>(text_service, context,
                                                      std::move(new_output));

  TipThreadContext *thread_context = text_service->GetThreadContext();

  if (mode == kSync && thread_context->use_async_lock_in_key_handler()) {
    // A workaround for MS Word's failure mode.
    // See https://github.com/google/mozc/issues/819 for details.
    // TODO(https://github.com/google/mozc/issues/821): Remove this workaround.
    mode = kAsync;
  }

  DWORD edit_session_flag = TF_ES_READWRITE;
  switch (mode) {
    case kAsync:
      edit_session_flag |= TF_ES_ASYNC;
      break;
    case kSync:
      edit_session_flag |= TF_ES_SYNC;
      break;
    case kDontCare:
      edit_session_flag |= TF_ES_ASYNCDONTCARE;
      break;
    default:
      DCHECK(false) << "unknown mode: " << mode;
      break;
  }

  HRESULT edit_session_result = S_OK;
  const HRESULT hr = context->RequestEditSession(
      text_service->GetClientID(), edit_session.get(), edit_session_flag,
      &edit_session_result);

  if (mode == kSync && edit_session_result == TF_E_SYNCHRONOUS) {
    // A workaround for MS Word's failure mode.
    // See https://github.com/google/mozc/issues/819 for details.
    // TODO(https://github.com/google/mozc/issues/821): Remove this workaround.
    thread_context->set_use_async_lock_in_key_handler(true);
  }

  return SUCCEEDED(hr) && SUCCEEDED(edit_session_result);
}

// This class is an implementation class for the ITfEditSession classes, which
// is an observer for exclusively updating the text store of a TSF thread
// manager.
class SyncGetTextEditSessionImpl final
    : public TipComImplements<ITfEditSession> {
 public:
  SyncGetTextEditSessionImpl(wil::com_ptr_nothrow<TipTextService> text_service,
                             wil::com_ptr_nothrow<ITfRange> range,
                             bool check_composition)
      : text_service_(std::move(text_service)),
        range_(std::move(range)),
        check_composition_(check_composition) {}

  // The ITfEditSession interface method.
  // This function is called back by the TSF thread manager when an edit
  // request is granted.
  virtual STDMETHODIMP DoEditSession(TfEditCookie read_cookie) {
    HRESULT result = TipRangeUtil::GetText(range_.get(), read_cookie, &text_);
    if (FAILED(result)) {
      return result;
    }
    if (check_composition_) {
      wil::com_ptr_nothrow<ITfCompositionView> composition_view =
          TipCompositionUtil::GetCompositionViewFromRange(range_.get(),
                                                          read_cookie);
      is_composing_ = !!composition_view;
    }
    return S_OK;
  }

  const std::wstring &text() const { return text_; }
  bool is_composing() const { return is_composing_; }

 private:
  wil::com_ptr_nothrow<TipTextService> text_service_;
  wil::com_ptr_nothrow<ITfRange> range_;
  bool check_composition_ = false;
  std::wstring text_;
  bool is_composing_ = false;
};

// This class is an implementation class for the ITfEditSession classes, which
// is an observer for exclusively updating the text store of a TSF thread
// manager.
class AsyncSetTextEditSessionImpl final
    : public TipComImplements<ITfEditSession> {
 public:
  AsyncSetTextEditSessionImpl(wil::com_ptr_nothrow<TipTextService> text_service,
                              const std::wstring_view text,
                              wil::com_ptr_nothrow<ITfRange> range)
      : text_service_(std::move(text_service)),
        text_(text),
        range_(std::move(range)) {}

  // The ITfEditSession interface method.
  // This function is called back by the TSF thread manager when an edit
  // request is granted.
  virtual STDMETHODIMP DoEditSession(TfEditCookie write_cookie) {
    range_->SetText(write_cookie, 0, text_.data(), text_.size());
    return S_OK;
  }

 private:
  wil::com_ptr_nothrow<TipTextService> text_service_;
  const std::wstring text_;
  wil::com_ptr_nothrow<ITfRange> range_;
};

}  // namespace

bool TipEditSession::OnOutputReceivedSync(TipTextService *text_service,
                                          ITfContext *context,
                                          Output new_output) {
  return OnOutputReceivedImpl(text_service, context, std::move(new_output),
                              kSync);
}

bool TipEditSession::OnOutputReceivedAsync(TipTextService *text_service,
                                           ITfContext *context,
                                           Output new_output) {
  return OnOutputReceivedImpl(text_service, context, std::move(new_output),
                              kAsync);
}

bool TipEditSession::OnLayoutChangedAsync(TipTextService *text_service,
                                          ITfContext *context) {
  return OnLayoutChangedAsyncImpl(text_service, context);
}

bool TipEditSession::OnSetFocusAsync(TipTextService *text_service,
                                     ITfDocumentMgr *document_manager) {
  if (document_manager == nullptr) {
    TipUiHandler::OnFocusChange(text_service, nullptr);
    return true;
  }

  wil::com_ptr_nothrow<ITfContext> context;
  if (FAILED(document_manager->GetBase(&context))) {
    return false;
  }

  // When RequestEditSession fails, it does not maintain the reference count.
  // So we need to ensure that AddRef/Release should be called at least once
  // per object.
  auto edit_session =
      MakeComPtr<AsyncSetFocusEditSessionImpl>(text_service, context);

  HRESULT edit_session_result = S_OK;
  const HRESULT hr = context->RequestEditSession(
      text_service->GetClientID(), edit_session.get(),
      TF_ES_ASYNCDONTCARE | TF_ES_READ, &edit_session_result);
  return SUCCEEDED(hr) && SUCCEEDED(edit_session_result);
}

bool TipEditSession::OnModeChangedAsync(TipTextService *text_service) {
  ITfThreadMgr *thread_mgr = text_service->GetThreadManager();
  if (thread_mgr == nullptr) {
    return false;
  }

  wil::com_ptr_nothrow<ITfDocumentMgr> document_manager;
  if (FAILED(thread_mgr->GetFocus(&document_manager))) {
    return false;
  }
  if (document_manager == nullptr) {
    // This is an unmanaged context. It's OK. Nothing to do.
    return true;
  }

  wil::com_ptr_nothrow<ITfContext> context;
  if (FAILED(document_manager->GetBase(&context))) {
    return false;
  }
  DWORD native_mode = false;
  if (!TipStatus::GetInputModeConversion(text_service->GetThreadManager(),
                                         text_service->GetClientID(),
                                         &native_mode)) {
    return false;
  }
  const auto action = text_service->GetThreadContext()
                          ->GetInputModeManager()
                          ->OnChangeConversionMode(native_mode);
  if (action == TipInputModeManager::kUpdateUI) {
    return OnLayoutChangedAsyncImpl(text_service, context.get());
  }
  return true;
}

bool TipEditSession::OnOpenCloseChangedAsync(TipTextService *text_service) {
  wil::com_ptr_nothrow<ITfDocumentMgr> document_manager;
  if (FAILED(text_service->GetThreadManager()->GetFocus(&document_manager))) {
    return false;
  }
  if (document_manager == nullptr) {
    // This is an unmanaged context. It's OK. Nothing to do.
    return true;
  }

  wil::com_ptr_nothrow<ITfContext> context;
  if (FAILED(document_manager->GetBase(&context))) {
    return false;
  }
  return OnUpdateOnOffModeAsync(
      text_service, context.get(),
      TipStatus::IsOpen(text_service->GetThreadManager()));
}

bool TipEditSession::OnRendererCallbackAsync(TipTextService *text_service,
                                             ITfContext *context, WPARAM wparam,
                                             LPARAM lparam) {
  const CommandType type = static_cast<CommandType>(wparam);
  switch (type) {
    case SessionCommand::HIGHLIGHT_CANDIDATE:
    case SessionCommand::SELECT_CANDIDATE: {
      const int32_t candidate_id = static_cast<int32_t>(lparam);
      TipPrivateContext *private_context =
          text_service->GetPrivateContext(context);
      if (private_context == nullptr) {
        return false;
      }
      if ((type == SessionCommand::HIGHLIGHT_CANDIDATE) &&
          IsCandidateFocused(private_context->last_output(), candidate_id)) {
        // Already focused. Nothing to do.
        return true;
      }

      SessionCommand command;
      command.set_type(type);
      command.set_id(candidate_id);
      return OnSessionCommandAsync(text_service, context, command);
    }
    default:
      return false;
  }
}

bool TipEditSession::SubmitAsync(TipTextService *text_service,
                                 ITfContext *context) {
  TipPrivateContext *private_context = text_service->GetPrivateContext(context);
  if (!private_context) {
    // This is an unmanaged context.
    return false;
  }

  SessionCommand session_command;
  session_command.set_type(SessionCommand::SUBMIT);
  return OnSessionCommandAsync(text_service, context, session_command);
}

bool TipEditSession::CancelCompositionAsync(TipTextService *text_service,
                                            ITfContext *context) {
  SessionCommand command;
  command.set_type(SessionCommand::REVERT);
  return OnSessionCommandAsync(text_service, context, command);
}

bool TipEditSession::HilightCandidateAsync(TipTextService *text_service,
                                           ITfContext *context,
                                           int candidate_id) {
  TipPrivateContext *private_context = text_service->GetPrivateContext(context);
  if (!private_context) {
    // This is an unmanaged context.
    return false;
  }

  SessionCommand session_command;
  session_command.set_type(SessionCommand::HIGHLIGHT_CANDIDATE);
  session_command.set_id(candidate_id);
  return OnSessionCommandAsync(text_service, context, session_command);
}

bool TipEditSession::SelectCandidateAsync(TipTextService *text_service,
                                          ITfContext *context,
                                          int candidate_id) {
  TipPrivateContext *private_context = text_service->GetPrivateContext(context);
  if (!private_context) {
    // This is an unmanaged context.
    return false;
  }

  SessionCommand session_command;
  session_command.set_type(SessionCommand::SELECT_CANDIDATE);
  session_command.set_id(candidate_id);
  return OnSessionCommandAsync(text_service, context, session_command);
}

bool TipEditSession::ReconvertFromApplicationSync(TipTextService *text_service,
                                                  ITfRange *range) {
  if (range == nullptr) {
    return false;
  }
  wil::com_ptr_nothrow<ITfContext> context;
  if (FAILED(range->GetContext(&context))) {
    return false;
  }
  TipPrivateContext *private_context =
      text_service->GetPrivateContext(context.get());
  if (!private_context) {
    // This is an unmanaged context.
    return false;
  }

  std::wstring selected_text;
  bool is_composing = false;
  if (!GetTextSync(text_service, range, &selected_text, &is_composing)) {
    return false;
  }

  if (selected_text.empty()) {
    // Selected text is empty. Nothing to do.
    return false;
  }

  if (is_composing) {
    // on-going composition is found.
    return false;
  }

  // Stop reconversion when any embedded object is found because we cannot
  // easily restore it. See b/3406434
  if (selected_text.find(static_cast<wchar_t>(TS_CHAR_EMBEDDED)) !=
      std::wstring::npos) {
    // embedded object is found.
    return false;
  }

  SessionCommand command;
  command.set_type(SessionCommand::CONVERT_REVERSE);
  command.set_text(WideToUtf8(selected_text));
  Output output;
  if (!private_context->GetClient()->SendCommand(command, &output)) {
    return false;
  }
  return OnOutputReceivedSync(text_service, context.get(), std::move(output));
}

bool TipEditSession::SwitchInputModeAsync(TipTextService *text_service,
                                          uint32_t mozc_mode) {
  commands::CompositionMode mode =
      static_cast<commands::CompositionMode>(mozc_mode);

  if (text_service == nullptr) {
    return false;
  }
  ITfThreadMgr *thread_mgr = text_service->GetThreadManager();
  if (thread_mgr == nullptr) {
    return false;
  }

  wil::com_ptr_nothrow<ITfDocumentMgr> document;
  if (FAILED(thread_mgr->GetFocus(&document))) {
    return false;
  }
  if (document == nullptr) {
    // This is an unmanaged context. It's OK. Nothing to do.
    return true;
  }

  wil::com_ptr_nothrow<ITfContext> context;
  if (FAILED(document->GetBase(&context))) {
    return false;
  }

  Output output;
  if (mode == commands::DIRECT) {
    DWORD native_mode = 0;
    if (!TipStatus::GetInputModeConversion(text_service->GetThreadManager(),
                                           text_service->GetClientID(),
                                           &native_mode)) {
      return false;
    }
    return OnSwitchInputModeAsync(text_service, context.get(), false,
                                  native_mode);
  }
  TipPrivateContext *private_context =
      text_service->GetPrivateContext(context.get());
  if (!private_context) {
    // This is an unmanaged context.
    return false;
  }

  uint32_t native_mode = 0;
  if (!ConversionModeUtil::ToNativeMode(
          mode, private_context->input_behavior().prefer_kana_input,
          &native_mode)) {
    return false;
  }

  return OnSwitchInputModeAsync(text_service, context.get(), true, native_mode);
}

bool TipEditSession::GetTextSync(TipTextService *text_service, ITfRange *range,
                                 std::wstring *text, bool *is_composing) {
  wil::com_ptr_nothrow<ITfContext> context;
  if (FAILED(range->GetContext(&context))) {
    return false;
  }
  const bool check_composition = (is_composing != nullptr);
  auto get_text = MakeComPtr<SyncGetTextEditSessionImpl>(text_service, range,
                                                         check_composition);

  HRESULT hr = S_OK;
  HRESULT hr_session = S_OK;
  hr = context->RequestEditSession(text_service->GetClientID(), get_text.get(),
                                   TF_ES_SYNC | TF_ES_READ, &hr_session);
  if (FAILED(hr)) {
    return false;
  }
  *text = get_text->text();
  if (check_composition) {
    *is_composing = get_text->is_composing();
  }
  return true;
}

// static
bool TipEditSession::SetTextAsync(TipTextService *text_service,
                                  const std::wstring_view text,
                                  ITfRange *range) {
  wil::com_ptr_nothrow<ITfContext> context;
  if (FAILED(range->GetContext(&context))) {
    return false;
  }
  auto set_text =
      MakeComPtr<AsyncSetTextEditSessionImpl>(text_service, text, range);

  HRESULT hr = S_OK;
  HRESULT hr_session = S_OK;
  hr = context->RequestEditSession(text_service->GetClientID(), set_text.get(),
                                   TF_ES_ASYNCDONTCARE | TF_ES_READWRITE,
                                   &hr_session);
  return SUCCEEDED(hr);
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc
