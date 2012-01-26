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

#include "win32/ime/ime_ui_window.h"

#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
#define _ATL_NO_HOSTING
// Workaround against KB813540
#include <atlbase_mozc.h>
#include <atlapp.h>
#include <atlstr.h>
#include <atlwin.h>
#include <atlmisc.h>
#include <strsafe.h>

#include "base/const.h"
#include "base/process.h"
#include "base/process_mutex.h"
#include "base/run_level.h"
#include "base/singleton.h"
#include "base/util.h"
#include "config/config_handler.h"
#include "renderer/renderer_client.h"
#include "session/commands.pb.h"
#include "win32/base/conversion_mode_util.h"
#include "win32/base/imm_util.h"
#include "win32/base/string_util.h"
#include "win32/ime/ime_core.h"
#include "win32/ime/ime_impl_imm.h"
#include "win32/ime/ime_trace.h"
#include "win32/ime/ime_language_bar.h"
#include "win32/ime/ime_mouse_tracker.h"
#include "win32/ime/ime_scoped_context.h"
#include "win32/ime/ime_types.h"
#include "win32/ime/ime_ui_context.h"
#include "win32/ime/ime_ui_visibility_tracker.h"
#include "win32/ime/output_util.h"

namespace mozc {
namespace win32 {
namespace {
using ATL::CRegKey;
using ATL::CStringA;
using ATL::CWindow;
using WTL::CPoint;
using WTL::CRect;

const int kRendereCommandMaxRetry = 2;

// True if the the DLL received DLL_PROCESS_DETACH notification.
bool g_module_unloaded = false;

// As filed in b/3088049 or b/4271156, the IME module (e.g. GIMEJa.ime) is
// sometimes unloaded too early. You can use this macro to guard callback
// functions from being called in such situation.
#define DANGLING_CALLBACK_GUARD(return_code)   \
  do {                                         \
    if (g_module_unloaded) {                   \
      return (return_code);                    \
    }                                          \
  } while (false)

// A global variable of mozc::once_t, which is POD, has no bad side effect.
static once_t g_launch_set_default_dialog = MOZC_ONCE_INIT;

void LaunchSetDefaultDialog() {
  const config::Config &config =
      config::ConfigHandler::GetConfig();
  // Note that we need not to launch SetDefaultDialog here if the old IME is
  // default.  See b/2935950 for details.
  if ((config.has_check_default() && !config.check_default()) ||
       ImeUtil::IsDefault()) {
    VLOG(1) << "Do not launch SetDefaultDialog.";
    return;
  }

  {
    mozc::ProcessMutex mutex("set_default_dialog");
    if (!mutex.Lock()) {
      VLOG(1) << "SetDefaultDialog is already launched";
      return;
    }
    // mutex is unlocked here
  }

  VLOG(1) << "Launching SetDefaultDialog";
  // Even if SetDefaultDialog is launched multiple times,
  // it's safe because mozc_tool also checks the existence of
  // the process with ProcessMutex.
  mozc::Process::SpawnMozcProcess(mozc::kMozcTool,
                                  "--mode=set_default_dialog");
}

// This class is expected to be used a singleton object to enable Win32
// message-based event callback from the renderer to the client mainly
// to support mouse operation on the candidate list.
class PrivateRendererMessageInitializer {
 public:
  PrivateRendererMessageInitializer()
      : private_renderer_message_(
            ::RegisterWindowMessage(mozc::kMessageReceiverMessageName)) {}

  // Adds an exceptional rule for the message filter which prevents from
  // receiving a window message from a process in lower integrity level.
  // Returns true if the operation completed successfully.  Make sure to
  // call this method once per window, otherwise this function returns false.
  bool Initialize(HWND target_window) {
    if (private_renderer_message_ == 0) {
      FUNCTION_TRACE(
          L"Failed to retrieve a private message ID for the renderer.");
      return false;
    }
    if (!::IsWindow(target_window)) {
      FUNCTION_TRACE(L"Invalid windows handle is specified.");
      return false;
    }
    return ChangeMessageFilter(private_renderer_message_, target_window);
  }

  // Returns true if the specified message ID is the callback message.
  bool IsPrivateRendererMessage(UINT message) const {
    if (private_renderer_message_ == 0) {
      return false;
    }
    return (private_renderer_message_ == message);
  }

 private:
  static bool ChangeMessageFilter(UINT msg, HWND handle) {
    typedef BOOL (WINAPI *FPChangeWindowMessageFilter)(UINT, DWORD);
    typedef BOOL (WINAPI *FPChangeWindowMessageFilterEx)(
        HWND, UINT, DWORD, LPVOID);

    // Following constants are not available unless we change the WINVER
    // higher enough.
    const int kMessageFilterAdd = 1;    // MSGFLT_ADD    (WINVER >=0x0600)
    const int kMessageFilterAllow = 1;  // MSGFLT_ALLOW  (WINVER >=0x0601)

    // Skip windows XP.
    // ChangeWindowMessageFilter is only available on Windows Vista or Later
    if (!mozc::Util::IsVistaOrLater()) {
      FUNCTION_TRACE(L"Skip ChangeWindowMessageFilter on Windows XP");
      return true;
    }

    const HMODULE lib = mozc::Util::GetSystemModuleHandle(L"user32.dll");
    if (lib == NULL) {
      FUNCTION_TRACE(L"GetModuleHandle for user32.dll failed.");
      return false;
    }

    // Windows 7
    // http://msdn.microsoft.com/en-us/library/dd388202.aspx
    FPChangeWindowMessageFilterEx change_window_message_filter_ex
        = reinterpret_cast<FPChangeWindowMessageFilterEx>(
            ::GetProcAddress(lib, "ChangeWindowMessageFilterEx"));
    if (change_window_message_filter_ex != NULL) {
      // Windows 7 only
      if (!(*change_window_message_filter_ex)(handle, msg,
                                              kMessageFilterAllow, NULL)) {
        const int error = ::GetLastError();
        FUNCTION_TRACE_FORMAT(
            L"ChangeWindowMessageFilterEx failed:. error = %1!d!", error);
        return false;
      }
      return true;
    }

    // Windows Vista
    FPChangeWindowMessageFilter change_window_message_filter
        = reinterpret_cast<FPChangeWindowMessageFilter>(
            ::GetProcAddress(lib, "ChangeWindowMessageFilter"));
    if (change_window_message_filter == NULL) {
      const int error = ::GetLastError();
      FUNCTION_TRACE_FORMAT(
          L"GetProcAddress failed. error = %1!d!", error);
      return false;
    }

    DCHECK(change_window_message_filter != NULL);
    if (!(*change_window_message_filter)(msg, kMessageFilterAdd)) {
      const int error = ::GetLastError();
      FUNCTION_TRACE_FORMAT(
          L"ChangeWindowMessageFilter failed. error = %1!d!", error);
      return false;
    }

    return true;
  }

  UINT private_renderer_message_;
  DISALLOW_COPY_AND_ASSIGN(PrivateRendererMessageInitializer);
};

void UpdateCommand(const UIContext &context,
                   HWND ui_window,
                   const UIVisibilityTracker &ui_visibility_tracker,
                   mozc::commands::RendererCommand *command) {
  typedef mozc::commands::RendererCommand::ApplicationInfo ApplicationInfo;
  const bool show_composition_window =
      ui_visibility_tracker.IsCompositionWindowVisible();
  const bool show_candidate_window =
      ui_visibility_tracker.IsCandidateWindowVisible();
  const bool show_suggest_window =
      ui_visibility_tracker.IsSuggestWindowVisible();

  vector<wstring> candidate_list;
  DWORD focused_index = 0;
  if (!context.IsEmpty() && context.GetOpenStatus()) {
    // Copy the last output.
    context.GetLastOutput(command->mutable_output());

    bool composition_window_visible = false;
    bool candidate_window_visible = false;
    bool suggest_window_visible = false;

    // Check if composition window is actually visible.
    if (command->output().has_preedit()) {
      composition_window_visible = show_composition_window;
    }

    // Check if suggest window and candidate window are actually visible.
    if (command->output().has_candidates() &&
        command->output().candidates().has_category()) {
      switch (command->output().candidates().category()) {
        case commands::SUGGESTION:
          suggest_window_visible = show_suggest_window;
          break;
        case commands::CONVERSION:
        case commands::PREDICTION:
          candidate_window_visible = show_candidate_window;
          break;
        default:
          // do nothing.
          break;
      }
    }

    if (composition_window_visible || candidate_window_visible ||
        suggest_window_visible) {
      command->set_visible(true);
    }
  }

  const CWindow target_window(context.GetAttachedWindow());
  ApplicationInfo &app_info = *command->mutable_application_info();
  app_info.set_process_id(::GetCurrentProcessId());
  app_info.set_thread_id(::GetCurrentThreadId());
  app_info.set_target_window_handle(
      reinterpret_cast<uint32>(target_window.m_hWnd));
  app_info.set_receiver_handle(reinterpret_cast<uint32>(ui_window));
  app_info.set_input_framework(ApplicationInfo::IMM32);
  int visibility = ApplicationInfo::ShowUIDefault;
  if (show_composition_window) {
    // Note that |ApplicationInfo::ShowCompositionWindow| represents that the
    // application does not mind the IME showing its own composition window.
    // This bit does not mean that |command| requires the composition window.
    visibility |= ApplicationInfo::ShowCompositionWindow;
  }
  if (show_candidate_window) {
    // Note that |ApplicationInfo::ShowCandidateWindow| represents that the
    // application does not mind the IME showing its own candidate window.
    // This bit does not mean that |command| requires the suggest window.
    visibility |= ApplicationInfo::ShowCandidateWindow;
  }
  if (show_suggest_window) {
    // Note that |ApplicationInfo::ShowCandidateWindow| represents that the
    // application does not mind the IME showing its own candidate window.
    // This bit does not mean that |command| requires the suggest window.
    visibility |= ApplicationInfo::ShowSuggestWindow;
  }
  app_info.set_ui_visibilities(visibility);

  // UIContext::FillCharPosition is subject to cause b/3208669, b/3096191,
  // b/3212271, b/3223011, and b/4285222.
  // So we do not retrieve IMM32 related positional information when the
  // renderer hides all the UI windows.
  // Currently, the following two cases are considered.
  //  - there is no composition string.
  //  - |command->visible() == false|
  if (!context.IsCompositionStringEmpty() && command->visible()) {
    context.FillFontInfo(&app_info);
    context.FillCaretInfo(&app_info);
    context.FillCompositionForm(&app_info);
    context.FillCharPosition(&app_info);
    context.FillCandidateForm(&app_info);
  }
}

// Returns a HIMC assuming from the handle of a UI window.
// Returns NULL if it is not available.
HIMC GetSafeHIMC(HWND window_handle) {
  if (!::IsWindow(window_handle)) {
    return NULL;
  }

  const HIMC himc =
      reinterpret_cast<HIMC>(::GetWindowLongPtr(window_handle, IMMGWLP_IMC));

  // As revealed in b/3207711, ImeSetActiveContext is called without any
  // prior call to ImeSelect.  Perhaps this horrible situation might come from
  // CUAS's implementation in XP.
  // We will not use a HIMC as long as it seems to be uninitialized.
  if (!ImeCore::IsInputContextInitialized(himc)) {
    return NULL;
  }

  return himc;
}

bool TurnOnIMEAndTryToReconvertFromIME(HWND hwnd) {
  const HIMC himc = GetSafeHIMC(hwnd);
  if (himc == NULL) {
    return false;
  }
  if (!mozc::win32::ImeCore::TurnOnIMEAndTryToReconvertFromIME(himc)) {
    return false;
  }
  return true;
}

class LangBarCallbackImpl : public LangBarCallback {
 public:
  explicit LangBarCallbackImpl(HWND hwnd)
      : hwnd_(hwnd),
        reference_count_(1) {
    FUNCTION_TRACE_FORMAT(L"thread = %1!d!, hwnd_ = %2!d!.",
                          ::GetCurrentThreadId(),
                          reinterpret_cast<DWORD>(hwnd_));
  }

  virtual ~LangBarCallbackImpl() {
    FUNCTION_TRACE_FORMAT(L"thread = %1!d!, hwnd_ = %2!d!.",
                          ::GetCurrentThreadId(),
                          reinterpret_cast<DWORD>(hwnd_));
  }

  virtual ULONG AddRef() {
    const LONG count = ::InterlockedIncrement(&reference_count_);
    FUNCTION_TRACE_FORMAT(L"thread = %1!d!, hwnd_ = %2!d!, refcount = %3!d!.",
                          ::GetCurrentThreadId(),
                          reinterpret_cast<DWORD>(hwnd_),
                          reference_count_);
    return max(count, 0);
  }

  virtual ULONG Release() {
    const LONG count = ::InterlockedDecrement(&reference_count_);
    FUNCTION_TRACE_FORMAT(L"thread = %1!d!, hwnd_ = %2!d!, refcount = %3!d!.",
                          ::GetCurrentThreadId(),
                          reinterpret_cast<DWORD>(hwnd_),
                          reference_count_);
    if (count <= 0) {
      delete this;
      return 0;
    }
    return count;
  }

  // This method is called back by the lang bar when an item on the langbar is
  // selected.
  virtual HRESULT OnMenuSelect(MenuId menu_id) {
    DANGLING_CALLBACK_GUARD(E_FAIL);
    if (::IsWindow(hwnd_) == FALSE) {
      return E_FAIL;
    }
    // TODO(yukawa): Check run level.
    HRESULT result = S_OK;
    switch (menu_id) {
      case LangBarCallback::kDirect: {
        result = SetInputMode(mozc::commands::DIRECT);
        break;
      }
      case LangBarCallback::kHiragana: {
        result = SetInputMode(mozc::commands::HIRAGANA);
        break;
      }
      case LangBarCallback::kFullKatakana: {
        result = SetInputMode(mozc::commands::FULL_KATAKANA);
        break;
      }
      case LangBarCallback::kHalfAlphanumeric: {
        result = SetInputMode(mozc::commands::HALF_ASCII);
        break;
      }
      case LangBarCallback::kFullAlphanumeric: {
        result = SetInputMode(mozc::commands::FULL_ASCII);
        break;
      }
      case LangBarCallback::kHalfKatakana: {
        result = SetInputMode(mozc::commands::HALF_KATAKANA);
        break;
      }
      case LangBarCallback::kProperty: {
        // Open the config dialog.
        if (!mozc::Process::SpawnMozcProcess(
                mozc::kMozcTool, "--mode=config_dialog")) {
          result = E_FAIL;
        }
        break;
      }
      case LangBarCallback::kDictionary: {
        // Open the dictionary tool.
        if (!mozc::Process::SpawnMozcProcess(
                mozc::kMozcTool, "--mode=dictionary_tool")) {
          result = E_FAIL;
        }
        break;
      }
      case LangBarCallback::kWordRegister: {
        // Open the word register dialog.
        if (!mozc::Process::SpawnMozcProcess(
                mozc::kMozcTool, "--mode=word_register_dialog")) {
          result = E_FAIL;
        }
        break;
      }
      case LangBarCallback::kHandWriting: {
        // Open the Hand Writing Tool.
        if (!mozc::Process::SpawnMozcProcess(
                mozc::kMozcTool, "--mode=hand_writing")) {
          result = E_FAIL;
        }
        break;
      }
      case LangBarCallback::kCharacterPalette: {
        // Open the Character Palette dialog.
        if (!mozc::Process::SpawnMozcProcess(
                mozc::kMozcTool, "--mode=character_palette")) {
          result = E_FAIL;
        }
        break;
      }
      case LangBarCallback::kReconversion: {
        if (!TurnOnIMEAndTryToReconvertFromIME(hwnd_)) {
          result = E_FAIL;
        }
        break;
      }
      case LangBarCallback::kAbout: {
        // Open the about dialog.
        if (!mozc::Process::SpawnMozcProcess(mozc::kMozcTool,
                                             "--mode=about_dialog")) {
          result = E_FAIL;
        }
        break;
      }
      case LangBarCallback::kHelp: {
        // Open the about dialog.
        const char kHelpUrl[] = "http://www.google.com/support/ime/japanese";
        if (!mozc::Process::OpenBrowser(kHelpUrl)) {
          result = E_FAIL;
        }
        break;
      }
      default: {
        break;
      }
    }
    return result;
  }

 private:
  HRESULT SetInputMode(mozc::commands::CompositionMode mode) {
    const HIMC himc = GetSafeHIMC(hwnd_);
    if (himc == NULL) {
      return E_FAIL;
    }

    if (mode == mozc::commands::DIRECT) {
      // Close IME.
      if (::ImmSetOpenStatus(himc, FALSE) == FALSE) {
        return E_FAIL;
      }
      return S_OK;
    }

    const bool is_open = (::ImmGetOpenStatus(himc) != FALSE);
    if (!is_open) {
      if (::ImmSetOpenStatus(himc, TRUE) == FALSE) {
        return E_FAIL;
      }
    }

    // TODO(yukawa): check kana-lock mode
    uint32 imm32_composition_mode = 0;
    if (!win32::ConversionModeUtil::ToNativeMode(mode, true,
                                                 &imm32_composition_mode)) {
      return E_FAIL;
    }

    DWORD composition_mode = 0;
    DWORD sentence_mode = 0;
    if (::ImmGetConversionStatus(himc, &composition_mode, &sentence_mode) ==
        FALSE) {
      return E_FAIL;
    }
    composition_mode = static_cast<DWORD>(imm32_composition_mode);
    if (::ImmSetConversionStatus(himc, composition_mode, sentence_mode) ==
        FALSE) {
      return E_FAIL;
    }
    return S_OK;
  }

  // Represents the reference count to an instance.
  // volatile modifier is added to conform with InterlockedIncrement API.
  volatile LONG reference_count_;
  HWND hwnd_;

  DISALLOW_COPY_AND_ASSIGN(LangBarCallbackImpl);
};

// TODO(yukawa): Refactor for unit tests and better integration with ImeCore.
class DefaultUIWindow {
 public:
  explicit DefaultUIWindow(HWND hwnd)
      : hwnd_(hwnd),
        renderer_command_num_retry_(kRendereCommandMaxRetry),
        langbar_callback_(new LangBarCallbackImpl(hwnd)),
        renderer_client_(new mozc::renderer::RendererClient),
        language_bar_(new LanguageBar) {
  }

  ~DefaultUIWindow() {
    langbar_callback_->Release();
    langbar_callback_ = NULL;
  }

  void UninitLangBar() {
    language_bar_->UninitLanguageBar();
  }

  void OnStartComposition(const UIContext &context) {
    context.ui_visibility_tracker()->OnStartComposition();
  }

  void OnComposition(
      const UIContext &context, wchar_t latest_change,
      const CompositionChangeAttributes &attributes) {
    context.ui_visibility_tracker()->OnComposition();
  }

  void OnEndComposition(const UIContext &context) {
    context.ui_visibility_tracker()->OnEndComposition();
  }

  LRESULT OnNotify(const UIContext &context,
                   DWORD sub_message, LPARAM lParam) {
    context.ui_visibility_tracker()->OnNotify(sub_message, lParam);

    LRESULT result = 0;
    switch (sub_message) {
      case IMN_CLOSESTATUSWINDOW:
        break;
      case IMN_OPENSTATUSWINDOW:
        break;
      case IMN_SETCONVERSIONMODE:
        result = (UpdateLangBar(context) ? 0 : 1);
        break;
      case IMN_SETSENTENCEMODE:
        // Do nothing because we only support IME_SMODE_PHRASEPREDICT, which
        // is not shown in our LangBar.
        // See b/2913510, b/2954777, and b/2955175 for details.
        break;
      case IMN_SETOPENSTATUS:
        result = (UpdateLangBar(context) ? 0 : 1);
        break;
      case IMN_SETCANDIDATEPOS: {
        if (lParam & 0x1) {
          UpdateCandidate(context);
        }
        break;
      }
      case IMN_SETCOMPOSITIONFONT:
        if (!context.IsEmpty() && context.GetOpenStatus()) {
          LOGFONT font;
          if (context.GetCompositionFont(&font)) {
            // TODO(yukawa):
            // - Update the composition window.
          }
        }
        break;
      case IMN_SETCOMPOSITIONWINDOW:
        UpdateCandidate(context);
        break;
      case IMN_SETSTATUSWINDOWPOS:
        // TODO(yukawa):
        // - Redraw status window.
        break;
      case IMN_GUIDELINE:
        break;
      case IMN_PRIVATE:
        if (lParam == kNotifyUpdateUI) {
          UpdateCandidate(context);
        } else if (lParam == kNotifyReconvertFromIME) {
          TurnOnIMEAndTryToReconvertFromIME(hwnd_);
        }
        break;
    }
    return result;
  }

  LRESULT OnSetContext(const UIContext &context, bool activated,
                       const ShowUIAttributes &show_ui_attributes) {
    // |context| might be uninitialized.  See b/3099588.
    if (context.ui_visibility_tracker() == NULL) {
      return 0;
    }

    if (activated) {
      // The input context specified with |context| is activated.
      context.ui_visibility_tracker()->OnSetContext(show_ui_attributes);
    }
    UpdateCandidate(context);
    return (UpdateLangBar(context) ? 0 : 1);
  }

  LRESULT OnControl(const UIContext &context, DWORD sub_message,
                    void *data) {
    return 0;
  }

  void OnCompositionFull(const UIContext &context) {
  }

  void OnSelect(const UIContext &context, bool select,
                HKL keyboard_layout) {
    if (!select) {
      UninitLangBar();
      return;
    }
    UpdateCandidate(context);
    UpdateLangBar(context);

    // If the application does not allow the IME to show any UI component,
    // it would be better not to show the set default dialog.
    // We use the visibility of suggest window as a launch condition of
    // SetDefaultDialog.
    if (context.ui_visibility_tracker()->IsSuggestWindowVisible()) {
      if (RunLevel::IsValidClientRunLevel()) {
        CallOnce(&g_launch_set_default_dialog,
                 &LaunchSetDefaultDialog);
      }
    }
  }

  LRESULT OnRequest(const UIContext &context,
                    WPARAM wParam, LPARAM lParam) {
    return 0;
  }

  LRESULT OnSessionCommand(
      HIMC himc, commands::SessionCommand::CommandType command_type,
      LPARAM lParam) {
    if (himc == NULL) {
      return 0;
    }

    if ((command_type != commands::SessionCommand::SELECT_CANDIDATE) &&
        (command_type != commands::SessionCommand::HIGHLIGHT_CANDIDATE) &&
        (command_type != commands::SessionCommand::USAGE_STATS_EVENT)) {
      // Unsupported command.
      return 0;
    }

    if ((command_type == commands::SessionCommand::SELECT_CANDIDATE) ||
        (command_type == commands::SessionCommand::HIGHLIGHT_CANDIDATE)) {
      // Convert |mozc_candidate_id| to candidate index.
      const int32 mozc_candidate_id = static_cast<int32>(lParam);
      int candidate_index = 0;
      {
        UIContext context(himc);
        commands::Output output;
        if (!context.GetLastOutput(&output)) {
          return 0;
        }
        if (!OutputUtil::GetCandidateIndexById(
                 output, mozc_candidate_id, &candidate_index)) {
          return 0;
        }
      }  // release |context|.

      const int kCandidateWindowIndex = 0;
      if (::ImmNotifyIME(himc, NI_SELECTCANDIDATESTR, kCandidateWindowIndex,
                         candidate_index) == FALSE) {
        return 0;
      }
      if (command_type == commands::SessionCommand::SELECT_CANDIDATE) {
        if (::ImmNotifyIME(himc, NI_CLOSECANDIDATE, kCandidateWindowIndex, 0) ==
            FALSE) {
          return 0;
        }
      }
    } else if (command_type == commands::SessionCommand::USAGE_STATS_EVENT) {
      // Send USAGE_STATS_EVENT to the server.
      UIContext context(himc);
      mozc::commands::Output output;
      mozc::commands::SessionCommand command;
      mozc::commands::SessionCommand::UsageStatsEvent event =
          static_cast<mozc::commands::SessionCommand::UsageStatsEvent>(lParam);
      command.set_type(mozc::commands::SessionCommand::USAGE_STATS_EVENT);
      command.set_usage_stats_event(event);
      if (!context.client()->SendCommand(command, &output)) {
        return 0;
      }
    }
    return 1;
  }

  LRESULT UIMessageProc(const UIContext &context,
                        UINT message,
                        WPARAM wParam,
                        LPARAM lParam) {
    // A UI window should admit receiving a message even when the context is
    // empty.  You could see this situation as follows.
    //   1. Do not set Mozc as default.
    //   2. Open Notepad.
    //   3. Open Help - Version Info.
    //   4. Select Mozc in Langbar.
    // See b/2973431 and b/2970662 for details.
    if (context.IsEmpty()) {
      if ((message == WM_IME_SELECT) && (wParam == FALSE)) {
        UninitLangBar();
      } else {
        UpdateLangBar(context);
      }
      return 0;
    }
    switch (message) {
      case WM_IME_COMPOSITION:
        // return value will be ignored.
        OnComposition(context, (wchar_t)(wParam),
                      CompositionChangeAttributes(lParam));
        break;
      case WM_IME_COMPOSITIONFULL:
        // return value will be ignored.
        OnCompositionFull(context);
        break;
      case WM_IME_CONTROL:
        return OnControl(context, static_cast<DWORD>(wParam),
                         reinterpret_cast<void *>(lParam));
      case WM_IME_ENDCOMPOSITION:
        // return value will be ignored.
        OnEndComposition(context);
        break;
      case WM_IME_NOTIFY:
        return OnNotify(context, static_cast<DWORD>(wParam), lParam);
      case WM_IME_REQUEST:
        return OnRequest(context, wParam, lParam);
      case WM_IME_SELECT:
        // return value will be ignored.
        OnSelect(context, wParam != FALSE, (HKL)lParam);
        break;
      case WM_IME_SETCONTEXT:
        return OnSetContext(context, wParam != FALSE, ShowUIAttributes(lParam));
      case WM_IME_STARTCOMPOSITION:
        // return value will be ignored.
        OnStartComposition(context);
        break;
      case WM_IME_CHAR:
      case WM_IME_KEYDOWN:
      case WM_IME_KEYUP:
        // return value will be ignored.
        break;
      default:
        // Unknown IME_* message
        break;
    }
    // default return value
    return 0;
  }

 private:
  // Constructs RendererCommand based on various parameters in the input
  // context.  This implementation is very experimental, should be revised.
  void UpdateCandidate(const UIContext &context) {
    if (renderer_command_num_retry_ <= 0) {
      DLOG(INFO) << "RendererClient::ExecCommand failed "
                 << kRendereCommandMaxRetry
                 << " times. Gave up to call RendererClient::ExecCommand.";
      return;
    }

    mozc::commands::RendererCommand command;
    if (!command.IsInitialized()) {
      return;
    }
    command.set_type(mozc::commands::RendererCommand::UPDATE);
    command.set_visible(false);
    UpdateCommand(context, hwnd_, *context.ui_visibility_tracker(), &command);
    if (renderer_client_->ExecCommand(command)) {
      renderer_command_num_retry_ = kRendereCommandMaxRetry;
    } else {
      DLOG(ERROR) << "RendererClient::ExecCommand failed.";
      --renderer_command_num_retry_;
    }
  }

  bool UpdateLangBar(const UIContext &context) {
    // Ensure the LangBar is initialized.
    language_bar_->InitLanguageBar(langbar_callback_);

    if (context.IsEmpty()) {
      language_bar_->UpdateLangbarMenu(commands::DIRECT);
      language_bar_->SetLangbarMenuEnabled(false);
      return true;
    }
    if (!context.GetOpenStatus()) {
      // Closed
      language_bar_->UpdateLangbarMenu(commands::DIRECT);
      language_bar_->SetLangbarMenuEnabled(true);
      return true;
    }
    DWORD imm32_composition_mode = 0;
    if (!context.GetConversionMode(&imm32_composition_mode)) {
      return false;
    }
    commands::CompositionMode mozc_mode;
    if (!win32::ConversionModeUtil::ToMozcMode(imm32_composition_mode,
      &mozc_mode)) {
        return false;
    }
    language_bar_->UpdateLangbarMenu(mozc_mode);
    language_bar_->SetLangbarMenuEnabled(true);
    return true;
  }

  HWND hwnd_;
  int renderer_command_num_retry_;
  scoped_ptr<mozc::renderer::RendererClient> renderer_client_;
  scoped_ptr<LanguageBar> language_bar_;
  LangBarCallbackImpl *langbar_callback_;

  DISALLOW_COPY_AND_ASSIGN(DefaultUIWindow);
};

// When a series of private callback messages is incoming from the renderer
// process, we might want to aggregate them mainly for performance.
// We can aggregate successive callbacks as follows.
//
// [Case 1]
//                   (Post Message Queue top)
//   [1] kMessageReceiverMessageName / HIGHLIGHT_CANDIDATE
//   [2] kMessageReceiverMessageName / HIGHLIGHT_CANDIDATE
//   [3] kMessageReceiverMessageName / HIGHLIGHT_CANDIDATE
//   [4] kMessageReceiverMessageName / SELECT_CANDIDATE
//   [5] kMessageReceiverMessageName / HIGHLIGHT_CANDIDATE
//                      any other message(s)
//   [N] kMessageReceiverMessageName / SELECT_CANDIDATE
//                      any other message(s)
//
//   In this case, messages from [1] to [3] can be removed and and start
//   handling the message [4] as if it the handler just received it.
//
//
// [Case 2]
//                   (Post Message Queue top)
//   [1] kMessageReceiverMessageName / HIGHLIGHT_CANDIDATE
//   [2] kMessageReceiverMessageName / HIGHLIGHT_CANDIDATE
//   [3] kMessageReceiverMessageName / HIGHLIGHT_CANDIDATE
//                      any other message(s)
//   [N] kMessageReceiverMessageName / HIGHLIGHT_CANDIDATE
//                      any other message(s)
//
//   In this case, messages from [1] to [2] can be removed and and start
//   handling the message [4] as if it the handler just received it.
//
//
// [Case 3]
//                   (Post Message Queue top)
//   [1] kMessageReceiverMessageName / SELECT_CANDIDATE
//   [2] kMessageReceiverMessageName / HIGHLIGHT_CANDIDATE
//                      any other message(s)
//   [N] kMessageReceiverMessageName / HIGHLIGHT_CANDIDATE
//                      any other message(s)
//
//   In this case, just start handling the message [1].
//
// This function returns the aggregated message which should be handled now.
MSG AggregateRendererCallbackMessage(
    HWND hwnd, UINT private_message, WPARAM wParam, LPARAM lParam) {
  MSG current_msg = {};
  current_msg.hwnd = hwnd;
  current_msg.message = private_message;
  current_msg.wParam = wParam;
  current_msg.lParam = lParam;

  while (true) {
    MSG next_msg = {};
    // Preview the next message from the post message queue.
    // You can avoid message dispatching from the send message queue by not
    // specifying PM_QS_SENDMESSAGE to the 5th argument.
    if (::PeekMessageW(&next_msg, hwnd, 0, 0,
                       PM_NOREMOVE | PM_QS_POSTMESSAGE | PM_NOYIELD) == 0) {
      // No message is in the queue.
      // |current_msg| is what we should handle now.
      return current_msg;
    }

    if (next_msg.message != private_message) {
      // The next message is not a private renderer callback message.
      // |current_msg| is what we should handle now.
      return current_msg;
    }

    // OK, the next message is a private renderer callback.
    // Remove this message from the post message queue.
    MSG removed_msg = {};
    if (::PeekMessageW(&removed_msg, hwnd, private_message, private_message,
                       PM_REMOVE | PM_QS_POSTMESSAGE | PM_NOYIELD) == 0) {
      // Someting wrong.
      // give up aggregating the message.
      return current_msg;
    }

    current_msg = next_msg;
    const mozc::commands::SessionCommand::CommandType command_type =
        static_cast<mozc::commands::SessionCommand::CommandType>(
            current_msg.wParam);

    // If this is a HIGHLIGHT_CANDIDATE message, resume aggregation.
    if (command_type ==
        mozc::commands::SessionCommand::HIGHLIGHT_CANDIDATE) {
      continue;
    }

    return current_msg;
  }
}

LRESULT WINAPI UIWindowProc(HWND hwnd,
                            UINT message,
                            WPARAM wParam,
                            LPARAM lParam) {
  DANGLING_CALLBACK_GUARD(0);

  const bool is_ui_message =
      (::ImmIsUIMessage(NULL, message, wParam, lParam) != FALSE);

  // Create UI window object and associate it to the window.
  if (message == WM_NCCREATE) {
    if (mozc::win32::IsInLockdownMode() ||
        !mozc::RunLevel::IsValidClientRunLevel()) {
      // Clear kana-lock state not to prevent users from typing their
      // correct passwords.
      // TODO(yukawa): Move this code to somewhere appropriate.
      BYTE keyboard_state[256] = {};
      if (::GetKeyboardState(keyboard_state) != FALSE) {
        keyboard_state[VK_KANA] = 0;
        ::SetKeyboardState(keyboard_state);
      }

      // Return FALSE not to be activated if the current session is
      // WinLogon. It may reduce the risk of BSOD.
      return FALSE;
    }

    DefaultUIWindow* ui_window = new DefaultUIWindow(hwnd);

    ::SetWindowLongPtr(hwnd,
                       IMMGWLP_PRIVATE,
                       reinterpret_cast<LONG_PTR>(ui_window));
    ThreadLocalMouseTracker::EnsureInstalled();

    Singleton<PrivateRendererMessageInitializer>::get()->Initialize(hwnd);
  }
  // Retrieves UI window object from the private area.
  DefaultUIWindow* ui_window = reinterpret_cast<DefaultUIWindow*>(
      ::GetWindowLongPtrW(hwnd, IMMGWLP_PRIVATE));

  if (ui_window == NULL) {
    if (is_ui_message) {
      return 0;
    } else {
      return ::DefWindowProc(hwnd, message, wParam, lParam);
    }
  }

  LRESULT result = 0;
  const bool is_renderer_message =
      Singleton<PrivateRendererMessageInitializer>::get()->
          IsPrivateRendererMessage(message);

  bool is_handled = false;
  if (is_ui_message) {
    const HIMC himc = GetSafeHIMC(hwnd);
    result = ui_window->UIMessageProc(UIContext(himc),
                                      message, wParam, lParam);
    is_handled = true;
  } else if (is_renderer_message) {
    const MSG renderer_msg = AggregateRendererCallbackMessage(
        hwnd, message, wParam, lParam);
    const mozc::commands::SessionCommand::CommandType command_type =
        static_cast<mozc::commands::SessionCommand::CommandType>(
            renderer_msg.wParam);
    const HIMC himc = GetSafeHIMC(hwnd);
    result = ui_window->OnSessionCommand(himc, command_type,
                                         renderer_msg.lParam);
    is_handled = true;
  } else if (message == WM_DESTROY) {
    // Ensure the LangBar is uninitialized.
    ui_window->UninitLangBar();
  } else if (message == WM_NCDESTROY) {
    // Unhook mouse events.
    ThreadLocalMouseTracker::EnsureUninstalled();

    // Delete UI window object if the window is destroyed.
    ::SetWindowLongPtr(hwnd, IMMGWLP_PRIVATE, NULL);
    delete ui_window;
  }
  if (!is_handled) {
    result = ::DefWindowProcW(hwnd, message, wParam, lParam);
    is_handled = true;
  }
  return result;
}
}  // anonymous namespace

bool UIWindowManager::OnDllProcessAttach(HINSTANCE module_handle,
                                         bool static_loading) {
  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(WNDCLASSEX);
  wc.style = CS_IME;
  wc.lpfnWndProc = UIWindowProc;
  wc.cbWndExtra = 2 * sizeof(LONG_PTR);
  wc.hInstance = module_handle;
  wc.lpszClassName = kIMEUIWndClassName;

  const ATOM atom = ::RegisterClassExW(&wc);
  if (atom == INVALID_ATOM) {
    const DWORD error = ::GetLastError();
    FUNCTION_TRACE_FORMAT(L"RegisterClass failed. error = %1!d!", error);
    return false;
  }

  return true;
}

void UIWindowManager::OnDllProcessDetach(HINSTANCE module_handle,
                                         bool process_shutdown) {
  if (::UnregisterClass(kIMEUIWndClassName, module_handle) == 0) {
    // Sometimes the IME DLL is unloaded before all the UI message windows
    // which belong to the DLL are destroyed.  In such a situation, we cannot
    // unregister window class. See b/4271156.
    FUNCTION_TRACE(L"UnregisterClass failed");
  }
  // This flag is used to inactivate out DefWindowProc and any other callbacks
  // to avoid further problems.
  g_module_unloaded = true;
}
}  // namespace win32
}  // namespace mozc
