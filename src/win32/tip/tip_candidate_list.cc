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

#include "win32/tip/tip_candidate_list.h"

#include <ctffunc.h>
#include <objbase.h>
#include <oleauto.h>

#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "base/win32/com.h"
#include "win32/tip/tip_candidate_string.h"
#include "win32/tip/tip_enum_candidates.h"

namespace mozc {
namespace win32 {
namespace tsf {

STDMETHODIMP TipCandidateList::EnumCandidates(
    IEnumTfCandidates** absl_nullable enum_candidate) {
  return SaveToOutParam(MakeComPtr<TipEnumCandidates>(candidates_),
                        enum_candidate);
}

STDMETHODIMP TipCandidateList::GetCandidate(
    ULONG index, ITfCandidateString** absl_nullable candidate_string) {
  if (index >= candidates_.size()) {
    return E_FAIL;
  }
  return SaveToOutParam(
      MakeComPtr<TipCandidateString>(index, candidates_[index]),
      candidate_string);
}

STDMETHODIMP TipCandidateList::GetCandidateNum(ULONG* absl_nullable count) {
  return SaveToOutParam(candidates_.size(), count);
}

STDMETHODIMP TipCandidateList::SetResult(ULONG index,
                                         TfCandidateResult candidate_result) {
  if (candidates_.size() <= index) {
    return E_INVALIDARG;
  }
  if (candidate_result == CAND_FINALIZED && on_finalize_) {
    std::move(on_finalize_)(index, candidates_[index]);
    on_finalize_ = nullptr;
  }
  return S_OK;
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc
