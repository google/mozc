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

package org.mozc.android.inputmethod.japanese.model;

import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.DeletionRange;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Preedit;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Preedit.Segment;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Preedit.Segment.Annotation;
import com.google.common.base.Strings;

import junit.framework.AssertionFailedError;
import junit.framework.TestCase;

import java.util.Arrays;

/**
 */
public class SelectionTrackerTest extends TestCase {
  private static Segment createSegment(String text) {
    return Segment.newBuilder()
        .setAnnotation(Annotation.UNDERLINE)
        .setValue(text)
        .buildPartial();
  }

  private static Segment createHighlightSegment(String text) {
    return Segment.newBuilder()
        .setAnnotation(Annotation.HIGHLIGHT)
        .setValue(text)
        .buildPartial();
  }

  private static Preedit createPreedit(int cursor, Segment... segments) {
    return Preedit.newBuilder()
        .setCursor(cursor)
        .addAllSegment(Arrays.asList(segments))
        .buildPartial();
  }

  // Currently, mozc server produces "offset == -length" DeletionRange instances only,
  // for undo and calculator rewriter.
  private static DeletionRange createDeletionRange(int length) {
    return DeletionRange.newBuilder()
        .setOffset(-length)
        .setLength(length)
        .build();
  }

  private static void assertTracker(
      int preeditStartPosition, int lastSelectionStart, int lastSelectionEnd,
      SelectionTracker tracker) {
    assertEquals(preeditStartPosition, tracker.getPreeditStartPosition());
    assertEquals(lastSelectionStart, tracker.getLastSelectionStart());
    assertEquals(lastSelectionEnd, tracker.getLastSelectionEnd());
  }

  public void testTextViewEmulation() {
    // This is the test for the simplest cases.
    // Tested on Xperia/Galaxy S/Galaxy Nexus share the same behavior.
    SelectionTracker tracker = new SelectionTracker();

    // Initialize: ""
    tracker.onStartInput(0, 0, false);
    assertTracker(0, 0, 0, tracker);

    // Type "あ": "[あ]|"
    tracker.onRender(null, null, createPreedit(1, createSegment("あ")));
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(0, 0, 1, 1, 0, 1));
    assertTracker(0, 1, 1, tracker);

    // Type "い": "[あい]|"
    tracker.onRender(null, null, createPreedit(2, createSegment("あい")));
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(1, 1, 2, 2, 0, 2));
    assertTracker(0, 2, 2, tracker);

    // Type "う": "[あいう]|"
    tracker.onRender(null, null, createPreedit(3, createSegment("あいう")));
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(2, 2, 3, 3, 0, 3));
    assertTracker(0, 3, 3, tracker);

    // Hit space: "[あ]_いう_|"
    tracker.onRender(
        null, null, createPreedit(3, createHighlightSegment("あ"), createSegment("いう")));
    assertTracker(0, 3, 3, tracker);

    // Type Right: "[愛]_う_|"
    tracker.onRender(
        null, null, createPreedit(2, createHighlightSegment("愛"), createSegment("う")));
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(3, 3, 2, 2, 0, 2));
    assertTracker(0, 2, 2, tracker);

    // Type Left: "[あ]_いう_|"
    tracker.onRender(
        null, null, createPreedit(3, createHighlightSegment("あ"), createSegment("いう")));
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(2, 2, 3, 3, 0, 3));
    assertTracker(0, 3, 3, tracker);

    // Commit あ: "あ[いう]|"
    tracker.onRender(null, "あ", createPreedit(2, createSegment("いう")));
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(3, 3, 3, 3, 1, 3));
    assertTracker(1, 3, 3, tracker);

    // Commit いう: "あいう|"
    tracker.onRender(null, "いう", null);
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(3, 3, 3, 3, -1, -1));
    assertTracker(3, 3, 3, tracker);

    // Tap between い and う: "あい|う"
    assertEquals(SelectionTracker.RESET_CONTEXT, tracker.onUpdateSelection(3, 3, 2, 2, -1, -1));
    assertTracker(2, 2, 2, tracker);

    // Type お: "あい[お]|う"
    tracker.onRender(null, null, createPreedit(1, createSegment("お")));
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(2, 2, 3, 3, 2, 3));
    assertTracker(2, 3, 3, tracker);

    // Type か: "あい[おか]|う"
    tracker.onRender(null, null, createPreedit(2, createSegment("おか")));
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(3, 3, 4, 4, 2, 4));
    assertTracker(2, 4, 4, tracker);

    // Type Left: "あい[お]|_か_う"
    tracker.onRender(null, null, createPreedit(1, createSegment("おか")));
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(4, 4, 3, 3, 2, 4));
    assertTracker(2, 3, 3, tracker);

    // Type Right: "あい[おか]|う"
    tracker.onRender(null, null, createPreedit(2, createSegment("おか")));
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(3, 3, 4, 4, 2, 4));
    assertTracker(2, 4, 4, tracker);

    // Tap between お and か: "あい[お]|_か_う"
    assertEquals(1, tracker.onUpdateSelection(4, 4, 3, 3, 2, 4));
    tracker.onRender(null, null, createPreedit(1, createSegment("おか")));
    assertTracker(2, 3, 3, tracker);

    // Type き: "あい[おき]|_か_う"
    tracker.onRender(null, null, createPreedit(2, createSegment("おきか")));
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(3, 3, 4, 4, 2, 5));
    assertTracker(2, 4, 4, tracker);

    // Type Enter.: "あいおきか|う"
    tracker.onRender(null, "おきか", null);
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(4, 4, 5, 5, -1, -1));
    assertTracker(5, 5, 5, tracker);

    // Type Undo.: "あい[おき]|_か_う"
    tracker.onRender(createDeletionRange(3), null, createPreedit(2, createSegment("おきか")));
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(5, 5, 4, 4, 2, 5));
    assertTracker(2, 4, 4, tracker);

    // Commit again.: "あいおきか|う"
    tracker.onRender(null, "おきか", null);
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(4, 4, 5, 5, -1, -1));
    assertTracker(5, 5, 5, tracker);

    // Hide the keyboard.
    tracker.onWindowHidden();

    // Show again and type さ: "あいおきか[さ]|う"
    tracker.onRender(null, null, createPreedit(1, createSegment("さ")));
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(5, 5, 6, 6, 5, 6));
    assertTracker(5, 6, 6, tracker);

    // Close the activity by home button.
    tracker.onWindowHidden();
    tracker.onStartInput(-1, -1, false);

    // Then re-activate the activity.
    tracker.onStartInput(6, 6, false);

    // Type た: "あいおきかさ[た]|う"
    tracker.onRender(null, null, createPreedit(1, createSegment("た")));
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(6, 6, 7, 7, 6, 7));
    assertTracker(6, 7, 7, tracker);
  }

  public void testBrowserOnAndroid2_1Emulation() {
    // This test case is make sure that this works well with android 2.1 defualt browser.
    // Extracted expected message by using google.co.jp.
    // The main difference between the expected case is that the caret position seems not to
    // be updated during the conversion.
    SelectionTracker tracker = new SelectionTracker();

    // Initialize. Not very sure, but the onStartInput invoked three times.
    tracker.onStartInput(0, 0, true);
    tracker.onStartInput(0, 0, true);
    tracker.onStartInput(0, 0, true);
    assertTracker(0, 0, 0, tracker);

    // Type "あ": "[あ]|"
    // onUpdateSelection is not invoked in most just type case.
    tracker.onRender(null, null, createPreedit(1, createSegment("あ")));
    assertTracker(0, 1, 1, tracker);

    // Type "い": "[あい]|"
    tracker.onRender(null, null, createPreedit(2, createSegment("あい")));
    assertTracker(0, 2, 2, tracker);

    // Type "う": "[あいう]|"
    tracker.onRender(null, null, createPreedit(3, createSegment("あいう")));
    assertTracker(0, 3, 3, tracker);

    // Hit space: "[あ]_いう_|"
    tracker.onRender(
        null, null, createPreedit(3, createHighlightSegment("あ"), createSegment("いう")));
    assertTracker(0, 3, 3, tracker);

    // Type Right: "[愛]_う_|"
    tracker.onRender(
        null, null, createPreedit(2, createHighlightSegment("愛"), createSegment("う")));
    assertTracker(0, 2, 2, tracker);

    // Type Left: "[あ]_いう_|"
    tracker.onRender(
        null, null, createPreedit(3, createHighlightSegment("あ"), createSegment("いう")));
    assertTracker(0, 3, 3, tracker);

    // Commit あ: "あ[いう]|"
    tracker.onRender(null, "あ", createPreedit(2, createSegment("いう")));
    assertTracker(1, 3, 3, tracker);

    // Commit いう: "あいう|"
    tracker.onRender(null, "いう", null);
    assertTracker(3, 3, 3, tracker);

    // Tap between い and う: "あい|う"
    // The message has _wrong_ oldSel{Start,End}...
    // Looks like the caret position in the browser is remained at the beginning of the field.
    assertEquals(SelectionTracker.RESET_CONTEXT, tracker.onUpdateSelection(0, 0, 2, 2, -1, -1));
    assertTracker(2, 2, 2, tracker);

    // Type お: "あい[お]|う"
    tracker.onRender(null, null, createPreedit(1, createSegment("お")));
    assertTracker(2, 3, 3, tracker);

    // Type か: "あい[おか]|う"
    tracker.onRender(null, null, createPreedit(2, createSegment("おか")));
    assertTracker(2, 4, 4, tracker);

    // Type Left: "あい[お]|_か_う"
    tracker.onRender(null, null, createPreedit(1, createSegment("おか")));
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(2, 2, 3, 3, 2, 4));
    assertTracker(2, 3, 3, tracker);

    // Type Right: "あい[おか]|う"
    // The candidates{Start,End} are wrongly set to (-1, -1).
    tracker.onRender(null, null, createPreedit(2, createSegment("おか")));
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(3, 3, 4, 4, -1, -1));
    assertTracker(2, 4, 4, tracker);

    // Tap between お and か: "あい[お]|_か_う"
    assertEquals(1, tracker.onUpdateSelection(4, 4, 3, 3, 2, 4));
    tracker.onRender(null, null, createPreedit(1, createSegment("おか")));
    assertTracker(2, 3, 3, tracker);

    // Type き: "あい[おき]|_か_う"
    tracker.onRender(null, null, createPreedit(2, createSegment("おかき")));
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(3, 3, 4, 4, -1, -1));
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(4, 4, 4, 4, 2, 5));
    assertTracker(2, 4, 4, tracker);

    // Type Enter.: "あいおきか|う"
    tracker.onRender(null, "おきか", null);
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(4, 4, 5, 5, -1, -1));
    assertTracker(5, 5, 5, tracker);

    // Type Undo.: "あい[おき]|_か_う"
    tracker.onRender(createDeletionRange(3), null, createPreedit(2, createSegment("おかき")));
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(5, 5, 4, 4, 2, 5));
    assertTracker(2, 4, 4, tracker);

    // Commit again.: "あいおきか|う"
    tracker.onRender(null, "おきか", null);
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(4, 4, 5, 5, -1, -1));
    assertTracker(5, 5, 5, tracker);

    // Hide the keyboard.
    tracker.onWindowHidden();

    // Show again and type さ: "あいおきか[さ]|う"
    tracker.onRender(null, null, createPreedit(1, createSegment("さ")));
    assertTracker(5, 6, 6, tracker);

    // Close the activity by home button.
    tracker.onStartInput(-1, -1, false);
    tracker.onWindowHidden();

    // Then re-activate the activity.
    tracker.onStartInput(6, 6, true);
    assertTracker(6, 6, 6, tracker);

    // Type た: "あいおきかさ[た]|う"
    tracker.onRender(null, null, createPreedit(1, createSegment("た")));
    assertTracker(6, 7, 7, tracker);

    // Unfocus from the text field.
    tracker.onWindowHidden();
    tracker.onStartInput(-1, -1, false);

    // Move to previous page and back to the current page, to clear the field.
    // Then tap again the search box.
    tracker.onStartInput(8, 8, true);
    tracker.onStartInput(8, 8, true);
    tracker.onStartInput(8, 8, true);
    tracker.onUpdateSelection(8, 8, 0, 0, -1, -1);
    assertTracker(0, 0, 0, tracker);
  }

  public void testBug5647852OnAndroid2_1() {
    SelectionTracker tracker = new SelectionTracker();

    tracker.onStartInput(0, 0, true);
    tracker.onStartInput(0, 0, true);
    tracker.onStartInput(0, 0, true);
    assertTracker(0, 0, 0, tracker);

    // Send "だいがく"
    tracker.onRender(null, null, createPreedit(1, createSegment("た")));
    assertTracker(0, 1, 1, tracker);

    tracker.onRender(null, null, createPreedit(1, createSegment("だ")));
    assertTracker(0, 1, 1, tracker);

    tracker.onRender(null, null, createPreedit(2, createSegment("だい")));
    assertTracker(0, 2, 2, tracker);

    tracker.onRender(null, null, createPreedit(3, createSegment("だいか")));
    assertTracker(0, 3, 3, tracker);

    tracker.onRender(null, null, createPreedit(3, createSegment("だいが")));
    assertTracker(0, 3, 3, tracker);

    tracker.onRender(null, null, createPreedit(4, createSegment("だいがく")));
    assertTracker(0, 4, 4, tracker);

    // Tap LEFT.
    tracker.onRender(null, null, createPreedit(3, createSegment("だいがく")));
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(0, 0, 3, 3, 0, 4));
    assertTracker(0, 3, 3, tracker);

    // Tap LEFT again.
    tracker.onRender(null, null, createPreedit(2, createSegment("だいがく")));
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(3, 3, 4, 4, -1, -1));
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(4, 4, 2, 2, 0, 4));
    assertTracker(0, 2, 2, tracker);
  }

  public void testBrowserOnAndroid2_3Emulation() {
    SelectionTracker tracker = new SelectionTracker();

    // Initialize. The browser says it is not a WebTextView.
    tracker.onStartInput(0, 0, false);

    // Type "あ": "[あ]|"
    // onUpdateSelection is not invoked in most just type case.
    tracker.onRender(null, null, createPreedit(1, createSegment("あ")));
    assertTracker(0, 1, 1, tracker);

    // Type "い": "[あい]|"
    tracker.onRender(null, null, createPreedit(2, createSegment("あい")));
    assertTracker(0, 2, 2, tracker);

    // Type "う": "[あいう]|"
    tracker.onRender(null, null, createPreedit(3, createSegment("あいう")));
    assertTracker(0, 3, 3, tracker);

    // Hit space: "[あ]_いう_|"
    tracker.onRender(
        null, null, createPreedit(3, createHighlightSegment("あ"), createSegment("いう")));
    assertTracker(0, 3, 3, tracker);

    // Type Right: "[愛]_う_|"
    tracker.onRender(
        null, null, createPreedit(2, createHighlightSegment("愛"), createSegment("う")));
    assertTracker(0, 2, 2, tracker);

    // Type Left: "[あ]_いう_|"
    tracker.onRender(
        null, null, createPreedit(3, createHighlightSegment("あ"), createSegment("いう")));
    assertTracker(0, 3, 3, tracker);

    // Commit あ: "あ[いう]|"
    tracker.onRender(null, "あ", createPreedit(2, createSegment("いう")));
    assertTracker(1, 3, 3, tracker);

    // Commit いう: "あいう|"
    tracker.onRender(null, "いう", null);
    assertTracker(3, 3, 3, tracker);

    // Tap between い and う: "あい|う"
    // The message has _wrong_ oldSel{Start,End}...
    // Looks like the caret position in the browser is remained at the beginning of the field.
    assertEquals(SelectionTracker.RESET_CONTEXT, tracker.onUpdateSelection(0, 0, 2, 2, -1, -1));
    assertTracker(2, 2, 2, tracker);

    // Type お: "あい[お]|う"
    tracker.onRender(null, null, createPreedit(1, createSegment("お")));
    assertTracker(2, 3, 3, tracker);

    // Type か: "あい[おか]|う"
    tracker.onRender(null, null, createPreedit(2, createSegment("おか")));
    assertTracker(2, 4, 4, tracker);

    // Type Left: "あい[お]|_か_う"
    tracker.onRender(null, null, createPreedit(1, createSegment("おか")));
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(2, 2, 3, 3, 2, 4));
    assertTracker(2, 3, 3, tracker);

    // Type Right: "あい[おか]|う"
    // The candidates{Start,End} are wrongly set to (-1, -1). This is failing case.
    // TODO(hidehiko): fix this case.
    // tracker.onRender(null, null, createPreedit(2, createSegment("おか")));
    // assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(3, 3, 4, 4, -1, -1));
    // assertTracker(2, 4, 4, tracker);

    // Reset. Set "あいうえお", and move the caret between え and お.
    tracker.onStartInput(0, 0, false);
    tracker.onRender(null, "あいうえお", null);
    assertEquals(SelectionTracker.RESET_CONTEXT, tracker.onUpdateSelection(0, 0, 4, 4, -1, -1));
    assertTracker(4, 4, 4, tracker);

    // Move to other page and back to the page to clear the field.
    tracker.onWindowHidden();
    tracker.onStartInput(-1, -1, false);
    tracker.onStartInput(0, 0, false);
    assertTracker(0, 0, 0, tracker);
  }

  public void testBug5647860OnAndroid2_3() {
    SelectionTracker tracker = new SelectionTracker();

    // Initialize
    tracker.onStartInput(0, 0, false);

    // Type "ma"
    tracker.onRender(null, null, createPreedit(1, createSegment("m")));
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(0, 0, 1, 1, 0, 1));
    assertTracker(0, 1, 1, tracker);

    tracker.onRender(null, null, createPreedit(2, createSegment("ma")));
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(1, 1, 2, 2, 0, 2));
    assertTracker(0, 2, 2, tracker);

    // Then select "Iam Example <example@exmaple.com>" from auto complete list.
    assertEquals(SelectionTracker.RESET_CONTEXT, tracker.onUpdateSelection(2, 2, 44, 44, -1, -1));
    assertTracker(44, 44, 44, tracker);

    // Type "z"
    tracker.onRender(null, null, createPreedit(1, createSegment("z")));
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(44, 44, 45, 45, 44, 45));
    assertTracker(44, 45, 45, tracker);
  }

  public void testBug6504588() {
    SelectionTracker tracker = new SelectionTracker();

    // Initialization.
    tracker.onStartInput(-1, -1, false);
    assertEquals(SelectionTracker.RESET_CONTEXT, tracker.onUpdateSelection(-1, -1, 0, 0, -1, -1));
    tracker.onStartInput(-1, -1, false);
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(-1, -1, 0, 0, -1, -1));
    tracker.onStartInput(-1, -1, false);
    tracker.onStartInput(-1, -1, false);

    assertTracker(0, 0, 0, tracker);

    // Type "あいうえお"
    tracker.onRender(null, null, createPreedit(1, createSegment("あ")));
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(-1, -1, 1, 1, 0, 1));
    assertTracker(0, 1, 1, tracker);

    tracker.onRender(null, null, createPreedit(2, createSegment("あい")));
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(1, 1, 2, 2, 0, 2));
    assertTracker(0, 2, 2, tracker);

    tracker.onRender(null, null, createPreedit(3, createSegment("あいう")));
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(2, 2, 3, 3, 0, 3));
    assertTracker(0, 3, 3, tracker);

    tracker.onRender(null, null, createPreedit(4, createSegment("あいうえ")));
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(3, 3, 4, 4, 0, 4));
    assertTracker(0, 4, 4, tracker);

    tracker.onRender(null, null, createPreedit(5, createSegment("あいうえお")));
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(4, 4, 5, 5, 0, 5));
    assertTracker(0, 5, 5, tracker);

    // Commit "あいうえお順"
    tracker.onRender(null, "あいうえお順", null);
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(5, 5, 6, 6, -1, -1));
    assertTracker(6, 6, 6, tracker);

    // Type "かきくけこ"
    tracker.onRender(null, null, createPreedit(1, createSegment("ｋ")));
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(6, 6, 7, 7, 6, 7));
    assertTracker(6, 7, 7, tracker);

    tracker.onRender(null, null, createPreedit(1, createSegment("か")));
    assertTracker(6, 7, 7, tracker);

    tracker.onRender(null, null, createPreedit(2, createSegment("かｋ")));
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(7, 7, 8, 8, 6, 8));
    assertTracker(6, 8, 8, tracker);

    tracker.onRender(null, null, createPreedit(2, createSegment("かき")));
    assertTracker(6, 8, 8, tracker);

    tracker.onRender(null, null, createPreedit(3, createSegment("かきｋ")));
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(8, 8, 9, 9, 6, 9));
    assertTracker(6, 9, 9, tracker);

    tracker.onRender(null, null, createPreedit(3, createSegment("かきく")));
    assertTracker(6, 9, 9, tracker);

    tracker.onRender(null, null, createPreedit(4, createSegment("かきくｋ")));
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(9, 9, 10, 10, 6, 10));
    assertTracker(6, 10, 10, tracker);

    tracker.onRender(null, null, createPreedit(4, createSegment("かきくけ")));
    assertTracker(6, 10, 10, tracker);

    tracker.onRender(null, null, createPreedit(5, createSegment("かきくけｋ")));
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(10, 10, 11, 11, 6, 11));
    assertTracker(6, 11, 11, tracker);

    tracker.onRender(null, null, createPreedit(5, createSegment("かきくけこ")));
    assertTracker(6, 11, 11, tracker);

    // Commit "かきくけこ": "あいうえお順かきくけこ|"
    tracker.onRender(null, "かきくけこ", null);
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(11, 11, 11, 11, -1, -1));
    assertTracker(11, 11, 11, tracker);

    // Tap bitween え and お: "あいうえ|お順かきくけこ"
    tracker.onStartInput(-1, -1, false);
    assertEquals(SelectionTracker.RESET_CONTEXT, tracker.onUpdateSelection(-1, -1, 4, 4, -1, -1));
    tracker.onStartInput(-1, -1, false);
    assertTracker(4, 4, 4, tracker);

    // Type "にほんご": "あいうえ[にほんご]|お順かきくけこ"
    tracker.onRender(null, null, createPreedit(1, createSegment("ｎ")));
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(-1, -1, 5, 5, 4, 5));
    assertTracker(4, 5, 5, tracker);

    tracker.onRender(null, null, createPreedit(1, createSegment("に")));
    assertTracker(4, 5, 5, tracker);

    tracker.onRender(null, null, createPreedit(2, createSegment("にｈ")));
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(5, 5, 6, 6, 4, 6));
    assertTracker(4, 6, 6, tracker);

    tracker.onRender(null, null, createPreedit(2, createSegment("にほ")));
    assertTracker(4, 6, 6, tracker);

    tracker.onRender(null, null, createPreedit(3, createSegment("にほｎ")));
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(6, 6, 7, 7, 4, 7));
    assertTracker(4, 7, 7, tracker);

    tracker.onRender(null, null, createPreedit(3, createSegment("にほん")));
    assertTracker(4, 7, 7, tracker);

    tracker.onRender(null, null, createPreedit(4, createSegment("にほんｇ")));
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(7, 7, 8, 8, 4, 8));
    assertTracker(4, 8, 8, tracker);

    tracker.onRender(null, null, createPreedit(4, createSegment("にほんご")));
    assertTracker(4, 8, 8, tracker);

    // Commit "日本語": "あいうえ日本語お順かきくけこ"
    tracker.onRender(null, "日本語", null);
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(8, 8, 7, 7, -1, -1));
    assertTracker(7, 7, 7, tracker);

    // Unfocus from the field.
    tracker.onStartInput(-1, -1, false);
    tracker.onStartInput(-1, -1, false);
    tracker.onStartInput(-1, -1, false);
    tracker.onWindowHidden();
    tracker.onStartInput(-1, -1, false);
  }

  public void testBug7557736() {
    SelectionTracker tracker = new SelectionTracker();

    // Initialization.
    tracker.onStartInput(-1, -1, false);
    assertEquals(SelectionTracker.RESET_CONTEXT, tracker.onUpdateSelection(-1, -1, 0, 0, -1, -1));

    // Type "あいうaa"
    tracker.onRender(null, null, createPreedit(1, createSegment("あ")));
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(-1, -1, 1, 1, 0, 1));
    assertTracker(0, 1, 1, tracker);

    tracker.onRender(null, null, createPreedit(2, createSegment("あい")));
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(1, 1, 2, 2, 0, 2));
    assertTracker(0, 2, 2, tracker);

    tracker.onRender(null, null, createPreedit(3, createSegment("あいう")));
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(2, 2, 3, 3, 0, 3));
    assertTracker(0, 3, 3, tracker);

    tracker.onRender(null, null, createPreedit(4, createSegment("あいうa")));
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(3, 3, 4, 4, 0, 4));
    assertTracker(0, 4, 4, tracker);

    tracker.onRender(null, null, createPreedit(5, createSegment("あいうaa")));
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(4, 4, 5, 5, 0, 5));
    assertTracker(0, 5, 5, tracker);

    // Commit "あいうaa"
    tracker.onRender(null, "あいうaa", null);
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(5, 5, 5, 5, -1, -1));
    assertTracker(5, 5, 5, tracker);

    // Move the cursor to right "あいうa|a"
    assertEquals(SelectionTracker.RESET_CONTEXT, tracker.onUpdateSelection(5, 5, 4, 4, -1, -1));
    assertTracker(4, 4, 4, tracker);

    // Type "あ" so that we'll get "あいうa[あ]|a"
    tracker.onRender(null, null, createPreedit(1, createSegment("あ")));
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(4, 4, 5, 5, 4, 5));
    assertTracker(4, 5, 5, tracker);

    // Then long tap around い, then first "あいう" should be selected.
    assertEquals(SelectionTracker.RESET_CONTEXT, tracker.onUpdateSelection(5, 5, 0, 3, 4, 5));

    // Finally the context reset causes canceling the composing text.
    assertEquals(SelectionTracker.RESET_CONTEXT, tracker.onUpdateSelection(0, 3, 0, 3, -1, -1));

    // Then we can type as usual.
    tracker.onRender(null, null, createPreedit(1, createSegment("あ")));
    assertEquals(SelectionTracker.DO_NOTHING, tracker.onUpdateSelection(0, 3, 1, 1, 0, 1));
    assertTracker(0, 1, 1, tracker);
  }

  interface ScenarioPiece {
    void execute(SelectionTracker tracker);
  }

  static class RenderCommit implements ScenarioPiece {

    private int commitLength;

    RenderCommit(int commitLength) {
      this.commitLength = commitLength;
    }

    @Override
    public void execute(SelectionTracker tracker) {
      tracker.onRender(null, Strings.repeat("a", commitLength), null);
    }
  }

  static class RenderPreedit implements ScenarioPiece {

    private int preeditLength;

    RenderPreedit(int preeditLength) {
      this.preeditLength = preeditLength;
    }

    @Override
    public void execute(SelectionTracker tracker) {
      tracker.onRender(null,  null,
          Preedit.newBuilder()
              .addSegment(Segment.newBuilder()
                              .setValue(Strings.repeat("a", preeditLength))
                              .buildPartial())
              .setCursor(preeditLength)
              .buildPartial());
    }
  }

  static class Selection implements ScenarioPiece {

    private int oldSelStart;
    private int oldSelEnd;
    private int newSelStart;
    private int newSelEnd;
    private int candidatesStart;
    private int candidatesEnd;
    private int expectedResult;

    Selection(int oldSelStart, int oldSelEnd,
        int newSelStart, int newSelEnd,
        int candidatesStart, int candidatesEnd,
        int expectedResult) {
      this.oldSelStart = oldSelStart;
      this.oldSelEnd = oldSelEnd;
      this.newSelStart = newSelStart;
      this.newSelEnd = newSelEnd;
      this.candidatesStart = candidatesStart;
      this.candidatesEnd = candidatesEnd;
      this.expectedResult = expectedResult;
    }

    @Override
    public void execute(SelectionTracker tracker) {
      assertEquals(
          expectedResult,
          tracker.onUpdateSelection(oldSelStart, oldSelEnd, newSelStart,
                                    newSelEnd, candidatesStart, candidatesEnd));
    }
  }

  static void runScenario(boolean webTextView, ScenarioPiece... scenaroPieces) {
    SelectionTracker tracker = new SelectionTracker();
    tracker.onStartInput(0, 0, webTextView);
    for (int i = 0; i < scenaroPieces.length; ++i) {
      try {
        scenaroPieces[i].execute(tracker);
      } catch (AssertionFailedError e) {
        throw new AssertionFailedError(
            String.format("step %d (%s): %s",
                i + 1, scenaroPieces[i].getClass().getSimpleName(), e.toString()));
      }
    }
  }

  public void testBackSpaceOnWebView() {
    runScenario(true,
        new RenderPreedit(1),
        new Selection(0, 0, 1, 1, 0, 1, SelectionTracker.DO_NOTHING),
        new RenderCommit(1),
        new Selection(1, 1, 1, 1, -1, -1, SelectionTracker.DO_NOTHING),
        // Here undetectable cursor move.
        new RenderPreedit(1),
        new Selection(1, 1, 1, 1, 0, 1, SelectionTracker.DO_NOTHING));
  }

  public void testBackSpaceOnTextEdit() {
    runScenario(false,
        new RenderPreedit(1),
        new Selection(0, 0, 1, 1, 0, 1, SelectionTracker.DO_NOTHING),
        new RenderCommit(1),
        new Selection(1, 1, 1, 1, -1, -1, SelectionTracker.DO_NOTHING),
        // Here undetectable cursor move.
        new RenderPreedit(1),
        new Selection(1, 1, 1, 1, 0, 1, SelectionTracker.RESET_CONTEXT));
  }

  public void testBackSpaceOnWebView2() {
    runScenario(true,
        new RenderPreedit(4),
        new Selection(0, 0, 4, 4, 0, 4, SelectionTracker.DO_NOTHING),
        new RenderCommit(4),
        new Selection(4, 4, 4, 4, -1, -1, SelectionTracker.DO_NOTHING),
        // Here undetectable cursor move to the beginning.
        new RenderPreedit(1),
        new Selection(4, 4, 1, 1, 0, 1, SelectionTracker.DO_NOTHING));
  }

  public void testBackSpaceOnTextEdit2() {
    runScenario(false,
        new RenderPreedit(4),
        new Selection(0, 0, 4, 4, 0, 4, SelectionTracker.DO_NOTHING),
        new RenderCommit(4),
        new Selection(4, 4, 4, 4, -1, -1, SelectionTracker.DO_NOTHING),
        // Here undetectable cursor move to the beginning.
        new RenderPreedit(1),
        new Selection(4, 4, 1, 1, 0, 1, SelectionTracker.RESET_CONTEXT));
  }

  public void testQuickTypeOnWebView() {
    runScenario(true,
        new RenderPreedit(10),  // If a user types very quickly, merged result will be arrive.
        new Selection(0, 0, 10, 10, 0, 10, SelectionTracker.DO_NOTHING));
  }

  public void testQuickTypeOnTextEdit() {
    runScenario(false,
        new RenderPreedit(10),  // If a user types very quickly, merged result will be arrive.
        new Selection(0, 0, 10, 10, 0, 10, SelectionTracker.DO_NOTHING));
  }

  public void testCompletionOnWebView() {
    runScenario(true,
        // Type two characters. Corresponding selection updates are done later.
        new RenderPreedit(1),
        new RenderPreedit(2),

        // 1st call-back correspoing to 1st character.
        new Selection(0, 0, 1, 1, 0, 1, SelectionTracker.DO_NOTHING),

        // Here completion is applied without selection update call-back.
        // The cursor moves to position 10. No preedit is shown.

        // 2nd call-back correspoing to 2nd character.
        // Note that old selection is 10 because of undetactable completion.
        // NOTE: What should expected here? Both resetting and keeping as-is are
        // uncomfortable for the users. Here DO_NOTHING is set as expectation but
        // RESET_CONTEXT is also acceptable here.
        new Selection(10, 10, 12, 12, 10, 12, SelectionTracker.DO_NOTHING));
  }

  public void testCompletionOnTextEdit() {
    runScenario(false,
        new RenderPreedit(1),
        new RenderPreedit(2),
        new Selection(0, 0, 1, 1, 0, 1, SelectionTracker.DO_NOTHING),
        new Selection(10, 10, 12, 12, 10, 12, SelectionTracker.RESET_CONTEXT));
  }
}
