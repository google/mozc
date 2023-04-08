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

#include "win32/tip/tip_search_candidate_provider.h"

#include <windows.h>
#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
#include <ctffunc.h>
#include <atlbase.h>
#include <atlcom.h>

#include <memory>
#include <vector>

#include "base/util.h"
#include "win32/tip/tip_candidate_list.h"
#include "win32/tip/tip_query_provider.h"
#include "win32/tip/tip_ref_count.h"

using ::ATL::CComPtr;

namespace mozc {
namespace win32 {
namespace tsf {
namespace {

#ifdef GOOGLE_JAPANESE_INPUT_BUILD
constexpr wchar_t kSearchCandidateProviderName[] = L"Google Japanese Input";
#else   // GOOGLE_JAPANESE_INPUT_BUILD
constexpr wchar_t kSearchCandidateProviderName[] = L"Mozc";
#endif  // GOOGLE_JAPANESE_INPUT_BUILD

class SearchCandidateProviderImpl final : public ITfFnSearchCandidateProvider {
 public:
  explicit SearchCandidateProviderImpl(TipQueryProvider *provider)
      : provider_(provider) {}
  SearchCandidateProviderImpl(const SearchCandidateProviderImpl &) = delete;
  SearchCandidateProviderImpl &operator=(const SearchCandidateProviderImpl &) =
      delete;

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
    } else if (IsEqualIID(interface_id, IID_ITfFnSearchCandidateProvider)) {
      *object = static_cast<ITfFnSearchCandidateProvider *>(this);
    } else {
      *object = nullptr;
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

  // The ITfFunction interface method.
  virtual HRESULT STDMETHODCALLTYPE GetDisplayName(BSTR *name) {
    if (name == nullptr) {
      return E_INVALIDARG;
    }
    *name = ::SysAllocString(kSearchCandidateProviderName);
    return S_OK;
  }

  // The ITfFnSearchCandidateProvider interface method.
  virtual HRESULT STDMETHODCALLTYPE GetSearchCandidates(
      BSTR query, BSTR application_id, ITfCandidateList **candidate_list) {
    if (candidate_list == nullptr) {
      return E_INVALIDARG;
    }
    std::vector<std::wstring> candidates;
    if (!provider_->Query(query, TipQueryProvider::kDefault, &candidates)) {
      return E_FAIL;
    }
    *candidate_list = TipCandidateList::New(candidates, nullptr);
    (*candidate_list)->AddRef();
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE SetResult(BSTR query, BSTR application_id,
                                              BSTR result) {
    // Not implemented.
    return S_OK;
  }

  TipRefCount ref_count_;
  std::unique_ptr<TipQueryProvider> provider_;
};

}  // namespace

// static
IUnknown *TipSearchCandidateProvider::New() {
  auto *provider = TipQueryProvider::Create();
  if (provider == nullptr) {
    return nullptr;
  }
  return new SearchCandidateProviderImpl(provider);
}

// static
const IID &TipSearchCandidateProvider::GetIID() {
  return IID_ITfFnSearchCandidateProvider;
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc
