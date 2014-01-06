// Copyright 2010-2014, Google Inc.
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

#include "win32/ime/ime_ui_visibility_tracker.h"

#include "win32/ime/ime_types.h"

namespace mozc {
namespace win32 {
namespace {
bool kDefaultUIActivated = false;
bool kDefaultCandidateWindowActivated = false;
// Note that WM_IME_SETCONTEXT will not be sent when a user changes IME by the
// LangBar.  To show the suggest window in this case, the default visibility
// of the suggest window must be true.
bool kDefaultSuggestWindowActivated = true;
bool kDefaultCompositionWindowActivated = false;
}  // anonymous namespace

UIVisibilityTracker::UIVisibilityTracker()
    : ui_activated_(kDefaultUIActivated),
      candidate_window_activated_(kDefaultCandidateWindowActivated),
      suggest_window_activated_(kDefaultSuggestWindowActivated),
      composition_window_activated_(kDefaultCompositionWindowActivated) {}

bool UIVisibilityTracker::IsVisibilityTestMessageForComposiwionWindow(
    UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
    case WM_IME_STARTCOMPOSITION:
    case WM_IME_ENDCOMPOSITION:
      return true;
    case WM_IME_COMPOSITION:
      if ((lParam & GCS_RESULTSTR) == GCS_RESULTSTR) {
        return false;
      }
      return true;
    default:
      // do nothing.
      return false;
  }
  return false;
}

bool UIVisibilityTracker::IsVisibilityTestMessageForCandidateWindow(
    UINT message, WPARAM wParam, LPARAM lParam) {
  if (message == WM_IME_NOTIFY) {
    switch (wParam) {
      case IMN_OPENCANDIDATE:
      case IMN_CLOSECANDIDATE:
        return true;
      default:
        // do nothing.
        return false;
    }
  }
  return false;
}

void UIVisibilityTracker::Initialize() {
  ui_activated_ = kDefaultUIActivated;
  candidate_window_activated_ = kDefaultCandidateWindowActivated;
  suggest_window_activated_ = kDefaultSuggestWindowActivated;
  composition_window_activated_ = kDefaultCompositionWindowActivated;
}

void UIVisibilityTracker::BeginVisibilityTestForCompositionWindow() {
  composition_window_activated_ = false;
}

void UIVisibilityTracker::BeginVisibilityTestForCandidateWindow() {
  candidate_window_activated_ = false;
}

void UIVisibilityTracker::OnFocus() {
  ui_activated_ = true;
}

void UIVisibilityTracker::OnBlur() {
  ui_activated_ = false;
}

void UIVisibilityTracker::OnNotify(DWORD sub_message, LPARAM lParam) {
  switch (sub_message) {
    case IMN_OPENCANDIDATE:
      // Although each bit in |lParam| corresponds to the index of candidate
      // window, currently those bits are ignored.
      candidate_window_activated_ = true;
      break;
    case IMN_CHANGECANDIDATE:
      // MS-IME and ATOK do not make candidate window visible when they receive
      // IMN_CHANGECANDIDATE.  We conform to them.
      break;
    case IMN_CLOSECANDIDATE:
      // Although each bit in |lParam| corresponds to the index of candidate
      // window, currently those bits are ignored.
      candidate_window_activated_ = false;
      break;
    default:
      // do nothing.
      break;
  }
}

void UIVisibilityTracker::OnStartComposition() {
  composition_window_activated_ = true;
}

void UIVisibilityTracker::OnComposition() {
  // When the UI window of MS-IME receives WM_IME_COMPOSITION, it begins to
  // draw composition window as opposed to ATOK.
  // We conform to MS-IME's style.
  composition_window_activated_ = true;
}

void UIVisibilityTracker::OnEndComposition() {
  composition_window_activated_ = false;
}

void UIVisibilityTracker::OnSetContext(
    const ShowUIAttributes &show_ui_attributes) {
  // You should carefully choose the condidion not to show the suggest window.
  // It turned out that using |AreAllUIAllowed()| is not appropriate because
  // there is a well-mannered application which clears the
  // ISC_SHOWUICOMPOSITIONWINDOW bit since it draws the composition string
  // by itself.  If |AreAllUIAllowed()| is used here, suggest window never
  // be shown in such a well-mannered application like Chrome, as filed in
  // b/3002445
  suggest_window_activated_ =
      show_ui_attributes.AreAllUICandidateWindowAllowed();

  if (!show_ui_attributes.candidate_window0) {
    candidate_window_activated_ = false;
  }

  if (!show_ui_attributes.composition_window) {
    composition_window_activated_ = false;
  }

  return;
}

bool UIVisibilityTracker::IsAnyWindowVisible() const {
  if (IsCandidateWindowVisible()) {
    return true;
  }
  if (IsSuggestWindowVisible()) {
    return true;
  }
  if (IsCompositionWindowVisible()) {
    return true;
  }
  return false;
}

bool UIVisibilityTracker::IsCandidateWindowVisible() const {
  if (!ui_activated()) {
    return false;
  }
  if (!candidate_window_activated()) {
    return false;
  }
  return true;
}

bool UIVisibilityTracker::IsSuggestWindowVisible() const {
  if (!ui_activated()) {
    return false;
  }
  if (!suggest_window_activated()) {
    return false;
  }
  return true;
}

bool UIVisibilityTracker::IsCompositionWindowVisible() const {
  if (!ui_activated()) {
    return false;
  }
  if (!composition_window_activated()) {
    return false;
  }
  return true;
}


bool UIVisibilityTracker::ui_activated() const {
  return ui_activated_;
}

bool UIVisibilityTracker::candidate_window_activated() const {
  return candidate_window_activated_;
}

bool UIVisibilityTracker::suggest_window_activated() const {
  return suggest_window_activated_;
}

bool UIVisibilityTracker::composition_window_activated() const {
  return composition_window_activated_;
}
}  // namespace win32
}  // namespace mozc
