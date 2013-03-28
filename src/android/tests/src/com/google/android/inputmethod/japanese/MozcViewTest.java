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

package org.mozc.android.inputmethod.japanese;

import static org.easymock.EasyMock.anyInt;
import static org.easymock.EasyMock.capture;
import static org.easymock.EasyMock.eq;
import static org.easymock.EasyMock.expect;
import static org.easymock.EasyMock.expectLastCall;
import static org.easymock.EasyMock.isA;
import static org.easymock.EasyMock.same;

import org.mozc.android.inputmethod.japanese.FeedbackManager.FeedbackEvent;
import org.mozc.android.inputmethod.japanese.InOutAnimatedFrameLayout.VisibilityChangeListener;
import org.mozc.android.inputmethod.japanese.JapaneseKeyboard.KeyboardSpecification;
import org.mozc.android.inputmethod.japanese.LayoutParamsAnimator.InterpolationListener;
import org.mozc.android.inputmethod.japanese.MozcView.DimensionPixelSize;
import org.mozc.android.inputmethod.japanese.MozcView.HeightLinearInterpolationListener;
import org.mozc.android.inputmethod.japanese.MozcView.InsetsCalculator;
import org.mozc.android.inputmethod.japanese.SymbolInputView.MinorCategory;
import org.mozc.android.inputmethod.japanese.ViewManager.LayoutAdjustment;
import org.mozc.android.inputmethod.japanese.emoji.EmojiProviderType;
import org.mozc.android.inputmethod.japanese.keyboard.KeyEventHandler;
import org.mozc.android.inputmethod.japanese.keyboard.KeyboardActionListener;
import org.mozc.android.inputmethod.japanese.keyboard.Row;
import org.mozc.android.inputmethod.japanese.model.SymbolCandidateStorage;
import org.mozc.android.inputmethod.japanese.model.SymbolCandidateStorage.SymbolHistoryStorage;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateList;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateWord;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Command;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Output;
import org.mozc.android.inputmethod.japanese.resources.R;
import org.mozc.android.inputmethod.japanese.testing.ApiLevel;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;
import org.mozc.android.inputmethod.japanese.testing.Parameter;
import org.mozc.android.inputmethod.japanese.testing.VisibilityProxy;
import org.mozc.android.inputmethod.japanese.ui.SideFrameStubProxy;
import org.mozc.android.inputmethod.japanese.view.SkinType;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Rect;
import android.inputmethodservice.InputMethodService.Insets;
import android.os.Handler;
import android.os.Looper;
import android.test.mock.MockResources;
import android.test.suitebuilder.annotation.SmallTest;
import android.util.DisplayMetrics;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.animation.Animation;
import android.widget.CompoundButton;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;
import org.easymock.Capture;
import java.util.Collections;

/**
 */
public class MozcViewTest extends InstrumentationTestCaseWithMock {
  @SmallTest
  public void testSetViewEventListener() {
    ViewEventListener viewEventListener = createMock(ViewEventListener.class);
    OnClickListener widenButtonClickListener = createMock(OnClickListener.class);
    CandidateView candidateView = createViewMock(CandidateView.class);
    candidateView.setViewEventListener(same(viewEventListener), isA(OnClickListener.class));
    SymbolInputView symbolInputView = createViewMock(SymbolInputView.class);
    symbolInputView.setViewEventListener(same(viewEventListener),
                                         isA(OnClickListener.class));
    ImageView hardwareCompositionButton = new ImageView(getInstrumentation().getContext());
    ImageView widenButton = new ImageView(getInstrumentation().getContext());
    OnClickListener leftAdjustButtonClickListener = createMock(OnClickListener.class);
    OnClickListener rightAdjustButtonClickListener = createMock(OnClickListener.class);

    MozcView mozcView = createViewMockBuilder(MozcView.class)
        .addMockedMethods(
            "checkInflated", "getCandidateView", "getSymbolInputView",
            "getHardwareCompositionButton", "getWidenButton")
        .createMock();
    mozcView.checkInflated();
    expectLastCall().asStub();
    expect(mozcView.getCandidateView()).andStubReturn(candidateView);
    expect(mozcView.getSymbolInputView()).andStubReturn(symbolInputView);
    expect(mozcView.getHardwareCompositionButton()).andStubReturn(hardwareCompositionButton);
    expect(mozcView.getWidenButton()).andStubReturn(widenButton);
    viewEventListener.onClickHardwareKeyboardCompositionModeButton();
    widenButtonClickListener.onClick(widenButton);

    replayAll();

    mozcView.setEventListener(viewEventListener, widenButtonClickListener,
                              leftAdjustButtonClickListener, rightAdjustButtonClickListener);
    mozcView.getHardwareCompositionButton().performClick();
    mozcView.getWidenButton().performClick();

    verifyAll();

    assertEquals(leftAdjustButtonClickListener,
                 VisibilityProxy.getField(VisibilityProxy.getField(mozcView, "leftFrameStubProxy"),
                     "buttonOnClickListener"));
    assertEquals(rightAdjustButtonClickListener,
                 VisibilityProxy.getField(VisibilityProxy.getField(mozcView, "rightFrameStubProxy"),
                     "buttonOnClickListener"));
  }

  @SmallTest
  public void testSetKeyEventHandler() {
    KeyEventHandler keyEventHandler = new KeyEventHandler(
        Looper.getMainLooper(), createNiceMock(KeyboardActionListener.class), 0, 0, 0);
    JapaneseKeyboardView keyboardView = createViewMock(JapaneseKeyboardView.class);
    keyboardView.setKeyEventHandler(keyEventHandler);
    SymbolInputView symbolInputView = createViewMock(SymbolInputView.class);
    symbolInputView.setKeyEventHandler(keyEventHandler);

    MozcView mozcView = createMockBuilder(MozcView.class)
        .withConstructor(Context.class)
        .withArgs(getInstrumentation().getTargetContext())
        .addMockedMethods(
            "checkInflated", "getKeyboardView", "getSymbolInputView")
        .createMock();
    mozcView.checkInflated();
    expectLastCall().asStub();
    expect(mozcView.getKeyboardView()).andStubReturn(keyboardView);
    expect(mozcView.getSymbolInputView()).andStubReturn(symbolInputView);

    replayAll();

    mozcView.setKeyEventHandler(keyEventHandler);

    verifyAll();
  }

  @SmallTest
  public void testGetJapaneseKeyboard() {
    JapaneseKeyboard keyboard = new JapaneseKeyboard(
        Collections.<Row>emptyList(), 1, KeyboardSpecification.TWELVE_KEY_TOGGLE_KANA);
    JapaneseKeyboardView keyboardView = createViewMock(JapaneseKeyboardView.class);
    expect(keyboardView.getJapaneseKeyboard()).andStubReturn(keyboard);
    MozcView mozcView = createViewMockBuilder(MozcView.class)
        .addMockedMethods("checkInflated", "getKeyboardView")
        .createMock();
    mozcView.checkInflated();
    expectLastCall().asStub();
    expect(mozcView.getKeyboardView()).andStubReturn(keyboardView);

    replayAll();

    assertSame(keyboard, mozcView.getJapaneseKeyboard());

    verifyAll();
  }

  @SmallTest
  public void testSetJapaneseKeyboard() {
    JapaneseKeyboard keyboard = new JapaneseKeyboard(
        Collections.<Row>emptyList(), 1, KeyboardSpecification.TWELVE_KEY_TOGGLE_KANA);
    JapaneseKeyboardView keyboardView = createViewMock(JapaneseKeyboardView.class);
    keyboardView.setJapaneseKeyboard(keyboard);
    MozcView mozcView = createViewMockBuilder(MozcView.class)
        .addMockedMethods("checkInflated", "getKeyboardView")
        .createMock();
    mozcView.checkInflated();
    expectLastCall().asStub();
    expect(mozcView.getKeyboardView()).andStubReturn(keyboardView);

    replayAll();

    mozcView.setJapaneseKeyboard(keyboard);

    verifyAll();
  }

  @SmallTest
  public void testSetEmojiEnabled() {
    SymbolInputView symbolInputView = createViewMock(SymbolInputView.class);
    MozcView mozcView = createViewMockBuilder(MozcView.class)
        .addMockedMethods("checkInflated", "getSymbolInputView")
        .createMock();

    boolean parameters[] = { true, false };
    for (boolean emojiEnabled : parameters) {
      resetAll();
      symbolInputView.setEmojiEnabled(emojiEnabled);
      mozcView.checkInflated();
      expectLastCall().asStub();
      expect(mozcView.getSymbolInputView()).andStubReturn(symbolInputView);
      replayAll();

      mozcView.setEmojiEnabled(emojiEnabled);

      verifyAll();
    }
  }

  @SmallTest
  public void testSetEmojiProviderType() {
    SymbolInputView symbolInputView = createViewMock(SymbolInputView.class);
    MozcView mozcView = createViewMockBuilder(MozcView.class)
        .addMockedMethods("checkInflated", "getSymbolInputView")
        .createMock();

    for (EmojiProviderType emojiProviderType : EmojiProviderType.values()) {
      resetAll();
      symbolInputView.setEmojiProviderType(emojiProviderType);
      mozcView.checkInflated();
      expectLastCall().asStub();
      expect(mozcView.getSymbolInputView()).andStubReturn(symbolInputView);
      replayAll();

      mozcView.setEmojiProviderType(emojiProviderType);

      verifyAll();
    }
  }

  @SmallTest
  public void testSetSkinType() {
    CandidateView candidateView = createViewMock(CandidateView.class);
    JapaneseKeyboardView keyboardView = createViewMock(JapaneseKeyboardView.class);
    SymbolInputView symbolInputView = createViewMock(SymbolInputView.class);
    MozcView mozcView = createViewMockBuilder(MozcView.class)
        .addMockedMethods("checkInflated", "getKeyboardView", "getCandidateView",
                          "getSymbolInputView")
        .createMock();

    for (SkinType skinType : SkinType.values()) {
      resetAll();
      mozcView.checkInflated();
      expectLastCall().asStub();
      expect(mozcView.getKeyboardView()).andStubReturn(keyboardView);
      expect(mozcView.getCandidateView()).andStubReturn(candidateView);
      expect(mozcView.getSymbolInputView()).andStubReturn(symbolInputView);
      keyboardView.setSkinType(skinType);
      candidateView.setSkinType(skinType);
      symbolInputView.setSkinType(skinType);
      replayAll();

      mozcView.setSkinType(skinType);

      verifyAll();
    }
  }

  @SmallTest
  public void testSetPopupEnabled() {
    JapaneseKeyboardView keyboardView = createViewMock(JapaneseKeyboardView.class);
    MozcView mozcView = createViewMockBuilder(MozcView.class)
        .addMockedMethods("checkInflated", "getKeyboardView")
        .createMock();

    boolean parameters[] = { true, false };
    for (boolean popupEnabled : parameters) {
      resetAll();
      keyboardView.setPopupEnabled(popupEnabled);
      mozcView.checkInflated();
      expectLastCall().asStub();
      expect(mozcView.getKeyboardView()).andStubReturn(keyboardView);
      replayAll();

      mozcView.setPopupEnabled(popupEnabled);

      verifyAll();
    }
  }

  @SmallTest
  public void testSetCommand() {
    CandidateView candidateView = createViewMock(CandidateView.class);
    MozcView mozcView = createViewMockBuilder(MozcView.class)
        .addMockedMethods("checkInflated", "getCandidateView", "startCandidateViewInAnimation",
                          "startCandidateViewOutAnimation")
        .createMock();

    // Set non-empty command.
    {
      Command command = Command.newBuilder()
          .setOutput(Output.newBuilder()
              .setAllCandidateWords(CandidateList.newBuilder()
                  .addCandidates(CandidateWord.getDefaultInstance())))
          .buildPartial();

      candidateView.update(command);
      mozcView.startCandidateViewInAnimation();
      mozcView.checkInflated();
      expectLastCall().asStub();
      expect(mozcView.getCandidateView()).andStubReturn(candidateView);
      replayAll();

      mozcView.setCommand(command);

      verifyAll();
    }

    // Set empty command.
    {
      resetAll();
      mozcView.startCandidateViewOutAnimation();
      mozcView.checkInflated();
      expectLastCall().asStub();
      expect(mozcView.getCandidateView()).andStubReturn(candidateView);
      replayAll();

      mozcView.setCommand(Command.getDefaultInstance());

      verifyAll();
    }
  }

  @SmallTest
  public void testReset() {
    LayoutInflater inflater = LayoutInflater.from(getInstrumentation().getTargetContext());
    MozcView mozcView = MozcView.class.cast(inflater.inflate(R.layout.mozc_view, null));

    {
      View keyboardFrame = mozcView.getKeyboardFrame();
      keyboardFrame.setVisibility(View.GONE);
      keyboardFrame.getLayoutParams().height = 0;

      mozcView.getCandidateView().startInAnimation();
      mozcView.getSymbolInputView().startInAnimation();
    }

    mozcView.reset();

    assertEquals(View.VISIBLE, mozcView.getKeyboardFrame().getVisibility());
    assertTrue(mozcView.getKeyboardFrame().getLayoutParams().height > 0);

    assertEquals(View.GONE, mozcView.getCandidateView().getVisibility());
    assertNull(mozcView.getCandidateView().getAnimation());

    assertEquals(View.GONE, mozcView.getSymbolInputView().getVisibility());
    assertNull(mozcView.getSymbolInputView().getAnimation());
  }

  @SmallTest
  public void testGetVisibleViewHeight() {
    Resources resources = getInstrumentation().getTargetContext().getResources();

    int imeWindowHeight =
        resources.getDimensionPixelSize(R.dimen.ime_window_height)
        - resources.getDimensionPixelSize(R.dimen.translucent_border_height);
    int inputFrameHeight =
        resources.getDimensionPixelSize(R.dimen.input_frame_height);
    int narrowImeWindowHeight =
        resources.getDimensionPixelSize(R.dimen.narrow_ime_window_height)
        - resources.getDimensionPixelSize(R.dimen.translucent_border_height);
    int narrowFrameHeight =
        resources.getDimensionPixelSize(R.dimen.narrow_frame_height);

    class TestData extends Parameter {
      final boolean narrowMode;
      final int candidateViewVisibility;
      final int symbolInputViewVisibility;
      final int expectedHeight;

      TestData(boolean narrowMode, int candidateViewVisibility, int symbolInputViewVisibility,
               int expectedHeight) {
        this.narrowMode = narrowMode;
        this.candidateViewVisibility = candidateViewVisibility;
        this.symbolInputViewVisibility = symbolInputViewVisibility;
        this.expectedHeight = expectedHeight;
      }
    }
    TestData[] testDataList = {
        // At normal view mode.
        // If both candidate view and symbol input view are hidden, the height of the view
        // equals to input frame height.
        new TestData(false, View.GONE, View.GONE, inputFrameHeight),
        // If either candidate view or symbol input view are visible, the height of the view
        // equals to ime window height.
        new TestData(false, View.VISIBLE, View.GONE, imeWindowHeight),
        new TestData(false, View.GONE, View.VISIBLE, imeWindowHeight),

        // At narrow view mode.
        // If both candidate view and symbol input view are hidden, the height of the view
        // equals to narrow frame height.
        new TestData(true, View.GONE, View.GONE, narrowFrameHeight),
        // If either candidate view or symbol input view are visible, the height of the view
        // equals to narrow ime window height.
        new TestData(true, View.VISIBLE, View.GONE, narrowImeWindowHeight),
        new TestData(true, View.GONE, View.VISIBLE, narrowImeWindowHeight),
    };

    LayoutInflater inflater = LayoutInflater.from(getInstrumentation().getTargetContext());
    MozcView mozcView = MozcView.class.cast(inflater.inflate(R.layout.mozc_view, null));
    for (TestData testData : testDataList) {
      mozcView.setNarrowMode(testData.narrowMode);
      mozcView.getCandidateView().setVisibility(testData.candidateViewVisibility);
      mozcView.getSymbolInputView().setVisibility(testData.symbolInputViewVisibility);
      assertEquals(testData.toString(),
                   testData.expectedHeight, mozcView.getVisibleViewHeight());
    }
  }

  @SmallTest
  public void testSymbolInputViewInitializationLazily() {
    LayoutInflater inflater = LayoutInflater.from(getInstrumentation().getTargetContext());
    MozcView mozcView = MozcView.class.cast(inflater.inflate(R.layout.mozc_view, null));
    VisibilityProxy.setField(mozcView, "isDropShadowExpanded", true);
    SymbolCandidateStorage candidateStorage = createMockBuilder(SymbolCandidateStorage.class)
        .withConstructor(SymbolHistoryStorage.class)
        .withArgs(SymbolHistoryStorage.class.cast(null))
        .createMock();
    expect(candidateStorage.getCandidateList(isA(MinorCategory.class)))
        .andStubReturn(CandidateList.getDefaultInstance());
    mozcView.setSymbolCandidateStorage(candidateStorage);
    replayAll();

    // At first, symbol input view is not inflated.
    assertFalse(mozcView.getSymbolInputView().isInflated());

    // Once symbol input view is shown, the view (and its children) should be inflated.
    assertTrue(mozcView.showSymbolInputView());
    assertTrue(mozcView.getSymbolInputView().isInflated());

    // Even after closing the view, the inflation state should be kept.
    mozcView.hideSymbolInputView();
    assertTrue(mozcView.getSymbolInputView().isInflated());
  }

  @SmallTest
  public void testUpdateInputFrameHeight() {
    Resources resources = getInstrumentation().getTargetContext().getResources();

    int shadowHeight = resources.getDimensionPixelSize(R.dimen.translucent_border_height);
    int imeWindowHeight = resources.getDimensionPixelSize(R.dimen.ime_window_height);
    int inputFrameHeight = resources.getDimensionPixelSize(R.dimen.input_frame_height);
    int narrowImeWindowHeight = resources.getDimensionPixelSize(R.dimen.narrow_ime_window_height);
    int narrowFrameHeight =  resources.getDimensionPixelSize(R.dimen.narrow_frame_height);


    class TestData extends Parameter {
      final boolean fullscreenMode;
      final boolean narrowMode;
      final int candidateViewVisibility;
      final int expectedHeight;

      TestData(boolean fullscreenMode, boolean narrowMode,
               int candidateViewVisibility, int expectedHeight) {
        this.fullscreenMode = fullscreenMode;
        this.narrowMode = narrowMode;
        this.candidateViewVisibility = candidateViewVisibility;
        this.expectedHeight = expectedHeight;
      }
    }
    TestData[] testDataList = {
        // At fullscreen mode and narrow mode
        // If candidate view is hidden, the height of the view
        // equals to narrow frame height.
        new TestData(true, true, View.GONE, narrowFrameHeight + shadowHeight),
        // If candidate view is visible, the height of the view
        // equals to narrow ime window height.
        new TestData(true, true, View.VISIBLE, narrowImeWindowHeight),

        // At fullscreen mode and not narrow mode
        // If candidate view is hidden, the height of the view
        // equals to input frame height.
        new TestData(true, false, View.GONE, inputFrameHeight + shadowHeight),
        // If candidate view is visible, the height of the view
        // equals to ime window height.
        new TestData(true, false, View.VISIBLE, imeWindowHeight),

        // At not fullscreen mode and narrow mode
        // The height of the view equals to narrow ime window height.
        new TestData(false, true, View.GONE, narrowImeWindowHeight),
        new TestData(false, true, View.VISIBLE, narrowImeWindowHeight),

        // At not fullscreen mode and not narrow mode
        // The height of the view equals to ime window height.
        new TestData(false, false, View.GONE, imeWindowHeight),
        new TestData(false, false, View.VISIBLE, imeWindowHeight),
    };

    LayoutInflater inflater = LayoutInflater.from(getInstrumentation().getTargetContext());
    MozcView mozcView = MozcView.class.cast(inflater.inflate(R.layout.mozc_view, null));
    for (TestData testData : testDataList) {
      mozcView.setFullscreenMode(testData.fullscreenMode);
      mozcView.resetFullscreenMode();
      mozcView.setNarrowMode(testData.narrowMode);
      mozcView.getCandidateView().setVisibility(testData.candidateViewVisibility);
      mozcView.updateInputFrameHeight();

      assertEquals(testData.toString(),
                   testData.expectedHeight, mozcView.getBottomFrame().getLayoutParams().height);
    }
  }

  @SmallTest
  public void testSetFullscreenMode() {
    LayoutInflater inflater = LayoutInflater.from(getInstrumentation().getTargetContext());
    MozcView mozcView = MozcView.class.cast(inflater.inflate(R.layout.mozc_view, null));

    assertFalse(mozcView.isFullscreenMode());

    boolean parameters[] = { true, false };
    for (boolean fullscreenModeEnabled : parameters) {
      mozcView.setFullscreenMode(fullscreenModeEnabled);

      assertEquals(fullscreenModeEnabled, mozcView.isFullscreenMode());
    }
  }

  @SmallTest
  public void testResetFullscreenMode() {
    Context context = getInstrumentation().getTargetContext();
    LayoutInflater inflater = LayoutInflater.from(context);
    MozcView mozcView = MozcView.class.cast(inflater.inflate(R.layout.mozc_view, null));
    View overlayView =  mozcView.getOverlayView();
    LinearLayout textInputFrame = mozcView.getTextInputFrame();
    VisibilityChangeListener onVisibilityChangeListener =
        VisibilityProxy.getField(mozcView, "onVisibilityChangeListener");

    mozcView.setFullscreenMode(true);
    mozcView.resetFullscreenMode();
    assertEquals(0, overlayView.getLayoutParams().height);
    assertEquals(FrameLayout.LayoutParams.WRAP_CONTENT, textInputFrame.getLayoutParams().height);
    assertSame(onVisibilityChangeListener,
               VisibilityProxy.getField(mozcView.getCandidateView(),
                                        "onVisibilityChangeListener"));
    assertSame(onVisibilityChangeListener,
               VisibilityProxy.getField(mozcView.getSymbolInputView(),
                                        "onVisibilityChangeListener"));

    mozcView.setFullscreenMode(false);
    mozcView.resetFullscreenMode();
    assertEquals(LinearLayout.LayoutParams.MATCH_PARENT, overlayView.getLayoutParams().height);
    assertEquals(FrameLayout.LayoutParams.MATCH_PARENT, textInputFrame.getLayoutParams().height);
    assertNull(
        VisibilityProxy.getField(mozcView.getCandidateView(), "onVisibilityChangeListener"));
    assertNull(
        VisibilityProxy.getField(mozcView.getSymbolInputView(), "onVisibilityChangeListener"));
  }

  @SmallTest
  public void testOnVisibilityChangeListener() {
    MozcView mozcView = createViewMockBuilder(MozcView.class)
        .addMockedMethod("updateInputFrameHeight").createMock();
    VisibilityChangeListener onVisibilityChangeListener =
        VisibilityProxy.getField(mozcView, "onVisibilityChangeListener");

    mozcView.updateInputFrameHeight();

    replayAll();

    onVisibilityChangeListener.onVisibilityChange(View.GONE, View.GONE);

    verifyAll();
  }

  @SmallTest
  public void testSwitchNarrowMode() {
    Context context = getInstrumentation().getTargetContext();
    MozcView mozcView =
        MozcView.class.cast(LayoutInflater.from(context).inflate(R.layout.mozc_view, null));
    View keyboardFrame = mozcView.getKeyboardFrame();
    View inputFrameButton = mozcView.getCandidateView().getInputFrameFoldButton();
    View narrowFrame = mozcView.getNarrowFrame();

    mozcView.setNarrowMode(true);
    assertTrue(mozcView.isNarrowMode());
    assertEquals(View.GONE, keyboardFrame.getVisibility());
    assertEquals(View.GONE, inputFrameButton.getVisibility());
    assertEquals(View.VISIBLE, narrowFrame.getVisibility());

    mozcView.setNarrowMode(false);
    assertFalse(mozcView.isNarrowMode());
    assertEquals(View.VISIBLE, keyboardFrame.getVisibility());
    assertEquals(View.VISIBLE, inputFrameButton.getVisibility());
    assertEquals(View.GONE, narrowFrame.getVisibility());
  }

  @SmallTest
  public void testGetKeyboardSize() {
    Context context = getInstrumentation().getTargetContext();
    Resources resources = context.getResources();
    LayoutInflater inflater = LayoutInflater.from(context);
    MozcView mozcView = MozcView.class.cast(inflater.inflate(R.layout.mozc_view, null));

    {
      mozcView.setLayoutAdjustment(LayoutAdjustment.FILL);
      Rect keyboardSize = mozcView.getKeyboardSize();
      assertEquals(resources.getDisplayMetrics().widthPixels, keyboardSize.width());
      assertEquals(resources.getDimensionPixelSize(R.dimen.input_frame_height),
                   keyboardSize.height());
    }

    {
      mozcView.setLayoutAdjustment(LayoutAdjustment.RIGHT);
      Rect keyboardSize = mozcView.getKeyboardSize();
      assertEquals(resources.getDimensionPixelSize(R.dimen.ime_window_partial_width),
                   keyboardSize.width());
      assertEquals(resources.getDimensionPixelSize(R.dimen.input_frame_height),
                   keyboardSize.height());
    }
  }

  @SmallTest
  public void testSetLayoutAdjustment() {
    MozcView mozcView = createViewMockBuilder(MozcView.class)
        .addMockedMethods("checkInflated", "getForegroundFrame", "getInputFrameHeight",
                          "updateBackgroundColor", "getSymbolInputView")
        .createMock();
    View foreGroundFrame = createViewMockBuilder(View.class)
        .addMockedMethods("getLayoutParams", "setLayoutParams")
        .createMock();
    createViewMockBuilder(View.class)
        .addMockedMethod("setVisibility")
        .createMock();
    createViewMockBuilder(View.class)
        .addMockedMethod("setVisibility")
        .createMock();
    SideFrameStubProxy leftFrameStubProxy = createMock(SideFrameStubProxy.class);
    SideFrameStubProxy rightFrameStubProxy = createMock(SideFrameStubProxy.class);
    VisibilityProxy.setField(mozcView, "leftFrameStubProxy", leftFrameStubProxy);
    VisibilityProxy.setField(mozcView, "rightFrameStubProxy", rightFrameStubProxy);

    class TestData extends Parameter {
      final LayoutAdjustment layoutAdjustment;
      final boolean narrowMode;

      final int expectForegroundFrameWidth;
      final int expectForegroundFrameGravity;
      final int expectLeftFrameVisibility;
      final int expectRightFrameVisibility;

      TestData(LayoutAdjustment layoutAdjustment, boolean narrowMode,
               int expectForegroundFrameWidth, int expectForegroundFrameGravity,
               int expectLeftFrameVisibility, int expectRightFrameVisibility) {
        this.layoutAdjustment = layoutAdjustment;
        this.narrowMode = narrowMode;
        this.expectForegroundFrameWidth = expectForegroundFrameWidth;
        this.expectForegroundFrameGravity = expectForegroundFrameGravity;
        this.expectLeftFrameVisibility = expectLeftFrameVisibility;
        this.expectRightFrameVisibility = expectRightFrameVisibility;
      }
    }

    Resources resources = getInstrumentation().getTargetContext().getResources();
    int displayWidth = resources.getDisplayMetrics().widthPixels;
    int partialWidth = resources.getDimensionPixelSize(R.dimen.ime_window_partial_width)
        + resources.getDimensionPixelSize(R.dimen.side_frame_width);

    TestData[] testDataList = {
      new TestData(LayoutAdjustment.FILL, false,
                   displayWidth, Gravity.BOTTOM, View.GONE, View.GONE),
      new TestData(LayoutAdjustment.LEFT, false,
                   partialWidth, Gravity.BOTTOM | Gravity.LEFT, View.GONE, View.VISIBLE),
      new TestData(LayoutAdjustment.RIGHT, false,
                   partialWidth, Gravity.BOTTOM | Gravity.RIGHT, View.VISIBLE, View.GONE),
      new TestData(LayoutAdjustment.FILL, true,
                   displayWidth, Gravity.BOTTOM, View.GONE, View.GONE),
      new TestData(LayoutAdjustment.LEFT, true,
                   displayWidth, Gravity.BOTTOM, View.GONE, View.GONE),
      new TestData(LayoutAdjustment.RIGHT, true,
                   displayWidth, Gravity.BOTTOM, View.GONE, View.GONE),
    };

    for (TestData testData : testDataList) {
      Capture<FrameLayout.LayoutParams> layoutCapture = new Capture<FrameLayout.LayoutParams>();
      resetAll();
      mozcView.checkInflated();
      expect(mozcView.getInputFrameHeight()).andStubReturn(100);
      expect(mozcView.getForegroundFrame()).andReturn(foreGroundFrame);
      expect(foreGroundFrame.getLayoutParams()).andReturn(new FrameLayout.LayoutParams(0, 0));
      foreGroundFrame.setLayoutParams(capture(layoutCapture));
      leftFrameStubProxy.setFrameVisibility(testData.expectLeftFrameVisibility);
      rightFrameStubProxy.setFrameVisibility(testData.expectRightFrameVisibility);
      mozcView.updateBackgroundColor();

      replayAll();
      VisibilityProxy.setField(mozcView, "narrowMode", testData.narrowMode);
      mozcView.setLayoutAdjustment(testData.layoutAdjustment);

      verifyAll();
      assertEquals(testData.toString(),
                   testData.layoutAdjustment,
                   VisibilityProxy.getField(mozcView, "layoutAdjustment"));
      assertEquals(testData.toString(),
                   testData.expectForegroundFrameWidth, layoutCapture.getValue().width);
      assertEquals(testData.toString(),
                   testData.expectForegroundFrameGravity, layoutCapture.getValue().gravity);
    }
  }

  @SmallTest
  public void testUpdateBackgroundColor() {
    MozcView mozcView = createViewMockBuilder(MozcView.class)
        .addMockedMethods("getBottomBackground")
        .createMock();

    View bottomBackground = createViewMockBuilder(View.class)
        .addMockedMethods("setBackgroundResource")
        .createMock();

    class TestData extends Parameter {
      final boolean fullscreenMode;
      final boolean narrowMode;
      final InsetsCalculator insetsCalculator;

      final int expectResourceId;

      TestData(boolean fullscreenMode, boolean narrowMode, final boolean isFloatable,
               int expectedResourceId) {
        this.fullscreenMode = fullscreenMode;
        this.narrowMode = narrowMode;
        this.insetsCalculator = new InsetsCalculator() {
          @Override
          public void setInsets(
              MozcView mozcView, int contentViewWidth, int contentViewHeight, Insets outInsets) {
          }

          @Override
          public boolean isFloatingMode(MozcView mozcView) {
            return isFloatable;
          }
        };
        this.expectResourceId = expectedResourceId;
      }
    }

    TestData[] testDataList = {
        new TestData(true, false, false, R.color.input_frame_background),
        new TestData(false, true, false, 0),
        new TestData(false, false, true, 0),
        new TestData(false, false, false, R.color.input_frame_background),
    };

    for (TestData testData : testDataList) {
      VisibilityProxy.setField(mozcView, "fullscreenMode", testData.fullscreenMode);
      VisibilityProxy.setField(mozcView, "narrowMode", testData.narrowMode);

      resetAll();
      expect(mozcView.getBottomBackground()).andStubReturn(bottomBackground);
      VisibilityProxy.setField(mozcView, "insetsCalculator", testData.insetsCalculator);
      bottomBackground.setBackgroundResource(testData.expectResourceId);

      replayAll();
      mozcView.updateBackgroundColor();

      verifyAll();
    }
  }

  @SmallTest
  public void testDefaultInsetsCalculator() {
    MozcView mozcView = createViewMockBuilder(MozcView.class)
        .addMockedMethod("getVisibleViewHeight")
        .createMock();
    InsetsCalculator insetsCalculator = new MozcView.DefaultInsetsCalculator();
    int visibleViewHeight = 240;
    int contentViewWidth = 800;
    int contentViewHeight = 400;
    expect(mozcView.getVisibleViewHeight()).andStubReturn(visibleViewHeight);
    replayAll();

    assertFalse(insetsCalculator.isFloatingMode(mozcView));
    Insets outInsets = new Insets();
    insetsCalculator.setInsets(mozcView, contentViewWidth, contentViewHeight, outInsets);
    assertEquals(Insets.TOUCHABLE_INSETS_CONTENT, outInsets.touchableInsets);
    assertEquals(contentViewHeight - visibleViewHeight, outInsets.contentTopInsets);
    assertEquals(contentViewHeight - visibleViewHeight, outInsets.visibleTopInsets);
  }

  @SmallTest
  @ApiLevel(11)
  @TargetApi(11)
  public void testRegionInsetsCalculator() {
    MozcView mozcView = createViewMockBuilder(MozcView.class)
        .addMockedMethods("getVisibleViewHeight", "getResources", "getSideAdjustedWidth")
        .createMock();
    Resources resources = createMockBuilder(MockResources.class)
        .addMockedMethods("getDisplayMetrics", "getDimensionPixelSize")
        .createMock();

    InsetsCalculator insetsCalculator = new MozcView.RegionInsetsCalculator();
    int visibleViewHeight = 240;
    int contentViewWidth = 800;
    int contentViewHeight = 400;
    int width = 500;

    expect(mozcView.getResources()).andStubReturn(resources);
    DisplayMetrics displayMetrics = new DisplayMetrics();
    displayMetrics.widthPixels = 100;
    expect(resources.getDisplayMetrics()).andStubReturn(displayMetrics);
    expect(resources.getDimensionPixelSize(R.dimen.ime_window_region_inset_threshold))
        .andStubReturn(50);
    expect(resources.getDimensionPixelSize(anyInt())).andStubReturn(0);
    expect(mozcView.getVisibleViewHeight()).andStubReturn(visibleViewHeight);
    expect(mozcView.getSideAdjustedWidth()).andStubReturn(width);

    replayAll();

    DimensionPixelSize dimensionPixelSize = new MozcView.DimensionPixelSize(resources);
    VisibilityProxy.setField(mozcView, "dimensionPixelSize", dimensionPixelSize);

    {
      VisibilityProxy.setField(mozcView, "layoutAdjustment", LayoutAdjustment.FILL);
      assertFalse(insetsCalculator.isFloatingMode(mozcView));

      Insets outInsets = new Insets();
      insetsCalculator.setInsets(mozcView, contentViewWidth, contentViewHeight, outInsets);
      assertEquals(Insets.TOUCHABLE_INSETS_CONTENT, outInsets.touchableInsets);
      assertEquals(contentViewHeight - visibleViewHeight, outInsets.contentTopInsets);
      assertEquals(contentViewHeight - visibleViewHeight, outInsets.visibleTopInsets);
    }

    {
      VisibilityProxy.setField(mozcView, "layoutAdjustment", LayoutAdjustment.LEFT);
      VisibilityProxy.setField(mozcView, "narrowMode", false);
      assertTrue(insetsCalculator.isFloatingMode(mozcView));

      Insets outInsets = new Insets();
      insetsCalculator.setInsets(mozcView, contentViewWidth, contentViewHeight, outInsets);
      assertEquals(Insets.TOUCHABLE_INSETS_REGION, outInsets.touchableInsets);
      Rect bounds = outInsets.touchableRegion.getBounds();
      assertEquals(0, bounds.left);
      assertEquals(contentViewHeight - visibleViewHeight, bounds.top);
      assertEquals(width, bounds.right);
      assertEquals(contentViewHeight, bounds.bottom);
      assertEquals(contentViewHeight, outInsets.contentTopInsets);
      assertEquals(contentViewHeight, outInsets.visibleTopInsets);
    }

    {
      VisibilityProxy.setField(mozcView, "layoutAdjustment", LayoutAdjustment.RIGHT);
      VisibilityProxy.setField(mozcView, "narrowMode", false);
      assertTrue(insetsCalculator.isFloatingMode(mozcView));

      Insets outInsets = new Insets();
      insetsCalculator.setInsets(mozcView, contentViewWidth, contentViewHeight, outInsets);
      assertEquals(Insets.TOUCHABLE_INSETS_REGION, outInsets.touchableInsets);
      Rect bounds = outInsets.touchableRegion.getBounds();
      assertEquals(contentViewWidth - width, bounds.left);
      assertEquals(contentViewHeight - visibleViewHeight, bounds.top);
      assertEquals(contentViewWidth, bounds.right);
      assertEquals(contentViewHeight, bounds.bottom);
      assertEquals(contentViewHeight, outInsets.contentTopInsets);
      assertEquals(contentViewHeight, outInsets.contentTopInsets);
    }
  }

  @SmallTest
  public void testExpandDropShadowAndBackground() {
    MozcView mozcView = createViewMockBuilder(MozcView.class)
        .addMockedMethods("getDropShadowTop", "getBottomBackground")
        .createMock();
    View dropShadowTop = createViewMock(View.class);
    View bottomBackground = createViewMockBuilder(View.class)
        .addMockedMethods("getLayoutParams", "setLayoutParams")
        .createMock();
    SideFrameStubProxy leftFrameStubProxy = createMock(SideFrameStubProxy.class);
    SideFrameStubProxy rightFrameStubProxy = createMock(SideFrameStubProxy.class);
    VisibilityProxy.setField(mozcView, "leftFrameStubProxy", leftFrameStubProxy);
    VisibilityProxy.setField(mozcView, "rightFrameStubProxy", rightFrameStubProxy);
    Resources resources = getInstrumentation().getTargetContext().getResources();
    int imeWindowHeight = 100;

    {
      resetAll();
      VisibilityProxy.setField(mozcView, "imeWindowHeight", imeWindowHeight);
      expect(mozcView.getDropShadowTop()).andStubReturn(dropShadowTop);
      expect(mozcView.getBottomBackground()).andStubReturn(bottomBackground);
      leftFrameStubProxy.flipDropShadowVisibility(View.INVISIBLE);
      rightFrameStubProxy.flipDropShadowVisibility(View.INVISIBLE);
      dropShadowTop.setVisibility(View.VISIBLE);
      expect(bottomBackground.getLayoutParams()).andStubReturn(new LayoutParams(0, 0));
      Capture<LayoutParams> captureBottomBackground = new Capture<LayoutParams>();
      bottomBackground.setLayoutParams(capture(captureBottomBackground));

      replayAll();
      VisibilityProxy.setField(mozcView, "fullscreenMode", false);
      mozcView.expandDropShadowAndBackground();

      verifyAll();
      assertEquals(imeWindowHeight
                       - resources.getDimensionPixelSize(R.dimen.translucent_border_height),
                   captureBottomBackground.getValue().height);
    }
    {
      resetAll();
      VisibilityProxy.setField(mozcView, "imeWindowHeight", imeWindowHeight);
      expect(mozcView.getDropShadowTop()).andStubReturn(dropShadowTop);
      expect(mozcView.getBottomBackground()).andStubReturn(bottomBackground);
      leftFrameStubProxy.flipDropShadowVisibility(View.INVISIBLE);
      rightFrameStubProxy.flipDropShadowVisibility(View.INVISIBLE);
      dropShadowTop.setVisibility(View.VISIBLE);
      expect(bottomBackground.getLayoutParams()).andStubReturn(new LayoutParams(0, 0));
      Capture<LayoutParams> captureBottomBackground = new Capture<LayoutParams>();
      bottomBackground.setLayoutParams(capture(captureBottomBackground));

      replayAll();
      VisibilityProxy.setField(mozcView, "fullscreenMode", true);
      mozcView.expandDropShadowAndBackground();

      verifyAll();
      assertEquals(imeWindowHeight, captureBottomBackground.getValue().height);
    }
  }

  @SmallTest
  public void testCollapseDropShadowAndBackground() {
    Resources resources = getInstrumentation().getTargetContext().getResources();
    MozcView mozcView = createViewMockBuilder(MozcView.class)
        .addMockedMethods("getDropShadowTop", "getBottomBackground", "getInputFrameHeight")
        .createMock();
    View dropShadowTop = createViewMock(View.class);
    View bottomBackground = createViewMockBuilder(View.class)
        .addMockedMethods("getLayoutParams", "setLayoutParams")
        .createMock();
    SideFrameStubProxy leftFrameStubProxy = createMock(SideFrameStubProxy.class);
    SideFrameStubProxy rightFrameStubProxy = createMock(SideFrameStubProxy.class);
    VisibilityProxy.setField(mozcView, "leftFrameStubProxy", leftFrameStubProxy);
    VisibilityProxy.setField(mozcView, "rightFrameStubProxy", rightFrameStubProxy);
    int inputFrameHeight = 100;

    {
      resetAll();
      expect(mozcView.getInputFrameHeight()).andStubReturn(inputFrameHeight);
      expect(mozcView.getDropShadowTop()).andStubReturn(dropShadowTop);
      expect(mozcView.getBottomBackground()).andStubReturn(bottomBackground);
      leftFrameStubProxy.flipDropShadowVisibility(View.VISIBLE);
      rightFrameStubProxy.flipDropShadowVisibility(View.VISIBLE);
      dropShadowTop.setVisibility(View.INVISIBLE);
      expect(bottomBackground.getLayoutParams()).andStubReturn(new LayoutParams(0, 0));
      Capture<LayoutParams> captureBottomBackground = new Capture<LayoutParams>();
      bottomBackground.setLayoutParams(capture(captureBottomBackground));

      replayAll();
      VisibilityProxy.setField(mozcView, "fullscreenMode", false);
      mozcView.collapseDropShadowAndBackground();

      verifyAll();
      assertEquals(inputFrameHeight, captureBottomBackground.getValue().height);
    }
    {
      resetAll();
      expect(mozcView.getInputFrameHeight()).andStubReturn(inputFrameHeight);
      expect(mozcView.getDropShadowTop()).andStubReturn(dropShadowTop);
      expect(mozcView.getBottomBackground()).andStubReturn(bottomBackground);
      leftFrameStubProxy.flipDropShadowVisibility(View.VISIBLE);
      rightFrameStubProxy.flipDropShadowVisibility(View.VISIBLE);
      dropShadowTop.setVisibility(View.VISIBLE);
      expect(bottomBackground.getLayoutParams()).andStubReturn(new LayoutParams(0, 0));
      Capture<LayoutParams> captureBottomBackground = new Capture<LayoutParams>();
      bottomBackground.setLayoutParams(capture(captureBottomBackground));

      replayAll();
      VisibilityProxy.setField(mozcView, "fullscreenMode", true);
      mozcView.collapseDropShadowAndBackground();

      verifyAll();
      assertEquals(inputFrameHeight
                       + resources.getDimensionPixelSize(R.dimen.translucent_border_height),
                   captureBottomBackground.getValue().height);
    }
  }

  @SmallTest
  public void testStartAnimation() {
    MozcView mozcView = createViewMockBuilder(MozcView.class)
        .addMockedMethods("getCandidateView", "getSymbolInputView",
                          "expandDropShadowAndBackground", "collapseDropShadowAndBackground",
                          "startDropShadowAnimation")
        .createMock();
    CandidateView candidateView = createViewMockBuilder(CandidateView.class)
        .addMockedMethods("startInAnimation", "startOutAnimation", "getVisibility")
        .createMock();
    SymbolInputView symbolInputView = createViewMockBuilder(SymbolInputView.class)
        .addMockedMethods("startInAnimation", "startOutAnimation", "getVisibility")
        .createMock();

    Animation candidateViewInAnimation = createMock(Animation.class);
    Animation candidateViewOutAnimation = createMock(Animation.class);
    Animation symbolInputViewInAnimation = createMock(Animation.class);
    Animation symbolInputViewOutAnimation = createMock(Animation.class);
    Animation dropShadowCandidateViewInAnimation = createMock(Animation.class);
    Animation dropShadowCandidateViewOutAnimation = createMock(Animation.class);
    Animation dropShadowSymbolInputViewInAnimation = createMock(Animation.class);
    Animation dropShadowSymbolInputViewOutAnimation = createMock(Animation.class);
    VisibilityProxy.setField(mozcView, "candidateViewInAnimation", candidateViewInAnimation);
    VisibilityProxy.setField(mozcView, "candidateViewOutAnimation", candidateViewOutAnimation);
    VisibilityProxy.setField(mozcView, "symbolInputViewInAnimation", symbolInputViewInAnimation);
    VisibilityProxy.setField(mozcView, "symbolInputViewOutAnimation", symbolInputViewOutAnimation);
    VisibilityProxy.setField(mozcView, "dropShadowCandidateViewInAnimation",
                             dropShadowCandidateViewInAnimation);
    VisibilityProxy.setField(mozcView, "dropShadowCandidateViewOutAnimation",
                             dropShadowCandidateViewOutAnimation);
    VisibilityProxy.setField(mozcView, "dropShadowSymbolInputViewInAnimation",
                             dropShadowSymbolInputViewInAnimation);
    VisibilityProxy.setField(mozcView, "dropShadowSymbolInputViewOutAnimation",
                             dropShadowSymbolInputViewOutAnimation);

    // Test startCandidateViewInAnimation.
    {
      resetAll();
      VisibilityProxy.setField(mozcView, "isDropShadowExpanded", true);
      expect(mozcView.getCandidateView()).andReturn(candidateView);
      candidateView.startInAnimation();

      replayAll();
      mozcView.startCandidateViewInAnimation();

      verifyAll();
    }
    {
      resetAll();
      VisibilityProxy.setField(mozcView, "isDropShadowExpanded", false);
      expect(mozcView.getCandidateView()).andReturn(candidateView);
      candidateView.startInAnimation();
      mozcView.expandDropShadowAndBackground();
      mozcView.startDropShadowAnimation(candidateViewInAnimation,
                                        dropShadowCandidateViewInAnimation);

      replayAll();
      mozcView.startCandidateViewInAnimation();

      verifyAll();
    }

    // Test startCandidateViewOutAnimation.
    {
      resetAll();
      expect(mozcView.getCandidateView()).andReturn(candidateView);
      candidateView.startOutAnimation();
      expect(mozcView.getSymbolInputView()).andReturn(symbolInputView);
      expect(symbolInputView.getVisibility()).andReturn(View.VISIBLE);

      replayAll();
      mozcView.startCandidateViewOutAnimation();

      verifyAll();
    }
    {
      resetAll();
      VisibilityProxy.setField(mozcView, "isDropShadowExpanded", true);
      expect(mozcView.getCandidateView()).andReturn(candidateView);
      candidateView.startOutAnimation();
      expect(mozcView.getSymbolInputView()).andReturn(symbolInputView);
      expect(symbolInputView.getVisibility()).andReturn(View.INVISIBLE);
      mozcView.collapseDropShadowAndBackground();
      mozcView.startDropShadowAnimation(candidateViewOutAnimation,
                                        dropShadowCandidateViewOutAnimation);

      replayAll();
      mozcView.startCandidateViewOutAnimation();

      verifyAll();
    }

    // Test startSymbolInputViewInAnimation.
    {
      resetAll();
      VisibilityProxy.setField(mozcView, "isDropShadowExpanded", true);
      expect(mozcView.getSymbolInputView()).andReturn(symbolInputView);
      symbolInputView.startInAnimation();

      replayAll();
      mozcView.startSymbolInputViewInAnimation();

      verifyAll();
    }
    {
      resetAll();
      VisibilityProxy.setField(mozcView, "isDropShadowExpanded", false);
      expect(mozcView.getSymbolInputView()).andReturn(symbolInputView);
      symbolInputView.startInAnimation();

      mozcView.expandDropShadowAndBackground();
      mozcView.startDropShadowAnimation(symbolInputViewInAnimation,
                                        dropShadowSymbolInputViewInAnimation);

      replayAll();
      mozcView.startSymbolInputViewInAnimation();

      verifyAll();
    }

    // Test startSymbolInputViewOutAnimation.
    {
      resetAll();
      expect(mozcView.getSymbolInputView()).andReturn(symbolInputView);
      symbolInputView.startOutAnimation();
      expect(mozcView.getCandidateView()).andReturn(candidateView);
      expect(candidateView.getVisibility()).andReturn(View.VISIBLE);

      replayAll();
      mozcView.startSymbolInputViewOutAnimation();

      verifyAll();
    }
    {
      resetAll();
      VisibilityProxy.setField(mozcView, "isDropShadowExpanded", true);
      expect(mozcView.getSymbolInputView()).andReturn(symbolInputView);
      symbolInputView.startOutAnimation();
      expect(mozcView.getCandidateView()).andReturn(candidateView);
      expect(candidateView.getVisibility()).andReturn(View.INVISIBLE);
      mozcView.collapseDropShadowAndBackground();
      mozcView.startDropShadowAnimation(symbolInputViewOutAnimation,
                                        dropShadowSymbolInputViewOutAnimation);

      replayAll();
      mozcView.startSymbolInputViewOutAnimation();

      verifyAll();
    }
  }

  @SmallTest
  public void testSetKeyboardHeight() {
    MozcView mozcView = createViewMockBuilder(MozcView.class)
        .addMockedMethods("checkInflated", "getResources",
                          "updateInputFrameHeight", "resetHeightDependingComponents")
        .createMock();
    Resources resources = createMock(MockResources.class);
    mozcView.checkInflated();
    expect(mozcView.getResources()).andStubReturn(resources);
    expect(resources.getDimensionPixelSize(R.dimen.ime_window_height)).andStubReturn(300);
    expect(resources.getDimensionPixelSize(R.dimen.input_frame_height)).andStubReturn(200);
    createViewMock(SymbolInputView.class);
    mozcView.updateInputFrameHeight();
    mozcView.resetHeightDependingComponents();

    replayAll();
    mozcView.setKeyboardHeightRatio(120);

    verifyAll();
    assertEquals(360, VisibilityProxy.getField(mozcView, "imeWindowHeight"));
    assertEquals(240, mozcView.getInputFrameHeight());
  }

  @SmallTest
  public void testSideFrameStubProxy() {
    Context context = getInstrumentation().getTargetContext();
    MozcView mozcView =
        MozcView.class.cast(LayoutInflater.from(context).inflate(R.layout.mozc_view, null));
    SideFrameStubProxy sideFrameStubProxy = VisibilityProxy.getField(mozcView,
                                                                     "leftFrameStubProxy");
    OnClickListener buttonClickListener = createMock(OnClickListener.class);
    buttonClickListener.onClick(isA(ImageView.class));
    replayAll();

    // Not inflated.
    assertNotNull(mozcView.findViewById(R.id.stub_left_frame));
    assertNull(mozcView.findViewById(R.id.left_frame));
    assertNull(mozcView.findViewById(R.id.dropshadow_left_short_top));
    assertNull(mozcView.findViewById(R.id.dropshadow_left_long_top));
    assertNull(mozcView.findViewById(R.id.left_adjust_button));
    assertNull(mozcView.findViewById(R.id.left_dropshadow_short));
    assertNull(mozcView.findViewById(R.id.left_dropshadow_long));
    assertFalse(Boolean.class.cast(VisibilityProxy.getField(sideFrameStubProxy, "inflated")));
    sideFrameStubProxy.setButtonOnClickListener(buttonClickListener);
    sideFrameStubProxy.flipDropShadowVisibility(View.VISIBLE);
    sideFrameStubProxy.setDropShadowHeight(3, 4);
    sideFrameStubProxy.resetAdjustButtonBottomMargin(100);

    // Inflate.
    sideFrameStubProxy.setFrameVisibility(View.VISIBLE);
    assertNull(mozcView.findViewById(R.id.stub_left_frame));
    assertNotNull(mozcView.findViewById(R.id.left_frame));
    assertNotNull(mozcView.findViewById(R.id.dropshadow_left_short_top));
    assertNotNull(mozcView.findViewById(R.id.dropshadow_left_long_top));
    assertNotNull(mozcView.findViewById(R.id.left_adjust_button));
    assertNotNull(mozcView.findViewById(R.id.left_dropshadow_short));
    assertNotNull(mozcView.findViewById(R.id.left_dropshadow_long));
    assertTrue(Boolean.class.cast(VisibilityProxy.getField(sideFrameStubProxy, "inflated")));
    ImageView imageView = ImageView.class.cast(mozcView.findViewById(R.id.left_adjust_button));
    imageView.performClick();
    assertEquals(View.VISIBLE, mozcView.findViewById(R.id.left_frame).getVisibility());
    FrameLayout.LayoutParams layoutParams = FrameLayout.LayoutParams.class.cast(
        imageView.getLayoutParams());
    assertEquals((100 - layoutParams.height) / 2, layoutParams.bottomMargin);
    assertEquals(View.VISIBLE, mozcView.findViewById(R.id.left_dropshadow_short).getVisibility());
    assertEquals(View.INVISIBLE, mozcView.findViewById(R.id.left_dropshadow_long).getVisibility());
    assertEquals(3, mozcView.findViewById(R.id.left_dropshadow_short).getLayoutParams().height);
    assertEquals(4, mozcView.findViewById(R.id.left_dropshadow_long).getLayoutParams().height);

    verifyAll();

    sideFrameStubProxy.setFrameVisibility(View.INVISIBLE);
    sideFrameStubProxy.flipDropShadowVisibility(View.INVISIBLE);
    sideFrameStubProxy.setDropShadowHeight(5, 6);
    Animation animation1 = createNiceMock(Animation.class);
    Animation animation2 = createNiceMock(Animation.class);
    sideFrameStubProxy.startDropShadowAnimation(animation1, animation2);

    assertEquals(View.INVISIBLE, mozcView.findViewById(R.id.left_frame).getVisibility());
    assertEquals(View.INVISIBLE, mozcView.findViewById(R.id.left_dropshadow_short).getVisibility());
    assertEquals(View.VISIBLE, mozcView.findViewById(R.id.left_dropshadow_long).getVisibility());
    assertEquals(5, mozcView.findViewById(R.id.left_dropshadow_short).getLayoutParams().height);
    assertEquals(6, mozcView.findViewById(R.id.left_dropshadow_long).getLayoutParams().height);
    assertEquals(animation1, mozcView.findViewById(R.id.left_dropshadow_short).getAnimation());
    assertEquals(animation2, mozcView.findViewById(R.id.left_dropshadow_long).getAnimation());
  }

  @SmallTest
  public void testCandidateViewListener() {
    ViewEventListener eventListener = createMock(ViewEventListener.class);

    View keyboardView = new View(getInstrumentation().getTargetContext());

    android.view.animation.Interpolator foldKeyboardViewInterpolator =
        createNiceMock(android.view.animation.Interpolator.class);
    android.view.animation.Interpolator expandKeyboardViewInterpolator =
        createNiceMock(android.view.animation.Interpolator.class);

    LayoutParamsAnimator layoutParamsAnimator = createMockBuilder(LayoutParamsAnimator.class)
        .withConstructor(Handler.class)
        .withArgs(new Handler())
        .createMock();
    Capture<InterpolationListener> interpolationListenerCapture =
        new Capture<InterpolationListener>();
    MozcView mozcView = createViewMock(MozcView.class);
    CompoundButton inputFrameFoldButton = createViewMock(CompoundButton.class);

    MozcView.InputFrameFoldButtonClickListener candidateViewListener =
        mozcView.new InputFrameFoldButtonClickListener(
            eventListener, keyboardView,
            300, foldKeyboardViewInterpolator, 400, expandKeyboardViewInterpolator,
            layoutParamsAnimator, 0.1f);

    {
      keyboardView.layout(0, 0, 250, 200);
      eventListener.onFireFeedbackEvent(FeedbackEvent.INPUTVIEW_FOLD);
      layoutParamsAnimator.startAnimation(same(keyboardView), capture(interpolationListenerCapture),
          same(foldKeyboardViewInterpolator), eq(300L), eq(0L));
      expect(mozcView.getInputFrameHeight()).andStubReturn(200);
      inputFrameFoldButton.setChecked(true);

      replayAll();

      candidateViewListener.onClick(inputFrameFoldButton);

      verifyAll();

      InterpolationListener interpolationListener = interpolationListenerCapture.getValue();
      assertSame(HeightLinearInterpolationListener.class, interpolationListener.getClass());
      assertEquals(200, VisibilityProxy.getField(interpolationListener, "fromHeight"));
      assertEquals(0, VisibilityProxy.getField(interpolationListener, "toHeight"));
    }

    {
      resetAll();
      keyboardView.layout(0, 0, 250, 100);
      expect(mozcView.getInputFrameHeight()).andStubReturn(200);
      interpolationListenerCapture.reset();
      eventListener.onFireFeedbackEvent(FeedbackEvent.INPUTVIEW_EXPAND);
      inputFrameFoldButton.setChecked(false);

      layoutParamsAnimator.startAnimation(same(keyboardView), capture(interpolationListenerCapture),
          same(expandKeyboardViewInterpolator), eq(400L), eq(0L));
      replayAll();

      candidateViewListener.onClick(inputFrameFoldButton);

      verifyAll();

      InterpolationListener interpolationListener = interpolationListenerCapture.getValue();
      assertSame(HeightLinearInterpolationListener.class, interpolationListener.getClass());
      assertEquals(100, VisibilityProxy.getField(interpolationListener, "fromHeight"));
      assertEquals(200, VisibilityProxy.getField(interpolationListener, "toHeight"));
    }
  }
}
