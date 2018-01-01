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

import static org.mozc.android.inputmethod.japanese.testing.MozcMatcher.sameOptional;
import static org.easymock.EasyMock.anyFloat;
import static org.easymock.EasyMock.anyInt;
import static org.easymock.EasyMock.anyObject;
import static org.easymock.EasyMock.capture;
import static org.easymock.EasyMock.eq;
import static org.easymock.EasyMock.expect;
import static org.easymock.EasyMock.expectLastCall;
import static org.easymock.EasyMock.isA;
import static org.easymock.EasyMock.same;

import org.mozc.android.inputmethod.japanese.CandidateViewManager.KeyboardCandidateViewHeightListener;
import org.mozc.android.inputmethod.japanese.FeedbackManager.FeedbackEvent;
import org.mozc.android.inputmethod.japanese.InOutAnimatedFrameLayout.VisibilityChangeListener;
import org.mozc.android.inputmethod.japanese.LayoutParamsAnimator.InterpolationListener;
import org.mozc.android.inputmethod.japanese.MozcView.DimensionPixelSize;
import org.mozc.android.inputmethod.japanese.MozcView.HeightLinearInterpolationListener;
import org.mozc.android.inputmethod.japanese.ViewManagerInterface.LayoutAdjustment;
import org.mozc.android.inputmethod.japanese.emoji.EmojiProviderType;
import org.mozc.android.inputmethod.japanese.keyboard.KeyEventHandler;
import org.mozc.android.inputmethod.japanese.keyboard.KeyState.MetaState;
import org.mozc.android.inputmethod.japanese.keyboard.Keyboard;
import org.mozc.android.inputmethod.japanese.keyboard.Keyboard.KeyboardSpecification;
import org.mozc.android.inputmethod.japanese.keyboard.KeyboardActionListener;
import org.mozc.android.inputmethod.japanese.keyboard.KeyboardView;
import org.mozc.android.inputmethod.japanese.keyboard.Row;
import org.mozc.android.inputmethod.japanese.model.SymbolCandidateStorage;
import org.mozc.android.inputmethod.japanese.model.SymbolCandidateStorage.SymbolHistoryStorage;
import org.mozc.android.inputmethod.japanese.model.SymbolMajorCategory;
import org.mozc.android.inputmethod.japanese.model.SymbolMinorCategory;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateList;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateWord;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Command;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.CompositionMode;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Output;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Preedit;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Preedit.Segment;
import org.mozc.android.inputmethod.japanese.resources.R;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;
import org.mozc.android.inputmethod.japanese.testing.Parameter;
import org.mozc.android.inputmethod.japanese.testing.VisibilityProxy;
import org.mozc.android.inputmethod.japanese.ui.SideFrameStubProxy;
import org.mozc.android.inputmethod.japanese.view.MozcImageView;
import org.mozc.android.inputmethod.japanese.view.Skin;
import org.mozc.android.inputmethod.japanese.view.SkinType;
import com.google.common.base.Optional;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.inputmethodservice.InputMethodService.Insets;
import android.os.Build;
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
import org.easymock.IMockBuilder;

import java.util.Collections;
import java.util.EnumSet;

/**
 */
public class MozcViewTest extends InstrumentationTestCaseWithMock {

  private IMockBuilder<CandidateViewManager> createCandidateViewManagerMockBuilder() {
    return createMockBuilder(CandidateViewManager.class)
        .withConstructor(CandidateView.class, FloatingCandidateView.class)
        .withArgs(createViewMockBuilder(CandidateView.class).createNiceMock(),
                  createViewMockBuilder(FloatingCandidateView.class).createNiceMock());
  }

  @SmallTest
  public void testSetViewEventListener() {
    Context context = getInstrumentation().getContext();

    CandidateViewManager candidateViewManager =
        createCandidateViewManagerMockBuilder().createMock();
    ViewEventListener viewEventListener = createMock(ViewEventListener.class);
    OnClickListener widenButtonClickListener = createMock(OnClickListener.class);
    SymbolInputView symbolInputView = createViewMock(SymbolInputView.class);
    OnClickListener leftAdjustButtonClickListener = createMock(OnClickListener.class);
    OnClickListener rightAdjustButtonClickListener = createMock(OnClickListener.class);
    OnClickListener mictophoneButtonClickListener = createMock(OnClickListener.class);
    NarrowFrameView narrowFrameView = createViewMock(NarrowFrameView.class);
    MozcImageView microphoneButton = new MozcImageView(context);
    MozcView mozcView = createViewMockBuilder(MozcView.class)
        .addMockedMethods(
            "checkInflated", "getSymbolInputView", "getNarrowFrame", "getMicrophoneButton")
        .createMock();
    mozcView.candidateViewManager = candidateViewManager;

    resetAll();

    symbolInputView.setEventListener(
        same(viewEventListener), isA(OnClickListener.class), isA(OnClickListener.class));
    candidateViewManager.setEventListener(
        same(viewEventListener), isA(KeyboardCandidateViewHeightListener.class));
    mozcView.checkInflated();
    expectLastCall().asStub();
    expect(mozcView.getSymbolInputView()).andStubReturn(symbolInputView);
    expect(mozcView.getNarrowFrame()).andStubReturn(narrowFrameView);
    expect(mozcView.getMicrophoneButton()).andStubReturn(microphoneButton);
    narrowFrameView.setEventListener(viewEventListener, widenButtonClickListener);

    replayAll();

    mozcView.setEventListener(viewEventListener, widenButtonClickListener,
                              leftAdjustButtonClickListener, rightAdjustButtonClickListener,
                              mictophoneButtonClickListener);

    verifyAll();

    assertEquals(leftAdjustButtonClickListener,
                 Optional.class.cast(VisibilityProxy.getField(
                     VisibilityProxy.getField(mozcView, "leftFrameStubProxy"),
                     "buttonOnClickListener")).get());
    assertEquals(rightAdjustButtonClickListener,
                 Optional.class.cast(VisibilityProxy.getField(
                     VisibilityProxy.getField(mozcView, "rightFrameStubProxy"),
                     "buttonOnClickListener")).get());
  }

  @SmallTest
  public void testSetKeyEventHandler() {
    KeyEventHandler keyEventHandler = new KeyEventHandler(
        Looper.getMainLooper(), createNiceMock(KeyboardActionListener.class), 0, 0, 0);
    KeyboardView keyboardView = createViewMock(KeyboardView.class);
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
    Keyboard keyboard = new Keyboard(
        Optional.<String>absent(),
        Collections.<Row>emptyList(), 1, KeyboardSpecification.TWELVE_KEY_TOGGLE_KANA);
    KeyboardView keyboardView = createViewMock(KeyboardView.class);
    expect(keyboardView.getKeyboard()).andStubReturn(Optional.of(keyboard));
    MozcView mozcView = createViewMockBuilder(MozcView.class)
        .addMockedMethods("checkInflated", "getKeyboardView")
        .createMock();
    mozcView.checkInflated();
    expectLastCall().asStub();
    expect(mozcView.getKeyboardView()).andStubReturn(keyboardView);

    replayAll();

    assertSame(keyboard, mozcView.getKeyboard());

    verifyAll();
  }

  @SmallTest
  public void testSetJapaneseKeyboard() {
    Keyboard keyboard = new Keyboard(
        Optional.<String>absent(),
        Collections.<Row>emptyList(), 1, KeyboardSpecification.TWELVE_KEY_TOGGLE_KANA);
    KeyboardView keyboardView = createViewMock(KeyboardView.class);
    NarrowFrameView narrowFrame = createViewMock(NarrowFrameView.class);
    CandidateViewManager candidateViewManager =
        createCandidateViewManagerMockBuilder().createNiceMock();
    MozcView mozcView = createViewMockBuilder(MozcView.class)
        .addMockedMethods("checkInflated", "getKeyboardView", "getNarrowFrame")
        .createMock();
    mozcView.candidateViewManager = candidateViewManager;

    resetAll();

    mozcView.checkInflated();
    expectLastCall().asStub();
    expect(mozcView.getKeyboardView()).andStubReturn(keyboardView);
    expect(mozcView.getNarrowFrame()).andStubReturn(narrowFrame);
    keyboardView.setKeyboard(keyboard);
    narrowFrame.setHardwareCompositionButtonImage(CompositionMode.HIRAGANA);

    replayAll();

    mozcView.setKeyboard(keyboard);

    verifyAll();
  }

  @SmallTest
  public void testSetEmojiEnabled() {
    SymbolInputView symbolInputView = createViewMock(SymbolInputView.class);
    MozcView mozcView = createViewMockBuilder(MozcView.class)
        .addMockedMethods("checkInflated", "getSymbolInputView")
        .createMock();

    for (boolean unicodeEmojiEnabled : new boolean[] {false, true}) {
      for (boolean carrierEmojiEnabled : new boolean[] {false, true}) {
        resetAll();
        symbolInputView.setEmojiEnabled(unicodeEmojiEnabled, carrierEmojiEnabled);
        mozcView.checkInflated();
        expectLastCall().asStub();
        expect(mozcView.getSymbolInputView()).andStubReturn(symbolInputView);
        replayAll();

        mozcView.setEmojiEnabled(unicodeEmojiEnabled, carrierEmojiEnabled);

        verifyAll();
      }
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

  @SuppressWarnings("deprecation")
  @SmallTest
  public void testSetSkinType() {
    Context context = getInstrumentation().getTargetContext();

    CandidateViewManager candidateViewManager =
        createCandidateViewManagerMockBuilder().createMock();
    KeyboardView keyboardView = createViewMock(KeyboardView.class);
    SymbolInputView symbolInputView = createViewMock(SymbolInputView.class);
    NarrowFrameView narrowFrameView = createViewMock(NarrowFrameView.class);
    MozcImageView microphoneButton = createViewMock(MozcImageView.class);
    View separatorView = createViewMock(View.class);
    View buttonFrame = createViewMock(View.class);
    MozcView mozcView = createViewMockBuilder(MozcView.class)
        .addMockedMethods("checkInflated", "getKeyboardView", "getSymbolInputView",
                          "getNarrowFrame", "getMicrophoneButton", "getButtonFrame",
                          "getMicrophoneButton", "getKeyboardFrameSeparator")
        .createMock();
    mozcView.candidateViewManager = candidateViewManager;

    for (SkinType skinType : SkinType.values()) {
      resetAll();
      mozcView.checkInflated();
      expectLastCall().asStub();
      expect(mozcView.getKeyboardView()).andStubReturn(keyboardView);
      expect(mozcView.getSymbolInputView()).andStubReturn(symbolInputView);
      expect(mozcView.getNarrowFrame()).andStubReturn(narrowFrameView);
      expect(mozcView.getMicrophoneButton()).andStubReturn(microphoneButton);
      expect(mozcView.getButtonFrame()).andStubReturn(buttonFrame);
      expect(mozcView.getKeyboardFrameSeparator()).andStubReturn(separatorView);
      Skin skin = skinType.getSkin(context.getResources());
      keyboardView.setSkin(skin);
      candidateViewManager.setSkin(skin);
      symbolInputView.setSkin(skin);
      microphoneButton.setSkin(skin);
      microphoneButton.setBackgroundDrawable(anyObject(Drawable.class));
      narrowFrameView.setSkin(skin);
      buttonFrame.setBackgroundDrawable(anyObject(Drawable.class));
      separatorView.setBackgroundDrawable(anyObject(Drawable.class));
      replayAll();

      mozcView.setSkin(skin);

      verifyAll();
    }
  }

  @SmallTest
  public void testSetPopupEnabled() {
    KeyboardView keyboardView = createViewMock(KeyboardView.class);
    SymbolInputView symbolInputView = createViewMock(SymbolInputView.class);
    MozcView mozcView = createViewMockBuilder(MozcView.class)
        .addMockedMethods("checkInflated", "getKeyboardView", "getSymbolInputView")
        .createMock();

    boolean parameters[] = { true, false };
    for (boolean popupEnabled : parameters) {
      resetAll();
      keyboardView.setPopupEnabled(popupEnabled);
      symbolInputView.setPopupEnabled(popupEnabled);
      mozcView.checkInflated();
      expectLastCall().asStub();
      expect(mozcView.getKeyboardView()).andStubReturn(keyboardView);
      expect(mozcView.getSymbolInputView()).andStubReturn(symbolInputView);
      replayAll();

      mozcView.setPopupEnabled(popupEnabled);

      verifyAll();
    }
  }

  @SuppressWarnings("unchecked")
  @SmallTest
  public void testSetCommand() {
    CandidateViewManager candidateViewManager =
        createCandidateViewManagerMockBuilder().createMock();
    MozcView mozcView = createViewMockBuilder(MozcView.class)
        .addMockedMethods("checkInflated", "updateMetaStatesBasedOnOutput")
        .createMock();
    mozcView.candidateViewManager = candidateViewManager;

    // Set non-empty command.
    {
      Command command = Command.newBuilder()
          .setOutput(Output.newBuilder()
              .setAllCandidateWords(CandidateList.newBuilder()
                  .addCandidates(CandidateWord.getDefaultInstance())))
          .buildPartial();

      resetAll();
      candidateViewManager.update(command);
      mozcView.checkInflated();
      expectLastCall().asStub();
      mozcView.updateMetaStatesBasedOnOutput(anyObject(Output.class));
      replayAll();

      mozcView.setCommand(command);

      verifyAll();
    }

    // Set empty command.
    {
      Command emptyCommand = Command.getDefaultInstance();

      resetAll();
      mozcView.checkInflated();
      expectLastCall().asStub();
      candidateViewManager.update(emptyCommand);
      mozcView.updateMetaStatesBasedOnOutput(anyObject(Output.class));
      replayAll();

      mozcView.setCommand(emptyCommand);

      verifyAll();
    }
  }

  @SmallTest
  public void testUpdateMetaStatesBasedOnOutput() {
    MozcView mozcView = createViewMockBuilder(MozcView.class)
        .addMockedMethods("getKeyboardView")
        .createMock();
    KeyboardView keyboard = createViewMockBuilder(KeyboardView.class)
        .addMockedMethod("updateMetaStates")
        .createMock();

    {
      // No preedit field -> Remove COMPOSING
      resetAll();
      keyboard.updateMetaStates(Collections.<MetaState>emptySet(), EnumSet.of(MetaState.COMPOSING));
      expect(mozcView.getKeyboardView()).andStubReturn(keyboard);
      replayAll();
      mozcView.updateMetaStatesBasedOnOutput(Output.getDefaultInstance());
      verifyAll();
    }
    {
      // No segments -> Remove COMPOSING
      resetAll();
      keyboard.updateMetaStates(Collections.<MetaState>emptySet(), EnumSet.of(MetaState.COMPOSING));
      expect(mozcView.getKeyboardView()).andStubReturn(keyboard);
      replayAll();
      mozcView.updateMetaStatesBasedOnOutput(
          Output.newBuilder().setPreedit(Preedit.getDefaultInstance()).buildPartial());
      verifyAll();
    }
    {
      // With segments -> Add COMPOSING
      resetAll();
      keyboard.updateMetaStates(EnumSet.of(MetaState.COMPOSING), Collections.<MetaState>emptySet());
      expect(mozcView.getKeyboardView()).andStubReturn(keyboard);
      replayAll();
      mozcView.updateMetaStatesBasedOnOutput(
          Output.newBuilder().setPreedit(Preedit.newBuilder().addSegment(
              Segment.getDefaultInstance()).buildPartial()).buildPartial());
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

      mozcView.getSymbolInputView().startInAnimation();
    }

    mozcView.reset();

    assertEquals(View.VISIBLE, mozcView.getKeyboardFrame().getVisibility());
    assertEquals(LayoutParams.WRAP_CONTENT, mozcView.getKeyboardFrame().getLayoutParams().height);
    assertEquals(View.VISIBLE, mozcView.getKeyboardView().getVisibility());
    assertTrue(mozcView.getKeyboardView().getLayoutParams().height > 0);
    assertFalse(mozcView.candidateViewManager.isKeyboardCandidateViewVisible());
    assertEquals(View.GONE, mozcView.getSymbolInputView().getVisibility());
    assertNull(mozcView.getSymbolInputView().getAnimation());
  }

  @SmallTest
  public void testSetGlobeButtonEnabled() {
    LayoutInflater inflater = LayoutInflater.from(getInstrumentation().getTargetContext());
    MozcView mozcView = MozcView.class.cast(inflater.inflate(R.layout.mozc_view, null));

    assertEquals(EnumSet.of(MetaState.NO_GLOBE), mozcView.getKeyboardView().getMetaStates());

    mozcView.setGlobeButtonEnabled(false);
    assertEquals(EnumSet.of(MetaState.NO_GLOBE), mozcView.getKeyboardView().getMetaStates());

    mozcView.setGlobeButtonEnabled(true);
    assertEquals(EnumSet.of(MetaState.GLOBE), mozcView.getKeyboardView().getMetaStates());

    mozcView.getKeyboardView().updateMetaStates(EnumSet.of(MetaState.ACTION_GO, MetaState.CAPS_LOCK,
        MetaState.VARIATION_EMAIL_ADDRESS, MetaState.GLOBE), EnumSet.allOf(MetaState.class));
    mozcView.setGlobeButtonEnabled(false);
    assertEquals(EnumSet.of(MetaState.NO_GLOBE,
                            MetaState.ACTION_GO,
                            MetaState.CAPS_LOCK,
                            MetaState.VARIATION_EMAIL_ADDRESS),
                 mozcView.getKeyboardView().getMetaStates());
    mozcView.setGlobeButtonEnabled(true);
    assertEquals(EnumSet.of(MetaState.ACTION_GO,
                            MetaState.CAPS_LOCK,
                            MetaState.VARIATION_EMAIL_ADDRESS,
                            MetaState.GLOBE),
                 mozcView.getKeyboardView().getMetaStates());

    mozcView.reset();
    assertEquals(EnumSet.of(MetaState.NO_GLOBE), mozcView.getKeyboardView().getMetaStates());
  }

  @SmallTest
  public void testGetVisibleViewHeight() {
    Resources resources = getInstrumentation().getTargetContext().getResources();

    int imeWindowHeight = resources.getDimensionPixelSize(R.dimen.ime_window_height);
    int inputFrameHeight =
        resources.getDimensionPixelSize(R.dimen.input_frame_height);
    int buttonFrameHeight = resources.getDimensionPixelSize(R.dimen.button_frame_height);
    int symbolInputFrameHeight = inputFrameHeight + buttonFrameHeight;
    int narrowFrameHeight = MozcView.getNarrowFrameHeight(resources);
    int narrowImeWindowHeight =
        narrowFrameHeight + resources.getDimensionPixelSize(R.dimen.narrow_candidate_window_height);

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
        new TestData(false, View.GONE, View.GONE, inputFrameHeight + buttonFrameHeight),
        // If either candidate view or symbol input view are visible, the height of the view
        // equals to ime window height.
        new TestData(false, View.VISIBLE, View.GONE, imeWindowHeight),
        new TestData(false, View.GONE, View.VISIBLE, symbolInputFrameHeight),

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
    mozcView.allowFloatingCandidateMode = false;
    for (TestData testData : testDataList) {
      mozcView.setLayoutAdjustmentAndNarrowMode(LayoutAdjustment.FILL, testData.narrowMode);
      mozcView.candidateViewManager.keyboardCandidateView.setVisibility(
          testData.candidateViewVisibility);
      mozcView.getSymbolInputView().setVisibility(testData.symbolInputViewVisibility);
      assertEquals(testData.toString(),
                   testData.expectedHeight, mozcView.getVisibleViewHeight());
    }
  }

  @SmallTest
  public void testSymbolInputViewInitializationLazily() {
    LayoutInflater inflater = LayoutInflater.from(getInstrumentation().getTargetContext());
    MozcView mozcView = MozcView.class.cast(inflater.inflate(R.layout.mozc_view, null));
    SymbolCandidateStorage candidateStorage = createMockBuilder(SymbolCandidateStorage.class)
        .withConstructor(SymbolHistoryStorage.class)
        .withArgs(createMock(SymbolHistoryStorage.class))
        .createMock();
    expect(candidateStorage.getCandidateList(isA(SymbolMinorCategory.class)))
        .andStubReturn(CandidateList.getDefaultInstance());
    mozcView.setSymbolCandidateStorage(candidateStorage);
    replayAll();

    // At first, symbol input view is not inflated.
    assertFalse(mozcView.getSymbolInputView().isInflated());

    // Once symbol input view is shown, the view (and its children) should be inflated.
    assertTrue(mozcView.showSymbolInputView(Optional.<SymbolMajorCategory>absent()));
    assertTrue(mozcView.getSymbolInputView().isInflated());

    // Even after closing the view, the inflation state should be kept.
    mozcView.hideSymbolInputView();
    assertTrue(mozcView.getSymbolInputView().isInflated());
  }

  @SmallTest
  public void testUpdateInputFrameHeight() {
    Resources resources = getInstrumentation().getTargetContext().getResources();

    int imeWindowHeight = resources.getDimensionPixelSize(R.dimen.ime_window_height);
    int inputFrameHeight = resources.getDimensionPixelSize(R.dimen.input_frame_height);
    int buttonFrameHeight = resources.getDimensionPixelSize(R.dimen.button_frame_height);
    int narrowFrameHeight =  MozcView.getNarrowFrameHeight(resources);
    int narrowImeWindowHeight =
        narrowFrameHeight + resources.getDimensionPixelSize(R.dimen.narrow_candidate_window_height);

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
        new TestData(true, true, View.GONE, narrowFrameHeight),
        // If candidate view is visible, the height of the view
        // equals to narrow ime window height.
        new TestData(true, true, View.VISIBLE, narrowImeWindowHeight),

        // At fullscreen mode and not narrow mode
        // If candidate view is hidden, the height of the view
        // equals to input frame height.
        new TestData(true, false, View.GONE, inputFrameHeight + buttonFrameHeight),
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
    mozcView.allowFloatingCandidateMode = false;
    for (TestData testData : testDataList) {
      mozcView.setFullscreenMode(testData.fullscreenMode);
      mozcView.resetFullscreenMode();
      mozcView.setLayoutAdjustmentAndNarrowMode(LayoutAdjustment.FILL, testData.narrowMode);
      mozcView.candidateViewManager.keyboardCandidateView.setVisibility(
          testData.candidateViewVisibility);
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
    CandidateViewManager candidateViewManager =
        createCandidateViewManagerMockBuilder().createMock();
    mozcView.allowFloatingCandidateMode = false;
    mozcView.candidateViewManager = candidateViewManager;
    View overlayView =  mozcView.getOverlayView();
    LinearLayout textInputFrame = mozcView.getTextInputFrame();
    VisibilityChangeListener onVisibilityChangeListener = mozcView.onVisibilityChangeListener;

    resetAll();
    candidateViewManager.setOnVisibilityChangeListener(sameOptional(onVisibilityChangeListener));
    candidateViewManager.setExtractedMode(true);
    expect(candidateViewManager.isKeyboardCandidateViewVisible()).andStubReturn(false);
    replayAll();
    mozcView.setFullscreenMode(true);
    mozcView.resetFullscreenMode();
    verifyAll();
    assertEquals(View.GONE, overlayView.getVisibility());
    assertEquals(FrameLayout.LayoutParams.WRAP_CONTENT, textInputFrame.getLayoutParams().height);
    assertSame(onVisibilityChangeListener,
               mozcView.getSymbolInputView().onVisibilityChangeListener);

    resetAll();
    candidateViewManager.setOnVisibilityChangeListener(
        Optional.<InOutAnimatedFrameLayout.VisibilityChangeListener>absent());
    expect(candidateViewManager.isKeyboardCandidateViewVisible()).andStubReturn(false);
    candidateViewManager.setExtractedMode(false);
    replayAll();
    mozcView.setFullscreenMode(false);
    mozcView.resetFullscreenMode();
    verifyAll();
    assertEquals(View.VISIBLE, overlayView.getVisibility());
    assertEquals(FrameLayout.LayoutParams.MATCH_PARENT, textInputFrame.getLayoutParams().height);
    assertNull(mozcView.getSymbolInputView().onVisibilityChangeListener);
  }

  @SmallTest
  public void testOnVisibilityChangeListener() {
    MozcView mozcView = createViewMockBuilder(MozcView.class)
        .addMockedMethod("updateInputFrameHeight").createMock();
    VisibilityChangeListener onVisibilityChangeListener = mozcView.onVisibilityChangeListener;

    mozcView.updateInputFrameHeight();

    replayAll();

    onVisibilityChangeListener.onVisibilityChange();

    verifyAll();
  }

  @SmallTest
  public void testSwitchNarrowMode() {
    Context context = getInstrumentation().getTargetContext();
    MozcView mozcView =
        MozcView.class.cast(LayoutInflater.from(context).inflate(R.layout.mozc_view, null));
    View keyboardFrame = mozcView.getKeyboardFrame();
    View inputFrameButton =
        mozcView.candidateViewManager.keyboardCandidateView.getInputFrameFoldButton();
    View narrowFrame = mozcView.getNarrowFrame();

    mozcView.setLayoutAdjustmentAndNarrowMode(LayoutAdjustment.FILL, true);
    assertTrue(mozcView.isNarrowMode());
    assertEquals(View.GONE, keyboardFrame.getVisibility());
    assertEquals(View.GONE, inputFrameButton.getVisibility());
    assertEquals(MozcView.IS_NARROW_FRAME_ENABLED ? View.VISIBLE : View.GONE,
                 narrowFrame.getVisibility());

    mozcView.setLayoutAdjustmentAndNarrowMode(LayoutAdjustment.FILL, false);
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
      mozcView.setLayoutAdjustmentAndNarrowMode(LayoutAdjustment.FILL, false);
      Rect keyboardSize = mozcView.getKeyboardSize();
      assertEquals(resources.getDisplayMetrics().widthPixels, keyboardSize.width());
      assertEquals(resources.getDimensionPixelSize(R.dimen.input_frame_height),
                   keyboardSize.height());
    }

    {
      mozcView.setLayoutAdjustmentAndNarrowMode(LayoutAdjustment.RIGHT, false);
      Rect keyboardSize = mozcView.getKeyboardSize();
      assertEquals(resources.getDimensionPixelSize(R.dimen.ime_window_partial_width),
                   keyboardSize.width());
      assertEquals(resources.getDimensionPixelSize(R.dimen.input_frame_height),
                   keyboardSize.height());
    }
  }

  @SmallTest
  public void testSetLayoutAdjustmentAndNarrowMode() {
    Context context = getInstrumentation().getTargetContext();
    Resources resources = context.getResources();

    MozcView mozcView = createViewMockBuilder(MozcView.class)
        .addMockedMethods("checkInflated", "getForegroundFrame", "getInputFrameHeight",
                          "updateBackgroundColor", "updateInputFrameHeight", "getSymbolInputView",
                          "getKeyboardFrame", "getNarrowFrame", "getButtonFrame",
                          "resetKeyboardFrameVisibility")
        .createMock();
    CandidateViewManager candidateViewManager =
        createCandidateViewManagerMockBuilder().createMock();
    mozcView.candidateViewManager = candidateViewManager;
    View foreGroundFrame = createViewMockBuilder(View.class)
        .addMockedMethods("getLayoutParams", "setLayoutParams")
        .createMock();
    View keyboardFrame = new View(context);
    NarrowFrameView narrowFrameView = new NarrowFrameView(context);
    SideFrameStubProxy leftFrameStubProxy = createMock(SideFrameStubProxy.class);
    SideFrameStubProxy rightFrameStubProxy = createMock(SideFrameStubProxy.class);
    VisibilityProxy.setField(mozcView, "leftFrameStubProxy", leftFrameStubProxy);
    VisibilityProxy.setField(mozcView, "rightFrameStubProxy", rightFrameStubProxy);
    SymbolInputView symbolInputView = createViewMockBuilder(SymbolInputView.class)
        .addMockedMethods("setCandidateTextDimension")
        .createMock();
    View buttonFrame = new View(context);

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

    boolean isCursorAnchorInfoEnabled = Build.VERSION.SDK_INT >= 21;
    resetAll();
    candidateViewManager.setAllowFloatingMode(isCursorAnchorInfoEnabled);
    replayAll();
    mozcView.setCursorAnchorInfoEnabled(isCursorAnchorInfoEnabled);
    verifyAll();

    for (TestData testData : testDataList) {
      Capture<FrameLayout.LayoutParams> layoutCapture = new Capture<FrameLayout.LayoutParams>();
      resetAll();
      mozcView.checkInflated();
      expectLastCall().atLeastOnce();
      expect(mozcView.getInputFrameHeight()).andStubReturn(100);
      expect(mozcView.getForegroundFrame()).andReturn(foreGroundFrame);
      expect(foreGroundFrame.getLayoutParams()).andReturn(new FrameLayout.LayoutParams(0, 0));
      expect(mozcView.getSymbolInputView()).andStubReturn(symbolInputView);
      candidateViewManager.setCandidateTextDimension(anyFloat(), anyFloat());
      candidateViewManager.setNarrowMode(testData.narrowMode);
      symbolInputView.setCandidateTextDimension(anyFloat(), anyFloat());
      foreGroundFrame.setLayoutParams(capture(layoutCapture));
      leftFrameStubProxy.setFrameVisibility(testData.expectLeftFrameVisibility);
      rightFrameStubProxy.setFrameVisibility(testData.expectRightFrameVisibility);
      expect(mozcView.getKeyboardFrame()).andStubReturn(keyboardFrame);
      expect(mozcView.getNarrowFrame()).andStubReturn(narrowFrameView);
      expect(mozcView.getButtonFrame()).andStubReturn(buttonFrame);
      if (!testData.narrowMode) {
        mozcView.resetKeyboardFrameVisibility();
      }
      mozcView.updateInputFrameHeight();
      mozcView.updateBackgroundColor();

      replayAll();

      mozcView.setLayoutAdjustmentAndNarrowMode(testData.layoutAdjustment, testData.narrowMode);
      verifyAll();
      assertEquals(testData.toString(), testData.layoutAdjustment, mozcView.layoutAdjustment);
      assertEquals(testData.toString(),
                   testData.expectForegroundFrameWidth, layoutCapture.getValue().width);
      assertEquals(testData.toString(),
                   testData.expectForegroundFrameGravity, layoutCapture.getValue().gravity);
      assertEquals(testData.narrowMode ? View.GONE : View.VISIBLE, keyboardFrame.getVisibility());
      boolean narrowFrameVisible = testData.narrowMode && MozcView.IS_NARROW_FRAME_ENABLED;
      assertEquals(narrowFrameVisible ? View.VISIBLE : View.GONE, narrowFrameView.getVisibility());
    }
  }

  @SmallTest
  public void testUpdateBackgroundColor() {
    MozcView mozcView = createViewMockBuilder(MozcView.class)
        .addMockedMethods("getBottomBackground", "isFloatingMode")
        .createMock();

    View bottomBackground = createViewMockBuilder(View.class)
        .addMockedMethods("setBackgroundResource")
        .createMock();

    class TestData extends Parameter {
      final boolean fullscreenMode;
      final boolean narrowMode;
      final boolean isFloatable;

      final int expectResourceId;

      TestData(boolean fullscreenMode, boolean narrowMode, boolean isFloatable,
               int expectedResourceId) {
        this.fullscreenMode = fullscreenMode;
        this.narrowMode = narrowMode;
        this.isFloatable = isFloatable;
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
      mozcView.fullscreenMode = testData.fullscreenMode;
      mozcView.narrowMode = testData.narrowMode;

      resetAll();
      expect(mozcView.getBottomBackground()).andStubReturn(bottomBackground);
      expect(mozcView.isFloatingMode()).andStubReturn(testData.isFloatable);
      bottomBackground.setBackgroundResource(testData.expectResourceId);

      replayAll();
      mozcView.updateBackgroundColor();

      verifyAll();
    }
  }

  @SmallTest
  public void testInsetsCalculator_static() {
    MozcView mozcView = createViewMockBuilder(MozcView.class)
        .addMockedMethods("getVisibleViewHeight", "isFloatingMode")
        .createMock();
    int visibleViewHeight = 240;
    int contentViewWidth = 800;
    int contentViewHeight = 400;
    expect(mozcView.getVisibleViewHeight()).andStubReturn(visibleViewHeight);
    expect(mozcView.isFloatingMode()).andStubReturn(false);
    replayAll();

    Insets outInsets = new Insets();
    mozcView.setInsets(contentViewWidth, contentViewHeight, outInsets);
    assertEquals(Insets.TOUCHABLE_INSETS_CONTENT, outInsets.touchableInsets);
    assertEquals(contentViewHeight - visibleViewHeight, outInsets.contentTopInsets);
    assertEquals(contentViewHeight - visibleViewHeight, outInsets.visibleTopInsets);
  }

  @SmallTest
  public void testInsetsCalculator_floating() {
    MozcView mozcView = createViewMockBuilder(MozcView.class)
        .addMockedMethods("getVisibleViewHeight", "getResources", "getSideAdjustedWidth",
                          "isFloatingMode")
        .createMock();
    Resources resources = createMockBuilder(MockResources.class)
        .addMockedMethods("getDisplayMetrics", "getDimensionPixelSize")
        .createMock();

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
    expect(mozcView.isFloatingMode()).andStubReturn(true);

    replayAll();

    DimensionPixelSize dimensionPixelSize = new MozcView.DimensionPixelSize(resources);
    VisibilityProxy.setField(mozcView, "dimensionPixelSize", dimensionPixelSize);

    {
      mozcView.layoutAdjustment = LayoutAdjustment.LEFT;
      mozcView.narrowMode = false;

      Insets outInsets = new Insets();
      mozcView.setInsets(contentViewWidth, contentViewHeight, outInsets);
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
      mozcView.layoutAdjustment = LayoutAdjustment.RIGHT;
      mozcView.narrowMode = false;

      Insets outInsets = new Insets();
      mozcView.setInsets(contentViewWidth, contentViewHeight, outInsets);
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
  public void testExpandBottomBackgroundView() {
    MozcView mozcView = createViewMockBuilder(MozcView.class)
        .addMockedMethods("getBottomBackground")
        .createMock();
    View bottomBackground = createViewMockBuilder(View.class)
        .addMockedMethods("getLayoutParams", "setLayoutParams")
        .createMock();
    SideFrameStubProxy leftFrameStubProxy = createMock(SideFrameStubProxy.class);
    SideFrameStubProxy rightFrameStubProxy = createMock(SideFrameStubProxy.class);
    VisibilityProxy.setField(mozcView, "leftFrameStubProxy", leftFrameStubProxy);
    VisibilityProxy.setField(mozcView, "rightFrameStubProxy", rightFrameStubProxy);
    int imeWindowHeight = 100;

    {
      resetAll();
      mozcView.imeWindowHeight = imeWindowHeight;
      expect(mozcView.getBottomBackground()).andStubReturn(bottomBackground);
      expect(bottomBackground.getLayoutParams()).andStubReturn(new LayoutParams(0, 0));
      Capture<LayoutParams> captureBottomBackground = new Capture<LayoutParams>();
      bottomBackground.setLayoutParams(capture(captureBottomBackground));

      replayAll();
      mozcView.fullscreenMode = false;
      mozcView.changeBottomBackgroundHeight(imeWindowHeight);

      verifyAll();
      assertEquals(imeWindowHeight, captureBottomBackground.getValue().height);
    }
    {
      resetAll();
      mozcView.imeWindowHeight = imeWindowHeight;
      expect(mozcView.getBottomBackground()).andStubReturn(bottomBackground);
      expect(bottomBackground.getLayoutParams()).andStubReturn(new LayoutParams(0, 0));
      Capture<LayoutParams> captureBottomBackground = new Capture<LayoutParams>();
      bottomBackground.setLayoutParams(capture(captureBottomBackground));

      replayAll();
      mozcView.fullscreenMode = true;
      mozcView.changeBottomBackgroundHeight(imeWindowHeight);

      verifyAll();
      assertEquals(imeWindowHeight, captureBottomBackground.getValue().height);
    }
  }

  @SmallTest
  public void testCollapseBottomBackgroundView() {
    Resources resources = getInstrumentation().getTargetContext().getResources();
    MozcView mozcView = createViewMockBuilder(MozcView.class)
        .addMockedMethods("getBottomBackground", "getInputFrameHeight")
        .createMock();
    View bottomBackground = createViewMockBuilder(View.class)
        .addMockedMethods("getLayoutParams", "setLayoutParams")
        .createMock();
    SideFrameStubProxy leftFrameStubProxy = createMock(SideFrameStubProxy.class);
    SideFrameStubProxy rightFrameStubProxy = createMock(SideFrameStubProxy.class);
    VisibilityProxy.setField(mozcView, "leftFrameStubProxy", leftFrameStubProxy);
    VisibilityProxy.setField(mozcView, "rightFrameStubProxy", rightFrameStubProxy);
    int inputFrameHeight = 100;
    int buttonFrameHeight = resources.getDimensionPixelSize(R.dimen.button_frame_height);

    {
      resetAll();
      expect(mozcView.getInputFrameHeight()).andStubReturn(inputFrameHeight);
      expect(mozcView.getBottomBackground()).andStubReturn(bottomBackground);
      expect(bottomBackground.getLayoutParams()).andStubReturn(new LayoutParams(0, 0));
      Capture<LayoutParams> captureBottomBackground = new Capture<LayoutParams>();
      bottomBackground.setLayoutParams(capture(captureBottomBackground));

      replayAll();
      mozcView.fullscreenMode = false;
      mozcView.resetBottomBackgroundHeight();

      verifyAll();
      assertEquals(inputFrameHeight + buttonFrameHeight, captureBottomBackground.getValue().height);
    }
    {
      resetAll();
      expect(mozcView.getInputFrameHeight()).andStubReturn(inputFrameHeight);
      expect(mozcView.getBottomBackground()).andStubReturn(bottomBackground);
      expect(bottomBackground.getLayoutParams()).andStubReturn(new LayoutParams(0, 0));
      Capture<LayoutParams> captureBottomBackground = new Capture<LayoutParams>();
      bottomBackground.setLayoutParams(capture(captureBottomBackground));

      replayAll();
      mozcView.fullscreenMode = true;
      mozcView.resetBottomBackgroundHeight();

      verifyAll();
      assertEquals(inputFrameHeight + buttonFrameHeight, captureBottomBackground.getValue().height);
    }
  }

  @SmallTest
  public void testStartAnimation() {
    MozcView mozcView = createViewMockBuilder(MozcView.class)
        .addMockedMethods("getSymbolInputView",
                          "changeBottomBackgroundHeight", "resetBottomBackgroundHeight")
        .createMock();
    CandidateViewManager candidateViewManager =
        createCandidateViewManagerMockBuilder().createMock();
    mozcView.candidateViewManager = candidateViewManager;
    SymbolInputView symbolInputView = createViewMockBuilder(SymbolInputView.class)
        .addMockedMethods("startInAnimation", "startOutAnimation", "getVisibility")
        .createMock();

    Animation symbolInputViewInAnimation = createMock(Animation.class);
    Animation symbolInputViewOutAnimation = createMock(Animation.class);
    mozcView.symbolInputViewInAnimation = symbolInputViewInAnimation;
    mozcView.symbolInputViewOutAnimation = symbolInputViewOutAnimation;

    // Test startCandidateViewInAnimation.
    {
      resetAll();
      mozcView.imeWindowHeight = 100;
      mozcView.changeBottomBackgroundHeight(100);

      replayAll();
      mozcView.softwareKeyboardHeightListener.onExpanded();

      verifyAll();
    }

    // Test startCandidateViewOutAnimation.
    {
      resetAll();
      expect(mozcView.getSymbolInputView()).andReturn(symbolInputView);
      expect(symbolInputView.getVisibility()).andReturn(View.VISIBLE);

      replayAll();
      mozcView.softwareKeyboardHeightListener.onCollapse();

      verifyAll();
    }
    {
      resetAll();
      expect(mozcView.getSymbolInputView()).andReturn(symbolInputView);
      expect(symbolInputView.getVisibility()).andReturn(View.INVISIBLE);
      mozcView.resetBottomBackgroundHeight();

      replayAll();
      mozcView.softwareKeyboardHeightListener.onCollapse();

      verifyAll();
    }

    // Test startSymbolInputViewInAnimation.
    {
      resetAll();
      expect(mozcView.getSymbolInputView()).andReturn(symbolInputView);
      symbolInputView.startInAnimation();
      mozcView.symbolInputViewHeight = 200;
      mozcView.changeBottomBackgroundHeight(200);

      replayAll();
      mozcView.startSymbolInputViewInAnimation();

      verifyAll();
    }

    // Test startSymbolInputViewOutAnimation.
    {
      resetAll();
      expect(mozcView.getSymbolInputView()).andReturn(symbolInputView);
      symbolInputView.startOutAnimation();
      expect(candidateViewManager.isKeyboardCandidateViewVisible()).andReturn(false);
      mozcView.resetBottomBackgroundHeight();

      replayAll();
      mozcView.startSymbolInputViewOutAnimation();

      verifyAll();
    }
  }

  @SmallTest
  public void testSetKeyboardHeight() {
    MozcView mozcView = createViewMockBuilder(MozcView.class)
        .addMockedMethods("checkInflated", "getSymbolInputView",
                          "updateInputFrameHeight", "resetHeightDependingComponents")
        .createMock();
    SymbolInputView symbolInputView = createViewMockBuilder(SymbolInputView.class)
        .addMockedMethods("setVerticalDimension")
        .createMock();
    mozcView.checkInflated();
    expect(mozcView.getSymbolInputView()).andStubReturn(symbolInputView);
    symbolInputView.setVerticalDimension(anyInt(), eq(1.2f, 1e-5f));
    mozcView.updateInputFrameHeight();
    mozcView.resetHeightDependingComponents();

    replayAll();
    mozcView.setKeyboardHeightRatio(120);

    verifyAll();
    Resources resources = getInstrumentation().getTargetContext().getResources();
    float originalImeWindowHeight = resources.getDimension(R.dimen.ime_window_height);
    float originalInputFrameHeight = resources.getDimension(R.dimen.input_frame_height);
    assertEquals(Math.round(originalImeWindowHeight * 1.2f), mozcView.imeWindowHeight);
    assertEquals(Math.round(originalInputFrameHeight * 1.2f), mozcView.getInputFrameHeight());
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
    assertNull(mozcView.findViewById(R.id.left_adjust_button));
    assertFalse(sideFrameStubProxy.inflated);
    sideFrameStubProxy.setButtonOnClickListener(buttonClickListener);
    sideFrameStubProxy.resetAdjustButtonBottomMargin(100);

    // Inflate.
    sideFrameStubProxy.setFrameVisibility(View.VISIBLE);
    assertNull(mozcView.findViewById(R.id.stub_left_frame));
    assertNotNull(mozcView.findViewById(R.id.left_frame));
    assertNotNull(mozcView.findViewById(R.id.left_adjust_button));
    assertTrue(sideFrameStubProxy.inflated);
    ImageView imageView = ImageView.class.cast(mozcView.findViewById(R.id.left_adjust_button));
    imageView.performClick();
    assertEquals(View.VISIBLE, mozcView.findViewById(R.id.left_frame).getVisibility());
    FrameLayout.LayoutParams layoutParams = FrameLayout.LayoutParams.class.cast(
        imageView.getLayoutParams());
    assertEquals((100 - layoutParams.height) / 2, layoutParams.bottomMargin);

    verifyAll();

    sideFrameStubProxy.setFrameVisibility(View.INVISIBLE);
    assertEquals(View.INVISIBLE, mozcView.findViewById(R.id.left_frame).getVisibility());
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
    mozcView.viewEventListener = eventListener;
    CompoundButton inputFrameFoldButton = createViewMock(CompoundButton.class);

    MozcView.InputFrameFoldButtonClickListener candidateViewListener =
        mozcView.new InputFrameFoldButtonClickListener(
            keyboardView, 200, 300L, foldKeyboardViewInterpolator,
            400L, expandKeyboardViewInterpolator, layoutParamsAnimator);

    {
      resetAll();
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
      HeightLinearInterpolationListener castedListener =
          HeightLinearInterpolationListener.class.cast(interpolationListener);
      assertEquals(200, castedListener.fromHeight);
      assertEquals(0, castedListener.toHeight);
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
      HeightLinearInterpolationListener castedListener =
          HeightLinearInterpolationListener.class.cast(interpolationListener);
      assertEquals(100, castedListener.fromHeight);
      assertEquals(200, castedListener.toHeight);
    }
  }
}
