// Copyright 2010-2018, Google Inc.
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
import static org.easymock.EasyMock.geq;
import static org.easymock.EasyMock.getCurrentArguments;
import static org.easymock.EasyMock.isA;
import static org.easymock.EasyMock.leq;
import static org.easymock.EasyMock.same;

import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.Annotation;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateList;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateWord;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;
import org.mozc.android.inputmethod.japanese.ui.CandidateLayout.Row;
import org.mozc.android.inputmethod.japanese.ui.CandidateLayout.Span;
import org.mozc.android.inputmethod.japanese.ui.CandidateLayoutRenderer.DescriptionLayoutPolicy;
import org.mozc.android.inputmethod.japanese.ui.CandidateLayoutRenderer.ValueScalingPolicy;
import org.mozc.android.inputmethod.japanese.view.CarrierEmojiRenderHelper;
import com.google.common.base.Optional;

import android.graphics.Canvas;
import android.graphics.Paint;
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
          Optional<CandidateWord> candidateWord = (i == 2 && j == 0)
              ? Optional.<CandidateWord>absent()
              : Optional.of(candidateList.getCandidates(index++));
          Span span = new Span(candidateWord, 0, 0, Collections.<String>emptyList());
          span.setLeft(j * 10);
          span.setRight((j + 1) * 10);
          row.addSpan(span);
        }
      }
    }
    CandidateLayout candidateLayout = new CandidateLayout(rowList, 50, 50);

    CandidateLayoutRenderer renderer = createMockBuilder(CandidateLayoutRenderer.class)
        .addMockedMethod("drawSpan")
        .createMock();

    // Test if the focused index is updated by setCandidateList.
    resetAll();

    replayAll();
    renderer.setCandidateList(Optional.of(candidateList));

    verifyAll();
    assertEquals(6, renderer.focusedIndex);

    // Test if the focused index is -1 for absent candidate list.
    resetAll();

    replayAll();
    renderer.setCandidateList(Optional.<CandidateList>absent());

    verifyAll();
    assertEquals(-1, renderer.focusedIndex);

    // drawCandidateLayout is tested below.  Set up CarrrierEmojiRenderHelper and Canvas mocks.
    View targetView = createViewMock(View.class);
    CarrierEmojiRenderHelper carrierEmojiRenderHelper =
        createMockBuilder(CarrierEmojiRenderHelper.class)
        .withConstructor(View.class)
        .withArgs(targetView)
        .createNiceMock();
    Canvas canvas = createMockBuilder(Canvas.class)
        .addMockedMethod("getClipBounds", Rect.class)
        .addMockedMethod("drawLine")
        .createMock();

    // Set the candidate list to both renderer and carrierEmojiRenderHelper.
    resetAll();

    replayAll();
    renderer.setCandidateList(Optional.of(candidateList));
    carrierEmojiRenderHelper.setCandidateList(Optional.of(candidateList));

    verifyAll();
    assertEquals(6, renderer.focusedIndex);

    // Then render the layout to the canvas.
    resetAll();

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
    // pressed (set "8" in the drawCandidateLayout's argument below).  In this case, separators are
    // drawn on the right edge of each span because each span to be rendered is not rightmost.
    // Separator length is not tested but should be less than or equal to the edge length, i.e., 10.
    renderer.drawSpan(
        same(canvas), same(rowList.get(1)), same(rowList.get(1).getSpanList().get(1)), eq(true),
        same(carrierEmojiRenderHelper));
    canvas.drawLine(eq(10f), geq(10f), eq(10f), leq(20f), isA(Paint.class));
    renderer.drawSpan(
        same(canvas), same(rowList.get(1)), same(rowList.get(1).getSpanList().get(2)), eq(false),
        same(carrierEmojiRenderHelper));
    canvas.drawLine(eq(20f), geq(10f), eq(20f), leq(20f), isA(Paint.class));
    renderer.drawSpan(
        same(canvas), same(rowList.get(1)), same(rowList.get(1).getSpanList().get(3)), eq(true),
        same(carrierEmojiRenderHelper));
    canvas.drawLine(eq(30f), geq(10f), eq(30f), leq(20f), isA(Paint.class));

    renderer.drawSpan(
        same(canvas), same(rowList.get(2)), same(rowList.get(2).getSpanList().get(1)), eq(false),
        same(carrierEmojiRenderHelper));
    canvas.drawLine(eq(10f), geq(20f), eq(10f), leq(30f), isA(Paint.class));
    renderer.drawSpan(
        same(canvas), same(rowList.get(2)), same(rowList.get(2).getSpanList().get(2)), eq(false),
        same(carrierEmojiRenderHelper));
    canvas.drawLine(eq(20f), geq(20f), eq(20f), leq(30f), isA(Paint.class));
    renderer.drawSpan(
        same(canvas), same(rowList.get(2)), same(rowList.get(2).getSpanList().get(3)), eq(false),
        same(carrierEmojiRenderHelper));
    canvas.drawLine(eq(30f), geq(20f), eq(30f), leq(30f), isA(Paint.class));

    renderer.drawSpan(
        same(canvas), same(rowList.get(3)), same(rowList.get(3).getSpanList().get(1)), eq(false),
        same(carrierEmojiRenderHelper));
    canvas.drawLine(eq(10f), geq(30f), eq(10f), leq(40f), isA(Paint.class));
    renderer.drawSpan(
        same(canvas), same(rowList.get(3)), same(rowList.get(3).getSpanList().get(2)), eq(false),
        same(carrierEmojiRenderHelper));
    canvas.drawLine(eq(20f), geq(30f), eq(20f), leq(40f), isA(Paint.class));
    renderer.drawSpan(
        same(canvas), same(rowList.get(3)), same(rowList.get(3).getSpanList().get(3)), eq(false),
        same(carrierEmojiRenderHelper));
    canvas.drawLine(eq(30f), geq(30f), eq(30f), leq(40f), isA(Paint.class));

    replayAll();

    // 8 is the pseudo pressing candidate.
    renderer.drawCandidateLayout(canvas, candidateLayout, 8, carrierEmojiRenderHelper);

    verifyAll();

    // Next, test rendering on the whole canvas.
    resetAll();

    expect(canvas.getClipBounds(isA(Rect.class))).andStubAnswer(new IAnswer<Boolean>() {
      @Override
      public Boolean answer() throws Throwable {
        Rect.class.cast(getCurrentArguments()[0]).set(0, 0, 50, 50);
        return Boolean.TRUE;
      }
    });

    // Note that, in each row, a separator is not drawn after the last span, i.e., drawn only
    // between spans.  The focused and pressed spans are the same as before.
    // Row 0
    renderer.drawSpan(
        same(canvas), same(rowList.get(0)), same(rowList.get(0).getSpanList().get(0)), eq(false),
        same(carrierEmojiRenderHelper));
    canvas.drawLine(eq(10f), geq(0f), eq(10f), leq(10f), isA(Paint.class));
    renderer.drawSpan(
        same(canvas), same(rowList.get(0)), same(rowList.get(0).getSpanList().get(1)), eq(false),
        same(carrierEmojiRenderHelper));
    canvas.drawLine(eq(20f), geq(0f), eq(20f), leq(10f), isA(Paint.class));
    renderer.drawSpan(
        same(canvas), same(rowList.get(0)), same(rowList.get(0).getSpanList().get(2)), eq(false),
        same(carrierEmojiRenderHelper));
    canvas.drawLine(eq(30f), geq(0f), eq(30f), leq(10f), isA(Paint.class));
    renderer.drawSpan(
        same(canvas), same(rowList.get(0)), same(rowList.get(0).getSpanList().get(3)), eq(false),
        same(carrierEmojiRenderHelper));
    canvas.drawLine(eq(40f), geq(0f), eq(40f), leq(10f), isA(Paint.class));
    renderer.drawSpan(
        same(canvas), same(rowList.get(0)), same(rowList.get(0).getSpanList().get(4)), eq(false),
        same(carrierEmojiRenderHelper));

    // Row 1
    renderer.drawSpan(
        same(canvas), same(rowList.get(1)), same(rowList.get(1).getSpanList().get(0)), eq(false),
        same(carrierEmojiRenderHelper));
    canvas.drawLine(eq(10f), geq(10f), eq(10f), leq(20f), isA(Paint.class));
    renderer.drawSpan(
        same(canvas), same(rowList.get(1)), same(rowList.get(1).getSpanList().get(1)), eq(true),
        same(carrierEmojiRenderHelper));
    canvas.drawLine(eq(20f), geq(10f), eq(20f), leq(20f), isA(Paint.class));
    renderer.drawSpan(
        same(canvas), same(rowList.get(1)), same(rowList.get(1).getSpanList().get(2)), eq(false),
        same(carrierEmojiRenderHelper));
    canvas.drawLine(eq(30f), geq(10f), eq(30f), leq(20f), isA(Paint.class));
    renderer.drawSpan(
        same(canvas), same(rowList.get(1)), same(rowList.get(1).getSpanList().get(3)), eq(true),
        same(carrierEmojiRenderHelper));
    canvas.drawLine(eq(40f), geq(10f), eq(40f), leq(20f), isA(Paint.class));
    renderer.drawSpan(
        same(canvas), same(rowList.get(1)), same(rowList.get(1).getSpanList().get(4)), eq(false),
        same(carrierEmojiRenderHelper));

    // Row 2
    renderer.drawSpan(
        same(canvas), same(rowList.get(2)), same(rowList.get(2).getSpanList().get(0)), eq(false),
        same(carrierEmojiRenderHelper));
    canvas.drawLine(eq(10f), geq(20f), eq(10f), leq(30f), isA(Paint.class));
    renderer.drawSpan(
        same(canvas), same(rowList.get(2)), same(rowList.get(2).getSpanList().get(1)), eq(false),
        same(carrierEmojiRenderHelper));
    canvas.drawLine(eq(20f), geq(20f), eq(20f), leq(30f), isA(Paint.class));
    renderer.drawSpan(
        same(canvas), same(rowList.get(2)), same(rowList.get(2).getSpanList().get(2)), eq(false),
        same(carrierEmojiRenderHelper));
    canvas.drawLine(eq(30f), geq(20f), eq(30f), leq(30f), isA(Paint.class));
    renderer.drawSpan(
        same(canvas), same(rowList.get(2)), same(rowList.get(2).getSpanList().get(3)), eq(false),
        same(carrierEmojiRenderHelper));
    canvas.drawLine(eq(40f), geq(20f), eq(40f), leq(30f), isA(Paint.class));
    renderer.drawSpan(
        same(canvas), same(rowList.get(2)), same(rowList.get(2).getSpanList().get(4)), eq(false),
        same(carrierEmojiRenderHelper));

    // Row 3
    renderer.drawSpan(
        same(canvas), same(rowList.get(3)), same(rowList.get(3).getSpanList().get(0)), eq(false),
        same(carrierEmojiRenderHelper));
    canvas.drawLine(eq(10f), geq(30f), eq(10f), leq(40f), isA(Paint.class));
    renderer.drawSpan(
        same(canvas), same(rowList.get(3)), same(rowList.get(3).getSpanList().get(1)), eq(false),
        same(carrierEmojiRenderHelper));
    canvas.drawLine(eq(20f), geq(30f), eq(20f), leq(40f), isA(Paint.class));
    renderer.drawSpan(
        same(canvas), same(rowList.get(3)), same(rowList.get(3).getSpanList().get(2)), eq(false),
        same(carrierEmojiRenderHelper));
    canvas.drawLine(eq(30f), geq(30f), eq(30f), leq(40f), isA(Paint.class));
    renderer.drawSpan(
        same(canvas), same(rowList.get(3)), same(rowList.get(3).getSpanList().get(3)), eq(false),
        same(carrierEmojiRenderHelper));
    canvas.drawLine(eq(40f), geq(30f), eq(40f), leq(40f), isA(Paint.class));
    renderer.drawSpan(
        same(canvas), same(rowList.get(3)), same(rowList.get(3).getSpanList().get(4)), eq(false),
        same(carrierEmojiRenderHelper));

    // Row 4
    renderer.drawSpan(
        same(canvas), same(rowList.get(4)), same(rowList.get(4).getSpanList().get(0)), eq(false),
        same(carrierEmojiRenderHelper));
    canvas.drawLine(eq(10f), geq(40f), eq(10f), leq(50f), isA(Paint.class));
    renderer.drawSpan(
        same(canvas), same(rowList.get(4)), same(rowList.get(4).getSpanList().get(1)), eq(false),
        same(carrierEmojiRenderHelper));
    canvas.drawLine(eq(20f), geq(40f), eq(20f), leq(50f), isA(Paint.class));
    renderer.drawSpan(
        same(canvas), same(rowList.get(4)), same(rowList.get(4).getSpanList().get(2)), eq(false),
        same(carrierEmojiRenderHelper));
    canvas.drawLine(eq(30f), geq(40f), eq(30f), leq(50f), isA(Paint.class));
    renderer.drawSpan(
        same(canvas), same(rowList.get(4)), same(rowList.get(4).getSpanList().get(3)), eq(false),
        same(carrierEmojiRenderHelper));
    canvas.drawLine(eq(40f), geq(40f), eq(40f), leq(50f), isA(Paint.class));
    renderer.drawSpan(
        same(canvas), same(rowList.get(4)), same(rowList.get(4).getSpanList().get(4)), eq(false),
        same(carrierEmojiRenderHelper));

    replayAll();
    renderer.drawCandidateLayout(canvas, candidateLayout, 8, carrierEmojiRenderHelper);

    verifyAll();
  }

  public void testVeryLongValueAndDescription() {
    String veryLongWord = "";
    for (int i = 0; i < 64; ++i) {
      veryLongWord += "ああああ";
    }
    CandidateList candidateList = CandidateList.newBuilder()
        .addCandidates(CandidateWord.newBuilder()
            .setId(0)
            .setIndex(0)
            .setKey(veryLongWord)
            .setValue(veryLongWord)
            .setAnnotation(Annotation.newBuilder()
                .setDescription(veryLongWord)))
        .build();
    CandidateLayoutRenderer renderer = new CandidateLayoutRenderer();
    renderer.setCandidateList(Optional.of(candidateList));
    renderer.setDescriptionLayoutPolicy(DescriptionLayoutPolicy.EXCLUSIVE);
    renderer.setValueScalingPolicy(ValueScalingPolicy.HORIZONTAL);
    int textSize = 10;
    renderer.setValueTextSize(textSize);
    renderer.setDescriptionTextSize(textSize);
    Paint paint = new Paint();
    paint.setTextSize(textSize);
    float wordWidth = paint.measureText(veryLongWord);

    Row row = new Row();
    row.setTop(10);
    row.setHeight(10);
    row.setWidth(10);
    Span span = new Span(
        Optional.of(candidateList.getCandidates(0)), wordWidth, wordWidth,
        Collections.<String>emptyList());
    span.setLeft(0);
    span.setRight(10);
    row.addSpan(span);
    List<Row> rowList = Collections.singletonList(row);
    CandidateLayout candidateLayout = new CandidateLayout(rowList, 10, 10);

    View targetView = createViewMock(View.class);
    CarrierEmojiRenderHelper carrierEmojiRenderHelper =
        createMockBuilder(CarrierEmojiRenderHelper.class)
        .withConstructor(View.class)
        .withArgs(targetView)
        .createNiceMock();
    Canvas canvas = createMockBuilder(Canvas.class)
        .addMockedMethod("getClipBounds", Rect.class)
        .createNiceMock();
    expect(canvas.getClipBounds(isA(Rect.class))).andStubAnswer(new IAnswer<Boolean>() {
      @Override
      public Boolean answer() throws Throwable {
        Rect.class.cast(getCurrentArguments()[0]).set(0, 0, 10, 10);
        return Boolean.TRUE;
      }
    });

    replayAll();
    renderer.drawCandidateLayout(canvas, candidateLayout, 0, carrierEmojiRenderHelper);
    verifyAll();
  }
}
