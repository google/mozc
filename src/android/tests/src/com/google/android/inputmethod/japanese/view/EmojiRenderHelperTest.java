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

package org.mozc.android.inputmethod.japanese.view;

import static org.easymock.EasyMock.anyInt;
import static org.easymock.EasyMock.anyObject;
import static org.easymock.EasyMock.eq;
import static org.easymock.EasyMock.expect;
import static org.easymock.EasyMock.expectLastCall;
import static org.easymock.EasyMock.same;

import org.mozc.android.inputmethod.japanese.emoji.EmojiProviderType;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateList;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateWord;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;
import org.mozc.android.inputmethod.japanese.testing.VisibilityProxy;
import org.mozc.android.inputmethod.japanese.view.EmojiRenderHelper.BackgroundTextView;

import android.content.res.Resources;
import android.os.Handler;
import android.test.mock.MockResources;
import android.util.TypedValue;
import android.view.View;

import java.util.Collections;
import java.util.Set;

/**
 */
public class EmojiRenderHelperTest extends InstrumentationTestCaseWithMock {

  private static String toString(int codePoint) {
    return new String(new int[] {codePoint}, 0, 1);
  }

  private Set<EmojiProviderType> originalRenderableEmojiProviderSet;

  @Override
  protected void setUp() throws Exception {
    super.setUp();

    // Inject RENDERABLE_EMOJI_PROVIDER_SET for testing as it's system dependent.
    originalRenderableEmojiProviderSet =
        VisibilityProxy.getStaticField(EmojiRenderHelper.class, "RENDERABLE_EMOJI_PROVIDER_SET");
    VisibilityProxy.setStaticField(
        EmojiRenderHelper.class, "RENDERABLE_EMOJI_PROVIDER_SET",
        Collections.singleton(EmojiProviderType.DOCOMO));
  }

  @Override
  protected void tearDown() throws Exception {
    VisibilityProxy.setStaticField(
        EmojiRenderHelper.class, "RENDERABLE_EMOJI_PROVIDER_SET", originalRenderableEmojiProviderSet);
    super.tearDown();
  }

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

  public void testBackgroundTextView_postInvalidate() {
    View view = createViewMock(View.class);
    BackgroundTextView backgroundTextView = new BackgroundTextView(view);

    resetAll();
    view.postInvalidate();
    replayAll();

    backgroundTextView.postInvalidate();

    verifyAll();
  }

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

  public void testSetEmojiProviderType() {
    View view = createViewMock(View.class);
    EmojiRenderHelper helper = new EmojiRenderHelper(view);
    EmojiDrawableFactory factory = createMockBuilder(EmojiDrawableFactory.class)
        .withConstructor(Resources.class)
        .withArgs(new MockResources())
        .createMock();
    VisibilityProxy.setField(helper, "emojiDrawableFactory", factory);

    for (EmojiProviderType emojiProviderType : EmojiProviderType.values()) {
      resetAll();
      factory.setProviderType(emojiProviderType);
      replayAll();

      helper.setEmojiProviderType(emojiProviderType);

      verifyAll();
    }

    resetAll();
    factory.setProviderType(null);
    replayAll();

    helper.setEmojiProviderType(null);

    verifyAll();

    // Do nothing for setting the same provider type.
    resetAll();
    replayAll();

    helper.setEmojiProviderType(null);

    verifyAll();
  }

  public void testIsSystemSupportedEmoji() {
    View view = createViewMock(View.class);
    EmojiRenderHelper helper = new EmojiRenderHelper(view);
    helper.setEmojiProviderType(null);
    assertFalse(helper.isSystemSupportedEmoji());

    // Docomo is system supported (see setUp and tearDown).
    helper.setEmojiProviderType(EmojiProviderType.DOCOMO);
    assertTrue(helper.isSystemSupportedEmoji());

    // Others are not.
    helper.setEmojiProviderType(EmojiProviderType.SOFTBANK);
    assertFalse(helper.isSystemSupportedEmoji());
    helper.setEmojiProviderType(EmojiProviderType.KDDI);
    assertFalse(helper.isSystemSupportedEmoji());
  }

  public void testIsRenderableEmoji() {
    View view = createViewMock(View.class);
    EmojiRenderHelper helper = new EmojiRenderHelper(view);
    EmojiDrawableFactory factory = createMockBuilder(EmojiDrawableFactory.class)
        .withConstructor(Resources.class)
        .withArgs(new MockResources())
        .createMock();
    VisibilityProxy.setField(helper, "emojiDrawableFactory", factory);

    // Ignore setProviderType invocations.
    resetAll();
    factory.setProviderType(anyObject(EmojiProviderType.class));
    expectLastCall().anyTimes();
    replayAll();

    // For null provider type, any value won't be rendered.
    helper.setEmojiProviderType(null);
    assertFalse(
        helper.isRenderableEmoji(toString(EmojiDrawableFactory.MIN_EMOJI_PUA_CODE_POINT)));

    // For system supported emoji, it'll depends on the EmojiData contents.
    helper.setEmojiProviderType(EmojiProviderType.DOCOMO);
    assertTrue(helper.isRenderableEmoji(toString(0xFEE10)));  // i-mode mark
    assertFalse(helper.isRenderableEmoji(toString(0xFEE40)));  // EZ mark

    verifyAll();

    // If not sytem supported emoji, it's up to if the factory has the drawable or not.
    resetAll();
    factory.setProviderType(EmojiProviderType.SOFTBANK);
    expect(factory.hasDrawable(0xFEE10)).andReturn(true);
    expect(factory.hasDrawable(0xFEE40)).andReturn(false);
    replayAll();

    helper.setEmojiProviderType(EmojiProviderType.SOFTBANK);
    assertTrue(helper.isRenderableEmoji(toString(0xFEE10)));
    assertFalse(helper.isRenderableEmoji(toString(0xFEE40)));

    verifyAll();
  }

  public void testSetCandidateList_empty() {
    View view = createViewMock(View.class);
    EmojiRenderHelper helper = new EmojiRenderHelper(view);
    helper.setEmojiProviderType(EmojiProviderType.DOCOMO);
    BackgroundTextView backgroundTextView = createMockBuilder(BackgroundTextView.class)
        .withConstructor(View.class)
        .withArgs(view)
        .createMock();
    VisibilityProxy.setField(helper, "backgroundTextView", backgroundTextView);

    resetAll();
    backgroundTextView.setText("");
    replayAll();

    helper.setCandidateList(null);

    verifyAll();

    resetAll();
    backgroundTextView.setText("");
    replayAll();

    helper.setCandidateList(CandidateList.getDefaultInstance());

    verifyAll();
  }

  public void testSetCandidateList() {
    View view = createViewMock(View.class);
    EmojiRenderHelper helper = new EmojiRenderHelper(view);
    helper.setEmojiProviderType(EmojiProviderType.DOCOMO);
    BackgroundTextView backgroundTextView = createMockBuilder(BackgroundTextView.class)
        .withConstructor(View.class)
        .withArgs(view)
        .createMock();
    VisibilityProxy.setField(helper, "backgroundTextView", backgroundTextView);

    resetAll();
    backgroundTextView.setTextSize(TypedValue.COMPLEX_UNIT_PX, 14);
    backgroundTextView.setText(toString(0xFEE10) + "\n");
    expect(backgroundTextView.getLayoutParams()).andReturn(null);
    backgroundTextView.measureInternal(anyInt(), anyInt());
    replayAll();

    helper.setCandidateTextSize(14);
    helper.setCandidateList(CandidateList.newBuilder()
        .addCandidates(CandidateWord.newBuilder()
            .setId(0)
            .setValue("a"))
        .addCandidates(CandidateWord.newBuilder()
            .setId(1)
            .setValue(toString(0xFEE10)))
        .build());

    verifyAll();
  }

  public void testSetCandidateList_nonSystemSupportedEmojiProviderType() {
    View view = createViewMock(View.class);
    EmojiRenderHelper helper = new EmojiRenderHelper(view);
    helper.setEmojiProviderType(EmojiProviderType.SOFTBANK);
    BackgroundTextView backgroundTextView = createMockBuilder(BackgroundTextView.class)
        .withConstructor(View.class)
        .withArgs(view)
        .createMock();
    VisibilityProxy.setField(helper, "backgroundTextView", backgroundTextView);
    resetAll();
    replayAll();

    helper.setCandidateTextSize(14);
    helper.setCandidateList(CandidateList.newBuilder()
        .addCandidates(CandidateWord.newBuilder()
            .setId(0)
            .setValue("a"))
        .addCandidates(CandidateWord.newBuilder()
            .setId(1)
            .setValue("b"))
        .build());

    verifyAll();
  }

  public void testProxyMethod() {
    View view = createViewMock(View.class);
    EmojiRenderHelper helper = new EmojiRenderHelper(view);
    BackgroundTextView backgroundTextView = createMockBuilder(BackgroundTextView.class)
        .withConstructor(View.class)
        .withArgs(view)
        .createMock();
    VisibilityProxy.setField(helper, "backgroundTextView", backgroundTextView);

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
