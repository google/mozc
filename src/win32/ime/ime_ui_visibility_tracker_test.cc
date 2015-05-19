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

#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "win32/ime/ime_types.h"
#include "win32/ime/ime_ui_visibility_tracker.h"

namespace mozc {
namespace win32 {
TEST(ImeUIVisibilityTrackerTest, BasicTest) {
  UIVisibilityTracker tracker;

  tracker.Initialize();

  // At first, no window is visible because IME does not have input focus.
  EXPECT_FALSE(tracker.IsAnyWindowVisible());

  // Notify the tracker that the IME is getting input focus.
  tracker.OnFocus();

  // Suggest window is visible by default when the IME has input focus.
  EXPECT_TRUE(tracker.IsSuggestWindowVisible());

  // Candidate window is not visible by default when the IME has input focus.
  EXPECT_FALSE(tracker.IsCandidateWindowVisible());

  // Composition window is not visible by default when the IME has input focus.
  EXPECT_FALSE(tracker.IsCompositionWindowVisible());

  // Notify the tracker of the UI visibility bits passed from the application
  // as lParam of WM_IME_SETCONTEXT.  These bits update the visibility for
  // each UI component.
  // ISC_SHOWUIALL means that the application allows the IME to show all the UI
  // components.
  const ShowUIAttributes ui_attributes(ISC_SHOWUIALL);
  tracker.OnSetContext(ui_attributes);

  // WM_IME_SETCONTEXT/ISC_SHOWUIALL do not change the UI visibilities.
  EXPECT_TRUE(tracker.IsSuggestWindowVisible());
  EXPECT_FALSE(tracker.IsCandidateWindowVisible());
  EXPECT_FALSE(tracker.IsCompositionWindowVisible());
}

// When a user changes the input method by the LangBar, WM_IME_SETCONTEXT will
// not be sent.  Even in this case, the suggest window should be visible by
// default.
TEST(ImeUIVisibilityTrackerTest,
     SuggestWindowShouldBeVisibleWhenImeIsChangedByLangBar) {
  UIVisibilityTracker tracker;

  tracker.Initialize();

  // At first, no window is visible because IME does not have input focus.
  EXPECT_FALSE(tracker.IsAnyWindowVisible());

  // Even when the input context has already got focus, |OnFocus| will be
  // called from ImeSelect.
  tracker.OnFocus();

  // Suggest window is visible just after the IME is changed by the LangBar.
  EXPECT_TRUE(tracker.IsSuggestWindowVisible());
}

// When a user changes the input method by the LangBar, WM_IME_SETCONTEXT will
// not be sent.  Even in this case, the composition window can be visible,
// without any focus change, which finally invokes ImeSetActiveContext.
TEST(ImeUIVisibilityTrackerTest,
     CompositionWindowCanBeShownWhenImeIsChangedByLangBar) {
  UIVisibilityTracker tracker;

  tracker.Initialize();

  // At first, no window is visible because IME does not have input focus.
  EXPECT_FALSE(tracker.IsAnyWindowVisible());

  // Even when the input context has already got focus, |OnFocus| will be
  // called from ImeSelect.
  tracker.OnFocus();

  // Composition window is not visible by default after the IME is changed by
  // the LangBar.
  EXPECT_FALSE(tracker.IsCompositionWindowVisible());

  // WM_IME_STARTCOMPOSITION should be marked as a visibility-test-message by
  // BeginVisibilityTestForCompositionWindow.
  EXPECT_TRUE(
      UIVisibilityTracker::IsVisibilityTestMessageForComposiwionWindow(
          WM_IME_STARTCOMPOSITION, 0, 0));

  // Since WM_IME_STARTCOMPOSITION is a visibility-test-message, the IME calls
  // BeginVisibilityTestForCompositionWindow the message is posted.
  tracker.BeginVisibilityTestForCompositionWindow();

  // If WM_IME_STARTCOMPOSITION is passed to the IME UI Window, the IME is
  // responsible to draw the composition window.
  tracker.OnStartComposition();
  EXPECT_TRUE(tracker.IsCompositionWindowVisible());
}


// When a user changes the input method by the LangBar, WM_IME_SETCONTEXT will
// not be sent.  Even in this case, the candiate window can be visible,
// without any focus change, which finally invokes ImeSetActiveContext.
TEST(ImeUIVisibilityTrackerTest,
     CandidateWindowCanBeShownWhenImeIsChangedByLangBar) {
  UIVisibilityTracker tracker;

  tracker.Initialize();

  // At first, no window is visible because IME does not have input focus.
  EXPECT_FALSE(tracker.IsAnyWindowVisible());

  // Even when the input context has already got focus, |OnFocus| will be
  // called from ImeSelect.
  tracker.OnFocus();

  // Candidate window is not visible by default after the IME is changed by the
  // LangBar.
  EXPECT_FALSE(tracker.IsCandidateWindowVisible());

  // Since WM_IME_NOTIFY/IMN_OPENCANDIDATE is a visibility-test-message, the
  // IME calls BeginVisibilityTestForCandidateWindow the message is
  // posted.
  tracker.BeginVisibilityTestForCandidateWindow();

  // BeginVisibilityTestForCandidateWindow changes the visibility bit for
  // candidate window.  However, it does not change the visibility bit for
  // suggestion window.
  EXPECT_FALSE(tracker.IsCandidateWindowVisible());

  // If WM_IME_NOTIFY/IMN_OPENCANDIDATE is passed to the IME UI Window, the IME
  // is responsible to draw the candidate window.
  tracker.OnNotify(IMN_OPENCANDIDATE, 1);
  EXPECT_TRUE(tracker.IsCandidateWindowVisible());
}

TEST(ImeUIVisibilityTrackerTest, HideSuggestWindowBySetInputContext) {
  UIVisibilityTracker tracker;

  tracker.Initialize();

  // At first, no window is visible because IME does not have input focus.
  EXPECT_FALSE(tracker.IsAnyWindowVisible());

  // Notify the tracker that the IME is getting input focus.
  tracker.OnFocus();

  // Suggest window is visible by default when the IME has input focus.
  EXPECT_TRUE(tracker.IsSuggestWindowVisible());

  // Candidate window is not visible by default when the IME has input focus.
  EXPECT_FALSE(tracker.IsCandidateWindowVisible());

  // Composition window is not visible by default when the IME has input focus.
  EXPECT_FALSE(tracker.IsCompositionWindowVisible());

  // Suggest window should be visible if all the candidate windows are visible.
  // You may see this situation in Chrome, as reported in b/3002445.
  tracker.OnSetContext(ShowUIAttributes(ISC_SHOWUIALLCANDIDATEWINDOW));

  // Suggest window should be visible.
  EXPECT_TRUE(tracker.IsSuggestWindowVisible());

  // Suggest window should be invisible if none of the bits in
  // ISC_SHOWUIALLCANDIDATEWINDOW is cleared, where ISC_SHOWUICANDIDATEWINDOW
  // means that the first candidate window can be shown but the others must be
  // invisible.
  tracker.OnSetContext(ShowUIAttributes(ISC_SHOWUICANDIDATEWINDOW));

  // Suggest window should be invisible.
  EXPECT_FALSE(tracker.IsSuggestWindowVisible());
}


TEST(ImeUIVisibilityTrackerTest, CompositionIsDrawnByIME) {
  UIVisibilityTracker tracker;

  tracker.Initialize();

  // Notify the tracker that the IME is getting input focus.
  tracker.OnFocus();

  // Notify the tracker of the UI visibility bits passed from the application
  // as lParam of WM_IME_SETCONTEXT.  These bits update the visibility for
  // each UI component.
  const ShowUIAttributes ui_attributes(ISC_SHOWUIALL);
  tracker.OnSetContext(ui_attributes);

  // WM_IME_STARTCOMPOSITION should be marked as a visibility-test-message by
  // BeginVisibilityTestForCompositionWindow.
  EXPECT_TRUE(
      UIVisibilityTracker::IsVisibilityTestMessageForComposiwionWindow(
          WM_IME_STARTCOMPOSITION, 0, 0));

  // Since WM_IME_STARTCOMPOSITION is a visibility-test-message, the IME calls
  // BeginVisibilityTestForCompositionWindow the message is posted.
  tracker.BeginVisibilityTestForCompositionWindow();

  // BeginVisibilityTestForCompositionWindow changes the visibility bit for
  // composition window.
  EXPECT_FALSE(tracker.IsCompositionWindowVisible());

  // If WM_IME_STARTCOMPOSITION is passed to the IME UI Window, the IME is
  // responsible to draw the composition window.
  tracker.OnStartComposition();
  EXPECT_TRUE(tracker.IsCompositionWindowVisible());

  // UIVisibilityTracker ignores these bits though.
  const DWORD kCompositionUpdateBits =
      (GCS_COMPREADSTR | GCS_COMPREADATTR | GCS_COMPREADCLAUSE | GCS_COMPSTR |
       GCS_COMPATTR | GCS_COMPCLAUSE | GCS_CURSORPOS | GCS_DELTASTART);

  // WM_IME_COMPOSITION should be marked as a visibility-test-message by
  // BeginVisibilityTestForCompositionWindow.
  EXPECT_TRUE(
      UIVisibilityTracker::IsVisibilityTestMessageForComposiwionWindow(
          WM_IME_COMPOSITION, 0, kCompositionUpdateBits));

  // Since WM_IME_COMPOSITION is a visibility-test-message, the IME calls
  // BeginVisibilityTestForCompositionWindow the message is posted.
  tracker.BeginVisibilityTestForCompositionWindow();

  // BeginVisibilityTestForCompositionWindow changes the visibility bit for
  // composition window.
  EXPECT_FALSE(tracker.IsCompositionWindowVisible());

  // If the IME UI Window receives WM_IME_COMPOSITION, the IME is responsible
  // to draw the composition window.
  tracker.OnComposition();
  EXPECT_TRUE(tracker.IsCompositionWindowVisible());

  // WM_IME_ENDCOMPOSITION should be marked as a visibility-test-message by
  // BeginVisibilityTestForCompositionWindow.
  EXPECT_TRUE(
      UIVisibilityTracker::IsVisibilityTestMessageForComposiwionWindow(
          WM_IME_ENDCOMPOSITION, 0, 0));

  // Since WM_IME_ENDCOMPOSITION is a visibility-test-message, the IME calls
  // BeginVisibilityTestForCompositionWindow the message is posted.
  tracker.BeginVisibilityTestForCompositionWindow();

  // BeginVisibilityTestForCompositionWindow changes the visibility bit for
  // composition window.
  EXPECT_FALSE(tracker.IsCompositionWindowVisible());

  // WM_IME_ENDCOMPOSITION makes the composition window invisible either way.
  tracker.OnEndComposition();
  EXPECT_FALSE(tracker.IsCompositionWindowVisible());
}

TEST(ImeUIVisibilityTrackerTest, CompositionIsDrawnByApplication) {
  UIVisibilityTracker tracker;

  tracker.Initialize();

  // Notify the tracker that the IME is getting input focus.
  tracker.OnFocus();

  // Notify the tracker of the UI visibility bits passed from the application
  // as lParam of WM_IME_SETCONTEXT.  These bits update the visibility for
  // each UI component.
  // Some applications such as GTK applications or OOo does not clear the UI
  // visibility bits even though they draw their own composition windows.
  // We conform with those applications.
  const ShowUIAttributes ui_attributes(ISC_SHOWUIALL);
  tracker.OnSetContext(ui_attributes);

  // WM_IME_STARTCOMPOSITION should be marked as a visibility-test-message by
  // BeginVisibilityTestForCompositionWindow.
  EXPECT_TRUE(
      UIVisibilityTracker::IsVisibilityTestMessageForComposiwionWindow(
          WM_IME_STARTCOMPOSITION, 0, 0));

  // Since WM_IME_STARTCOMPOSITION is a visibility-test-message, the IME calls
  // BeginVisibilityTestForCompositionWindow the message is posted.
  tracker.BeginVisibilityTestForCompositionWindow();

  // BeginVisibilityTestForCompositionWindow changes the visibility bit for
  // composition window.
  // If WM_IME_STARTCOMPOSITION is not passed to the IME UI Window, the
  // application is responsible to draw the composition window.
  EXPECT_FALSE(tracker.IsCompositionWindowVisible());

  // UIVisibilityTracker ignores these bits though.
  const DWORD kCompositionUpdateBits =
      (GCS_COMPREADSTR | GCS_COMPREADATTR | GCS_COMPREADCLAUSE | GCS_COMPSTR |
       GCS_COMPATTR | GCS_COMPCLAUSE | GCS_CURSORPOS | GCS_DELTASTART);

  // WM_IME_COMPOSITION should be marked as a visibility-test-message by
  // BeginVisibilityTestForCompositionWindow.
  EXPECT_TRUE(
      UIVisibilityTracker::IsVisibilityTestMessageForComposiwionWindow(
          WM_IME_COMPOSITION, 0, kCompositionUpdateBits));

  // Since WM_IME_COMPOSITION is a visibility-test-message, the IME calls
  // BeginVisibilityTestForCompositionWindow the message is posted.
  tracker.BeginVisibilityTestForCompositionWindow();

  // BeginVisibilityTestForCompositionWindow changes the visibility bit for
  // composition window.
  // If WM_IME_COMPOSITION is not passed to the IME UI Window, the application
  // is responsible to draw the composition window.
  EXPECT_FALSE(tracker.IsCompositionWindowVisible());

  // WM_IME_ENDCOMPOSITION should be marked as a visibility-test-message by
  // BeginVisibilityTestForCompositionWindow.
  EXPECT_TRUE(
      UIVisibilityTracker::IsVisibilityTestMessageForComposiwionWindow(
          WM_IME_ENDCOMPOSITION, 0, 0));

  // Since WM_IME_ENDCOMPOSITION is a visibility-test-message, the IME calls
  // BeginVisibilityTestForCompositionWindow the message is posted.
  tracker.BeginVisibilityTestForCompositionWindow();

  // BeginVisibilityTestForCompositionWindow changes the visibility bit for
  // composition window.
  // WM_IME_ENDCOMPOSITION makes the composition window invisible either way.
  EXPECT_FALSE(tracker.IsCompositionWindowVisible());
}

TEST(ImeUIVisibilityTrackerTest, CandidateIsDrawnByIME) {
  UIVisibilityTracker tracker;

  tracker.Initialize();

  // Notify the tracker that the IME is getting input focus.
  tracker.OnFocus();

  // Notify the tracker of the UI visibility bits passed from the application
  // as lParam of WM_IME_SETCONTEXT.  These bits update the visibility for
  // each UI component.
  const ShowUIAttributes ui_attributes(ISC_SHOWUIALL);
  tracker.OnSetContext(ui_attributes);

  // IMN_OPENCANDIDATE should be marked as a visibility-test-message by
  // IsVisibilityTestMessageForCandidateWindow.
  EXPECT_TRUE(
      UIVisibilityTracker::IsVisibilityTestMessageForCandidateWindow(
          WM_IME_NOTIFY, IMN_OPENCANDIDATE, 1));

  // Since WM_IME_NOTIFY/IMN_OPENCANDIDATE is a visibility-test-message, the
  // IME calls BeginVisibilityTestForCandidateWindow the message is
  // posted.
  tracker.BeginVisibilityTestForCandidateWindow();

  // BeginVisibilityTestForCandidateWindow changes the visibility bit for
  // candidate window.  However, it does not change the visibility bit for
  // suggestion window.
  EXPECT_FALSE(tracker.IsCandidateWindowVisible());
  EXPECT_TRUE(tracker.IsSuggestWindowVisible());

  // If WM_IME_NOTIFY/IMN_OPENCANDIDATE is passed to the IME UI Window, the IME
  // is responsible to draw the candidate window.
  tracker.OnNotify(IMN_OPENCANDIDATE, 1);
  EXPECT_TRUE(tracker.IsCandidateWindowVisible());

  // WM_IME_NOTIFY/IMN_CHANGECANDIDATE is not marked as a visibility-test-
  // message by IsVisibilityTestMessageForCandidateWindow.
  EXPECT_FALSE(
      UIVisibilityTracker::IsVisibilityTestMessageForCandidateWindow(
          WM_IME_NOTIFY, IMN_CHANGECANDIDATE, 1));

  // WM_IME_NOTIFY/IMN_CLOSECANDIDATE is not marked as a visibility-test-
  // message by IsVisibilityTestMessageForCandidateWindow.
  EXPECT_TRUE(
      UIVisibilityTracker::IsVisibilityTestMessageForCandidateWindow(
          WM_IME_NOTIFY, IMN_CLOSECANDIDATE, 1));

  // Since WM_IME_NOTIFY/IMN_CLOSECANDIDATE is a visibility-test-message, the
  // IME calls BeginVisibilityTestForCandidateWindow the message is
  // posted.
  tracker.BeginVisibilityTestForCandidateWindow();

  // BeginVisibilityTestForCandidateWindow changes the visibility bit for
  // candidate window.
  EXPECT_FALSE(tracker.IsCandidateWindowVisible());

  // WM_IME_NOTIFY/IMN_CLOSECANDIDATE makes the candidate window invisible
  // either way.
  tracker.OnNotify(IMN_CLOSECANDIDATE, 1);
  EXPECT_FALSE(tracker.IsCandidateWindowVisible());
}

TEST(ImeUIVisibilityTrackerTest, CandidateIsDrawnByApplication) {
  UIVisibilityTracker tracker;

  tracker.Initialize();

  // Notify the tracker that the IME is getting input focus.
  tracker.OnFocus();

  // Notify the tracker of the UI visibility bits passed from the application
  // as lParam of WM_IME_SETCONTEXT.  These bits update the visibility for
  // each UI component.
  // Some game applications might not clear the UI visibility bits even though
  // they draw their own candidate windows.
  // We conform with those applications.
  const ShowUIAttributes ui_attributes(ISC_SHOWUIALL);
  tracker.OnSetContext(ui_attributes);

  // IMN_OPENCANDIDATE should be marked as a visibility-test-message by
  // IsVisibilityTestMessageForCandidateWindow.
  EXPECT_TRUE(
      UIVisibilityTracker::IsVisibilityTestMessageForCandidateWindow(
          WM_IME_NOTIFY, IMN_OPENCANDIDATE, 1));

  // Since WM_IME_NOTIFY/IMN_OPENCANDIDATE is a visibility-test-message, the
  // IME calls BeginVisibilityTestForCandidateWindow before the message is
  // posted.
  tracker.BeginVisibilityTestForCandidateWindow();

  // BeginVisibilityTestForCandidateWindow changes the visibility bit for
  // candidate window.  However, it does not change the visibility bit for
  // suggestion window.
  // If WM_IME_NOTIFY/IMN_OPENCANDIDATE is not passed to the IME UI Window, the
  // application is responsible to draw the candidate window.
  EXPECT_FALSE(tracker.IsCandidateWindowVisible());
  EXPECT_TRUE(tracker.IsSuggestWindowVisible());

  // WM_IME_NOTIFY/IMN_CHANGECANDIDATE is not marked as a visibility-test-
  // message by IsVisibilityTestMessageForCandidateWindow.
  EXPECT_FALSE(
      UIVisibilityTracker::IsVisibilityTestMessageForCandidateWindow(
          WM_IME_NOTIFY, IMN_CHANGECANDIDATE, 1));

  // WM_IME_NOTIFY/IMN_CLOSECANDIDATE is not marked as a visibility-test-
  // message by IsVisibilityTestMessageForCandidateWindow.
  EXPECT_TRUE(
      UIVisibilityTracker::IsVisibilityTestMessageForCandidateWindow(
          WM_IME_NOTIFY, IMN_CLOSECANDIDATE, 1));

  // Since WM_IME_NOTIFY/IMN_CLOSECANDIDATE is a visibility-test-message, the
  // IME calls BeginVisibilityTestForCandidateWindow before the message is
  // posted.
  // WM_IME_NOTIFY/IMN_CLOSECANDIDATE makes the candidate window invisible
  // either way.
  tracker.BeginVisibilityTestForCandidateWindow();

  EXPECT_FALSE(tracker.IsCandidateWindowVisible());
}

// Some applications such as gvim 7.3.55, Notepad++ 5.8.4 (Scintilla 2.22),
// EmEditor 10.0.4, do not pass WM_IME_COMPOSITION message to ::DefWindowProc
// when |lParam| contains GCS_RESULTSTR flag. (b/3223935)
TEST(ImeUIVisibilityTrackerTest,
     Issue3223935_WM_IME_COMPOSITION_IsEatenIfItContainsResultString) {
  UIVisibilityTracker tracker;

  tracker.Initialize();

  // Notify the tracker that the IME is getting input focus.
  tracker.OnFocus();

  // Notify the tracker of the UI visibility bits passed from the application
  // as lParam of WM_IME_SETCONTEXT.  These bits update the visibility for
  // each UI component.
  // Some game applications might not clear the UI visibility bits even though
  // they draw their own candidate windows.
  // We conform with those applications.
  const ShowUIAttributes ui_attributes(ISC_SHOWUIALL);
  tracker.OnSetContext(ui_attributes);

  // WM_IME_STARTCOMPOSITION should be marked as a visibility-test-message by
  // BeginVisibilityTestForCompositionWindow.
  EXPECT_TRUE(
      UIVisibilityTracker::IsVisibilityTestMessageForComposiwionWindow(
          WM_IME_STARTCOMPOSITION, 0, 0));

  // Since WM_IME_STARTCOMPOSITION is a visibility-test-message, the IME calls
  // BeginVisibilityTestForCompositionWindow the message is posted.
  tracker.BeginVisibilityTestForCompositionWindow();

  // If WM_IME_STARTCOMPOSITION is passed to the IME UI Window, the IME is
  // responsible to draw the composition window.
  tracker.OnStartComposition();
  EXPECT_TRUE(tracker.IsCompositionWindowVisible());

  // |lParam| contains GCS_RESULTSTR.  Do not use WM_IME_COMPOSITION as a
  // visibility-test-message in this case.
  EXPECT_FALSE(
      UIVisibilityTracker::IsVisibilityTestMessageForCandidateWindow(
          WM_IME_COMPOSITION, 0, GCS_RESULTSTR));

  // |lParam| contains GCS_RESULTSTR.  Do not use WM_IME_COMPOSITION as a
  // visibility-test-message in this case.
  EXPECT_FALSE(
      UIVisibilityTracker::IsVisibilityTestMessageForCandidateWindow(
          WM_IME_COMPOSITION, 0, GCS_COMPSTR | GCS_RESULTSTR));

  // Composition Window should be visible.
  EXPECT_TRUE(tracker.IsCompositionWindowVisible());
}
}  // namespace win32
}  // namespace mozc
