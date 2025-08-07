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

#ifndef MOZC_WIN32_TIP_TIP_CANDIDATE_LIST_H_
#define MOZC_WIN32_TIP_TIP_CANDIDATE_LIST_H_

#include <ctffunc.h>
#include <guiddef.h>
#include <wil/com.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/functional/any_invocable.h"
#include "win32/tip/tip_dll_module.h"

namespace mozc {
namespace win32 {
namespace tsf {

// A callback function to be called when ITfCandidateList::SetResult is called
// with CAND_FINALIZED.
using TipCandidateOnFinalize =
    absl::AnyInvocable<void(size_t, std::wstring_view) &&>;

// Implements ITfCandidateList.
class TipCandidateList : public TipComImplements<ITfCandidateList> {
 public:
  // |on_finalize| can be empty.
  TipCandidateList(std::vector<std::wstring> candidates,
                   TipCandidateOnFinalize on_finalize)
      : candidates_(std::move(candidates)),
        on_finalize_(std::move(on_finalize)) {}

  // The ITfCandidateList interface methods.
  STDMETHODIMP EnumCandidates(
      IEnumTfCandidates** absl_nullable enum_candidate) override;
  STDMETHODIMP GetCandidate(
      ULONG index,
      ITfCandidateString** absl_nullable candidate_string) override;
  STDMETHODIMP GetCandidateNum(ULONG* absl_nullable count) override;
  STDMETHODIMP SetResult(ULONG index,
                         TfCandidateResult candidate_result) override;

 private:
  std::vector<std::wstring> candidates_;
  TipCandidateOnFinalize on_finalize_;
};

}  // namespace tsf
}  // namespace win32
}  // namespace mozc

#endif  // MOZC_WIN32_TIP_TIP_CANDIDATE_LIST_H_
