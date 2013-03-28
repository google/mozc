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

#include "win32/tip/tip_ui_handler_immersive.h"

#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
// Workaround against KB813540
#include <atlbase_mozc.h>
#include <atlcom.h>
#include <atlapp.h>
#include <atlmisc.h>
#include <atlwin.h>
#include <msctf.h>

#include "base/hash_tables.h"
#include "base/util.h"
#include "session/commands.pb.h"
#include "win32/tip/tip_composition_util.h"
#include "win32/tip/tip_private_context.h"
#include "win32/tip/tip_text_service.h"
#include "win32/tip/tip_ui_element_immersive.h"
#include "win32/tip/tip_ui_element_manager.h"

namespace mozc {
namespace win32 {
namespace tsf {

namespace {

using ATL::CComPtr;
using ATL::CComQIPtr;
using ATL::CWindow;
using WTL::CRect;

using ::mozc::commands::Output;
using ::mozc::commands::Preedit;
typedef ::mozc::commands::Preedit_Segment Segment;
typedef ::mozc::commands::Preedit_Segment::Annotation Annotation;

// Represents the module handle of this module.
volatile HMODULE g_module = nullptr;

// True if the the DLL received DLL_PROCESS_DETACH notification.
volatile bool g_module_unloaded = false;

// Thread Local Storage (TLS) index to specify the current UI thread is
// initialized or not. if ::GetTlsValue(g_tls_index) returns non-zero
// value, the current thread is initialized.
volatile DWORD g_tls_index = TLS_OUT_OF_INDEXES;

// Visual C++ 2008 requires this.
#if 1310 <= _MSC_VER || _MSC_VER < 1600
using stdext::hash_compare;
#endif  // 1310 <= _MSC_VER < 1600

// Custom hash function for ATL::CComPtr.
template <typename T>
struct PtrHashCompare : public hash_compare<T> {
  std::size_t operator()(const T &value) const {
    // Caveats: On x86 environment, both _M_X64 and _M_IX86 are defined. So we
    //     need to check _M_X64 first.
#if defined(_M_X64)
    const size_t kUnusedBits = 3;  // assuming 8-byte aligned
#elif defined(_M_IX86)
    const size_t kUnusedBits = 2;  // assuming 4-byte aligned
#else
#error "unsupported platform"
#endif
    // Compress the data by shifting unused bits.
    return reinterpret_cast<size_t>(value) >> kUnusedBits;
  }
  bool operator()(const T &value1, const T &value2) const {
    return value1 != value2;
  }
};

class UiElementMap
    : public hash_map<ITfUIElement *,
                      HWND,
                      PtrHashCompare<ITfUIElement *> > {
};

class ThreadLocalInfo {
 public:
  ThreadLocalInfo() {}
  UiElementMap *ui_element_map() {
    return &ui_element_map_;
  }

 private:
  UiElementMap ui_element_map_;

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

void UpdateUI(TipTextService *text_service, ITfContext *context) {
  if (text_service == nullptr) {
    return;
  }

  ThreadLocalInfo *info = GetThreadLocalInfo();
  if (info == nullptr) {
    return;
  }
  TipPrivateContext *private_context =
      text_service->GetPrivateContext(context);
  if (private_context == nullptr) {
    return;
  }
  private_context->GetUiElementManager()->OnUpdate(text_service, context);
  const UiElementMap &map = *info->ui_element_map();

  const TipUiElementManager::UIElementFlags kUiFlags[] = {
    TipUiElementManager::kSuggestWindow,
    TipUiElementManager::kCandidateWindow,
  };
  for (size_t i = 0; i < arraysize(kUiFlags); ++i) {
    const CComPtr<ITfUIElement> ui_element =
        private_context->GetUiElementManager()->GetElement(kUiFlags[i]);
    if (ui_element) {
      const UiElementMap::const_iterator it = map.find(ui_element);
      if (it == map.end()) {
        continue;
      }
      ::PostMessageW(it->second, WM_MOZC_IMMERSIVE_WINDOW_UPDATE, 0, 0);
    }
  }
}

}  // namespace

ITfUIElement *TipUiHandlerImmersive::CreateUI(TipUiHandler::UiType type,
                                              TipTextService *text_service,
                                              ITfContext *context) {
  switch (type) {
    case TipUiHandler::kSuggestWindow:
    case TipUiHandler::kCandidateWindow: {
      ThreadLocalInfo *info = GetThreadLocalInfo();
      if (info == nullptr) {
        return nullptr;
      }
      HWND window_handle = nullptr;
      CComPtr<ITfUIElement> element(TipUiElementImmersive::New(
          text_service, context, &window_handle));
      if (element == nullptr) {
        return nullptr;
      }
      if (window_handle == nullptr) {
        return nullptr;
      }
      (*info->ui_element_map())[element] = window_handle;
      // pass the ownership to the caller.
      return element.Detach();
    }
    default:
      return nullptr;
  }
}

void TipUiHandlerImmersive::OnDestroyElement(ITfUIElement *element) {
  ThreadLocalInfo *info = GetThreadLocalInfo();
  if (info == nullptr) {
    return;
  }
  UiElementMap::iterator it = info->ui_element_map()->find(element);
  if (it == info->ui_element_map()->end()) {
    return;
  }
  ::DestroyWindow(it->second);
  info->ui_element_map()->erase(it);
}

void TipUiHandlerImmersive::OnActivate() {
  TipUiElementImmersive::OnActivate();
}

void TipUiHandlerImmersive::OnDeactivate() {
  EnsureThreadLocalInfoDestroyed();
  TipUiElementImmersive::OnDeactivate();
}

void TipUiHandlerImmersive::OnFocusChange(
    TipTextService *text_service, ITfDocumentMgr *focused_document_manager) {
  if (!focused_document_manager) {
    // Empty document is not an error.
    return;
  }

  CComPtr<ITfContext> context;
  if (FAILED(focused_document_manager->GetBase(&context))) {
    return;
  }
  if (!context) {
    return;
  }

  UpdateUI(text_service, context);
}

bool TipUiHandlerImmersive::OnLayoutChange(TipTextService *text_service,
                                           ITfContext *context,
                                           TfLayoutCode layout_code,
                                           ITfContextView *context_view) {
  UpdateUI(text_service, context);
  return true;
}

bool TipUiHandlerImmersive::Update(TipTextService *text_service,
                                   ITfContext *context,
                                   TfEditCookie read_cookie) {
  UpdateUI(text_service, context);
  return true;
}

bool TipUiHandlerImmersive::OnDllProcessAttach(HINSTANCE module_handle,
                                               bool static_loading) {
  g_module = module_handle;
  g_tls_index = ::TlsAlloc();
  return TipUiElementImmersive::OnDllProcessAttach(
      module_handle, static_loading);
}

void TipUiHandlerImmersive::OnDllProcessDetach(HINSTANCE module_handle,
                                               bool process_shutdown) {
  if (g_tls_index != TLS_OUT_OF_INDEXES) {
    ::TlsFree(g_tls_index);
    g_tls_index = g_tls_index;
  }
  g_module_unloaded = true;
  g_module = nullptr;
  TipUiElementImmersive::OnDllProcessDetach(module_handle, process_shutdown);
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc
