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

// An implementation of the IMM32 interface.
//

#include "win32/ime/ime_impl_imm.h"

#include <ime.h>
#include <strsafe.h>

#include <vector>

#include "google/protobuf/stubs/common.h"
#include "base/const.h"
#include "base/crash_report_handler.h"
#include "base/process.h"
#include "base/singleton.h"
#include "base/update_util.h"
#include "base/util.h"
#include "config/stats_config_util.h"
#include "languages/global_language_spec.h"
#include "languages/japanese/lang_dep_spec.h"
#include "win32/base/conversion_mode_util.h"
#include "win32/base/immdev.h"
#include "win32/base/string_util.h"
#include "win32/ime/ime_candidate_info.h"
#include "win32/ime/ime_composition_string.h"
#include "win32/ime/ime_core.h"
#include "win32/ime/ime_deleter.h"
#include "win32/ime/ime_input_context.h"
#include "win32/ime/ime_message_queue.h"
#include "win32/ime/ime_mouse_tracker.h"
#include "win32/ime/ime_private_context.h"
#include "win32/ime/ime_scoped_context.h"
#include "win32/ime/ime_surrogate_pair_observer.h"
#include "win32/ime/ime_trace.h"
#include "win32/ime/ime_ui_context.h"
#include "win32/ime/ime_ui_visibility_tracker.h"
#include "win32/ime/ime_ui_window.h"
#include "win32/ime/output_util.h"

namespace {
HINSTANCE g_instance = NULL;
DWORD g_ime_system_info = MAXDWORD;
mozc::language::LanguageDependentSpecInterface *g_lang_spec_ptr = NULL;
// True if the boot mode is safe mode.
bool g_in_safe_mode = true;
// True when Util::EnsureVitalImmutableDataIsAvailable() returns false.
bool g_fundamental_data_is_not_available = false;

// Breakpad never be enabled except for official builds.
#if defined(GOOGLE_JAPANESE_INPUT_BUILD)
#define USE_BREAKPAD
#endif  // GOOGLE_JAPANESE_INPUT_BUILD

#if defined(USE_BREAKPAD)
// A critical section object for breakpad because its initialization routine is
// not thread-safe.  See b/3100365 why we need this.
CRITICAL_SECTION g_critical_section_for_breakpad;
// 4,000 is a typical value of per-heap critical sections.
// See http://msdn.microsoft.com/en-us/library/ms683476.aspx
const int kSpinCountForCriticalSection = 4000;
#endif  // USE_BREAKPAD

// True if the DLL received DLL_PROCESS_DETACH notification as a result of
// process shutting down.  If this bit is true, you must not call any
// function exported from other DLLs nor function implemented by CRT because
// they might also have been uninitialized.
bool g_process_is_shutting_down = false;

// Some troublesome DLLs such as msctf.dll on XP may violate the rule that
// a DLL should not call functions exported from other DLLs except for
// kernel32.dll in DllMain.  As a result, function in other DLLs including
// this DLL might be called even after it received DLL_PROCESS_DETACH
// notification.  You are likely to notice this issue expecially on
// CUAS-enabled XP, as filed in b/3088049.
// This macro can be used to mitigate this scenario.
#define DANGLING_CALLBACK_GUARD(return_code)   \
  do {                                         \
    if (g_process_is_shutting_down) {          \
      return (return_code);                    \
    }                                          \
  } while (false)

COMPILE_ASSERT(arraysize(mozc::kIMEUIWndClassName)
               <= mozc::kIMEUIwndClassNameLimitInTchars,
               UIWnd_class_name_is_limited);

// Maximum number of characters for |REGISTERWORD::lpWord| and
// |REGISTERWORD::lpReading|.
const size_t kMaxCharsForRegisterWord = 64;

// If given string is too long, returns an empty string instead.
wstring GetStringIfWithinLimit(const wchar_t *src, size_t size_limit) {
  if (src == NULL) {
    return wstring();
  }
  for (size_t i = 0; i < size_limit; ++i) {
    if (src[i] == L'\0') {
      return wstring(src, i);
    }
  }
  return wstring();
}

void SetEnveronmentVariablesForWordRegisterDialog(
    const wstring &word_value, const wstring &word_reading) {
  wstring word_value_env_name;
  wstring word_reading_env_name;
  mozc::Util::UTF8ToWide(
      mozc::kWordRegisterEnvironmentName, &word_value_env_name);
  mozc::Util::UTF8ToWide(
      mozc::kWordRegisterEnvironmentReadingName, &word_reading_env_name);

  if (word_value.empty()) {
    // Remove relevant variables.
    ::SetEnvironmentVariable(word_value_env_name.c_str(), NULL);
    ::SetEnvironmentVariable(word_reading_env_name.c_str(), NULL);
  }

  ::SetEnvironmentVariable(word_value_env_name.c_str(),
                           word_value.c_str());
  if (word_reading.empty()) {
    // Remove relevant variable.
    ::SetEnvironmentVariable(word_reading_env_name.c_str(),
                             NULL);
  } else {
    ::SetEnvironmentVariable(word_reading_env_name.c_str(),
                             word_reading.c_str());
  }
}

static HIMCC InitializeHIMCC(HIMCC himcc, DWORD size) {
  if (himcc == NULL) {
    return ::ImmCreateIMCC(size);
  } else {
    return ::ImmReSizeIMCC(himcc, size);
  }
}

}  // namespace

HINSTANCE ImeGetResource() {
  return g_instance;
}

namespace mozc {
namespace win32 {
bool IsInLockdownMode() {
  if (g_in_safe_mode) {
    return true;
  }
  if ((g_ime_system_info & IME_SYSINFO_WINLOGON) == IME_SYSINFO_WINLOGON) {
    return true;
  }
  if (g_fundamental_data_is_not_available) {
    return true;
  }
  return false;
}

BOOL OnDllProcessAttach(HINSTANCE instance, bool static_loading) {
  g_instance = instance;
#if defined(USE_BREAKPAD)
  if(!::InitializeCriticalSectionAndSpinCount(&g_critical_section_for_breakpad,
                                              kSpinCountForCriticalSection)) {
    return FALSE;
  }
  mozc::CrashReportHandler::SetCriticalSection(
      &g_critical_section_for_breakpad);
#endif  // USE_BREAKPAD

  g_lang_spec_ptr = new mozc::japanese::LangDepSpecJapanese();
  mozc::language::GlobalLanguageSpec::SetLanguageDependentSpec(g_lang_spec_ptr);

  ATOM atom = INVALID_ATOM;
  if (!UIWindowManager::OnDllProcessAttach(instance, static_loading)) {
    return FALSE;
  }

  ThreadLocalMouseTracker::OnDllProcessAttach(instance, static_loading);
  return TRUE;
}

BOOL OnDllProcessDetach(HINSTANCE instance, bool process_shutdown) {
  ThreadLocalMouseTracker::OnDllProcessDetach(instance, process_shutdown);
  UIWindowManager::OnDllProcessDetach(instance, process_shutdown);

  g_instance = NULL;

  if (!g_in_safe_mode) {
    // It is our responsibility to make sure that our code never touch protobuf
    // library after google::protobuf::ShutdownProtobufLibrary is called.
    // Unfortunately, DllMain is the only place that satisfies this condition.
    // So we carefully call it here, even though DllMain is expected to be
    // dangerous for potential deadlocks.  See b/2126375 for details.
    google::protobuf::ShutdownProtobufLibrary();
  }

  if (process_shutdown) {
    g_process_is_shutting_down |= true;
  }

#if defined(USE_BREAKPAD)
  mozc::CrashReportHandler::SetCriticalSection(NULL);
  ::DeleteCriticalSection(&g_critical_section_for_breakpad);
#endif  // USE_BREAKPAD

  mozc::language::GlobalLanguageSpec::SetLanguageDependentSpec(NULL);
  delete g_lang_spec_ptr;
  g_lang_spec_ptr = NULL;
  return TRUE;
}
}  // namespace win32
}  // namespace mozc

BOOL WINAPI ImeInquire(LPIMEINFO ime_info,
                       LPTSTR class_name,
                       DWORD system_info_flags) {
  FUNCTION_ENTER();

  // Cache the boot mode here so that we need not call user32.dll functions
  // from DllMain.  If it is safe mode, we omit some initializations/
  // uninitializations to reduce potential crashes around them. (b/2728123)
  // 0: Normal boot
  // 1: Fail-safe boot
  // 2: Fail-safe with network boot
  g_in_safe_mode = (::GetSystemMetrics(SM_CLEANBOOT) > 0);
  if (g_in_safe_mode) {
    // Fail immediately in safe mode
    return FALSE;
  }

  ::ZeroMemory(ime_info, sizeof(IMEINFO));

  // Although |IME_PROP_NO_KEYS_ON_CLOSE| might be benefitial from performance
  // perspective, we actually have to check all key events, even when the IME
  // is turned off, to allow users to use an arbitrary key combination to turn
  // on IME.
  ime_info->fdwProperty = 0
      | IME_PROP_END_UNLOAD
      | IME_PROP_KBD_CHAR_FIRST
      | IME_PROP_ACCEPT_WIDE_VKEY
      | IME_PROP_AT_CARET
      | IME_PROP_NEED_ALTKEY
      | IME_PROP_CANDLIST_START_FROM_1
      | IME_PROP_UNICODE;

#if !defined(UNICODE)
  // Actually, we have never tested on non-unicode build.
  COMPILE_ASSERT(false, non_unicode_build_has_never_been_tested);
#endif  // !UNICODE

  ime_info->fdwConversionCaps = IME_CMODE_LANGUAGE
                              | IME_CMODE_KATAKANA
                              | IME_CMODE_FULLSHAPE
                              | IME_CMODE_ROMAN;

  // Currently, only IME_SMODE_PHRASEPREDICT is supported.
  // See b/2913510, b/2954777, and b/2955175 for details.
  ime_info->fdwSentenceCaps = IME_SMODE_PHRASEPREDICT;

  ime_info->fdwUICaps = UI_CAP_2700;

  ime_info->fdwSCSCaps = SCS_CAP_MAKEREAD
                       | SCS_CAP_SETRECONVERTSTRING;

  ime_info->fdwSelectCaps = SELECT_CAP_CONVERSION
                          | SELECT_CAP_SENTENCE;

  const PTSTR strcpy_result =
      lstrcpyn(class_name, mozc::kIMEUIWndClassName,
               mozc::kIMEUIwndClassNameLimitInTchars);

  if (!strcpy_result) {
    FUNCTION_TRACE(L"lstrcpyn failed");
    return FALSE;
  }

  g_ime_system_info = system_info_flags;
  if (!mozc::Util::EnsureVitalImmutableDataIsAvailable()) {
    // This process might be sandboxed.
    g_fundamental_data_is_not_available = true;
  }

  if (!mozc::win32::IsInLockdownMode()) {
#if defined(USE_BREAKPAD)
    if (mozc::StatsConfigUtil::IsEnabled()) {
      mozc::CrashReportHandler::Initialize(true);
    }
#endif  // USE_BREAKPAD
  }

  return TRUE;
}

DWORD WINAPI ImeConversionList(HIMC himc,
                               LPCTSTR source,
                               LPCANDIDATELIST candidate_list,
                               DWORD buffer_length,
                               UINT flags) {
  DANGLING_CALLBACK_GUARD(FALSE);
  FUNCTION_ENTER();

  return 0;
}

BOOL WINAPI ImeDestroy(UINT force) {
  DANGLING_CALLBACK_GUARD(FALSE);
  FUNCTION_ENTER();
  // As commented above, ImeDestroy is not so reliable place to free global
  // resources allocated in this DLL.
  // TODO(yukawa): Find better place.

  // Free all singleton instances
  mozc::SingletonFinalizer::Finalize();

  // We deliberately call google::protobuf::ShutdownProtobufLibrary from
  // OnDllProcessDetach rather than here.  See b/2126375 for details.

#if defined(USE_BREAKPAD)
  if (mozc::CrashReportHandler::IsInitialized()) {
    // Uninitialize the breakpad.
    mozc::CrashReportHandler::Uninitialize();
  }
#endif  // !USE_BREAKPAD

  return TRUE;
}

LRESULT WINAPI ImeEscape(HIMC himc, UINT sub_func, LPVOID data) {
  DANGLING_CALLBACK_GUARD(FALSE);
  FUNCTION_ENTER();
  switch (sub_func) {
    case IME_ESC_IME_NAME: {
      // Application wants to retrieve the name of the IME.
      // Currently, we returns english name.
      // According to the document, the buffer is guaranteed to be greater
      // than or equal to 64 characters in Windows NT.
      // http://msdn.microsoft.com/en-us/library/dd318166.aspx
      wstring name;
      mozc::Util::UTF8ToWide(mozc::kProductNameInEnglish, &name);
      wchar_t *dest = static_cast<wchar_t *>(data);
      const HRESULT result = ::StringCchCopyN(
          dest,
          mozc::kSafeIMENameLengthForNTInTchars,
          name.c_str(),
          mozc::kSafeIMENameLengthForNTInTchars);
      return (SUCCEEDED(result) ? TRUE : FALSE);
    }
    default:
      // not implemented
      return FALSE;
  }
  return FALSE;
}

BOOL WINAPI ImeSetActiveContext(HIMC himc, BOOL flag) {
  DANGLING_CALLBACK_GUARD(FALSE);
  FUNCTION_ENTER();

  // Clear kana-lock state so that users can input their passwords.
  // TODO(yukawa): Move this code to somewhere appropriate.
  BYTE keyboard_state[256];
  ::GetKeyboardState(keyboard_state);
  keyboard_state[VK_KANA] = 0;
  ::SetKeyboardState(keyboard_state);

  // Occasionally this function is called with NULL in |himc|.
  if (himc == NULL) {
    return TRUE;
  }

  const bool activated = (flag != FALSE);

  // A temporal workaround for b/3046497.  Occasionally ImmLockIMC fails
  // to lock the context.
  // TODO(yukawa): Refactor HIMCLockerT to support this scenario.
  {
    INPUTCONTEXT *context = ::ImmLockIMC(himc);
    if (context == NULL) {
      return FALSE;
    }

    // Clear |INPUTCONTEXT::hWnd| so that IMM subsystem will not generate UI
    // messages when a deactivated input contexts is specified.
    // Background:
    //   It seems that IMM does not clear |INPUTCONTEXT::hWnd| by default when
    //   an input context is about being deactivated.  However, an application
    //   still can access to a deactivated input context via IMM APIs such as
    //   ImmSetOpenStatus and IMM subsystem generates UI messages as long as
    //   the |INPUTCONTEXT::hWnd| contains a valid window handle.
    if (!activated) {
      context->hWnd = NULL;
    }
    ::ImmUnlockIMC(himc);
  }

  mozc::win32::ScopedHIMC<mozc::win32::InputContext> context(himc);
  if (!mozc::win32::PrivateContextUtil::IsValidPrivateContext(
           context->hPrivate)) {
    return FALSE;
  }
  mozc::win32::ScopedHIMCC<mozc::win32::PrivateContext>
      private_context(context->hPrivate);

  mozc::win32::MessageQueue message_queue(himc);
  if (activated) {
    private_context->ui_visibility_tracker->OnFocus();
  } else {
    private_context->ui_visibility_tracker->OnBlur();
  }
  message_queue.AddMessage(
      WM_IME_NOTIFY, IMN_PRIVATE, mozc::win32::kNotifyUpdateUI);

  return (message_queue.Send() ? TRUE : FALSE);
}

BOOL WINAPI ImeProcessKey(HIMC himc,
                          UINT virtual_key,
                          LPARAM lParam,
                          CONST LPBYTE key_state) {
  DANGLING_CALLBACK_GUARD(FALSE);
  FUNCTION_ENTER();
  if (!mozc::win32::ImeCore::IsInputContextInitialized(himc)) {
    return FALSE;
  }

  mozc::win32::ScopedHIMC<mozc::win32::InputContext> context(himc);
  mozc::win32::ScopedHIMCC<mozc::win32::PrivateContext>
      private_context(context->hPrivate);

  // Because of IME_PROP_ACCEPT_WIDE_VKEY, |HIWORD(virtual_key)| contains an
  // Unicode character if |HIWORD(virtual_key) == VK_PACKET|.
  // You cannot assume that |virtual_key| is in [0, 255].
  mozc::win32::VirtualKey vk =
      mozc::win32::VirtualKey::FromCombinedVirtualKey(virtual_key);

  const mozc::win32::KeyboardStatus keyboard_status(key_state);
  const mozc::win32::LParamKeyInfo key_info(lParam);

  // Check if this key event is handled by VKBackBasedDeleter to support
  // *deletion_range* rule.
  const mozc::win32::VKBackBasedDeleter::ClientAction vk_back_action =
      private_context->deleter->OnKeyEvent(
          vk.virtual_key(), key_info.IsKeyDownInImeProcessKey(), true);
  switch (vk_back_action) {
    case mozc::win32::VKBackBasedDeleter::DO_DEFAULT_ACTION:
      // do nothing.
      break;
    case mozc::win32::VKBackBasedDeleter::
             CALL_END_DELETION_THEN_DO_DEFAULT_ACTION:
      private_context->deleter->EndDeletion();
      break;
    case mozc::win32::VKBackBasedDeleter::SEND_KEY_TO_APPLICATION:
      return FALSE;  // Do not consume this key.
    case mozc::win32::VKBackBasedDeleter::CONSUME_KEY_BUT_NEVER_SEND_TO_SERVER:
      return TRUE;  // Consume this key but do not send this key to server.
    case mozc::win32::VKBackBasedDeleter::
             CALL_END_DELETION_BUT_NEVER_SEND_TO_SERVER:
    case mozc::win32::VKBackBasedDeleter::APPLY_PENDING_STATUS:
    default:
      DLOG(FATAL) << "this action is not applicable to ImeProcessKey.";
      break;
  }

  if (context->fOpen) {
    const mozc::win32::SurrogatePairObserver::ClientAction surrogate_action =
        private_context->surrogate_pair_observer->OnTestKeyEvent(
            vk, key_info.IsKeyDownInImeProcessKey());
    switch (surrogate_action.type) {
      case mozc::win32::SurrogatePairObserver::DO_DEFAULT_ACTION:
        break;
      case mozc::win32::SurrogatePairObserver::
          DO_DEFAULT_ACTION_WITH_RETURNED_UCS4:
        vk = mozc::win32::VirtualKey::FromUnicode(surrogate_action.ucs4);
        break;
      case mozc::win32::SurrogatePairObserver::
          CONSUME_KEY_BUT_NEVER_SEND_TO_SERVER:
        return TRUE;  // Consume this key but do not send this key to server.
      default:
        DLOG(FATAL) << "this action is not applicable to ImeProcessKey.";
        break;
    }
  }

  mozc::win32::ImeState ime_state = *private_context->ime_state;

  // Update |private_context->ime_behavior| to support Kana/Roman input toggle
  // keys.  See b/3118905 for details.
  mozc::win32::KeyEventHandler::UpdateBehaviorInImeProcessKey(
      vk, key_info, ime_state, private_context->ime_behavior);

  // Make an immutable snapshot of |private_context->ime_behavior|, which
  // cannot be substituted for by const reference.
  const mozc::win32::ImeBehavior behavior = *private_context->ime_behavior;

  ime_state.conversion_status = context->fdwConversion;
  ime_state.open = context->fOpen;
  mozc::win32::ImeState next_state;
  mozc::commands::Output temporal_output;
  const mozc::win32::KeyEventHandlerResult result =
      mozc::win32::ImeCore::ImeProcessKey(private_context->client,
          vk, key_info, keyboard_status, behavior, ime_state, &next_state,
          &temporal_output);
  if (!result.succeeded) {
    return FALSE;
  }

  *private_context->ime_state = next_state;

  if (result.should_be_sent_to_server && temporal_output.has_consumed()) {
    private_context->last_output->CopyFrom(temporal_output);
  }

  // Mozc server sometimes assumes that characters left side of the caret have
  // not been changed since they were input by Mozc itself. However, this
  // assumption is not always true. For example, the caret can be moved by the
  // mouse event. This kind of context change information can help Mozc server
  // to invalidate internal buffers. See b/3282221 for details.
  if (result.should_be_eaten) {
    mozc::win32::ImeCore::ResetServerContextIfNeccesary(himc);
  }

  return result.should_be_eaten ? TRUE : FALSE;
}

// TODO(yukawa): Refactor the implemenation.
BOOL WINAPI NotifyIME(HIMC himc, DWORD action, DWORD index, DWORD value) {
  DANGLING_CALLBACK_GUARD(FALSE);
  FUNCTION_ENTER();
  if (!mozc::win32::ImeCore::IsInputContextInitialized(himc)) {
    return FALSE;
  }

  const bool generate_message = mozc::win32::ImeCore::IsActiveContext(himc);

  switch (action) {
    case NI_CLOSECANDIDATE: {
      const DWORD candidate_window_index = index;
      if (candidate_window_index != 0) {
        return FALSE;
      }
      return mozc::win32::ImeCore::CloseCandidate(himc, generate_message);
    }
    case NI_SELECTCANDIDATESTR: {
      const DWORD candidate_window_index = index;
      if (candidate_window_index != 0) {
        return FALSE;
      }
      const int32 candidate_index = static_cast<int32>(value);
      return mozc::win32::ImeCore::HighlightCandidate(
          himc, candidate_index, generate_message);
    }
    case NI_OPENCANDIDATE:
    case NI_CHANGECANDIDATELIST:
    case NI_FINALIZECONVERSIONRESULT:
    case NI_SETCANDIDATE_PAGESTART:
    case NI_SETCANDIDATE_PAGESIZE:
    case NI_IMEMENUSELECTED:
      // not implemented
      // TODO(yukawa): implement them.
      return FALSE;
  }

  if (action == NI_COMPOSITIONSTR) {
    mozc::win32::UIContext context(himc);
    if (context.IsCompositionStringEmpty()) {
      return TRUE;
    }

    switch (index) {
      case CPS_COMPLETE: {
        if (!mozc::win32::ImeCore::SubmitComposition(
                himc, generate_message)) {
          return FALSE;
        }
        return TRUE;
      }
      case CPS_CANCEL: {
        if (!mozc::win32::ImeCore::CancelComposition(
                himc, generate_message)) {
          return FALSE;
        }
        return TRUE;
      }
      case CPS_REVERT:
      case CPS_CONVERT:
        // not implemented
        // TODO(yukawa): implement them.
        return FALSE;
      default:
        return FALSE;
    }
    // never reaches here.
    CHECK(FALSE);
  }

  if (action != NI_CONTEXTUPDATED) {
    return FALSE;
  }

  CHECK_EQ(NI_CONTEXTUPDATED, action);

  switch (value) {
    case IMC_SETCONVERSIONMODE: {
      mozc::win32::ScopedHIMC<mozc::win32::InputContext> context(himc);
      context->fdwConversion =
          mozc::win32::ImeCore::GetSupportableConversionMode(
              context->fdwConversion);
      // We need not to generate WM_IME_NOTIFY/IMN_SETSENTENCEMODE because
      // ImmSetOpenStatus API generates it anyway.
      return TRUE;
    }
    case IMC_SETSENTENCEMODE: {
      // See b/2913510, b/2954777, and b/2955175 for details.
      mozc::win32::ScopedHIMC<mozc::win32::InputContext> context(himc);
      // We need not to generate WM_IME_NOTIFY/IMN_SETSENTENCEMODE because
      // ImmSetOpenStatus API generates it anyway.
      context->fdwSentence =
          mozc::win32::ImeCore::GetSupportableSentenceMode(
              context->fdwSentence);
      return TRUE;
    }
    case IMC_SETOPENSTATUS: {
      mozc::win32::UIContext context(himc);
      if (!context.GetOpenStatus()) {
        // If this is an active context, we have to generate message because
        // ImmSetOpenStatus API is responsible for generating only
        // a WM_IME_NOTIFY(IMC_SETOPENSTATUS) message.  Any other UI messages
        // including composition messages should be delivered when an on-going
        // composition is terminated.  See b/3186132 for details.
        mozc::win32::ImeCore::IMEOff(himc, generate_message);
        return TRUE;
      }

      DCHECK(context.GetOpenStatus());
      DWORD mode = 0;
      if (!context.GetConversionMode(&mode)) {
        return FALSE;
      }
      // This is OK because ImeCore::OpenIME generates no UI messages.
      mozc::win32::ImeCore::OpenIME(context.client(), mode);
      // We need not to generate WM_IME_NOTIFY/IMN_SETOPENSTATUS because
      // ImmSetOpenStatus API generates it anyway.
      return TRUE;
    }
    case IMC_SETCANDIDATEPOS:
    case IMC_SETCOMPOSITIONFONT:
    case IMC_SETCOMPOSITIONWINDOW:
      // We need not to generate corresponding UI messages because Imm API
      // generate it anyway.
      return TRUE;
    default:
      return FALSE;
  }
  return FALSE;
}

// We need not to generate any UI message in this callback.
// UI window is responsible for updating its UI when it receives
// WM_IME_SETCONTEXT.
BOOL WINAPI ImeSelect(HIMC himc, BOOL select) {
  DANGLING_CALLBACK_GUARD(FALSE);
  FUNCTION_ENTER();

  // Clear kana-lock state so that users can input their passwords.
  // TODO(yukawa): Move this code to somewhere appropriate.
  BYTE keyboard_state[256];
  ::GetKeyboardState(keyboard_state);
  keyboard_state[VK_KANA] = 0;
  ::SetKeyboardState(keyboard_state);

  if (himc == NULL) {
    return TRUE;
  }

  // In "lockdown" mode, it would be definitely better do nothing in our DLL.
  // For example, lots of fundamental stop working in a sandboxed process as
  // reported in b/3216603.  In such a situation, remaining CHECK macro is
  // likely to cause process crash.
  if (mozc::win32::IsInLockdownMode()) {
    FUNCTION_TRACE(L"Lockdown Mode");
    return FALSE;
  }

  if (select == FALSE) {
    // If there exist any on-going composition, IMM32 automatically calls
    // NotifyIME with CPS_CANCEL or CPS_COMPLETE in advance based on
    // IME_PROP_COMPLETE_ON_UNSELECT property.
    // You need not to submit nor cancel composition here.

    // Clean up resources in PrivateContext.
    mozc::win32::ScopedHIMC<INPUTCONTEXT> context(himc);
    if (mozc::win32::PrivateContextUtil::IsValidPrivateContext(
            context->hPrivate)) {
      mozc::win32::ScopedHIMCC<mozc::win32::PrivateContext>
          private_context(context->hPrivate);
      private_context->Uninitialize();
    }
    return TRUE;
  }

  // Unfortunately, InitLogStream cannot be placed inside DllMain because it
  // may internally call LoadSystemLibrary to retrieve user profile directory.
  // We should definitely avoid using LoadSystemLibrary when the thread owns
  // loader lock.
  mozc::Logging::InitLogStream(kProductPrefix "_imm32_ui");

  mozc::win32::ScopedHIMC<mozc::win32::InputContext> context(himc);
  if (context.get() == NULL) {
    return FALSE;
  }

  if (!context->Initialize()) {
    return FALSE;
  }

  // If the private area of the input context is not initialized,
  // allocate the new region in which new client management object is
  // stored.
  if (!mozc::win32::PrivateContextUtil::EnsurePrivateContextIsInitialized(
           &context->hPrivate)) {
    return FALSE;
  }

  mozc::win32::ScopedHIMCC<mozc::win32::PrivateContext>
      private_context(context->hPrivate);
  DCHECK(private_context->Validate());

  // Allocate composition string buffer.
  const HIMCC composition_string_handle = InitializeHIMCC(
      context->hCompStr, sizeof(mozc::win32::CompositionString));
  if (composition_string_handle == NULL) {
    return FALSE;
  }
  mozc::win32::ScopedHIMCC<mozc::win32::CompositionString>
      composition_string(composition_string_handle);
  if (!composition_string->Initialize()) {
    return FALSE;
  }

  context->hCandInfo =
      mozc::win32::CandidateInfoUtil::Initialize(context->hCandInfo);
  if (context->hCandInfo == NULL) {
    return FALSE;
  }

  context->fdwConversion = mozc::win32::ImeCore::GetSupportableConversionMode(
      context->fdwConversion);

  // When this is an active context, notify it because ImeSetActiveContext will
  // not be called when IME is changed.
  if (mozc::win32::ImeCore::IsActiveContext(himc)) {
    private_context->ui_visibility_tracker->OnFocus();
  }

  // Send the local status to the server when IME is ON.
  if (context->fOpen) {
    if ((context->fdwInit & INIT_CONVERSION) != INIT_CONVERSION) {
      return FALSE;
    }
    // This is OK because ImeCore::OpenIME does not generate any UI message.
    if (!mozc::win32::ImeCore::OpenIME(
             private_context->client, context->fdwConversion)) {
      return FALSE;
    }
  }

  // Write a registry value for usage tracking by Omaha.
  // We ignore the returned value by the function because we should not disturb
  // the application by the result of this function.
  if (!mozc::UpdateUtil::WriteActiveUsageInfo()) {
    LOG(WARNING) << "WriteActiveUsageInfo failed";
  }

  return TRUE;
}

BOOL WINAPI ImeSetCompositionString(HIMC himc,
                                    DWORD index,
                                    LPVOID comp,
                                    DWORD comp_length,
                                    LPVOID read,
                                    DWORD read_length) {
  DANGLING_CALLBACK_GUARD(FALSE);
  FUNCTION_ENTER();

  if (index == SCS_QUERYRECONVERTSTRING) {
    // In this case, IMEs are supposed to update |composition_info| and/or
    // |reading_info| if neccesary.
    RECONVERTSTRING *composition_info =
        reinterpret_cast<RECONVERTSTRING *>(comp);
    RECONVERTSTRING *reading_info =
        reinterpret_cast<RECONVERTSTRING *>(read);
    return mozc::win32::ImeCore::QueryReconversionFromApplication(
        himc, composition_info, reading_info);
  } else if (index == SCS_SETRECONVERTSTRING) {
    // In this case, IMEs must not update |composition_info| nor
    // |reading_info|.  This is why they are treated as const pointer.
    const RECONVERTSTRING *composition_info =
        reinterpret_cast<const RECONVERTSTRING *>(comp);
    const RECONVERTSTRING *reading_info =
        reinterpret_cast<const RECONVERTSTRING *>(read);
    return mozc::win32::ImeCore::ReconversionFromApplication(
        himc, composition_info, reading_info);
  }

  return FALSE;
}

DWORD WINAPI ImeGetImeMenuItems(HIMC himc,
                                DWORD flags,
                                DWORD type,
                                LPIMEMENUITEMINFOW ime_parent_menu,
                                LPIMEMENUITEMINFOW ime_menu,
                                DWORD size) {
  DANGLING_CALLBACK_GUARD(FALSE);
  FUNCTION_ENTER();

  return 0;
}

// TODO(yukawa): Refactor the implemenation.
UINT WINAPI ImeToAsciiEx(UINT virtual_key,
                         UINT scan_code,
                         CONST LPBYTE key_state,
                         LPTRANSMSGLIST trans_buf,
                         UINT state,
                         HIMC himc) {
  // If fails, no message generated.
  DANGLING_CALLBACK_GUARD(0);
  FUNCTION_ENTER();
  if (!mozc::win32::ImeCore::IsInputContextInitialized(himc)) {
    // no message generated.
    return 0;
  }

  mozc::win32::ScopedHIMC<mozc::win32::InputContext> context(himc);
  mozc::win32::ScopedHIMCC<mozc::win32::PrivateContext>
      private_context(context->hPrivate);

  mozc::win32::VirtualKey vk =
      mozc::win32::VirtualKey::FromCombinedVirtualKey(virtual_key);
  const mozc::win32::KeyboardStatus keyboard_status(key_state);
  const mozc::win32::ImeBehavior behavior = *private_context->ime_behavior;
  mozc::win32::ImeState ime_state = *private_context->ime_state;
  ime_state.conversion_status = context->fdwConversion;
  ime_state.open = context->fOpen;
  mozc::win32::ImeState next_state;
  const BYTE raw_scan_code = static_cast<BYTE>(scan_code & 0xff);
  const bool is_key_down = ((scan_code & 0x8000) == 0);
  mozc::commands::Output temporal_output;

  const mozc::win32::VKBackBasedDeleter::ClientAction vk_back_action =
      private_context->deleter->OnKeyEvent(
          vk.virtual_key(), is_key_down, false);

  // Check if this key event is handled by VKBackBasedDeleter to support
  // *deletion_range* rule.
  bool use_pending_status = false;
  bool ignore_this_keyevent = false;
  switch (vk_back_action) {
    case mozc::win32::VKBackBasedDeleter::DO_DEFAULT_ACTION:
      // do nothing.
      break;
    case mozc::win32::VKBackBasedDeleter::
             CALL_END_DELETION_THEN_DO_DEFAULT_ACTION:
      private_context->deleter->EndDeletion();
      break;
    case mozc::win32::VKBackBasedDeleter::APPLY_PENDING_STATUS:
      use_pending_status = true;
      break;
    case mozc::win32::VKBackBasedDeleter::CONSUME_KEY_BUT_NEVER_SEND_TO_SERVER:
      ignore_this_keyevent = true;
      break;
    case mozc::win32::VKBackBasedDeleter::
             CALL_END_DELETION_BUT_NEVER_SEND_TO_SERVER:
      ignore_this_keyevent = true;
      private_context->deleter->EndDeletion();
      break;
    case mozc::win32::VKBackBasedDeleter::SEND_KEY_TO_APPLICATION:
    default:
      DLOG(FATAL) << "this action is not applicable to ImeProcessKey.";
      break;
  }

  if (ignore_this_keyevent) {
    // no message generated.
    return 0;
  }

  if (context->fOpen) {
    ignore_this_keyevent = false;
    const mozc::win32::SurrogatePairObserver::ClientAction surrogate_action =
        private_context->surrogate_pair_observer->OnKeyEvent(vk, is_key_down);
    switch (surrogate_action.type) {
      case mozc::win32::SurrogatePairObserver::DO_DEFAULT_ACTION:
        break;
      case mozc::win32::SurrogatePairObserver::
             DO_DEFAULT_ACTION_WITH_RETURNED_UCS4:
        vk = mozc::win32::VirtualKey::FromUnicode(surrogate_action.ucs4);
      break;
      case mozc::win32::SurrogatePairObserver::
          CONSUME_KEY_BUT_NEVER_SEND_TO_SERVER:
        ignore_this_keyevent = true;
        break;
      default:
        DLOG(FATAL) << "this action is not applicable to ImeProcessKey.";
        break;
    }
    if (ignore_this_keyevent) {
      // no message generated.
      return 0;
    }
  }

  if (use_pending_status) {
    next_state = private_context->deleter->pending_ime_state();
    temporal_output.CopyFrom(
        private_context->deleter->pending_output());
  } else {
    const mozc::win32::KeyEventHandlerResult result =
        mozc::win32::ImeCore::ImeToAsciiEx(
            private_context->client,
            vk, raw_scan_code, is_key_down, keyboard_status, behavior,
            ime_state, &next_state, &temporal_output);

    if (!result.succeeded) {
      // no message generated.
      return 0;
    }

    if (!result.should_be_sent_to_server) {
      // no message generated.
      return 0;
    }
  }

  mozc::win32::MessageQueue message_queue(himc);
  message_queue.Attach(trans_buf);

  if (!mozc::win32::ImeCore::UpdateContext(
          himc, next_state, temporal_output, &message_queue)) {
    // no message generated.
    return 0;
  }

  // MessageQueue::Detach returns the number of messages.
  return message_queue.Detach();
}

BOOL WINAPI ImeConfigure(HKL hkl, HWND wnd, DWORD mode, LPVOID data) {
  DANGLING_CALLBACK_GUARD(FALSE);
  FUNCTION_ENTER();

  if (mode == IME_CONFIG_GENERAL) {
    if (!mozc::Process::SpawnMozcProcess(
            mozc::kMozcTool, "--mode=config_dialog")) {
      return FALSE;
    }
    return TRUE;
  }

  if (mode == IME_CONFIG_SELECTDICTIONARY) {
    if (!mozc::Process::SpawnMozcProcess(
            mozc::kMozcTool, "--mode=dictionary_tool")) {
      return FALSE;
    }
    return TRUE;
  }

  if (mode == IME_CONFIG_REGISTERWORD) {
    if (data == NULL) {
      // |data| must not be NULL if |mode| is IME_CONFIG_REGISTERWORD.
      // http://msdn.microsoft.com/en-us/library/dd318173.aspx
      return FALSE;
    }

    // Retrieves word registration data.
    const REGISTERWORD *reg_word = static_cast<REGISTERWORD *>(data);
    const wstring word = GetStringIfWithinLimit(reg_word->lpWord,
                                                kMaxCharsForRegisterWord);
    const wstring reading = GetStringIfWithinLimit(reg_word->lpReading,
                                                   kMaxCharsForRegisterWord);

    SetEnveronmentVariablesForWordRegisterDialog(word, reading);
    const bool spawn_succeeded = mozc::Process::SpawnMozcProcess(
          mozc::kMozcTool, "--mode=word_register_dialog");
    // Delete all environment variables used.
    SetEnveronmentVariablesForWordRegisterDialog(L"", L"");

    if (!spawn_succeeded) {
      return FALSE;
    }

    return TRUE;
  }

  return FALSE;
}

BOOL WINAPI ImeRegisterWord(LPCTSTR reading, DWORD style, LPCTSTR value) {
  DANGLING_CALLBACK_GUARD(FALSE);
  FUNCTION_ENTER();

  return FALSE;
}

BOOL WINAPI ImeUnregisterWord(LPCTSTR lpRead, DWORD style, LPCTSTR value) {
  DANGLING_CALLBACK_GUARD(FALSE);
  FUNCTION_ENTER();

  return FALSE;
}

UINT WINAPI ImeGetRegisterWordStyle(UINT item, LPSTYLEBUF style_buffer) {
  DANGLING_CALLBACK_GUARD(0);
  FUNCTION_ENTER();

  return 0;
}

UINT WINAPI ImeEnumRegisterWord(REGISTERWORDENUMPROC enum_proc,
                                LPCTSTR reading,
                                DWORD style,
                                LPCTSTR value,
                                LPVOID data) {
  DANGLING_CALLBACK_GUARD(0);
  FUNCTION_ENTER();

  return 0;
}
