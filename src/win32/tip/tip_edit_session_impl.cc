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

#include "win32/tip/tip_edit_session_impl.h"

#include <inputscope.h>
#include <msctf.h>
#include <wil/com.h>
#include <wil/resource.h>
#include <windows.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "base/util.h"
#include "base/win32/com.h"
#include "base/win32/wide_char.h"
#include "client/client_interface.h"
#include "protocol/commands.pb.h"
#include "win32/base/conversion_mode_util.h"
#include "win32/base/input_state.h"
#include "win32/base/string_util.h"
#include "win32/tip/tip_composition_util.h"
#include "win32/tip/tip_edit_session.h"
#include "win32/tip/tip_input_mode_manager.h"
#include "win32/tip/tip_private_context.h"
#include "win32/tip/tip_range_util.h"
#include "win32/tip/tip_status.h"
#include "win32/tip/tip_text_service.h"
#include "win32/tip/tip_thread_context.h"
#include "win32/tip/tip_ui_handler.h"

namespace mozc {
namespace win32 {
namespace tsf {
namespace {

using ::mozc::commands::Output;
using ::mozc::commands::Preedit;
using ::mozc::commands::SessionCommand;
using ::mozc::commands::Status;
using CompositionMode = ::mozc::commands::CompositionMode;
using Segment = ::mozc::commands::Preedit::Segment;
using Annotation = ::mozc::commands::Preedit::Segment::Annotation;

HRESULT SetReadingProperties(ITfContext *context, ITfRange *range,
                             const std::string &reading_string_utf8,
                             TfEditCookie write_cookie) {
  HRESULT result = S_OK;

  // Get out the reading property
  wil::com_ptr_nothrow<ITfProperty> reading_property;
  result = context->GetProperty(GUID_PROP_READING, &reading_property);
  if (FAILED(result)) {
    return result;
  }

  const std::wstring &canonical_reading_string =
      StringUtil::KeyToReading(reading_string_utf8);
  wil::unique_variant reading =
      wil::make_variant_bstr_nothrow(canonical_reading_string.c_str());
  return reading_property->SetValue(write_cookie, range, reading.addressof());
}

HRESULT ClearReadingProperties(ITfContext *context, ITfRange *range,
                               TfEditCookie write_cookie) {
  HRESULT result = S_OK;

  // Get out the reading property
  wil::com_ptr_nothrow<ITfProperty> reading_property;
  result = context->GetProperty(GUID_PROP_READING, &reading_property);
  if (FAILED(result)) {
    return result;
  }
  // Clear existing attributes.
  result = reading_property->Clear(write_cookie, range);
  if (FAILED(result)) {
    return result;
  }
  return result;
}

wil::com_ptr_nothrow<ITfComposition> CreateComposition(
    TipTextService *text_service, ITfContext *context,
    TfEditCookie write_cookie) {
  auto composition_context = ComQuery<ITfContextComposition>(context);
  if (!composition_context) {
    return nullptr;
  }
  auto insert_selection = ComQuery<ITfInsertAtSelection>(context);
  if (!insert_selection) {
    return nullptr;
  }
  wil::com_ptr_nothrow<ITfRange> insertion_pos;
  if (FAILED(insert_selection->InsertTextAtSelection(
          write_cookie, TF_IAS_QUERYONLY, nullptr, 0, &insertion_pos))) {
    return nullptr;
  }
  wil::com_ptr_nothrow<ITfComposition> composition;
  if (FAILED(composition_context->StartComposition(
          write_cookie, insertion_pos.get(),
          text_service->CreateCompositionSink(context).get(), &composition))) {
    return nullptr;
  }
  return composition;
}

// Note: Committing a text is a tricky part in TSF/CUAS. Basically it should be
// done as following steps.
//   1. Create a composition (if not exists).
//   2. Replace the text stored in the composition range with the text to be
//      committed. Note that CUAS updates GCS_RESULTCLAUSE and
//      GCS_RESULTREADCLAUSE by using the segment structure of GUID_PROP_READING
//      property. For example, CUAS generates two segments for the following
//      reading text structure.
//        "今日は(きょうは)/晴天(せいてん)"
//   3. Call ITfComposition::ShiftStart to shrink the composition range. Note
//      that the text that is pushed out from the composition range is
//      interpreted as the "committed text".
//   4. Update the caret position explicitly. Note that some applications
//      such as WPF's TextBox do not update the caret position automatically
//      when a composition is committed.
// See also b/8406545 and b/9747361.
wil::com_ptr_nothrow<ITfComposition> CommitText(
    TipTextService *text_service, ITfContext *context,
    TfEditCookie write_cookie, wil::com_ptr_nothrow<ITfComposition> composition,
    const Output &output) {
  if (!composition) {
    composition = CreateComposition(text_service, context, write_cookie);
    if (!composition) {
      return nullptr;
    }
  }

  HRESULT result = S_OK;

  wil::com_ptr_nothrow<ITfRange> composition_range;
  result = composition->GetRange(&composition_range);
  if (FAILED(result)) {
    return nullptr;
  }

  std::wstring composition_text;
  TipRangeUtil::GetText(composition_range.get(), write_cookie,
                        &composition_text);

  // Make sure that |composition_text| begins with |result_text| so that
  // CUAS can generate an appropriate GCS_RESULTREADCLAUSE information.
  // See b/8406545
  const std::wstring result_text = Utf8ToWide(output.result().value());
  if (composition_text.find(result_text) != 0) {
    result = composition_range->SetText(write_cookie, 0, result_text.c_str(),
                                        result_text.size());
    if (FAILED(result)) {
      return nullptr;
    }
    result = SetReadingProperties(context, composition_range.get(),
                                  output.result().key(), write_cookie);
    if (FAILED(result)) {
      return nullptr;
    }
  }

  wil::com_ptr_nothrow<ITfRange> new_composition_start;
  result = composition_range->Clone(&new_composition_start);
  if (FAILED(result)) {
    return nullptr;
  }
  LONG moved = 0;
  result = new_composition_start->ShiftStart(write_cookie, result_text.size(),
                                             &moved, nullptr);
  if (FAILED(result)) {
    return nullptr;
  }
  result = new_composition_start->Collapse(write_cookie, TF_ANCHOR_START);
  if (FAILED(result)) {
    return nullptr;
  }
  result = composition->ShiftStart(write_cookie, new_composition_start.get());
  if (FAILED(result)) {
    return nullptr;
  }
  // We need to update the caret position manually for WPF's TextBox, where
  // caret position is not updated automatically when a composition text is
  // committed by ITfComposition::ShiftStart.
  result = TipRangeUtil::SetSelection(context, write_cookie,
                                      new_composition_start.get(), TF_AE_END);
  if (FAILED(result)) {
    return nullptr;
  }
  return composition;
}

HRESULT UpdateComposition(TipTextService *text_service, ITfContext *context,
                          wil::com_ptr_nothrow<ITfComposition> composition,
                          TfEditCookie write_cookie, const Output &output) {
  HRESULT result = S_OK;

  if (!output.has_preedit()) {
    if (composition) {
      wil::com_ptr_nothrow<ITfRange> composition_range;
      result = composition->GetRange(&composition_range);
      if (FAILED(result)) {
        return result;
      }
      BOOL is_empty = FALSE;
      result = composition_range->IsEmpty(write_cookie, &is_empty);
      if (FAILED(result)) {
        return result;
      }
      if (is_empty != TRUE) {
        std::wstring str;
        TipRangeUtil::GetText(composition_range.get(), write_cookie, &str);
        result = composition_range->SetText(write_cookie, 0, L"", 0);
        if (FAILED(result)) {
          return result;
        }
        result = ClearReadingProperties(context, composition_range.get(),
                                        write_cookie);
        if (FAILED(result)) {
          return result;
        }
      }
      result = composition->EndComposition(write_cookie);
      if (FAILED(result)) {
        return result;
      }
    }
    return S_OK;
  }

  DCHECK(output.has_preedit());

  if (!composition) {
    auto insert_selection = ComQuery<ITfInsertAtSelection>(context);
    if (!insert_selection) {
      return E_FAIL;
    }
    wil::com_ptr_nothrow<ITfRange> insertion_pos;
    result = insert_selection->InsertTextAtSelection(
        write_cookie, TF_IAS_QUERYONLY, nullptr, 0, &insertion_pos);
    if (FAILED(result)) {
      return result;
    }
    composition = CreateComposition(text_service, context, write_cookie);
    if (!composition) {
      return E_FAIL;
    }
  }
  wil::com_ptr_nothrow<ITfRange> composition_range;
  result = composition->GetRange(&composition_range);
  if (FAILED(result)) {
    return result;
  }

  const Preedit &preedit = output.preedit();
  const std::wstring &preedit_text = StringUtil::ComposePreeditText(preedit);
  result = composition_range->SetText(write_cookie, 0, preedit_text.c_str(),
                                      preedit_text.size());
  if (FAILED(result)) {
    return result;
  }

  // Get out the display attribute property
  wil::com_ptr_nothrow<ITfProperty> display_attribute;
  result = context->GetProperty(GUID_PROP_ATTRIBUTE, &display_attribute);
  if (FAILED(result)) {
    return result;
  }

  // Get out the reading property
  wil::com_ptr_nothrow<ITfProperty> reading_property;
  result = context->GetProperty(GUID_PROP_READING, &reading_property);
  if (FAILED(result)) {
    return result;
  }

  // Set each segment's display attribute
  int start = 0;
  int end = 0;
  for (int i = 0; i < preedit.segment_size(); ++i) {
    const Preedit::Segment &segment = preedit.segment(i);
    end = start + WideCharsLen(segment.value());
    const Preedit::Segment::Annotation &annotation = segment.annotation();
    TfGuidAtom attribute = TF_INVALID_GUIDATOM;
    if (annotation == Preedit::Segment::UNDERLINE) {
      attribute = text_service->input_attribute();
    } else if (annotation == Preedit::Segment::HIGHLIGHT) {
      attribute = text_service->converted_attribute();
    } else {  // mozc::commands::Preedit::Segment::NONE or unknown value
      continue;
    }

    wil::com_ptr_nothrow<ITfRange> segment_range;
    result = composition_range->Clone(&segment_range);
    if (FAILED(result)) {
      return result;
    }
    result = segment_range->Collapse(write_cookie, TF_ANCHOR_START);
    if (FAILED(result)) {
      return result;
    }
    LONG shift = 0;
    result = segment_range->ShiftEnd(write_cookie, end, &shift, nullptr);
    if (FAILED(result)) {
      return result;
    }
    result = segment_range->ShiftStart(write_cookie, start, &shift, nullptr);
    if (FAILED(result)) {
      return result;
    }
    wil::unique_variant var;
    // set the value over the range
    var.vt = VT_I4;
    var.lVal = attribute;
    result = display_attribute->SetValue(write_cookie, segment_range.get(),
                                         var.addressof());
    if (segment.has_key()) {
      const std::wstring &reading_string =
          StringUtil::KeyToReading(segment.key());
      wil::unique_variant reading =
          wil::make_variant_bstr_nothrow(reading_string.c_str());
      result = reading_property->SetValue(write_cookie, segment_range.get(),
                                          reading.addressof());
    }
    start = end;
  }

  // Update cursor.
  {
    std::string preedit_text;
    for (int i = 0; i < preedit.segment_size(); ++i) {
      preedit_text += preedit.segment(i).value();
    }

    wil::com_ptr_nothrow<ITfRange> cursor_range;
    result = composition_range->Clone(&cursor_range);
    if (FAILED(result)) {
      return result;
    }
    // |output.preedit().cursor()| is in the unit of UTF-32. We need to convert
    // it to UTF-16 for TSF.
    const uint32_t cursor_pos_utf16 =
        WideCharsLen(Util::Utf8SubString(preedit_text, 0, preedit.cursor()));

    result = cursor_range->Collapse(write_cookie, TF_ANCHOR_START);
    if (FAILED(result)) {
      return result;
    }
    LONG shift = 0;
    result =
        cursor_range->ShiftEnd(write_cookie, cursor_pos_utf16, &shift, nullptr);
    if (FAILED(result)) {
      return result;
    }
    result = cursor_range->ShiftStart(write_cookie, cursor_pos_utf16, &shift,
                                      nullptr);
    if (FAILED(result)) {
      return result;
    }
    result = TipRangeUtil::SetSelection(context, write_cookie,
                                        cursor_range.get(), TF_AE_END);
  }
  return result;
}

HRESULT UpdatePrivateContext(TipTextService *text_service, ITfContext *context,
                             TfEditCookie write_cookie, const Output &output) {
  TipPrivateContext *private_context = text_service->GetPrivateContext(context);
  if (private_context == nullptr) {
    return S_FALSE;
  }
  *private_context->mutable_last_output() = output;
  if (!output.has_status()) {
    return S_FALSE;
  }

  const Status &status = output.status();
  TipInputModeManager *input_mode_manager =
      text_service->GetThreadContext()->GetInputModeManager();
  const TipInputModeManager::NotifyActionSet action_set =
      input_mode_manager->OnReceiveCommand(
          status.activated(), status.comeback_mode(), status.mode());
  if ((action_set & TipInputModeManager::kNotifySystemOpenClose) ==
      TipInputModeManager::kNotifySystemOpenClose) {
    TipStatus::SetIMEOpen(text_service->GetThreadManager(),
                          text_service->GetClientID(),
                          input_mode_manager->GetEffectiveOpenClose());
  }

  if ((action_set & TipInputModeManager::kNotifySystemConversionMode) ==
      TipInputModeManager::kNotifySystemConversionMode) {
    const CompositionMode mozc_mode = static_cast<CompositionMode>(
        input_mode_manager->GetEffectiveConversionMode());
    uint32_t native_mode = 0;
    if (ConversionModeUtil::ToNativeMode(
            mozc_mode, private_context->input_behavior().prefer_kana_input,
            &native_mode)) {
      TipStatus::SetInputModeConversion(text_service->GetThreadManager(),
                                        text_service->GetClientID(),
                                        native_mode);
    }
  }
  return S_OK;
}

HRESULT UpdatePreeditAndComposition(TipTextService *text_service,
                                    ITfContext *context,
                                    TfEditCookie write_cookie,
                                    const Output &output) {
  wil::com_ptr_nothrow<ITfComposition> composition =
      TipCompositionUtil::GetComposition(context, write_cookie);

  // Clear the display attributes first.
  // TODO(https://github.com/google/mozc/discussions/1388): Revisit here.
  if (composition) {
    const HRESULT result = TipCompositionUtil::ClearDisplayAttributes(
        context, composition.get(), write_cookie);
    if (FAILED(result)) {
      return result;
    }
  }

  if (output.has_result()) {
    composition = CommitText(text_service, context, write_cookie,
                             std::move(composition), output);
    if (!composition) {
      return E_FAIL;
    }
  }

  return UpdateComposition(text_service, context, std::move(composition),
                           write_cookie, output);
}

HRESULT DoEditSessionInComposition(TipTextService *text_service,
                                   ITfContext *context,
                                   TfEditCookie write_cookie,
                                   const Output &output) {
  const HRESULT result =
      UpdatePrivateContext(text_service, context, write_cookie, output);
  if (FAILED(result)) {
    return result;
  }
  return UpdatePreeditAndComposition(text_service, context, write_cookie,
                                     output);
}

HRESULT DoEditSessionAfterComposition(TipTextService *text_service,
                                      ITfContext *context,
                                      TfEditCookie write_cookie,
                                      const Output &output) {
  return UpdatePrivateContext(text_service, context, write_cookie, output);
}

HRESULT OnEndEditImpl(TipTextService *text_service, ITfContext *context,
                      TfEditCookie write_cookie, ITfEditRecord *edit_record,
                      bool *update_ui) {
  bool dummy_bool = false;
  if (update_ui == nullptr) {
    update_ui = &dummy_bool;
  }
  *update_ui = false;

  HRESULT result = S_OK;

  {
    wil::com_ptr_nothrow<ITfRange> selection_range;
    TfActiveSelEnd active_sel_end = TF_AE_NONE;
    result = TipRangeUtil::GetDefaultSelection(
        context, write_cookie, &selection_range, &active_sel_end);
    if (FAILED(result)) {
      return result;
    }
    std::vector<InputScope> input_scopes;
    result = TipRangeUtil::GetInputScopes(selection_range.get(), write_cookie,
                                          &input_scopes);
    TipInputModeManager *input_mode_manager =
        text_service->GetThreadContext()->GetInputModeManager();
    const auto actions = input_mode_manager->OnChangeInputScope(input_scopes);
    if (actions == TipInputModeManager::kUpdateUI) {
      *update_ui = true;
    }
    // If the indicator is visible, update UI just in case.
    if (input_mode_manager->IsIndicatorVisible()) {
      *update_ui = true;
    }
  }

  wil::com_ptr_nothrow<ITfComposition> composition =
      TipCompositionUtil::GetComposition(context, write_cookie);
  if (!composition) {
    // Nothing to do.
    return S_OK;
  }

  wil::com_ptr_nothrow<ITfRange> composition_range;
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
    // covered by the composition range. Otherwise, terminate the composition.
    wil::com_ptr_nothrow<ITfRange> selected_range;
    TfActiveSelEnd active_sel_end = TF_AE_NONE;
    result = TipRangeUtil::GetDefaultSelection(
        context, write_cookie, &selected_range, &active_sel_end);
    if (FAILED(result)) {
      return result;
    }
    if (!TipRangeUtil::IsRangeCovered(write_cookie, selected_range.get(),
                                      composition_range.get())) {
      // We enqueue another edit session to sync the composition state between
      // the application and Mozc server because we are already in
      // ITfTextEditSink::OnEndEdit and some operations (e.g.,
      // ITfComposition::EndComposition) result in failure in this edit
      // session.
      result = TipEditSession::SubmitAsync(text_service, context);
      if (FAILED(result)) {
        return result;
      }
      // Cancels further operations.
      return S_OK;
    }
  }

  BOOL is_empty = FALSE;
  result = composition_range->IsEmpty(write_cookie, &is_empty);
  if (FAILED(result)) {
    return result;
  }
  if (is_empty) {
    // When the composition range is empty, we assume the composition is
    // canceled by the application or something. Actually CUAS does this when
    // it receives NI_COMPOSITIONSTR/CPS_CANCEL. You can see this as Excel's
    // auto-completion. If this happens, send REVERT command to the server to
    // keep the state consistent. See b/1793331 for details.

    // We enqueue another edit session to sync the composition state between
    // the application and Mozc server because we are already in
    // ITfTextEditSink::OnEndEdit and some operations (e.g.,
    // ITfComposition::EndComposition) result in failure in this edit session.
    result = TipEditSession::CancelCompositionAsync(text_service, context);
    *update_ui = false;
    if (FAILED(result)) {
      return result;
    }
  }
  return S_OK;
}

}  // namespace

HRESULT TipEditSessionImpl::OnEndEdit(TipTextService *text_service,
                                      ITfContext *context,
                                      TfEditCookie write_cookie,
                                      ITfEditRecord *edit_record) {
  bool update_ui = false;
  const HRESULT result = OnEndEditImpl(text_service, context, write_cookie,
                                       edit_record, &update_ui);
  if (update_ui) {
    TipEditSessionImpl::UpdateUI(text_service, context, write_cookie);
  }
  return result;
}

HRESULT TipEditSessionImpl::OnCompositionTerminated(
    TipTextService *text_service, ITfContext *context,
    ITfComposition *composition, TfEditCookie write_cookie) {
  if (text_service == nullptr) {
    return E_FAIL;
  }
  if (context == nullptr) {
    return E_FAIL;
  }

  // Clear the display attributes first.
  // TODO(https://github.com/google/mozc/discussions/1388): Revisit here.
  if (composition) {
    const HRESULT result = TipCompositionUtil::ClearDisplayAttributes(
        context, composition, write_cookie);
    if (FAILED(result)) {
      return result;
    }
  }

  SessionCommand command;
  command.set_type(SessionCommand::SUBMIT);
  Output output;
  TipPrivateContext *private_context = text_service->GetPrivateContext(context);
  if (private_context == nullptr) {
    return E_FAIL;
  }
  if (!private_context->GetClient()->SendCommand(command, &output)) {
    return E_FAIL;
  }
  const HRESULT result = DoEditSessionAfterComposition(text_service, context,
                                                       write_cookie, output);
  UpdateUI(text_service, context, write_cookie);
  return result;
}

HRESULT TipEditSessionImpl::UpdateContext(TipTextService *text_service,
                                          ITfContext *context,
                                          TfEditCookie write_cookie,
                                          const commands::Output &output) {
  const HRESULT result =
      DoEditSessionInComposition(text_service, context, write_cookie, output);
  UpdateUI(text_service, context, write_cookie);
  return result;
}

void TipEditSessionImpl::UpdateUI(TipTextService *text_service,
                                  ITfContext *context,
                                  TfEditCookie read_cookie) {
  TipUiHandler::Update(text_service, context, read_cookie);
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc
