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

#ifndef MOZC_WIN32_TIP_TIP_SEARCH_CANDIDATE_PROVIDER_H_
#define MOZC_WIN32_TIP_TIP_SEARCH_CANDIDATE_PROVIDER_H_

#include <ctffunc.h>
#include <guiddef.h>
#include <unknwn.h>
#include <wil/com.h>

#include <memory>
#include <utility>

#include "absl/base/nullability.h"
#include "win32/tip/tip_dll_module.h"
#include "win32/tip/tip_query_provider.h"

namespace mozc {
namespace win32 {
namespace tsf {

class TipSearchCandidateProvider
    : public TipComImplements<ITfFnSearchCandidateProvider> {
 public:
  explicit TipSearchCandidateProvider(
      std::unique_ptr<TipQueryProvider> absl_nonnull provider)
      : provider_(std::move(provider)) {}

  // Returns a COM object that implements ITfFnSearchCandidateProvider.
  static wil::com_ptr_nothrow<TipSearchCandidateProvider> New();

  // ITfFunction interface methods.
  STDMETHODIMP GetDisplayName(BSTR* absl_nullable name) override;

  // ITfFnSearchCandidateProvider interface methods.
  STDMETHODIMP GetSearchCandidates(
      BSTR absl_nullable query, BSTR absl_nullable application_id,
      ITfCandidateList** absl_nullable candidate_list) override;

  STDMETHODIMP SetResult(BSTR absl_nullable query,
                         BSTR absl_nullable application_id,
                         BSTR absl_nullable result) override;

 private:
  std::unique_ptr<TipQueryProvider> provider_;
};

}  // namespace tsf
}  // namespace win32
}  // namespace mozc

#endif  // MOZC_WIN32_TIP_TIP_SEARCH_CANDIDATE_PROVIDER_H_
