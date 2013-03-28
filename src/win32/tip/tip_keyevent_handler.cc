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

#include "win32/tip/tip_keyevent_handler.h"

#include <Windows.h>
#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
// Workaround against KB813540
#include <atlbase_mozc.h>
#include <atlcom.h>
#include <msctf.h>

#include <string>

#include "base/util.h"
#include "session/commands.pb.h"
#include "win32/base/conversion_mode_util.h"
#include "win32/base/deleter.h"
#include "win32/base/input_state.h"
#include "win32/base/keyboard.h"
#include "win32/base/keyevent_handler.h"
#include "win32/base/surrogate_pair_observer.h"
#include "win32/base/win32_window_util.h"
#include "win32/tip/tip_command_handler.h"
#include "win32/tip/tip_private_context.h"
#include "win32/tip/tip_ref_count.h"
#include "win32/tip/tip_status.h"
#include "win32/tip/tip_text_service.h"

namespace mozc {
namespace win32 {
namespace tsf {

using ATL::CComPtr;
using ::mozc::commands::Output;

namespace {

// Defined in the following white paper.
// http://msdn.microsoft.com/en-us/library/windows/apps/hh967425.aspx
const UINT kTouchKeyboardNextPage = 0xf003;
const UINT kTouchKeyboardPreviousPage = 0xf004;

// Unlike IMM32 Mozc which is marked as IME_PROP_ACCEPT_WIDE_VKEY,
// TSF Mozc cannot always receive a VK_PACKET keyevent whose high word consists
// of a Unicode character. To retrieve the underlaying Unicode character,
// use ToUnicode API as documented in the following white paper.
// http://msdn.microsoft.com/en-us/library/windows/apps/hh967425.aspx
VirtualKey GetVK(WPARAM wparam, const KeyboardStatus &keyboad_status) {
  if (LOWORD(wparam) != VK_PACKET) {
    return VirtualKey::FromVirtualKey(wparam);
  }

  const UINT scan_code = ::MapVirtualKey(wparam, MAPVK_VK_TO_VSC);
  wchar_t buffer[4] = {};
  if (::ToUnicode(wparam, scan_code, keyboad_status.status(), buffer,
                  arraysize(buffer), 0) != 1) {
    return VirtualKey::FromVirtualKey(wparam);
  }
  const uint32 ucs2 = buffer[0];
  if (ucs2 == L' ') {
    return VirtualKey::FromVirtualKey(VK_SPACE);
  }
  if (L'0' <= ucs2 && ucs2 <= L'9') {
    return VirtualKey::FromVirtualKey(ucs2);
  }
  if (L'a' <= ucs2 && ucs2 <= L'z') {
    return VirtualKey::FromVirtualKey(L'z' - ucs2 + L'A');
  }
  if (L'A' <= ucs2 && ucs2 <= L'Z') {
    return VirtualKey::FromVirtualKey(ucs2);
  }
  if (ucs2 == kTouchKeyboardNextPage) {
    return VirtualKey::FromVirtualKey(VK_NEXT);
  }
  if (ucs2 == kTouchKeyboardPreviousPage) {
    return VirtualKey::FromVirtualKey(VK_PRIOR);
  }

  // Emulate IME_PROP_ACCEPT_WIDE_VKEY.
  return VirtualKey::FromCombinedVirtualKey(ucs2 << 16 | VK_PACKET);
}

bool GetOpenAndMode(TipTextService *text_service, ITfContext *context,
                    bool *open, DWORD *mode) {
  DCHECK(text_service);
  DCHECK(context);
  DCHECK(open);
  DCHECK(mode);
  *open = (!TipStatus::IsDisabledContext(context) &&
           TipStatus::IsOpen(text_service->GetThreadManager()));
  return TipStatus::GetInputModeConversion(text_service->GetThreadManager(),
                                           text_service->GetClientID(), mode);
}

// Returns a bitmap of experimental features.
bool GetExperimentalFeatures(ITfContext *context, InputBehavior *behavior) {
  if (behavior == nullptr) {
    return false;
  }
  behavior->suppress_suggestion = false;
  behavior->experimental_features = InputBehavior::NO_FEATURE;
  CComPtr<ITfContextView> context_view;
  if (FAILED(context->GetActiveView(&context_view))) {
    return false;
  }
  if (context_view == nullptr) {
    return false;
  }
  HWND attached_window = nullptr;
  if (FAILED(context_view->GetWnd(&attached_window))) {
    return false;
  }
  if (WindowUtil::IsInChromeOmnibox(attached_window)) {
    behavior->suppress_suggestion = true;
    behavior->experimental_features |= InputBehavior::CHROME_OMNIBOX;
  }
  if (WindowUtil::IsInGoogleSearchBox(attached_window)) {
    behavior->suppress_suggestion = true;
    behavior->experimental_features |= InputBehavior::GOOGLE_SEARCH_BOX;
  }
  return true;
}

HRESULT OnTestKey(TipTextService *text_service, ITfContext *context,
                  bool is_key_down, WPARAM wparam, LPARAM lparam,
                  BOOL *eaten) {
  DCHECK(text_service);
  DCHECK(eaten);
  TipPrivateContext *private_context =
      text_service->GetPrivateContext(context);
  if (private_context == nullptr) {
    *eaten = FALSE;
    return S_OK;
  }

  BYTE key_state[256] = {};
  if (!::GetKeyboardState(key_state)) {
    *eaten = FALSE;
    return S_OK;
  }

  bool open = false;
  DWORD mode = 0;
  if (!GetOpenAndMode(text_service, context, &open, &mode)) {
    *eaten = FALSE;
    return S_OK;
  }

  const KeyboardStatus keyboard_status(key_state);
  const LParamKeyInfo key_info(lparam);
  VirtualKey vk = GetVK(wparam, keyboard_status);

  if (open) {
    // Check if this key event is handled by VKBackBasedDeleter to support
    // *deletion_range* rule.
    const VKBackBasedDeleter::ClientAction vk_back_action =
        private_context->GetDeleter()->OnKeyEvent(
            vk.virtual_key(), key_info.IsKeyDownInImeProcessKey(), true);
    switch (vk_back_action) {
      case VKBackBasedDeleter::DO_DEFAULT_ACTION:
        // do nothing.
        break;
      case VKBackBasedDeleter::CALL_END_DELETION_THEN_DO_DEFAULT_ACTION:
        private_context->GetDeleter()->EndDeletion();
        break;
      case VKBackBasedDeleter::SEND_KEY_TO_APPLICATION:
        *eaten = FALSE;  // Do not consume this key.
        return S_OK;
      case VKBackBasedDeleter::CONSUME_KEY_BUT_NEVER_SEND_TO_SERVER:
        *eaten = TRUE;  // Consume this key but do not send this key to server.
        return S_OK;
      case VKBackBasedDeleter::CALL_END_DELETION_BUT_NEVER_SEND_TO_SERVER:
      case VKBackBasedDeleter::APPLY_PENDING_STATUS:
      default:
        DLOG(FATAL) << "this action is not applicable to OnTestKey.";
        *eaten = FALSE;
        return E_UNEXPECTED;
    }

    const SurrogatePairObserver::ClientAction surrogate_action =
        private_context->GetSurrogatePairObserver()->OnTestKeyEvent(
            vk, is_key_down);
    switch (surrogate_action.type) {
      case SurrogatePairObserver::DO_DEFAULT_ACTION:
        break;
      case SurrogatePairObserver::DO_DEFAULT_ACTION_WITH_RETURNED_UCS4:
        vk = VirtualKey::FromUnicode(surrogate_action.ucs4);
        break;
      case SurrogatePairObserver::CONSUME_KEY_BUT_NEVER_SEND_TO_SERVER:
        *eaten = TRUE;
        return S_OK;  // Consume this key but do not send this key to server.
      default:
        DLOG(FATAL) << "this action is not applicable to OnTestKey.";
        break;
    }
  }

  InputState input_state = private_context->input_state();

  // Make an immutable snapshot of |private_context->ime_behavior_|, which
  // cannot be substituted for by const reference.
  InputBehavior behavior = private_context->input_behavior();
  GetExperimentalFeatures(context, &behavior);

  input_state.conversion_status = mode;
  input_state.open = open;
  InputState next_state;
  commands::Output temporal_output;
  scoped_ptr<Win32KeyboardInterface>
      keyboard(Win32KeyboardInterface::CreateDefault());

  const KeyEventHandlerResult result = KeyEventHandler::ImeProcessKey(
      vk, key_info.GetScanCode(), is_key_down, keyboard_status, behavior,
      input_state, private_context->GetClient(), keyboard.get(), &next_state,
      &temporal_output);
  if (!result.succeeded) {
    *eaten = FALSE;
    return S_OK;
  }

  *private_context->mutable_input_state() = next_state;

  if (result.should_be_sent_to_server && temporal_output.has_consumed()) {
    private_context->mutable_last_output()->CopyFrom(temporal_output);
  }

  *eaten = result.should_be_eaten ? TRUE : FALSE;
  return S_OK;
}

HRESULT OnKey(TipTextService *text_service, ITfContext *context,
              bool is_key_down, WPARAM wparam, LPARAM lparam, BOOL *eaten) {
  DCHECK(text_service);
  DCHECK(eaten);
  TipPrivateContext *private_context =
      text_service->GetPrivateContext(context);
  if (private_context == nullptr) {
    *eaten = FALSE;
    return S_OK;
  }

  BYTE key_state[256] = {};
  if (!::GetKeyboardState(key_state)) {
    *eaten = FALSE;
    return S_OK;
  }

  bool open = false;
  DWORD mode = 0;
  if (!GetOpenAndMode(text_service, context, &open, &mode)) {
    *eaten = FALSE;
    return S_OK;
  }

  const LParamKeyInfo key_info(lparam);
  const KeyboardStatus keyboard_status(key_state);
  VirtualKey vk = GetVK(wparam, keyboard_status);

  const VKBackBasedDeleter::ClientAction vk_back_action =
      private_context->GetDeleter()->OnKeyEvent(
          vk.virtual_key(), is_key_down, false);

  // Check if this key event is handled by VKBackBasedDeleter to support
  // *deletion_range* rule.
  bool use_pending_output = false;
  bool ignore_this_keyevent = false;
  if (open) {
    switch (vk_back_action) {
      case VKBackBasedDeleter::DO_DEFAULT_ACTION:
        // do nothing.
        break;
      case VKBackBasedDeleter::CALL_END_DELETION_THEN_DO_DEFAULT_ACTION:
        private_context->GetDeleter()->EndDeletion();
        break;
      case VKBackBasedDeleter::APPLY_PENDING_STATUS:
        use_pending_output = true;
        break;
      case VKBackBasedDeleter::CONSUME_KEY_BUT_NEVER_SEND_TO_SERVER:
        ignore_this_keyevent = true;
        break;
      case VKBackBasedDeleter::CALL_END_DELETION_BUT_NEVER_SEND_TO_SERVER:
        ignore_this_keyevent = true;
        private_context->GetDeleter()->EndDeletion();
        break;
      case VKBackBasedDeleter::SEND_KEY_TO_APPLICATION:
      default:
        DLOG(FATAL) << "this action is not applicable to OnKey.";
        break;
    }
    if (ignore_this_keyevent) {
      *eaten = TRUE;
      return S_OK;
    }

    const SurrogatePairObserver::ClientAction surrogate_action =
        private_context->GetSurrogatePairObserver()->OnKeyEvent(
            vk, is_key_down);
    switch (surrogate_action.type) {
      case SurrogatePairObserver::DO_DEFAULT_ACTION:
        break;
      case SurrogatePairObserver::DO_DEFAULT_ACTION_WITH_RETURNED_UCS4:
        vk = VirtualKey::FromUnicode(surrogate_action.ucs4);
      break;
      case SurrogatePairObserver::CONSUME_KEY_BUT_NEVER_SEND_TO_SERVER:
        ignore_this_keyevent = true;
        break;
      default:
        DLOG(FATAL) << "this action is not applicable to OnKey.";
        ignore_this_keyevent = true;
        break;
    }

    if (ignore_this_keyevent) {
      *eaten = TRUE;
      return S_OK;
    }
  }

  commands::Output temporal_output;
  if (use_pending_output) {
    // In this case, we have a pending output. So no need to call
    // KeyEventHandler::ImeToAsciiEx.
    temporal_output.CopyFrom(
        private_context->GetDeleter()->pending_output());
  } else {
    InputBehavior behavior = private_context->input_behavior();
    GetExperimentalFeatures(context, &behavior);

    InputState ime_state = private_context->input_state();
    ime_state.conversion_status = mode;
    ime_state.open = open;

    // This call is placed in OnKey instead on OnTestKey because VK_DBE_ROMAN
    // and VK_DBE_NOROMAN are handled as preserved keys in TSF Mozc.
    // See b/3118905 for why this is necessary.
    KeyEventHandler::UpdateBehaviorInImeProcessKey(
        vk, is_key_down, ime_state, private_context->mutable_input_behavior());

    scoped_ptr<Win32KeyboardInterface>
        keyboard(Win32KeyboardInterface::CreateDefault());

    InputState unused_next_state;
    const KeyEventHandlerResult result = KeyEventHandler::ImeToAsciiEx(
        vk, key_info.GetScanCode(), is_key_down, keyboard_status, behavior,
        ime_state, private_context->GetClient(), keyboard.get(),
        &unused_next_state, &temporal_output);

    if (!result.succeeded) {
      // no message generated.
      *eaten = FALSE;
      return S_OK;
    }

    if (!result.should_be_sent_to_server) {
      // no message generated.
      *eaten = FALSE;
      return S_OK;
    }

    ignore_this_keyevent = !result.should_be_eaten;
  }

  TipCommandHandler::OnCommandReceived(text_service, context, temporal_output);
  *eaten = !ignore_this_keyevent ? TRUE : FALSE;

  return S_OK;
}

const bool kKeyDown = true;
const bool kKeyUp = false;

}  // namespace

HRESULT TipKeyeventHandler::OnTestKeyDown(
    TipTextService *text_service, ITfContext *context,
    WPARAM wparam, LPARAM lparam, BOOL *eaten) {
  return OnTestKey(text_service, context, kKeyDown, wparam, lparam, eaten);
}

HRESULT TipKeyeventHandler::OnTestKeyUp(
    TipTextService *text_service, ITfContext *context, WPARAM wparam,
    LPARAM lparam, BOOL *eaten) {
  return OnTestKey(text_service, context, kKeyUp, wparam, lparam, eaten);
}

HRESULT TipKeyeventHandler::OnKeyDown(
    TipTextService *text_service, ITfContext *context, WPARAM wparam,
    LPARAM lparam, BOOL *eaten) {
  return OnKey(text_service, context, kKeyDown, wparam, lparam, eaten);
}

HRESULT TipKeyeventHandler::OnKeyUp(
    TipTextService *text_service, ITfContext *context, WPARAM wparam,
    LPARAM lparam, BOOL *eaten) {
  return OnKey(text_service, context, kKeyUp, wparam, lparam, eaten);
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc
