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

#include "win32/ime/ime_types.h"

// clang-format off
#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlmisc.h>
// clang-format on

namespace mozc {
namespace win32 {
namespace {
template <LPARAM bit>
bool BitwiseTest(LPARAM lParam) {
  return (lParam & bit) == bit;
}
}  // namespace

const LPARAM kNotifyUpdateUI = 0x10000;
const LPARAM kNotifyReconvertFromIME = 0x10001;

CompositionChangeAttributes::CompositionChangeAttributes()
    : composition_attribute(false),
      composition_clause(false),
      composition_reading_string(false),
      composition_reading_attribute(false),
      composition_reading_clause(false),
      composition_string(false),
      cursor_position(false),
      delta_start(false),
      result_clause(false),
      result_reading_clause(false),
      result_string(false),
      insert_char(false),
      move_caret(false),
      original_flags(0),
      remaining_flags(0) {}

CompositionChangeAttributes::CompositionChangeAttributes(LPARAM lParam)
    : composition_attribute(BitwiseTest<GCS_COMPATTR>(lParam)),
      composition_clause(BitwiseTest<GCS_COMPCLAUSE>(lParam)),
      composition_reading_string(BitwiseTest<GCS_COMPREADSTR>(lParam)),
      composition_reading_attribute(BitwiseTest<GCS_COMPREADATTR>(lParam)),
      composition_reading_clause(BitwiseTest<GCS_COMPREADCLAUSE>(lParam)),
      composition_string(BitwiseTest<GCS_COMPSTR>(lParam)),
      cursor_position(BitwiseTest<GCS_CURSORPOS>(lParam)),
      delta_start(BitwiseTest<GCS_DELTASTART>(lParam)),
      result_clause(BitwiseTest<GCS_RESULTCLAUSE>(lParam)),
      result_reading_clause(BitwiseTest<GCS_RESULTREADCLAUSE>(lParam)),
      result_string(BitwiseTest<GCS_RESULTSTR>(lParam)),
      insert_char(BitwiseTest<CS_INSERTCHAR>(lParam)),
      move_caret(BitwiseTest<CS_NOMOVECARET>(lParam)),
      original_flags(lParam),
      remaining_flags(GetRemainingBits(lParam)) {}

LPARAM CompositionChangeAttributes::AsLParam() const { return original_flags; }

LPARAM CompositionChangeAttributes::GetRemainingBits(WPARAM lParam) {
  constexpr DWORD kKnownBits =
      GCS_COMPATTR | GCS_COMPCLAUSE | GCS_COMPREADSTR | GCS_COMPREADATTR |
      GCS_COMPREADCLAUSE | GCS_COMPSTR | GCS_CURSORPOS | GCS_DELTASTART |
      GCS_RESULTCLAUSE | GCS_RESULTREADCLAUSE | GCS_RESULTREADSTR |
      GCS_RESULTSTR | CS_INSERTCHAR | CS_NOMOVECARET;
  return lParam & ~kKnownBits;
}

ShowUIAttributes::ShowUIAttributes()
    : composition_window(false),
      guide_window(false),
      candidate_window0(false),
      candidate_window1(false),
      candidate_window2(false),
      candidate_window3(false),
      original_flags(0),
      remaining_flags(0) {}

ShowUIAttributes::ShowUIAttributes(LPARAM lParam)
    : composition_window(BitwiseTest<ISC_SHOWUICOMPOSITIONWINDOW>(lParam)),
      guide_window(BitwiseTest<ISC_SHOWUIGUIDELINE>(lParam)),
      candidate_window0(BitwiseTest<ISC_SHOWUICANDIDATEWINDOW>(lParam)),
      candidate_window1(BitwiseTest<(ISC_SHOWUICANDIDATEWINDOW << 1)>(lParam)),
      candidate_window2(BitwiseTest<(ISC_SHOWUICANDIDATEWINDOW << 2)>(lParam)),
      candidate_window3(BitwiseTest<(ISC_SHOWUICANDIDATEWINDOW << 3)>(lParam)),
      original_flags(lParam),
      remaining_flags(GetRemainingBits(lParam)) {}

bool ShowUIAttributes::AreAllUICandidateWindowAllowed() const {
  return BitwiseTest<ISC_SHOWUIALLCANDIDATEWINDOW>(original_flags);
}

bool ShowUIAttributes::AreAllUIAllowed() const {
  return BitwiseTest<ISC_SHOWUIALL>(original_flags);
}

LPARAM ShowUIAttributes::AsLParam() const { return original_flags; }

LPARAM ShowUIAttributes::GetRemainingBits(WPARAM lParam) {
  constexpr DWORD kKnownBits =
      ISC_SHOWUICOMPOSITIONWINDOW | ISC_SHOWUIGUIDELINE |
      ISC_SHOWUICANDIDATEWINDOW | (ISC_SHOWUICANDIDATEWINDOW << 1) |
      (ISC_SHOWUICANDIDATEWINDOW << 2) | (ISC_SHOWUICANDIDATEWINDOW << 3);
  return lParam & ~kKnownBits;
}

UIMessage::UIMessage() : message_(0), lparam_(0), wparam_(0) {}

UIMessage::UIMessage(UINT message, WPARAM wparam, LPARAM lparam)
    : message_(message), wparam_(wparam), lparam_(lparam) {}

UINT UIMessage::message() const { return message_; }

WPARAM UIMessage::wparam() const { return wparam_; }

LPARAM UIMessage::lparam() const { return lparam_; }
}  // namespace win32
}  // namespace mozc
