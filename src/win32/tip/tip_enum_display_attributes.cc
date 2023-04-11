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

#include "win32/tip/tip_display_attributes.h"

namespace mozc {
namespace win32 {
namespace tsf {

// Implements the IUnknown::QueryInterface() function.
HRESULT STDMETHODCALLTYPE
TipEnumDisplayAttributes::QueryInterface(REFIID interface_id, void **object) {
  if (*object == nullptr) {
    return E_INVALIDARG;
  }

  // Find a matching interface from the ones implemented by this object.
  if (::IsEqualIID(interface_id, IID_IUnknown)) {
    *object = static_cast<IUnknown *>(this);
    AddRef();
    return S_OK;
  } else if (::IsEqualIID(interface_id, IID_IEnumTfDisplayAttributeInfo)) {
    *object = static_cast<IEnumTfDisplayAttributeInfo *>(this);
    AddRef();
    return S_OK;
  }

  *object = nullptr;
  return E_NOINTERFACE;
}

// Implements the IUnknown::AddRef() function.
ULONG STDMETHODCALLTYPE TipEnumDisplayAttributes::AddRef() {
  return ref_count_.AddRefImpl();
}

// Implements the IUnknown::Release() function.
ULONG STDMETHODCALLTYPE TipEnumDisplayAttributes::Release() {
  const ULONG count = ref_count_.ReleaseImpl();
  if (count == 0) {
    delete this;
  }
  return count;
}

// Implements the IEnumTfDisplayAttributeInfo::Clone() function.
HRESULT STDMETHODCALLTYPE
TipEnumDisplayAttributes::Clone(IEnumTfDisplayAttributeInfo **enum_attributes) {
  // Check the output argument and return if it is invalid.
  if (enum_attributes == nullptr) {
    return E_INVALIDARG;
  }

  // Create a new ImeEnumDisplayAttributeInfo object.
  TipEnumDisplayAttributes *clone = new TipEnumDisplayAttributes;

  // Copy the state of the source object (except its reference count).
  clone->index_ = index_;
  *enum_attributes = clone;
  (*enum_attributes)->AddRef();
  return S_OK;
}

// Implements the IEnumTfDisplayAttributeInfo::Next() function.
// This function copies the |count| items from the current position into
// the |attribute_array|.
HRESULT STDMETHODCALLTYPE TipEnumDisplayAttributes::Next(
    ULONG count, ITfDisplayAttributeInfo **attribute_array, ULONG *fetched) {
  ULONG items = 0;
  for (; items < count; ++items) {
    ITfDisplayAttributeInfo *attribute = nullptr;
    if (index_ == 0) {
      attribute = new TipDisplayAttributeInput();
    } else if (index_ == 1) {
      attribute = new TipDisplayAttributeConverted();
    } else {
      break;
    }
    attribute_array[items] = attribute;
    attribute->AddRef();
    ++index_;
  }

  if (fetched) {
    *fetched = items;
  }

  return (items == count) ? S_OK : S_FALSE;
}

// Implements the IEnumTfDisplayAttributeInfo::Reset() function.
// This function resets the iterator of this enumeration list.
HRESULT STDMETHODCALLTYPE TipEnumDisplayAttributes::Reset() {
  index_ = 0;
  return S_OK;
}

// Implements the IEnumTfDisplayAttributeInfo::Skip() function.
// This function skips |count| items in this enumeration list.
HRESULT STDMETHODCALLTYPE TipEnumDisplayAttributes::Skip(ULONG count) {
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
