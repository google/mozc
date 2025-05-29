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

#include "win32/tip/tip_surrounding_text.h"

#include <msctf.h>
#include <wil/com.h>
#include <windows.h>

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/win32/com.h"
#include "base/win32/wide_char.h"
#include "win32/base/imm_reconvert_string.h"
#include "win32/tip/tip_composition_util.h"
#include "win32/tip/tip_dll_module.h"
#include "win32/tip/tip_range_util.h"
#include "win32/tip/tip_text_service.h"
#include "win32/tip/tip_transitory_extension.h"

namespace mozc {
namespace win32 {
namespace tsf {
namespace {

constexpr int kMaxSurroundingLength = 20;
constexpr int kMaxCharacterLength = 1024 * 1024;

class SurroudingTextUpdater final : public TipComImplements<ITfEditSession> {
 public:
  SurroudingTextUpdater(wil::com_ptr_nothrow<ITfContext> context,
                        bool move_anchor)
      : context_(std::move(context)), move_anchor_(move_anchor) {}

  const TipSurroundingTextInfo &result() const { return result_; }

 private:
  STDMETHODIMP DoEditSession(TfEditCookie edit_cookie) override {
    HRESULT result = S_OK;
    {
      wil::com_ptr_nothrow<ITfCompositionView> composition_view =
          TipCompositionUtil::GetCompositionView(context_.get(), edit_cookie);
      result_.in_composition = !!composition_view;
    }

    wil::com_ptr_nothrow<ITfRange> selected_range;
    {
      result = TipRangeUtil::GetDefaultSelection(context_.get(), edit_cookie,
                                                 &selected_range, nullptr);
      if (FAILED(result)) {
        return result;
      }

      result = TipRangeUtil::GetText(selected_range.get(), edit_cookie,
                                     &result_.selected_text);
      result_.has_selected_text = SUCCEEDED(result);

      // For reconversion, the active selection end should be moved to the
      // front character.
      if (move_anchor_) {
        result = TipRangeUtil::SetSelection(context_.get(), edit_cookie,
                                            selected_range.get(), TF_AE_START);
        if (FAILED(result)) {
          return result;
        }
      }
    }

    const TF_HALTCOND halt_cond = {nullptr, TF_ANCHOR_START, TF_HF_OBJECT};

    {
      wil::com_ptr_nothrow<ITfRange> preceding_range;
      LONG preceding_range_shifted = 0;
      if (SUCCEEDED(selected_range->Clone(&preceding_range)) &&
          SUCCEEDED(preceding_range->Collapse(edit_cookie, TF_ANCHOR_START)) &&
          SUCCEEDED(preceding_range->ShiftStart(
              edit_cookie, -kMaxSurroundingLength, &preceding_range_shifted,
              &halt_cond))) {
        HRESULT result = TipRangeUtil::GetText(
            preceding_range.get(), edit_cookie, &result_.preceding_text);
        result_.has_preceding_text = SUCCEEDED(result);
      }
    }

    {
      wil::com_ptr_nothrow<ITfRange> following_range;
      LONG following_range_shifted = 0;
      if (SUCCEEDED(selected_range->Clone(&following_range)) &&
          SUCCEEDED(following_range->Collapse(edit_cookie, TF_ANCHOR_END)) &&
          SUCCEEDED(following_range->ShiftEnd(
              edit_cookie, kMaxSurroundingLength, &following_range_shifted,
              &halt_cond))) {
        HRESULT result = TipRangeUtil::GetText(
            following_range.get(), edit_cookie, &result_.following_text);
        result_.has_following_text = SUCCEEDED(result);
      }
    }

    return S_OK;
  }

  wil::com_ptr_nothrow<ITfContext> context_;
  TipSurroundingTextInfo result_;
  bool move_anchor_;
};

class PrecedingTextDeleter final : public TipComImplements<ITfEditSession> {
 public:
  PrecedingTextDeleter(wil::com_ptr_nothrow<ITfContext> context,
                       size_t num_characters_in_codepoint)
      : context_(std::move(context)),
        num_characters_in_codepoint_(num_characters_in_codepoint) {}

 private:
  STDMETHODIMP DoEditSession(TfEditCookie edit_cookie) override {
    HRESULT result = S_OK;

    wil::com_ptr_nothrow<ITfRange> selected_range;
    result = TipRangeUtil::GetDefaultSelection(context_.get(), edit_cookie,
                                               &selected_range, nullptr);
    if (FAILED(result)) {
      return result;
    }

    const TF_HALTCOND halt_cond = {nullptr, TF_ANCHOR_START, 0};

    wil::com_ptr_nothrow<ITfRange> preceding_range;
    if (FAILED(selected_range->Clone(&preceding_range))) {
      return E_FAIL;
    }
    if (FAILED(preceding_range->Collapse(edit_cookie, TF_ANCHOR_START))) {
      return E_FAIL;
    }

    // If all the characters are surrogate-pair, |num_characters_in_codepoint_|
    // * 2 is required.
    if (num_characters_in_codepoint_ >= kMaxCharacterLength) {
      return E_UNEXPECTED;
    }
    const LONG initial_offset_utf16 =
        -static_cast<LONG>(num_characters_in_codepoint_) * 2;
    LONG preceding_range_shifted = 0;
    if (FAILED(preceding_range->ShiftStart(edit_cookie, initial_offset_utf16,
                                           &preceding_range_shifted,
                                           &halt_cond))) {
      return E_FAIL;
    }
    std::wstring total_string;
    if (FAILED(TipRangeUtil::GetText(preceding_range.get(), edit_cookie,
                                     &total_string))) {
      return E_FAIL;
    }
    if (total_string.empty()) {
      return E_FAIL;
    }

    size_t len_in_utf16 = 0;
    if (!TipSurroundingTextUtil::MeasureCharactersBackward(
            total_string, num_characters_in_codepoint_, &len_in_utf16)) {
      return E_FAIL;
    }

    const LONG final_offset = total_string.size() - len_in_utf16;
    if (FAILED(preceding_range->ShiftStart(
            edit_cookie, final_offset, &preceding_range_shifted, &halt_cond))) {
      return E_FAIL;
    }
    if (final_offset != preceding_range_shifted) {
      return E_FAIL;
    }
    if (FAILED(preceding_range->SetText(edit_cookie, 0, L"", 0))) {
      return E_FAIL;
    }

    return S_OK;
  }

  wil::com_ptr_nothrow<ITfContext> context_;
  size_t num_characters_in_codepoint_;
};

bool GetSurroundingTextImm32(ITfContext *context,
                             ReconvertString::RequestType request_type,
                             TipSurroundingTextInfo *info) {
  wil::com_ptr_nothrow<ITfContextView> context_view;
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

  UniqueReconvertString reconvert_string =
      ReconvertString::Request(attached_window, request_type);
  if (!reconvert_string) {
    return false;
  }

  std::optional<ReconvertString::Strings> ss = reconvert_string->Decompose();
  if (!ss.has_value()) {
    return false;
  }
  info->in_composition = false;
  info->has_preceding_text = true;
  info->preceding_text.assign(ss->preceding_text.begin(),
                              ss->preceding_text.end());
  info->has_selected_text = true;
  info->selected_text =
      StrCatW(ss->preceding_composition, ss->target, ss->following_composition);
  info->has_following_text = true;
  info->following_text.assign(ss->following_text.begin(),
                              ss->following_text.end());

  return true;
}

}  // namespace

bool TipSurroundingText::Get(TipTextService *text_service, ITfContext *context,
                             TipSurroundingTextInfo *info) {
  if (info == nullptr) {
    return false;
  }
  *info = TipSurroundingTextInfo();

  // Surrounding text retrieval through TSF APIs should be performed only with
  // the full context.
  wil::com_ptr_nothrow<ITfContext> full_context(
      TipTransitoryExtension::AsFullContext(context));
  if (full_context == nullptr) {
    // Legacy IMM32-based editors fall into this category.
    // Try to retrieve surrounding text through IMR_DOCUMENTFEED as a fallback.
    return GetSurroundingTextImm32(
        context, ReconvertString::RequestType::kDocumentFeed, info);
  }

  // When RequestEditSession fails, it does not maintain the reference count.
  // So we need to ensure that AddRef/Release should be called at least once
  // per object.
  auto updater = MakeComPtr<SurroudingTextUpdater>(full_context, false);

  HRESULT edit_session_result = S_OK;
  const HRESULT hr = full_context->RequestEditSession(
      text_service->GetClientID(), updater.get(), TF_ES_SYNC | TF_ES_READ,
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
  // Reconversion through TSF APIs should be performed only with the full
  // context.
  wil::com_ptr_nothrow<ITfContext> full_context(
      TipTransitoryExtension::AsFullContext(context));
  if (full_context == nullptr) {
    return false;
  }

  // When RequestEditSession fails, it does not maintain the reference count.
  // So we need to ensure that AddRef/Release should be called at least once
  // per object.
  auto updater = MakeComPtr<SurroudingTextUpdater>(full_context, true);

  HRESULT edit_session_result = S_OK;
  const HRESULT hr = full_context->RequestEditSession(
      text_service->GetClientID(), updater.get(), TF_ES_SYNC | TF_ES_READWRITE,
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
    return true;
  }
  if (!GetSurroundingTextImm32(context,
                               ReconvertString::RequestType::kReconvertString,
                               info)) {
    return false;
  }
  // IMM32-like reconversion requires async edit session.
  *need_async_reconversion = true;
  return true;
}

bool TipSurroundingText::DeletePrecedingText(
    TipTextService *text_service, ITfContext *context,
    size_t num_characters_to_be_deleted_in_codepoint) {
  // Surrounding text deletion through TSF APIs should be performed only with
  // the full context.
  wil::com_ptr_nothrow<ITfContext> full_context(
      TipTransitoryExtension::AsFullContext(context));
  if (full_context == nullptr) {
    return false;
  }

  // When RequestEditSession fails, it does not maintain the reference count.
  // So we need to ensure that AddRef/Release should be called at least once
  // per object.
  auto edit_session = MakeComPtr<PrecedingTextDeleter>(
      full_context, num_characters_to_be_deleted_in_codepoint);

  HRESULT edit_session_result = S_OK;
  const HRESULT hr = full_context->RequestEditSession(
      text_service->GetClientID(), edit_session.get(),
      TF_ES_SYNC | TF_ES_READWRITE, &edit_session_result);
  if (FAILED(hr)) {
    return false;
  }
  if (FAILED(edit_session_result)) {
    return false;
  }
  return true;
}

bool TipSurroundingTextUtil::MeasureCharactersBackward(
    const std::wstring_view text, const size_t characters_in_codepoint,
    size_t *characters_in_utf16) {
  if (characters_in_utf16 == nullptr) {
    return false;
  }
  *characters_in_utf16 = 0;

  // Count characters from the end of |text| with taking surrogate pair into
  // consideration. Finally, we will find that |num_char_in_codepoint|
  // characters consist of |checked_len_in_utf16| UTF16 elements.
  size_t checked_len_in_utf16 = 0;
  size_t num_char_in_codepoint = 0;
  while (true) {
    if (num_char_in_codepoint >= characters_in_codepoint) {
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
    ++num_char_in_codepoint;
  }

  if (num_char_in_codepoint != characters_in_codepoint) {
    return false;
  }
  *characters_in_utf16 = checked_len_in_utf16;
  return true;
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc
