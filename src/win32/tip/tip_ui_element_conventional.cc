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

#include <msctf.h>
#include <objbase.h>
#include <wil/com.h>
#include <windows.h>

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/win32/com.h"
#include "win32/tip/tip_dll_module.h"
#include "win32/tip/tip_text_service.h"
#include "win32/tip/tip_ui_element_delegate.h"

namespace mozc {
namespace win32 {

template <>
bool IsIIDOf<ITfCandidateListUIElementBehavior>(REFIID riid) {
  return IsIIDOf<ITfCandidateListUIElementBehavior, ITfCandidateListUIElement,
                 ITfUIElement>(riid);
}

template <>
bool IsIIDOf<ITfToolTipUIElement>(REFIID riid) {
  return IsIIDOf<ITfToolTipUIElement, ITfUIElement>(riid);
}

namespace tsf {
namespace {

class TipCandidateListImpl final
    : public TipComImplements<ITfCandidateListUIElementBehavior> {
 public:
  TipCandidateListImpl(TipUiElementConventional::UIType type,
                       wil::com_ptr_nothrow<TipTextService> text_service,
                       wil::com_ptr_nothrow<ITfContext> context)
      : delegate_(TipUiElementDelegateFactory::Create(std::move(text_service),
                                                      std::move(context),
                                                      ToDelegateType(type))) {}

 private:
  // The IUnknown interface methods.
  STDMETHODIMP QueryInterface(REFIID interface_id, void **object) override {
    if (!object) {
      return E_POINTER;
    }
    if (delegate_->IsObservable()) {
      // ITfCandidateListUIElementBehavior and ITfCandidateListUIElement
      // interfaces are available iff the |type_| is observable.
      *object =
          QueryInterfaceImpl<ITfCandidateListUIElementBehavior>(interface_id);
    } else {
      *object = QueryInterfaceImpl<ITfUIElement>(interface_id);
    }
    if (*object == nullptr) {
      return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
  }

  // The ITfUIElement interface methods
  STDMETHODIMP GetDescription(BSTR *description) override {
    return delegate_->GetDescription(description);
  }
  STDMETHODIMP GetGUID(GUID *guid) override { return delegate_->GetGUID(guid); }
  STDMETHODIMP Show(BOOL show) override { return delegate_->Show(show); }
  STDMETHODIMP IsShown(BOOL *show) override { return delegate_->IsShown(show); }

  // The ITfCandidateListUIElement interface methods
  STDMETHODIMP GetUpdatedFlags(DWORD *flags) override {
    return delegate_->GetUpdatedFlags(flags);
  }
  STDMETHODIMP GetDocumentMgr(ITfDocumentMgr **document_manager) override {
    return delegate_->GetDocumentMgr(document_manager);
  }
  STDMETHODIMP GetCount(UINT *count) override {
    return delegate_->GetCount(count);
  }
  STDMETHODIMP GetSelection(UINT *index) override {
    return delegate_->GetSelection(index);
  }
  STDMETHODIMP GetString(UINT index, BSTR *text) override {
    return delegate_->GetString(index, text);
  }
  STDMETHODIMP GetPageIndex(UINT *index, UINT size, UINT *page_count) override {
    return delegate_->GetPageIndex(index, size, page_count);
  }
  STDMETHODIMP SetPageIndex(UINT *index, UINT page_count) override {
    return delegate_->SetPageIndex(index, page_count);
  }
  STDMETHODIMP GetCurrentPage(UINT *current_page) override {
    return delegate_->GetCurrentPage(current_page);
  }

  // The ITfCandidateListUIElementBehavior interface methods
  STDMETHODIMP SetSelection(UINT index) override {
    return delegate_->SetSelection(index);
  }
  STDMETHODIMP Finalize() override { return delegate_->Finalize(); }
  STDMETHODIMP Abort() override { return delegate_->Abort(); }

 private:
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

  std::unique_ptr<TipUiElementDelegate> delegate_;
};

class TipIndicatorImpl final : public TipComImplements<ITfToolTipUIElement> {
 public:
  TipIndicatorImpl(wil::com_ptr_nothrow<TipTextService> text_service,
                   wil::com_ptr_nothrow<ITfContext> context)
      : delegate_(TipUiElementDelegateFactory::Create(
            std::move(text_service), std::move(context),
            TipUiElementDelegateFactory::kConventionalIndicatorWindow)) {}

  // The ITfUIElement interface methods
  STDMETHODIMP GetDescription(BSTR *description) override {
    return delegate_->GetDescription(description);
  }
  STDMETHODIMP GetGUID(GUID *guid) override { return delegate_->GetGUID(guid); }
  STDMETHODIMP Show(BOOL show) override { return delegate_->Show(show); }
  STDMETHODIMP IsShown(BOOL *show) override { return delegate_->IsShown(show); }

  // The ITfToolTipUIElement interface methods
  STDMETHODIMP GetString(BSTR *str) override {
    return delegate_->GetString(str);
  }

 private:
  std::unique_ptr<TipUiElementDelegate> delegate_;
};

}  // namespace

wil::com_ptr_nothrow<ITfUIElement> TipUiElementConventional::New(
    TipUiElementConventional::UIType type,
    wil::com_ptr_nothrow<TipTextService> text_service,
    wil::com_ptr_nothrow<ITfContext> context) {
  if (!text_service || !context) return nullptr;
  switch (type) {
    case TipUiElementConventional::kUnobservableSuggestWindow:
      [[fallthrough]];
    case TipUiElementConventional::kObservableSuggestWindow:
      [[fallthrough]];
    case TipUiElementConventional::kCandidateWindow:
      return MakeComPtr<TipCandidateListImpl>(type, std::move(text_service),
                                              std::move(context));
    case TipUiElementConventional::KIndicatorWindow:
      return MakeComPtr<TipIndicatorImpl>(std::move(text_service),
                                          std::move(context));
    default:
      return nullptr;
  }
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc
