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

package org.mozc.android.inputmethod.japanese.keyboard;

import static org.easymock.EasyMock.expect;
import static org.easymock.EasyMock.expectLastCall;
import static org.easymock.EasyMock.getCurrentArguments;
import static org.easymock.EasyMock.isA;

import org.mozc.android.inputmethod.japanese.keyboard.BackgroundDrawableFactory.DrawableType;
import org.mozc.android.inputmethod.japanese.keyboard.Key.Stick;
import org.mozc.android.inputmethod.japanese.resources.R;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;
import org.mozc.android.inputmethod.japanese.testing.MockResourcesWithDisplayMetrics;
import org.mozc.android.inputmethod.japanese.view.DrawableCache;
import com.google.common.base.Optional;

import android.content.res.Resources;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.test.suitebuilder.annotation.SmallTest;
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
    int longTapIconResourceId = 2;

    Resources resources = getInstrumentation().getTargetContext().getResources();
    DrawableCache cache = createMockBuilder(DrawableCache.class)
        .withConstructor(Resources.class)
        .withArgs(resources)
        .createMock();
    expect(cache.getDrawable(iconResourceId)).andStubReturn(Optional.of(icon));

    int popupHeight = 80;
    PopUp popup = new PopUp(iconResourceId, longTapIconResourceId, popupHeight, 0, -30, 10, 10);
    Key key = new Key(5, 10, 30, 20, 0, 0, false, false, Stick.EVEN,
                      DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND,
                      Collections.<KeyState>emptyList());

    final int parentLocationX = 100;
    final int parentLocationY = 200;

    IAnswer<Void> getLocationInWindowAnswer = new IAnswer<Void>() {
      @Override
      public Void answer() {
        int[] location = int[].class.cast(getCurrentArguments()[0]);
        location[0] = parentLocationX;
        location[1] = parentLocationY;
        return null;
      }
    };

    View mockView = createViewMock(View.class);
    mockView.getLocationInWindow(isA(int[].class));
    expectLastCall().andAnswer(getLocationInWindowAnswer);
    expect(mockView.getRootView()).andStubReturn(null);

    replayAll();

    PopUpPreview preview =
        new PopUpPreview(mockView, new BackgroundDrawableFactory(resources), cache);
    ImageView popupView = preview.popUp.getContentView();

    preview.showIfNecessary(key, Optional.<PopUp>absent(), false);
    assertEquals(View.GONE, popupView.getVisibility());
    assertNull(popupView.getDrawable());
    assertNull(popupView.getBackground());

    FrameLayout.LayoutParams layoutParams = new FrameLayout.LayoutParams(0, 0);
    popupView.setLayoutParams(layoutParams);
    preview.showIfNecessary(key, Optional.of(popup), false);
    verifyAll();
    assertEquals(View.VISIBLE, popupView.getVisibility());
    assertSame(icon, popupView.getDrawable());
    assertNotNull(popupView.getBackground());
    float density = resources.getDisplayMetrics().density;
    int popupWindowPadding = (int) (BackgroundDrawableFactory.POPUP_WINDOW_PADDING * density);
    assertEquals(Math.min(key.getWidth(),
                          resources.getDimensionPixelSize(R.dimen.popup_width_limitation))
                     + popupWindowPadding * 2,
                 layoutParams.width);
    assertEquals(popupHeight + popupWindowPadding * 2, layoutParams.height);
    assertEquals(key.getX() + key.getWidth() / 2 + popup.getXOffset() - layoutParams.width / 2
                     + parentLocationX,
                 layoutParams.leftMargin);
    assertEquals(key.getY() + key.getHeight() / 2 + popup.getYOffset() - popup.getHeight() / 2
                     + parentLocationY - popupWindowPadding,
                 layoutParams.topMargin);
    assertEquals(0, layoutParams.rightMargin);
    assertEquals(0, layoutParams.bottomMargin);
  }

  @SmallTest
  public void testDismiss() {
    PopUpPreview preview = new PopUpPreview(
            new View(getInstrumentation().getTargetContext()),
            new BackgroundDrawableFactory(new MockResourcesWithDisplayMetrics()),
            new DrawableCache(new MockResourcesWithDisplayMetrics()));
    preview.dismiss();
    ImageView popupView = preview.popUp.getContentView();
    assertEquals(View.GONE, popupView.getVisibility());
    assertNull(popupView.getDrawable());
    assertNull(popupView.getBackground());
  }

  @SmallTest
  public void testShowIfNecessaryWithInvalidResourceId() {
    Drawable icon = new ColorDrawable();
    int invalidResourceId = 0;
    int validResourceId = 1;

    DrawableCache cache = createMockBuilder(DrawableCache.class)
        .withConstructor(Resources.class)
        .withArgs(new MockResourcesWithDisplayMetrics())
        .createMock();
    expect(cache.getDrawable(invalidResourceId)).andStubReturn(Optional.<Drawable>absent());
    expect(cache.getDrawable(validResourceId)).andStubReturn(Optional.of(icon));

    PopUpPreview preview = new PopUpPreview(
        new View(getInstrumentation().getTargetContext()),
        new BackgroundDrawableFactory(new MockResourcesWithDisplayMetrics()), cache);
    ImageView popupView = preview.popUp.getContentView();

    replayAll();

    for (boolean useValidIconResourceId : new boolean[] {false, true}) {
      for (boolean useValidLongTapIconResourceId : new boolean[] {false, true}) {
        int iconId = useValidIconResourceId ? validResourceId : invalidResourceId;
        int longTapIconId = useValidLongTapIconResourceId ? validResourceId : invalidResourceId;
        Key key = new Key(
            10, 10, 10, 10, 0, 0, false, false, Stick.EVEN,
            DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND, Collections.<KeyState>emptyList());

        preview.showIfNecessary(
            key, Optional.of(new PopUp(iconId, longTapIconId, 10, 0, 0, 10, 10)), false);
        assertEquals(useValidIconResourceId, popupView.getVisibility() == View.VISIBLE);

        preview.showIfNecessary(
            key, Optional.of(new PopUp(iconId, longTapIconId, 10, 0, 0, 10, 10)), true);
        assertEquals(useValidLongTapIconResourceId, popupView.getVisibility() == View.VISIBLE);
      }
    }
  }
}
