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

#include "absl/base/nullability.h"
#include "base/win32/com.h"
#include "base/win32/hresultor.h"
#include "win32/tip/tip_compartment_util.h"

namespace mozc {
namespace win32 {
namespace tsf {
namespace {

// An undocumented GUID found with ITfCompartmentMgr::EnumGuid().
// Looks like if its value is VT_I4 and 0x01 bit is set, then the correspinding
// ITfDocumentMgr is implemented by CUAS and its ITfContext does not return an
// actual surrounding text.
// {A94C5FD2-C471-4031-9546-709C17300CB9}
constinit static const GUID kTsfEmulatedDocumentMgrGuid = {
    0xa94c5fd2,
    0xc471,
    0x4031,
    {0x95, 0x46, 0x70, 0x9c, 0x17, 0x30, 0x0c, 0xb9}};

bool IsTsfEmulatedDocumentMgr(ITfDocumentMgr *absl_nonnull document_mgr) {
  HResultOr<wil::unique_variant> var =
      TipCompartmentUtil::Get(document_mgr, kTsfEmulatedDocumentMgrGuid);
  if (!var.has_value()) {
    return false;
  }
  // If the variant is VT_I4 and 0x01 bit is set, then the app is likely to be
  // a legacy IMM32-based app and the focused field is not a EditText/RichEdit
  // common control.
  return var->vt == VT_I4 ? (var->intVal & 0x01) == 0x01 : false;
}

wil::com_ptr_nothrow<ITfDocumentMgr> GetTransitoryExtensionDocumentMgrOrSelf(
    ITfDocumentMgr *absl_nonnull document_manager) {
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

  wil::com_ptr_nothrow<ITfDocumentMgr> result =
      ComQuery<ITfDocumentMgr>(var.punkVal);
  return result == nullptr ? document_manager : result;
}

}  // namespace

wil::com_ptr_nothrow<ITfContext>
TipTransitoryExtension::AsFullContext(ITfContext *context) {
  if (context == nullptr) {
    return nullptr;
  }

  TF_STATUS status = {};
  if (FAILED(context->GetStatus(&status))) {
    return nullptr;
  }

  if ((status.dwStaticFlags & TF_SS_TRANSITORY) == 0) {
    // Non trantitory context is expected to be a full context.
    // Fully TSF-aware apps such as Microsoft Word, WPF-based apps, and Firefox
    // fall into this path.
    return context;
  }

  wil::com_ptr_nothrow<ITfDocumentMgr> document_mgr;
  if (FAILED(context->GetDocumentMgr(&document_mgr))) {
    return nullptr;
  }

  if (document_mgr == nullptr) {
    return nullptr;
  }

  // Here after we want to distinguish the following three cases:
  // 1. Fully TSF-aware cases through Transitory Extension.
  // 2. Legacy IMM32-based apps where CUAS does not fully provide TSF
  //    surrounding text APIs.
  // 3. TSF-based apps that explicitly specify TF_SS_TRANSITORY.

  if (wil::com_ptr_nothrow<ITfDocumentMgr> target_document_mgr =
          GetTransitoryExtensionDocumentMgrOrSelf(document_mgr.get());
      target_document_mgr != document_mgr) {
    // When Transitory Extension is available, there should exist another
    // ITfContext object that is expected to support full text store operations.
    wil::com_ptr_nothrow<ITfContext> target_context;
    if (FAILED(target_document_mgr->GetTop(&target_context))) {
      return nullptr;
    }
    if (target_context == nullptr) {
      return nullptr;
    }

    TF_STATUS target_status = {};
    if (FAILED(target_context->GetStatus(&target_status))) {
      return nullptr;
    }

    if ((target_status.dwStaticFlags & TF_SS_TRANSITORY) == 0) {
      // Case 1: Fully TSF-aware cases through Transitory Extension.
      // EditControl and RichEdit fall into this path on Vista.
      // https://learn.microsoft.com/en-us/archive/blogs/tsfaware/transitory-extensions-or-how-to-get-full-text-store-support-in-tsf-unaware-controls
      // https://web.archive.org/web/20140518145404/http://blogs.msdn.com/b/tsfaware/archive/2007/05/21/transitory-extensions.aspx
      return target_context;
    }

    return nullptr;
  }

  if (IsTsfEmulatedDocumentMgr(document_mgr.get())) {
    // Case 2: Legacy IMM32-based apps that are running through CUAS.
    // IMM32-based legacy apps such as Sakura Editor fall into this path.
    return nullptr;
  }

  // Case 3: TSF-based apps that explicitly specify TF_SS_TRANSITORY.
  // Chromium-based apps fall into this path.
  // To support surrunding text on Chromium-based apps, here we assume that
  // surrounding text TSF APIs are fully available in this case.
  // https://github.com/google/mozc/issues/1289
  // https://issues.chromium.org/issues/40724714#comment38
  // https://issues.chromium.org/issues/417529154
  return context;
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc
