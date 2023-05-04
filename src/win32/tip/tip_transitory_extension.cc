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

#include "win32/tip/tip_transitory_extension.h"

#include <msctf.h>
#include <wil/com.h>
#include <wil/resource.h>
#include <windows.h>

#include "base/win32/com.h"

namespace mozc {
namespace win32 {
namespace tsf {

wil::com_ptr_nothrow<ITfDocumentMgr>
TipTransitoryExtension::ToParentDocumentIfExists(
    ITfDocumentMgr *document_manager) {
  if (document_manager == nullptr) {
    return nullptr;
  }

  wil::com_ptr_nothrow<ITfContext> context;
  if (FAILED(document_manager->GetTop(&context))) {
    return document_manager;
  }

  if (!context) {
    return document_manager;
  }

  TF_STATUS status = {};
  if (FAILED(context->GetStatus(&status))) {
    return document_manager;
  }

  if ((status.dwStaticFlags & TF_SS_TRANSITORY) != TF_SS_TRANSITORY) {
    return document_manager;
  }

  auto compartment_mgr = ComQuery<ITfCompartmentMgr>(document_manager);
  if (!compartment_mgr) {
    return document_manager;
  }

  wil::com_ptr_nothrow<ITfCompartment> compartment;
  if (FAILED(compartment_mgr->GetCompartment(
          GUID_COMPARTMENT_TRANSITORYEXTENSION_PARENT, &compartment))) {
    return document_manager;
  }

  wil::unique_variant var;
  if (FAILED(compartment->GetValue(var.reset_and_addressof()))) {
    return document_manager;
  }

  if (var.vt != VT_UNKNOWN || var.punkVal == nullptr) {
    return document_manager;
  }

  auto parent_document_mgr = ComQuery<ITfDocumentMgr>(var.punkVal);
  if (!parent_document_mgr) {
    return document_manager;
  }

  return parent_document_mgr;
}

wil::com_ptr_nothrow<ITfContext>
TipTransitoryExtension::ToParentContextIfExists(ITfContext *context) {
  if (context == nullptr) {
    return nullptr;
  }

  wil::com_ptr_nothrow<ITfDocumentMgr> document_mgr;
  if (FAILED(context->GetDocumentMgr(&document_mgr))) {
    return context;
  }

  wil::com_ptr_nothrow<ITfContext> parent_context;
  if (FAILED(ToParentDocumentIfExists(document_mgr.get())
                 ->GetTop(&parent_context))) {
    return context;
  }
  if (!parent_context) {
    return context;
  }
  return parent_context;
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc
