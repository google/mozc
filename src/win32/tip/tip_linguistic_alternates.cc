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

#include "win32/tip/tip_linguistic_alternates.h"

#include <ctffunc.h>
#include <guiddef.h>
#include <wil/com.h>
#include <windows.h>

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "base/win32/com.h"
#include "base/win32/com_implements.h"
#include "win32/tip/tip_candidate_list.h"
#include "win32/tip/tip_edit_session.h"
#include "win32/tip/tip_query_provider.h"
#include "win32/tip/tip_text_service.h"

namespace mozc {
namespace win32 {

template <>
bool IsIIDOf<ITfFnGetLinguisticAlternates>(REFIID riid) {
  return IsIIDOf<ITfFnGetLinguisticAlternates, ITfFunction>(riid);
}

namespace tsf {
namespace {

#ifdef GOOGLE_JAPANESE_INPUT_BUILD
constexpr std::wstring_view kSearchCandidateProviderName =
    L"Google Japanese Input";
#else   // GOOGLE_JAPANESE_INPUT_BUILD
constexpr std::wstring_view kSearchCandidateProviderName = L"Mozc";
#endif  // GOOGLE_JAPANESE_INPUT_BUILD

}  // namespace

wil::com_ptr_nothrow<TipLinguisticAlternates> TipLinguisticAlternates::New(
    wil::com_ptr_nothrow<TipTextService> text_service) {
  std::unique_ptr<TipQueryProvider> provider(TipQueryProvider::Create());
  if (!provider) {
    return nullptr;
  }
  return MakeComPtr<TipLinguisticAlternates>(std::move(text_service),
                                             std::move(provider));
}

STDMETHODIMP TipLinguisticAlternates::GetDisplayName(BSTR *absl_nullable name) {
  return SaveToOutParam(MakeUniqueBSTR(kSearchCandidateProviderName), name);
}

// The ITfFnGetLinguisticAlternates interface method.
STDMETHODIMP
TipLinguisticAlternates::GetAlternates(
    ITfRange *absl_nullable range,
    ITfCandidateList **absl_nullable candidate_list) {
  if (range == nullptr) {
    return E_INVALIDARG;
  }
  if (candidate_list == nullptr) {
    return E_INVALIDARG;
  }
  *candidate_list = nullptr;
  std::wstring query;
  if (!TipEditSession::GetTextSync(text_service_.get(), range, &query,
                                   nullptr)) {
    return E_FAIL;
  }
  std::vector<std::wstring> candidates;
  if (!provider_->Query(query, TipQueryProvider::kDefault, &candidates)) {
    return E_FAIL;
  }
  return SaveToOutParam(
      MakeComPtr<TipCandidateList>(std::move(candidates), nullptr),
      candidate_list);
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc
