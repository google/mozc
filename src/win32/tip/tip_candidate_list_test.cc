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

#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
#include <atlbase.h>
#include <atlcom.h>
#include <ctffunc.h>

#include <string>
#include <vector>

#include "testing/googletest.h"
#include "testing/gunit.h"
#include "win32/tip/tip_dll_module.h"

namespace mozc {
namespace win32 {
namespace tsf {
namespace {

using ATL::CComBSTR;
using ATL::CComPtr;
using testing::AssertionFailure;
using testing::AssertionResult;
using testing::AssertionSuccess;

class TipCandidateListTest : public testing::Test {
 protected:
  static void SetUpTestCase() { TipDllModule::InitForUnitTest(); }
};

class MockCallbackResult {
 public:
  MockCallbackResult() : on_finalize_called_(false), index_(0) {}
  MockCallbackResult(const MockCallbackResult &) = delete;
  MockCallbackResult &operator=(const MockCallbackResult &) = delete;

  void Reset() {
    on_finalize_called_ = false;
    index_ = 0;
    candidate_.clear();
  }

  void OnFinalize(size_t index, const std::wstring &candidate) {
    on_finalize_called_ = true;
    index_ = index;
    candidate_ = candidate;
  }

  bool on_finalize_called() const { return on_finalize_called_; }

  size_t index() const { return index_; }

  const std::wstring &candidate() const { return candidate_; }

 private:
  bool on_finalize_called_;
  size_t index_;
  std::wstring candidate_;
};

class MockCallback : public TipCandidateListCallback {
 public:
  // Does not take ownership of |result|.
  explicit MockCallback(MockCallbackResult *result) : result_(result) {}
  MockCallback(const MockCallback &) = delete;
  MockCallback &operator=(const MockCallback &) = delete;

 private:
  // TipCandidateListCallback overrides:
  virtual void OnFinalize(size_t index, const std::wstring &candidate) {
    result_->OnFinalize(index, candidate);
  }

  MockCallbackResult *result_;
};

std::wstring ToWStr(const CComBSTR &bstr) {
  return std::wstring(static_cast<const wchar_t *>(bstr), bstr.Length());
}

AssertionResult ExpectCandidateString(ULONG expected_index,
                                      const std::wstring &expected_text,
                                      CComPtr<ITfCandidateString> candidate) {
  if (candidate == nullptr) {
    return AssertionFailure() << "|actual| should be non-null";
  }
  HRESULT hr = S_OK;
  {
    ULONG index = 0;
    hr = candidate->GetIndex(&index);
    if (FAILED(hr)) {
      return AssertionFailure() << "ITfCandidateString::GetIndex failed."
                                << " hr = " << hr;
    }
    if (expected_index != index) {
      return AssertionFailure()
             << "expected: " << expected_index << ", actual: " << index;
    }
  }
  {
    CComBSTR str;
    hr = candidate->GetString(&str);
    if (FAILED(hr)) {
      return AssertionFailure() << "ITfCandidateString::GetString failed."
                                << " hr = " << hr;
    }
    const std::wstring wstr(ToWStr(str));
    if (expected_text != wstr) {
      return AssertionFailure()
             << "expected: " << expected_text << ", actual: " << wstr;
    }
  }
  return AssertionSuccess();
}

#define EXPECT_CANDIDATE_STR(expected_index, expected_str, actual) \
  EXPECT_PRED3(ExpectCandidateString, expected_index, expected_str, actual)

TEST(TipCandidateListTest, EmptyCandidate) {
  MockCallbackResult result;

  std::vector<std::wstring> empty;
  CComPtr<ITfCandidateList> candidate_list(
      TipCandidateList::New(empty, new MockCallback(&result)));
  ASSERT_NE(candidate_list, nullptr);

  ULONG num = 0;
  EXPECT_HRESULT_SUCCEEDED(candidate_list->GetCandidateNum(&num));
  EXPECT_EQ(num, 0);

  CComPtr<ITfCandidateString> str;
  EXPECT_HRESULT_FAILED(candidate_list->GetCandidate(0, &str));
  EXPECT_EQ(str, nullptr);

  CComPtr<IEnumTfCandidates> enum_candidates;
  EXPECT_HRESULT_SUCCEEDED(candidate_list->EnumCandidates(&enum_candidates));
  ASSERT_NE(enum_candidates, nullptr);

  ITfCandidateString *buffer[3] = {};
  ULONG num_fetched = 0;
  EXPECT_EQ(enum_candidates->Next(std::size(buffer), buffer, &num_fetched),
            S_FALSE);
  EXPECT_EQ(num_fetched, 0);

  EXPECT_FALSE(result.on_finalize_called());

  // SetResult() should do nothing because it is out of index.
  EXPECT_HRESULT_FAILED(candidate_list->SetResult(1, CAND_FINALIZED));
  EXPECT_FALSE(result.on_finalize_called());
}

TEST(TipCandidateListTest, NonEmptyCandidates) {
  MockCallbackResult result;

  std::vector<std::wstring> source;
  for (wchar_t c = L'A'; c < L'Z'; ++c) {
    source.push_back(std::wstring(c, 1));
  }
  CComPtr<ITfCandidateList> candidate_list(
      TipCandidateList::New(source, new MockCallback(&result)));
  ASSERT_NE(candidate_list, nullptr);

  ULONG num = 0;
  EXPECT_HRESULT_SUCCEEDED(candidate_list->GetCandidateNum(&num));
  ASSERT_EQ(num, source.size());

  for (size_t i = 0; i < source.size(); ++i) {
    CComPtr<ITfCandidateString> candidate_str;
    EXPECT_HRESULT_SUCCEEDED(candidate_list->GetCandidate(i, &candidate_str));
    EXPECT_CANDIDATE_STR(i, source[i], candidate_str);
  }

  CComPtr<IEnumTfCandidates> enum_candidates;
  EXPECT_HRESULT_SUCCEEDED(candidate_list->EnumCandidates(&enum_candidates));
  EXPECT_NE(enum_candidates, nullptr);

  size_t offset = 0;
  while (offset < source.size()) {
    constexpr size_t kBufferSize = 10;
    ITfCandidateString *buffer[kBufferSize] = {};
    CComPtr<ITfCandidateString> strings[kBufferSize];
    ULONG num_fetched = 0;
    HRESULT hr = enum_candidates->Next(kBufferSize, buffer, &num_fetched);
    for (size_t i = 0; i < kBufferSize; ++i) {
      strings[i].Attach(buffer[i]);
    }
    const size_t rest = source.size() - offset;
    if (rest > kBufferSize) {
      EXPECT_EQ(hr, S_OK);
      ASSERT_EQ(num_fetched, kBufferSize);
    } else {
      EXPECT_EQ(hr, S_FALSE);
      ASSERT_EQ(num_fetched, rest);
    }
    for (size_t buffer_index = 0; buffer_index < num_fetched; ++buffer_index) {
      const size_t index = offset + buffer_index;
      EXPECT_CANDIDATE_STR(index, source[index], strings[buffer_index]);
    }
    offset += num_fetched;
  }

  // Test ITfCandidateList::SetResult hereafter.
  EXPECT_FALSE(result.on_finalize_called());

  EXPECT_HRESULT_SUCCEEDED(candidate_list->SetResult(1, CAND_SELECTED));
  EXPECT_FALSE(result.on_finalize_called());

  EXPECT_HRESULT_SUCCEEDED(candidate_list->SetResult(1, CAND_FINALIZED));
  EXPECT_TRUE(result.on_finalize_called());
  EXPECT_EQ(result.index(), 1);
  EXPECT_EQ(result.candidate(), source[1]);

  result.Reset();

  // Second CAND_FINALIZED must not fire the callback.
  EXPECT_HRESULT_SUCCEEDED(candidate_list->SetResult(1, CAND_FINALIZED));
  EXPECT_FALSE(result.on_finalize_called());
}

}  // namespace
}  // namespace tsf
}  // namespace win32
}  // namespace mozc
