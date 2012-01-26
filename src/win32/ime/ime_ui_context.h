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

#ifndef MOZC_WIN32_IME_IME_UI_CONTEXT_H_
#define MOZC_WIN32_IME_IME_UI_CONTEXT_H_

#include <windows.h>
#include <imm.h>

#include <string>
#include <vector>

#include "win32/base/immdev.h"
#include "win32/ime/ime_scoped_context.h"

namespace mozc {
namespace client {
class ClientInterface;
}  // namespace client

namespace commands {
class Output;
class RendererCommand_ApplicationInfo;
}  // namespace commands

namespace win32 {
struct PrivateContext;
class ImeCore;
class UIVisibilityTracker;

// TODO(yukawa): Refactor for unit tests and better integration with ImeCore.
class UIContext {
 public:
  explicit UIContext(HIMC context_handle);
  bool GetLastOutput(commands::Output *output) const;
  HWND GetAttachedWindow() const;
  wstring GetAttachedWindowClass() const;
  bool IsEmpty() const;
  bool IsCompositionStringEmpty() const;
  bool GetFocusedCharacterIndexInComposition(DWORD *index) const;
  bool GetCompositionForm(COMPOSITIONFORM *composition_form) const;
  bool GetCandidateForm(
      DWORD form_index, CANDIDATEFORM *candidate_form) const;
  bool GetCompositionFont(LOGFONTW *font) const;
  bool GetConversionMode(DWORD *conversion) const;
  bool GetOpenStatus() const;
  bool IsKanaInputPreferred() const;

  mozc::client::ClientInterface *client() const;
  const INPUTCONTEXT *input_context() const;
  UIVisibilityTracker *ui_visibility_tracker() const;

  bool FillCompositionForm(
      commands::RendererCommand_ApplicationInfo *info) const;
  bool FillCandidateForm(
      commands::RendererCommand_ApplicationInfo *info) const;
  bool FillCharPosition(commands::RendererCommand_ApplicationInfo *info) const;
  bool FillCaretInfo(commands::RendererCommand_ApplicationInfo *info) const;
  bool FillFontInfo(commands::RendererCommand_ApplicationInfo *info) const;
 private:
  const HIMC context_handle_;
  const ScopedHIMC<INPUTCONTEXT> input_context_;
  const ScopedHIMCC<PrivateContext> private_context_;
};
}  // namespace win32
}  // namespace mozc

#endif  // MOZC_WIN32_IME_IME_UI_CONTEXT_H_
