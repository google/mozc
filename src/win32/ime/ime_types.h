// Copyright 2010-2012, Google Inc.
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

#ifndef MOZC_WIN32_IME_IME_TYPES_H_
#define MOZC_WIN32_IME_IME_TYPES_H_

#include <windows.h>
#include "win32/base/immdev.h"

namespace mozc {

namespace commands {
class RendererCommand;
}  // namespace commands

namespace win32 {
class UIContext;

extern const LPARAM kNotifyUpdateUI;
extern const LPARAM kNotifyReconvertFromIME;

class UIMessage {
 public:
  UIMessage();
  UIMessage(UINT message, WPARAM wparam, LPARAM lparam);

  UINT message() const;
  WPARAM wparam() const;
  LPARAM lparam() const;

 private:
  UINT message_;
  WPARAM wparam_;
  LPARAM lparam_;
};

struct CompositionChangeAttributes {
  CompositionChangeAttributes();
  explicit CompositionChangeAttributes(LPARAM lParam);
  LPARAM AsLParam() const;
  const bool composition_attribute;
  const bool composition_clause;
  const bool composition_reading_string;
  const bool composition_reading_attribute;
  const bool composition_reading_clause;
  const bool composition_string;
  const bool cursor_position;
  const bool delta_start;
  const bool result_clause;
  const bool result_reading_clause;
  const bool result_string;
  const bool insert_char;
  const bool move_caret;
  const WPARAM original_flags;
  const WPARAM remaining_flags;
 private:
  static LPARAM GetRemainingBits(WPARAM lParam);
};

struct ShowUIAttributes {
  ShowUIAttributes();
  explicit ShowUIAttributes(LPARAM lParam);

  bool AreAllUICandidateWindowAllowed() const;
  bool AreAllUIAllowed() const;
  LPARAM AsLParam() const;

  const bool composition_window;
  const bool guide_window;
  const bool candidate_window0;
  const bool candidate_window1;
  const bool candidate_window2;
  const bool candidate_window3;
  const WPARAM original_flags;
  const WPARAM remaining_flags;

 private:
  static LPARAM GetRemainingBits(WPARAM lParam);
};
}  // namespace win32
}  // namespace mozc
#endif  // MOZC_WIN32_IME_IME_TYPES_H_
