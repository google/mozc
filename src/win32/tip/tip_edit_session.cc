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

#include "win32/tip/tip_edit_session.h"

#include <Windows.h>
#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
// Workaround against KB813540
#include <atlbase_mozc.h>
#include <atlcom.h>
#include <msctf.h>

#include <string>

#include "base/util.h"
#include "session/commands.pb.h"
#include "win32/base/conversion_mode_util.h"
#include "win32/base/input_state.h"
#include "win32/base/string_util.h"
#include "win32/tip/tip_composition_util.h"
#include "win32/tip/tip_private_context.h"
#include "win32/tip/tip_range_util.h"
#include "win32/tip/tip_ref_count.h"
#include "win32/tip/tip_status.h"
#include "win32/tip/tip_text_service.h"
#include "win32/tip/tip_ui_handler.h"

namespace mozc {
namespace win32 {
namespace tsf {

using ATL::CComBSTR;
using ATL::CComPtr;
using ATL::CComQIPtr;
using ATL::CComVariant;
using ::mozc::commands::Output;
using ::mozc::commands::Preedit;
using ::mozc::commands::Result;
typedef ::mozc::commands::Preedit::Segment Segment;
typedef ::mozc::commands::Preedit::Segment::Annotation Annotation;

namespace {

// An ad-hoc function to update the cursor position and TSF properties
// associated with a result string specified by |range| which corresponds to
// |output.result()|.
// Currently this function is separated from EditSessionImpl::DoEditSessionImpl
// but basically they should be integrated.
// TODO(yukawa): Refactor this function and EditSessionImpl::DoEditSessionImpl.
//     We'd better to have some unit tests if possible because this is the core
//     logic of command handling.
HRESULT UpdateResult(ITfContext *context,
                     ITfRange *range,
                     const Output &output,
                     TfEditCookie edit_cookie) {
  HRESULT result = S_OK;
  if (!output.has_result() || !output.result().has_key()) {
    return S_OK;
  }
  const string &key = output.result().key();
  const wstring &reading_string = StringUtil::KeyToReading(key);

  // Get out the reading property
  CComPtr<ITfProperty> reading_property;
  result = context->GetProperty(GUID_PROP_READING, &reading_property);
  if (FAILED(result)) {
    return result;
  }

  CComVariant reading(CComBSTR(reading_string.c_str()));
  result = reading_property->SetValue(edit_cookie, range, &reading);
  if (FAILED(result)) {
    return result;
  }

  // Update the cursor position at the end of the |range|.
  CComPtr<ITfRange> selection_range;
  result = range->Clone(&selection_range);
  if (FAILED(result)) {
    return result;
  }
  result = selection_range->Collapse(edit_cookie, TF_ANCHOR_END);
  if (FAILED(result)) {
    return result;
  }
  return TipRangeUtil::SetSelection(
      context, edit_cookie, selection_range, TF_AE_END);
}

// An ad-hoc function to update the TSF properties associated with a
// composition which consists of |output.preedit()|. Currently this function is
// separated from EditSessionImpl::DoEditSessionImpl but basically they should
// be integrated.
// TODO(yukawa): Refactor this function and EditSessionImpl::DoEditSessionImpl.
//     We'd better to have some unit tests if possible because this is the core
//     logic of command handling.
HRESULT UpdateComposition(
    TipTextService *text_service,
    ITfContext *context,
    const Output &output,
    TfEditCookie edit_cookie) {
  HRESULT result = S_OK;

  if (!output.has_preedit()) {
    return S_OK;
  }
  const Preedit &preedit = output.preedit();

  CComPtr<ITfCompositionView> composition =
      TipCompositionUtil::GetComposition(context, edit_cookie);
  if (!composition) {
    return S_OK;
  }

  // Retrieve the current composition range.
  CComPtr<ITfRange> composition_range;
  result = composition->GetRange(&composition_range);
  if (FAILED(result)) {
    return result;
  }

  // Get out the display attribute property
  CComPtr<ITfProperty> display_attribute;
  result = context->GetProperty(GUID_PROP_ATTRIBUTE, &display_attribute);
  if (FAILED(result)) {
    return result;
  }

  // Get out the reading property
  CComPtr<ITfProperty> reading_property;
  result = context->GetProperty(GUID_PROP_READING, &reading_property);
  if (FAILED(result)) {
    return result;
  }

  // Set each segment's display attribute
  int start = 0;
  int end = 0;
  for (int i = 0; i < preedit.segment_size(); ++i) {
    const Preedit::Segment &segment = preedit.segment(i);
    end = start + Util::WideCharsLen(segment.value());
    const Preedit::Segment::Annotation &annotation =
        segment.annotation();
    TfGuidAtom attribute = TF_INVALID_GUIDATOM;
    if (annotation == Preedit::Segment::UNDERLINE) {
      attribute = text_service->input_attribute();
    } else if (annotation == Preedit::Segment::HIGHLIGHT) {
      attribute = text_service->converted_attribute();
    } else {  // mozc::commands::Preedit::Segment::NONE or unknown value
      continue;
    }

    CComPtr<ITfRange> segment_range;
    result = composition_range->Clone(&segment_range);
    if (FAILED(result)) {
      return result;
    }
    result = segment_range->Collapse(edit_cookie, TF_ANCHOR_START);
    if (FAILED(result)) {
      return result;
    }
    LONG shift = 0;
    result = segment_range->ShiftEnd(edit_cookie, end, &shift, nullptr);
    if (FAILED(result)) {
      return result;
    }
    result = segment_range->ShiftStart(edit_cookie, start, &shift, nullptr);
    if (FAILED(result)) {
      return result;
    }
    CComVariant var;
    // set the value over the range
    var.vt = VT_I4;
    var.lVal = attribute;
    result = display_attribute->SetValue(edit_cookie, segment_range, &var);
    if (segment.has_key()) {
      const wstring &reading_string = StringUtil::KeyToReading(segment.key());
      CComVariant reading(CComBSTR(reading_string.c_str()));
      result = reading_property->SetValue(
          edit_cookie, segment_range, &reading);
    }
    start = end;
  }

  // Update cursor.
  {
    string preedit_text;
    for (int i = 0; i < preedit.segment_size(); ++i) {
      preedit_text += preedit.segment(i).value();
    }

    CComPtr<ITfRange> cursor_range;
    result = composition_range->Clone(&cursor_range);
    if (FAILED(result)) {
      return result;
    }
    // |output.preedit().cursor()| is in the unit of UTF-32. We need to convert
    // it to UTF-16 for TSF.
    const uint32 cursor_pos_utf16 = Util::WideCharsLen(Util::SubString(
        preedit_text, 0, output.preedit().cursor()));

    result = cursor_range->Collapse(edit_cookie, TF_ANCHOR_START);
    if (FAILED(result)) {
      return result;
    }
    LONG shift = 0;
    result = cursor_range->ShiftEnd(
        edit_cookie, cursor_pos_utf16, &shift, nullptr);
    if (FAILED(result)) {
      return result;
    }
    result = cursor_range->ShiftStart(
        edit_cookie, cursor_pos_utf16, &shift, nullptr);
    if (FAILED(result)) {
      return result;
    }
    result = TipRangeUtil::SetSelection(
        context, edit_cookie, cursor_range, TF_AE_END);
  }
  return result;
}

// This class is an implementation class for the ITfEditSession classes, which
// is an observer for exclusively updating the text store of a TSF thread
// manager.
class EditSessionImpl : public ITfEditSession {
 public:
  EditSessionImpl(CComPtr<TipTextService> text_service,
                  CComPtr<ITfContext> context,
                  const Output &output)
      : text_service_(text_service),
        context_(context) {
    output_.CopyFrom(output);
  }
  ~EditSessionImpl() {}

  // The IUnknown interface methods.
  STDMETHODIMP QueryInterface(REFIID interface_id, void **object) {
    if (!object) {
      return E_INVALIDARG;
    }

    // Find a matching interface from the ones implemented by this object.
    // This object implements IUnknown and ITfEditSession.
    if (::IsEqualIID(interface_id, IID_IUnknown)) {
      *object = static_cast<IUnknown *>(this);
    } else if (IsEqualIID(interface_id, IID_ITfEditSession)) {
      *object = static_cast<ITfEditSession *>(this);
    } else {
      *object = nullptr;
      return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
  }

  STDMETHODIMP_(ULONG) AddRef() {
    return ref_count_.AddRefImpl();
  }

  STDMETHODIMP_(ULONG) Release() {
    const ULONG count = ref_count_.ReleaseImpl();
    if (count == 0) {
      delete this;
    }
    return count;
  }

  // The ITfEditSession interface method.
  // This function is called back by the TSF thread manager when an edit
  // request is granted.
  virtual STDMETHODIMP DoEditSession(TfEditCookie edit_cookie) {
    const HRESULT result = DoEditSessionImpl(edit_cookie);
    UpdateComposition(text_service_, context_, output_, edit_cookie);
    TipUiHandler::Update(text_service_, context_, edit_cookie);
    return result;
  }

 private:
  HRESULT DoEditSessionImpl(TfEditCookie edit_cookie) {
    TipPrivateContext *private_context =
        text_service_->GetPrivateContext(context_);
    if (private_context != nullptr) {
      private_context->mutable_last_output()->CopyFrom(output_);
      if (output_.has_status()) {
        private_context->mutable_input_state()->open =
            output_.status().activated();
        // Convert Mozc's composition mode to TSF's conversion mode.
        uint32 tsf_conversion_mode = 0;
        if (ConversionModeUtil::ToNativeMode(
                output_.status().mode(),
                private_context->input_behavior().prefer_kana_input,
                &tsf_conversion_mode)) {
          private_context->mutable_input_state()->conversion_status =
              tsf_conversion_mode;
        }
      }
      TipStatus::UpdateFromMozcStatus(
          text_service_->GetThreadManager(),
          text_service_->GetClientID(),
          private_context->input_behavior().prefer_kana_input,
          output_.status());
    }

    CComPtr<ITfCompositionView> composition_view =
        TipCompositionUtil::GetComposition(context_, edit_cookie);

    CComQIPtr<ITfComposition> composition = composition_view;

    // Clear the existing properties.
    if (composition) {
      if (FAILED(TipCompositionUtil::ClearProperties(
              context_, composition, edit_cookie))) {
        return E_FAIL;
      }
    }

    if (composition && output_.has_result()) {
      CComPtr<ITfRange> range;
      if (FAILED(composition_view->GetRange(&range))) {
        return E_FAIL;
      }
      wstring result;
      Util::UTF8ToWide(output_.result().value(), &result);
      if (FAILED(range->SetText(edit_cookie, 0, result.c_str(),
                                result.size()))) {
        return E_FAIL;
      }
      if (!output_.has_preedit()) {
        if (FAILED(UpdateResult(context_, range, output_, edit_cookie))) {
          return E_FAIL;
        }
        if (FAILED(composition->EndComposition(edit_cookie))) {
          return E_FAIL;
        }
        return S_OK;
      }
      range->Collapse(edit_cookie, TF_ANCHOR_END);
      composition->ShiftStart(edit_cookie, range);
      composition->ShiftEnd(edit_cookie, range);
      const wstring &preedit =
          StringUtil::ComposePreeditText(output_.preedit());
      return range->SetText(edit_cookie, 0, preedit.c_str(), preedit.length());
    } else if (composition_view && output_.has_preedit()) {
      const wstring &preedit =
          StringUtil::ComposePreeditText(output_.preedit());
      CComPtr<ITfRange> range;
      if (FAILED(composition_view->GetRange(&range))) {
        return E_FAIL;
      }
      return range->SetText(edit_cookie, 0, preedit.c_str(), preedit.size());
    } else if (composition && !output_.has_preedit()) {
      CComPtr<ITfRange> range;
      if (FAILED(composition_view->GetRange(&range))) {
        return E_FAIL;
      }
      if (FAILED(range->SetText(edit_cookie, 0, nullptr, 0))) {
        return E_FAIL;
      }
      if (FAILED(composition->EndComposition(edit_cookie))) {
        return E_FAIL;
      }
      return S_OK;
    }

    if (output_.has_result()) {
      CComPtr<ITfInsertAtSelection> insert_selection;
      if (FAILED(context_.QueryInterface(&insert_selection))) {
        return E_FAIL;
      }
      wstring result;
      Util::UTF8ToWide(output_.result().value(), &result);
      CComPtr<ITfRange> insertion_range;
      if (FAILED(insert_selection->InsertTextAtSelection(
          edit_cookie, 0, result.c_str(), result.size(), &insertion_range))) {
        return E_FAIL;
      }
      if (FAILED(UpdateResult(
              context_, insertion_range, output_, edit_cookie))) {
        return E_FAIL;
      }
    }

    if (output_.has_preedit()) {
      CComPtr<ITfInsertAtSelection> insert_selection;
      if (FAILED(context_.QueryInterface(&insert_selection))) {
        return E_FAIL;
      }
      CComPtr<ITfRange> range;
      if (FAILED(insert_selection->InsertTextAtSelection(
              edit_cookie, TF_IAS_QUERYONLY, nullptr, 0, &range))) {
        return E_FAIL;
      }

      // Create a new composition object and attach it to the text service.
      // This composition object contains a composition text and used for
      // updating it.
      CComPtr<ITfContextComposition> composition_context;
      if (FAILED(context_.QueryInterface(&composition_context))) {
        return E_FAIL;
      }

      const wstring &preedit =
          StringUtil::ComposePreeditText(output_.preedit());
      CComPtr<ITfComposition> composition;
      if (FAILED(composition_context->StartComposition(
              edit_cookie, range,
              text_service_->CreateCompositionSink(context_), &composition))) {
        return E_FAIL;
      }
      if (FAILED(range->SetText(
              edit_cookie, 0, preedit.c_str(), preedit.length()))) {
        return E_FAIL;
      }
    }

    return S_OK;
  }

  TipRefCount ref_count_;
  CComPtr<TipTextService> text_service_;
  CComPtr<ITfContext> context_;
  Output output_;

  DISALLOW_COPY_AND_ASSIGN(EditSessionImpl);
};

}  // namespace

ITfEditSession *TipEditSession::New(TipTextService *text_service,
                                    ITfContext *context,
                                    const Output &output) {
  return new EditSessionImpl(text_service, context, output);
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc
