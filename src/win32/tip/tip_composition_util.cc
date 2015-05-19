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

#include "win32/tip/tip_composition_util.h"

#include "win32/base/tsf_profile.h"
#include "win32/tip/tip_command_handler.h"
#include "win32/tip/tip_range_util.h"
#include "win32/tip/tip_text_service.h"

namespace mozc {
namespace win32 {
namespace tsf {

using ATL::CComPtr;

CComPtr<ITfCompositionView> TipCompositionUtil::GetComposition(
    CComPtr<ITfContext> context, TfEditCookie edit_cookie) {
  CComPtr<ITfContextComposition> context_composition;
  if (FAILED(context.QueryInterface(&context_composition))) {
    return nullptr;
  }

  CComPtr<IEnumITfCompositionView> enum_composition;
  if (FAILED(context_composition->FindComposition(edit_cookie, nullptr,
                                                  &enum_composition))) {
    return nullptr;
  }

  while (true) {
    CComPtr<ITfCompositionView> composition_view;
    ULONG num_fetched = 0;
    if (enum_composition->Next(1, &composition_view, &num_fetched) != S_OK) {
      return nullptr;
    }
    if (num_fetched != 1) {
      return nullptr;
    }
    GUID clsid = GUID_NULL;
    if (FAILED(composition_view->GetOwnerClsid(&clsid))) {
      continue;
    }
    if (!::IsEqualCLSID(TsfProfile::GetTextServiceGuid(), clsid)) {
      continue;
    }
    // Although TSF supports multiple composition, Mozc uses only one
    // composition at the same time. So the first one must be the only one.
    return composition_view;
  }
}

HRESULT TipCompositionUtil::ClearProperties(ITfContext *context,
                                            ITfComposition *composition,
                                            TfEditCookie write_cookie) {
  HRESULT result = S_OK;

  // Retrieve the current composition range.
  CComPtr<ITfRange> composition_range;
  result = composition->GetRange(&composition_range);
  if (FAILED(result)) {
    return result;
  }

  {
    // Get out the display attribute property
    CComPtr<ITfProperty> display_attribute;
    result = context->GetProperty(GUID_PROP_ATTRIBUTE, &display_attribute);
    if (FAILED(result)) {
      return result;
    }
    // Clear existing attributes.
    result = display_attribute->Clear(write_cookie, composition_range);
    if (FAILED(result)) {
      return result;
    }
  }

  {
    // Get out the reading property
    CComPtr<ITfProperty> reading_property;
    result = context->GetProperty(GUID_PROP_READING, &reading_property);
    if (FAILED(result)) {
      return result;
    }
    // Clear existing attributes.
    result = reading_property->Clear(write_cookie, composition_range);
    if (FAILED(result)) {
      return result;
    }
  }
  return S_OK;
}

HRESULT TipCompositionUtil::OnEndEdit(TipTextService *text_service,
                                      ITfContext *context,
                                      ITfComposition *composition,
                                      TfEditCookie edit_cookie,
                                      ITfEditRecord *edit_record) {
  HRESULT result = S_OK;
  if (!composition) {
    // Nothing to do.
    return S_OK;
  }

  CComPtr<ITfRange> composition_range;
  result = composition->GetRange(&composition_range);
  if (FAILED(result)) {
    return result;
  }

  BOOL selection_changed = FALSE;
  result = edit_record->GetSelectionStatus(&selection_changed);
  if (FAILED(result)) {
    return result;
  }
  if (selection_changed) {
    // When the selection is changed, make sure the new selection range is
    // covered by the compositon range. Otherwise, terminate the composition.
    CComPtr<ITfRange> selected_range;
    TfActiveSelEnd active_sel_end = TF_AE_NONE;
    if (!TipRangeUtil::GetDefaultSelection(
            context, edit_cookie, &selected_range, &active_sel_end)) {
      return E_FAIL;
    }
    if (!TipRangeUtil::IsRangeCovered(
            edit_cookie, selected_range, composition_range)) {
      if (!TipCommandHandler::OnCompositionTerminated(
              text_service, context, composition, edit_cookie)) {
        return E_FAIL;
      }
      // Cancels further operations.
      return S_OK;
    }
  }

  BOOL is_empty = FALSE;
  result = composition_range->IsEmpty(edit_cookie, &is_empty);
  if (FAILED(result)) {
    return result;
  }
  if (is_empty) {
    // When the composition range is empty, we assume the composition is
    // cancelled by the application or something. Actually CUAS does this
    // when it receives NI_COMPOSITIONSTR/CPS_CANCEL. You can see this as
    // Excel's auto-completion. If this happens, send REVERT command to
    // the server to keep the state consistent. See b/1793331 for details.
    if (!TipCommandHandler::NotifyCompositionReverted(
            text_service, context, composition, edit_cookie)) {
      return E_FAIL;
    }
  }

  return S_OK;
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc
