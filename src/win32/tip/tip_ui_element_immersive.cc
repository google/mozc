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

#include "win32/tip/tip_ui_element_immersive.h"

// clang-format off
#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
#include <atlbase.h>
#include <atlcom.h>
#include <atlapp.h>
#include <atlmisc.h>
#include <atlstr.h>
#include <atlwin.h>
#include <msctf.h>
// clang-format on

#include <memory>
#include <unordered_map>

#include "base/util.h"
#include "protocol/commands.pb.h"
#include "renderer/table_layout.h"
#include "renderer/win32/text_renderer.h"
#include "renderer/win32/win32_renderer_util.h"
#include "renderer/window_util.h"
#include "win32/tip/tip_composition_util.h"
#include "win32/tip/tip_dll_module.h"
#include "win32/tip/tip_edit_session.h"
#include "win32/tip/tip_private_context.h"
#include "win32/tip/tip_range_util.h"
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

using ::mozc::commands::Candidates;
using ::mozc::commands::Output;
using ::mozc::commands::Preedit;
using ::mozc::renderer::TableLayout;
using ::mozc::renderer::WindowUtil;

typedef ::mozc::commands::Candidates::Candidate Candidate;
typedef ::mozc::commands::Preedit_Segment Segment;
typedef ::mozc::commands::Preedit_Segment::Annotation Annotation;

#ifdef GOOGLE_JAPANESE_INPUT_BUILD

const wchar_t kImmersiveUIWindowClassName[] =
    L"Google Japanese Input Immersive UI Window";

#else  // GOOGLE_JAPANESE_INPUT_BUILD

const wchar_t kImmersiveUIWindowClassName[] = L"Mozc Immersive UI Window";

#endif  // GOOGLE_JAPANESE_INPUT_BUILD

#ifndef EVENT_OBJECT_IME_SHOW
#define EVENT_OBJECT_IME_SHOW 0x8027
#define EVENT_OBJECT_IME_HIDE 0x8028
#define EVENT_OBJECT_IME_CHANGE 0x8029
#endif  // EVENT_OBJECT_IME_SHOW

// Represents the module handle of this module.
volatile HMODULE g_module = nullptr;

// True if the DLL received DLL_PROCESS_DETACH notification.
volatile bool g_module_unloaded = false;

// Thread Local Storage (TLS) index to specify the current UI thread is
// initialized or not. If ::GetTlsValue(g_tls_index) returns non-zero
// value, the current thread is initialized.
volatile DWORD g_tls_index = TLS_OUT_OF_INDEXES;

struct RenderingInfo {
  CRect target_rect;
  Output output;
  bool has_target_rect;
  RenderingInfo() : has_target_rect(false) {}
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
bool FillRenderInfo(TipTextService *text_service, ITfContext *context,
                    TfEditCookie read_cookie, RenderingInfo *info) {
  TipPrivateContext *private_context = text_service->GetPrivateContext(context);
  if (private_context == nullptr) {
    return false;
  }

  info->output.Clear();
  info->target_rect = CRect();
  info->has_target_rect = false;
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
    if (FAILED(target_range->ShiftStart(read_cookie, target_pos, &shifted,
                                        nullptr))) {
      return false;
    }
    if (FAILED(target_range->ShiftEnd(read_cookie, target_pos + 1, &shifted,
                                      nullptr))) {
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
  bool clipped = false;
  const HRESULT hr = TipRangeUtil::GetTextExt(
      context_view, read_cookie, target_range, &text_rect, &clipped);
  if (SUCCEEDED(hr)) {
    info->target_rect = text_rect;
    info->has_target_rect = true;
  } else if (hr == TF_E_NOLAYOUT) {
    // This is not a fatal error but |text_rect| is not available yet.
  } else {
    return false;
  }
  info->output = output;
  return true;
}

CRect ToCRect(const Rect &rect) {
  return WTL::CRect(rect.Left(), rect.Top(), rect.Right(), rect.Bottom());
}

// Returns the smallest index of the given candidate list which satisfies
// candidates.candidate(i) == |candidate_index|.
// This function returns the size of the given candidate list when there
// aren't any candidates satisfying the above condition.
int GetCandidateArrayIndexByCandidateIndex(const Candidates &candidates,
                                           int candidate_index) {
  for (size_t i = 0; i < candidates.candidate_size(); ++i) {
    const Candidate &candidate = candidates.candidate(i);
    if (candidate.index() == candidate_index) {
      return i;
    }
  }
  return candidates.candidate_size();
}

// Returns the smallest index of the given candidate list which satisfies
// |candidates.focused_index| == |candidates.candidate(i).index()|.
// This function returns the size of the given candidate list when there
// aren't any candidates satisfying the above condition.
int GetFocusedArrayIndex(const Candidates &candidates) {
  const int invalid_index = candidates.candidate_size();

  if (!candidates.has_focused_index()) {
    return invalid_index;
  }

  const int focused_index = candidates.focused_index();

  return GetCandidateArrayIndexByCandidateIndex(candidates, focused_index);
}

class TipImmersiveUiElementImpl : public ITfCandidateListUIElementBehavior {
 public:
  TipImmersiveUiElementImpl(TipTextService *text_service, ITfContext *context,
                            HWND window_handle)
      : text_service_(text_service),
        context_(context),
        delegate_(TipUiElementDelegateFactory::Create(
            text_service, context,
            TipUiElementDelegateFactory::kImmersiveCandidateWindow)),
        working_area_(renderer::win32::WorkingAreaFactory::Create()),
        text_renderer_(renderer::win32::TextRenderer::Create()),
        window_(window_handle),
        window_visible_(false) {}

  TipImmersiveUiElementImpl(const TipImmersiveUiElementImpl &) = delete;
  TipImmersiveUiElementImpl &operator=(const TipImmersiveUiElementImpl &) =
      delete;

  // Destructor is kept as non-virtual because this class is designed to be
  // destroyed only by "delete this" in Release() method.
  // TODO(yukawa): put "final" keyword to the class declaration when C++11
  //     is allowed.
  ~TipImmersiveUiElementImpl() {}

  // The IUnknown interface methods.
  virtual STDMETHODIMP QueryInterface(REFIID interface_id, void **object) {
    if (!object) {
      return E_INVALIDARG;
    }
    *object = nullptr;

    // Find a matching interface from the ones implemented by this object.
    // This object implements IUnknown and ITfEditSession.
    if (::IsEqualIID(interface_id, IID_IUnknown)) {
      *object = static_cast<IUnknown *>(
          static_cast<ITfCandidateListUIElement *>(this));
    } else if (IsEqualIID(interface_id, IID_ITfUIElement)) {
      *object = static_cast<ITfUIElement *>(
          static_cast<ITfCandidateListUIElement *>(this));
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

  virtual ULONG STDMETHODCALLTYPE AddRef() { return ref_count_.AddRefImpl(); }

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
    // per object.
    CComPtr<ITfEditSession> edit_session(
        new UpdateUiEditSession(text_service_, context_, this));

    HRESULT edit_session_result = S_OK;
    const HRESULT result = context_->RequestEditSession(
        text_service_->GetClientID(), edit_session,
        TF_ES_ASYNCDONTCARE | TF_ES_READ, &edit_session_result);
  }

  // Returns true when handled.
  bool HandleMouseEvent(UINT nFlags, const CPoint &point,
                        bool select_candidate) {
    const Candidates candidates = output_.candidates();
    for (size_t i = 0; i < candidates.candidate_size(); ++i) {
      const Candidate &candidate = candidates.candidate(i);

      const CRect rect = ToCRect(table_layout_.GetRowRect(i));
      if (rect.PtInRect(point)) {
        if (select_candidate) {
          TipEditSession::SelectCandidateAsync(text_service_, context_,
                                               candidate.id());
          return true;
        } else {
          const int focused_array_index = GetFocusedArrayIndex(candidates);
          if (i != focused_array_index) {
            TipEditSession::HilightCandidateAsync(text_service_, context_,
                                                  candidate.id());
            return true;
          }
          return false;
        }
      }
    }
    return false;
  }

  void ShowWindow(bool content_changed) {
    window_.ShowWindow(SW_SHOWNA);
    if (!window_visible_) {
      ::NotifyWinEvent(EVENT_OBJECT_IME_SHOW, window_.m_hWnd, OBJID_WINDOW,
                       CHILDID_SELF);
    } else if (content_changed) {
      ::NotifyWinEvent(EVENT_OBJECT_IME_CHANGE, window_.m_hWnd, OBJID_WINDOW,
                       CHILDID_SELF);
    }
    window_visible_ = true;
  }

  void HideWindow() {
    window_.ShowWindow(SW_HIDE);
    if (window_visible_) {
      ::NotifyWinEvent(EVENT_OBJECT_IME_HIDE, window_.m_hWnd, OBJID_WINDOW,
                       CHILDID_SELF);
    }
    window_visible_ = false;
  }

  void Render(const RenderingInfo &info) {
    // Should be compared here before |output_| is synced to |info.output|.
    const bool content_changed =
        (target_rect_ != info.target_rect) ||
        (output_.SerializeAsString() != info.output.SerializeAsString());

    output_.CopyFrom(info.output);
    if (info.has_target_rect) {
      target_rect_ = info.target_rect;
      RenderImpl();
    }

    BOOL shown = FALSE;
    delegate_->IsShown(&shown);
    if (!shown) {
      HideWindow();
    } else {
      ShowWindow(content_changed);
    }
  }

  LRESULT WindowProc(HWND window_handle, UINT message, WPARAM wparam,
                     LPARAM lparam) {
    switch (message) {
      case WM_MOZC_IMMERSIVE_WINDOW_UPDATE:
        OnUpdate();
        return 0;
      case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
      case WM_LBUTTONDOWN:
        HandleMouseEvent(static_cast<UINT>(wparam),
                         CPoint(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)),
                         false);
        return ::DefWindowProcW(window_handle, message, wparam, lparam);
      case WM_LBUTTONUP:
        HandleMouseEvent(static_cast<UINT>(wparam),
                         CPoint(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)),
                         true);
        return ::DefWindowProcW(window_handle, message, wparam, lparam);
      case WM_MOUSEMOVE:
        if ((static_cast<UINT>(wparam) & MK_LBUTTON) == MK_LBUTTON) {
          HandleMouseEvent(static_cast<UINT>(wparam),
                           CPoint(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)),
                           false);
        }
        return ::DefWindowProcW(window_handle, message, wparam, lparam);
      case WM_SETCURSOR:
        ::SetCursor(::LoadCursor(nullptr, IDC_ARROW));
        return 0;
      default:
        return ::DefWindowProcW(window_handle, message, wparam, lparam);
    }
  }

 private:
  void RenderImpl() {
    if (!output_.has_candidates()) {
      return;
    }
    CSize size;
    int left_offset = 0;
    CBitmap bitmap(TipUiRendererImmersive::Render(
        output_.candidates(), text_renderer_.get(), &table_layout_, &size,
        &left_offset));
    const CPoint target_point(target_rect_.left, target_rect_.bottom);
    Rect new_position(target_point.x - left_offset, target_point.y, size.cx,
                      size.cy);
    {
      const Rect preedit_rect(target_rect_.left, target_rect_.top,
                              target_rect_.Width(), target_rect_.Height());
      const Size window_size(size.cx, size.cy);
      const Point zero_point_offset(left_offset, 0);
      Rect working_area;
      CRect area;
      if (working_area_->GetWorkingAreaFromPoint(target_point, &area)) {
        working_area = Rect(area.left, area.top, area.Width(), area.Height());
      }
      new_position =
          WindowUtil::GetWindowRectForMainWindowFromTargetPointAndPreedit(
              Point(target_point.x, target_point.y), preedit_rect, window_size,
              zero_point_offset, working_area, false);
    }
    CDC memdc;
    memdc.CreateCompatibleDC();
    const CBitmapHandle old_bitmap = memdc.SelectBitmap(bitmap);
    CPoint src_left_top(0, 0);
    CPoint new_top_left(new_position.Left(), new_position.Top());
    CSize new_size(new_position.Width(), new_position.Height());
    BLENDFUNCTION func = {AC_SRC_OVER, 0, 255, 0};
    ::UpdateLayeredWindow(window_.m_hWnd, nullptr, &new_top_left, &new_size,
                          memdc, &src_left_top, 0, &func, ULW_ALPHA);
    memdc.SelectBitmap(old_bitmap);
  }

  // This class is an implementation class for the ITfEditSession classes,
  // which is an observer for exclusively read the date from the text store.
  class UpdateUiEditSession : public ITfEditSession {
   public:
    UpdateUiEditSession(const UpdateUiEditSession &) = delete;
    UpdateUiEditSession &operator=(const UpdateUiEditSession &) = delete;

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
    UpdateUiEditSession(TipTextService *text_service, ITfContext *context,
                        TipImmersiveUiElementImpl *ui_element);

   private:
    TipRefCount ref_count_;
    CComPtr<TipTextService> text_service_;
    CComPtr<ITfContext> context_;
    CComPtr<TipImmersiveUiElementImpl> ui_element_;
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
  virtual HRESULT STDMETHODCALLTYPE
  GetDocumentMgr(ITfDocumentMgr **document_manager) {
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
  virtual HRESULT STDMETHODCALLTYPE SetPageIndex(UINT *index, UINT page_count) {
    return delegate_->SetPageIndex(index, page_count);
  }
  virtual HRESULT STDMETHODCALLTYPE GetCurrentPage(UINT *current_page) {
    return delegate_->GetCurrentPage(current_page);
  }

  // The ITfCandidateListUIElementBehavior interface methods
  virtual HRESULT STDMETHODCALLTYPE SetSelection(UINT index) {
    return delegate_->SetSelection(index);
  }
  virtual HRESULT STDMETHODCALLTYPE Finalize() { return delegate_->Finalize(); }
  virtual HRESULT STDMETHODCALLTYPE Abort() { return delegate_->Abort(); }

  TipRefCount ref_count_;
  CComPtr<TipTextService> text_service_;
  CComPtr<ITfContext> context_;
  std::unique_ptr<TipUiElementDelegate> delegate_;
  std::unique_ptr<renderer::win32::WorkingAreaInterface> working_area_;
  std::unique_ptr<renderer::win32::TextRenderer> text_renderer_;
  CWindow window_;
  bool window_visible_;
  TableLayout table_layout_;
  CRect target_rect_;
  Output output_;
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

using WindowMap = std::unordered_map<HWND, TipImmersiveUiElementImpl *>;

class ThreadLocalInfo {
 public:
  ThreadLocalInfo() {}
  ThreadLocalInfo(const ThreadLocalInfo &) = delete;
  ThreadLocalInfo &operator=(const ThreadLocalInfo &) = delete;
  WindowMap *window_map() { return &window_map_; }

 private:
  WindowMap window_map_;
};

ThreadLocalInfo *GetThreadLocalInfo() {
  if (g_module_unloaded) {
    return nullptr;
  }
  if (g_tls_index == TLS_OUT_OF_INDEXES) {
    return nullptr;
  }
  ThreadLocalInfo *info =
      static_cast<ThreadLocalInfo *>(::TlsGetValue(g_tls_index));
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
  ThreadLocalInfo *info =
      static_cast<ThreadLocalInfo *>(::TlsGetValue(g_tls_index));
  if (info == nullptr) {
    // already destroyed.
    return;
  }
  delete info;
  ::TlsSetValue(g_tls_index, nullptr);
}

LRESULT WINAPI WindowProc(HWND window_handle, UINT message, WPARAM wparam,
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
    case WM_NCDESTROY: {
      const LRESULT result =
          it->second->WindowProc(window_handle, message, wparam, lparam);
      info->window_map()->erase(it);
      return result;
    }
    default:
      return it->second->WindowProc(window_handle, message, wparam, lparam);
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
  if (FillRenderInfo(text_service_, context_, read_cookie, &info)) {
    ui_element_->Render(info);
  }
  return S_OK;
}

TipImmersiveUiElementImpl::UpdateUiEditSession::UpdateUiEditSession(
    TipTextService *text_service, ITfContext *context,
    TipImmersiveUiElementImpl *ui_element)
    : text_service_(text_service), context_(context), ui_element_(ui_element) {}

}  // namespace

ITfUIElement *TipUiElementImmersive::New(TipTextService *text_service,
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
  const HWND window =
      ::CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                        kImmersiveUIWindowClassName, L"", WS_POPUP, 0, 0, 0, 0,
                        owner_window, nullptr, g_module, nullptr);
  if (window == nullptr) {
    return nullptr;
  }
  TipImmersiveUiElementImpl *impl =
      new TipImmersiveUiElementImpl(text_service, context, window);
  (*info->window_map())[window] = impl;
  *window_handle = window;
  return static_cast<ITfCandidateListUIElement *>(impl);
}

void TipUiElementImmersive::OnActivate() { GetThreadLocalInfo(); }

void TipUiElementImmersive::OnDeactivate() { EnsureThreadLocalInfoDestroyed(); }

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
