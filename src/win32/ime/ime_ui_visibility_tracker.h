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

#ifndef MOZC_WIN32_IME_IME_UI_VISIBILITY_TRACKER_H_
#define MOZC_WIN32_IME_IME_UI_VISIBILITY_TRACKER_H_

#include <windows.h>

#include "base/port.h"

namespace mozc {
namespace win32 {
struct ShowUIAttributes;

class UIVisibilityTracker {
 public:
  UIVisibilityTracker();

  // Returns true if the given message is a visibility-test-message for the
  // composition window.  If a visibility-test-message is trapped by the
  // application, the application is responsible to render the composition.
  // To conform to this protocol, the IME should call
  // BeginVisibilityTestForCompositionWindow before the message is posted when
  // this function returns true.
  static bool IsVisibilityTestMessageForComposiwionWindow(
      UINT message, WPARAM wParam, LPARAM lParam);

  // Returns true if the given message is a visibility-test-message for the
  // candidate window.  If a visibility-test-message is trapped by the
  // application, the application is responsible to render the candidate list.
  // To conform to this protocol, the IME should call
  // BeginVisibilityTestForCandidateWindow before the message is posted when
  // this function returns true.
  static bool IsVisibilityTestMessageForCandidateWindow(
      UINT message, WPARAM wParam, LPARAM lParam);

  // Start visibility test for composition window.  This method can be called
  // multiple times.
  void BeginVisibilityTestForCompositionWindow();

  // Start visibility test for candidate window.  This method can be called
  // multiple times.
  void BeginVisibilityTestForCandidateWindow();

  // Initializes internal state.
  void Initialize();

  // Should be called when the IME gets focus.
  void OnFocus();

  // Should be called when the IME loses focus.
  void OnBlur();

  // Should be called when the IME UI window receives WM_IME_NOTIFY message.
  // http://msdn.microsoft.com/en-us/library/dd374139.aspx
  void OnNotify(DWORD sub_message, LPARAM lParam);

  // Should be called when the IME UI window receives WM_IME_STARTCOMPOSITION
  // message.
  // http://msdn.microsoft.com/en-us/library/dd374143.aspx
  void OnStartComposition();

  // Should be called when the IME UI window receives WM_IME_COMPOSITION
  // message.
  // http://msdn.microsoft.com/en-us/library/dd374133.aspx
  void OnComposition();

  // Should be called when the IME UI window receives WM_IME_ENDCOMPOSITION
  // message.
  // http://msdn.microsoft.com/en-us/library/dd374136.aspx
  void OnEndComposition();

  // Should be called when the IME UI window receives WM_IME_SETCONTEXT
  // message.
  // http://msdn.microsoft.com/en-us/library/dd374142.aspx
  void OnSetContext(const ShowUIAttributes &show_ui_attributes);

  // Returns true if there exists any visible window.
  bool IsAnyWindowVisible() const;

  // Returns true if the candidate window is visible.
  bool IsCandidateWindowVisible() const;

  // Returns true if the suggest window is visible.
  bool IsSuggestWindowVisible() const;

  // Returns true if the composition window is visible.
  bool IsCompositionWindowVisible() const;

  bool ui_activated() const;
  bool candidate_window_activated() const;
  bool suggest_window_activated() const;
  bool composition_window_activated() const;

 private:
  bool ui_activated_;
  bool candidate_window_activated_;
  bool suggest_window_activated_;
  bool composition_window_activated_;

  DISALLOW_COPY_AND_ASSIGN(UIVisibilityTracker);
};
}  // namespace win32
}  // namespace mozc

#endif  // MOZC_WIN32_IME_IME_UI_VISIBILITY_TRACKER_H_
