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

import static org.easymock.EasyMock.anyInt;
import static org.easymock.EasyMock.eq;
import static org.easymock.EasyMock.expect;
import static org.easymock.EasyMock.isA;
import static org.easymock.EasyMock.same;

import org.mozc.android.inputmethod.japanese.CandidateWordView.Orientation;
import org.mozc.android.inputmethod.japanese.CandidateWordView.OrientationTrait;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateList;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateWord;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;
import org.mozc.android.inputmethod.japanese.testing.MozcLayoutUtil;
import org.mozc.android.inputmethod.japanese.testing.Parameter;
import org.mozc.android.inputmethod.japanese.testing.VisibilityProxy;
import org.mozc.android.inputmethod.japanese.ui.CandidateLayout;
import org.mozc.android.inputmethod.japanese.ui.CandidateLayout.Row;
import org.mozc.android.inputmethod.japanese.ui.CandidateLayout.Span;
import org.mozc.android.inputmethod.japanese.ui.CandidateLayouter;
import org.mozc.android.inputmethod.japanese.ui.SnapScroller;

import android.annotation.SuppressLint;
import android.content.Context;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.MotionEvent;
import android.view.View;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/**
 */
public class CandidateWordViewTest extends InstrumentationTestCaseWithMock {
  class StubCandidateWordView extends CandidateWordView {
    StubCandidateWordView(Context context, OrientationTrait orientationTrait) {
      super(context, orientationTrait);
    }

    @Override
    public void invalidate() {
      // We are not willing to test rendering routine running background of event processing test.
      // So just ignore this method's invocation.
    }
  }

  public static Span createNullCandidateWordSpan(int left, int right) {
    Span span = new Span(
        null, 0, 0, Collections.<String>emptyList());
    span.setLeft(left);
    span.setRight(right);
    return span;
  }

  private static final List<Row> ROW_DATA =
      Arrays.asList(
          MozcLayoutUtil.createRow(10, 20, 50,
                                   MozcLayoutUtil.createSpan(0, 10, 30),
                                   MozcLayoutUtil.createSpan(1, 40, 60)),
          MozcLayoutUtil.createRow(40, 20, 50,
                                   MozcLayoutUtil.createSpan(0, 10, 30),
                                   MozcLayoutUtil.createSpan(1, 40, 60)),
          MozcLayoutUtil.createRow(70, 20, 50,
                                   createNullCandidateWordSpan(10, 30),
                                   createNullCandidateWordSpan(40, 60)));

  private static final CandidateList CANDIDATE_LIST = CandidateList.newBuilder()
      .addCandidates(CandidateWord.newBuilder().setId(0).setValue("cand1"))
      .addCandidates(CandidateWord.newBuilder().setId(1).setValue("cand2"))
      .build();

  @SmallTest
  public void testOrientationHorizontal() {
    View view = createViewMock(View.class);
    OrientationTrait trait = Orientation.HORIZONTAL;
    // getScrollPosition
    // We cannot do enough test because we cannot control the result of
    // View#getScrollX().
    assertEquals(view.getScrollX(), trait.getScrollPosition(view));

    // scrollTo
    view.scrollTo(100, 0);
    replayAll();
    trait.scrollTo(view, 100);
    verifyAll();

    // getValueFromPair
    assertEquals(100f, trait.projectVector(100f, 0f));

    // Note that Row and Span are final class.
    Row row = MozcLayoutUtil.createRow(100, 200, 400);
    Span span = MozcLayoutUtil.createSpan(0, 300, 700);

    // getPosition
    assertEquals(300f, trait.getCandidatePosition(row, span));
    // getLength
    assertEquals(400f, trait.getCandidateLength(row, span));

    // getViewLength
    // We cannot do enough test because we cannot control the result of
    // View#getWidth().
    // TODO(matsuza): Use #setLeft() method (API Level 11)
    assertEquals(view.getWidth(), trait.getViewLength(view));
  }

  @SmallTest
  public void testOrientationVertical() {
    View view = createViewMock(View.class);
    OrientationTrait trait = Orientation.VERTICAL;

    // getScrollPosition
    // We cannot do enough test because we cannot control the result of
    // View#getScrollY().
    assertEquals(view.getScrollY(), trait.getScrollPosition(view));

    // scrollTo
    view.scrollTo(0, 100);
    replayAll();
    trait.scrollTo(view, 100);
    verifyAll();

    // getValueFromPair
    assertEquals(100f, trait.projectVector(0f, 100f));

    // Note that Row and Span are final class.
    Row row = MozcLayoutUtil.createRow(100, 200, 400);
    Span span = MozcLayoutUtil.createSpan(0, 300, 700);

    // getPosition
    assertEquals(100f, trait.getCandidatePosition(row, span));
    // getLength
    assertEquals(200f, trait.getCandidateLength(row, span));

    // getViewLength
    // We cannot do enough test because we cannot control the result of
    // View#getWidth().
    // TODO(matsuza): Use #setBotton() method (API Level 11)
    assertEquals(view.getHeight(), trait.getViewLength(view));
  }

  @SmallTest
  public void testOnSingleTapUp()
      throws IllegalArgumentException, SecurityException {
    CandidateWordView candidateWordView =
        new StubCandidateWordView(getInstrumentation().getTargetContext(),
                                  CandidateWordView.Orientation.VERTICAL);
    CandidateSelectListener candidateSelectListener = createMock(CandidateSelectListener.class);
    candidateWordView.setCandidateSelectListener(candidateSelectListener);
    CandidateLayout mockLayout = MozcLayoutUtil.createCandidateLayoutMock(getMockSupport());
    candidateWordView.calculatedLayout = mockLayout;

    // onCandidateSelected() is never be called back.
    List<MotionEvent> events = new ArrayList<MotionEvent>();

    try {
      expect(mockLayout.getRowList()).andStubReturn(ROW_DATA);
      replayAll();
      events.add(MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, Integer.MIN_VALUE, 0));
      assertTrue(candidateWordView.onTouchEvent(events.get(events.size() - 1)));
      events.add(MotionEvent.obtain(0, 1000, MotionEvent.ACTION_UP, 0, Integer.MIN_VALUE, 0));
      assertTrue(candidateWordView.onTouchEvent(events.get(events.size() - 1)));
      verifyAll();

      resetAll();
      expect(mockLayout.getRowList()).andStubReturn(ROW_DATA);
      replayAll();
      events.add(MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, Integer.MAX_VALUE, 0));
      assertTrue(candidateWordView.onTouchEvent(events.get(events.size() - 1)));
      events.add(MotionEvent.obtain(0, 1000, MotionEvent.ACTION_UP, 0, Integer.MAX_VALUE, 0));
      assertTrue(candidateWordView.onTouchEvent(events.get(events.size() - 1)));
      verifyAll();

      resetAll();
      expect(mockLayout.getRowList()).andStubReturn(ROW_DATA);
      replayAll();
      events.add(MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, Integer.MIN_VALUE, 0, 0));
      assertTrue(candidateWordView.onTouchEvent(events.get(events.size() - 1)));
      events.add(MotionEvent.obtain(0, 1000, MotionEvent.ACTION_UP, Integer.MIN_VALUE, 0, 0));
      assertTrue(candidateWordView.onTouchEvent(events.get(events.size() - 1)));
      verifyAll();

      resetAll();
      expect(mockLayout.getRowList()).andStubReturn(ROW_DATA);
      replayAll();
      events.add(MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, Integer.MAX_VALUE, 0, 0));
      assertTrue(candidateWordView.onTouchEvent(events.get(events.size() - 1)));
      events.add(MotionEvent.obtain(0, 1000, MotionEvent.ACTION_UP, Integer.MAX_VALUE, 0, 0));
      assertTrue(candidateWordView.onTouchEvent(events.get(events.size() - 1)));
      verifyAll();

      // TouchUp on a candidate.
      resetAll();
      expect(mockLayout.getRowList()).andStubReturn(ROW_DATA);
      candidateSelectListener.onCandidateSelected(
          ROW_DATA.get(0).getSpanList().get(0).getCandidateWord());
      replayAll();
      events.add(MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 10, 10, 0));
      assertTrue(candidateWordView.onTouchEvent(events.get(events.size() - 1)));
      events.add(MotionEvent.obtain(0, 1000, MotionEvent.ACTION_UP, 10, 10, 0));
      assertTrue(candidateWordView.onTouchEvent(events.get(events.size() - 1)));
      verifyAll();

      // Slide within a candidate.
      resetAll();
      expect(mockLayout.getRowList()).andStubReturn(ROW_DATA);
      candidateSelectListener.onCandidateSelected(
          ROW_DATA.get(0).getSpanList().get(0).getCandidateWord());
      replayAll();
      events.add(MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 20, 10, 0));
      assertTrue(candidateWordView.onTouchEvent(events.get(events.size() - 1)));
      events.add(MotionEvent.obtain(0, 1000, MotionEvent.ACTION_MOVE, 29, 10, 0));
      assertTrue(candidateWordView.onTouchEvent(events.get(events.size() - 1)));
      events.add(MotionEvent.obtain(0, 2000, MotionEvent.ACTION_UP, 25, 10, 0));
      assertTrue(candidateWordView.onTouchEvent(events.get(events.size() - 1)));
      verifyAll();

      // Slide out of a candidate. No selection.
      resetAll();
      expect(mockLayout.getRowList()).andStubReturn(ROW_DATA);
      replayAll();
      events.add(MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 20, 10, 0));
      assertTrue(candidateWordView.onTouchEvent(events.get(events.size() - 1)));
      events.add(MotionEvent.obtain(0, 1000, MotionEvent.ACTION_MOVE, 30, 10, 0));
      assertTrue(candidateWordView.onTouchEvent(events.get(events.size() - 1)));
      events.add(MotionEvent.obtain(0, 2000, MotionEvent.ACTION_UP, 30, 10, 0));
      assertTrue(candidateWordView.onTouchEvent(events.get(events.size() - 1)));
      verifyAll();

      // Slide out of a candidate, then come back and release. No selection.
      resetAll();
      expect(mockLayout.getRowList()).andStubReturn(ROW_DATA);
      replayAll();
      events.add(MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 20, 10, 0));
      assertTrue(candidateWordView.onTouchEvent(events.get(events.size() - 1)));
      events.add(MotionEvent.obtain(0, 1000, MotionEvent.ACTION_MOVE, 31, 10, 0));
      assertTrue(candidateWordView.onTouchEvent(events.get(events.size() - 1)));
      events.add(MotionEvent.obtain(0, 2000, MotionEvent.ACTION_MOVE, 15, 10, 0));
      assertTrue(candidateWordView.onTouchEvent(events.get(events.size() - 1)));
      events.add(MotionEvent.obtain(0, 3000, MotionEvent.ACTION_UP, 15, 10, 0));
      assertTrue(candidateWordView.onTouchEvent(events.get(events.size() - 1)));
      verifyAll();

      // Must consider scroll value.
      resetAll();
      expect(mockLayout.getRowList()).andStubReturn(ROW_DATA);
      replayAll();
      candidateWordView.scrollTo(0, -5);
      events.add(MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 10, 10, 0));
      assertTrue(candidateWordView.onTouchEvent(events.get(events.size() - 1)));
      events.add(MotionEvent.obtain(0, 1000, MotionEvent.ACTION_UP, 10, 10, 0));
      assertTrue(candidateWordView.onTouchEvent(events.get(events.size() - 1)));
      verifyAll();

      resetAll();
      expect(mockLayout.getRowList()).andStubReturn(ROW_DATA);
      replayAll();
      candidateWordView.scrollTo(-5, 0);
      events.add(MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 10, 10, 0));
      assertTrue(candidateWordView.onTouchEvent(events.get(events.size() - 1)));
      events.add(MotionEvent.obtain(0, 1000, MotionEvent.ACTION_UP, 10, 10, 0));
      assertTrue(candidateWordView.onTouchEvent(events.get(events.size() - 1)));
      verifyAll();
    } finally {
      for (MotionEvent event : events) {
        event.recycle();
      }
    }
  }

  @SmallTest
  public void testUpdate_noLayouter() {
    CandidateWordView candidateWordView = new StubCandidateWordView(
        getInstrumentation().getTargetContext(), Orientation.VERTICAL);
    assertNull(candidateWordView.getCandidateLayouter());
    candidateWordView.update(CANDIDATE_LIST);
    assertEquals(CANDIDATE_LIST, candidateWordView.currentCandidateList);
    assertNull(candidateWordView.calculatedLayout);
  }

  @SmallTest
  public void testUpdate_successPath() {
    CandidateWordView candidateWordView = new StubCandidateWordView(
        getInstrumentation().getTargetContext(), Orientation.VERTICAL);
    CandidateLayouter layouter = createMock(CandidateLayouter.class);
    expect(layouter.layout(isA(CandidateList.class)))
        .andReturn(MozcLayoutUtil.createNiceCandidateLayoutMock(getMockSupport()));
    candidateWordView.layouter = layouter;
    expect(layouter.getPageHeight()).andReturn(50);
    replayAll();

    candidateWordView.updateLayouter();
    assertNotNull(candidateWordView.layouter);  // Pre-condition
    candidateWordView.update(CANDIDATE_LIST);

    verifyAll();
    assertEquals(CANDIDATE_LIST, candidateWordView.currentCandidateList);
    assertNotNull(candidateWordView.calculatedLayout);
  }

  @SmallTest
  public void testUpdate_identicalOutput() {
    CandidateWordView candidateWordView = new StubCandidateWordView(
        getInstrumentation().getTargetContext(), Orientation.VERTICAL);
    CandidateLayouter layouter = createMock(CandidateLayouter.class);
    expect(layouter.layout(isA(CandidateList.class)))
        .andStubReturn(MozcLayoutUtil.createNiceCandidateLayoutMock(getMockSupport()));
    expect(layouter.getPageHeight()).andStubReturn(50);
    candidateWordView.layouter = layouter;
    replayAll();

    candidateWordView.updateLayouter();
    assertNotNull(candidateWordView.layouter);  // Pre-condition
    // If the same CandidateList is passed, post-condition should not be changed.
    candidateWordView.update(CANDIDATE_LIST);
    candidateWordView.update(CANDIDATE_LIST);

    verifyAll();
    assertEquals(CANDIDATE_LIST, candidateWordView.currentCandidateList);
    assertNotNull(candidateWordView.calculatedLayout);
  }

  @SmallTest
  public void testUpdate_null() {
    CandidateWordView candidateWordView = new StubCandidateWordView(
        getInstrumentation().getTargetContext(), Orientation.VERTICAL);
    CandidateLayouter layouter = createMock(CandidateLayouter.class);
    expect(layouter.layout(isA(CandidateList.class)))
        .andStubReturn(MozcLayoutUtil.createNiceCandidateLayoutMock(getMockSupport()));
    expect(layouter.getPageHeight()).andReturn(50);
    candidateWordView.layouter = layouter;

    replayAll();

    candidateWordView.updateLayouter();
    assertNotNull(candidateWordView.layouter);
    candidateWordView.update(CANDIDATE_LIST);

    // Pre-condition
    verifyAll();
    assertNotNull(candidateWordView.calculatedLayout);

    resetAll();
    replayAll();

    // Update with null should reset calculatedLayout.
    candidateWordView.update(null);

    verifyAll();
    assertNull(candidateWordView.calculatedLayout);
  }

  @SmallTest
  public void testGetUpdatedScrollPosition() {
    class TestData extends Parameter {
      final float candidatePosition;
      final int scrollPosition;
      final float candidateLength;
      final int viewLength;
      final int expected;

      TestData(int scrollPosition, float candidatePosition, float candidateLength, int viewLength,
          int expected) {
        this.scrollPosition = scrollPosition;
        this.candidatePosition = candidatePosition;
        this.candidateLength = candidateLength;
        this.viewLength = viewLength;
        this.expected = expected;
      }
    }
    TestData[] testDataList = {
        // Should scroll to forward direction.
        new TestData(100, 50, 0, 0, 50),
        // Should scroll to backward direction.
        new TestData(0, 50, 100, 100, 50),
        // Stay here.
        new TestData(50, 50, 100, 100, 50),
        new TestData(0, 0, 0, 0, 0),
    };

    CandidateWordView.OrientationTrait trait =
        createMock(CandidateWordView.OrientationTrait.class);
    CandidateWordView candidateWordView = new StubCandidateWordView(
        getInstrumentation().getTargetContext(), trait);
    for (TestData testData : testDataList) {
      Row row = new Row();
      Span span = MozcLayoutUtil.createEmptySpan();

      resetAll();
      expect(trait.getScrollPosition(candidateWordView)).andReturn(testData.scrollPosition);
      expect(trait.getCandidatePosition(row, span)).andReturn(testData.candidatePosition);
      expect(trait.getCandidateLength(row, span)).andReturn(testData.candidateLength);
      expect(trait.getViewLength(candidateWordView)).andStubReturn(testData.viewLength);
      replayAll();
      assertEquals(
          testData.toString(),
          testData.expected,
          candidateWordView.getUpdatedScrollPosition(row, span));
      verifyAll();
    }
  }

  @SmallTest
  public void testUpdateScrollPositionBasedOnFocusedIndex() {
    SnapScroller scroller = createMock(SnapScroller.class);
    OrientationTrait trait = createMock(CandidateWordView.OrientationTrait.class);
    CandidateWordView candidateWordView = new StubCandidateWordView(
        getInstrumentation().getTargetContext(), trait);
    VisibilityProxy.setField(candidateWordView, "scroller", scroller);
    candidateWordView.currentCandidateList =
        CandidateList.newBuilder().setFocusedIndex(Integer.MAX_VALUE).build();
    CandidateLayout mockLayout = MozcLayoutUtil.createCandidateLayoutMock(getMockSupport());

    // No layout is set
    scroller.scrollTo(0);
    expect(scroller.getScrollPosition()).andReturn(0);
    trait.scrollTo(same(candidateWordView), eq(0));
    replayAll();
    candidateWordView.updateScrollPositionBasedOnFocusedIndex();
    verifyAll();

    // No correspoinding candidate
    resetAll();
    expect(mockLayout.getRowList()).andStubReturn(ROW_DATA);
    scroller.scrollTo(0);
    expect(scroller.getScrollPosition()).andReturn(0);
    trait.scrollTo(same(candidateWordView), eq(0));
    replayAll();
    candidateWordView.calculatedLayout = mockLayout;
    candidateWordView.scrollTo(100, 100);
    candidateWordView.updateScrollPositionBasedOnFocusedIndex();
    verifyAll();

    // Correspoinding candidate is found
    candidateWordView.currentCandidateList =
        CandidateList.newBuilder().setFocusedIndex(0).build();
    resetAll();
    expect(trait.getScrollPosition(candidateWordView)).andStubReturn(0);
    expect(trait.getCandidatePosition(isA(Row.class), isA(Span.class))).andReturn(0f);
    expect(trait.getCandidateLength(isA(Row.class), isA(Span.class))).andReturn(0f);
    expect(trait.getViewLength(candidateWordView)).andReturn(0);
    expect(mockLayout.getRowList()).andStubReturn(ROW_DATA);
    scroller.scrollTo(anyInt());
    expect(scroller.getScrollPosition()).andReturn(10);
    trait.scrollTo(same(candidateWordView), eq(10));
    replayAll();
    candidateWordView.calculatedLayout = mockLayout;
    candidateWordView.updateScrollPositionBasedOnFocusedIndex();
    verifyAll();
  }

  @SuppressLint("WrongCall")
  @SmallTest
  public void testOnLayout() {
    CandidateWordView candWordView = new StubCandidateWordView(
        getInstrumentation().getTargetContext(), Orientation.VERTICAL);
    CandidateLayouter layouter = createMock(CandidateLayouter.class);
    candWordView.layouter = layouter;
    replayAll();

    // Check pre-condition.
    assertNull(candWordView.calculatedLayout);
    assertNull(candWordView.currentCandidateList);

    verifyAll();

    // Call onLayout with empty view size.
    resetAll();
    expect(layouter.setViewSize(0, 0)).andReturn(false);
    replayAll();

    candWordView.currentCandidateList = CANDIDATE_LIST;
    candWordView.onLayout(true, 0, 0, 0, 0);

    verifyAll();
    assertNull(candWordView.calculatedLayout);
    assertNotNull(candWordView.currentCandidateList);

    // Call onLayout with non-empty view size.
    resetAll();
    expect(layouter.setViewSize(320, 240)).andReturn(true);
    expect(layouter.layout(same(CANDIDATE_LIST)))
        .andReturn(MozcLayoutUtil.createNiceCandidateLayoutMock(getMockSupport()));
    expect(layouter.getPageHeight()).andReturn(50);
    replayAll();

    candWordView.onLayout(true, 0, 0, 320, 240);

    verifyAll();
    assertNotNull(candWordView.calculatedLayout);
    assertNotNull(candWordView.currentCandidateList);
  }
}
