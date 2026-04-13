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

#ifndef MOZC_WIN32_TIP_TIP_RECONVERT_FUNCTION_H_
#define MOZC_WIN32_TIP_TIP_RECONVERT_FUNCTION_H_

#include <ctffunc.h>
#include <msctf.h>
#include <wil/com.h>

#include <utility>

#include "absl/base/nullability.h"
#include "base/win32/com_implements.h"
#include "win32/tip/tip_candidate_list.h"
#include "win32/tip/tip_dll_module.h"
#include "win32/tip/tip_text_service.h"

namespace mozc {
namespace win32 {

template <>
inline bool IsIIDOf<ITfFnReconversion>(REFIID riid) {
  return IsIIDOf<ITfFnReconversion, ITfFunction>(riid);
}

namespace tsf {

// A TSF function object that can be use to invoke reconversion from an
// application.
class TipReconvertFunction : public TipComImplements<ITfFnReconversion> {
 public:
  explicit TipReconvertFunction(
      wil::com_ptr_nothrow<TipTextService> text_service)
      : text_service_(std::move(text_service)) {}

  // The ITfFunction interface method.
  STDMETHODIMP GetDisplayName(BSTR* absl_nullable name) override;

  // The ITfFnReconversion interface methods.
  STDMETHODIMP QueryRange(ITfRange* absl_nullable range,
                          ITfRange** absl_nullable new_range,
                          BOOL* absl_nullable opt_convertible) override;
  STDMETHODIMP
  GetReconversion(ITfRange* absl_nullable range,
                  ITfCandidateList** absl_nullable candidate_list) override;
  STDMETHODIMP Reconvert(ITfRange* absl_nullable range) override;

 private:
  TipCandidateOnFinalize OnCandidateFinalize(
      wil::com_ptr_nothrow<ITfRange> range) const;

  wil::com_ptr_nothrow<TipTextService> text_service_;
};

}  // namespace tsf
}  // namespace win32
}  // namespace mozc

#endif  // MOZC_WIN32_TIP_TIP_RECONVERT_FUNCTION_H_
