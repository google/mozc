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

#include "win32/tip/tip_compartment_util.h"

namespace mozc {
namespace win32 {
namespace tsf {

using ATL::CComPtr;
using ATL::CComQIPtr;
using ATL::CComVariant;

bool TipCompartmentUtil::Set(ITfCompartmentMgr *compartment_manager,
                             const GUID &compartment_guid,
                             TfClientId client_id,
                             const ATL::CComVariant &data) {
  if (!compartment_manager) {
    return false;
  }

  CComPtr<ITfCompartment> compartment;
  if (FAILED(compartment_manager->GetCompartment(compartment_guid,
                                                 &compartment))) {
    return false;
  }
  if (!compartment) {
    return false;
  }

  CComVariant existing_data;
  const HRESULT result = compartment->GetValue(&existing_data);
  if (FAILED(result)) {
    return false;
  }
  if (result == S_OK && data == existing_data) {
    // |existing_data| is equal to |data|. To avoid unnecessary event
    // from being notified, do nothing in this case.
    return true;
  }

  // Remove const from |data|.
  CComVariant nonconst_data(data);
  return SUCCEEDED(compartment->SetValue(client_id, &nonconst_data));
}

bool TipCompartmentUtil::Set(ITfThreadMgr *thread_manager,
                             const GUID &compartment_guid,
                             TfClientId client_id,
                             const ATL::CComVariant &data) {
  CComQIPtr<ITfCompartmentMgr> compartment_manager(thread_manager);
  return Set(compartment_manager, compartment_guid, client_id, data);
}

bool TipCompartmentUtil::Set(ITfDocumentMgr *document_manager,
                             const GUID &compartment_guid,
                             TfClientId client_id,
                             const ATL::CComVariant &data) {
  CComQIPtr<ITfCompartmentMgr> compartment_manager(document_manager);
  return Set(compartment_manager, compartment_guid, client_id, data);
}

bool TipCompartmentUtil::Set(ITfContext *context,
                             const GUID &compartment_guid,
                             TfClientId client_id,
                             const ATL::CComVariant &data) {
  CComQIPtr<ITfCompartmentMgr> compartment_manager(context);
  return Set(compartment_manager, compartment_guid, client_id, data);
}

bool TipCompartmentUtil::Get(ITfCompartmentMgr *compartment_manager,
                             const GUID &compartment_guid,
                             CComVariant *data) {
  if (data == nullptr) {
    return false;
  }
  data->Clear();

  if (!compartment_manager) {
    return false;
  }

  CComPtr<ITfCompartment> compartment;
  if (FAILED(compartment_manager->GetCompartment(compartment_guid,
                                                 &compartment))) {
    return false;
  }
  if (!compartment) {
    return false;
  }

  return SUCCEEDED(compartment->GetValue(data));
}

bool TipCompartmentUtil::Get(ITfThreadMgr *thread_manager,
                             const GUID &compartment_guid,
                             ATL::CComVariant *data) {
  CComQIPtr<ITfCompartmentMgr> compartment_manager(thread_manager);
  return Get(compartment_manager, compartment_guid, data);
}

bool TipCompartmentUtil::Get(ITfDocumentMgr *document_manager,
                             const GUID &compartment_guid,
                             ATL::CComVariant *data) {
  CComQIPtr<ITfCompartmentMgr> compartment_manager(document_manager);
  return Get(compartment_manager, compartment_guid, data);
}

bool TipCompartmentUtil::Get(ITfContext *context,
                             const GUID &compartment_guid,
                             ATL::CComVariant *data) {
  CComQIPtr<ITfCompartmentMgr> compartment_manager(context);
  return Get(compartment_manager, compartment_guid, data);
}

bool TipCompartmentUtil::GetAndEnsureDataExists(
    ITfCompartmentMgr *compartment_manager,
    const GUID &compartment_guid,
    TfClientId client_id,
    const CComVariant &default_data,
    CComVariant *data) {
  if (data == nullptr) {
    return false;
  }
  data->Clear();

  if (compartment_manager == nullptr) {
    return false;
  }

  CComPtr<ITfCompartment> compartment;
  if (FAILED(compartment_manager->GetCompartment(compartment_guid,
                                                 &compartment))) {
    return false;
  }
  if (!compartment) {
    return false;
  }

  const HRESULT result = compartment->GetValue(data);
  if (FAILED(result)) {
    return false;
  }
  if (result == S_FALSE) {
    if (FAILED(compartment->SetValue(client_id, &default_data))) {
      return false;
    }
    *data = default_data;
  }
  return true;
}

bool TipCompartmentUtil::GetAndEnsureDataExists(
      ITfThreadMgr *thread_manager,
      const GUID &compartment_guid,
      TfClientId client_id,
      const ATL::CComVariant &default_data,
      ATL::CComVariant *data) {
  CComQIPtr<ITfCompartmentMgr> compartment_manager(thread_manager);
  return GetAndEnsureDataExists(
      compartment_manager, compartment_guid, client_id, default_data, data);
}

bool TipCompartmentUtil::GetAndEnsureDataExists(
      ITfDocumentMgr *document_manager,
      const GUID &compartment_guid,
      TfClientId client_id,
      const ATL::CComVariant &default_data,
      ATL::CComVariant *data) {
  CComQIPtr<ITfCompartmentMgr> compartment_manager(document_manager);
  return GetAndEnsureDataExists(
      compartment_manager, compartment_guid, client_id, default_data, data);
}

bool TipCompartmentUtil::GetAndEnsureDataExists(
      ITfContext *context,
      const GUID &compartment_guid,
      TfClientId client_id,
      const ATL::CComVariant &default_data,
      ATL::CComVariant *data) {
  CComQIPtr<ITfCompartmentMgr> compartment_manager(context);
  return GetAndEnsureDataExists(
      compartment_manager, compartment_guid, client_id, default_data, data);
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc
