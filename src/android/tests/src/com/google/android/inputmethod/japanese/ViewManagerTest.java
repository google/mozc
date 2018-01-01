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

import static org.mozc.android.inputmethod.japanese.testing.MozcMatcher.matchesKeyEvent;
import static org.easymock.EasyMock.anyObject;
import static org.easymock.EasyMock.capture;
import static org.easymock.EasyMock.eq;
import static org.easymock.EasyMock.expect;
import static org.easymock.EasyMock.isA;
import static org.easymock.EasyMock.same;

import org.mozc.android.inputmethod.japanese.FeedbackManager.FeedbackEvent;
import org.mozc.android.inputmethod.japanese.KeycodeConverter.KeyEventInterface;
import org.mozc.android.inputmethod.japanese.MozcView.HeightLinearInterpolationListener;
import org.mozc.android.inputmethod.japanese.ViewManager.ViewManagerEventListener;
import org.mozc.android.inputmethod.japanese.emoji.EmojiProviderType;
import org.mozc.android.inputmethod.japanese.emoji.EmojiUtil;
import org.mozc.android.inputmethod.japanese.hardwarekeyboard.HardwareKeyboard;
import org.mozc.android.inputmethod.japanese.hardwarekeyboard.HardwareKeyboard.CompositionSwitchMode;
import org.mozc.android.inputmethod.japanese.keyboard.BackgroundDrawableFactory.DrawableType;
import org.mozc.android.inputmethod.japanese.keyboard.Key;
import org.mozc.android.inputmethod.japanese.keyboard.Key.Stick;
import org.mozc.android.inputmethod.japanese.keyboard.KeyEntity;
import org.mozc.android.inputmethod.japanese.keyboard.KeyEventContext;
import org.mozc.android.inputmethod.japanese.keyboard.KeyState;
import org.mozc.android.inputmethod.japanese.keyboard.KeyState.MetaState;
import org.mozc.android.inputmethod.japanese.keyboard.Keyboard;
import org.mozc.android.inputmethod.japanese.keyboard.Keyboard.KeyboardSpecification;
import org.mozc.android.inputmethod.japanese.keyboard.KeyboardActionListener;
import org.mozc.android.inputmethod.japanese.keyboard.KeyboardView;
import org.mozc.android.inputmethod.japanese.keyboard.ProbableKeyEventGuesser;
import org.mozc.android.inputmethod.japanese.model.JapaneseSoftwareKeyboardModel;
import org.mozc.android.inputmethod.japanese.model.JapaneseSoftwareKeyboardModel.KeyboardMode;
import org.mozc.android.inputmethod.japanese.model.SymbolCandidateStorage.SymbolHistoryStorage;
import org.mozc.android.inputmethod.japanese.model.SymbolMajorCategory;
import org.mozc.android.inputmethod.japanese.preference.ClientSidePreference.InputStyle;
import org.mozc.android.inputmethod.japanese.preference.ClientSidePreference.KeyboardLayout;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateList;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateWord;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.Candidates;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.Candidates.Candidate;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Command;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.CompositionMode;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.TouchEvent;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.KeyEvent.ProbableKeyEvent;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.KeyEvent.SpecialKey;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Output;
import org.mozc.android.inputmethod.japanese.resources.R;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;
import org.mozc.android.inputmethod.japanese.testing.MockWindow;
import org.mozc.android.inputmethod.japanese.testing.Parameter;
import org.mozc.android.inputmethod.japanese.ui.MenuDialog;
import org.mozc.android.inputmethod.japanese.ui.MenuDialog.MenuDialogListener;
import org.mozc.android.inputmethod.japanese.util.ImeSwitcherFactory.ImeSwitcher;
import org.mozc.android.inputmethod.japanese.view.SkinType;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.AssetManager;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Rect;
import android.inputmethodservice.InputMethodService.Insets;
import android.os.Build;
import android.os.Bundle;
import android.test.MoreAsserts;
import android.test.mock.MockContext;
import android.test.mock.MockResources;
import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.view.Window;
import android.view.inputmethod.EditorInfo;

import org.easymock.Capture;
import org.easymock.EasyMock;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Map;

/**
 */
public class ViewManagerTest extends InstrumentationTestCaseWithMock {

  private ViewManager createViewManager(Context context) {
    return createViewManagerWithEventListener(context, createNiceMock(ViewEventListener.class));
  }

  private ViewManager createViewManagerWithEventListener(
      Context context, ViewEventListener listener) {
    return new ViewManager(
        context, listener, createNiceMock(SymbolHistoryStorage.class),
        createNiceMock(ImeSwitcher.class), createNiceMock(MenuDialogListener.class));
  }

  @SmallTest
  public void testViewManagerEventListener() {
    ViewEventListener viewEventListener = createNiceMock(ViewEventListener.class);
    replayAll();
    Context context = getInstrumentation().getTargetContext();
    ViewManager viewManager = createViewManagerWithEventListener(context, viewEventListener);
    ViewManagerEventListener viewManagerEventListener =
        ViewManagerEventListener.class.cast(viewManager.eventListener);

    MozcView mozcView = viewManager.createMozcView(context);

    // Minimize keyboard frame view.
    View keyboardFrame = mozcView.getKeyboardFrame();
    keyboardFrame.setVisibility(View.INVISIBLE);
    LayoutParams layoutParams = keyboardFrame.getLayoutParams();
    layoutParams.height = 0;
    keyboardFrame.setLayoutParams(layoutParams);

    // Set the input frame fold button checked.
    mozcView.candidateViewManager.keyboardCandidateView.getInputFrameFoldButton().setChecked(true);

    resetAllToDefault();

    viewEventListener.onConversionCandidateSelected(0, Optional.<Integer>absent());
    replayAll();

    viewManagerEventListener.onConversionCandidateSelected(0, Optional.<Integer>absent());

    verifyAll();

    MoreAsserts.assertNotEqual(0, keyboardFrame.getLayoutParams().height);
    assertFalse(
        mozcView.candidateViewManager.keyboardCandidateView.getInputFrameFoldButton().isChecked());
  }

  @SmallTest
  public void testHeightLinearInterpolationListener() {
    HeightLinearInterpolationListener listener = new HeightLinearInterpolationListener(0, 256);
    LayoutParams layoutParams = new LayoutParams(0, 0);

    listener.calculateAnimatedParams(0, layoutParams);
    assertEquals(0, layoutParams.height);

    listener.calculateAnimatedParams(0.25f, layoutParams);
    assertEquals(64, layoutParams.height);

    listener.calculateAnimatedParams(0.5f, layoutParams);
    assertEquals(128, layoutParams.height);

    listener.calculateAnimatedParams(0.75f, layoutParams);
    assertEquals(192, layoutParams.height);

    listener.calculateAnimatedParams(1, layoutParams);
    assertEquals(256, layoutParams.height);
  }

  @SmallTest
  public void testCreateInputViewReturnsDifferentInstance() {
    Context context = getInstrumentation().getTargetContext();
    ViewManager viewManager = createViewManager(context);
    replayAll();

    View inputView1 = viewManager.createMozcView(context);
    View inputView2 = viewManager.createMozcView(context);

    verifyAll();
    assertNotSame(inputView1, inputView2);
  }

  @SmallTest
  public void testCreateInputView() {
    Context context = getInstrumentation().getTargetContext();
    ViewManager viewManager = createViewManager(context);
    replayAll();

    MozcView mozcView = viewManager.createMozcView(context);

    verifyAll();

    // createInputView shouldn't return null.
    assertNotNull(mozcView);

    // Keyboard instance is correctly registered.
    KeyboardView keyboardView = mozcView.getKeyboardView();
    assertTrue(keyboardView.getKeyboard().isPresent());

    // Make sure layout params.
    assertEquals(LayoutParams.MATCH_PARENT, keyboardView.getLayoutParams().width);
    assertEquals(context.getResources().getDimensionPixelSize(R.dimen.input_frame_height),
                 keyboardView.getLayoutParams().height);

    // Check visibility
    assertFalse(mozcView.candidateViewManager.isKeyboardCandidateViewVisible());
  }

  @SmallTest
  public void testRender_nullOutput() {
    ViewManager viewManager = createViewManager(getInstrumentation().getTargetContext());
    replayAll();

    viewManager.render(null);

    verifyAll();
  }

  @SmallTest
  public void testRender_noCandidateView() {
    ViewManager viewManager = createViewManager(getInstrumentation().getTargetContext());
    replayAll();

    // Render with no CandidateView
    Command outCommand = Command.newBuilder()
        .setOutput(Output.newBuilder()
            .setCandidates(Candidates.newBuilder()
                .addCandidate(Candidate.newBuilder().setId(0).setIndex(0).setValue("cand0"))
                .addCandidate(Candidate.newBuilder().setId(1).setIndex(1).setValue("cand1"))
                .setSize(2)
                .setPosition(0)))
        .buildPartial();
    viewManager.render(outCommand);

    verifyAll();
  }

  @SmallTest
  public void testRender_noRequestSuggest() {
    ViewManager viewManager = createViewManager(getInstrumentation().getTargetContext());
    MozcView mozcView = createViewMock(MozcView.class);
    viewManager.mozcView = mozcView;

    replayAll();

    // Render with CandidateView
    Command outCommand = Command.newBuilder()
        .setInput(Input.newBuilder()
            .setRequestSuggestion(false)
            .buildPartial())
        .setOutput(Output.getDefaultInstance())
        .buildPartial();
    viewManager.render(outCommand);

    // Make sure that mozcView is not touched.
    verifyAll();
  }

  @SuppressLint("WrongCall")
  @MediumTest
  public void testRender_withCandidate() {
    Context context = getInstrumentation().getTargetContext();
    ViewManager viewManager = createViewManager(context);
    replayAll();

    // Render with CandidateView
    MozcView mozcView = viewManager.createMozcView(context);
    CandidateView candidateView = mozcView.candidateViewManager.keyboardCandidateView;
    assertNotNull(candidateView);
    CandidateView.ConversionCandidateWordView candidateWordView =
        candidateView.getConversionCandidateWordView();
    candidateWordView.onLayout(false, 0, 0, 320, 240);
    assertNull(candidateWordView.calculatedLayout);

    viewManager.render(Command.newBuilder()
        .setOutput(Output.newBuilder()
            .setAllCandidateWords(CandidateList.newBuilder()
                .addCandidates(CandidateWord.getDefaultInstance())))
        .buildPartial());

    verifyAll();
    // CandidateView's bodyView has children now as a result of
    // calling CandidateView#update() from ViewManager#render().
    assertNotNull(candidateWordView.calculatedLayout);
  }

  @SmallTest
  public void testSetEditorInfo() {
    Context context = getInstrumentation().getTargetContext();
    MozcView mozcView = createViewMock(MozcView.class);
    ViewManager viewManager = createViewManager(context);
    viewManager.mozcView = mozcView;

    class TestData extends Parameter {
      final boolean allowEmoji;
      final boolean expectCarrierEnableEmoji;

      TestData(boolean allowEmoji, boolean expectCarrierEmojiEnabled) {
        this.allowEmoji = allowEmoji;
        this.expectCarrierEnableEmoji = expectCarrierEmojiEnabled;
      }
    }
    TestData[] testDataList = {
        new TestData(true, true),
        new TestData(false, false),
    };

    for (TestData testData : testDataList) {
      Bundle bundle = new Bundle();
      bundle.putBoolean("allowEmoji", testData.allowEmoji);
      EditorInfo editorInfo = new EditorInfo();
      editorInfo.extras = bundle;

      resetAll();
      mozcView.setEmojiEnabled(
          EmojiUtil.isUnicodeEmojiAvailable(Build.VERSION.SDK_INT),
          testData.expectCarrierEnableEmoji);
      expect(mozcView.getKeyboardSize()).andReturn(new Rect(0, 0, 100, 200));
      expect(mozcView.getResources()).andReturn(context.getResources());
      expect(mozcView.getInputFrameHeight()).andStubReturn(200);
      mozcView.setEditorInfo(editorInfo);
      mozcView.setPasswordField(false);
      mozcView.setKeyboard(isA(Keyboard.class));
      replayAll();

      viewManager.setEditorInfo(editorInfo);

      verifyAll();
    }
  }

  @SmallTest
  public void testNumberField() {
    ViewManager viewManager = createViewManager(getInstrumentation().getTargetContext());
    EditorInfo editorInfo = new EditorInfo();

    editorInfo.inputType = EditorInfo.TYPE_CLASS_DATETIME;
    viewManager.setEditorInfo(editorInfo);
    assertEquals(
        KeyboardMode.NUMBER, viewManager.getActiveSoftwareKeyboardModel().getKeyboardMode());
    assertEquals(
        KeyboardSpecification.HARDWARE_QWERTY_ALPHABET,
        viewManager.hardwareKeyboard.getKeyboardSpecification());

    editorInfo.inputType = EditorInfo.TYPE_CLASS_NUMBER;
    viewManager.setEditorInfo(editorInfo);
    assertEquals(
        KeyboardMode.NUMBER, viewManager.getActiveSoftwareKeyboardModel().getKeyboardMode());
    assertEquals(
        KeyboardSpecification.HARDWARE_QWERTY_ALPHABET,
        viewManager.hardwareKeyboard.getKeyboardSpecification());

    editorInfo.inputType = EditorInfo.TYPE_CLASS_PHONE;
    viewManager.setEditorInfo(editorInfo);
    assertEquals(
        KeyboardMode.NUMBER, viewManager.getActiveSoftwareKeyboardModel().getKeyboardMode());
    assertEquals(
        KeyboardSpecification.HARDWARE_QWERTY_ALPHABET,
        viewManager.hardwareKeyboard.getKeyboardSpecification());

    editorInfo.inputType = EditorInfo.TYPE_CLASS_TEXT;
    viewManager.setEditorInfo(editorInfo);
    assertEquals(KeyboardMode.KANA, viewManager.getActiveSoftwareKeyboardModel().getKeyboardMode());
    assertEquals(
        KeyboardSpecification.HARDWARE_QWERTY_KANA,
        viewManager.hardwareKeyboard.getKeyboardSpecification());
  }

  @SmallTest
  public void testHideSubInputView() {
    Context context = getInstrumentation().getTargetContext();
    ViewManager viewManager = createViewManager(context);
    MozcView mozcView = viewManager.createMozcView(context);
    SymbolInputView symbolInputView = mozcView.getSymbolInputView();

    class TestData extends Parameter {
      final int symbolInputViewVisibility;
      final boolean expectedResult;
      final int expectedSymbolInputViewVisibility;
      final boolean expectedSymbolInputViewOutAnimation;

      TestData(int symbolInputViewVisibility,
               boolean expectedResult,
               int expectedSymbolInputViewVisibility,
               boolean expectedSymbolInputViewOutAnimation) {
        this.symbolInputViewVisibility = symbolInputViewVisibility;
        this.expectedResult = expectedResult;
        this.expectedSymbolInputViewVisibility = expectedSymbolInputViewVisibility;
        this.expectedSymbolInputViewOutAnimation = expectedSymbolInputViewOutAnimation;
      }
    }
    TestData[] testDataList = {
        // If SymbolInputView is not visible, just returns false.
        new TestData(View.GONE, false, View.GONE, false),

        // If SymbolInputView is visible, start hiding animation.
        new TestData(View.VISIBLE, true, View.VISIBLE, true),
    };

    for (TestData testData : testDataList) {
      symbolInputView.setVisibility(testData.symbolInputViewVisibility);
      symbolInputView.clearAnimation();

      assertEquals(testData.toString(), testData.expectedResult, viewManager.hideSubInputView());
      assertEquals(testData.toString(),
                   testData.expectedSymbolInputViewVisibility, symbolInputView.getVisibility());
      assertEquals(
          testData.toString(),
          testData.expectedSymbolInputViewOutAnimation, symbolInputView.getAnimation() != null);
    }
  }

  @SmallTest
  public void testKeyboardSwitchEventCallback() {
    Context context = getInstrumentation().getTargetContext();
    ViewEventListener eventListener = createMock(ViewEventListener.class);
    ViewManager viewManager = createViewManagerWithEventListener(context, eventListener);

    assertEquals(KeyboardSpecification.TWELVE_KEY_TOGGLE_KANA,
                 viewManager.getKeyboardSpecification());

    // Make sure that whenever keyboard is changed, the callback should be invoked.
    eventListener.onKeyEvent(null, null, KeyboardSpecification.QWERTY_KANA,
                             Collections.<TouchEvent>emptyList());
    replayAll();
    viewManager.setKeyboardLayout(KeyboardLayout.QWERTY);
    verifyAll();

    resetAll();
    eventListener.onKeyEvent(null, null, KeyboardSpecification.TWELVE_KEY_TOGGLE_KANA,
                             Collections.<TouchEvent>emptyList());
    replayAll();
    viewManager.setKeyboardLayout(KeyboardLayout.TWELVE_KEYS);
    verifyAll();
  }

  @SmallTest
  public void testKeyboardSwitching() {
    Context context = getInstrumentation().getTargetContext();
    Resources resources = context.getResources();

    ViewManager viewManager = createViewManager(context);
    replayAll();
    MozcView mozcView = viewManager.createMozcView(context);
    KeyboardView keyboardView = mozcView.getKeyboardView();
    HardwareKeyboard hardwareKeyboard = viewManager.hardwareKeyboard;
    List<TouchEvent> touchEventList = Collections.singletonList(TouchEvent.getDefaultInstance());

    // The default keyboard is 12key's-"kana"-toggle mode.
    assertEquals(KeyboardSpecification.TWELVE_KEY_TOGGLE_KANA,
                 keyboardView.getKeyboard().get().getSpecification());
    assertEquals(KeyboardSpecification.HARDWARE_QWERTY_KANA,
                 hardwareKeyboard.getKeyboardSpecification());

    viewManager.onKey(resources.getInteger(R.integer.key_chartype_to_abc), touchEventList);
    assertEquals(KeyboardSpecification.TWELVE_KEY_TOGGLE_ALPHABET,
                 keyboardView.getKeyboard().get().getSpecification());
    assertEquals(KeyboardSpecification.HARDWARE_QWERTY_ALPHABET,
                 hardwareKeyboard.getKeyboardSpecification());

    viewManager.onKey(resources.getInteger(R.integer.key_chartype_to_kana), touchEventList);
    assertEquals(KeyboardSpecification.TWELVE_KEY_TOGGLE_KANA,
                 keyboardView.getKeyboard().get().getSpecification());
    assertEquals(KeyboardSpecification.HARDWARE_QWERTY_KANA,
                 hardwareKeyboard.getKeyboardSpecification());

    // The default mode is "kana".
    viewManager.setKeyboardLayout(KeyboardLayout.QWERTY);
    assertEquals(KeyboardSpecification.QWERTY_KANA,
                 keyboardView.getKeyboard().get().getSpecification());
    assertEquals(KeyboardSpecification.HARDWARE_QWERTY_KANA,
                 hardwareKeyboard.getKeyboardSpecification());


    viewManager.onKey(resources.getInteger(R.integer.key_chartype_to_kana), touchEventList);
    assertEquals(KeyboardSpecification.QWERTY_KANA,
                 keyboardView.getKeyboard().get().getSpecification());
    assertEquals(KeyboardSpecification.HARDWARE_QWERTY_KANA,
                 hardwareKeyboard.getKeyboardSpecification());

    viewManager.onKey(resources.getInteger(R.integer.key_chartype_to_abc), touchEventList);
    assertEquals(KeyboardSpecification.QWERTY_ALPHABET,
                 keyboardView.getKeyboard().get().getSpecification());
    assertEquals(KeyboardSpecification.HARDWARE_QWERTY_ALPHABET,
                 hardwareKeyboard.getKeyboardSpecification());

    viewManager.onKey(resources.getInteger(R.integer.key_chartype_to_abc), touchEventList);
    assertEquals(KeyboardSpecification.QWERTY_ALPHABET,
                 keyboardView.getKeyboard().get().getSpecification());
    assertEquals(KeyboardSpecification.HARDWARE_QWERTY_ALPHABET,
                 hardwareKeyboard.getKeyboardSpecification());

    viewManager.onKey(resources.getInteger(R.integer.key_chartype_to_kana), touchEventList);
    assertEquals(KeyboardSpecification.QWERTY_KANA,
                 keyboardView.getKeyboard().get().getSpecification());
    assertEquals(KeyboardSpecification.HARDWARE_QWERTY_KANA,
                 hardwareKeyboard.getKeyboardSpecification());

    // Make sure we can return back to 12-keys keyboard.
    viewManager.setKeyboardLayout(KeyboardLayout.TWELVE_KEYS);
    assertEquals(KeyboardSpecification.TWELVE_KEY_TOGGLE_KANA,
                 keyboardView.getKeyboard().get().getSpecification());
    assertEquals(KeyboardSpecification.HARDWARE_QWERTY_KANA,
                 hardwareKeyboard.getKeyboardSpecification());

    // Then set to flick mode.
    viewManager.setInputStyle(InputStyle.FLICK);
    assertEquals(KeyboardSpecification.TWELVE_KEY_FLICK_KANA,
                 keyboardView.getKeyboard().get().getSpecification());
    assertEquals(KeyboardSpecification.HARDWARE_QWERTY_KANA,
                 hardwareKeyboard.getKeyboardSpecification());

    viewManager.onKey(resources.getInteger(R.integer.key_chartype_to_abc), touchEventList);
    assertEquals(KeyboardSpecification.TWELVE_KEY_FLICK_ALPHABET,
                 keyboardView.getKeyboard().get().getSpecification());
    assertEquals(KeyboardSpecification.HARDWARE_QWERTY_ALPHABET,
                 hardwareKeyboard.getKeyboardSpecification());

    // Return back to flick-kana mode.
    viewManager.onKey(resources.getInteger(R.integer.key_chartype_to_kana), touchEventList);
    assertEquals(KeyboardSpecification.TWELVE_KEY_FLICK_KANA,
                 keyboardView.getKeyboard().get().getSpecification());
    assertEquals(KeyboardSpecification.HARDWARE_QWERTY_KANA,
                 hardwareKeyboard.getKeyboardSpecification());

    // Then set to toggle-flick mode.
    viewManager.setInputStyle(InputStyle.TOGGLE_FLICK);
    assertEquals(KeyboardSpecification.TWELVE_KEY_TOGGLE_FLICK_KANA,
                 keyboardView.getKeyboard().get().getSpecification());
    assertEquals(KeyboardSpecification.HARDWARE_QWERTY_KANA,
                 hardwareKeyboard.getKeyboardSpecification());

    viewManager.onKey(resources.getInteger(R.integer.key_chartype_to_abc), touchEventList);
    assertEquals(KeyboardSpecification.TWELVE_KEY_TOGGLE_FLICK_ALPHABET,
                 keyboardView.getKeyboard().get().getSpecification());
    assertEquals(KeyboardSpecification.HARDWARE_QWERTY_ALPHABET,
                 hardwareKeyboard.getKeyboardSpecification());

    // Return back to flick-kana mode.
    viewManager.onKey(resources.getInteger(R.integer.key_chartype_to_kana), touchEventList);
    assertEquals(KeyboardSpecification.TWELVE_KEY_TOGGLE_FLICK_KANA,
                 keyboardView.getKeyboard().get().getSpecification());
    assertEquals(KeyboardSpecification.HARDWARE_QWERTY_KANA,
                 hardwareKeyboard.getKeyboardSpecification());

    // Testing for qwerty layout for alphabet mode.
    viewManager.setQwertyLayoutForAlphabet(true);
    assertEquals(KeyboardSpecification.TWELVE_KEY_TOGGLE_FLICK_KANA,
                 keyboardView.getKeyboard().get().getSpecification());
    assertEquals(KeyboardSpecification.HARDWARE_QWERTY_KANA,
                 hardwareKeyboard.getKeyboardSpecification());

    viewManager.onKey(resources.getInteger(R.integer.key_chartype_to_abc), touchEventList);
    assertEquals(KeyboardSpecification.TWELVE_KEY_TOGGLE_QWERTY_ALPHABET,
                 keyboardView.getKeyboard().get().getSpecification());
    assertEquals(KeyboardSpecification.HARDWARE_QWERTY_ALPHABET,
                 hardwareKeyboard.getKeyboardSpecification());

    // Switching the mode should trigger the alphabet keyboard view switching as well.
    viewManager.setQwertyLayoutForAlphabet(false);
    assertEquals(KeyboardSpecification.TWELVE_KEY_TOGGLE_FLICK_ALPHABET,
                 keyboardView.getKeyboard().get().getSpecification());
    assertEquals(KeyboardSpecification.HARDWARE_QWERTY_ALPHABET,
                 hardwareKeyboard.getKeyboardSpecification());

    viewManager.setQwertyLayoutForAlphabet(true);
    assertEquals(KeyboardSpecification.TWELVE_KEY_TOGGLE_QWERTY_ALPHABET,
                 keyboardView.getKeyboard().get().getSpecification());
    assertEquals(KeyboardSpecification.HARDWARE_QWERTY_ALPHABET,
                 hardwareKeyboard.getKeyboardSpecification());

    viewManager.onKey(resources.getInteger(R.integer.key_chartype_to_abc), touchEventList);
    assertEquals(KeyboardSpecification.TWELVE_KEY_TOGGLE_QWERTY_ALPHABET,
                 keyboardView.getKeyboard().get().getSpecification());
    assertEquals(KeyboardSpecification.HARDWARE_QWERTY_ALPHABET,
                 hardwareKeyboard.getKeyboardSpecification());

    viewManager.onKey(resources.getInteger(R.integer.key_chartype_to_kana), touchEventList);
    assertEquals(KeyboardSpecification.TWELVE_KEY_TOGGLE_FLICK_KANA,
                 keyboardView.getKeyboard().get().getSpecification());
    assertEquals(KeyboardSpecification.HARDWARE_QWERTY_KANA,
                 hardwareKeyboard.getKeyboardSpecification());

    viewManager.setQwertyLayoutForAlphabet(false);
    assertEquals(KeyboardSpecification.TWELVE_KEY_TOGGLE_FLICK_KANA,
                 keyboardView.getKeyboard().get().getSpecification());
    assertEquals(KeyboardSpecification.HARDWARE_QWERTY_KANA,
                 hardwareKeyboard.getKeyboardSpecification());
  }

  @SmallTest
  public void testShowMenuDialog() {
    Context context = getInstrumentation().getTargetContext();

    ViewEventListener listener = createMock(ViewEventListener.class);
    List<TouchEvent> touchEventList = Collections.singletonList(TouchEvent.getDefaultInstance());
    listener.onShowMenuDialog(touchEventList);
    ViewManager viewManager = createViewManagerWithEventListener(context, listener);
    replayAll();
    viewManager.onKey(
        context.getResources().getInteger(R.integer.key_menu_dialog), touchEventList);
    verifyAll();
  }

  @SmallTest
  public void testSetKeyboardStyleForUninitializedKeyboardViewDefault() {
    Context context = getInstrumentation().getTargetContext();
    ViewManager viewManager = createViewManager(context);

    // Make sure keyboard by createInputView is 12keys-kana keyboard by default.
    MozcView mozcView = viewManager.createMozcView(context);
    KeyboardView keyboardView = mozcView.getKeyboardView();
    assertEquals(KeyboardSpecification.TWELVE_KEY_TOGGLE_KANA,
                 keyboardView.getKeyboard().get().getSpecification());
  }

  @SmallTest
  public void testSetKeyboardLayoutForUninitializedInputView() {
    Context context = getInstrumentation().getTargetContext();
    ViewManager viewManager = createViewManager(context);

    // Set MobileInputStyle before creating InputView.
    viewManager.setKeyboardLayout(KeyboardLayout.QWERTY);

    // Make sure the keyboard instance created via createInputView is not default keyboard.
    MozcView mozcView = viewManager.createMozcView(context);
    KeyboardView keyboardView = mozcView.getKeyboardView();
    assertEquals(KeyboardSpecification.QWERTY_KANA,
                 keyboardView.getKeyboard().get().getSpecification());
  }

  @SmallTest
  public void testSetTwelveKeyInputStyleForUninitializedInputView() {
    Context context = getInstrumentation().getTargetContext();
    ViewManager viewManager = createViewManager(context);

    // Set TwelveKeysInputStyle before creating InputView.
    viewManager.setInputStyle(InputStyle.FLICK);

    // Make sure the keyboard instance created via createInputView is not default keyboard.
    MozcView mozcView = viewManager.createMozcView(context);
    KeyboardView keyboardView = mozcView.getKeyboardView();
    assertEquals(KeyboardSpecification.TWELVE_KEY_FLICK_KANA,
                 keyboardView.getKeyboard().get().getSpecification());
  }

  @SmallTest
  public void testSetKeyboardLayoutWithNull() {
    ViewManager viewManager = createViewManager(getInstrumentation().getTargetContext());
    try {
      viewManager.setKeyboardLayout(null);
      fail("NullPointerException is expected.");
    } catch (NullPointerException e) {
      // success.
    }
  }

  @SmallTest
  public void testCreateKeyEvent() {
    Context context = getInstrumentation().getTargetContext();
    ViewEventListener listener = createMock(ViewEventListener.class);
    ProbableKeyEventGuesser guesser = createMockBuilder(ProbableKeyEventGuesser.class)
        .addMockedMethod("getProbableKeyEvents")
        .withConstructor(AssetManager.class)
        .withArgs(getInstrumentation().getTargetContext().getAssets())
        .createMock();
    ViewManager viewManager = new ViewManager(
        context, listener, createNiceMock(SymbolHistoryStorage.class),
        createMock(ImeSwitcher.class), createNiceMock(MenuDialogListener.class), guesser,
        new HardwareKeyboard());
    viewManager.createMozcView(context);
    KeyboardSpecification keyboardSpecification =
        JapaneseSoftwareKeyboardModel.class.cast(viewManager.getActiveSoftwareKeyboardModel())
            .getKeyboardSpecification();

    Resources resources = context.getResources();
    List<TouchEvent> touchEventList = Collections.singletonList(TouchEvent.getDefaultInstance());

    class TestData {
      final int keyCode;
      final List<ProbableKeyEvent> probableKeyEvents;
      final boolean expectInvokeGuesser;
      final ProtoCommands.KeyEvent expectKeyEvent;
      final int expectKeyCode;
      TestData(int keyCode, ProbableKeyEvent[] probableKeyEvents,
               boolean expectInvokeGuesser,
               ProtoCommands.KeyEvent expectKeyEvent, int expectKeyCode) {
        this.keyCode = keyCode;
        this.probableKeyEvents =
            probableKeyEvents == null ? Collections.<ProbableKeyEvent>emptyList()
                                      : Arrays.asList(probableKeyEvents);
        this.expectInvokeGuesser = expectInvokeGuesser;
        this.expectKeyEvent = expectKeyEvent;
        this.expectKeyCode = expectKeyCode;
      }
    }

    TestData[] testDataList = {
        // White space.
        new TestData(' ',
                     new ProbableKeyEvent[0],
                     false,
                     ProtoCommands.KeyEvent.newBuilder().setSpecialKey(SpecialKey.SPACE).build(),
                     KeyEvent.KEYCODE_SPACE),
        // Enter.
        new TestData(resources.getInteger(R.integer.key_enter),
                     new ProbableKeyEvent[0],
                     false,
                     ProtoCommands.KeyEvent.newBuilder()
                         .setSpecialKey(SpecialKey.VIRTUAL_ENTER).build(),
                     KeyEvent.KEYCODE_ENTER),
        // Delete.
        new TestData(resources.getInteger(R.integer.key_backspace),
                     new ProbableKeyEvent[0],
                     false,
                     ProtoCommands.KeyEvent.newBuilder().setSpecialKey(SpecialKey.BACKSPACE)
                         .build(),
                     KeyEvent.KEYCODE_DEL),
        // Left.
        new TestData(resources.getInteger(R.integer.key_left),
                     new ProbableKeyEvent[0],
                     false,
                     ProtoCommands.KeyEvent.newBuilder()
                         .setSpecialKey(SpecialKey.VIRTUAL_LEFT).build(),
                     KeyEvent.KEYCODE_DPAD_LEFT),
        // Right.
        new TestData(resources.getInteger(R.integer.key_right),
                     new ProbableKeyEvent[0],
                     false,
                     ProtoCommands.KeyEvent.newBuilder()
                         .setSpecialKey(SpecialKey.VIRTUAL_RIGHT).build(),
                     KeyEvent.KEYCODE_DPAD_RIGHT),
        // Normal character with no correction stats.
        new TestData('a',
                     new ProbableKeyEvent[0],
                     true,
                     ProtoCommands.KeyEvent.newBuilder().setKeyCode('a').build(),
                     KeyEvent.KEYCODE_A),
        // Normal character with correction stats.
        new TestData('a',
                     new ProbableKeyEvent[] {
                        ProbableKeyEvent.newBuilder().setKeyCode('b').setProbability(0.1d).build()},
                     true,
                     ProtoCommands.KeyEvent.newBuilder()
                                           .setKeyCode('a')
                                           .addAllProbableKeyEvent(
                                               Arrays.asList(
                                                   ProbableKeyEvent.newBuilder()
                                                                   .setKeyCode('b')
                                                                   .setProbability(0.1d)
                                                                   .build()))
                                           .build(),
                     KeyEvent.KEYCODE_A)
    };

    for (TestData testData : testDataList) {
      resetAll();
      listener.onKeyEvent(eq(testData.expectKeyEvent),
                          matchesKeyEvent(testData.expectKeyCode),
                          eq(keyboardSpecification), eq(touchEventList));
      if (testData.expectInvokeGuesser) {
        expect(guesser.getProbableKeyEvents(touchEventList))
            .andReturn(testData.probableKeyEvents);
      }
      replayAll();
      viewManager.onKey(testData.keyCode, touchEventList);
      verifyAll();
    }
  }

  @SmallTest
  public void testCreateKeyEventForInvalidKeyCode() {
    Context context = getInstrumentation().getTargetContext();
    ViewEventListener listener = createNiceMock(ViewEventListener.class);
    ViewManager viewManager = createViewManagerWithEventListener(context, listener);
    viewManager.createMozcView(context);
    // Invalid Keycode.
    resetAll();
    replayAll();
    viewManager.onKey(Integer.MIN_VALUE, Collections.<TouchEvent>emptyList());
    verifyAll();
  }

  @SmallTest
  public void testOnKeyForSwitchingKeyboard() {
    Context context = getInstrumentation().getTargetContext();
    ViewEventListener listener = createMock(ViewEventListener.class);
    ViewManager viewManager = createViewManagerWithEventListener(context, listener);
    MozcView mozcView = viewManager.createMozcView(context);

    Resources resources = context.getResources();

    resetAll();
    List<TouchEvent> touchEventList = Collections.singletonList(TouchEvent.getDefaultInstance());
    listener.onKeyEvent(null, null,
                        KeyboardSpecification.TWELVE_KEY_TOGGLE_ALPHABET, touchEventList);
    replayAll();

    // Make sure the current keyboard is 12keys-toggle-kana mode before sending a key.
    KeyboardView keyboardView = mozcView.getKeyboardView();
    assertEquals(KeyboardSpecification.TWELVE_KEY_TOGGLE_KANA,
                 keyboardView.getKeyboard().get().getSpecification());

    // Send key code to switch number keyboard style.
    viewManager.onKey(resources.getInteger(R.integer.key_chartype_to_abc), touchEventList);

    // Make sure actually keyboard style has been changed.
    assertEquals(KeyboardSpecification.TWELVE_KEY_TOGGLE_ALPHABET,
                 keyboardView.getKeyboard().get().getSpecification());

    // Make sure that the listener can catch the expected events.
    verifyAll();
  }

  @SmallTest
  public void testOnKeyForGlobeButton() {
    Context context = getInstrumentation().getTargetContext();
    ImeSwitcher imeSwitcher = createMock(ImeSwitcher.class);

    ViewManager viewManager = new ViewManager(
        context, createNiceMock(ViewEventListener.class),
        createNiceMock(SymbolHistoryStorage.class),
        imeSwitcher, createNiceMock(MenuDialogListener.class));

    expect(imeSwitcher.switchToNextInputMethod(false)).andReturn(Boolean.TRUE);

    replayAll();
    viewManager.onKey(context.getResources().getInteger(R.integer.key_globe),
                      Collections.<TouchEvent>emptyList());
    verifyAll();
  }

  @SmallTest
  public void testKeyboardActionAdapter_onKey() {
    Context context = getInstrumentation().getTargetContext();
    List<TouchEvent> touchEventList = Collections.singletonList(TouchEvent.getDefaultInstance());
    ViewEventListener listener = createNiceMock(ViewEventListener.class);
    ViewManager viewManager = createViewManagerWithEventListener(context, listener);
    resetAll();
    listener.onKeyEvent(
        eq(ProtoCommands.KeyEvent.newBuilder().setKeyCode('1').build()),
        anyObject(KeyEventInterface.class),
        anyObject(KeyboardSpecification.class), same(touchEventList));
    replayAll();
    KeyboardActionListener keyboardActionListener = viewManager.new KeyboardActionAdapter();
    keyboardActionListener.onKey('1', touchEventList);

    verifyAll();
  }

  @SmallTest
  public void testKeyboardActionAdapter_onPress() {
    Context context = getInstrumentation().getTargetContext();
    ViewEventListener listener = createMock(ViewEventListener.class);
    ViewManager viewManager = createViewManagerWithEventListener(context, listener);

    KeyboardActionListener keyboardActionListener = viewManager.new KeyboardActionAdapter();

    // Listener should receive onFireFeedbackEvent.
    listener.onFireFeedbackEvent(FeedbackEvent.KEY_DOWN);
    replayAll();
    keyboardActionListener.onPress(10);
    verifyAll();

    // Listener shouldn't receive onFireFeedbackEvent for INVALID_KEY_CODE.
    resetAll();
    replayAll();
    keyboardActionListener.onPress(KeyEntity.INVALID_KEY_CODE);
    verifyAll();
  }

  @SmallTest
  public void testSimpleOnKey() {
    Context context = getInstrumentation().getTargetContext();
    ViewEventListener listener = createMock(ViewEventListener.class);
    ViewManager viewManager = createViewManagerWithEventListener(context, listener);
    viewManager.createMozcView(context);
    KeyboardSpecification keyboardSpecification =
        JapaneseSoftwareKeyboardModel.class.cast(viewManager.getActiveSoftwareKeyboardModel())
            .getKeyboardSpecification();

    // Emulate toggling a key.
    resetAll();
    List<TouchEvent> touchEventList = Collections.singletonList(TouchEvent.getDefaultInstance());
    listener.onKeyEvent(anyObject(ProtoCommands.KeyEvent.class),
                        matchesKeyEvent(KeyEvent.KEYCODE_1),
                        eq(keyboardSpecification), eq(touchEventList));
    replayAll();
    viewManager.onKey('1', touchEventList);

    verifyAll();
  }

  @SmallTest
  public void testRender_visibility() {
    Context context = getInstrumentation().getTargetContext();
    ViewEventListener listener = createMock(ViewEventListener.class);
    ViewManager viewManager = createViewManagerWithEventListener(context, listener);

    // Rendering null. Just making sure that this shouldn't fail.
    viewManager.render(null);

    // Rendering before the mozcView is initialized. Again making sure no-failure.
    viewManager.render(Command.getDefaultInstance());

    // Set mozcView mock.
    MozcView mozcView = createViewMock(MozcView.class);
    viewManager.mozcView = mozcView;

    // Render with empty output.
    {
      resetAll();
      Command emptyOutput = Command.getDefaultInstance();
      mozcView.setCommand(emptyOutput);
      mozcView.resetKeyboardFrameVisibility();
      replayAll();

      viewManager.render(emptyOutput);

      verifyAll();
    }

    // Render with non-empty output.
    {
      resetAll();
      Command output = Command.newBuilder()
          .setOutput(Output.newBuilder()
              .setAllCandidateWords(CandidateList.newBuilder()
                  .addCandidates(CandidateWord.getDefaultInstance())))
          .buildPartial();
      mozcView.setCommand(output);
      replayAll();

      viewManager.render(output);

      verifyAll();
    }
  }

  @SmallTest
  public void testReset() {
    Context context = getInstrumentation().getTargetContext();
    ViewManager viewManager = createViewManager(context);

    MozcView mozcView = viewManager.createMozcView(context);
    // TODO(matsuzakit): The lines from the beginning of the method to here should be
    //     extracted into a method.
    // Set pre-condition and call reset().
    mozcView.getKeyboardFrame().setVisibility(View.GONE);
    LayoutParams layoutParams = mozcView.getKeyboardFrame().getLayoutParams();
    layoutParams.height = 0;
    Map<Integer, KeyEventContext> keyEventContextMap =
        mozcView.getKeyboardView().keyEventContextMap;
    {
      // Add dummy KeyEventContext.
      KeyEventContext keyEventContext =
          new KeyEventContext(
              new Key(0, 0, 0, 0, 0, 0, false, false, Stick.EVEN,
                      DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND,
                      Collections.<KeyState>emptyList()),
              0, 0, 0, 0, 0, 0, Collections.<MetaState>emptySet());
      keyEventContextMap.put(0, keyEventContext);
    }

    mozcView.softwareKeyboardHeightListener.onExpanded();
    mozcView.getSymbolInputView().startInAnimation();

    MenuDialog menuDialog = createMockBuilder(MenuDialog.class)
        .withConstructor(Context.class, Optional.class)
        .withArgs(context, Optional.of(createNiceMock(MenuDialogListener.class)))
        .createMock();
    resetAll();
    viewManager.menuDialog = menuDialog;
    menuDialog.dismiss();
    replayAll();

    viewManager.reset();

    // Post-condition.
    verifyAll();
    assertEquals(View.VISIBLE, mozcView.getKeyboardFrame().getVisibility());
    assertEquals(LayoutParams.WRAP_CONTENT, mozcView.getKeyboardFrame().getLayoutParams().height);
    assertEquals(View.VISIBLE, mozcView.getKeyboardView().getVisibility());
    assertEquals(context.getResources().getDimensionPixelSize(R.dimen.input_frame_height),
                 mozcView.getKeyboardView().getLayoutParams().height);
    assertFalse(mozcView.candidateViewManager.isKeyboardCandidateViewVisible());
    assertEquals(View.GONE, mozcView.getSymbolInputView().getVisibility());
    assertNull(mozcView.candidateViewManager.keyboardCandidateView.getAnimation());
    assertNull(mozcView.getSymbolInputView().getAnimation());
    assertTrue(keyEventContextMap.isEmpty());
  }

  @SmallTest
  public void testComputeInsets() {
    class TestData extends Parameter {
      final int contentViewHeight;
      final int inputFrameHeight;
      final int expectedContentTopInsets;

      public TestData(int contentViewHeight,
                      int inputFrameHeight,
                      int expectedContentTopInsets) {
        this.contentViewHeight = contentViewHeight;
        this.inputFrameHeight = inputFrameHeight;
        this.expectedContentTopInsets = expectedContentTopInsets;
      }
    }

    TestData[] testDataList = {
        new TestData(480, 240, 240),
        new TestData(480, 325, 155),
    };

    Context context = getInstrumentation().getTargetContext();
    ViewManager viewManager = createViewManager(context);
    Window window = createMockBuilder(MockWindow.class)
        .withConstructor(Context.class)
        .withArgs(context)
        .createMock();
    Resources mockResources = createNiceMock(MockResources.class);
    Context mockContext = createNiceMock(MockContext.class);

    for (TestData testData : testDataList) {
      resetAll();
      viewManager.mozcView = null;

      View mockContentView = new View(context);
      mockContentView.layout(0, 0, 0, testData.contentViewHeight);
      expect(window.findViewById(Window.ID_ANDROID_CONTENT)).andReturn(mockContentView);

      expect(mockResources.getDimensionPixelSize(R.dimen.input_frame_height))
          .andStubReturn(testData.inputFrameHeight);
      expect(mockContext.getResources()).andStubReturn(mockResources);
      replayAll();

      Insets insets = new Insets();
      viewManager.computeInsets(mockContext, insets, window);

      verifyAll();
      assertEquals(
          testData.toString(), testData.expectedContentTopInsets, insets.contentTopInsets);
      assertEquals(
          testData.toString(), insets.contentTopInsets, insets.visibleTopInsets);
      assertEquals(
          testData.toString(), Insets.TOUCHABLE_INSETS_CONTENT, insets.touchableInsets);
    }

    {
      MozcView mozcView = createViewMock(MozcView.class);
      resetAll();
      viewManager.mozcView = mozcView;

      int contentViewWidth = 200;
      int contentViewHeight = 400;
      View mockContentView = new View(context);
      mockContentView.layout(0, 0, contentViewWidth, contentViewHeight);
      expect(window.findViewById(Window.ID_ANDROID_CONTENT)).andReturn(mockContentView);
      Insets insets = new Insets();
      mozcView.setInsets(contentViewWidth, contentViewHeight, insets);

      replayAll();

      viewManager.computeInsets(mockContext, insets, window);

      verifyAll();
    }
  }

  @SmallTest
  public void testSetFullscreenMode() {
    Context context = getInstrumentation().getTargetContext();
    ViewManager viewManager = createViewManager(context);
    MozcView mozcView = viewManager.createMozcView(context);

    for (boolean fullscreenMode : new boolean[] {true, false}) {
      viewManager.setFullscreenMode(fullscreenMode);
      assertEquals(fullscreenMode, viewManager.isFullscreenMode());
      assertEquals(fullscreenMode, mozcView.isFullscreenMode());
    }
  }

  @SmallTest
  public void testSetEmojiProviderType() {
    Context context = getInstrumentation().getTargetContext();
    ViewManager viewManager = createViewManager(context);
    MozcView mozcView = createViewMock(MozcView.class);
    viewManager.mozcView = mozcView;

    for (EmojiProviderType emojiProviderType : EmojiProviderType.values()) {
      resetAll();
      mozcView.setEmojiProviderType(emojiProviderType);
      replayAll();

      viewManager.setEmojiProviderType(emojiProviderType);

      verifyAll();
      assertEquals(emojiProviderType, viewManager.emojiProviderType);
    }
  }

  @SmallTest
  public void testSetNarrowMode() {
    Context context = getInstrumentation().getTargetContext();
    ViewManager viewManager = createViewManager(context);
    MozcView mozcView = viewManager.createMozcView(context);

    Configuration configuration = new Configuration();

    // viewManager to mozcView.
    configuration.hardKeyboardHidden = Configuration.HARDKEYBOARDHIDDEN_NO;
    viewManager.onConfigurationChanged(configuration);
    assertTrue(mozcView.isNarrowMode());
    configuration.hardKeyboardHidden = Configuration.HARDKEYBOARDHIDDEN_YES;
    viewManager.onConfigurationChanged(configuration);
    assertFalse(mozcView.isNarrowMode());
  }

  @SmallTest
  public void testSetPopupEnabled() {
    Context context = getInstrumentation().getTargetContext();
    ViewManager viewManager = createViewManager(context);
    MozcView mozcView = viewManager.createMozcView(context);

    // viewManager to mozcView.
    viewManager.setPopupEnabled(false);
    assertFalse(mozcView.isPopupEnabled());
    viewManager.setPopupEnabled(true);
    assertTrue(mozcView.isPopupEnabled());
  }

  @SmallTest
  public void testSetFlickSensitivity() {
    Context context = getInstrumentation().getTargetContext();
    ViewManager viewManager = createViewManager(context);
    MozcView mozcView = viewManager.createMozcView(context);

    viewManager.setFlickSensitivity(10);
    assertEquals(10, mozcView.getKeyboardView().getFlickSensitivity());
    viewManager.setFlickSensitivity(-10);
    assertEquals(-10, mozcView.getKeyboardView().getFlickSensitivity());
  }

  @SmallTest
  public void testSetSkinType() {
    Context context = getInstrumentation().getTargetContext();
    Resources resources = context.getResources();
    ViewManager viewManager = createViewManager(context);
    MozcView mozcView = viewManager.createMozcView(context);

    viewManager.setSkin(SkinType.ORANGE_LIGHTGRAY.getSkin(resources));
    assertEquals(SkinType.ORANGE_LIGHTGRAY.getSkin(resources), mozcView.getSkin());
    viewManager.setSkin(SkinType.TEST.getSkin(resources));
    assertEquals(SkinType.TEST.getSkin(resources), mozcView.getSkin());
  }

  @SmallTest
  public void testMaybeTransitToNarrowMode() {
    Context context = getInstrumentation().getTargetContext();
    ViewManager viewManager = createViewManager(context);

    class TestData extends Parameter {
      final Command command;
      final KeyEventInterface keyEventInterface;
      final boolean expectation;
      public TestData(Command command, KeyEventInterface keyEventInterface, boolean expectation) {
        this.command = command;
        this.keyEventInterface = keyEventInterface;
        this.expectation = expectation;
      }
    }
    KeyEventInterface softwareEvent = new KeyEventInterface() {
      @Override
      public Optional<KeyEvent> getNativeEvent() {
        return Optional.absent();
      }
      @Override
      public int getKeyCode() {
        return 0;
      }
    };
    KeyEventInterface hardwareEvent = new KeyEventInterface() {
      @Override
      public Optional<KeyEvent> getNativeEvent() {
        return Optional.of(new KeyEvent(0, 0));
      }
      @Override
      public int getKeyCode() {
        return 0;
      }
    };
    Command noKey = Command.newBuilder().setInput(Input.getDefaultInstance()).buildPartial();
    Command noKeyCode = Command.newBuilder()
        .setInput(Input.newBuilder()
            .setKey(ProtoCommands.KeyEvent.getDefaultInstance())
            .buildPartial())
        .buildPartial();
    Command withModifier = Command.newBuilder()
        .setInput(Input.newBuilder()
            .setKey(ProtoCommands.KeyEvent.newBuilder()
                .setKeyCode(0)
                .setModifiers(0))
            .buildPartial())
        .buildPartial();
    Command printable = Command.newBuilder()
        .setInput(Input.newBuilder()
            .setKey(ProtoCommands.KeyEvent.newBuilder()
                .setKeyCode(0))
            .buildPartial())
        .buildPartial();
    TestData[] testDataList = new TestData[] {
        new TestData(noKey, hardwareEvent, false),
        new TestData(noKeyCode, hardwareEvent, true),
        new TestData(withModifier, hardwareEvent, true),
        new TestData(printable, null, false),
        new TestData(printable, softwareEvent, false),
        new TestData(printable, hardwareEvent, true),
    };

    Configuration configuration = new Configuration();
    if (Build.VERSION.SDK_INT < 21) {
      configuration.hardKeyboardHidden = Configuration.HARDKEYBOARDHIDDEN_YES;
      for (TestData testData : testDataList) {
        viewManager.onConfigurationChanged(configuration);
        viewManager.maybeTransitToNarrowMode(testData.command, testData.keyEventInterface);
        assertEquals(testData.expectation, viewManager.isNarrowMode());
      }
    } else {
      // ViewManager doesn't take care about key event on Lollipop or later.
      configuration.hardKeyboardHidden = Configuration.HARDKEYBOARDHIDDEN_YES;
      for (TestData testData : testDataList) {
        viewManager.onConfigurationChanged(configuration);
        viewManager.maybeTransitToNarrowMode(testData.command, testData.keyEventInterface);
        assertEquals(false, viewManager.isNarrowMode());
      }
      configuration.hardKeyboardHidden = Configuration.HARDKEYBOARDHIDDEN_NO;
      for (TestData testData : testDataList) {
        viewManager.onConfigurationChanged(configuration);
        viewManager.maybeTransitToNarrowMode(testData.command, testData.keyEventInterface);
        assertEquals(true, viewManager.isNarrowMode());
      }
    }
  }

  @SmallTest
  public void testHandleEmojiKey() {
    Context context = getInstrumentation().getTargetContext();
    ViewManager viewManager = createViewManager(context);
    MozcView mozcView = createViewMockBuilder(MozcView.class)
        .addMockedMethods("showSymbolInputView", "setLayoutAdjustmentAndNarrowMode")
        .createNiceMock();
    viewManager.mozcView = mozcView;

    // Disable input device check.
    // Otherwise, this feature doesn't work except for keyboards which has Emoji key(s).
    viewManager.viewLayerKeyEventHandler.disableDeviceCheck = true;

    KeyEvent aDown = new KeyEvent(0L, 0L, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_A, 0);
    KeyEvent aUp = new KeyEvent(0L, 0L, KeyEvent.ACTION_UP, KeyEvent.KEYCODE_A, 0);
    KeyEvent aDownWithAlt = new KeyEvent(
        0L, 0L, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_A, 0, KeyEvent.META_ALT_ON);
    KeyEvent aUpWithAlt = new KeyEvent(
        0L, 0L, KeyEvent.ACTION_UP, KeyEvent.KEYCODE_A, 0, KeyEvent.META_ALT_ON);
    KeyEvent altDown = new KeyEvent(
        0L, 0L, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ALT_LEFT, 0, KeyEvent.META_ALT_ON);
    KeyEvent altUp = new KeyEvent(0L, 0L, KeyEvent.ACTION_UP, KeyEvent.KEYCODE_ALT_LEFT, 0);
    KeyEvent shiftDown = new KeyEvent(
        0L, 0L, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_SHIFT_LEFT, 0, KeyEvent.META_SHIFT_ON);
    KeyEvent shiftUp = new KeyEvent(0L, 0L, KeyEvent.ACTION_UP, KeyEvent.KEYCODE_SHIFT_LEFT, 0);
    KeyEvent altDownWithShift = new KeyEvent(
        0L, 0L, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ALT_LEFT, 0, KeyEvent.META_SHIFT_ON);
    KeyEvent altUpWithShift = new KeyEvent(
        0L, 0L, KeyEvent.ACTION_UP, KeyEvent.KEYCODE_ALT_LEFT, 0, KeyEvent.META_SHIFT_ON);
    KeyEvent shiftDownWithAlt = new KeyEvent(
        0L, 0L, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_SHIFT_LEFT, 0, KeyEvent.META_ALT_ON);
    KeyEvent shiftUpWithAlt = new KeyEvent(
        0L, 0L, KeyEvent.ACTION_UP, KeyEvent.KEYCODE_SHIFT_LEFT, 0, KeyEvent.META_ALT_ON);
    KeyEvent altDownWithAlt = new KeyEvent(
        0L, 0L, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ALT_LEFT, 0, KeyEvent.META_ALT_ON);
    KeyEvent altUpWithAlt = new KeyEvent(
        0L, 0L, KeyEvent.ACTION_UP, KeyEvent.KEYCODE_ALT_LEFT, 0, KeyEvent.META_ALT_ON);


    class TestData extends Parameter {
      final KeyEvent[] keyEvents;
      final boolean expectLaunchEmojiPalette;
      TestData(KeyEvent[] keyEvents, boolean expectLaunchEmojiPalette) {
        this.keyEvents = Preconditions.checkNotNull(keyEvents);
        this.expectLaunchEmojiPalette = expectLaunchEmojiPalette;
      }
    }
    TestData[] testDataArray = new TestData[] {
        new TestData(new KeyEvent[] {altDown, altUp}, true),
        new TestData(new KeyEvent[] {shiftDown, shiftUp}, false),
        new TestData(new KeyEvent[] {aDown, aUp}, false),

        new TestData(new KeyEvent[] {altDown, aDownWithAlt, aUpWithAlt, altUp}, false),
        new TestData(new KeyEvent[] {aDown, altDown, altUp, aUp}, false),
        new TestData(new KeyEvent[] {altDown, aDownWithAlt, altUp, aUpWithAlt}, false),
        new TestData(new KeyEvent[] {aDown, altDown, aUpWithAlt, altUp}, false),

        new TestData(new KeyEvent[] {altDown, shiftDownWithAlt, shiftUpWithAlt, altUp}, false),
        new TestData(new KeyEvent[] {shiftDown, altDownWithShift, altUpWithShift, shiftUp}, false),
        new TestData(new KeyEvent[] {altDown, shiftDownWithAlt, altUpWithShift, shiftUp}, false),
        new TestData(new KeyEvent[] {shiftDown, altDownWithShift, shiftUpWithAlt, altUp}, false),

        new TestData(new KeyEvent[] {altDown, altDownWithAlt, altUpWithAlt, altUp}, false),
    };

    for (TestData testData : testDataArray) {
      // Key events except for last one should NOT be consumed by view layer.
      for (int i = 0; i < testData.keyEvents.length - 1; ++i) {
        assertFalse(viewManager.isKeyConsumedOnViewAsynchronously(testData.keyEvents[i]));
      }

      KeyEvent lastKeyEvent = testData.keyEvents[testData.keyEvents.length - 1];
      if (testData.expectLaunchEmojiPalette) {
        assertTrue(viewManager.isKeyConsumedOnViewAsynchronously(lastKeyEvent));
        resetAll();
        expect(mozcView.showSymbolInputView(Optional.of(SymbolMajorCategory.EMOJI)))
            .andReturn(true);
        replayAll();
        viewManager.consumeKeyOnViewSynchronously(lastKeyEvent);
        verifyAll();
      } else {
        assertFalse(viewManager.isKeyConsumedOnViewAsynchronously(lastKeyEvent));
        resetAll();
        replayAll();
        viewManager.consumeKeyOnViewSynchronously(lastKeyEvent);
        verifyAll();
      }
    }
  }

  @SmallTest
  public void testOnKeyDownByHardwareKeyboard() {
    Context context = getInstrumentation().getTargetContext();
    HardwareKeyboard hardwareKeyboard = createNiceMock(HardwareKeyboard.class);
    ViewEventListener listener = createMock(ViewEventListener.class);
    ViewManager viewManager = new ViewManager(
        context, listener, createNiceMock(SymbolHistoryStorage.class),
        createNiceMock(ImeSwitcher.class), createNiceMock(MenuDialogListener.class),
        new ProbableKeyEventGuesser(context.getAssets()), hardwareKeyboard);

    MozcView mozcView = createViewMockBuilder(MozcView.class)
        .addMockedMethods("setKeyboard")
        .createNiceMock();
    viewManager.mozcView = mozcView;

    KeyEvent event = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_A);
    ProtoCommands.KeyEvent mozcKeyEvent =
        ProtoCommands.KeyEvent.newBuilder().setKeyCode('a').build();
    KeyEventInterface keyEventInterface = KeycodeConverter.getKeyEventInterface(event);
    KeyboardSpecification keyboardSpecification = KeyboardSpecification.HARDWARE_QWERTY_KANA;

    resetAll();

    expect(hardwareKeyboard.getCompositionMode()).andStubReturn(CompositionMode.HIRAGANA);
    expect(hardwareKeyboard.setCompositionModeByKey(event)).andReturn(false);
    expect(hardwareKeyboard.getMozcKeyEvent(event)).andReturn(mozcKeyEvent);
    expect(hardwareKeyboard.getKeyEventInterface(event)).andReturn(keyEventInterface);
    expect(hardwareKeyboard.getKeyboardSpecification()).andReturn(keyboardSpecification);
    listener.onKeyEvent(
        same(mozcKeyEvent), same(keyEventInterface), same(keyboardSpecification),
        eq(Collections.<TouchEvent>emptyList()));

    replayAll();
    viewManager.onHardwareKeyEvent(event);
    verifyAll();

    resetAll();

    keyboardSpecification = KeyboardSpecification.HARDWARE_QWERTY_ALPHABET;
    expect(hardwareKeyboard.getCompositionMode()).andReturn(CompositionMode.HIRAGANA);
    expect(hardwareKeyboard.setCompositionModeByKey(event)).andReturn(false);
    expect(hardwareKeyboard.getCompositionMode()).andReturn(CompositionMode.HALF_ASCII);
    expect(hardwareKeyboard.getMozcKeyEvent(event)).andReturn(mozcKeyEvent);
    expect(hardwareKeyboard.getKeyEventInterface(event)).andReturn(keyEventInterface);
    expect(hardwareKeyboard.getKeyboardSpecification()).andReturn(keyboardSpecification);
    Capture<Keyboard> keyboardCapture = new Capture<Keyboard>();
    mozcView.setKeyboard(capture(keyboardCapture));
    listener.onKeyEvent(
        same(mozcKeyEvent), same(keyEventInterface), same(keyboardSpecification),
        eq(Collections.<TouchEvent>emptyList()));

    replayAll();
    viewManager.onHardwareKeyEvent(event);
    verifyAll();
    assertEquals(KeyboardSpecification.TWELVE_KEY_TOGGLE_ALPHABET,
                 keyboardCapture.getValue().getSpecification());
  }

  @SmallTest
  public void testSwitchHardwareKeyboardCompositionMode() {
    Context context = getInstrumentation().getTargetContext();
    ViewEventListener listener = createMock(ViewEventListener.class);
    ViewManager viewManager = createViewManagerWithEventListener(context, listener);

    // Set KANA as a initial composition mode without MozcView.
    viewManager.switchHardwareKeyboardCompositionMode(CompositionSwitchMode.KANA);

    MozcView mozcView = createViewMockBuilder(MozcView.class)
        .addMockedMethods("setKeyboard")
        .createNiceMock();
    viewManager.mozcView = mozcView;

    Capture<Keyboard> keyboardCapture = new Capture<Keyboard>();

    // HIRAGANA to HIRAGANA by CompositionSwitchMode.KANA.
    resetAll();
    replayAll();
    viewManager.switchHardwareKeyboardCompositionMode(CompositionSwitchMode.KANA);
    verifyAll();

    // HIRAGANA to HALF_ASCII by CompositionSwitchMode.HALF_ASCII.
    resetAll();
    mozcView.setKeyboard(capture(keyboardCapture));
    listener.onKeyEvent(EasyMock.<ProtoCommands.KeyEvent>isNull(),
                        EasyMock.<KeyEventInterface>isNull(),
                        same(KeyboardSpecification.HARDWARE_QWERTY_ALPHABET),
                        eq(Collections.<TouchEvent>emptyList()));
    replayAll();
    viewManager.switchHardwareKeyboardCompositionMode(CompositionSwitchMode.ALPHABET);
    verifyAll();
    assertEquals(KeyboardSpecification.TWELVE_KEY_TOGGLE_ALPHABET,
        keyboardCapture.getValue().getSpecification());

    // HALF_ASCII to HIRAGANA by CompositionSwitchMode.TOGGLE.
    resetAll();
    mozcView.setKeyboard(capture(keyboardCapture));
    listener.onKeyEvent(EasyMock.<ProtoCommands.KeyEvent>isNull(),
                        EasyMock.<KeyEventInterface>isNull(),
                        same(KeyboardSpecification.HARDWARE_QWERTY_KANA),
                        eq(Collections.<TouchEvent>emptyList()));
    replayAll();
    viewManager.switchHardwareKeyboardCompositionMode(CompositionSwitchMode.TOGGLE);
    verifyAll();
    assertEquals(KeyboardSpecification.TWELVE_KEY_TOGGLE_KANA,
                 keyboardCapture.getValue().getSpecification());
  }
}
