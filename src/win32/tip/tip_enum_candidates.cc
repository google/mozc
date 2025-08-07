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

#include "win32/tip/tip_enum_candidates.h"

#include "absl/base/nullability.h"
#include "base/win32/com.h"
#include "win32/tip/tip_candidate_string.h"

namespace mozc::win32::tsf {

STDMETHODIMP TipEnumCandidates::Clone(
    IEnumTfCandidates** absl_nullable enum_candidates) {
  return SaveToOutParam(MakeComPtr<TipEnumCandidates>(candidates_),
                        enum_candidates);
}

STDMETHODIMP TipEnumCandidates::Next(
    ULONG count, ITfCandidateString** absl_nullable candidate_string,
    ULONG* absl_nullable opt_fetched_count) {
  if (candidate_string == nullptr) {
    return E_INVALIDARG;
  }
  const auto candidates_size = candidates_.size();
  for (ULONG i = 0; i < count; ++i) {
    if (current_ >= candidates_size) {
      SaveToOptionalOutParam(i, opt_fetched_count);
      return S_FALSE;
    }
    candidate_string[i] =
        MakeComPtr<TipCandidateString>(current_, candidates_[current_])
            .detach();
    ++current_;
  }
  SaveToOptionalOutParam(count, opt_fetched_count);
  return S_OK;
}

STDMETHODIMP TipEnumCandidates::Reset() {
  current_ = 0;
  return S_OK;
}

STDMETHODIMP TipEnumCandidates::Skip(ULONG count) {
  if ((candidates_.size() - current_) < count) {
    current_ = candidates_.size();
    return S_FALSE;
  }
  current_ += count;
  return S_OK;
}

}  // namespace mozc::win32::tsf
