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

package org.mozc.android.inputmethod.japanese;

import static android.test.MoreAsserts.assertAssignableFrom;

import static org.easymock.EasyMock.eq;
import static org.easymock.EasyMock.expect;
import static org.easymock.EasyMock.same;

import org.mozc.android.inputmethod.japanese.testing.ApiLevel;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;

import android.os.Bundle;
import android.view.KeyEvent;
import android.view.inputmethod.CompletionInfo;
import android.view.inputmethod.CorrectionInfo;
import android.view.inputmethod.ExtractedText;
import android.view.inputmethod.ExtractedTextRequest;
import android.view.inputmethod.InputConnection;

/**
 */
public class ComposingTextTrackingInputConnectionTest extends InstrumentationTestCaseWithMock {
  private InputConnection inputConnectionMock;
  private ComposingTextTrackingInputConnection composingTextTrackingInputConnection;

  @Override
  protected void setUp() throws Exception {
    super.setUp();
    inputConnectionMock = createMock(InputConnection.class);
    composingTextTrackingInputConnection =
        new ComposingTextTrackingInputConnection(inputConnectionMock);
  }

  @Override
  protected void tearDown() throws Exception {
    composingTextTrackingInputConnection = null;
    inputConnectionMock = null;
    super.tearDown();
  }

  public void testComposingText() {
    assertEquals("", composingTextTrackingInputConnection.getComposingText());

    expect(inputConnectionMock.setComposingText("abc", 0)).andReturn(true);
    replayAll();

    assertTrue(composingTextTrackingInputConnection.setComposingText("abc", 0));

    verifyAll();
    assertEquals("abc", composingTextTrackingInputConnection.getComposingText());

    resetAll();
    expect(inputConnectionMock.setComposingText("def", 1)).andReturn(true);
    replayAll();

    assertTrue(composingTextTrackingInputConnection.setComposingText("def", 1));

    verifyAll();
    assertEquals("def", composingTextTrackingInputConnection.getComposingText());

    resetAll();
    expect(inputConnectionMock.finishComposingText()).andReturn(true);
    replayAll();

    assertTrue(composingTextTrackingInputConnection.finishComposingText());

    verifyAll();
    assertEquals("", composingTextTrackingInputConnection.getComposingText());
  }

  public void testBeginBatchEdit() {
    expect(inputConnectionMock.beginBatchEdit()).andReturn(true);
    replayAll();

    assertTrue(composingTextTrackingInputConnection.beginBatchEdit());

    verifyAll();
  }

  public void testClearMetaKeyStates() {
    expect(inputConnectionMock.clearMetaKeyStates(0)).andReturn(true);
    replayAll();

    assertTrue(composingTextTrackingInputConnection.clearMetaKeyStates(0));

    verifyAll();
  }

  public void testCommitCompletion() {
    CompletionInfo text = new CompletionInfo(0, 0, "");
    expect(inputConnectionMock.commitCompletion(same(text))).andReturn(true);
    replayAll();

    assertTrue(composingTextTrackingInputConnection.commitCompletion(text));

    verifyAll();
  }

  @ApiLevel(11)
  public void testCommitCorrection() {
    CorrectionInfo info = new CorrectionInfo(0, "", "");
    expect(inputConnectionMock.commitCorrection(same(info))).andReturn(true);
    replayAll();

    assertTrue(composingTextTrackingInputConnection.commitCorrection(info));

    verifyAll();
  }

  public void testCommitText() {
    expect(inputConnectionMock.commitText("abc", 0)).andReturn(true);
    replayAll();

    assertTrue(composingTextTrackingInputConnection.commitText("abc", 0));

    verifyAll();
  }

  public void testDeleteSurroundingText() {
    expect(inputConnectionMock.deleteSurroundingText(5, 10)).andReturn(true);
    replayAll();

    assertTrue(composingTextTrackingInputConnection.deleteSurroundingText(5, 10));

    verifyAll();
  }

  public void testEndBatchEdit() {
    expect(inputConnectionMock.endBatchEdit()).andReturn(true);
    replayAll();

    assertTrue(composingTextTrackingInputConnection.endBatchEdit());

    verifyAll();
  }

  public void testFinishComposingText() {
    expect(inputConnectionMock.finishComposingText()).andReturn(true);
    replayAll();

    assertTrue(composingTextTrackingInputConnection.finishComposingText());

    verifyAll();
  }

  public void testGetCursorCapsMode() {
    expect(inputConnectionMock.getCursorCapsMode(0)).andReturn(10);
    replayAll();

    assertEquals(10, composingTextTrackingInputConnection.getCursorCapsMode(0));

    verifyAll();
  }

  public void testGetExtractedText() {
    ExtractedText result = new ExtractedText();
    ExtractedTextRequest request = new ExtractedTextRequest();
    expect(inputConnectionMock.getExtractedText(same(request), eq(0))).andReturn(result);
    replayAll();

    assertSame(result, composingTextTrackingInputConnection.getExtractedText(request, 0));

    verifyAll();
  }

  @ApiLevel(9)
  public void testGetSelectedText() {
    expect(inputConnectionMock.getSelectedText(0)).andReturn("abc");
    replayAll();

    assertEquals("abc", composingTextTrackingInputConnection.getSelectedText(0));

    verifyAll();
  }

  public void testGetTextAfterCursor() {
    expect(inputConnectionMock.getTextAfterCursor(10, 0)).andReturn("abc");
    replayAll();

    assertEquals("abc", composingTextTrackingInputConnection.getTextAfterCursor(10, 0));

    verifyAll();
  }

  public void testGetTextBeforeCursor() {
    expect(inputConnectionMock.getTextBeforeCursor(10, 0)).andReturn("abc");
    replayAll();

    assertEquals("abc", composingTextTrackingInputConnection.getTextBeforeCursor(10, 0));

    verifyAll();
  }

  public void testPerformContextMenuAction() {
    expect(inputConnectionMock.performContextMenuAction(10)).andReturn(true);
    replayAll();

    assertTrue(composingTextTrackingInputConnection.performContextMenuAction(10));

    verifyAll();
  }

  public void testPerformEditorAction() {
    expect(inputConnectionMock.performEditorAction(10)).andReturn(true);
    replayAll();

    assertTrue(composingTextTrackingInputConnection.performEditorAction(10));

    verifyAll();
  }

  public void testPerformPrivateCommand() {
    Bundle data = new Bundle();
    expect(inputConnectionMock.performPrivateCommand(eq("action"), same(data))).andReturn(true);
    replayAll();

    assertTrue(composingTextTrackingInputConnection.performPrivateCommand("action", data));

    verifyAll();
  }

  public void testReportFullscreenMode() {
    expect(inputConnectionMock.reportFullscreenMode(true)).andReturn(true);
    replayAll();

    assertTrue(composingTextTrackingInputConnection.reportFullscreenMode(true));

    verifyAll();
  }

  public void testSendKeyEvent() {
    KeyEvent event = new KeyEvent(0, 0);
    expect(inputConnectionMock.sendKeyEvent(same(event))).andReturn(true);
    replayAll();

    assertTrue(composingTextTrackingInputConnection.sendKeyEvent(event));

    verifyAll();
  }

  @ApiLevel(9)
  public void testSetComposingRegion(int start, int end) {
    expect(inputConnectionMock.setComposingRegion(10, 20)).andReturn(true);
    replayAll();

    assertTrue(composingTextTrackingInputConnection.setComposingRegion(10, 20));

    verifyAll();
  }

  public void testSetComposingText() {
    expect(inputConnectionMock.setComposingText("abc", 0)).andReturn(true);
    replayAll();

    assertTrue(composingTextTrackingInputConnection.setComposingText("abc", 0));

    verifyAll();
  }

  public void testSetSelection() {
    expect(inputConnectionMock.setSelection(10, 20)).andReturn(true);
    replayAll();

    assertTrue(composingTextTrackingInputConnection.setSelection(10, 20));

    verifyAll();
  }

  public void testNewInstance() {
    assertNull(ComposingTextTrackingInputConnection.newInstance(null));
    assertSame(
        composingTextTrackingInputConnection,
        ComposingTextTrackingInputConnection.newInstance(composingTextTrackingInputConnection));
    assertAssignableFrom(
        ComposingTextTrackingInputConnection.class,
        ComposingTextTrackingInputConnection.newInstance(inputConnectionMock));
  }
}
