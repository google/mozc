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

package org.mozc.android.inputmethod.japanese.keyboard;

import static org.easymock.EasyMock.expect;
import static org.easymock.EasyMock.expectLastCall;
import static org.easymock.EasyMock.getCurrentArguments;
import static org.easymock.EasyMock.isA;

import org.mozc.android.inputmethod.japanese.keyboard.Key.Stick;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;
import org.mozc.android.inputmethod.japanese.testing.VisibilityProxy;
import org.mozc.android.inputmethod.japanese.view.DrawableCache;
import org.mozc.android.inputmethod.japanese.view.MozcDrawableFactory;

import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.test.mock.MockResources;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.Gravity;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;

import org.easymock.IAnswer;

import java.util.Collections;

/**
 */
public class PopUpPreviewTest extends InstrumentationTestCaseWithMock {

  @SmallTest
  public void testShow() {
    Drawable icon = new ColorDrawable();
    int iconResourceId = 1;

    DrawableCache cache = createMockBuilder(DrawableCache.class)
        .withConstructor(MozcDrawableFactory.class)
        .withArgs(new MozcDrawableFactory(new MockResources()))
        .createMock();
    expect(cache.getDrawable(iconResourceId)).andStubReturn(icon);

    PopUp popup = new PopUp(iconResourceId, 40, 80, 0, -30);
    Key key = new Key(5, 10, 30, 20, 0, 0, false, false, false, Stick.EVEN,
                      Collections.<KeyState>emptyList());

    IAnswer<Void> getLocationInWindowAnswer = new IAnswer<Void>() {
      @Override
      public Void answer() {
        int[] location = int[].class.cast(getCurrentArguments()[0]);
        location[0] = 100;
        location[1] = 200;
        return null;
      }
    };

    View mockView = createViewMock(View.class);
    mockView.getLocationInWindow(isA(int[].class));
    expectLastCall().andAnswer(getLocationInWindowAnswer);
    expect(mockView.getRootView()).andStubReturn(null);

    FrameLayout.LayoutParams layoutParams = createMockBuilder(FrameLayout.LayoutParams.class)
        .withConstructor(int.class, int.class, int.class)
        .withArgs(0, 0, Gravity.LEFT | Gravity.TOP)
        .createMock();
    layoutParams.setMargins(100, 150, 0, 0);
    replayAll();

    PopUpPreview preview = new PopUpPreview(mockView, new BackgroundDrawableFactory(1f), cache);
    ImageView popupView = VisibilityProxy.getField(preview, "popupView");

    preview.showIfNecessary(null, popup);
    assertEquals(View.GONE, popupView.getVisibility());
    assertNull(popupView.getDrawable());
    assertNull(popupView.getBackground());

    preview.showIfNecessary(key, null);
    assertEquals(View.GONE, popupView.getVisibility());
    assertNull(popupView.getDrawable());
    assertNull(popupView.getBackground());

    popupView.setLayoutParams(layoutParams);

    preview.showIfNecessary(key, popup);

    verifyAll();
    assertEquals(View.VISIBLE, popupView.getVisibility());
    assertSame(icon, popupView.getDrawable());
    assertNotNull(popupView.getBackground());
    assertEquals(40, layoutParams.width);
    assertEquals(80, layoutParams.height);
  }

  @SmallTest
  public void testDismiss() {
    PopUpPreview preview =
        new PopUpPreview(new View(getInstrumentation().getTargetContext()), null, null);
    preview.dismiss();
    ImageView popupView = VisibilityProxy.getField(preview, "popupView");
    assertEquals(View.GONE, popupView.getVisibility());
    assertNull(popupView.getDrawable());
    assertNull(popupView.getBackground());
  }
}
