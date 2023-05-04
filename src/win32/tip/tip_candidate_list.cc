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
#include <wil/com.h>

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/win32/com.h"
#include "win32/tip/tip_dll_module.h"

namespace mozc {
namespace win32 {
namespace tsf {
namespace {

class CandidateStringImpl final : public TipComImplements<ITfCandidateString> {
 public:
  CandidateStringImpl(ULONG index, std::wstring value)
      : index_(index), value_(std::move(value)) {}

 private:
  // The ITfCandidateString interface methods.
  STDMETHODIMP GetString(BSTR *str) override {
    if (str == nullptr) {
      return E_INVALIDARG;
    }
    *str = ::SysAllocStringLen(value_.data(), value_.size());
    return S_OK;
  }

  STDMETHODIMP GetIndex(ULONG *index) override {
    if (index == nullptr) {
      return E_INVALIDARG;
    }
    *index = index_;
    return S_OK;
  }

  const ULONG index_;
  const std::wstring value_;
};

class EnumTfCandidatesImpl final : public TipComImplements<IEnumTfCandidates> {
 public:
  explicit EnumTfCandidatesImpl(std::vector<std::wstring> candidates)
      : candidates_(std::move(candidates)), current_(0) {}

 private:
  STDMETHODIMP Clone(IEnumTfCandidates **enum_candidates) override {
    if (enum_candidates == nullptr) {
      return E_INVALIDARG;
    }
    auto impl = MakeComPtr<EnumTfCandidatesImpl>(candidates_);
    if (!impl) {
      return E_OUTOFMEMORY;
    }
    *enum_candidates = impl.detach();
    return S_OK;
  }

  STDMETHODIMP Next(ULONG count, ITfCandidateString **candidate_string,
                    ULONG *fetched_count) override {
    if (candidate_string == nullptr) {
      return E_INVALIDARG;
    }
    ULONG dummy = 0;
    if (fetched_count == nullptr) {
      fetched_count = &dummy;
    }
    const auto candidates_size = candidates_.size();
    *fetched_count = 0;
    for (ULONG i = 0; i < count; ++i) {
      if (current_ >= candidates_size) {
        *fetched_count = i;
        return S_FALSE;
      }
      candidate_string[i] =
          MakeComPtr<CandidateStringImpl>(current_, candidates_[current_])
              .detach();
      ++current_;
    }
    *fetched_count = count;
    return S_OK;
  }

  STDMETHODIMP Reset() override {
    current_ = 0;
    return S_OK;
  }

  STDMETHODIMP Skip(ULONG count) override {
    if ((candidates_.size() - current_) < count) {
      current_ = candidates_.size();
      return S_FALSE;
    }
    current_ += count;
    return S_OK;
  }

  std::vector<std::wstring> candidates_;
  size_t current_;
};

class CandidateListImpl final : public TipComImplements<ITfCandidateList> {
 public:
  CandidateListImpl(std::vector<std::wstring> candidates,
                    std::unique_ptr<TipCandidateListCallback> callback)
      : candidates_(std::move(candidates)), callback_(std::move(callback)) {}

 private:
  // The ITfCandidateList interface methods.
  STDMETHODIMP EnumCandidates(IEnumTfCandidates **enum_candidate) override {
    if (enum_candidate == nullptr) {
      return E_INVALIDARG;
    }
    auto impl = MakeComPtr<EnumTfCandidatesImpl>(candidates_);
    if (!impl) {
      return E_OUTOFMEMORY;
    }
    *enum_candidate = impl.detach();
    return S_OK;
  }

  STDMETHODIMP GetCandidate(ULONG index,
                            ITfCandidateString **candidate_string) override {
    if (candidate_string == nullptr) {
      return E_INVALIDARG;
    }
    if (index >= candidates_.size()) {
      return E_FAIL;
    }
    *candidate_string =
        MakeComPtr<CandidateStringImpl>(index, candidates_[index]).detach();
    return S_OK;
  }

  STDMETHODIMP GetCandidateNum(ULONG *count) override {
    if (count == nullptr) {
      return E_INVALIDARG;
    }
    *count = static_cast<ULONG>(candidates_.size());
    return S_OK;
  }

  STDMETHODIMP SetResult(ULONG index,
                         TfCandidateResult candidate_result) override {
    if (candidates_.size() <= index) {
      return E_INVALIDARG;
    }
    if (candidate_result == CAND_FINALIZED && callback_) {
      callback_->OnFinalize(index, candidates_[index]);
      callback_.reset();
    }
    return S_OK;
  }

  std::vector<std::wstring> candidates_;
  std::unique_ptr<TipCandidateListCallback> callback_;
};

}  // namespace

// static
wil::com_ptr_nothrow<ITfCandidateList> TipCandidateList::New(
    std::vector<std::wstring> candidates,
    std::unique_ptr<TipCandidateListCallback> callback) {
  return MakeComPtr<CandidateListImpl>(std::move(candidates),
                                       std::move(callback));
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc
