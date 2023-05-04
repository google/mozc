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

#include <ctffunc.h>
#include <objbase.h>
#include <oleauto.h>
#include <unknwn.h>
#include <wil/com.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/win32/com.h"
#include "base/win32/com_implements.h"
#include "win32/tip/tip_candidate_list.h"
#include "win32/tip/tip_dll_module.h"
#include "win32/tip/tip_query_provider.h"

namespace mozc {
namespace win32 {

template <>
bool IsIIDOf<ITfFnSearchCandidateProvider>(REFIID riid) {
  return IsIIDOf<ITfFnSearchCandidateProvider, ITfFunction>(riid);
}

namespace tsf {
namespace {

#ifdef GOOGLE_JAPANESE_INPUT_BUILD
constexpr wchar_t kSearchCandidateProviderName[] = L"Google Japanese Input";
#else   // GOOGLE_JAPANESE_INPUT_BUILD
constexpr wchar_t kSearchCandidateProviderName[] = L"Mozc";
#endif  // GOOGLE_JAPANESE_INPUT_BUILD

class SearchCandidateProviderImpl final
    : public TipComImplements<ITfFnSearchCandidateProvider> {
 public:
  explicit SearchCandidateProviderImpl(
      std::unique_ptr<TipQueryProvider> provider)
      : provider_(std::move(provider)) {}

  // The ITfFunction interface method.
  STDMETHODIMP GetDisplayName(BSTR *name) override {
    if (name == nullptr) {
      return E_INVALIDARG;
    }
    *name = ::SysAllocString(kSearchCandidateProviderName);
    return S_OK;
  }

  // The ITfFnSearchCandidateProvider interface method.
  STDMETHODIMP GetSearchCandidates(BSTR query, BSTR application_id,
                                   ITfCandidateList **candidate_list) override {
    if (candidate_list == nullptr) {
      return E_INVALIDARG;
    }
    std::vector<std::wstring> candidates;
    if (!provider_->Query(query, TipQueryProvider::kDefault, &candidates)) {
      return E_FAIL;
    }
    *candidate_list =
        TipCandidateList::New(std::move(candidates), nullptr).detach();
    return S_OK;
  }

  STDMETHODIMP SetResult(BSTR query, BSTR application_id,
                         BSTR result) override {
    // Not implemented.
    return S_OK;
  }

  std::unique_ptr<TipQueryProvider> provider_;
};

}  // namespace

// static
wil::com_ptr_nothrow<IUnknown> TipSearchCandidateProvider::New() {
  std::unique_ptr<TipQueryProvider> provider = TipQueryProvider::Create();
  if (!provider) {
    return nullptr;
  }
  return MakeComPtr<SearchCandidateProviderImpl>(std::move(provider));
}

// static
const IID &TipSearchCandidateProvider::GetIID() {
  return IID_ITfFnSearchCandidateProvider;
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc
