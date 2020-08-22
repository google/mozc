// Copyright 2010-2020, Google Inc.
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

#include "win32/tip/tip_surrounding_text.h"

#include <Windows.h>
#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
#include <atlbase.h>
#include <atlcom.h>
#include <msctf.h>

#include <memory>

#include "base/util.h"
#include "win32/base/imm_reconvert_string.h"
#include "win32/tip/tip_composition_util.h"
#include "win32/tip/tip_range_util.h"
#include "win32/tip/tip_ref_count.h"
#include "win32/tip/tip_text_service.h"
#include "win32/tip/tip_transitory_extension.h"

namespace mozc {
namespace win32 {
namespace tsf {

using ATL::CComPtr;
using ATL::CComQIPtr;
using ATL::CComVariant;
using std::unique_ptr;

namespace {

const int kMaxSurroundingLength = 20;
const int kMaxCharacterLength = 1024 * 1024;

class SurroudingTextUpdater : public ITfEditSession {
 public:
  SurroudingTextUpdater(ITfContext *context, bool move_anchor)
      : context_(context), move_anchor_(move_anchor) {}

  // Destructor is kept as non-virtual because this class is designed to be
  // destroyed only by "delete this" in Release() method.
  // TODO(yukawa): put "final" keyword to the class declaration when C++11
  //     is allowed.
  ~SurroudingTextUpdater() {}

  // The IUnknown interface methods.
  virtual STDMETHODIMP QueryInterface(REFIID interface_id, void **object) {
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

  virtual STDMETHODIMP_(ULONG) AddRef() { return ref_count_.AddRefImpl(); }

  virtual STDMETHODIMP_(ULONG) Release() {
    const ULONG count = ref_count_.ReleaseImpl();
    if (count == 0) {
      delete this;
    }
    return count;
  }

  const TipSurroundingTextInfo &result() const { return result_; }

 private:
  virtual STDMETHODIMP DoEditSession(TfEditCookie edit_cookie) {
    HRESULT result = S_OK;
    {
      TF_STATUS status = {};
      result = context_->GetStatus(&status);
      if (FAILED(result)) {
        return result;
      }
      result_.is_transitory =
          ((status.dwStaticFlags & TF_SS_TRANSITORY) == TF_SS_TRANSITORY);
    }
    {
      CComPtr<ITfCompositionView> composition_view =
          TipCompositionUtil::GetComposition(context_, edit_cookie);
      result_.in_composition = !!composition_view;
    }

    CComPtr<ITfRange> selected_range;
    {
      result = TipRangeUtil::GetDefaultSelection(context_, edit_cookie,
                                                 &selected_range, nullptr);
      if (FAILED(result)) {
        return result;
      }

      result = TipRangeUtil::GetText(selected_range, edit_cookie,
                                     &result_.selected_text);
      result_.has_selected_text = SUCCEEDED(result);

      // For reconversion, the active selection end should be moved to the
      // front character.
      if (move_anchor_) {
        result = TipRangeUtil::SetSelection(context_, edit_cookie,
                                            selected_range, TF_AE_START);
        if (FAILED(result)) {
          return result;
        }
      }
    }

    const TF_HALTCOND halt_cond = {nullptr, TF_ANCHOR_START, TF_HF_OBJECT};

    {
      CComPtr<ITfRange> preceeding_range;
      LONG preceeding_range_shifted = 0;
      if (SUCCEEDED(selected_range->Clone(&preceeding_range)) &&
          SUCCEEDED(preceeding_range->Collapse(edit_cookie, TF_ANCHOR_START)) &&
          SUCCEEDED(preceeding_range->ShiftStart(
              edit_cookie, -kMaxSurroundingLength, &preceeding_range_shifted,
              &halt_cond))) {
        result = TipRangeUtil::GetText(preceeding_range, edit_cookie,
                                       &result_.preceding_text);
        result_.has_preceding_text = SUCCEEDED(result);
      }
    }

    {
      CComPtr<ITfRange> following_range;
      LONG following_range_shifted = 0;
      if (SUCCEEDED(selected_range->Clone(&following_range)) &&
          SUCCEEDED(following_range->Collapse(edit_cookie, TF_ANCHOR_END)) &&
          SUCCEEDED(following_range->ShiftEnd(
              edit_cookie, kMaxSurroundingLength, &following_range_shifted,
              &halt_cond))) {
        result = TipRangeUtil::GetText(following_range, edit_cookie,
                                       &result_.following_text);
        result_.has_following_text = SUCCEEDED(result);
      }
    }

    return S_OK;
  }

  TipRefCount ref_count_;
  CComPtr<ITfContext> context_;
  TipSurroundingTextInfo result_;
  bool move_anchor_;

  DISALLOW_COPY_AND_ASSIGN(SurroudingTextUpdater);
};

class PrecedingTextDeleter : public ITfEditSession {
 public:
  PrecedingTextDeleter(ITfContext *context, size_t num_characters_in_ucs4)
      : context_(context), num_characters_in_ucs4_(num_characters_in_ucs4) {}

  // Destructor is kept as non-virtual because this class is designed to be
  // destroyed only by "delete this" in Release() method.
  // TODO(yukawa): put "final" keyword to the class declaration when C++11
  //     is allowed.
  ~PrecedingTextDeleter() {}

  // The IUnknown interface methods.
  virtual STDMETHODIMP QueryInterface(REFIID interface_id, void **object) {
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

  virtual STDMETHODIMP_(ULONG) AddRef() { return ref_count_.AddRefImpl(); }

  virtual STDMETHODIMP_(ULONG) Release() {
    const ULONG count = ref_count_.ReleaseImpl();
    if (count == 0) {
      delete this;
    }
    return count;
  }

 private:
  virtual STDMETHODIMP DoEditSession(TfEditCookie edit_cookie) {
    HRESULT result = S_OK;

    CComPtr<ITfRange> selected_range;
    result = TipRangeUtil::GetDefaultSelection(context_, edit_cookie,
                                               &selected_range, nullptr);
    if (FAILED(result)) {
      return result;
    }

    const TF_HALTCOND halt_cond = {nullptr, TF_ANCHOR_START, 0};

    CComPtr<ITfRange> preceeding_range;
    if (FAILED(selected_range->Clone(&preceeding_range))) {
      return E_FAIL;
    }
    if (FAILED(preceeding_range->Collapse(edit_cookie, TF_ANCHOR_START))) {
      return E_FAIL;
    }

    // If all the characters are surrogate-pair, |num_characters_in_ucs4_| * 2
    // is required.
    if (num_characters_in_ucs4_ >= kMaxCharacterLength) {
      return E_UNEXPECTED;
    }
    const LONG initial_offset_utf16 =
        -static_cast<LONG>(num_characters_in_ucs4_) * 2;
    LONG preceeding_range_shifted = 0;
    if (FAILED(preceeding_range->ShiftStart(edit_cookie, initial_offset_utf16,
                                            &preceeding_range_shifted,
                                            &halt_cond))) {
      return E_FAIL;
    }
    std::wstring total_string;
    if (FAILED(TipRangeUtil::GetText(preceeding_range, edit_cookie,
                                     &total_string))) {
      return E_FAIL;
    }
    if (total_string.empty()) {
      return E_FAIL;
    }

    size_t len_in_utf16 = 0;
    if (!TipSurroundingTextUtil::MeasureCharactersBackward(
            total_string, num_characters_in_ucs4_, &len_in_utf16)) {
      return E_FAIL;
    }

    const LONG final_offset = total_string.size() - len_in_utf16;
    if (FAILED(preceeding_range->ShiftStart(edit_cookie, final_offset,
                                            &preceeding_range_shifted,
                                            &halt_cond))) {
      return E_FAIL;
    }
    if (final_offset != preceeding_range_shifted) {
      return E_FAIL;
    }
    if (FAILED(preceeding_range->SetText(edit_cookie, 0, L"", 0))) {
      return E_FAIL;
    }

    return S_OK;
  }

  TipRefCount ref_count_;
  CComPtr<ITfContext> context_;
  size_t num_characters_in_ucs4_;

  DISALLOW_COPY_AND_ASSIGN(PrecedingTextDeleter);
};

bool PrepareForReconversionIMM32(ITfContext *context,
                                 TipSurroundingTextInfo *info) {
  CComPtr<ITfContextView> context_view;
  if (FAILED(context->GetActiveView(&context_view))) {
    return false;
  }
  if (context_view == nullptr) {
    return false;
  }
  HWND attached_window = nullptr;
  if (FAILED(context_view->GetWnd(&attached_window))) {
    return false;
  }

  LRESULT result =
      ::SendMessage(attached_window, WM_IME_REQUEST, IMR_RECONVERTSTRING, 0);
  if (result == 0) {
    // IMR_RECONVERTSTRING is not supported.
    return false;
  }

  const size_t buffer_size = static_cast<size_t>(result);
  unique_ptr<BYTE[]> buffer(new BYTE[buffer_size]);

  RECONVERTSTRING *reconvert_string =
      reinterpret_cast<RECONVERTSTRING *>(buffer.get());
  reconvert_string->dwSize = buffer_size;
  reconvert_string->dwVersion = 0;

  result = ::SendMessage(attached_window, WM_IME_REQUEST, IMR_RECONVERTSTRING,
                         reinterpret_cast<LPARAM>(reconvert_string));
  if (result == 0) {
    return false;
  }

  std::wstring preceding_text;
  std::wstring preceding_composition;
  std::wstring target;
  std::wstring following_composition;
  std::wstring following_text;
  if (!ReconvertString::Decompose(reconvert_string, &preceding_text,
                                  &preceding_composition, &target,
                                  &following_composition, &following_text)) {
    return false;
  }
  info->in_composition = false;
  info->is_transitory = false;
  info->has_preceding_text = true;
  info->preceding_text = preceding_text;
  info->has_selected_text = true;
  info->selected_text = preceding_composition + target + following_composition;
  info->has_following_text = true;
  info->following_text = following_text;

  return true;
}

}  // namespace

TipSurroundingTextInfo::TipSurroundingTextInfo()
    : has_preceding_text(false),
      has_selected_text(false),
      has_following_text(false),
      is_transitory(false),
      in_composition(false) {}

bool TipSurroundingText::Get(TipTextService *text_service, ITfContext *context,
                             TipSurroundingTextInfo *info) {
  if (info == nullptr) {
    return false;
  }
  *info = TipSurroundingTextInfo();

  // Use Transitory Extensions when supported. Common controls provides
  // surrounding text via Transitory Extensions.
  CComPtr<ITfContext> target_context(
      TipTransitoryExtension::ToParentContextIfExists(context));

  // When RequestEditSession fails, it does not maintain the reference count.
  // So we need to ensure that AddRef/Release should be called at least once
  // per object.
  CComPtr<SurroudingTextUpdater> updater(
      new SurroudingTextUpdater(target_context, false));

  HRESULT edit_session_result = S_OK;
  const HRESULT hr = target_context->RequestEditSession(
      text_service->GetClientID(), updater, TF_ES_SYNC | TF_ES_READ,
      &edit_session_result);
  if (FAILED(hr)) {
    return false;
  }
  if (FAILED(edit_session_result)) {
    return false;
  }

  *info = updater->result();

  return true;
}

bool PrepareForReconversionTSF(TipTextService *text_service,
                               ITfContext *context,
                               TipSurroundingTextInfo *info) {
  // Use Transitory Extensions when supported. Common controls provides
  // surrounding text via Transitory Extensions.
  CComPtr<ITfContext> target_context(
      TipTransitoryExtension::ToParentContextIfExists(context));

  // When RequestEditSession fails, it does not maintain the reference count.
  // So we need to ensure that AddRef/Release should be called at least once
  // per oject.
  CComPtr<SurroudingTextUpdater> updater(
      new SurroudingTextUpdater(target_context, true));

  HRESULT edit_session_result = S_OK;
  const HRESULT hr = target_context->RequestEditSession(
      text_service->GetClientID(), updater, TF_ES_SYNC | TF_ES_READWRITE,
      &edit_session_result);
  if (FAILED(hr)) {
    return false;
  }
  if (FAILED(edit_session_result)) {
    return false;
  }

  *info = updater->result();
  return true;
}

bool TipSurroundingText::PrepareForReconversionFromIme(
    TipTextService *text_service, ITfContext *context,
    TipSurroundingTextInfo *info, bool *need_async_reconversion) {
  if (info == nullptr) {
    return false;
  }
  if (need_async_reconversion == nullptr) {
    return false;
  }
  *info = TipSurroundingTextInfo();
  *need_async_reconversion = false;
  if (PrepareForReconversionTSF(text_service, context, info)) {
    // Here we assume selection text info is valid iff |info->is_transitory| is
    // false.
    // TODO(yukawa): Investigate more reliable method to determine this.
    if (!info->is_transitory) {
      return true;
    }
  }
  if (!PrepareForReconversionIMM32(context, info)) {
    return false;
  }
  // IMM32-like reconversion requires async edit session.
  *need_async_reconversion = true;
  return true;
}

bool TipSurroundingText::DeletePrecedingText(
    TipTextService *text_service, ITfContext *context,
    size_t num_characters_to_be_deleted_in_ucs4) {
  // Use Transitory Extensions when supported. Common controls provides
  // surrounding text via Transitory Extensions.
  CComPtr<ITfContext> target_context(
      TipTransitoryExtension::ToParentContextIfExists(context));

  // When RequestEditSession fails, it does not maintain the reference count.
  // So we need to ensure that AddRef/Release should be called at least once
  // per oject.
  CComPtr<ITfEditSession> edit_session(new PrecedingTextDeleter(
      target_context, num_characters_to_be_deleted_in_ucs4));

  HRESULT edit_session_result = S_OK;
  const HRESULT hr = target_context->RequestEditSession(
      text_service->GetClientID(), edit_session, TF_ES_SYNC | TF_ES_READWRITE,
      &edit_session_result);
  if (FAILED(hr)) {
    return false;
  }
  if (FAILED(edit_session_result)) {
    return false;
  }
  return true;
}

bool TipSurroundingTextUtil::MeasureCharactersBackward(
    const std::wstring &text, size_t characters_in_ucs4,
    size_t *characters_in_utf16) {
  if (characters_in_utf16 == nullptr) {
    return false;
  }
  *characters_in_utf16 = 0;

  // Count characters from the end of |text| with taking surrogate pair into
  // consideration. Finally, we will find that |num_char_in_ucs4| characters
  // consist of |checked_len_in_utf16| UTF16 elements.
  size_t checked_len_in_utf16 = 0;
  size_t num_char_in_ucs4 = 0;
  while (true) {
    if (num_char_in_ucs4 >= characters_in_ucs4) {
      break;
    }
    if (checked_len_in_utf16 + 1 > text.size()) {
      break;
    }
    ++checked_len_in_utf16;
    const size_t index_low = text.size() - checked_len_in_utf16;
    if (IS_LOW_SURROGATE(text[index_low])) {
      if (checked_len_in_utf16 + 1 <= text.size()) {
        const size_t index_high = text.size() - checked_len_in_utf16 - 1;
        if (IS_HIGH_SURROGATE(text[index_high])) {
          ++checked_len_in_utf16;
        }
      }
    }
    ++num_char_in_ucs4;
  }

  if (num_char_in_ucs4 != characters_in_ucs4) {
    return false;
  }
  *characters_in_utf16 = checked_len_in_utf16;
  return true;
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc
