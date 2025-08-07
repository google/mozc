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

#include "win32/tip/tip_text_service.h"

#include <ctffunc.h>
#include <ime.h>
#include <msctf.h>
#include <objbase.h>
#include <wil/com.h>
#include <wil/result_macros.h>
#include <windows.h>

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/base/casts.h"
#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/log/log.h"
#include "base/const.h"
#include "base/process.h"
#include "base/update_util.h"
#include "base/win32/com.h"
#include "base/win32/com_implements.h"
#include "base/win32/hresult.h"
#include "base/win32/hresultor.h"
#include "base/win32/win_util.h"
#include "protocol/commands.pb.h"
#include "win32/base/win32_window_util.h"
#include "win32/tip/tip_display_attributes.h"
#include "win32/tip/tip_dll_module.h"
#include "win32/tip/tip_edit_session.h"
#include "win32/tip/tip_edit_session_impl.h"
#include "win32/tip/tip_enum_display_attributes.h"
#include "win32/tip/tip_input_mode_manager.h"
#include "win32/tip/tip_keyevent_handler.h"
#include "win32/tip/tip_lang_bar.h"
#include "win32/tip/tip_lang_bar_callback.h"
#include "win32/tip/tip_preferred_touch_keyboard.h"
#include "win32/tip/tip_private_context.h"
#include "win32/tip/tip_reconvert_function.h"
#include "win32/tip/tip_resource.h"
#include "win32/tip/tip_status.h"
#include "win32/tip/tip_thread_context.h"
#include "win32/tip/tip_ui_handler.h"

namespace mozc {
namespace win32 {

template <>
bool IsIIDOf<ITfTextInputProcessorEx>(REFIID riid) {
  return IsIIDOf<ITfTextInputProcessorEx, ITfTextInputProcessor>(riid);
}

// Do not respond to QueryInterface calls for internal interfaces and for now.
// TODO(yuryu): Give them a UUID or stop deriving from IUnknown.
template <>
bool IsIIDOf<tsf::TipTextService>(REFIID riid) {
  return false;
}

template <>
bool IsIIDOf<tsf::TipLangBarCallback>(REFIID riid) {
  return false;
}

namespace tsf {
namespace {

// Represents the module handle of this module.
volatile HMODULE g_module = nullptr;

// True if the DLL received DLL_PROCESS_DETACH notification.
volatile bool g_module_unloaded = false;

// Thread Local Storage (TLS) index to specify the current UI thread is
// initialized or not. if ::GetTlsValue(g_tls_index) returns non-zero
// value, the current thread is initialized.
volatile DWORD g_tls_index = TLS_OUT_OF_INDEXES;

constexpr UINT kUpdateUIMessage = WM_USER;

#ifdef GOOGLE_JAPANESE_INPUT_BUILD

constexpr char kHelpUrl[] = "http://www.google.com/support/ime/japanese";
constexpr wchar_t kTaskWindowClassName[] =
    L"Google Japanese Input Task Message Window";

// {67526BED-E4BE-47CA-97F8-3C84D5B408DA}
constexpr GUID kTipPreservedKey_Kanji = {
    0x67526bed,
    0xe4be,
    0x47ca,
    {0x97, 0xf8, 0x3c, 0x84, 0xd5, 0xb4, 0x08, 0xda}};

// {B62565AA-288A-432B-B517-EC333E0F99F3}
constexpr GUID kTipPreservedKey_F10 = {
    0xb62565aa,
    0x288a,
    0x432b,
    {0xb5, 0x17, 0xec, 0x33, 0x3e, 0xf, 0x99, 0xf3}};

// {CF6E26FB-1A11-4D81-BD92-52FA852A42EB}
constexpr GUID kTipPreservedKey_Romaji = {
    0xcf6e26fb,
    0x1a11,
    0x4d81,
    {0xbd, 0x92, 0x52, 0xfa, 0x85, 0x2a, 0x42, 0xeb}};

// {EEBABC50-7FEC-4A08-9E1D-0BEF628B5F0E}
constexpr GUID kTipFunctionProvider = {
    0xeebabc50,
    0x7fec,
    0x4a08,
    {0x9e, 0x1d, 0xb, 0xef, 0x62, 0x8b, 0x5f, 0x0e}};

#else  // GOOGLE_JAPANESE_INPUT_BUILD

constexpr char kHelpUrl[] = "https://github.com/google/mozc";
constexpr wchar_t kTaskWindowClassName[] =
    L"Mozc Immersive Task Message Window";

// {F16B7D92-84B0-4AC6-A35B-06EA77180A18}
constexpr GUID kTipPreservedKey_Kanji = {
    0xf16b7d92,
    0x84b0,
    0x4ac6,
    {0xa3, 0x5b, 0x6, 0xea, 0x77, 0x18, 0x0a, 0x18}};

// {80DAD291-1981-46FA-998D-B84D6C1BA02C}
constexpr GUID kTipPreservedKey_F10 = {
    0x80dad291,
    0x1981,
    0x46fa,
    {0x99, 0x8d, 0xb8, 0x4d, 0x6c, 0x1b, 0xa0, 0x2c}};

// {95571C08-B05A-4ABA-B038-F3DEAE532F91}
constexpr GUID kTipPreservedKey_Romaji = {
    0x95571c08,
    0xb05a,
    0x4aba,
    {0xb0, 0x38, 0xf3, 0xde, 0xae, 0x53, 0x2f, 0x91}};

// {ECFB2528-E7D2-4CA0-BBE4-32FE08C148F4}
constexpr GUID kTipFunctionProvider = {
    0xecfb2528,
    0xe7d2,
    0x4ca0,
    {0xbb, 0xe4, 0x32, 0xfe, 0x8, 0xc1, 0x48, 0xf4}};

#endif  // GOOGLE_JAPANESE_INPUT_BUILD

HRESULT SpawnTool(const std::string &command) {
  if (!Process::SpawnMozcProcess(kMozcTool, "--mode=" + command)) {
    return E_FAIL;
  }
  return S_OK;
}

commands::CompositionMode GetMozcMode(TipLangBarCallback::ItemId menu_id) {
  switch (menu_id) {
    case TipLangBarCallback::kDirect:
      return commands::DIRECT;
    case TipLangBarCallback::kHiragana:
      return commands::HIRAGANA;
    case TipLangBarCallback::kFullKatakana:
      return commands::FULL_KATAKANA;
    case TipLangBarCallback::kHalfAlphanumeric:
      return commands::HALF_ASCII;
    case TipLangBarCallback::kFullAlphanumeric:
      return commands::FULL_ASCII;
    case TipLangBarCallback::kHalfKatakana:
      return commands::HALF_KATAKANA;
    default:
      DLOG(FATAL) << "Unexpected item id: " << menu_id;
      // Fall back to DIRECT in release builds.
      return commands::DIRECT;
  }
}

std::string GetMozcToolCommand(TipLangBarCallback::ItemId menu_id) {
  switch (menu_id) {
    case TipLangBarCallback::kProperty:
      // Open the config dialog.
      return "config_dialog";
    case TipLangBarCallback::kDictionary:
      // Open the dictionary tool.
      return "dictionary_tool";
    case TipLangBarCallback::kWordRegister:
      // Open the word register dialog.
      return "word_register_dialog";
    case TipLangBarCallback::kAbout:
      // Open the about dialog.
      return "about_dialog";
    default:
      DLOG(FATAL) << "Unexpected item id: " << menu_id;
      return "";
  }
}

void EnsureKanaLockUnlocked() {
  // Clear Kana-lock state so that users can input their passwords.
  BYTE keyboard_state[256];
  ::GetKeyboardState(keyboard_state);
  keyboard_state[VK_KANA] = 0;
  ::SetKeyboardState(keyboard_state);
}

// A COM-independent way to instantiate Category Manager object.
wil::com_ptr_nothrow<ITfCategoryMgr> GetCategoryMgr() {
  wil::com_ptr_nothrow<ITfCategoryMgr> ptr;
  return SUCCEEDED(TF_CreateCategoryMgr(&ptr)) ? ptr : nullptr;
}

// Custom hash function for wil::com_ptr_nothrow.
template <typename T>
struct ComPtrHash {
  size_t operator()(const wil::com_ptr_nothrow<T> &value) const {
    // The minimum size of COM objects is the pointer to vtable.
    // For instance the last 3 bits are guaranteed to be zero on 64-bit
    // processes.
    constexpr size_t kUnusedBits =
        std::max(std::bit_width(sizeof(void *)), 1) - 1;
    // Compress the data by shifting unused bits.
    return reinterpret_cast<size_t>(value.get()) >> kUnusedBits;
  }
};

// Custom hash function for GUID.
struct GuidHash {
  size_t operator()(const GUID &value) const {
    // Compress the data by shifting unused bits.
    return value.Data1;
  }
};

// Wraps |TipPrivateContext| with a sink cleanup callback.
class PrivateContextWrapper final {
 public:
  PrivateContextWrapper() = delete;
  PrivateContextWrapper(const PrivateContextWrapper &) = delete;
  PrivateContextWrapper &operator=(const PrivateContextWrapper &) = delete;

  explicit PrivateContextWrapper(absl::AnyInvocable<void() &&> sink_cleaner)
      : sink_cleaner_(std::move(sink_cleaner)) {}

  ~PrivateContextWrapper() { std::move(sink_cleaner_)(); }

  TipPrivateContext *get() { return &private_context_; }

 private:
  absl::AnyInvocable<void() &&> sink_cleaner_;
  TipPrivateContext private_context_;
};

// An observer that binds ITfCompositionSink::OnCompositionTerminated callback
// to TipEditSession::OnCompositionTerminated.
class CompositionSinkImpl final : public TipComImplements<ITfCompositionSink> {
 public:
  CompositionSinkImpl(wil::com_ptr_nothrow<TipTextService> text_service,
                      wil::com_ptr_nothrow<ITfContext> context)
      : text_service_(std::move(text_service)), context_(std::move(context)) {}

  // Implements the ITfCompositionSink::OnCompositionTerminated() function.
  // This function is called by Windows when an ongoing composition is
  // terminated by applications.
  STDMETHODIMP OnCompositionTerminated(TfEditCookie write_cookie,
                                       ITfComposition *composition) override {
    return TipEditSessionImpl::OnCompositionTerminated(
        text_service_.get(), context_.get(), composition, write_cookie);
  }

 private:
  wil::com_ptr_nothrow<TipTextService> text_service_;
  wil::com_ptr_nothrow<ITfContext> context_;
};

// Represents preserved keys used by this class.
constexpr wchar_t kTipKeyTilde[] = L"OnOff";
constexpr wchar_t kTipKeyKanji[] = L"Kanji";
constexpr wchar_t kTipKeyF10[] = L"Function 10";
constexpr wchar_t kTipKeyRoman[] = L"Roman";
constexpr wchar_t kTipKeyNoRoman[] = L"NoRoman";

struct PreserveKeyItem {
  const GUID &guid;
  TF_PRESERVEDKEY key;
  DWORD mapped_vkey;
  const wchar_t *description;
  size_t length;
};

constexpr PreserveKeyItem kPreservedKeyItems[] = {
    {kTipPreservedKey_Kanji,
     {VK_OEM_3, TF_MOD_ALT},
     VK_OEM_3,
     &kTipKeyTilde[0],
     std::size(kTipKeyTilde) - 1},
    {kTipPreservedKey_Kanji,
     {VK_KANJI, TF_MOD_IGNORE_ALL_MODIFIER},
     // KeyEventHandler maps VK_KANJI to KeyEvent::NO_SPECIALKEY instead of
     // KeyEvent::KANJI because of an anomaly of IMM32 behavior. So, in TSF
     // mode, we treat VK_KANJI as if it was VK_DBE_DBCSCHAR. See b/7592743 and
     // b/7970379 about what happened.
     VK_DBE_DBCSCHAR,
     &kTipKeyKanji[0],
     std::size(kTipKeyKanji) - 1},
    {kTipPreservedKey_Romaji,
     {VK_DBE_ROMAN, TF_MOD_IGNORE_ALL_MODIFIER},
     VK_DBE_ROMAN,
     &kTipKeyRoman[0],
     std::size(kTipKeyRoman) - 1},
    {kTipPreservedKey_Romaji,
     {VK_DBE_NOROMAN, TF_MOD_IGNORE_ALL_MODIFIER},
     VK_DBE_NOROMAN,
     &kTipKeyNoRoman[0],
     std::size(kTipKeyNoRoman) - 1},
    {kTipPreservedKey_F10,
     {VK_F10, 0},
     VK_F10,
     &kTipKeyF10[0],
     std::size(kTipKeyF10) - 1},
};

class UpdateUiEditSessionImpl final : public TipComImplements<ITfEditSession> {
 public:
  // The ITfEditSession interface method.
  // This function is called back by the TSF thread manager when an edit
  // request is granted.
  STDMETHODIMP DoEditSession(TfEditCookie edit_cookie) override {
    TipUiHandler::Update(text_service_.get(), context_.get(), edit_cookie);
    return S_OK;
  }

  static bool BeginRequest(TipTextService *text_service, ITfContext *context) {
    // When RequestEditSession fails, it does not maintain the reference count.
    // So we need to ensure that AddRef/Release should be called at least once
    // per object.
    wil::com_ptr_nothrow<ITfEditSession> edit_session(
        new UpdateUiEditSessionImpl(text_service, context));

    HRESULT edit_session_result = S_OK;
    const HRESULT result = context->RequestEditSession(
        text_service->GetClientID(), edit_session.get(),
        TF_ES_ASYNCDONTCARE | TF_ES_READ, &edit_session_result);
    return SUCCEEDED(result);
  }

 private:
  UpdateUiEditSessionImpl(wil::com_ptr_nothrow<TipTextService> text_service,
                          wil::com_ptr_nothrow<ITfContext> context)
      : text_service_(std::move(text_service)), context_(std::move(context)) {}

  wil::com_ptr_nothrow<TipTextService> text_service_;
  wil::com_ptr_nothrow<ITfContext> context_;
};

bool RegisterWindowClass(HINSTANCE module_handle, const wchar_t *class_name,
                         WNDPROC window_procedure) {
  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(WNDCLASSEX);
  wc.style = 0;
  wc.lpfnWndProc = window_procedure;
  wc.hInstance = module_handle;
  wc.lpszClassName = class_name;

  const ATOM atom = ::RegisterClassExW(&wc);
  return atom != INVALID_ATOM;
}

class TipTextServiceImpl
    : public TipComImplements<
          ITfTextInputProcessorEx, ITfDisplayAttributeProvider,
          ITfThreadMgrEventSink, ITfThreadFocusSink, ITfTextEditSink,
          ITfTextLayoutSink, ITfKeyEventSink, ITfFnConfigure,
          ITfFunctionProvider, ITfCompartmentEventSink, TipLangBarCallback,
          TipTextService> {
 public:
  TipTextServiceImpl()
      : client_id_(TF_CLIENTID_NULL),
        activate_flags_(0),
        thread_mgr_cookie_(TF_INVALID_COOKIE),
        thread_focus_cookie_(TF_INVALID_COOKIE),
        keyboard_openclose_cookie_(TF_INVALID_COOKIE),
        keyboard_inputmode_conversion_cookie_(TF_INVALID_COOKIE),
        input_attribute_(TF_INVALID_GUIDATOM),
        converted_attribute_(TF_INVALID_GUIDATOM),
        thread_context_(nullptr),
        task_window_handle_(nullptr),
        renderer_callback_window_handle_(nullptr) {}

  static bool OnDllProcessAttach(HMODULE module_handle) {
    if (!RegisterWindowClass(module_handle, kTaskWindowClassName,
                             TaskWindowProc)) {
      return false;
    }

    if (!RegisterWindowClass(module_handle, kMessageReceiverClassName,
                             RendererCallbackWidnowProc)) {
      return false;
    }
    return true;
  }

  static void OnDllProcessDetach(HMODULE module_handle) {
    ::UnregisterClass(kTaskWindowClassName, module_handle);
    ::UnregisterClass(kMessageReceiverClassName, module_handle);
  }

  // ITfTextInputProcessorEx
  STDMETHODIMP Activate(ITfThreadMgr *thread_mgr,
                        TfClientId client_id) override {
    return ActivateEx(thread_mgr, client_id, 0);
  }
  STDMETHODIMP Deactivate() override {
    if (TipDllModule::IsUnloaded()) {
      // Crash report indicates that this method is called after the DLL is
      // unloaded. In such case, we can do nothing safely.
      return S_OK;
    }

    // Stop advising the ITfThreadFocusSink events.
    UninitThreadFocusSink();

    // Unregister the hot keys.
    UninitPreservedKey();

    // Stop advising the ITfCompartmentEventSink events.
    UninitCompartmentEventSink();

    // Stop advising the ITfKeyEvent events.
    UninitKeyEventSink();

    // Remove our button menus from the language bar.
    UninitLanguageBar();

    // Stop advising the ITfFunctionProvider events.
    UninitFunctionProvider();

    // Stop advising the ITfThreadMgrEventSink events.
    UninitThreadManagerEventSink();

    UninitPrivateContexts();

    UninitRendererCallbackWindow();

    UninitTaskWindow();

    // Release the ITfCategoryMgr.
    category_.reset();

    // Release the client ID who communicates with this IME.
    client_id_ = TF_CLIENTID_NULL;

    // Release the ITfThreadMgr object who owns this object.
    thread_mgr_.reset();

    TipUiHandler::OnDeactivate(this);

    thread_context_.reset();
    StorePointerForCurrentThread(nullptr);

    return S_OK;
  }
  STDMETHODIMP ActivateEx(ITfThreadMgr *thread_mgr, TfClientId client_id,
                          DWORD flags) override {
    if (TipDllModule::IsUnloaded()) {
      // Crash report indicates that this method is called after the DLL is
      // unloaded. In such case, we can do nothing safely. b/7915484.
      return S_OK;  // the returned value will be ignored according to the MSDN.
    }
    thread_context_ = std::make_unique<TipThreadContext>();
    StorePointerForCurrentThread(this);

    HRESULT result = E_UNEXPECTED;

    EnsureKanaLockUnlocked();

    // A stack trace reported in http://b/2243760 implies that a
    // call of DestroyWindow API during the Deactivation may invokes another
    // message dispatch, which, in turn, may cause a problematic reentrant
    // activation.
    // There are potential code paths to cause such a reentrance so we
    // return E_UNEXPECTED if |thread_| has been initialized.
    // TODO(yukawa): Fix this problem.
    if (thread_mgr_ != nullptr) {
      LOG(ERROR) << "Recursive Activation found.";
      return E_UNEXPECTED;
    }

    // Copy the given thread manager.
    thread_mgr_ = thread_mgr;
    if (!thread_mgr_) {
      LOG(ERROR) << "Failed to retrieve ITfThreadMgr interface.";
      return E_UNEXPECTED;
    }

    // Copy the given client ID.
    // An IME can identify an application with this ID.
    client_id_ = client_id;

    // Copy the given activation flags.
    activate_flags_ = flags;

    result = InitTaskWindow();
    if (FAILED(result)) {
      LOG(ERROR) << "InitTaskWindow failed: " << result;
      return Deactivate();
    }

    // Do nothing even when we fail to initialize the renderer callback
    // because 1), it is not so critical, and 2) it actually fails in
    // Internet Explorer 10 on Windows 8.
    InitRendererCallbackWindow();

    // Start advising thread events to this object.
    result = InitThreadManagerEventSink();
    if (FAILED(result)) {
      LOG(ERROR) << "InitThreadManagerEventSink failed: " << result;
      return Deactivate();
    }

    // Start advising function provider events to this object.
    result = InitFunctionProvider();
    if (FAILED(result)) {
      LOG(ERROR) << "InitFunctionProvider failed: " << result;
      return Deactivate();
    }

    category_ = GetCategoryMgr();
    if (!category_) {
      LOG(ERROR) << "GetCategoryMgr failed";
      return Deactivate();
    }

    result = InitLanguageBar();
    if (FAILED(result)) {
      LOG(ERROR) << "InitLanguageBar failed: " << result;
      return result;
    }

    // Start advising the keyboard events (ITfKeyEvent) to this object.
    result = InitKeyEventSink();
    if (FAILED(result)) {
      LOG(ERROR) << "InitKeyEventSink failed: " << result;
      return result;
    }

    // Start advising ITfCompartmentEventSink to this object.
    result = InitCompartmentEventSink();
    if (FAILED(result)) {
      LOG(ERROR) << "InitCompartmentEventSink failed: " << result;
      return Deactivate();
    }

    // Register the hot-keys used by this object to Windows.
    result = InitPreservedKey();
    if (FAILED(result)) {
      LOG(ERROR) << "InitPreservedKey failed: " << result;
      return Deactivate();
    }

    // Start advising ITfThreadFocusSink to this object.
    result = InitThreadFocusSink();
    if (FAILED(result)) {
      LOG(ERROR) << "InitThreadFocusSink failed: " << result;
      return Deactivate();
    }

    // Initialize text attributes used by this object.
    result = InitDisplayAttributes();
    if (FAILED(result)) {
      LOG(ERROR) << "InitDisplayAttributes failed: " << result;
      return Deactivate();
    }

    // Write a registry value for usage tracking by Omaha.
    // We ignore the returned value by the function because we should not
    // disturb the application by the result of this function.
    if (!mozc::UpdateUtil::WriteActiveUsageInfo()) {
      LOG(WARNING) << "WriteActiveUsageInfo failed";
    }

    // Copy the initial mode.
    DWORD native_mode = 0;
    if (TipStatus::GetInputModeConversion(thread_mgr, client_id,
                                          &native_mode)) {
      GetThreadContext()->GetInputModeManager()->OnInitialize(
          TipStatus::IsOpen(thread_mgr), native_mode);
    }

    // Emulate document changed event against the current document manager.
    {
      wil::com_ptr_nothrow<ITfDocumentMgr> document_mgr;
      result = thread_mgr_->GetFocus(&document_mgr);
      if (FAILED(result)) {
        return Deactivate();
      }
      if (document_mgr != nullptr) {
        wil::com_ptr_nothrow<ITfContext> context;
        result = document_mgr->GetBase(&context);
        if (SUCCEEDED(result)) {
          EnsurePrivateContextExists(context.get());
        }
      }

      TipUiHandler::OnActivate(this);

      result = OnDocumentMgrChanged(document_mgr.get());
      if (FAILED(result)) {
        return Deactivate();
      }
    }

    return result;
  }

  // ITfDisplayAttributeProvider
  STDMETHODIMP
  EnumDisplayAttributeInfo(IEnumTfDisplayAttributeInfo **attributes) override {
    if (attributes == nullptr) {
      return E_INVALIDARG;
    }

    *attributes = MakeComPtr<TipEnumDisplayAttributes>().detach();
    return S_OK;
  }
  STDMETHODIMP GetDisplayAttributeInfo(
      REFGUID guid, ITfDisplayAttributeInfo **attribute) override {
    if (attribute == nullptr) {
      return E_INVALIDARG;
    }

    // Compare the given GUID with known ones and creates a new instance of the
    // specified display attribute.
    if (::IsEqualGUID(guid, TipDisplayAttributeInput::guid())) {
      *attribute = MakeComPtr<TipDisplayAttributeInput>().detach();
    } else if (::IsEqualGUID(guid, TipDisplayAttributeConverted::guid())) {
      *attribute = MakeComPtr<TipDisplayAttributeConverted>().detach();
    } else {
      *attribute = nullptr;
      return E_INVALIDARG;
    }

    return S_OK;
  }

  // ITfThreadMgrEventSink
  STDMETHODIMP
  OnInitDocumentMgr(ITfDocumentMgr *document) override {
    // In order to defer the initialization timing of TipPrivateContext,
    // we won't call OnDocumentMgrChanged against |document| here.
    return S_OK;
  }
  STDMETHODIMP
  OnUninitDocumentMgr(ITfDocumentMgr *document) override {
    // Usually |document| no longer has any context here: all the contexts are
    // likely to be destroyed through ITfThreadMgrEventSink::OnPushContext.
    // We enumerate remaining contexts just in case.

    if (!document) {
      return E_INVALIDARG;
    }

    wil::com_ptr_nothrow<IEnumTfContexts> enum_context;
    RETURN_IF_FAILED_HRESULT(document->EnumContexts(&enum_context));
    while (true) {
      wil::com_ptr_nothrow<ITfContext> context;
      ULONG fetched = 0;
      const HRESULT hr = enum_context->Next(1, &context, &fetched);
      if (FAILED(hr)) {
        return hr;
      }
      if (hr == S_FALSE || fetched == 0) {
        break;
      }
      RemovePrivateContextIfExists(context.get());
    }

    return S_OK;
  }
  STDMETHODIMP OnSetFocus(ITfDocumentMgr *focused,
                          ITfDocumentMgr *previous) override {
    GetThreadContext()->IncrementFocusRevision();
    OnDocumentMgrChanged(focused);
    return S_OK;
  }
  STDMETHODIMP OnPushContext(ITfContext *context) override {
    EnsurePrivateContextExists(context);
    return S_OK;
  }
  STDMETHODIMP OnPopContext(ITfContext *context) override {
    RemovePrivateContextIfExists(context);
    return S_OK;
  }

  // ITfThreadFocusSink
  STDMETHODIMP OnSetThreadFocus() override {
    EnsureKanaLockUnlocked();

    // A temporary workaround for b/24793812.  When previous attempt to
    // establish conection failed, retry again as if this was the first attempt.
    // TODO(yukawa): We should give up if this fails a number of times.
    if (WinUtil::IsProcessSandboxed()) {
      auto *private_context = GetFocusedPrivateContext();
      if (private_context != nullptr) {
        private_context->EnsureInitialized();
      }
    }

    // While ITfThreadMgrEventSink::OnSetFocus notifies the logical focus inside
    // the application, ITfThreadFocusSink notifies the OS-level keyboard focus
    // events. In both cases, Mozc's UI visibility should be updated.
    wil::com_ptr_nothrow<ITfDocumentMgr> document_manager;
    if (FAILED(thread_mgr_->GetFocus(&document_manager))) {
      return S_OK;
    }
    if (!document_manager) {
      return S_OK;
    }
    TipUiHandler::OnFocusChange(this, document_manager.get());
    return S_OK;
  }
  STDMETHODIMP OnKillThreadFocus() override {
    // See the comment in OnSetThreadFocus().
    TipUiHandler::OnFocusChange(this, nullptr);
    return S_OK;
  }

  // ITfTextEditSink
  STDMETHODIMP OnEndEdit(ITfContext *context, TfEditCookie edit_cookie,
                         ITfEditRecord *edit_record) override {
    return TipEditSessionImpl::OnEndEdit(this, context, edit_cookie,
                                         edit_record);
  }

  STDMETHODIMP
  OnLayoutChange(ITfContext *context, TfLayoutCode layout_code,
                 ITfContextView *context_view) override {
    TipEditSession::OnLayoutChangedAsync(this, context);
    return S_OK;
  }

  // ITfKeyEventSink
  STDMETHODIMP OnSetFocus(BOOL foreground) override { return S_OK; }
  STDMETHODIMP OnTestKeyDown(ITfContext *context, WPARAM wparam, LPARAM lparam,
                             BOOL *eaten) override {
    BOOL dummy_eaten = FALSE;
    if (eaten == nullptr) {
      eaten = &dummy_eaten;
    }
    *eaten = FALSE;
    return TipKeyeventHandler::OnTestKeyDown(this, context, wparam, lparam,
                                             eaten);
  }

  // ITfKeyEventSink
  STDMETHODIMP OnTestKeyUp(ITfContext *context, WPARAM wparam, LPARAM lparam,
                           BOOL *eaten) override {
    BOOL dummy_eaten = FALSE;
    if (eaten == nullptr) {
      eaten = &dummy_eaten;
    }
    *eaten = FALSE;
    return TipKeyeventHandler::OnTestKeyUp(this, context, wparam, lparam,
                                           eaten);
  }
  STDMETHODIMP OnKeyDown(ITfContext *context, WPARAM wparam, LPARAM lparam,
                         BOOL *eaten) override {
    BOOL dummy_eaten = FALSE;
    if (eaten == nullptr) {
      eaten = &dummy_eaten;
    }
    *eaten = FALSE;
    return TipKeyeventHandler::OnKeyDown(this, context, wparam, lparam, eaten);
  }
  STDMETHODIMP OnKeyUp(ITfContext *context, WPARAM wparam, LPARAM lparam,
                       BOOL *eaten) override {
    BOOL dummy_eaten = FALSE;
    if (eaten == nullptr) {
      eaten = &dummy_eaten;
    }
    *eaten = FALSE;
    return TipKeyeventHandler::OnKeyUp(this, context, wparam, lparam, eaten);
  }
  STDMETHODIMP OnPreservedKey(ITfContext *context, REFGUID guid,
                              BOOL *eaten) override {
    HRESULT result = S_OK;
    BOOL dummy_eaten = FALSE;
    if (eaten == nullptr) {
      eaten = &dummy_eaten;
    }
    *eaten = FALSE;
    const auto it = preserved_key_map_.find(guid);
    if (it == preserved_key_map_.end()) {
      return result;
    }
    const UINT vk = it->second;
    const UINT alt_down = (::GetKeyState(VK_MENU) & 0x8000) != 0 ? 1 : 0;
    const UINT scan_code = ::MapVirtualKey(VK_F10, 0);
    const UINT lparam = (alt_down << 29) | (scan_code << 16) | 1;
    result = TipKeyeventHandler::OnKeyDown(this, context, vk, lparam, eaten);
    if (*eaten == FALSE && vk == VK_F10) {
      // Special treatment for F10:
      // Setting FALSE to |*eaten| is not enough when F10 key is handled by the
      // application. So here manually compose WM_SYSKEYDOWN message to emulate
      // F10 key.
      // http://msdn.microsoft.com/en-us/library/ms646286.aspx
      ::PostMessage(::GetFocus(), WM_SYSKEYDOWN, VK_F10, lparam);
    }
    return result;
  }

  // ITfFnConfigure
  STDMETHODIMP GetDisplayName(BSTR *name) override {
    if (name == nullptr) {
      return E_INVALIDARG;
    }
    *name = ::SysAllocString(kConfigurationDisplayname);
    return (*name != nullptr) ? S_OK : E_FAIL;
  }
  STDMETHODIMP Show(HWND parent, LANGID langid, REFGUID profile) override {
    return SpawnTool("config_dialog");
  }

  // ITfFunctionProvider
  STDMETHODIMP GetType(GUID *guid) override {
    if (guid == nullptr) {
      return E_INVALIDARG;
    }
    *guid = kTipFunctionProvider;
    return S_OK;
  }
  STDMETHODIMP GetDescription(BSTR *description) override {
    if (description == nullptr) {
      return E_INVALIDARG;
    }
    *description = SysAllocString(L"");
    return *description ? S_OK : E_FAIL;
  }
  STDMETHODIMP GetFunction(const GUID &guid, const IID &iid,
                           IUnknown **unknown) override {
    if (unknown == nullptr) {
      return E_INVALIDARG;
    }
    if (::IsEqualGUID(IID_ITfFnReconversion, iid)) {
      *unknown = MakeComPtr<TipReconvertFunction>(this).detach();
    } else if (::IsEqualGUID(TipPreferredTouchKeyboard::GetIID(), iid)) {
      *unknown = TipPreferredTouchKeyboard::New().detach();
    } else {
      return E_NOINTERFACE;
    }
    return *unknown ? S_OK : E_OUTOFMEMORY;
  }

  // ITfCompartmentEventSink
  STDMETHODIMP OnChange(const GUID &guid) override {
    if (!thread_mgr_) {
      return E_FAIL;
    }

    if (::IsEqualGUID(guid, GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION)) {
      TipEditSession::OnModeChangedAsync(this);
    } else if (::IsEqualGUID(guid, GUID_COMPARTMENT_KEYBOARD_OPENCLOSE)) {
      TipEditSession::OnOpenCloseChangedAsync(this);
    }
    return S_OK;
  }

  // TipLangBarCallback
  STDMETHODIMP OnMenuSelect(ItemId menu_id) override {
    switch (menu_id) {
      case TipLangBarCallback::kDirect:
      case TipLangBarCallback::kHiragana:
      case TipLangBarCallback::kFullKatakana:
      case TipLangBarCallback::kHalfAlphanumeric:
      case TipLangBarCallback::kFullAlphanumeric:
      case TipLangBarCallback::kHalfKatakana: {
        const commands::CompositionMode mozc_mode = GetMozcMode(menu_id);
        return TipEditSession::SwitchInputModeAsync(this, mozc_mode);
      }
      case TipLangBarCallback::kProperty:
      case TipLangBarCallback::kDictionary:
      case TipLangBarCallback::kWordRegister:
      case TipLangBarCallback::kAbout:
        return SpawnTool(GetMozcToolCommand(menu_id));
      case TipLangBarCallback::kHelp:
        // Open the about dialog.
        return Process::OpenBrowser(kHelpUrl) ? S_OK : E_FAIL;
      default:
        return S_OK;
    }
  }
  STDMETHODIMP OnItemClick(const wchar_t *description) override {
    // Change input mode to be consistent with MSIME 2012 on Windows 8.
    const bool open =
        thread_context_->GetInputModeManager()->GetEffectiveOpenClose();
    if (open) {
      return TipStatus::SetIMEOpen(thread_mgr_.get(), client_id_, false)
                 ? S_OK
                 : E_FAIL;
    }

    // Like MSIME 2012, switch to Hiragana mode when the LangBar button is
    // clicked.
    return TipEditSession::SwitchInputModeAsync(this, commands::HIRAGANA);
  }

  // TipTextService
  TfClientId GetClientID() const override { return client_id_; }
  ITfThreadMgr *GetThreadManager() const override { return thread_mgr_.get(); }
  TfGuidAtom input_attribute() const override { return input_attribute_; }
  TfGuidAtom converted_attribute() const override {
    return converted_attribute_;
  }
  HWND renderer_callback_window_handle() const override {
    return renderer_callback_window_handle_;
  }

  wil::com_ptr_nothrow<ITfCompositionSink> CreateCompositionSink(
      ITfContext *context) override {
    return MakeComPtr<CompositionSinkImpl>(this, context);
  }
  TipPrivateContext *GetPrivateContext(ITfContext *context) override {
    if (context == nullptr) {
      return nullptr;
    }
    const auto it = private_context_map_.find(context);
    if (it == private_context_map_.end()) {
      return nullptr;
    }
    return it->second->get();
  }
  TipThreadContext *GetThreadContext() override {
    return thread_context_.get();
  }
  void PostUIUpdateMessage() override {
    if (!::IsWindow(task_window_handle_)) {
      return;
    }
    PostMessageW(task_window_handle_, kUpdateUIMessage, 0, 0);
  }

  void UpdateLangbar(bool enabled, uint32_t mozc_mode) override {
    langbar_.UpdateMenu(enabled, mozc_mode);
  }

  bool IsLangbarInitialized() const override {
    return langbar_.IsInitialized();
  }

 private:
  // Following functions are private utilities.
  static void StorePointerForCurrentThread(TipTextServiceImpl *impl) {
    if (g_module_unloaded) {
      return;
    }
    if (g_tls_index == TLS_OUT_OF_INDEXES) {
      return;
    }
    ::TlsSetValue(g_tls_index, impl);
  }
  static TipTextServiceImpl *Self() {
    if (g_module_unloaded) {
      return nullptr;
    }
    if (g_tls_index == TLS_OUT_OF_INDEXES) {
      return nullptr;
    }
    return static_cast<TipTextServiceImpl *>(::TlsGetValue(g_tls_index));
  }

  HRESULT OnDocumentMgrChanged(ITfDocumentMgr *document_mgr) {
    // nullptr document is not an error.
    if (document_mgr != nullptr) {
      wil::com_ptr_nothrow<ITfContext> context;
      RETURN_IF_FAILED_HRESULT(document_mgr->GetTop(&context));
      EnsurePrivateContextExists(context.get());
    }
    TipUiHandler::OnDocumentMgrChanged(this, document_mgr);
    TipEditSession::OnSetFocusAsync(this, document_mgr);
    return S_OK;
  }

  void EnsurePrivateContextExists(ITfContext *context) {
    if (context == nullptr) {
      // Do not care about nullptr context.
      return;
    }
    if (private_context_map_.contains(context)) {
      return;
    }

    // If this |context| has not been registered, create our own private data
    // then associate it with |context|.
    auto source = ComQuery<ITfSource>(context);
    if (!source) {
      // In general this should not happen.
      // Register private context without sink-cleanup callback.
      private_context_map_.emplace(
          context, std::make_unique<PrivateContextWrapper>([]() {}));
      return;
    }

    DWORD text_edit_sink_cookie = TF_INVALID_COOKIE;
    DWORD text_layout_sink_cookie = TF_INVALID_COOKIE;
    if (FAILED(source->AdviseSink(IID_ITfTextEditSink,
                                  absl::implicit_cast<ITfTextEditSink *>(this),
                                  &text_edit_sink_cookie))) {
      // In general this should not happen.
      text_edit_sink_cookie = TF_INVALID_COOKIE;
    }
    if (FAILED(
            source->AdviseSink(IID_ITfTextLayoutSink,
                               absl::implicit_cast<ITfTextLayoutSink *>(this),
                               &text_layout_sink_cookie))) {
      // In general this should not happen.
      text_layout_sink_cookie = TF_INVALID_COOKIE;
    }

    // Register private context with sink-cleanup callback.
    private_context_map_.emplace(
        context, std::make_unique<PrivateContextWrapper>(
                     [=, source = std::move(source)]() {
                       if (text_edit_sink_cookie != TF_INVALID_COOKIE) {
                         source->UnadviseSink(text_edit_sink_cookie);
                       }
                       if (text_layout_sink_cookie != TF_INVALID_COOKIE) {
                         source->UnadviseSink(text_layout_sink_cookie);
                       }
                     }));
  }

  void RemovePrivateContextIfExists(ITfContext *context) {
    private_context_map_.erase(context);
  }

  void UninitPrivateContexts() { private_context_map_.clear(); }

  TipPrivateContext *GetFocusedPrivateContext() {
    wil::com_ptr_nothrow<ITfDocumentMgr> focused_document;
    if (FAILED(thread_mgr_->GetFocus(&focused_document))) {
      return nullptr;
    }
    if (!focused_document) {
      return nullptr;
    }
    wil::com_ptr_nothrow<ITfContext> current_context;
    if (FAILED(focused_document->GetTop(&current_context))) {
      return nullptr;
    }
    return GetPrivateContext(current_context.get());
  }

  HRESULT InitThreadManagerEventSink() {
    // Retrieve the event source for this thread and start advising the
    // ITfThreadMgrEventSink events to this object, i.e. register this object
    // as a listener for the TSF thread events.
    ASSIGN_OR_RETURN_HRESULT(auto source, ComQueryHR<ITfSource>(thread_mgr_));

    const HRESULT result = source->AdviseSink(
        IID_ITfThreadMgrEventSink, static_cast<ITfThreadMgrEventSink *>(this),
        &thread_mgr_cookie_);

    if (FAILED(result)) {
      thread_mgr_cookie_ = TF_INVALID_COOKIE;
    }

    return result;
  }

  HRESULT UninitThreadManagerEventSink() {
    // If we have started advising the TSF thread events, retrieve the event
    // source for the events and stop advising them.
    if (thread_mgr_cookie_ == TF_INVALID_COOKIE) {
      return S_OK;
    }

    if (thread_mgr_ == nullptr) {
      return E_FAIL;
    }

    ASSIGN_OR_RETURN_HRESULT(auto source, ComQueryHR<ITfSource>(thread_mgr_));
    const HRESULT result = source->UnadviseSink(thread_mgr_cookie_);
    thread_mgr_cookie_ = TF_INVALID_COOKIE;
    return result;
  }

  HRESULT InitLanguageBar() { return langbar_.InitLangBar(this); }

  HRESULT UninitLanguageBar() { return langbar_.UninitLangBar(); }

  HRESULT InitKeyEventSink() {
    if (thread_mgr_ == nullptr) {
      return E_FAIL;
    }

    ASSIGN_OR_RETURN_HRESULT(auto keystroke,
                             ComQueryHR<ITfKeystrokeMgr>(thread_mgr_));
    return keystroke->AdviseKeyEventSink(
        client_id_, static_cast<ITfKeyEventSink *>(this), TRUE);
  }

  HRESULT UninitKeyEventSink() {
    if (thread_mgr_ == nullptr) {
      return E_FAIL;
    }

    ASSIGN_OR_RETURN_HRESULT(auto keystroke,
                             ComQueryHR<ITfKeystrokeMgr>(thread_mgr_));
    return keystroke->UnadviseKeyEventSink(client_id_);
  }

  HRESULT InitCompartmentEventSink() {
    ASSIGN_OR_RETURN_HRESULT(auto manager,
                             ComQueryHR<ITfCompartmentMgr>(thread_mgr_));

    RETURN_IF_FAILED_HRESULT(AdviseCompartmentEventSink(
        manager.get(), GUID_COMPARTMENT_KEYBOARD_OPENCLOSE,
        &keyboard_openclose_cookie_));

    return AdviseCompartmentEventSink(
        manager.get(), GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION,
        &keyboard_inputmode_conversion_cookie_);
  }

  HRESULT UninitCompartmentEventSink() {
    if (thread_mgr_ == nullptr) {
      return E_FAIL;
    }

    ASSIGN_OR_RETURN_HRESULT(auto manager,
                             ComQueryHR<ITfCompartmentMgr>(thread_mgr_));

    UnadviseCompartmentEventSink(manager.get(),
                                 GUID_COMPARTMENT_KEYBOARD_OPENCLOSE,
                                 &keyboard_openclose_cookie_);
    UnadviseCompartmentEventSink(manager.get(),
                                 GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION,
                                 &keyboard_inputmode_conversion_cookie_);

    return S_OK;
  }

  HRESULT AdviseCompartmentEventSink(ITfCompartmentMgr *manager, REFGUID guid,
                                     DWORD *cookie) {
    if (manager == nullptr || cookie == nullptr) {
      return E_INVALIDARG;
    }

    wil::com_ptr_nothrow<ITfCompartment> compartment;
    RETURN_IF_FAILED_HRESULT(manager->GetCompartment(guid, &compartment));
    ASSIGN_OR_RETURN_HRESULT(auto source, ComQueryHR<ITfSource>(compartment));
    return source->AdviseSink(IID_ITfCompartmentEventSink,
                              static_cast<ITfCompartmentEventSink *>(this),
                              cookie);
  }

  HRESULT UnadviseCompartmentEventSink(ITfCompartmentMgr *manager, REFGUID guid,
                                       DWORD *cookie) {
    if (manager == nullptr || cookie == nullptr) {
      return E_INVALIDARG;
    }

    if (*cookie == TF_INVALID_COOKIE) {
      return E_UNEXPECTED;
    }

    wil::com_ptr_nothrow<ITfCompartment> compartment;
    RETURN_IF_FAILED(manager->GetCompartment(guid, &compartment));
    ASSIGN_OR_RETURN_HRESULT(auto source, ComQueryHR<ITfSource>(compartment));
    const HRESULT result = source->UnadviseSink(*cookie);
    *cookie = TF_INVALID_COOKIE;
    return result;
  }

  HRESULT InitPreservedKey() {
    HRESULT result = S_OK;

    // Retrieve the keyboard-stroke manager from the thread manager, and
    // add the hot keys defined in the kPreservedKeyItems[] array.
    // A keyboard-stroke manager belongs to a thread manager because Windows
    // allows each thread to have its own keyboard (and Language) settings.
    if (thread_mgr_ == nullptr) {
      return E_FAIL;
    }
    ASSIGN_OR_RETURN_HRESULT(auto keystroke,
                             ComQueryHR<ITfKeystrokeMgr>(thread_mgr_));
    for (const PreserveKeyItem &item : kPreservedKeyItems) {
      // Register a hot key to the keystroke manager.
      result = keystroke->PreserveKey(client_id_, item.guid, &item.key,
                                      item.description, item.length);
      if (SUCCEEDED(result)) {
        preserved_key_map_[item.guid] = item.mapped_vkey;
      }
    }
    return result;
  }

  HRESULT UninitPreservedKey() {
    HRESULT result = S_OK;

    if (thread_mgr_ == nullptr) {
      return E_FAIL;
    }
    ASSIGN_OR_RETURN_HRESULT(auto keystroke,
                             ComQueryHR<ITfKeystrokeMgr>(thread_mgr_));

    for (const PreserveKeyItem &item : kPreservedKeyItems) {
      result = keystroke->UnpreserveKey(item.guid, &item.key);
    }
    preserved_key_map_.clear();

    return result;
  }

  HRESULT InitThreadFocusSink() {
    if (thread_focus_cookie_ != TF_INVALID_COOKIE) {
      return S_OK;
    }
    if (thread_mgr_ == nullptr) {
      return E_FAIL;
    }

    ASSIGN_OR_RETURN_HRESULT(auto source, ComQueryHR<ITfSource>(thread_mgr_));
    const HRESULT result = source->AdviseSink(
        IID_ITfThreadFocusSink, static_cast<ITfThreadFocusSink *>(this),
        &thread_focus_cookie_);
    if (FAILED(result)) {
      thread_focus_cookie_ = TF_INVALID_COOKIE;
    }

    return result;
  }

  HRESULT UninitThreadFocusSink() {
    if (thread_focus_cookie_ == TF_INVALID_COOKIE) {
      return S_OK;
    }
    if (thread_mgr_ == nullptr) {
      return E_FAIL;
    }

    ASSIGN_OR_RETURN_HRESULT(auto source, ComQueryHR<ITfSource>(thread_mgr_));
    const HRESULT result = source->UnadviseSink(thread_focus_cookie_);
    thread_focus_cookie_ = TF_INVALID_COOKIE;

    return result;
  }

  HRESULT InitFunctionProvider() {
    if (thread_mgr_ == nullptr) {
      return E_FAIL;
    }

    ASSIGN_OR_RETURN_HRESULT(auto source,
                             ComQueryHR<ITfSourceSingle>(thread_mgr_));

    return source->AdviseSingleSink(client_id_, IID_ITfFunctionProvider,
                                    static_cast<ITfFunctionProvider *>(this));
  }

  HRESULT UninitFunctionProvider() {
    if (thread_mgr_ == nullptr) {
      return E_FAIL;
    }

    ASSIGN_OR_RETURN_HRESULT(auto source,
                             ComQueryHR<ITfSourceSingle>(thread_mgr_));

    return source->UnadviseSingleSink(client_id_, IID_ITfFunctionProvider);
  }

  HRESULT InitDisplayAttributes() {
    if (!category_) {
      return E_UNEXPECTED;
    }

    // register the display attribute for input strings and the one for
    // converted strings.
    RETURN_IF_FAILED_HRESULT(category_->RegisterGUID(
        TipDisplayAttributeInput::guid(), &input_attribute_));
    return category_->RegisterGUID(TipDisplayAttributeConverted::guid(),
                                   &converted_attribute_);
  }

  HRESULT InitTaskWindow() {
    if (::IsWindow(task_window_handle_)) {
      return S_FALSE;
    }
    task_window_handle_ =
        ::CreateWindowExW(0, kTaskWindowClassName, L"", 0, 0, 0, 0, 0,
                          HWND_MESSAGE, nullptr, g_module, nullptr);
    if (!::IsWindow(task_window_handle_)) {
      return E_FAIL;
    }
    return S_OK;
  }

  HRESULT UninitTaskWindow() {
    if (!::IsWindow(task_window_handle_)) {
      return S_FALSE;
    }
    ::DestroyWindow(task_window_handle_);
    task_window_handle_ = nullptr;
    return S_OK;
  }

  static LRESULT WINAPI TaskWindowProc(HWND window_handle, UINT message,
                                       WPARAM wparam, LPARAM lparam) {
    TipTextServiceImpl *self = Self();
    if (self == nullptr) {
      return ::DefWindowProcW(window_handle, message, wparam, lparam);
    }

    if (window_handle == self->task_window_handle_) {
      if (message == kUpdateUIMessage) {
        self->OnUpdateUI();
        return 0;
      }
    }
    return ::DefWindowProcW(window_handle, message, wparam, lparam);
  }

  void OnUpdateUI() {
    wil::com_ptr_nothrow<ITfDocumentMgr> document_manager;
    if (FAILED(thread_mgr_->GetFocus(&document_manager))) {
      return;
    }
    if (!document_manager) {
      return;
    }
    wil::com_ptr_nothrow<ITfContext> context;
    if (FAILED(document_manager->GetBase(&context))) {
      return;
    }
    if (!context) {
      return;
    }
    UpdateUiEditSessionImpl::BeginRequest(this, context.get());
  }

  HRESULT InitRendererCallbackWindow() {
    if (::IsWindow(renderer_callback_window_handle_)) {
      return S_FALSE;
    }
    renderer_callback_window_handle_ =
        ::CreateWindowExW(0, kMessageReceiverClassName, L"", 0, 0, 0, 0, 0,
                          HWND_MESSAGE, nullptr, g_module, nullptr);
    if (!::IsWindow(renderer_callback_window_handle_)) {
      return E_FAIL;
    }

    // Do not care about thread safety.
    static UINT renderer_callback_message =
        ::RegisterWindowMessage(mozc::kMessageReceiverMessageName);

    if (!WindowUtil::ChangeMessageFilter(renderer_callback_window_handle_,
                                         renderer_callback_message)) {
      ::DestroyWindow(renderer_callback_window_handle_);
      renderer_callback_window_handle_ = nullptr;
      return E_FAIL;
    }
    return S_OK;
  }

  HRESULT UninitRendererCallbackWindow() {
    if (!::IsWindow(renderer_callback_window_handle_)) {
      return S_FALSE;
    }
    ::DestroyWindow(renderer_callback_window_handle_);
    renderer_callback_window_handle_ = nullptr;
    return S_OK;
  }

  static LRESULT WINAPI RendererCallbackWidnowProc(HWND window_handle,
                                                   UINT message, WPARAM wparam,
                                                   LPARAM lparam) {
    TipTextServiceImpl *self = Self();
    if (self == nullptr) {
      return ::DefWindowProcW(window_handle, message, wparam, lparam);
    }

    // Do not care about thread safety.
    static UINT renderer_callback_message =
        ::RegisterWindowMessage(mozc::kMessageReceiverMessageName);
    if (window_handle == self->renderer_callback_window_handle_) {
      if (message == renderer_callback_message) {
        self->OnRendererCallback(wparam, lparam);
        return 0;
      }
    }
    return ::DefWindowProcW(window_handle, message, wparam, lparam);
  }

  void OnRendererCallback(WPARAM wparam, LPARAM lparam) {
    wil::com_ptr_nothrow<ITfDocumentMgr> document_manager;
    if (FAILED(thread_mgr_->GetFocus(&document_manager))) {
      return;
    }
    if (!document_manager) {
      return;
    }
    wil::com_ptr_nothrow<ITfContext> context;
    if (FAILED(document_manager->GetBase(&context))) {
      return;
    }
    if (!context) {
      return;
    }
    TipEditSession::OnRendererCallbackAsync(this, context.get(), wparam,
                                            lparam);
  }

  // Represents the status of the thread manager which owns this IME object.
  wil::com_ptr_nothrow<ITfThreadMgr> thread_mgr_;

  // Represents the ID of the client application using this IME object.
  TfClientId client_id_;

  // Stores the flag passed to ActivateEx.
  DWORD activate_flags_;

  // Represents the cookie ID for the thread manager.
  DWORD thread_mgr_cookie_;

  // The cookie issued for installing ITfThreadFocusSink.
  DWORD thread_focus_cookie_;

  // The cookie issued for installing ITfCompartmentEventSink.
  DWORD keyboard_openclose_cookie_;
  DWORD keyboard_inputmode_conversion_cookie_;

  // The category manager object to register or query a GUID.
  wil::com_ptr_nothrow<ITfCategoryMgr> category_;

  // Represents the display attributes.
  TfGuidAtom input_attribute_;
  TfGuidAtom converted_attribute_;

  // Used for LangBar integration.
  TipLangBar langbar_;

  using PreservedKeyMap = absl::flat_hash_map<GUID, UINT, GuidHash>;
  using PrivateContextMap =
      absl::flat_hash_map<wil::com_ptr_nothrow<ITfContext>,
                          std::unique_ptr<PrivateContextWrapper>,
                          ComPtrHash<ITfContext>>;
  PrivateContextMap private_context_map_;
  PreservedKeyMap preserved_key_map_;
  std::unique_ptr<TipThreadContext> thread_context_;
  HWND task_window_handle_;
  HWND renderer_callback_window_handle_;
};

}  // namespace

wil::com_ptr_nothrow<TipTextService> TipTextServiceFactory::Create() {
  return MakeComPtr<TipTextServiceImpl>();
}

bool TipTextServiceFactory::OnDllProcessAttach(HINSTANCE module_handle,
                                               bool static_loading) {
  g_module = module_handle;
  g_tls_index = ::TlsAlloc();
  if (!TipTextServiceImpl::OnDllProcessAttach(module_handle)) {
    return false;
  }
  return true;
}

void TipTextServiceFactory::OnDllProcessDetach(HINSTANCE module_handle,
                                               bool process_shutdown) {
  TipTextServiceImpl::OnDllProcessDetach(module_handle);

  if (g_tls_index != TLS_OUT_OF_INDEXES) {
    ::TlsFree(g_tls_index);
    g_tls_index = TLS_OUT_OF_INDEXES;
  }
  g_module_unloaded = true;
  g_module = nullptr;
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc
