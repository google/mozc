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

#ifndef MOZC_WIN32_TIP_TIP_UI_ELEMENT_DELEGATE_H_
#define MOZC_WIN32_TIP_TIP_UI_ELEMENT_DELEGATE_H_

#include <windows.h>
#include <msctf.h>

#include "base/port.h"

struct ITfContext;

namespace mozc {
namespace win32 {
namespace tsf {

class TipTextService;

class TipUiElementDelegate {
 public:
  TipUiElementDelegate() = default;
  TipUiElementDelegate(const TipUiElementDelegate &) = delete;
  TipUiElementDelegate &operator=(const TipUiElementDelegate &) = delete;
  virtual ~TipUiElementDelegate() = default;

  virtual bool IsObservable() const = 0;

  // The ITfUIElement interface methods
  virtual HRESULT GetDescription(BSTR *description) = 0;
  virtual HRESULT GetGUID(GUID *guid) = 0;
  virtual HRESULT Show(BOOL show) = 0;
  virtual HRESULT IsShown(BOOL *show) = 0;

  // The ITfCandidateListUIElement interface methods
  virtual HRESULT GetUpdatedFlags(DWORD *flags) = 0;
  virtual HRESULT GetDocumentMgr(ITfDocumentMgr **document_manager) = 0;
  virtual HRESULT GetCount(UINT *count) = 0;
  virtual HRESULT GetSelection(UINT *index) = 0;
  virtual HRESULT GetString(UINT index, BSTR *text) = 0;
  virtual HRESULT GetPageIndex(UINT *index, UINT size, UINT *page_count) = 0;
  virtual HRESULT SetPageIndex(UINT *index, UINT page_count) = 0;
  virtual HRESULT GetCurrentPage(UINT *current_page) = 0;

  // The ITfCandidateListUIElementBehavior interface methods
  virtual HRESULT SetSelection(UINT index) = 0;
  virtual HRESULT Finalize() = 0;
  virtual HRESULT Abort() = 0;

  // The ITfToolTipUIElement interface methods
  virtual HRESULT GetString(BSTR *text) = 0;
};

class TipUiElementDelegateFactory {
 public:
  enum ElementType {
    kConventionalUnobservableSuggestWindow,
    kConventionalObservableSuggestWindow,
    kConventionalCandidateWindow,
    kConventionalIndicatorWindow,
    kImmersiveCandidateWindow,
    kImmersiveIndicatorWindow,
  };

  static TipUiElementDelegate *Create(TipTextService *text_service,
                                      ITfContext *context, ElementType type);

  TipUiElementDelegateFactory() = delete;
  TipUiElementDelegateFactory(const TipUiElementDelegateFactory &) = delete;
  TipUiElementDelegateFactory &operator=(const TipUiElementDelegateFactory &) =
      delete;
};

}  // namespace tsf
}  // namespace win32
}  // namespace mozc

#endif  // MOZC_WIN32_TIP_TIP_UI_ELEMENT_DELEGATE_H_
