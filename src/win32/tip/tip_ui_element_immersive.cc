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

#include "win32/tip/tip_ui_element_immersive.h"

#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
// Workaround against KB813540
#include <atlbase_mozc.h>
#include <atlcom.h>
#include <atlapp.h>
#include <atlmisc.h>
#include <atlstr.h>
#include <atlwin.h>
#include <msctf.h>

#include "base/hash_tables.h"
#include "base/util.h"
#include "session/commands.pb.h"
#include "win32/tip/tip_command_handler.h"
#include "win32/tip/tip_composition_util.h"
#include "win32/tip/tip_dll_module.h"
#include "win32/tip/tip_private_context.h"
#include "win32/tip/tip_ref_count.h"
#include "win32/tip/tip_text_service.h"
#include "win32/tip/tip_ui_element_delegate.h"
#include "win32/tip/tip_ui_handler_immersive.h"
#include "win32/tip/tip_ui_renderer_immersive.h"

namespace mozc {
namespace win32 {
namespace tsf {

namespace {

using ATL::CComPtr;
using ATL::CWindow;
using WTL::CBitmap;
using WTL::CBitmapHandle;
using WTL::CDC;
using WTL::CPaintDC;
using WTL::CPoint;
using WTL::CRect;
using WTL::CSize;

using ::mozc::commands::Output;
using ::mozc::commands::Preedit;
typedef ::mozc::commands::Preedit_Segment Segment;
typedef ::mozc::commands::Preedit_Segment::Annotation Annotation;

#ifdef GOOGLE_JAPANESE_INPUT_BUILD

const wchar_t kImmersiveUIWindowClassName[] =
    L"Google Japanese Input Immersive UI Window";

#else

const wchar_t kImmersiveUIWindowClassName[] = L"Mozc Immersive UI Window";

#endif  // GOOGLE_JAPANESE_INPUT_BUILD

// Represents the module handle of this module.
volatile HMODULE g_module = nullptr;

// True if the the DLL received DLL_PROCESS_DETACH notification.
volatile bool g_module_unloaded = false;

// Thread Local Storage (TLS) index to specify the current UI thread is
// initialized or not. If ::GetTlsValue(g_tls_index) returns non-zero
// value, the current thread is initialized.
volatile DWORD g_tls_index = TLS_OUT_OF_INDEXES;

struct RenderingInfo {
  CRect target_rect;
  Output output;
};

size_t GetTargetPos(const commands::Output &output) {
  if (!output.has_candidates() || !output.candidates().has_category()) {
    return 0;
  }
  switch (output.candidates().category()) {
    case commands::PREDICTION:
    case commands::SUGGESTION:
      return 0;
    case commands::CONVERSION: {
        const Preedit &preedit = output.preedit();
        size_t offset = 0;
        for (int i = 0; i < preedit.segment_size(); ++i) {
          const Segment &segment = preedit.segment(i);
          const Annotation &annotation = segment.annotation();
          if (annotation == Segment::HIGHLIGHT) {
            return offset;
          }
          offset += Util::WideCharsLen(segment.value());
        }
        return offset;
      }
    default:
      return 0;
  }
}

// This function updates RendererCommand::CharacterPosition to emulate
// IMM32-based client. Ideally we'd better to define new field for TSF Mozc
// into which the result of ITfContextView::GetTextExt is stored.
bool FillRenderInfo(TipTextService *text_service,
                    ITfContext *context,
                    TfEditCookie read_cookie,
                    RenderingInfo *info) {
  TipPrivateContext *private_context =
      text_service->GetPrivateContext(context);
  if (private_context == nullptr) {
    return nullptr;
  }

  info->output.Clear();
  const Output &output = private_context->last_output();

  CComPtr<ITfCompositionView> composition_view =
      TipCompositionUtil::GetComposition(context, read_cookie);
  if (!composition_view) {
    return false;
  }

  CComPtr<ITfRange> composition_range;
  if (FAILED(composition_view->GetRange(&composition_range))) {
    return false;
  }
  if (!composition_range) {
    return false;
  }

  CComPtr<ITfRange> target_range;
  if (FAILED(composition_range->Clone(&target_range))) {
    return false;
  }

  if (FAILED(target_range->Collapse(read_cookie, TF_ANCHOR_START))) {
    return false;
  }

  {
    const size_t target_pos = GetTargetPos(output);
    LONG shifted = 0;
    if (FAILED(target_range->ShiftStart(
            read_cookie, target_pos, &shifted, nullptr))) {
      return false;
    }
    if (FAILED(target_range->ShiftEnd(
            read_cookie, target_pos + 1, &shifted, nullptr))) {
      return false;
    }
  }

  CComPtr<ITfContextView> context_view;
  if (FAILED(context->GetActiveView(&context_view)) || !context_view) {
    return false;
  }

  RECT document_rect = {};
  if (FAILED(context_view->GetScreenExt(&document_rect))) {
    return false;
  }

  RECT text_rect = {};
  BOOL clipped = FALSE;
  if (FAILED(context_view->GetTextExt(read_cookie, target_range,
                                      &text_rect, &clipped))) {
    return false;
  }

  *info->target_rect = text_rect;
  info->output.CopyFrom(output);

  return true;
}

class TipImmersiveUiElementImpl : public ITfCandidateListUIElementBehavior {
 public:
  TipImmersiveUiElementImpl(TipTextService *text_service,
                            ITfContext *context,
                            HWND window_handle)
      : text_service_(text_service),
        context_(context),
        delegate_(TipUiElementDelegateFactory::Create(
            text_service, context,
            TipUiElementDelegateFactory::kImmersiveCandidateWindow)),
        window_(window_handle) {
  }

  // Destructor is kept as non-virtual because this class is designed to be
  // destroyed only by "delete this" in Release() method.
  // TODO(yukawa): put "final" keyword to the class declaration when C++11
  //     is allowed.
  ~TipImmersiveUiElementImpl() {
  }

  // The IUnknown interface methods.
  virtual STDMETHODIMP QueryInterface(REFIID interface_id, void **object) {
    if (!object) {
      return E_INVALIDARG;
    }
    *object = nullptr;

    // Find a matching interface from the ones implemented by this object.
    // This object implements IUnknown and ITfEditSession.
    if (::IsEqualIID(interface_id, IID_IUnknown)) {
      *object = static_cast<IUnknown *>(this);
    } else if (IsEqualIID(interface_id, IID_ITfUIElement)) {
      *object = static_cast<ITfUIElement *>(this);
    } else if (IsEqualIID(interface_id, IID_ITfCandidateListUIElement)) {
      *object = static_cast<ITfCandidateListUIElement *>(this);
    } else if (IsEqualIID(interface_id,
                          IID_ITfCandidateListUIElementBehavior)) {
      *object = static_cast<ITfCandidateListUIElementBehavior *>(this);
    }

    if (*object == nullptr) {
      return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
  }

  virtual ULONG STDMETHODCALLTYPE AddRef() {
    return ref_count_.AddRefImpl();
  }

  virtual ULONG STDMETHODCALLTYPE Release() {
    const ULONG count = ref_count_.ReleaseImpl();
    if (count == 0) {
      delete this;
    }
    return count;
  }

  void OnUpdate() {
    // When RequestEditSession fails, it does not maintain the reference count.
    // So we need to ensure that AddRef/Release should be called at least once
    // per oject.
    CComPtr<ITfEditSession> edit_session(new UpdateUiEditSession(
        text_service_, context_, this));

    HRESULT edit_session_result = S_OK;
    const HRESULT result = context_->RequestEditSession(
        text_service_->GetClientID(),
        edit_session,
        TF_ES_ASYNCDONTCARE | TF_ES_READ, &edit_session_result);
  }

  void Render(const RenderingInfo &info) {
    if (!info.output.has_candidates()) {
      window_.ShowWindow(SW_HIDE);
      return;
    }

    RenderImpl(info);

    BOOL shown = FALSE;
    delegate_->IsShown(&shown);
    if (!shown) {
      window_.ShowWindow(SW_HIDE);
    } else {
      window_.Invalidate();
      window_.ShowWindow(SW_SHOWNA);
    }
  }

 private:
  void RenderImpl(const RenderingInfo &info) {
    CSize size;
    int left_offset = 0;
    CBitmap bitmap(TipUiRendererImmersive::Render(
        info.output.candidates(), &size, &left_offset));
    CPoint left_top(info.target_rect.left - left_offset,
                    info.target_rect.bottom);
    CPoint src_left_top(0, 0);
    BLENDFUNCTION func = {AC_SRC_OVER, 0, 255, 0};
    CDC memdc;
    memdc.CreateCompatibleDC();
    const CBitmapHandle old_bitmap = memdc.SelectBitmap(bitmap);
    ::UpdateLayeredWindow(window_.m_hWnd, nullptr, &left_top, &size, memdc,
                          &src_left_top, 0, &func, ULW_OPAQUE);
    memdc.SelectBitmap(old_bitmap);
  }

  // This class is an implementation class for the ITfEditSession classes,
  // which is an observer for exclusively read the date from the text store.
  class UpdateUiEditSession : public ITfEditSession {
   public:
    // Destructor is kept as non-virtual because this class is designed to be
    // destroyed only by "delete this" in Release() method.
    // TODO(yukawa): put "final" keyword to the class declaration when C++11
    //     is allowed.
    ~UpdateUiEditSession();

    // The IUnknown interface methods.
    virtual STDMETHODIMP QueryInterface(REFIID interface_id, void **object);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    // The ITfEditSession interface method.
    // This function is called back by the TSF thread manager when an edit
    // request is granted.
    virtual HRESULT STDMETHODCALLTYPE DoEditSession(TfEditCookie read_cookie);
    UpdateUiEditSession(
        TipTextService *text_service, ITfContext *context,
        TipImmersiveUiElementImpl *ui_element);

   private:
    TipRefCount ref_count_;
    CComPtr<TipTextService> text_service_;
    CComPtr<ITfContext> context_;
    CComPtr<TipImmersiveUiElementImpl> ui_element_;

    DISALLOW_COPY_AND_ASSIGN(UpdateUiEditSession);
  };

  // The ITfUIElement interface methods
  virtual HRESULT STDMETHODCALLTYPE GetDescription(BSTR *description) {
    return delegate_->GetDescription(description);
  }
  virtual HRESULT STDMETHODCALLTYPE GetGUID(GUID *guid) {
    return delegate_->GetGUID(guid);
  }
  virtual HRESULT STDMETHODCALLTYPE Show(BOOL show) {
    return delegate_->Show(show);
  }
  virtual HRESULT STDMETHODCALLTYPE IsShown(BOOL *show) {
    return delegate_->IsShown(show);
  }

  // The ITfCandidateListUIElement interface methods
  virtual HRESULT STDMETHODCALLTYPE GetUpdatedFlags(DWORD *flags) {
    return delegate_->GetUpdatedFlags(flags);
  }
  virtual HRESULT STDMETHODCALLTYPE GetDocumentMgr(
      ITfDocumentMgr **document_manager) {
    return delegate_->GetDocumentMgr(document_manager);
  }
  virtual HRESULT STDMETHODCALLTYPE GetCount(UINT *count) {
    return delegate_->GetCount(count);
  }
  virtual HRESULT STDMETHODCALLTYPE GetSelection(UINT *index) {
    return delegate_->GetSelection(index);
  }
  virtual HRESULT STDMETHODCALLTYPE GetString(UINT index, BSTR *text) {
    return delegate_->GetString(index, text);
  }
  virtual HRESULT STDMETHODCALLTYPE GetPageIndex(UINT *index, UINT size,
                                                 UINT *page_count) {
    return delegate_->GetPageIndex(index, size, page_count);
  }
  virtual HRESULT STDMETHODCALLTYPE SetPageIndex(UINT *index,
                                                 UINT page_count) {
    return delegate_->SetPageIndex(index, page_count);
  }
  virtual HRESULT STDMETHODCALLTYPE GetCurrentPage(UINT *current_page) {
    return delegate_->GetCurrentPage(current_page);
  }

  // The ITfCandidateListUIElementBehavior interface methods
  virtual HRESULT STDMETHODCALLTYPE SetSelection(UINT index) {
    return delegate_->SetSelection(index);
  }
  virtual HRESULT STDMETHODCALLTYPE Finalize() {
    return delegate_->Finalize();
  }
  virtual HRESULT STDMETHODCALLTYPE Abort() {
    return delegate_->Abort();
  }

  TipRefCount ref_count_;
  CComPtr<TipTextService> text_service_;
  CComPtr<ITfContext> context_;
  scoped_ptr<TipUiElementDelegate> delegate_;
  CWindow window_;
  DISALLOW_COPY_AND_ASSIGN(TipImmersiveUiElementImpl);
};

HWND GetOwnerWindow(ITfContext *context) {
  CComPtr<ITfContextView> context_view;
  if (FAILED(context->GetActiveView(&context_view)) || !context_view) {
    return nullptr;
  }

  HWND window_handle = nullptr;
  if (FAILED(context_view->GetWnd(&window_handle)) ||
      window_handle == nullptr) {
    window_handle = ::GetFocus();
  }
  return window_handle;
}

class WindowMap
    : public hash_map<HWND, TipImmersiveUiElementImpl *> {
};

class ThreadLocalInfo {
 public:
  ThreadLocalInfo() {}
  WindowMap *window_map() {
    return &window_map_;
  }

 private:
  WindowMap window_map_;

  DISALLOW_COPY_AND_ASSIGN(ThreadLocalInfo);
};

ThreadLocalInfo *GetThreadLocalInfo() {
  if (g_module_unloaded) {
    return nullptr;
  }
  if (g_tls_index == TLS_OUT_OF_INDEXES) {
    return nullptr;
  }
  ThreadLocalInfo *info = static_cast<ThreadLocalInfo *>(
      ::TlsGetValue(g_tls_index));
  if (info != nullptr) {
    // already initialized.
    return info;
  }
  info = new ThreadLocalInfo();
  ::TlsSetValue(g_tls_index, info);
  return info;
}

void EnsureThreadLocalInfoDestroyed() {
  if (g_module_unloaded) {
    return;
  }
  if (g_tls_index == TLS_OUT_OF_INDEXES) {
    return;
  }
  ThreadLocalInfo *info = static_cast<ThreadLocalInfo *>(
      ::TlsGetValue(g_tls_index));
  if (info == nullptr) {
    // already destroyes.
    return;
  }
  delete info;
  ::TlsSetValue(g_tls_index, nullptr);
}

LRESULT WINAPI WindowProc(HWND window_handle,
                          UINT message,
                          WPARAM wparam,
                          LPARAM lparam) {
  ThreadLocalInfo *info = GetThreadLocalInfo();
  if (info == nullptr) {
    return ::DefWindowProcW(window_handle, message, wparam, lparam);
  }

  const WindowMap::iterator it = info->window_map()->find(window_handle);
  if (it == info->window_map()->end()) {
    return ::DefWindowProcW(window_handle, message, wparam, lparam);
  }

  switch (message) {
    case WM_MOZC_IMMERSIVE_WINDOW_UPDATE:
      it->second->OnUpdate();
      return 0;
    case WM_ERASEBKGND:
      return 1;
    case WM_NCDESTROY:
      info->window_map()->erase(it);
      return ::DefWindowProcW(window_handle, message, wparam, lparam);
    default:
      return ::DefWindowProcW(window_handle, message, wparam, lparam);
  }
}

TipImmersiveUiElementImpl::UpdateUiEditSession::~UpdateUiEditSession() {}

    // The IUnknown interface methods.
STDMETHODIMP TipImmersiveUiElementImpl::UpdateUiEditSession::QueryInterface(
    REFIID interface_id, void **object) {
  if (!object) {
    return E_INVALIDARG;
  }

  // Find a matching interface from the ones implemented by this object.
  // This object implements IUnknown and ITfEditSession.
  if (::IsEqualIID(interface_id, IID_IUnknown)) {
    *object = static_cast<IUnknown *>(this);
  } else if (IsEqualIID(interface_id, IID_ITfEditSession)) {
    *object = static_cast<ITfEditSession *>(this);
  } else {
    *object = nullptr;
    return E_NOINTERFACE;
  }

  AddRef();
  return S_OK;
}

ULONG STDMETHODCALLTYPE
    TipImmersiveUiElementImpl::UpdateUiEditSession::AddRef() {
  return ref_count_.AddRefImpl();
}

ULONG STDMETHODCALLTYPE
    TipImmersiveUiElementImpl::UpdateUiEditSession::Release() {
  const ULONG count = ref_count_.ReleaseImpl();
  if (count == 0) {
    delete this;
  }
  return count;
}

// The ITfEditSession interface method.
// This function is called back by the TSF thread manager when an edit
// request is granted.
HRESULT STDMETHODCALLTYPE
    TipImmersiveUiElementImpl::UpdateUiEditSession::DoEditSession(
    TfEditCookie read_cookie) {
  RenderingInfo info;
  FillRenderInfo(text_service_, context_, read_cookie, &info);
  ui_element_->Render(info);
  return S_OK;
}

TipImmersiveUiElementImpl::UpdateUiEditSession::UpdateUiEditSession(
    TipTextService *text_service, ITfContext *context,
    TipImmersiveUiElementImpl *ui_element)
    : text_service_(text_service),
      context_(context),
      ui_element_(ui_element) {
}

}  // namespace

ITfUIElement *TipUiElementImmersive::New(
    TipTextService *text_service,
    ITfContext *context,
    HWND *window_handle) {
  if (window_handle == nullptr) {
    return nullptr;
  }
  *window_handle = nullptr;

  ThreadLocalInfo *info = GetThreadLocalInfo();
  if (info == nullptr) {
    return nullptr;
  }

  const HWND owner_window = GetOwnerWindow(context);
  if (!::IsWindow(owner_window)) {
    return nullptr;
  }
  const HWND window = ::CreateWindowExW(
      WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
      kImmersiveUIWindowClassName,
      L"",
      WS_POPUP | WS_DISABLED,
      0, 0, 0, 0,
      owner_window,
      nullptr,
      g_module,
      nullptr);
  if (window == nullptr) {
    return nullptr;
  }
  TipImmersiveUiElementImpl *impl =
      new TipImmersiveUiElementImpl(text_service, context, window);
  (*info->window_map())[window] = impl;
  *window_handle = window;
  return impl;
}

void TipUiElementImmersive::OnActivate() {
  GetThreadLocalInfo();
}

void TipUiElementImmersive::OnDeactivate() {
  EnsureThreadLocalInfoDestroyed();
}

bool TipUiElementImmersive::OnDllProcessAttach(HINSTANCE module_handle,
                                               bool static_loading) {
  g_module = module_handle;
  g_tls_index = ::TlsAlloc();

  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(WNDCLASSEX);
  wc.style = CS_IME;
  wc.lpfnWndProc = WindowProc;
  wc.hInstance = module_handle;
  wc.lpszClassName = kImmersiveUIWindowClassName;

  const ATOM atom = ::RegisterClassExW(&wc);
  if (atom == INVALID_ATOM) {
    return false;
  }

  return true;
}

void TipUiElementImmersive::OnDllProcessDetach(HINSTANCE module_handle,
                                               bool process_shutdown) {
  if (g_tls_index != TLS_OUT_OF_INDEXES) {
    ::TlsFree(g_tls_index);
    g_tls_index = TLS_OUT_OF_INDEXES;
  }

  ::UnregisterClass(kImmersiveUIWindowClassName, module_handle);
  g_module_unloaded = true;
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc
