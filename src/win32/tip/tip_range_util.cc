// Copyright 2010-2013, Google Inc.
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

#include "win32/tip/tip_range_util.h"

#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
// Workaround against KB813540
#include <atlbase_mozc.h>
#include <atlcom.h>

#include "base/scoped_ptr.h"

namespace mozc {
namespace win32 {
namespace tsf {

using ATL::CComPtr;

HRESULT TipRangeUtil::SetSelection(
      ITfContext *context, TfEditCookie edit_cookie, ITfRange *range,
      TfActiveSelEnd active_sel_end) {
  if (context == nullptr) {
    return E_INVALIDARG;
  }
  TF_SELECTION selections[1] = {};
  selections[0].range = range;
  selections[0].style.ase = active_sel_end;
  selections[0].style.fInterimChar = FALSE;
  return context->SetSelection(edit_cookie, arraysize(selections), selections);
}

HRESULT TipRangeUtil::GetDefaultSelection(
    ITfContext *context, TfEditCookie edit_cookie, ITfRange **range,
    TfActiveSelEnd *active_sel_end) {
  if (context == nullptr) {
    return E_INVALIDARG;
  }
  TF_SELECTION selections[1] = {};
  ULONG fetched = 0;
  const HRESULT result = context->GetSelection(
      edit_cookie, TF_DEFAULT_SELECTION, arraysize(selections), selections,
      &fetched);
  if (FAILED(result)) {
    return result;
  }
  if (fetched != 1) {
    return E_FAIL;
  }
  if (range != nullptr) {
    *range = selections[0].range;
    (*range)->AddRef();
  }
  if (active_sel_end != nullptr) {
    *active_sel_end = selections[0].style.ase;
  }

  return S_OK;
}

HRESULT TipRangeUtil::GetText(
    ITfRange *range, TfEditCookie edit_cookie, wstring *text) {
  if (range == nullptr) {
    return E_INVALIDARG;
  }
  if (text == nullptr) {
    return E_INVALIDARG;
  }
  text->clear();

  // Create a clone of |range| so that we can move the start position while
  // reading the text via ITfRange::GetText with TF_TF_MOVESTART flag.
  CComPtr<ITfRange> range_view;
  if (FAILED(range->Clone(&range_view))) {
    return E_FAIL;
  }

  // Use a buffer on stack for shorter size case.
  {
    wchar_t buffer[64];
    ULONG fetched = 0;
    const HRESULT result = range_view->GetText(
        edit_cookie, TF_TF_MOVESTART, buffer, arraysize(buffer), &fetched);
    if (FAILED(result)) {
      return result;
    }
    if (fetched > arraysize(buffer)) {
      return E_UNEXPECTED;
    }
    text->append(buffer, fetched);
    if (fetched < arraysize(buffer)) {
      return S_OK;
    }
  }

  // Use a buffer on heap for longer size case.
  {
    const size_t kBufferSize = 1024;
    scoped_array<wchar_t> buffer(new wchar_t[kBufferSize]);
    while (true) {
      ULONG fetched = 0;
      const HRESULT result = range_view->GetText(
          edit_cookie, TF_TF_MOVESTART, buffer.get(), kBufferSize, &fetched);
      if (FAILED(result)) {
        return result;
      }
      if (fetched > kBufferSize) {
        return E_UNEXPECTED;
      }
      text->append(buffer.get(), fetched);
      if (fetched < kBufferSize) {
        break;
      }
    }
  }
  return S_OK;
}

// Checks whether or not |range_test| becomes a subset of |range_cover|.
bool TipRangeUtil::IsRangeCovered(TfEditCookie edit_cookie,
                                  ITfRange* range_test,
                                  ITfRange* range_cover) {
  HRESULT result = S_OK;

  // Check if {the start position of |range_cover|) <= {the start position
  // of |range_test|}.
  LONG position = 0;
  result = range_cover->CompareStart(edit_cookie, range_test, TF_ANCHOR_START,
                                     &position);
  if (result != S_OK || position > 0) {
    return false;
  }

  // Check if {the end position of |range_cover|} >= {the end position of
  // |range_test|}.
  result = range_cover->CompareEnd(edit_cookie, range_test, TF_ANCHOR_END,
                                   &position);
  if (result != S_OK || position < 0) {
    return false;
  }

  return true;
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc
