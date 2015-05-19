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

import static org.easymock.EasyMock.expect;
import static org.easymock.EasyMock.isA;

import org.mozc.android.inputmethod.japanese.CandidateView.ConversionCandidateSelectListener;
import org.mozc.android.inputmethod.japanese.CandidateView.ConversionCandidateWordView;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateList;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateWord;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;
import org.mozc.android.inputmethod.japanese.testing.MozcLayoutUtil;
import org.mozc.android.inputmethod.japanese.ui.CandidateLayout;
import org.mozc.android.inputmethod.japanese.ui.CandidateLayout.Row;
import org.mozc.android.inputmethod.japanese.ui.CandidateLayouter;
import org.mozc.android.inputmethod.japanese.ui.ScrollGuideView;

import android.graphics.drawable.Drawable;
import android.test.suitebuilder.annotation.SmallTest;
import android.widget.ToggleButton;

import java.util.Collections;

/**
 */
public class CandidateViewTest extends InstrumentationTestCaseWithMock {
  @SmallTest
  public void testConversionCandidateSelectListener() {
    ViewEventListener viewEventListener = createMock(ViewEventListener.class);
    ConversionCandidateSelectListener conversionCandidateSelectListener =
        new ConversionCandidateSelectListener(viewEventListener);

    viewEventListener.onConversionCandidateSelected(0);
    replayAll();

    conversionCandidateSelectListener.onCandidateSelected(
        CandidateWord.newBuilder().setId(0).buildPartial());

    verifyAll();
  }

  @SmallTest
  public void testOnFinishInflate() {
    ConversionCandidateWordView mockCandidateWordView =
        new ConversionCandidateWordView(getInstrumentation().getTargetContext(), null);
    ScrollGuideView mockScrollGuideView = createViewMock(ScrollGuideView.class);
    ToggleButton mockInputFrameFoldButton = createViewMockBuilder(ToggleButton.class)
        .addMockedMethod("setBackgroundDrawable")
        .createMock();
    CandidateView candidateView = createViewMockBuilder(CandidateView.class)
        .addMockedMethods(
            "getConversionCandidateWordView", "getScrollGuideView", "getInputFrameFoldButton")
        .createMock();
    expect(candidateView.getConversionCandidateWordView()).andStubReturn(mockCandidateWordView);
    expect(candidateView.getScrollGuideView()).andStubReturn(mockScrollGuideView);
    expect(candidateView.getInputFrameFoldButton()).andStubReturn(mockInputFrameFoldButton);

    mockScrollGuideView.setScroller(mockCandidateWordView.scroller);
    mockInputFrameFoldButton.setBackgroundDrawable(isA(Drawable.class));
    replayAll();

    candidateView.onFinishInflate();

    verifyAll();
  }

  @SmallTest
  public void testCandidateWordView_update() {
    ConversionCandidateWordView candidateWordView =
        new ConversionCandidateWordView(getInstrumentation().getTargetContext(), null);
    candidateWordView.setCandidateTextDimension(1, 1);
    candidateWordView.layout(0, 0, 320, 240);
    candidateWordView.scrollTo(100, 100);

    // Setup layouter.
    CandidateList candidateList = CandidateList.newBuilder()
        .addCandidates(CandidateWord.newBuilder().setId(0).setValue("cand1"))
        .addCandidates(CandidateWord.newBuilder().setId(1).setValue("cand2"))
        .build();
    candidateWordView.update(candidateList);

    // Check postcondition.
    assertEquals(0, candidateWordView.getScrollX());
    assertEquals(0, candidateWordView.getScrollY());
    assertNotNull(candidateWordView.calculatedLayout);
    assertSame(candidateList, candidateWordView.currentCandidateList);
  }

  @SmallTest
  public void testCandidateWordView_updateZeroRow() {
    ConversionCandidateWordView candidateWordView =
        new ConversionCandidateWordView(getInstrumentation().getTargetContext(), null);

    // Setup precondition, which should be overriden by calling update method.
    candidateWordView.scrollTo(100, 100);

    // Setup mock layouter.
    CandidateLayout layout = MozcLayoutUtil.createNiceCandidateLayoutMock(getMockSupport());
    expect(layout.getRowList()).andStubReturn(Collections.<Row>emptyList());
    CandidateLayouter layouter = createMock(CandidateLayouter.class);
    expect(layouter.layout(isA(CandidateList.class))).andReturn(layout);
    expect(layouter.getPageHeight()).andReturn(100);
    candidateWordView.layouter = layouter;
    replayAll();

    candidateWordView.update(CandidateList.getDefaultInstance());

    // Check postcondition.
    verifyAll();
    assertEquals(0, candidateWordView.getScrollX());
    assertEquals(0, candidateWordView.getScrollY());
    assertNotNull(candidateWordView.calculatedLayout);
    assertSame(CandidateList.getDefaultInstance(), candidateWordView.currentCandidateList);
  }

  @SmallTest
  public void testCandidateWordView_updateDoNothing() {
    ConversionCandidateWordView candidateWordView =
        new ConversionCandidateWordView(getInstrumentation().getTargetContext(), null);

    // Setup precondition
    candidateWordView.scrollTo(100, 100);

    // Clear the layouter.
    candidateWordView.layouter = null;

    // Calling before setting up a layouter.
    candidateWordView.update(CandidateList.getDefaultInstance());
    assertEquals(0, candidateWordView.getScrollX());
    assertEquals(0, candidateWordView.getScrollY());
    assertNull(candidateWordView.calculatedLayout);

    // Set currentCandidateList field.
    candidateWordView.currentCandidateList = CandidateList.getDefaultInstance();

    // Setup precondition
    candidateWordView.scrollTo(100, 100);

    // Calling with the same value as previous.
    candidateWordView.update(CandidateList.getDefaultInstance());
    assertEquals(0, candidateWordView.getScrollX());
    assertEquals(0, candidateWordView.getScrollY());
    assertNull(candidateWordView.calculatedLayout);
  }
}
