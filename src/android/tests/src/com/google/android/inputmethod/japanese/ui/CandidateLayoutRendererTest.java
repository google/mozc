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

package org.mozc.android.inputmethod.japanese.ui;

import static org.easymock.EasyMock.eq;
import static org.easymock.EasyMock.expect;
import static org.easymock.EasyMock.getCurrentArguments;
import static org.easymock.EasyMock.isA;
import static org.easymock.EasyMock.isNull;
import static org.easymock.EasyMock.same;

import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateList;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateWord;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;
import org.mozc.android.inputmethod.japanese.testing.VisibilityProxy;
import org.mozc.android.inputmethod.japanese.ui.CandidateLayout.Row;
import org.mozc.android.inputmethod.japanese.ui.CandidateLayout.Span;
import org.mozc.android.inputmethod.japanese.view.EmojiRenderHelper;

import android.graphics.Canvas;
import android.graphics.Rect;
import android.view.View;

import org.easymock.IAnswer;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 */
public class CandidateLayoutRendererTest extends InstrumentationTestCaseWithMock {
  public void testDrawCandidateLayout() {
    // Create 5x5 dummy candidate list.
    CandidateList.Builder builder = CandidateList.newBuilder();
    for (int i = 0; i < 25; ++i) {
      builder.addCandidates(CandidateWord.newBuilder()
          .setId(i)
          .setIndex(i));
    }
    // Set focused index to 7-th (2-row, 2-column) candidate word.
    builder.setFocusedIndex(6);
    CandidateList candidateList = builder.build();

    // Then layout it to 10px x 10px grid.
    List<Row> rowList = new ArrayList<Row>(5);
    {
      int index = 0;
      for (int i = 0; i < 5; ++i) {
        Row row = new Row();
        rowList.add(row);
        row.setTop(10 * i);
        row.setHeight(10);
        row.setWidth(50);

        for (int j = 0; j < 5; ++j) {
          // Insert a null candidate word span
          CandidateWord candidateWord = (i == 2 && j == 0) ? null :
              candidateList.getCandidates(index++);
          Span span = new Span(candidateWord, 0, 0, Collections.<String>emptyList());
          span.setLeft(j * 10);
          span.setRight((j + 1) * 10);
          row.addSpan(span);
        }
      }
    }
    CandidateLayout candidateLayout = new CandidateLayout(rowList, 50, 50);

    // Set up the testee instance and mocks.
    View targetView = createViewMock(View.class);
    CandidateLayoutRenderer renderer = createMockBuilder(CandidateLayoutRenderer.class)
        .withConstructor(View.class)
        .withArgs(targetView)
        .addMockedMethod("drawSpan")
        .createMock();

    // Inject EmojiRenderHelper mock.
    resetAll();
    EmojiRenderHelper emojiRenderHelper = createMockBuilder(EmojiRenderHelper.class)
        .withConstructor(View.class)
        .withArgs(targetView)
        .createMock();
    VisibilityProxy.setField(renderer, "emojiRenderHelper", emojiRenderHelper);

    Canvas canvas = createMock(Canvas.class);
    resetAll();

    // Set CandidateList should affect to the emojiRenderHelper.
    emojiRenderHelper.setCandidateList(same(candidateList));
    replayAll();

    renderer.setCandidateList(candidateList);

    verifyAll();
    // The focused index should also be updated.
    assertEquals(6, VisibilityProxy.getField(renderer, "focusedIndex"));

    resetAll();
    emojiRenderHelper.setCandidateList(isNull(CandidateList.class));
    replayAll();

    renderer.setCandidateList(null);

    verifyAll();
    assertEquals(-1, VisibilityProxy.getField(renderer, "focusedIndex"));

    resetAll();
    // Set the candidateList again.
    emojiRenderHelper.setCandidateList(same(candidateList));
    replayAll();

    renderer.setCandidateList(candidateList);

    verifyAll();
    assertEquals(6, VisibilityProxy.getField(renderer, "focusedIndex"));

    // Then render the layout to the canvas.
    resetAll();

    // The onPreDraw() should be invoked.
    emojiRenderHelper.onPreDraw();

    // Dummy clip of the canvas. It's (15, 15) - (35, 35).
    expect(canvas.getClipBounds(isA(Rect.class))).andStubAnswer(new IAnswer<Boolean>() {
      @Override
      public Boolean answer() throws Throwable {
        Rect.class.cast(getCurrentArguments()[0]).set(15, 15, 35, 35);
        return Boolean.TRUE;
      }
    });

    // All spans which are in the bounding box (or crossing the boundary) should be rendered.
    // (1, 1) element is focused (specified by CandidateList#getFocusedIndex), and (1, 3) element is
    // pressed (set "8" in the drawCandidateLayout's argument below).
    renderer.drawSpan(
        same(canvas), same(rowList.get(1)), same(rowList.get(1).getSpanList().get(1)), eq(true));
    renderer.drawSpan(
        same(canvas), same(rowList.get(1)), same(rowList.get(1).getSpanList().get(2)), eq(false));
    renderer.drawSpan(
        same(canvas), same(rowList.get(1)), same(rowList.get(1).getSpanList().get(3)), eq(true));

    renderer.drawSpan(
        same(canvas), same(rowList.get(2)), same(rowList.get(2).getSpanList().get(1)), eq(false));
    renderer.drawSpan(
        same(canvas), same(rowList.get(2)), same(rowList.get(2).getSpanList().get(2)), eq(false));
    renderer.drawSpan(
        same(canvas), same(rowList.get(2)), same(rowList.get(2).getSpanList().get(3)), eq(false));

    renderer.drawSpan(
        same(canvas), same(rowList.get(3)), same(rowList.get(3).getSpanList().get(1)), eq(false));
    renderer.drawSpan(
        same(canvas), same(rowList.get(3)), same(rowList.get(3).getSpanList().get(2)), eq(false));
    renderer.drawSpan(
        same(canvas), same(rowList.get(3)), same(rowList.get(3).getSpanList().get(3)), eq(false));

    // After all the rendering process, onPostDraw should be invoked.
    emojiRenderHelper.onPostDraw();

    replayAll();

    // 8 is the pseudo pressing candidate.
    renderer.drawCandidateLayout(canvas, candidateLayout, 8);

    verifyAll();
  }
}
