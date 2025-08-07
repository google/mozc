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

#include "win32/tip/tip_enum_display_attributes.h"

#include <msctf.h>
#include <objbase.h>
#include <unknwn.h>
#include <wil/com.h>

#include <utility>

#include "absl/base/nullability.h"
#include "base/win32/com.h"
#include "win32/tip/tip_display_attributes.h"

namespace mozc {
namespace win32 {
namespace tsf {

// Implements the IEnumTfDisplayAttributeInfo::Clone() function.
STDMETHODIMP
TipEnumDisplayAttributes::Clone(
    IEnumTfDisplayAttributeInfo **absl_nullable enum_attributes) {
  // Check the output argument and return if it is invalid.
  if (enum_attributes == nullptr) {
    return E_INVALIDARG;
  }

  auto clone = MakeComPtr<TipEnumDisplayAttributes>();
  // Copy the state of the source object (except its reference count).
  clone->index_ = index_;

  return SaveToOutParam(std::move(clone), enum_attributes);
}

// Implements the IEnumTfDisplayAttributeInfo::Next() function.
// This function copies the |count| items from the current position into
// the |attribute_array|.
STDMETHODIMP TipEnumDisplayAttributes::Next(
    ULONG count, ITfDisplayAttributeInfo **absl_nonnull attribute_array,
    ULONG *absl_nullable fetched) {
  ULONG items = 0;
  for (; items < count; ++items) {
    wil::com_ptr_nothrow<ITfDisplayAttributeInfo> attribute;
    if (index_ == 0) {
      attribute_array[items] = MakeComPtr<TipDisplayAttributeInput>().detach();
    } else if (index_ == 1) {
      attribute_array[items] =
          MakeComPtr<TipDisplayAttributeConverted>().detach();
    } else {
      break;
    }
    ++index_;
  }
  SaveToOptionalOutParam(items, fetched);

  return (items == count) ? S_OK : S_FALSE;
}

// Implements the IEnumTfDisplayAttributeInfo::Reset() function.
// This function resets the iterator of this enumeration list.
STDMETHODIMP TipEnumDisplayAttributes::Reset() {
  index_ = 0;
  return S_OK;
}

// Implements the IEnumTfDisplayAttributeInfo::Skip() function.
// This function skips |count| items in this enumeration list.
STDMETHODIMP TipEnumDisplayAttributes::Skip(ULONG count) {
  // There is only a single item to enum
  // so just skip it and avoid any overflow errors
  if (count > 0 && index_ == 0) {
    ++index_;
  }
  return S_OK;
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc
