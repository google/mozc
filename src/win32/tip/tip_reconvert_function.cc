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

#include "win32/tip/tip_reconvert_function.h"

#include <ctffunc.h>
#include <msctf.h>
#include <wil/com.h>
#include <windows.h>

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "base/win32/com.h"
#include "win32/tip/tip_candidate_list.h"
#include "win32/tip/tip_edit_session.h"
#include "win32/tip/tip_query_provider.h"
#include "win32/tip/tip_surrounding_text.h"

namespace mozc {
namespace win32 {
namespace tsf {
namespace {

#ifdef GOOGLE_JAPANESE_INPUT_BUILD
constexpr std::wstring_view kReconvertFunctionDisplayName =
    L"Google Japanese Input: Reconversion Function";
#else   // GOOGLE_JAPANESE_INPUT_BUILD
constexpr std::wstring_view kReconvertFunctionDisplayName =
    L"Mozc: Reconversion Function";
#endif  // GOOGLE_JAPANESE_INPUT_BUILD

}  // namespace

STDMETHODIMP TipReconvertFunction::GetDisplayName(BSTR *absl_nullable name) {
  return SaveToOutParam(MakeUniqueBSTR(kReconvertFunctionDisplayName), name);
}

STDMETHODIMP TipReconvertFunction::QueryRange(
    ITfRange *absl_nullable range, ITfRange **absl_nullable new_range,
    BOOL *absl_nullable opt_convertible) {
  if (range == nullptr) {
    return E_INVALIDARG;
  }
  if (new_range == nullptr) {
    return E_INVALIDARG;
  }
  *new_range = nullptr;

  wil::com_ptr_nothrow<ITfContext> context;
  if (FAILED(range->GetContext(&context))) {
    return E_FAIL;
  }

  TipSurroundingTextInfo info;
  if (!TipSurroundingText::Get(text_service_.get(), context.get(), &info)) {
    return E_FAIL;
  }

  if (info.in_composition) {
    // on-going composition is found.
    SaveToOptionalOutParam(FALSE, opt_convertible);
    return S_OK;
  }

  if (info.selected_text.find(static_cast<wchar_t>(TS_CHAR_EMBEDDED)) !=
      std::wstring::npos) {
    // embedded object is found.
    SaveToOptionalOutParam(FALSE, opt_convertible);
    return S_OK;
  }

  if (FAILED(range->Clone(new_range))) {
    return E_FAIL;
  }
  SaveToOptionalOutParam(TRUE, opt_convertible);
  return S_OK;
}

STDMETHODIMP
TipReconvertFunction::GetReconversion(
    ITfRange *absl_nullable range,
    ITfCandidateList **absl_nullable candidate_list) {
  if (range == nullptr) {
    return E_INVALIDARG;
  }
  std::unique_ptr<TipQueryProvider> provider(TipQueryProvider::Create());
  if (!provider) {
    return E_FAIL;
  }
  std::wstring query;
  if (!TipEditSession::GetTextSync(text_service_.get(), range, &query)) {
    return E_FAIL;
  }
  std::vector<std::wstring> candidates;
  if (!provider->Query(query, TipQueryProvider::kReconversion, &candidates)) {
    return E_FAIL;
  }
  return SaveToOutParam(MakeComPtr<TipCandidateList>(
                            std::move(candidates), OnCandidateFinalize(range)),
                        candidate_list);
}

TipCandidateOnFinalize TipReconvertFunction::OnCandidateFinalize(
    wil::com_ptr_nothrow<ITfRange> range) const {
  return [this, range = std::move(range)](size_t index,
                                          std::wstring_view candidate) {
    TipEditSession::SetTextAsync(text_service_.get(), candidate, range.get());
  };
}

STDMETHODIMP TipReconvertFunction::Reconvert(ITfRange *absl_nullable range) {
  if (range == nullptr) {
    return E_INVALIDARG;
  }

  if (!TipEditSession::ReconvertFromApplicationSync(text_service_.get(),
                                                    range)) {
    return E_FAIL;
  }
  return S_OK;
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc
