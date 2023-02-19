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

#include "win32/ime/ime_ui_context.h"

// clang-format off
#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
#include <atlbase.h>
#include <atltypes.h>
#include <atlapp.h>
#include <atlstr.h>
#include <atlmisc.h>
#include <commctrl.h>  // for CCSIZEOF_STRUCT
// clang-format on

#include <strsafe.h>

#include "base/win32/win_util.h"
#include "client/client_interface.h"
#include "protocol/renderer_command.pb.h"
#include "renderer/win32/win32_font_util.h"
#include "win32/base/immdev.h"
#include "win32/base/indicator_visibility_tracker.h"
#include "win32/base/input_state.h"
#include "win32/ime/ime_composition_string.h"
#include "win32/ime/ime_private_context.h"

namespace mozc {
namespace win32 {

typedef mozc::commands::RendererCommand RendererCommand;
typedef mozc::commands::RendererCommand::ApplicationInfo ApplicationInfo;

namespace {
constexpr size_t kSizeOfImeCharPositionV1 =
    CCSIZEOF_STRUCT(IMECHARPOSITION, rcDocument);
constexpr size_t kSizeOfGUIThreadInfoV1 =
    CCSIZEOF_STRUCT(GUITHREADINFO, rcCaret);

HIMCC GetPrivateContextHandle(const INPUTCONTEXT *input_context) {
  if (input_context == nullptr) {
    return nullptr;
  }
  if (!PrivateContextUtil::IsValidPrivateContext(input_context->hPrivate)) {
    return nullptr;
  }
  return input_context->hPrivate;
}

void SetCharPosition(const IMECHARPOSITION &position,
                     commands::RendererCommand::CharacterPosition *target) {
  target->set_line_height(position.cLineHeight);
  target->set_position(position.dwCharPos);

  RendererCommand::Point *point = target->mutable_top_left();
  point->set_x(position.pt.x);
  point->set_y(position.pt.y);

  RendererCommand::Rectangle *rect = target->mutable_document_area();
  rect->set_left(position.rcDocument.left);
  rect->set_top(position.rcDocument.top);
  rect->set_bottom(position.rcDocument.bottom);
  rect->set_right(position.rcDocument.right);
}
}  // namespace

UIContext::UIContext(HIMC context_handle)
    : context_handle_(context_handle),
      input_context_(context_handle),
      private_context_(GetPrivateContextHandle(input_context_.get())) {}

bool UIContext::GetLastOutput(mozc::commands::Output *output) const {
  if (output == nullptr) {
    return false;
  }
  if (private_context_.get() == nullptr) {
    return false;
  }
  if (!private_context_->Validate()) {
    return false;
  }
  *output = *private_context_->last_output;
  return true;
}

// TODO(yukawa): Check if this procedure is safe or not.
HWND UIContext::GetAttachedWindow() const {
  if (input_context_.get() == nullptr) {
    return nullptr;
  }
  return input_context_->hWnd;
}

bool UIContext::IsEmpty() const { return (context_handle_ == nullptr); }

bool UIContext::IsCompositionStringEmpty() const {
  if (input_context_.get() == nullptr) {
    return true;
  }
  ScopedHIMCC<COMPOSITIONSTRING> composition_string(input_context_->hCompStr);
  if (composition_string.get() == nullptr) {
    return true;
  }
  return (composition_string->dwCompStrLen == 0);
}

bool UIContext::GetFocusedCharacterIndexInComposition(DWORD *index) const {
  DWORD dummy_dword = 0;
  if (index == nullptr) {
    index = &dummy_dword;
  }

  *index = 0;
  if (input_context_.get() == nullptr) {
    return false;
  }
  if (input_context_->hCompStr == nullptr) {
    return false;
  }
  if (::ImmGetIMCCSize(input_context_->hCompStr) != sizeof(CompositionString)) {
    return false;
  }
  ScopedHIMCC<CompositionString> composition_string(input_context_->hCompStr);
  if (composition_string.get() == nullptr) {
    return false;
  }
  *index = composition_string->focused_character_index();
  return true;
}

bool UIContext::GetCompositionForm(COMPOSITIONFORM *composition_form) const {
  if (input_context_.get() == nullptr) {
    return false;
  }
  if (composition_form == nullptr) {
    return false;
  }
  if ((input_context_->fdwInit & INIT_COMPFORM) != INIT_COMPFORM) {
    return false;
  }
  *composition_form = input_context_->cfCompForm;
  return true;
}

bool UIContext::GetCandidateForm(DWORD form_index,
                                 CANDIDATEFORM *candidate_form) const {
  if (input_context_.get() == nullptr) {
    return false;
  }
  if (candidate_form == nullptr) {
    return false;
  }

  // Currently the array size is 4.
  if (form_index >= std::size(input_context_->cfCandForm)) {
    return false;
  }

  if (input_context_->cfCandForm[form_index].dwIndex != form_index) {
    return false;
  }
  *candidate_form = input_context_->cfCandForm[form_index];
  return true;
}

bool UIContext::GetCompositionFont(LOGFONTW *font) const {
  if (input_context_.get() == nullptr) {
    return false;
  }
  if (font == nullptr) {
    return false;
  }
  // ImmGetCompositionFontW internally checks if INPUTCONTEXT::fdwInit has
  // INIT_LOGFONT bit or not.  ImmGetCompositionFontW works well even when
  // the target window is a native Unicode window except that
  // INPUTCONTEXT::lfFont has already been corrupted by some IMEs such as
  // ATOK 2009.  See b/3042347 for details.
  if (::ImmGetCompositionFontW(context_handle_, font) == FALSE) {
    return false;
  }

  // There exist some troublesome applications which set broken composition
  // font. We ignore such composition font if its face-name is empty.
  // See b/4506404 for details.
  bool null_terminated = false;
  bool empty_facename = true;
  for (size_t i = 0; i < std::size(font->lfFaceName); ++i) {
    if (font->lfFaceName[i] == L'\0') {
      null_terminated = true;
      break;
    }
    empty_facename = false;
  }
  if (!null_terminated) {
    return false;
  }
  if (empty_facename) {
    return false;
  }

  return true;
}

bool UIContext::GetConversionMode(DWORD *conversion) const {
  if (input_context_.get() == nullptr) {
    return false;
  }
  if (conversion == nullptr) {
    return false;
  }
  if ((input_context_->fdwInit & INIT_CONVERSION) != INIT_CONVERSION) {
    return false;
  }
  *conversion = input_context_->fdwConversion;
  return true;
}

bool UIContext::GetVisibleConversionMode(DWORD *conversion) const {
  if (conversion == nullptr) {
    return false;
  }
  if (input_context_.get() == nullptr) {
    return false;
  }
  if (private_context_.get() == nullptr) {
    return false;
  }
  if (!private_context_->Validate()) {
    return false;
  }
  *conversion = private_context_->ime_state->visible_conversion_mode;
  return true;
}

bool UIContext::GetLogicalConversionMode(DWORD *conversion) const {
  if (conversion == nullptr) {
    return false;
  }
  if (input_context_.get() == nullptr) {
    return false;
  }
  if (private_context_.get() == nullptr) {
    return false;
  }
  if (!private_context_->Validate()) {
    return false;
  }
  *conversion = private_context_->ime_state->logical_conversion_mode;
  return true;
}

bool UIContext::GetOpenStatus() const {
  if (input_context_.get() == nullptr) {
    return false;
  }
  return (input_context_->fOpen != FALSE);
}

bool UIContext::IsKanaInputPreferred() const {
  if (input_context_.get() == nullptr) {
    return false;
  }
  if (private_context_.get() == nullptr) {
    return false;
  }
  if (!private_context_->Validate()) {
    return false;
  }
  if (private_context_->ime_behavior == nullptr) {
    return false;
  }
  return private_context_->ime_behavior->prefer_kana_input;
}

bool UIContext::IsModeIndicatorEnabled() const {
  if (input_context_.get() == nullptr) {
    return false;
  }
  if (private_context_.get() == nullptr) {
    return false;
  }
  if (!private_context_->Validate()) {
    return false;
  }
  if (private_context_->ime_behavior == nullptr) {
    return false;
  }
  return private_context_->ime_behavior->use_mode_indicator;
}

mozc::client::ClientInterface *UIContext::client() const {
  if (input_context_.get() == nullptr) {
    return nullptr;
  }
  if (private_context_.get() == nullptr) {
    return nullptr;
  }
  return private_context_->client;
}

const INPUTCONTEXT *UIContext::input_context() const {
  if (input_context_.get() == nullptr) {
    return nullptr;
  }
  return input_context_.get();
}

UIVisibilityTracker *UIContext::ui_visibility_tracker() const {
  if (input_context_.get() == nullptr) {
    return nullptr;
  }
  if (private_context_.get() == nullptr) {
    return nullptr;
  }
  return private_context_->ui_visibility_tracker;
}

IndicatorVisibilityTracker *UIContext::indicator_visibility_tracker() const {
  if (input_context_.get() == nullptr) {
    return nullptr;
  }
  if (private_context_.get() == nullptr) {
    return nullptr;
  }
  return private_context_->indicator_visibility_tracker;
}

bool UIContext::FillCompositionForm(ApplicationInfo *info) const {
  COMPOSITIONFORM composition_form = {0};
  if (!GetCompositionForm(&composition_form)) {
    return false;
  }

  commands::RendererCommand::CompositionForm *form =
      info->mutable_composition_form();
  form->set_style_bits(composition_form.dwStyle);

  // Set |current_pos|.
  RendererCommand::Point *point = form->mutable_current_position();
  point->set_x(composition_form.ptCurrentPos.x);
  point->set_y(composition_form.ptCurrentPos.y);

  // Set |area|
  RendererCommand::Rectangle *area = form->mutable_area();
  area->set_left(composition_form.rcArea.left);
  area->set_top(composition_form.rcArea.top);
  area->set_right(composition_form.rcArea.right);
  area->set_bottom(composition_form.rcArea.bottom);

  return true;
}

bool UIContext::FillCandidateForm(ApplicationInfo *info) const {
  CANDIDATEFORM candidate_form = {0};
  if (!GetCandidateForm(0, &candidate_form)) {
    return false;
  }

  RendererCommand::CandidateForm *form = info->mutable_candidate_form();
  form->set_style_bits(candidate_form.dwStyle);

  // Set |current_pos|.
  RendererCommand::Point *point = form->mutable_current_position();
  point->set_x(candidate_form.ptCurrentPos.x);
  point->set_y(candidate_form.ptCurrentPos.y);

  // Set |area| if available.
  RendererCommand::Rectangle *area = form->mutable_area();
  area->set_left(candidate_form.rcArea.left);
  area->set_top(candidate_form.rcArea.top);
  area->set_right(candidate_form.rcArea.right);
  area->set_bottom(candidate_form.rcArea.bottom);

  return true;
}

bool UIContext::FillCharPosition(ApplicationInfo *info) const {
  // Some applications such as Excel sometimes get stuck in the message handler
  // against IMR_QUERYCHARPOSITION.  b/4285222.
  // To reduce the risk of hung-up, do nothing if the target window is not the
  // focused window.
  const HWND window_handle = input_context()->hWnd;
  if (!::IsWindow(window_handle)) {
    return false;
  }
  if (window_handle != ::GetFocus()) {
    return false;
  }

  // This index must be calculated in unit of wide characters to support
  // surrogate pair. See b/4159275 for details.
  DWORD target_char_index = 0;
  if (!GetFocusedCharacterIndexInComposition(&target_char_index)) {
    return false;
  }

  IMECHARPOSITION position = {};
  position.dwSize = kSizeOfImeCharPositionV1;
  position.dwCharPos = target_char_index;
  if (::ImmRequestMessageW(context_handle_, IMR_QUERYCHARPOSITION,
                           reinterpret_cast<LPARAM>(&position)) == 0) {
    return false;
  }
  SetCharPosition(position, info->mutable_composition_target());
  return true;
}

bool UIContext::FillCaretInfo(ApplicationInfo *info) const {
  GUITHREADINFO thread_info = {0};
  thread_info.cbSize = kSizeOfGUIThreadInfoV1;
  if (::GetGUIThreadInfo(::GetCurrentThreadId(), &thread_info) == FALSE) {
    return false;
  }

  RendererCommand::CaretInfo *caret = info->mutable_caret_info();
  caret->set_blinking((thread_info.flags & GUI_CARETBLINKING) ==
                      GUI_CARETBLINKING);

  // Set |caret_rect|
  const CRect caret_rect(thread_info.rcCaret);
  RendererCommand::Rectangle *rect = caret->mutable_caret_rect();
  rect->set_left(thread_info.rcCaret.left);
  rect->set_top(thread_info.rcCaret.top);
  rect->set_right(thread_info.rcCaret.right);
  rect->set_bottom(thread_info.rcCaret.bottom);

  caret->set_target_window_handle(
      WinUtil::EncodeWindowHandle(thread_info.hwndCaret));

  return true;
}

bool UIContext::FillFontInfo(ApplicationInfo *info) const {
  LOGFONT composition_font = {0};
  if (!GetCompositionFont(&composition_font)) {
    return false;
  }

  FontUtil::ToWinLogFont(composition_font, info->mutable_composition_font());
  return true;
}

}  // namespace win32
}  // namespace mozc
