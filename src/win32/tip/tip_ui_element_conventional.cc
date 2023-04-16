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

#include "win32/tip/tip_ui_element_conventional.h"

#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
#include <atlbase.h>
#include <atlcom.h>
#include <atlstr.h>
#include <msctf.h>
#include <wrl/client.h>

#include <memory>

#include "base/logging.h"
#include "win32/tip/tip_ref_count.h"
#include "win32/tip/tip_text_service.h"
#include "win32/tip/tip_ui_element_delegate.h"

namespace mozc {
namespace win32 {
namespace tsf {

namespace {

using Microsoft::WRL::ComPtr;

class TipCandidateListImpl final : public ITfCandidateListUIElementBehavior {
 public:
  TipCandidateListImpl(TipUiElementConventional::UIType type,
                       const ComPtr<TipTextService> &text_service,
                       const ComPtr<ITfContext> &context)
      : delegate_(TipUiElementDelegateFactory::Create(text_service, context,
                                                      ToDelegateType(type))) {}
  TipCandidateListImpl(const TipCandidateListImpl &) = delete;
  TipCandidateListImpl &operator=(const TipCandidateListImpl &) = delete;

 private:
  ~TipCandidateListImpl() = default;

  // The IUnknown interface methods.
  virtual STDMETHODIMP QueryInterface(REFIID interface_id, void **object) {
    if (!object) {
      return E_INVALIDARG;
    }
    *object = nullptr;

    // Finds a matching interface from the ones implemented by this object.
    // This object implements IUnknown.
    if (::IsEqualIID(interface_id, IID_IUnknown)) {
      *object = static_cast<IUnknown *>(this);
    } else if (IsEqualIID(interface_id, IID_ITfUIElement)) {
      *object = static_cast<ITfUIElement *>(this);
    } else if (delegate_->IsObservable()) {
      // Following interfaces are available iff the |type_| is observable.
      if (IsEqualIID(interface_id, IID_ITfCandidateListUIElement)) {
        *object = static_cast<ITfCandidateListUIElement *>(this);
      } else if (IsEqualIID(interface_id,
                            IID_ITfCandidateListUIElementBehavior)) {
        *object = static_cast<ITfCandidateListUIElementBehavior *>(this);
      }
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

  static TipUiElementDelegateFactory::ElementType ToDelegateType(
      TipUiElementConventional::UIType type) {
    switch (type) {
      case TipUiElementConventional::kUnobservableSuggestWindow:
        return TipUiElementDelegateFactory::
            kConventionalUnobservableSuggestWindow;
      case TipUiElementConventional::kObservableSuggestWindow:
        return TipUiElementDelegateFactory::
            kConventionalObservableSuggestWindow;
      case TipUiElementConventional::kCandidateWindow:
        return TipUiElementDelegateFactory::kConventionalCandidateWindow;
      default:
        LOG(FATAL) << "must not reach here.";
        return TipUiElementDelegateFactory::kConventionalCandidateWindow;
    }
  }

  TipRefCount ref_count_;
  std::unique_ptr<TipUiElementDelegate> delegate_;
};

class TipIndicatorImpl final : public ITfToolTipUIElement {
 public:
  TipIndicatorImpl(const ComPtr<TipTextService> &text_service,
                   const ComPtr<ITfContext> &context)
      : delegate_(TipUiElementDelegateFactory::Create(
            text_service, context,
            TipUiElementDelegateFactory::kConventionalIndicatorWindow)) {}
  TipIndicatorImpl(const TipIndicatorImpl &) = delete;
  TipIndicatorImpl &operator=(const TipIndicatorImpl &) = delete;

 private:
  ~TipIndicatorImpl() = default;

  // The IUnknown interface methods.
  virtual STDMETHODIMP QueryInterface(REFIID interface_id, void **object) {
    if (!object) {
      return E_INVALIDARG;
    }
    *object = nullptr;

    // Finds a matching interface from the ones implemented by this object.
    // This object implements IUnknown.
    if (::IsEqualIID(interface_id, IID_IUnknown)) {
      *object = static_cast<IUnknown *>(this);
    } else if (::IsEqualIID(interface_id, IID_ITfUIElement)) {
      *object = static_cast<ITfUIElement *>(this);
    } else if (::IsEqualIID(interface_id, IID_ITfToolTipUIElement)) {
      *object = static_cast<ITfToolTipUIElement *>(this);
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

  // The ITfToolTipUIElement interface methods
  virtual HRESULT STDMETHODCALLTYPE GetString(BSTR *str) {
    return delegate_->GetString(str);
  }

  TipRefCount ref_count_;
  std::unique_ptr<TipUiElementDelegate> delegate_;
};

}  // namespace

ComPtr<ITfUIElement> TipUiElementConventional::New(
    TipUiElementConventional::UIType type,
    const ComPtr<TipTextService> &text_service,
    const ComPtr<ITfContext> &context) {
  if (!text_service || !context) return nullptr;
  switch (type) {
    case TipUiElementConventional::kUnobservableSuggestWindow:
      return new TipCandidateListImpl(type, text_service, context);
    case TipUiElementConventional::kObservableSuggestWindow:
      return new TipCandidateListImpl(type, text_service, context);
    case TipUiElementConventional::kCandidateWindow:
      return new TipCandidateListImpl(type, text_service, context);
    case TipUiElementConventional::KIndicatorWindow:
      return new TipIndicatorImpl(text_service, context);
    default:
      return nullptr;
  }
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc
