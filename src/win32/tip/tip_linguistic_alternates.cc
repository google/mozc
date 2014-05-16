// Copyright 2010-2014, Google Inc.
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

#include "win32/tip/tip_linguistic_alternates.h"

#include <Windows.h>
#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
// Workaround against KB813540
#include <atlbase_mozc.h>
#include <atlcom.h>
#include <Ctffunc.h>

#include <memory>
#include <vector>

#include "base/util.h"
#include "win32/tip/tip_candidate_list.h"
#include "win32/tip/tip_edit_session.h"
#include "win32/tip/tip_query_provider.h"
#include "win32/tip/tip_ref_count.h"
#include "win32/tip/tip_text_service.h"

using ::ATL::CComPtr;
using ::std::unique_ptr;

// ITfFnGetLinguisticAlternates is available on Windows 8.1 SDK and later.
#ifndef __ITfFnGetLinguisticAlternates_INTERFACE_DEFINED__
#define __ITfFnGetLinguisticAlternates_INTERFACE_DEFINED__

// {ea163ce2-7a65-4506-82a3-c528215da64e}
const IID IID_ITfFnGetLinguisticAlternates = {
  0xea163ce2, 0x7a65, 0x4506, {0x82, 0xa3, 0xc5, 0x28, 0x21, 0x5d, 0xa6, 0x4e}
};

// Note: "ea163ce2-7a65-4506-82a3-c528215da64e" is equivalent to
// IID_ITfFnSearchCandidateProvider
struct __declspec(uuid("ea163ce2-7a65-4506-82a3-c528215da64e"))
ITfFnGetLinguisticAlternates : public ITfFunction {
 public:
  virtual HRESULT STDMETHODCALLTYPE GetAlternates(
      ITfRange *range,
      ITfCandidateList **candidate_list) = 0;
};
#endif  // !__ITfFnSearchCandidateProvider_INTERFACE_DEFINED__

namespace mozc {
namespace win32 {
namespace tsf {

namespace {

#ifdef GOOGLE_JAPANESE_INPUT_BUILD
const wchar_t kSearchCandidateProviderName[] =
    L"Google Japanese Input";
#else
const wchar_t kSearchCandidateProviderName[] = L"Mozc";
#endif

class GetLinguisticAlternatesImpl : public ITfFnGetLinguisticAlternates {
 public:
  GetLinguisticAlternatesImpl(TipTextService *text_service,
                              TipQueryProvider *provider)
      : text_service_(text_service),
        provider_(provider) {
  }

 private:
  // The IUnknown interface methods.
  virtual HRESULT STDMETHODCALLTYPE QueryInterface(const IID &interface_id,
                                                   void **object) {
    if (!object) {
      return E_INVALIDARG;
    }

    // Find a matching interface from the ones implemented by this object.
    // This object implements IUnknown and ITfEditSession.
    if (::IsEqualIID(interface_id, IID_IUnknown)) {
      *object = static_cast<IUnknown *>(this);
    } else if (IsEqualIID(interface_id, IID_ITfFunction)) {
      *object = static_cast<ITfFunction *>(this);
    } else if (IsEqualIID(interface_id, IID_ITfFnGetLinguisticAlternates)) {
      *object = static_cast<ITfFnGetLinguisticAlternates *>(this);
    } else {
      *object = nullptr;
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

  // The ITfFunction interface method.
  virtual HRESULT STDMETHODCALLTYPE GetDisplayName(BSTR *name) {
    if (name == nullptr) {
      return E_INVALIDARG;
    }
    *name = ::SysAllocString(kSearchCandidateProviderName);
    return S_OK;
  }

  // The ITfFnGetLinguisticAlternates interface method.
  virtual HRESULT STDMETHODCALLTYPE GetAlternates(
      ITfRange *range,
      ITfCandidateList **candidate_list) {
    if (range == nullptr) {
      return E_INVALIDARG;
    }
    if (candidate_list == nullptr) {
      return E_INVALIDARG;
    }
    *candidate_list = nullptr;
    wstring query;
    if (!TipEditSession::GetTextSync(text_service_, range, &query)) {
      return E_FAIL;
    }
    std::vector<wstring> candidates;
    if (!provider_->Query(query, TipQueryProvider::kDefault, &candidates)) {
      return E_FAIL;
    }
    *candidate_list = TipCandidateList::New(candidates, nullptr);
    (*candidate_list)->AddRef();
    return S_OK;
  }

  TipRefCount ref_count_;
  CComPtr<TipTextService> text_service_;
  unique_ptr<TipQueryProvider> provider_;

  DISALLOW_COPY_AND_ASSIGN(GetLinguisticAlternatesImpl);
};

}  // namespace

// static
IUnknown *TipLinguisticAlternates::New(TipTextService *text_service) {
  auto *provider = TipQueryProvider::Create();
  if (provider == nullptr) {
    return nullptr;
  }
  return new GetLinguisticAlternatesImpl(text_service, provider);
}

// static
const IID &TipLinguisticAlternates::GetIID() {
  return IID_ITfFnGetLinguisticAlternates;
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc
