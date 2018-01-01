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

package org.mozc.android.inputmethod.japanese;

import static org.easymock.EasyMock.expect;

import org.mozc.android.inputmethod.japanese.FeedbackManager.FeedbackEvent;
import org.mozc.android.inputmethod.japanese.hardwarekeyboard.HardwareKeyboard.CompositionSwitchMode;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;
import org.mozc.android.inputmethod.japanese.view.DummyDrawable;
import org.mozc.android.inputmethod.japanese.view.MozcImageView;
import org.mozc.android.inputmethod.japanese.view.Skin;
import org.mozc.android.inputmethod.japanese.view.SkinType;

import android.content.Context;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.View;
import android.view.View.OnClickListener;

/** Test for narrow frame view. */
public class NarrowFrameViewTest extends InstrumentationTestCaseWithMock {

  @SmallTest
  public void testSetEventListener() {
    Context context = getInstrumentation().getTargetContext();
    NarrowFrameView narrowFrameView = createViewMockBuilder(NarrowFrameView.class)
        .addMockedMethods("getHardwareCompositionButton", "getWidenButton")
        .createMock();

    ViewEventListener viewEventListener = createMock(ViewEventListener.class);
    OnClickListener widenButtonClickListener = createMock(OnClickListener.class);
    MozcImageView hardwareCompositionButton = new MozcImageView(context);
    MozcImageView widenButton = new MozcImageView(context);

    resetAll();
    expect(narrowFrameView.getHardwareCompositionButton()).andStubReturn(hardwareCompositionButton);
    expect(narrowFrameView.getWidenButton()).andStubReturn(widenButton);
    replayAll();
    narrowFrameView.setEventListener(viewEventListener, widenButtonClickListener);
    verifyAll();

    resetAll();
    viewEventListener.onFireFeedbackEvent(
        FeedbackEvent.NARROW_FRAME_HARDWARE_COMPOSITION_BUTTON_DOWN);
    viewEventListener.onHardwareKeyboardCompositionModeChange(CompositionSwitchMode.TOGGLE);
    replayAll();
    hardwareCompositionButton.performClick();
    verifyAll();

    resetAll();
    widenButtonClickListener.onClick(widenButton);
    replayAll();
    widenButton.performClick();
    verifyAll();
  }

  @SuppressWarnings("deprecation")
  @SmallTest
  public void testSetSkinType() {
    Context context = getInstrumentation().getTargetContext();
    NarrowFrameView narrowFrameView = createViewMockBuilder(NarrowFrameView.class)
        .addMockedMethods("setBackgroundDrawable", "getHardwareCompositionButton", "getWidenButton",
                          "getNarrowFrameSeparator")
        .createMock();
    MozcImageView hardwareCompositionButton = new MozcImageView(context);
    MozcImageView widenButton = new MozcImageView(context);
    Skin skin = SkinType.MATERIAL_DESIGN_LIGHT.getSkin(context.getResources());

    resetAll();
    expect(narrowFrameView.getHardwareCompositionButton()).andStubReturn(hardwareCompositionButton);
    expect(narrowFrameView.getWidenButton()).andStubReturn(widenButton);
    expect(narrowFrameView.getNarrowFrameSeparator()).andStubReturn(new View(context));
    narrowFrameView.setBackgroundDrawable(skin.narrowFrameBackgroundDrawable);
    replayAll();
    narrowFrameView.setSkin(skin);
    verifyAll();

    assertNotSame(DummyDrawable.getInstance(), hardwareCompositionButton.getDrawable());
    assertNotSame(DummyDrawable.getInstance(), widenButton.getDrawable());
    assertNotSame(hardwareCompositionButton.getDrawable(), widenButton.getDrawable());
  }
}
