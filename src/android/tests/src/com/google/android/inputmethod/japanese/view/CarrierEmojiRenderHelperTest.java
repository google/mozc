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

package org.mozc.android.inputmethod.japanese.view;

import static org.easymock.EasyMock.anyInt;
import static org.easymock.EasyMock.eq;
import static org.easymock.EasyMock.expect;
import static org.easymock.EasyMock.same;

import org.mozc.android.inputmethod.japanese.emoji.EmojiProviderType;
import org.mozc.android.inputmethod.japanese.emoji.EmojiUtil;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateList;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateWord;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;
import org.mozc.android.inputmethod.japanese.view.CarrierEmojiRenderHelper.BackgroundTextView;
import com.google.common.base.Optional;

import android.os.Handler;
import android.test.suitebuilder.annotation.SmallTest;
import android.util.TypedValue;
import android.view.View;

import org.easymock.IMockBuilder;

import java.util.Collections;
import java.util.Set;

/**
 */
public class CarrierEmojiRenderHelperTest extends InstrumentationTestCaseWithMock {

  private static final int CODE_POINT_IMODE = 0xFEE10;
  private static final int CODE_POINT_EZMARK = 0xFEE40;

  private static final Set<EmojiProviderType> DOCOMO_PROVIDER_SET =
      Collections.singleton(EmojiProviderType.DOCOMO);

  private static String toString(int codePoint) {
    return new String(new int[] {codePoint}, 0, 1);
  }

  private IMockBuilder<BackgroundTextView> createBackgroundTextViewMockBuilder() {
    return createMockBuilder(BackgroundTextView.class)
        .withConstructor(View.class)
        .withArgs(createViewMock(View.class));
  }

  private BackgroundTextView createBackgroundTextViewMock() {
    return createBackgroundTextViewMockBuilder().createMock();
  }

  private BackgroundTextView createBackgroundTextViewNiceMock() {
    return createBackgroundTextViewMockBuilder().createNiceMock();
  }

  @SmallTest
  public void testBackgroundTextView_isShown() {
    View view = createViewMock(View.class);
    BackgroundTextView backgroundTextView = new BackgroundTextView(view);

    for (boolean isShown : new boolean[] {true, false}) {
      resetAll();
      expect(view.isShown()).andReturn(isShown);
      replayAll();

      assertEquals(isShown, backgroundTextView.isShown());

      verifyAll();
    }
  }

  @SmallTest
  public void testBackgroundTextView_invalidate() {
    View view = createViewMock(View.class);
    BackgroundTextView backgroundTextView = new BackgroundTextView(view);

    // By default invalidate should be proxy'ed.
    resetAll();
    view.invalidate();
    replayAll();

    backgroundTextView.invalidate();

    verifyAll();

    // Once lockInvalidate is invoked, invalidate shouldn't be proxy'ed.
    resetAll();
    replayAll();

    backgroundTextView.lockInvalidate();
    backgroundTextView.invalidate();

    verifyAll();

    // When unlockInvalidate is invoked, invalidate should be proxy'ed again.
    resetAll();
    view.invalidate();
    replayAll();

    backgroundTextView.unlockInvalidate();
    backgroundTextView.invalidate();

    verifyAll();
  }

  @SmallTest
  public void testBackgroundTextView_postInvalidate() {
    View view = createViewMock(View.class);
    BackgroundTextView backgroundTextView = new BackgroundTextView(view);

    resetAll();
    view.postInvalidate();
    replayAll();

    backgroundTextView.postInvalidate();

    verifyAll();
  }

  @SmallTest
  public void testBackgroundTextView_postDelayed() {
    View view = createViewMock(View.class);
    BackgroundTextView backgroundTextView = new BackgroundTextView(view);

    Runnable runnable = createNiceMock(Runnable.class);
    resetAll();
    expect(view.postDelayed(same(runnable), eq(100L))).andReturn(true);
    replayAll();

    assertTrue(backgroundTextView.postDelayed(runnable, 100L));

    verifyAll();
  }

  @SmallTest
  public void testBackgroundTextView_getHandler() {
    View view = createViewMock(View.class);
    BackgroundTextView backgroundTextView = new BackgroundTextView(view);

    Handler handler = new Handler();
    resetAll();
    expect(view.getHandler()).andReturn(handler);
    replayAll();

    assertSame(handler, backgroundTextView.getHandler());

    verifyAll();
  }

  @SmallTest
  public void testIsSystemSupportedEmoji() {
    CarrierEmojiRenderHelper helper = new CarrierEmojiRenderHelper(
        DOCOMO_PROVIDER_SET, createBackgroundTextViewNiceMock());

    helper.setEmojiProviderType(EmojiProviderType.NONE);
    assertFalse(helper.isSystemSupportedEmoji());

    // Docomo is system supported.
    helper.setEmojiProviderType(EmojiProviderType.DOCOMO);
    assertTrue(helper.isSystemSupportedEmoji());

    // Others are not.
    helper.setEmojiProviderType(EmojiProviderType.SOFTBANK);
    assertFalse(helper.isSystemSupportedEmoji());
    helper.setEmojiProviderType(EmojiProviderType.KDDI);
    assertFalse(helper.isSystemSupportedEmoji());
  }

  @SmallTest
  public void testIsRenderableEmoji() {
    CarrierEmojiRenderHelper helper = new CarrierEmojiRenderHelper(
        DOCOMO_PROVIDER_SET, createBackgroundTextViewNiceMock());

    // For NONE provider type, any value won't be rendered.
    helper.setEmojiProviderType(EmojiProviderType.NONE);
    assertFalse(helper.isRenderableEmoji(toString(EmojiUtil.MIN_EMOJI_PUA_CODE_POINT)));

    // For system supported emoji, it'll depends on the EmojiData contents.
    helper.setEmojiProviderType(EmojiProviderType.DOCOMO);
    assertTrue(helper.isRenderableEmoji(toString(CODE_POINT_IMODE)));
    assertFalse(helper.isRenderableEmoji(toString(CODE_POINT_EZMARK)));
  }

  @SmallTest
  public void testSetCandidateList_empty() {
    BackgroundTextView backgroundTextView = createBackgroundTextViewMock();
    CarrierEmojiRenderHelper helper = new CarrierEmojiRenderHelper(
        DOCOMO_PROVIDER_SET, backgroundTextView);
    helper.setEmojiProviderType(EmojiProviderType.DOCOMO);

    resetAll();
    backgroundTextView.setText("");
    replayAll();

    helper.setCandidateList(Optional.<CandidateList>absent());

    verifyAll();

    resetAll();
    backgroundTextView.setText("");
    replayAll();

    helper.setCandidateList(Optional.of(CandidateList.getDefaultInstance()));

    verifyAll();
  }

  @SmallTest
  public void testSetCandidateList() {
    BackgroundTextView backgroundTextView = createBackgroundTextViewMock();
    CarrierEmojiRenderHelper helper = new CarrierEmojiRenderHelper(
        DOCOMO_PROVIDER_SET, backgroundTextView);
    helper.setEmojiProviderType(EmojiProviderType.DOCOMO);

    resetAll();
    backgroundTextView.setTextSize(TypedValue.COMPLEX_UNIT_PX, 14);
    backgroundTextView.setText(toString(CODE_POINT_IMODE) + "\n");
    expect(backgroundTextView.getLayoutParams()).andReturn(null);
    backgroundTextView.measureInternal(anyInt(), anyInt());
    replayAll();

    helper.setCandidateTextSize(14);
    helper.setCandidateList(Optional.of(CandidateList.newBuilder()
        .addCandidates(CandidateWord.newBuilder()
            .setId(0)
            .setValue("a"))
        .addCandidates(CandidateWord.newBuilder()
            .setId(1)
            .setValue(toString(CODE_POINT_IMODE)))
        .build()));

    verifyAll();
  }

  @SmallTest
  public void testSetCandidateList_nonSystemSupportedEmojiProviderType() {
    BackgroundTextView backgroundTextView = createBackgroundTextViewMock();
    CarrierEmojiRenderHelper helper = new CarrierEmojiRenderHelper(
        DOCOMO_PROVIDER_SET, backgroundTextView);
    helper.setEmojiProviderType(EmojiProviderType.SOFTBANK);

    resetAll();
    replayAll();

    helper.setCandidateTextSize(14);
    helper.setCandidateList(Optional.of(CandidateList.newBuilder()
        .addCandidates(CandidateWord.newBuilder()
            .setId(0)
            .setValue("a"))
        .addCandidates(CandidateWord.newBuilder()
            .setId(1)
            .setValue("b"))
        .build()));

    verifyAll();
  }

  @SmallTest
  public void testProxyMethod() {
    BackgroundTextView backgroundTextView = createBackgroundTextViewMock();
    CarrierEmojiRenderHelper helper = new CarrierEmojiRenderHelper(
        DOCOMO_PROVIDER_SET, backgroundTextView);

    resetAll();
    backgroundTextView.onAttachedToWindow();
    replayAll();

    helper.onAttachedToWindow();

    verifyAll();

    resetAll();
    backgroundTextView.onDetachedFromWindow();
    replayAll();

    helper.onDetachedFromWindow();

    verifyAll();
  }
}
