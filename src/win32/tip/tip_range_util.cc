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

#include "win32/tip/tip_range_util.h"

#include <inputscope.h>
#include <msctf.h>
#include <wil/com.h>
#include <wil/resource.h>
#include <windows.h>

#include <iterator>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "base/win32/com.h"

namespace mozc {
namespace win32 {
namespace tsf {
namespace {

// GUID_PROP_INPUTSCOPE
constexpr GUID kGuidPropInputscope = {
    0x1713dd5a,
    0x68e7,
    0x4a5b,
    {0x9a, 0xf6, 0x59, 0x2a, 0x59, 0x5c, 0x77, 0x8d}};

// TSATTRID_Text_VerticalWriting
constexpr GUID kGuidAttrIdTextVerticalWriting = {
    0x6bba8195,
    0x046f,
    0x4ea9,
    {0xb3, 0x11, 0x97, 0xfd, 0x66, 0xc4, 0x27, 0x4b}};

HRESULT GetReadOnlyAppProperty(ITfRange *range, TfEditCookie read_cookie,
                               const GUID &guid, VARIANT *variant_addr) {
  HRESULT result = S_OK;
  wil::com_ptr_nothrow<ITfContext> context;
  result = range->GetContext(&context);
  if (FAILED(result)) {
    return result;
  }

  wil::com_ptr_nothrow<ITfReadOnlyProperty> readonly_property;
  result = context->GetAppProperty(guid, &readonly_property);
  if (FAILED(result)) {
    return result;
  }
  if (!readonly_property) {
    return E_FAIL;
  }

  result = readonly_property->GetValue(read_cookie, range, variant_addr);
  if (FAILED(result)) {
    return result;
  }
  return S_OK;
}

}  // namespace

HRESULT TipRangeUtil::SetSelection(ITfContext *context,
                                   TfEditCookie edit_cookie, ITfRange *range,
                                   TfActiveSelEnd active_sel_end) {
  if (context == nullptr) {
    return E_INVALIDARG;
  }
  TF_SELECTION selections[1] = {};
  selections[0].range = range;
  selections[0].style.ase = active_sel_end;
  selections[0].style.fInterimChar = FALSE;
  return context->SetSelection(edit_cookie, std::size(selections), selections);
}

HRESULT TipRangeUtil::GetDefaultSelection(ITfContext *context,
                                          TfEditCookie edit_cookie,
                                          ITfRange **range,
                                          TfActiveSelEnd *active_sel_end) {
  if (context == nullptr) {
    return E_INVALIDARG;
  }
  TF_SELECTION selections[1] = {};
  ULONG fetched = 0;
  const HRESULT result =
      context->GetSelection(edit_cookie, TF_DEFAULT_SELECTION,
                            std::size(selections), selections, &fetched);
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

HRESULT TipRangeUtil::GetText(ITfRange *range, TfEditCookie edit_cookie,
                              std::wstring *text) {
  if (range == nullptr) {
    return E_INVALIDARG;
  }
  if (text == nullptr) {
    return E_INVALIDARG;
  }
  text->clear();

  // Create a clone of |range| so that we can move the start position while
  // reading the text via ITfRange::GetText with TF_TF_MOVESTART flag.
  wil::com_ptr_nothrow<ITfRange> range_view;
  if (FAILED(range->Clone(&range_view))) {
    return E_FAIL;
  }

  // Use a buffer on stack for shorter size case.
  {
    wchar_t buffer[64];
    ULONG fetched = 0;
    const HRESULT result = range_view->GetText(
        edit_cookie, TF_TF_MOVESTART, buffer, std::size(buffer), &fetched);
    if (FAILED(result)) {
      return result;
    }
    if (fetched > std::size(buffer)) {
      return E_UNEXPECTED;
    }
    text->append(buffer, fetched);
    if (fetched < std::size(buffer)) {
      return S_OK;
    }
  }

  // Use a buffer on heap for longer size case.
  {
    constexpr size_t kBufferSize = 1024;
    std::unique_ptr<wchar_t[]> buffer(new wchar_t[kBufferSize]);
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

HRESULT TipRangeUtil::GetInputScopes(ITfRange *range, TfEditCookie read_cookie,
                                     std::vector<InputScope> *input_scopes) {
  if (input_scopes == nullptr) {
    return E_FAIL;
  }
  input_scopes->clear();

  wil::unique_variant variant;
  HRESULT result = GetReadOnlyAppProperty(range,
                                          read_cookie,
                                          kGuidPropInputscope,
                                          variant.reset_and_addressof());
  if (FAILED(result)) {
    return result;
  }
  if (variant.vt != VT_UNKNOWN) {
    return S_OK;
  }

  auto input_scope = ComQuery<ITfInputScope>(variant.punkVal);
  InputScope *input_scopes_buffer = nullptr;
  UINT num_input_scopes = 0;
  result = input_scope->GetInputScopes(&input_scopes_buffer, &num_input_scopes);
  if (FAILED(result)) {
    return result;
  }
  input_scopes->assign(input_scopes_buffer,
                       input_scopes_buffer + num_input_scopes);
  ::CoTaskMemFree(input_scopes_buffer);
  return S_OK;
}

HRESULT TipRangeUtil::IsVerticalWriting(ITfRange *range,
                                        TfEditCookie read_cookie,
                                        bool *vertical_writing) {
  if (vertical_writing == nullptr) {
    return E_FAIL;
  }
  *vertical_writing = false;

  wil::unique_variant variant;
  HRESULT result = GetReadOnlyAppProperty(range,
                                          read_cookie,
                                          kGuidAttrIdTextVerticalWriting,
                                          variant.reset_and_addressof());
  if (FAILED(result)) {
    return result;
  }
  *vertical_writing = (variant.vt == VT_BOOL && variant.boolVal != 0);
  return S_OK;
}

bool TipRangeUtil::IsRangeCovered(TfEditCookie edit_cookie,
                                  ITfRange *range_test, ITfRange *range_cover) {
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

HRESULT TipRangeUtil::GetTextExt(ITfContextView *context_view,
                                 TfEditCookie read_cookie, ITfRange *range,
                                 RECT *rect, bool *clipped) {
  BOOL clipped_result = FALSE;
  HRESULT hr =
      context_view->GetTextExt(read_cookie, range, rect, &clipped_result);
  if (clipped != nullptr) {
    *clipped = (clipped_result != FALSE);
  }
  // Due to a bug of TSF subsystem, as of Windows 8.1 and prior,
  // ITfContextView::GetTextExt never returns TF_E_NOLAYOUT even when an
  // application returns TS_E_NOLAYOUT in ITextStoreACP::GetTextExt. Since this
  // bug is specific to ITfContextView::GetTextExt, a possible workaround is
  // to also see the returned value of ITfContextView::GetRangeFromPoint.
  // If the attached application implements ITextStoreACP::GetACPFromPoint
  // consistently with ITfContextView::GetTextExt,, we can suspect if E_FAIL
  // returned from ITfContextView::GetTextExt comes from TF_E_NOLAYOUT.
  if (hr == E_FAIL) {
    // Depending on the internal design of an application, the implementation of
    // ITextStoreACP::GetACPFromPoint can easily be computationally expensive.
    // This is why we should carefully choose parameters passed to
    // ITfContextView::GetRangeFromPoint here.
    const POINT dummy_point = {std::numeric_limits<LONG>::min(),
                               std::numeric_limits<LONG>::min()};
    wil::com_ptr_nothrow<ITfRange> dummy_range;
    const HRESULT next_hr = context_view->GetRangeFromPoint(
        read_cookie, &dummy_point, 0, &dummy_range);
    if (next_hr == TF_E_NOLAYOUT) {
      hr = TF_E_NOLAYOUT;
    }
  }
  return hr;
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc
