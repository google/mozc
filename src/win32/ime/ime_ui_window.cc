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

#include "win32/ime/ime_ui_window.h"

// clang-format off
#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
#include <atlbase.h>
#include <atltypes.h>
#include <atlapp.h>
#include <atlstr.h>
#include <atlwin.h>
#include <atlmisc.h>
#include <strsafe.h>
// clang-format on

#include <algorithm>
#include <cstdint>
#include <memory>

#include "base/const.h"
#include "base/logging.h"
#include "base/process.h"
#include "base/process_mutex.h"
#include "base/run_level.h"
#include "base/scoped_handle.h"
#include "base/singleton.h"
#include "base/win_util.h"
#include "client/client_interface.h"
#include "config/config_handler.h"
#include "protocol/renderer_command.pb.h"
#include "renderer/win32/win32_renderer_client.h"
#include "session/output_util.h"
#include "win32/base/conversion_mode_util.h"
#include "win32/base/imm_util.h"
#include "win32/base/indicator_visibility_tracker.h"
#include "win32/base/string_util.h"
#include "win32/base/win32_window_util.h"
#include "win32/ime/ime_core.h"
#include "win32/ime/ime_impl_imm.h"
#include "win32/ime/ime_language_bar.h"
#include "win32/ime/ime_scoped_context.h"
#include "win32/ime/ime_types.h"
#include "win32/ime/ime_ui_context.h"
#include "win32/ime/ime_ui_visibility_tracker.h"
#include "absl/base/call_once.h"

namespace mozc {
namespace win32 {
namespace {

using ATL::CRegKey;
using ATL::CStringA;
using ATL::CWindow;

using ::mozc::renderer::win32::Win32RendererClient;

// True if the DLL received DLL_PROCESS_DETACH notification.
volatile bool g_module_unloaded = false;

// As filed in b/3088049 or b/4271156, the IME module (e.g. GIMEJa.ime) is
// sometimes unloaded too early. You can use this macro to guard callback
// functions from being called in such situation.
#define DANGLING_CALLBACK_GUARD(return_code) \
  do {                                       \
    if (g_module_unloaded) {                 \
      return (return_code);                  \
    }                                        \
  } while (false)

// absl::once_flag can be allocated as a global variable.
absl::once_flag g_launch_set_default_dialog;

void LaunchSetDefaultDialog() {
  config::Config config;
  config::ConfigHandler::GetConfig(&config);
  if (config.has_check_default() && !config.check_default()) {
    // User opted out the default IME checking. Do nothing.
    return;
  }

  if (ImeUtil::IsDefault()) {
    // Mozc has already been the default IME. Do nothing.
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
  mozc::Process::SpawnMozcProcess(mozc::kMozcTool, "--mode=set_default_dialog");
}

bool IsProcessSandboxedImpl() {
  bool is_restricted = false;
  if (!WinUtil::IsProcessRestricted(::GetCurrentProcess(), &is_restricted)) {
    return true;
  }
  if (is_restricted) {
    return true;
  }

  bool in_appcontainer = false;
  if (!WinUtil::IsProcessInAppContainer(::GetCurrentProcess(),
                                        &in_appcontainer)) {
    return true;
  }

  return in_appcontainer;
}

bool IsProcessSandboxed() {
  // Thread safety is not required.
  static bool sandboxed = IsProcessSandboxedImpl();
  return sandboxed;
}

// This class is expected to be used a singleton object to enable Win32
// message-based event callback from the renderer to the client mainly
// to support mouse operation on the candidate list.
class PrivateRendererMessageInitializer {
 public:
  PrivateRendererMessageInitializer()
      : private_renderer_message_(
            ::RegisterWindowMessage(mozc::kMessageReceiverMessageName)) {}
  PrivateRendererMessageInitializer(const PrivateRendererMessageInitializer &) =
      delete;
  PrivateRendererMessageInitializer &operator=(
      const PrivateRendererMessageInitializer &) = delete;

  // Adds an exceptional rule for the message filter which prevents from
  // receiving a window message from a process in lower integrity level.
  // Returns true if the operation completed successfully.  Make sure to
  // call this method once per window, otherwise this function returns false.
  bool Initialize(HWND target_window) {
    if (private_renderer_message_ == 0) {
      return false;
    }
    if (!::IsWindow(target_window)) {
      return false;
    }
    return WindowUtil::ChangeMessageFilter(target_window,
                                           private_renderer_message_);
  }

  // Returns true if the specified message ID is the callback message.
  bool IsPrivateRendererMessage(UINT message) const {
    if (private_renderer_message_ == 0) {
      return false;
    }
    return (private_renderer_message_ == message);
  }

 private:
  UINT private_renderer_message_;
};

void UpdateCommand(const UIContext &context, HWND ui_window,
                   const UIVisibilityTracker &ui_visibility_tracker,
                   mozc::commands::RendererCommand *command) {
  typedef ::mozc::commands::RendererCommand::ApplicationInfo ApplicationInfo;
  typedef ::mozc::commands::RendererCommand_IndicatorInfo IndicatorInfo;
  typedef ::mozc::commands::CompositionMode CompositionMode;

  const bool show_composition_window =
      ui_visibility_tracker.IsCompositionWindowVisible();
  const bool show_candidate_window =
      ui_visibility_tracker.IsCandidateWindowVisible();
  const bool show_suggest_window =
      ui_visibility_tracker.IsSuggestWindowVisible();

  std::vector<std::wstring> candidate_list;
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
      WinUtil::EncodeWindowHandle(target_window.m_hWnd));
  app_info.set_receiver_handle(WinUtil::EncodeWindowHandle(ui_window));
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

  // Honor visibility bits for UI-less mode.
  if (visibility != 0 && context.IsModeIndicatorEnabled()) {
    IndicatorVisibilityTracker *indicator_tracker =
        context.indicator_visibility_tracker();
    if (indicator_tracker != nullptr) {
      if (indicator_tracker->IsVisible()) {
        DWORD native_mode = 0;
        CompositionMode mode = commands::DIRECT;
        if (context.GetConversionMode(&native_mode) &&
            ConversionModeUtil::ToMozcMode(native_mode, &mode)) {
          if (!command->has_output()) {
            context.GetLastOutput(command->mutable_output());
          }
          command->set_visible(true);
          IndicatorInfo *info = app_info.mutable_indicator_info();
          info->mutable_status()->set_activated(context.GetOpenStatus());
          info->mutable_status()->set_mode(mode);
        }
      }
    }
  }

  context.FillFontInfo(&app_info);
  context.FillCaretInfo(&app_info);
  context.FillCompositionForm(&app_info);
  context.FillCandidateForm(&app_info);
  // UIContext::FillCharPosition is subject to cause b/3208669, b/3096191,
  // b/3212271, b/3223011, and b/4285222.
  // So we do not retrieve IMM32 related positional information when the
  // renderer hides all the UI windows.
  if (command->visible()) {
    context.FillCharPosition(&app_info);
  }
}

// Returns a HIMC assuming from the handle of a UI window.
// Returns nullptr if it is not available.
HIMC GetSafeHIMC(HWND window_handle) {
  if (!::IsWindow(window_handle)) {
    return nullptr;
  }

  const HIMC himc =
      reinterpret_cast<HIMC>(::GetWindowLongPtr(window_handle, IMMGWLP_IMC));

  // As revealed in b/3207711, ImeSetActiveContext is called without any
  // prior call to ImeSelect.  Perhaps this horrible situation might come from
  // CUAS's implementation in XP.
  // We will not use a HIMC as long as it seems to be uninitialized.
  if (!ImeCore::IsInputContextInitialized(himc)) {
    return nullptr;
  }

  return himc;
}

bool TurnOnIMEAndTryToReconvertFromIME(HWND hwnd) {
  const HIMC himc = GetSafeHIMC(hwnd);
  if (himc == nullptr) {
    return false;
  }
  if (!mozc::win32::ImeCore::TurnOnIMEAndTryToReconvertFromIME(himc)) {
    return false;
  }
  return true;
}

class LangBarCallbackImpl : public LangBarCallback {
 public:
  LangBarCallbackImpl(const LangBarCallbackImpl &) = delete;
  LangBarCallbackImpl &operator=(const LangBarCallbackImpl &) = delete;

  explicit LangBarCallbackImpl(HWND hwnd) : hwnd_(hwnd), reference_count_(1) {}

  virtual ~LangBarCallbackImpl() {}

  virtual ULONG AddRef() {
    const LONG count = ::InterlockedIncrement(&reference_count_);
    return static_cast<ULONG>(std::max<LONG>(count, 0));
  }

  virtual ULONG Release() {
    const LONG count = ::InterlockedDecrement(&reference_count_);
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
        if (!mozc::Process::SpawnMozcProcess(mozc::kMozcTool,
                                             "--mode=config_dialog")) {
          result = E_FAIL;
        }
        break;
      }
      case LangBarCallback::kDictionary: {
        // Open the dictionary tool.
        if (!mozc::Process::SpawnMozcProcess(mozc::kMozcTool,
                                             "--mode=dictionary_tool")) {
          result = E_FAIL;
        }
        break;
      }
      case LangBarCallback::kWordRegister: {
        // Open the word register dialog.
        if (!mozc::Process::SpawnMozcProcess(mozc::kMozcTool,
                                             "--mode=word_register_dialog")) {
          result = E_FAIL;
        }
        break;
      }
      case LangBarCallback::kHandWriting: {
        // Open the Hand Writing Tool.
        if (!mozc::Process::SpawnMozcProcess(mozc::kMozcTool,
                                             "--mode=hand_writing")) {
          result = E_FAIL;
        }
        break;
      }
      case LangBarCallback::kCharacterPalette: {
        // Open the Character Palette dialog.
        if (!mozc::Process::SpawnMozcProcess(mozc::kMozcTool,
                                             "--mode=character_palette")) {
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
        constexpr char kHelpUrl[] =
            "http://www.google.com/support/ime/japanese";
        if (!mozc::Process::OpenBrowser(kHelpUrl)) {
          result = E_FAIL;
        }
        break;
      }
      default:
        break;
    }
    return result;
  }

 private:
  HRESULT SetInputMode(mozc::commands::CompositionMode mode) {
    const HIMC himc = GetSafeHIMC(hwnd_);
    if (himc == nullptr) {
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

    const UIContext context(himc);
    uint32_t imm32_composition_mode = 0;
    if (!win32::ConversionModeUtil::ToNativeMode(
            mode, context.IsKanaInputPreferred(), &imm32_composition_mode)) {
      return E_FAIL;
    }

    DWORD composition_mode = 0;
    DWORD sentence_mode = 0;
    if (::ImmGetConversionStatus(himc, &composition_mode, &sentence_mode) ==
        FALSE) {
      return E_FAIL;
    }
    composition_mode = static_cast<DWORD>(imm32_composition_mode);
    DWORD visible_composition_mode = 0;
    DWORD logical_composition_mode = 0;
    if (context.GetVisibleConversionMode(&visible_composition_mode) &&
        context.GetLogicalConversionMode(&logical_composition_mode) &&
        (composition_mode != visible_composition_mode) &&
        (composition_mode == logical_composition_mode)) {
      // The visible conversion mode is different from the selected mode but the
      // actual conversion mode is the same to the selected mode. In this case,
      // ImmSetConversionStatus cannot be suitable because the actual conversion
      // mode will not be changed. So we will send SwitchInputMode command
      // explicitly.
      mozc::win32::ImeCore::SwitchInputMode(himc, composition_mode, true);
    } else if (::ImmSetConversionStatus(himc, composition_mode,
                                        sentence_mode) == FALSE) {
      return E_FAIL;
    }
    return S_OK;
  }

  // Represents the reference count to an instance.
  // volatile modifier is added to conform with InterlockedIncrement API.
  volatile LONG reference_count_;
  HWND hwnd_;
};

// TODO(yukawa): Refactor for unit tests and better integration with ImeCore.
class DefaultUIWindow {
 public:
  DefaultUIWindow(const DefaultUIWindow &) = delete;
  DefaultUIWindow &operator=(const DefaultUIWindow &) = delete;

  explicit DefaultUIWindow(HWND hwnd)
      : hwnd_(hwnd),
        langbar_callback_(new LangBarCallbackImpl(hwnd)),
        language_bar_(new LanguageBar),
        has_pending_langbar_update_(false) {}

  ~DefaultUIWindow() {
    langbar_callback_->Release();
    langbar_callback_ = nullptr;
  }

  void UninitLangBar() {
    CancelDeferredLangBarUpdateIfExists();
    language_bar_->UninitLanguageBar();
  }

  void OnStartComposition(const UIContext &context) {
    context.ui_visibility_tracker()->OnStartComposition();
  }

  void OnComposition(const UIContext &context, wchar_t latest_change,
                     const CompositionChangeAttributes &attributes) {
    context.ui_visibility_tracker()->OnComposition();
  }

  void OnEndComposition(const UIContext &context) {
    context.ui_visibility_tracker()->OnEndComposition();
  }

  LRESULT OnNotify(const UIContext &context, DWORD sub_message, LPARAM lParam) {
    context.ui_visibility_tracker()->OnNotify(sub_message, lParam);

    LRESULT result = 0;
    switch (sub_message) {
      case IMN_CLOSESTATUSWINDOW:
        break;
      case IMN_OPENSTATUSWINDOW:
        break;
      case IMN_SETCONVERSIONMODE:
        UpdateIndicator(context);
        result = (UpdateLangBar(context, kDeferred) ? 0 : 1);
        break;
      case IMN_SETSENTENCEMODE:
        // Do nothing because we only support IME_SMODE_PHRASEPREDICT, which
        // is not shown in our LangBar.
        // See b/2913510, b/2954777, and b/2955175 for details.
        break;
      case IMN_SETOPENSTATUS:
        UpdateIndicator(context);
        result = (UpdateLangBar(context, kDeferred) ? 0 : 1);
        break;
      case IMN_SETCANDIDATEPOS: {
        if (lParam & 0x1) {
          UpdateCandidate(context, kMoveFocusedWindow);
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
        // TODO(yukawa): Use message hook instead.
        UpdateCandidate(context, kMoveFocusedWindow);
        break;
      case IMN_SETSTATUSWINDOWPOS:
        // TODO(yukawa):
        // - Redraw status window.
        break;
      case IMN_GUIDELINE:
        break;
      case IMN_PRIVATE:
        if (lParam == kNotifyUpdateUI) {
          UpdateLangBar(context, kDeferred);
          UpdateCandidate(context, kNoEvent);
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
    if (context.ui_visibility_tracker() == nullptr) {
      return 0;
    }

    if (activated) {
      // The input context specified with |context| is activated.
      context.ui_visibility_tracker()->OnSetContext(show_ui_attributes);
    }
    UpdateCandidate(context, activated ? kNoEvent : kDissociateContext);
    if (activated) {
      // Invalidate the LangBar state cache because the actual state of LangBar
      // can be changed by other IMEs or applications.
      InvalidateLangBarInfoCache();
    }
    UpdateLangBar(context, kImmediate);
    return 0;
  }

  LRESULT OnControl(const UIContext &context, DWORD sub_message, void *data) {
    return 0;
  }

  void OnCompositionFull(const UIContext &context) {}

  void OnSelect(const UIContext &context, bool select, HKL keyboard_layout) {
    if (!select) {
      UninitLangBar();
      return;
    }
    UpdateCandidate(context, kNoEvent);
    UpdateLangBar(context, kImmediate);

    // If the application does not allow the IME to show any UI component,
    // it would be better not to show the set default dialog.
    // We use the visibility of suggest window as a launch condition of
    // SetDefaultDialog.
    if (context.ui_visibility_tracker()->IsSuggestWindowVisible()) {
      if (!IsProcessSandboxed() && RunLevel::IsValidClientRunLevel()) {
        absl::call_once(g_launch_set_default_dialog, &LaunchSetDefaultDialog);
      }
    }
  }

  LRESULT OnRequest(const UIContext &context, WPARAM wParam, LPARAM lParam) {
    return 0;
  }

  LRESULT OnSessionCommand(HIMC himc,
                           commands::SessionCommand::CommandType command_type,
                           LPARAM lParam) {
    if (himc == nullptr) {
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
      const int32_t mozc_candidate_id = static_cast<int32_t>(lParam);
      int candidate_index = 0;
      {
        UIContext context(himc);
        commands::Output output;
        if (!context.GetLastOutput(&output)) {
          return 0;
        }
        if (!OutputUtil::GetCandidateIndexById(output, mozc_candidate_id,
                                               &candidate_index)) {
          return 0;
        }
      }  // release |context|.

      constexpr int kCandidateWindowIndex = 0;
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

  LRESULT UIMessageProc(const UIContext &context, UINT message, WPARAM wParam,
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
        UpdateLangBar(context, kDeferred);
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

  void OnTimer(WPARAM event_id) {
    switch (event_id) {
      case kDeferredLangBarUpdateTimerId:
        OnDeferredUpdateLangBar();
        break;
    }
  }

 private:
  // Timer for the langbar update.
  static const UINT_PTR kDeferredLangBarUpdateTimerId = 2;
  static constexpr DWORD kLangBarUpdateDelayMilliSec = 50;

  enum LangBarUpdateMode {
    kDeferred = 0,
    kImmediate = 1,
  };

  enum IndicatorEventType {
    kNoEvent,
    kMoveFocusedWindow,
    kDissociateContext,
  };

  struct LangBarInfo {
    LangBarInfo() : enabled(false), mode(commands::DIRECT) {}

    bool enabled;
    commands::CompositionMode mode;
  };

  // Constructs RendererCommand based on various parameters in the input
  // context.  This implementation is very experimental, should be revised.
  void UpdateCandidate(const UIContext &context,
                       IndicatorEventType indicator_event_type) {
    if (indicator_event_type != kNoEvent) {
      // We need to send UI event to the renderer anyway.
      // So the returned value from |tracker| will be ignored.
      IndicatorVisibilityTracker *tracker =
          context.indicator_visibility_tracker();
      if (tracker != nullptr) {
        switch (indicator_event_type) {
          case kMoveFocusedWindow:
            tracker->OnMoveFocusedWindow();
            break;
          case kDissociateContext:
            tracker->OnDissociateContext();
            break;
        }
      }
    }

    commands::RendererCommand command;
    command.set_type(commands::RendererCommand::UPDATE);
    command.set_visible(false);
    UpdateCommand(context, hwnd_, *context.ui_visibility_tracker(), &command);
    Win32RendererClient::OnUpdated(command);
  }

  void UpdateIndicator(const UIContext &context) {
    IndicatorVisibilityTracker *tracker =
        context.indicator_visibility_tracker();
    if (tracker == nullptr) {
      return;
    }
    const IndicatorVisibilityTracker::Action action =
        tracker->OnChangeInputMode();
    if (action == IndicatorVisibilityTracker::kUpdateUI) {
      commands::RendererCommand command;
      command.set_type(commands::RendererCommand::UPDATE);
      // Initialize as invisible just in case. Basically this flag will
      // be set to true in UpdateCommand.
      command.set_visible(false);
      UpdateCommand(context, hwnd_, *context.ui_visibility_tracker(), &command);
      Win32RendererClient::OnUpdated(command);
    }
  }

  bool UpdateLangBar(const UIContext &context, LangBarUpdateMode update_mode) {
    bool enabled = false;
    commands::CompositionMode mode = commands::DIRECT;

    if (context.IsEmpty()) {
      enabled = false;
      mode = commands::DIRECT;
    } else if (!context.GetOpenStatus()) {
      // Closed
      enabled = true;
      mode = commands::DIRECT;
    } else {
      DWORD imm32_visible_mode = 0;
      if (!context.GetVisibleConversionMode(&imm32_visible_mode)) {
        return false;
      }
      commands::CompositionMode mozc_mode = commands::HIRAGANA;
      if (!win32::ConversionModeUtil::ToMozcMode(imm32_visible_mode,
                                                 &mozc_mode)) {
        return false;
      }
      enabled = true;
      mode = mozc_mode;
    }

    switch (update_mode) {
      case kDeferred:
        SetDeferredLangBarUpdate(true, mode);
        break;
      case kImmediate:
        UpdateLangBarAndCancelUpdateTimer(enabled, mode);
        break;
      default:
        DCHECK(false) << "Unknown mode: " << update_mode;
        return false;
    }

    return true;
  }

  void InvalidateLangBarInfoCache() { langbar_info_cache_.reset(); }

  void SetDeferredLangBarUpdate(bool enabled, commands::CompositionMode mode) {
    CancelDeferredLangBarUpdateIfExists();

    deferred_langbar_update_request_.enabled = enabled;
    deferred_langbar_update_request_.mode = mode;
    const auto result = ::SetTimer(hwnd_, kDeferredLangBarUpdateTimerId,
                                   kLangBarUpdateDelayMilliSec, nullptr);
    if (result != 0) {
      has_pending_langbar_update_ = true;
    }
  }

  void CancelDeferredLangBarUpdateIfExists() {
    if (!has_pending_langbar_update_) {
      return;
    }
    ::KillTimer(hwnd_, kDeferredLangBarUpdateTimerId);
    has_pending_langbar_update_ = false;
  }

  void UpdateLangBarAndCancelUpdateTimer(bool enabled,
                                         commands::CompositionMode mode) {
    CancelDeferredLangBarUpdateIfExists();

    // Ensure the LangBar is initialized.
    language_bar_->InitLanguageBar(langbar_callback_);

    const bool is_redundant_call = (langbar_info_cache_.get() != nullptr &&
                                    langbar_info_cache_->enabled == enabled &&
                                    langbar_info_cache_->mode == mode);

    if (!is_redundant_call) {
      language_bar_->SetLangbarMenuEnabled(enabled);
      language_bar_->UpdateLangbarMenu(mode);
    }

    if (langbar_info_cache_.get() == nullptr) {
      langbar_info_cache_.reset(new LangBarInfo);
    }
    langbar_info_cache_->enabled = enabled;
    langbar_info_cache_->mode = mode;
  }

  void OnDeferredUpdateLangBar() {
    UpdateLangBarAndCancelUpdateTimer(deferred_langbar_update_request_.enabled,
                                      deferred_langbar_update_request_.mode);
  }

  HWND hwnd_;
  // TODO(yukawa): Make a wrapper class to encapsulate LangBar implementation
  // including cache mechanism to reduce API calls.
  std::unique_ptr<LanguageBar> language_bar_;
  LangBarCallbackImpl *langbar_callback_;
  // Represents the LangBarInfo that should be set to the LangBar when deferred
  // timer is fired.
  LangBarInfo deferred_langbar_update_request_;
  // True while the deferred timer that updates the LangBar is scheduled.
  bool has_pending_langbar_update_;
  // Represents the last LangBarInfo that is set to the LangBar. nullpter if
  // no cached data is available.
  std::unique_ptr<LangBarInfo> langbar_info_cache_;
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
//   In this case, messages from [1] to [3] can be removed and start
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
//   In this case, messages from [1] to [2] can be removed and start
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
MSG AggregateRendererCallbackMessage(HWND hwnd, UINT private_message,
                                     WPARAM wParam, LPARAM lParam) {
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
      // Something wrong.
      // give up aggregating the message.
      return current_msg;
    }

    current_msg = next_msg;
    const mozc::commands::SessionCommand::CommandType command_type =
        static_cast<mozc::commands::SessionCommand::CommandType>(
            current_msg.wParam);

    // If this is a HIGHLIGHT_CANDIDATE message, resume aggregation.
    if (command_type == mozc::commands::SessionCommand::HIGHLIGHT_CANDIDATE) {
      continue;
    }

    return current_msg;
  }
}

LRESULT WINAPI UIWindowProc(HWND hwnd, UINT message, WPARAM wParam,
                            LPARAM lParam) {
  DANGLING_CALLBACK_GUARD(0);

  const bool is_ui_message =
      (::ImmIsUIMessage(nullptr, message, wParam, lParam) != FALSE);

  // Create UI window object and associate it to the window.
  if (message == WM_NCCREATE) {
    if (mozc::win32::IsInLockdownMode() ||
        !mozc::RunLevel::IsValidClientRunLevel()) {
      // Clear Kana-lock state not to prevent users from typing their
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

    DefaultUIWindow *ui_window = new DefaultUIWindow(hwnd);

    ::SetWindowLongPtr(hwnd, IMMGWLP_PRIVATE,
                       reinterpret_cast<LONG_PTR>(ui_window));

    Singleton<PrivateRendererMessageInitializer>::get()->Initialize(hwnd);
  }
  // Retrieves UI window object from the private area.
  DefaultUIWindow *ui_window = reinterpret_cast<DefaultUIWindow *>(
      ::GetWindowLongPtrW(hwnd, IMMGWLP_PRIVATE));

  if (ui_window == nullptr) {
    if (is_ui_message) {
      return 0;
    } else {
      return ::DefWindowProc(hwnd, message, wParam, lParam);
    }
  }

  LRESULT result = 0;
  const bool is_renderer_message =
      Singleton<PrivateRendererMessageInitializer>::get()
          ->IsPrivateRendererMessage(message);

  bool is_handled = false;
  if (is_ui_message) {
    const HIMC himc = GetSafeHIMC(hwnd);
    result = ui_window->UIMessageProc(UIContext(himc), message, wParam, lParam);
    is_handled = true;
  } else if (is_renderer_message) {
    const MSG renderer_msg =
        AggregateRendererCallbackMessage(hwnd, message, wParam, lParam);
    const mozc::commands::SessionCommand::CommandType command_type =
        static_cast<mozc::commands::SessionCommand::CommandType>(
            renderer_msg.wParam);
    const HIMC himc = GetSafeHIMC(hwnd);
    result =
        ui_window->OnSessionCommand(himc, command_type, renderer_msg.lParam);
    is_handled = true;
  } else if (message == WM_DESTROY) {
    // Ensure the LangBar is uninitialized.
    ui_window->UninitLangBar();
  } else if (message == WM_NCDESTROY) {
    Win32RendererClient::OnUIThreadUninitialized();

    // Delete UI window object if the window is destroyed.
    ::SetWindowLongPtr(hwnd, IMMGWLP_PRIVATE, 0);
    delete ui_window;
  } else if (message == WM_TIMER) {
    ui_window->OnTimer(wParam);
    // In order to reduce the potential risk of shatter attack, we don't want
    // to pass the WM_TIMER to ::DefWindowProc.
    is_handled = true;
  }
  if (!is_handled) {
    result = ::DefWindowProcW(hwnd, message, wParam, lParam);
    is_handled = true;
  }
  return result;
}

}  // namespace

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
    return false;
  }

  Win32RendererClient::OnModuleLoaded(module_handle);
  return true;
}

void UIWindowManager::OnDllProcessDetach(HINSTANCE module_handle,
                                         bool process_shutdown) {
  if (::UnregisterClass(kIMEUIWndClassName, module_handle) == 0) {
    // Sometimes the IME DLL is unloaded before all the UI message windows
    // which belong to the DLL are destroyed.  In such a situation, we cannot
    // unregister window class. See b/4271156.
  }
  // This flag is used to inactivate out DefWindowProc and any other callbacks
  // to avoid further problems.
  g_module_unloaded = true;
  Win32RendererClient::OnModuleUnloaded();
}

}  // namespace win32
}  // namespace mozc
