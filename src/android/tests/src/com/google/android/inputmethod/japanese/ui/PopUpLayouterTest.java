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

import static org.easymock.EasyMock.capture;
import static org.easymock.EasyMock.eq;
import static org.easymock.EasyMock.expect;
import static org.easymock.EasyMock.expectLastCall;
import static org.easymock.EasyMock.getCurrentArguments;
import static org.easymock.EasyMock.isA;

import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;

import android.content.Context;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.View;
import android.widget.FrameLayout;

import org.easymock.Capture;
import org.easymock.IAnswer;

/**
 * Test for PopUpLayouter class.
 */
public class PopUpLayouterTest extends InstrumentationTestCaseWithMock {

  @SmallTest
  public void testBasicLayout() {
    Context context = getInstrumentation().getTargetContext();
    FrameLayout screenContentLayout = createMockBuilder(FrameLayout.class)
        .withConstructor(context)
        .createMock();
    View rootView = createViewMock(View.class);
    View parentView = createViewMock(View.class);
    View popUpContentView = createViewMock(View.class);

    IAnswer<Void> getLocationInWindowAnswer = new IAnswer<Void>() {
      @Override
      public Void answer() {
        int[] location = int[].class.cast(getCurrentArguments()[0]);
        location[0] = 100;
        location[1] = 200;
        return null;
      }
    };

    Capture<FrameLayout.LayoutParams> layoutCapture = new Capture<FrameLayout.LayoutParams>();

    resetAll();
    expect(rootView.findViewById(android.R.id.content)).andStubReturn(
        View.class.cast(screenContentLayout));
    screenContentLayout.addView(eq(popUpContentView), isA(FrameLayout.LayoutParams.class));
    expect(parentView.getRootView()).andStubReturn(View.class.cast(rootView));
    parentView.getLocationInWindow(isA(int[].class));
    expectLastCall().andAnswer(getLocationInWindowAnswer);
    expect(popUpContentView.getLayoutParams()).andStubReturn(
        new FrameLayout.LayoutParams(1000, 1000));
    popUpContentView.setLayoutParams(capture(layoutCapture));
    replayAll();
    PopUpLayouter<View> layouter = new PopUpLayouter<View>(parentView, popUpContentView);
    layouter.setBounds(110, 220, 330, 550);
    verifyAll();

    assertEquals(popUpContentView, layouter.getContentView());
    assertEquals(210, layoutCapture.getValue().leftMargin);
    assertEquals(420, layoutCapture.getValue().topMargin);
    assertEquals(220, layoutCapture.getValue().width);
    assertEquals(330, layoutCapture.getValue().height);
  }
}
