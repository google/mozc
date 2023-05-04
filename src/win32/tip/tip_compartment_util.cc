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

#include "win32/tip/tip_compartment_util.h"

#include <msctf.h>
#include <objbase.h>
#include <wil/com.h>

#include <utility>

#include "base/win32/com.h"
#include "base/win32/hresultor.h"

namespace mozc {
namespace win32 {
namespace tsf {

HRESULT TipCompartmentUtil::Set(ITfCompartmentMgr *compartment_manager,
                                const GUID &compartment_guid,
                                TfClientId client_id,
                                wil::unique_variant data) {
  if (!compartment_manager) {
    return E_POINTER;
  }

  wil::com_ptr_nothrow<ITfCompartment> compartment;
  RETURN_IF_FAILED_HRESULT(
      compartment_manager->GetCompartment(compartment_guid, &compartment));
  if (!compartment) {
    return E_FAIL;
  }

  wil::unique_variant existing_data;
  HRESULT hr = compartment->GetValue(&existing_data);
  RETURN_IF_FAILED_HRESULT(hr);
  if (hr == S_OK && VarCmp(data.addressof(), existing_data.addressof(),
                           LOCALE_USER_DEFAULT) == VARCMP_EQ) {
    // |existing_data| is equal to |data|. To avoid unnecessary event
    // from being notified, do nothing in this case.
    return S_OK;
  }

  // Remove const from |data|.
  return compartment->SetValue(client_id, data.addressof());
}

HRESULT TipCompartmentUtil::Set(ITfThreadMgr *thread_manager,
                                const GUID &compartment_guid,
                                TfClientId client_id,
                                wil::unique_variant data) {
  return Set(ComQuery<ITfCompartmentMgr>(thread_manager).get(),
             compartment_guid, client_id, std::move(data));
}

HRESULT TipCompartmentUtil::Set(ITfDocumentMgr *document_manager,
                                const GUID &compartment_guid,
                                TfClientId client_id,
                                wil::unique_variant data) {
  return Set(ComQuery<ITfCompartmentMgr>(document_manager).get(),
             compartment_guid, client_id, std::move(data));
}

HRESULT TipCompartmentUtil::Set(ITfContext *context,
                                const GUID &compartment_guid,
                                TfClientId client_id,
                                wil::unique_variant data) {
  return Set(ComQuery<ITfCompartmentMgr>(context).get(), compartment_guid,
             client_id, std::move(data));
}

HResultOr<wil::unique_variant> TipCompartmentUtil::Get(
    ITfCompartmentMgr *compartment_manager, const GUID &compartment_guid) {
  if (!compartment_manager) {
    return HResult(E_POINTER);
  }

  wil::com_ptr_nothrow<ITfCompartment> compartment;
  RETURN_IF_FAILED_HRESULT(
      compartment_manager->GetCompartment(compartment_guid, &compartment));
  if (!compartment) {
    return HResult(E_FAIL);
  }

  wil::unique_variant result;
  return HResult(compartment->GetValue(result.reset_and_addressof()));
}

HResultOr<wil::unique_variant> TipCompartmentUtil::Get(
    ITfThreadMgr *thread_manager, const GUID &compartment_guid) {
  return Get(ComQuery<ITfCompartmentMgr>(thread_manager).get(),
             compartment_guid);
}

HResultOr<wil::unique_variant> TipCompartmentUtil::Get(
    ITfDocumentMgr *document_manager, const GUID &compartment_guid) {
  return Get(ComQuery<ITfCompartmentMgr>(document_manager).get(),
             compartment_guid);
}

HResultOr<wil::unique_variant> TipCompartmentUtil::Get(
    ITfContext *context, const GUID &compartment_guid) {
  return Get(ComQuery<ITfCompartmentMgr>(context).get(), compartment_guid);
}

HResultOr<wil::unique_variant> TipCompartmentUtil::GetAndEnsureDataExists(
    ITfCompartmentMgr *compartment_manager, const GUID &compartment_guid,
    TfClientId client_id, wil::unique_variant default_data) {
  if (compartment_manager == nullptr) {
    return HResult(E_POINTER);
  }

  wil::com_ptr_nothrow<ITfCompartment> compartment;
  RETURN_IF_FAILED_HRESULT(
      compartment_manager->GetCompartment(compartment_guid, &compartment));
  if (!compartment) {
    return HResult(E_FAIL);
  }

  wil::unique_variant result;
  const HRESULT hr = compartment->GetValue(result.reset_and_addressof());
  if (FAILED(hr)) {
    return HResult(hr);
  }
  if (hr == S_FALSE) {
    RETURN_IF_FAILED_HRESULT(
        compartment->SetValue(client_id, default_data.addressof()));
    result = std::move(default_data);
  }
  return result;
}

HResultOr<wil::unique_variant> TipCompartmentUtil::GetAndEnsureDataExists(
    ITfThreadMgr *thread_manager, const GUID &compartment_guid,
    TfClientId client_id, wil::unique_variant default_data) {
  return GetAndEnsureDataExists(
      ComQuery<ITfCompartmentMgr>(thread_manager).get(), compartment_guid,
      client_id, std::move(default_data));
}

HResultOr<wil::unique_variant> TipCompartmentUtil::GetAndEnsureDataExists(
    ITfDocumentMgr *document_manager, const GUID &compartment_guid,
    TfClientId client_id, wil::unique_variant default_data) {
  return GetAndEnsureDataExists(
      ComQuery<ITfCompartmentMgr>(document_manager).get(), compartment_guid,
      client_id, std::move(default_data));
}

HResultOr<wil::unique_variant> TipCompartmentUtil::GetAndEnsureDataExists(
    ITfContext *context, const GUID &compartment_guid, TfClientId client_id,
    wil::unique_variant default_data) {
  return GetAndEnsureDataExists(ComQuery<ITfCompartmentMgr>(context).get(),
                                compartment_guid, client_id,
                                std::move(default_data));
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc
