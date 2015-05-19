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

import static org.mozc.android.inputmethod.japanese.testing.MozcMatcher.matchesKeyEvent;
import static org.easymock.EasyMock.anyObject;
import static org.easymock.EasyMock.eq;
import static org.easymock.EasyMock.expect;
import static org.easymock.EasyMock.isA;
import org.mozc.android.inputmethod.japanese.FeedbackManager.FeedbackEvent;
import org.mozc.android.inputmethod.japanese.JapaneseKeyboard.KeyboardSpecification;
import org.mozc.android.inputmethod.japanese.MozcView.HeightLinearInterpolationListener;
import org.mozc.android.inputmethod.japanese.ViewManager.ViewManagerEventListener;
import org.mozc.android.inputmethod.japanese.emoji.EmojiProviderType;
import org.mozc.android.inputmethod.japanese.keyboard.Key;
import org.mozc.android.inputmethod.japanese.keyboard.Key.Stick;
import org.mozc.android.inputmethod.japanese.keyboard.KeyEntity;
import org.mozc.android.inputmethod.japanese.keyboard.KeyEventContext;
import org.mozc.android.inputmethod.japanese.keyboard.KeyState;
import org.mozc.android.inputmethod.japanese.keyboard.KeyState.MetaState;
import org.mozc.android.inputmethod.japanese.keyboard.KeyboardActionListener;
import org.mozc.android.inputmethod.japanese.keyboard.ProbableKeyEventGuesser;
import org.mozc.android.inputmethod.japanese.model.JapaneseSoftwareKeyboardModel;
import org.mozc.android.inputmethod.japanese.model.SymbolCandidateStorage.SymbolHistoryStorage;
import org.mozc.android.inputmethod.japanese.preference.ClientSidePreference.InputStyle;
import org.mozc.android.inputmethod.japanese.preference.ClientSidePreference.KeyboardLayout;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateList;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateWord;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.Candidates;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.Candidates.Candidate;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Command;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.TouchEvent;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.KeyEvent.ProbableKeyEvent;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.KeyEvent.SpecialKey;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Output;
import org.mozc.android.inputmethod.japanese.resources.R;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;
import org.mozc.android.inputmethod.japanese.testing.Parameter;
import org.mozc.android.inputmethod.japanese.testing.VisibilityProxy;
import org.mozc.android.inputmethod.japanese.ui.MenuDialog;
import org.mozc.android.inputmethod.japanese.ui.MenuDialog.MenuDialogListener;
import org.mozc.android.inputmethod.japanese.view.SkinType;

import android.content.Context;
import android.content.res.AssetManager;
import android.content.res.Resources;
import android.graphics.Rect;
import android.inputmethodservice.InputMethodService.Insets;
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
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Map;

/**
 */
public class ViewManagerTest extends InstrumentationTestCaseWithMock {
  @SmallTest
  public void testViewManagerEventListener() {
    ViewEventListener viewEventListener = createNiceMock(ViewEventListener.class);
    replayAll();
    Context context = getInstrumentation().getTargetContext();
    ViewManager viewManager = new ViewManager(context, viewEventListener, null, null);
    ViewManagerEventListener viewManagerEventListener =
        VisibilityProxy.getField(viewManager, "eventListener");

    MozcView mozcView = viewManager.createMozcView(context);

    // Minimize keyboard frame view.
    View keyboardFrame = mozcView.getKeyboardFrame();
    keyboardFrame.setVisibility(View.INVISIBLE);
    LayoutParams layoutParams = keyboardFrame.getLayoutParams();
    layoutParams.height = 0;
    keyboardFrame.setLayoutParams(layoutParams);

    // Set the input frame fold button checked.
    mozcView.getCandidateView().getInputFrameFoldButton().setChecked(true);

    resetAllToDefault();

    viewEventListener.onConversionCandidateSelected(0);
    replayAll();

    viewManagerEventListener.onConversionCandidateSelected(0);

    verifyAll();

    MoreAsserts.assertNotEqual(0, keyboardFrame.getLayoutParams().height);
    assertFalse(mozcView.getCandidateView().getInputFrameFoldButton().isChecked());
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
    ViewManager viewManager =
        new ViewManager(context, createNiceMock(ViewEventListener.class), null, null);
    replayAll();

    View inputView1 = viewManager.createMozcView(context);
    View inputView2 = viewManager.createMozcView(context);

    verifyAll();
    assertNotSame(inputView1, inputView2);
  }

  @SmallTest
  public void testCreateInputView() {
    Context context = getInstrumentation().getTargetContext();
    ViewManager viewManager =
        new ViewManager(context, createNiceMock(ViewEventListener.class), null, null);
    replayAll();

    MozcView mozcView = viewManager.createMozcView(context);

    verifyAll();

    // createInputView shouldn't return null.
    assertNotNull(mozcView);

    // Keyboard instance is correctly registered.
    JapaneseKeyboardView japaneseKeyboardView = mozcView.getKeyboardView();
    assertNotNull(japaneseKeyboardView.getJapaneseKeyboard());

    // Make sure layout params.
    assertEquals(LayoutParams.MATCH_PARENT, japaneseKeyboardView.getLayoutParams().width);
    assertEquals(LayoutParams.MATCH_PARENT, japaneseKeyboardView.getLayoutParams().height);

    // Check visibility
    assertEquals(View.GONE, mozcView.getCandidateView().getVisibility());
  }

  @SmallTest
  public void testRender_nullOutput() {
    ViewManager viewManager = new ViewManager(getInstrumentation().getTargetContext(),
                                              createNiceMock(ViewEventListener.class),
                                              null,
                                              null);
    replayAll();

    viewManager.render(null);

    verifyAll();
  }

  @SmallTest
  public void testRender_noCandidateView() {
    ViewManager viewManager = new ViewManager(getInstrumentation().getTargetContext(),
                                              createNiceMock(ViewEventListener.class),
                                              null,
                                              null);
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
    Context context = getInstrumentation().getTargetContext();
    ViewManager viewManager =
        new ViewManager(context, createNiceMock(ViewEventListener.class), null, null);
    MozcView mozcView = createViewMock(MozcView.class);
    VisibilityProxy.setField(viewManager, "mozcView", mozcView);

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

  @MediumTest
  public void testRender_withCandidate() {
    Context context = getInstrumentation().getTargetContext();
    ViewManager viewManager =
        new ViewManager(context, createNiceMock(ViewEventListener.class), null, null);
    replayAll();

    // Render with CandidateView
    MozcView mozcView = viewManager.createMozcView(context);
    CandidateView candidateView = mozcView.getCandidateView();
    assertNotNull(candidateView);
    CandidateView.ConversionCandidateWordView candidateWordView =
        candidateView.getConversionCandidateWordView();
    candidateWordView.onLayout(false, 0, 0, 320, 240);
    assertNull(VisibilityProxy.getField(candidateWordView, "calculatedLayout"));

    viewManager.render(Command.newBuilder()
        .setOutput(Output.newBuilder()
            .setAllCandidateWords(CandidateList.newBuilder()
                .addCandidates(CandidateWord.getDefaultInstance())))
        .buildPartial());

    verifyAll();
    // CandidateView's bodyView has children now as a result of
    // calling CandidateView#update() from ViewManager#render().
    assertNotNull(VisibilityProxy.getField(candidateWordView, "calculatedLayout"));
  }

  @SmallTest
  public void testSetEditorInfo() {
    Context context = getInstrumentation().getTargetContext();
    MozcView mozcView = createViewMock(MozcView.class);
    ViewManager viewManager =
        new ViewManager(context, createNiceMock(ViewEventListener.class), null, null);
    VisibilityProxy.setField(viewManager, "mozcView", mozcView);

    class TestData extends Parameter {
      final boolean allowEmoji;
      final boolean expectEnableEmoji;

      TestData(boolean allowEmoji, boolean expectEnableEmoji) {
        this.allowEmoji = allowEmoji;
        this.expectEnableEmoji = expectEnableEmoji;
      }
    }
    TestData[] testDataList = {
        new TestData(true, true), new TestData(false, false)
    };

    for (TestData testData : testDataList) {
      Bundle bundle = new Bundle();
      bundle.putBoolean("allowEmoji", testData.allowEmoji);
      EditorInfo editorInfo = new EditorInfo();
      editorInfo.extras = bundle;

      resetAll();
      mozcView.setEmojiEnabled(testData.expectEnableEmoji);
      expect(mozcView.getKeyboardSize()).andReturn(new Rect(0, 0, 100, 200));
      expect(mozcView.getResources()).andReturn(context.getResources());
      expect(mozcView.getInputFrameHeight()).andStubReturn(200);
      mozcView.setJapaneseKeyboard(isA(JapaneseKeyboard.class));
      replayAll();

      viewManager.setEditorInfo(editorInfo);

      verifyAll();
    }
  }

  @SmallTest
  public void testHideSubInputView() {
    Context context = getInstrumentation().getTargetContext();
    ViewManager viewManager =
        new ViewManager(context, createNiceMock(ViewEventListener.class), null, null);
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
  public void testJapaneseKeyboardSwitchEventCallback() {
    Context context = getInstrumentation().getTargetContext();
    ViewEventListener eventListener = createMock(ViewEventListener.class);
    ViewManager viewManager = new ViewManager(context, eventListener, null, null);

    assertEquals(KeyboardSpecification.TWELVE_KEY_TOGGLE_KANA,
                 viewManager.getJapaneseKeyboardSpecification());

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
  public void testJapaneseKeyboardSwitching() {
    Context context = getInstrumentation().getTargetContext();
    Resources resources = context.getResources();

    ViewManager viewManager =
        new ViewManager(context, createNiceMock(ViewEventListener.class), null, null);
    replayAll();
    MozcView mozcView = viewManager.createMozcView(context);
    JapaneseKeyboardView keyboardView = mozcView.getKeyboardView();
    List<TouchEvent> touchEventList = Collections.singletonList(TouchEvent.getDefaultInstance());

    // The default keyboard is 12key's-"kana"-toggle mode.
    assertEquals(KeyboardSpecification.TWELVE_KEY_TOGGLE_KANA,
                 keyboardView.getJapaneseKeyboard().getSpecification());

    viewManager.onKey(resources.getInteger(R.integer.key_chartype_to_abc), touchEventList);
    assertEquals(KeyboardSpecification.TWELVE_KEY_TOGGLE_ALPHABET,
                 keyboardView.getJapaneseKeyboard().getSpecification());

    viewManager.onKey(resources.getInteger(R.integer.key_chartype_to_123), touchEventList);
    assertEquals(KeyboardSpecification.TWELVE_KEY_TOGGLE_NUMBER,
                 keyboardView.getJapaneseKeyboard().getSpecification());

    viewManager.onKey(resources.getInteger(R.integer.key_chartype_to_kana), touchEventList);
    assertEquals(KeyboardSpecification.TWELVE_KEY_TOGGLE_KANA,
                 keyboardView.getJapaneseKeyboard().getSpecification());

    // The default mode is "kana".
    viewManager.setKeyboardLayout(KeyboardLayout.QWERTY);
    assertEquals(KeyboardSpecification.QWERTY_KANA,
                 keyboardView.getJapaneseKeyboard().getSpecification());

    viewManager.onKey(resources.getInteger(R.integer.key_chartype_to_kana_123), touchEventList);
    assertEquals(KeyboardSpecification.QWERTY_KANA_NUMBER,
                 keyboardView.getJapaneseKeyboard().getSpecification());

    viewManager.onKey(resources.getInteger(R.integer.key_chartype_to_kana), touchEventList);
    assertEquals(KeyboardSpecification.QWERTY_KANA,
                 keyboardView.getJapaneseKeyboard().getSpecification());

    viewManager.onKey(resources.getInteger(R.integer.key_chartype_to_abc), touchEventList);
    assertEquals(KeyboardSpecification.QWERTY_ALPHABET,
                 keyboardView.getJapaneseKeyboard().getSpecification());

    viewManager.onKey(resources.getInteger(R.integer.key_chartype_to_abc_123), touchEventList);
    assertEquals(KeyboardSpecification.QWERTY_ALPHABET_NUMBER,
                 keyboardView.getJapaneseKeyboard().getSpecification());

    viewManager.onKey(resources.getInteger(R.integer.key_chartype_to_abc), touchEventList);
    assertEquals(KeyboardSpecification.QWERTY_ALPHABET,
                 keyboardView.getJapaneseKeyboard().getSpecification());

    viewManager.onKey(resources.getInteger(R.integer.key_chartype_to_kana), touchEventList);
    assertEquals(KeyboardSpecification.QWERTY_KANA,
                 keyboardView.getJapaneseKeyboard().getSpecification());

    // Make sure we can return back to 12-keys keyboard.
    viewManager.setKeyboardLayout(KeyboardLayout.TWELVE_KEYS);
    assertEquals(KeyboardSpecification.TWELVE_KEY_TOGGLE_KANA,
                 keyboardView.getJapaneseKeyboard().getSpecification());

    // Then set to flick mode.
    viewManager.setInputStyle(InputStyle.FLICK);
    assertEquals(KeyboardSpecification.TWELVE_KEY_FLICK_KANA,
                 keyboardView.getJapaneseKeyboard().getSpecification());

    viewManager.onKey(resources.getInteger(R.integer.key_chartype_to_abc), touchEventList);
    assertEquals(KeyboardSpecification.TWELVE_KEY_FLICK_ALPHABET,
                 keyboardView.getJapaneseKeyboard().getSpecification());

    viewManager.onKey(resources.getInteger(R.integer.key_chartype_to_123), touchEventList);
    assertEquals(KeyboardSpecification.TWELVE_KEY_FLICK_NUMBER,
                 keyboardView.getJapaneseKeyboard().getSpecification());

    // Return back to flick-kana mode.
    viewManager.onKey(resources.getInteger(R.integer.key_chartype_to_kana), touchEventList);
    assertEquals(KeyboardSpecification.TWELVE_KEY_FLICK_KANA,
                 keyboardView.getJapaneseKeyboard().getSpecification());

    // Then set to toggle-flick mode.
    viewManager.setInputStyle(InputStyle.TOGGLE_FLICK);
    assertEquals(KeyboardSpecification.TWELVE_KEY_TOGGLE_FLICK_KANA,
                 keyboardView.getJapaneseKeyboard().getSpecification());

    viewManager.onKey(resources.getInteger(R.integer.key_chartype_to_abc), touchEventList);
    assertEquals(KeyboardSpecification.TWELVE_KEY_TOGGLE_FLICK_ALPHABET,
                 keyboardView.getJapaneseKeyboard().getSpecification());

    viewManager.onKey(resources.getInteger(R.integer.key_chartype_to_123), touchEventList);
    assertEquals(KeyboardSpecification.TWELVE_KEY_TOGGLE_FLICK_NUMBER,
                 keyboardView.getJapaneseKeyboard().getSpecification());

    // Return back to flick-kana mode.
    viewManager.onKey(resources.getInteger(R.integer.key_chartype_to_kana), touchEventList);
    assertEquals(KeyboardSpecification.TWELVE_KEY_TOGGLE_FLICK_KANA,
                 keyboardView.getJapaneseKeyboard().getSpecification());

    // Testing for qwerty layout for alphabet mode.
    viewManager.setQwertyLayoutForAlphabet(true);
    assertEquals(KeyboardSpecification.TWELVE_KEY_TOGGLE_FLICK_KANA,
                 keyboardView.getJapaneseKeyboard().getSpecification());

    viewManager.onKey(resources.getInteger(R.integer.key_chartype_to_abc), touchEventList);
    assertEquals(KeyboardSpecification.TWELVE_KEY_TOGGLE_QWERTY_ALPHABET,
                 keyboardView.getJapaneseKeyboard().getSpecification());

    // Switching the mode should trigger the alphabet keyboard view switching as well.
    viewManager.setQwertyLayoutForAlphabet(false);
    assertEquals(KeyboardSpecification.TWELVE_KEY_TOGGLE_FLICK_ALPHABET,
                 keyboardView.getJapaneseKeyboard().getSpecification());

    viewManager.setQwertyLayoutForAlphabet(true);
    assertEquals(KeyboardSpecification.TWELVE_KEY_TOGGLE_QWERTY_ALPHABET,
                 keyboardView.getJapaneseKeyboard().getSpecification());

    viewManager.onKey(resources.getInteger(R.integer.key_chartype_to_abc_123), touchEventList);
    assertEquals(KeyboardSpecification.QWERTY_ALPHABET_NUMBER,
                 keyboardView.getJapaneseKeyboard().getSpecification());

    viewManager.onKey(resources.getInteger(R.integer.key_chartype_to_abc), touchEventList);
    assertEquals(KeyboardSpecification.TWELVE_KEY_TOGGLE_QWERTY_ALPHABET,
                 keyboardView.getJapaneseKeyboard().getSpecification());

    viewManager.onKey(resources.getInteger(R.integer.key_chartype_to_123), touchEventList);
    assertEquals(KeyboardSpecification.TWELVE_KEY_TOGGLE_FLICK_NUMBER,
                 keyboardView.getJapaneseKeyboard().getSpecification());

    viewManager.onKey(resources.getInteger(R.integer.key_chartype_to_kana), touchEventList);
    assertEquals(KeyboardSpecification.TWELVE_KEY_TOGGLE_FLICK_KANA,
                 keyboardView.getJapaneseKeyboard().getSpecification());

    viewManager.setQwertyLayoutForAlphabet(false);
    assertEquals(KeyboardSpecification.TWELVE_KEY_TOGGLE_FLICK_KANA,
                 keyboardView.getJapaneseKeyboard().getSpecification());
  }

  @SmallTest
  public void testShowMenuDialog() {
    Context context = getInstrumentation().getTargetContext();

    ViewEventListener listener = createMock(ViewEventListener.class);
    List<TouchEvent> touchEventList = Collections.singletonList(TouchEvent.getDefaultInstance());
    listener.onShowMenuDialog(touchEventList);
    replayAll();
    ViewManager viewManager = new ViewManager(context, listener, null, null);
    viewManager.onKey(
        context.getResources().getInteger(R.integer.key_menu_dialog), touchEventList);
    verifyAll();
  }

  @SmallTest
  public void testSetJapaneseKeyboardStyleForUninitializedKeyboardViewDefault() {
    Context context = getInstrumentation().getTargetContext();
    ViewManager viewManager =
        new ViewManager(context, createMock(ViewEventListener.class), null, null);

    // Make sure keyboard by createInputView is 12keys-kana keyboard by default.
    MozcView mozcView = viewManager.createMozcView(context);
    JapaneseKeyboardView keyboardView = mozcView.getKeyboardView();
    assertEquals(KeyboardSpecification.TWELVE_KEY_TOGGLE_KANA,
                 keyboardView.getJapaneseKeyboard().getSpecification());
  }

  @SmallTest
  public void testSetKeyboardLayoutForUninitializedInputView() {
    Context context = getInstrumentation().getTargetContext();
    ViewManager viewManager =
        new ViewManager(context, createMock(ViewEventListener.class), null, null);

    // Set MobileInputStyle before creating InputView.
    viewManager.setKeyboardLayout(KeyboardLayout.QWERTY);

    // Make sure the keyboard instance created via createInputView is not default keyboard.
    MozcView mozcView = viewManager.createMozcView(context);
    JapaneseKeyboardView keyboardView = mozcView.getKeyboardView();
    assertEquals(KeyboardSpecification.QWERTY_KANA,
                 keyboardView.getJapaneseKeyboard().getSpecification());
  }

  @SmallTest
  public void testSetTwelveKeyInputStyleForUninitializedInputView() {
    Context context = getInstrumentation().getTargetContext();
    ViewManager viewManager =
        new ViewManager(context, createMock(ViewEventListener.class), null, null);

    // Set TwelveKeysInputStyle before creating InputView.
    viewManager.setInputStyle(InputStyle.FLICK);

    // Make sure the keyboard instance created via createInputView is not default keyboard.
    MozcView mozcView = viewManager.createMozcView(context);
    JapaneseKeyboardView keyboardView = mozcView.getKeyboardView();
    assertEquals(KeyboardSpecification.TWELVE_KEY_FLICK_KANA,
                 keyboardView.getJapaneseKeyboard().getSpecification());
  }

  @SmallTest
  public void testSetKeyboardLayoutWithNull() {
    ViewManager viewManager = new ViewManager(getInstrumentation().getTargetContext(),
                                              createNiceMock(ViewEventListener.class),
                                              null,
                                              null);
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
    ViewManager viewManager = new ViewManager(context, listener, null, null, guesser);
    viewManager.createMozcView(context);
    KeyboardSpecification keyboardSpecification =
        JapaneseSoftwareKeyboardModel.class.cast(
            VisibilityProxy.getField(viewManager, "japaneseSoftwareKeyboardModel"))
                .getKeyboardSpecification();

    Resources resources = context.getResources();
    List<TouchEvent> touchEventList = Collections.singletonList(TouchEvent.getDefaultInstance());

    class TestData {
      final int keyCode;
      final boolean expectInvokeGuesser;
      final ProtoCommands.KeyEvent expectKeyEvent;
      final int expectKeyCode;
      TestData(int keyCode, ProbableKeyEvent[] probableKeyEvents,
               boolean expectInvokeGuesser,
               ProtoCommands.KeyEvent expectKeyEvent, int expectKeyCode) {
        this.keyCode = keyCode;
        this.expectInvokeGuesser = expectInvokeGuesser;
        this.expectKeyEvent = expectKeyEvent;
        this.expectKeyCode = expectKeyCode;
      }
    }

    TestData[] testDataList = {
        // White space.
        new TestData(' ',
                     null,
                     false,
                     ProtoCommands.KeyEvent.newBuilder().setSpecialKey(SpecialKey.SPACE).build(),
                     KeyEvent.KEYCODE_SPACE),
        // Enter.
        new TestData(resources.getInteger(R.integer.key_enter),
                     null,
                     false,
                     ProtoCommands.KeyEvent.newBuilder().setSpecialKey(SpecialKey.ENTER).build(),
                     KeyEvent.KEYCODE_ENTER),
        // Delete.
        new TestData(resources.getInteger(R.integer.key_backspace),
                     null,
                     false,
                     ProtoCommands.KeyEvent.newBuilder().setSpecialKey(SpecialKey.BACKSPACE)
                         .build(),
                     KeyEvent.KEYCODE_DEL),
        // Left.
        new TestData(resources.getInteger(R.integer.key_left),
                     null,
                     false,
                     ProtoCommands.KeyEvent.newBuilder().setSpecialKey(SpecialKey.LEFT).build(),
                     KeyEvent.KEYCODE_DPAD_LEFT),
        // Right.
        new TestData(resources.getInteger(R.integer.key_right),
                     null,
                     false,
                     ProtoCommands.KeyEvent.newBuilder().setSpecialKey(SpecialKey.RIGHT).build(),
                     KeyEvent.KEYCODE_DPAD_RIGHT),
        // Normal character with no correction stats.
        new TestData('a',
                     null,
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
            .andReturn(testData.expectKeyEvent.getProbableKeyEventList());
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
    ViewManager viewManager = new ViewManager(context, listener, null, null);
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
    ViewManager viewManager = new ViewManager(context, listener, null, null);
    MozcView mozcView = viewManager.createMozcView(context);

    Resources resources = context.getResources();

    resetAll();
    List<TouchEvent> touchEventList = Collections.singletonList(TouchEvent.getDefaultInstance());
    listener.onKeyEvent(null, null,
                        KeyboardSpecification.TWELVE_KEY_TOGGLE_NUMBER, touchEventList);
    replayAll();

    // Make sure the current keyboard is 12keys-toggle-kana mode before sending a key.
    JapaneseKeyboardView keyboardView = mozcView.getKeyboardView();
    assertEquals(KeyboardSpecification.TWELVE_KEY_TOGGLE_KANA,
                 keyboardView.getJapaneseKeyboard().getSpecification());

    // Send key code to switch number keyboard style.
    viewManager.onKey(resources.getInteger(R.integer.key_chartype_to_123), touchEventList);

    // Make sure actually keyboard style has been changed.
    assertEquals(KeyboardSpecification.TWELVE_KEY_TOGGLE_NUMBER,
                 keyboardView.getJapaneseKeyboard().getSpecification());

    // Make sure that the listener can catch the expected events.
    verifyAll();
  }

  @SmallTest
  public void testKeyboardActionAdapter_onKey() {
    Context context = getInstrumentation().getTargetContext();
    List<TouchEvent> touchEventList = Collections.singletonList(TouchEvent.getDefaultInstance());
    ViewEventListener listener = createNiceMock(ViewEventListener.class);
    ViewManager viewManager = createMockBuilder(ViewManager.class)
        .withConstructor(Context.class, ViewEventListener.class,
                         SymbolHistoryStorage.class, MenuDialogListener.class)
        .withArgs(context, listener, null, null)
        .createMock();
    resetAll();
    viewManager.onKey('1', touchEventList);
    replayAll();

    KeyboardActionListener keyboardActionListener = viewManager.new KeyboardActionAdapter();
    keyboardActionListener.onKey('1', touchEventList);

    verifyAll();
  }

  @SmallTest
  public void testKeyboardActionAdapter_onPress() {
    Context context = getInstrumentation().getTargetContext();
    ViewEventListener listener = createMock(ViewEventListener.class);
    ViewManager viewManager = new ViewManager(context, listener, null, null);

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
    ViewManager viewManager = new ViewManager(context, listener, null, null);
    viewManager.createMozcView(context);
    KeyboardSpecification keyboardSpecification =
        JapaneseSoftwareKeyboardModel.class.cast(
            VisibilityProxy.getField(viewManager, "japaneseSoftwareKeyboardModel"))
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
    ViewManager viewManager = new ViewManager(context, listener, null, null);

    // Rendering null. Just making sure that this shouldn't fail.
    viewManager.render(null);

    // Rendering before the mozcView is initialized. Again making sure no-failure.
    viewManager.render(Command.getDefaultInstance());

    // Set mozcView mock.
    MozcView mozcView = createViewMock(MozcView.class);
    VisibilityProxy.setField(viewManager, "mozcView", mozcView);

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
    ViewManager viewManager =
        new ViewManager(context, createNiceMock(ViewEventListener.class), null, null);

    MozcView mozcView = viewManager.createMozcView(context);
    // TODO(matsuzakit): The lines from the beginning of the method to here should be
    //     extracted into a method.
    // Set pre-condition and call reset().
    mozcView.getKeyboardFrame().setVisibility(View.GONE);
    LayoutParams layoutParams = mozcView.getKeyboardFrame().getLayoutParams();
    layoutParams.height = 0;
    Map<Integer, KeyEventContext> keyEventContextMap =
        VisibilityProxy.getField(mozcView.getKeyboardView(), "keyEventContextMap");
    {
      // Add dummy KeyEventContext.
      KeyEventContext keyEventContext =
          new KeyEventContext(
              new Key(0, 0, 0, 0, 0, 0, false, false, false,
                      Stick.EVEN, Collections.<KeyState>emptyList()),
              0, 0, 0, 0, 0, 0, MetaState.UNMODIFIED);
      keyEventContextMap.put(0, keyEventContext);
    }

    mozcView.getCandidateView().startInAnimation();
    mozcView.getSymbolInputView().startInAnimation();

    MenuDialog menuDialog = createMockBuilder(MenuDialog.class)
        .withConstructor(Context.class, MenuDialogListener.class)
        .withArgs(context, null)
        .createMock();
    resetAll();
    VisibilityProxy.setField(viewManager, "menuDialog", menuDialog);
    menuDialog.dismiss();
    replayAll();

    viewManager.reset();

    // Post-condition.
    verifyAll();
    assertEquals(View.VISIBLE, mozcView.getKeyboardFrame().getVisibility());
    assertEquals(context.getResources().getDimensionPixelSize(R.dimen.input_frame_height),
                 mozcView.getKeyboardFrame().getLayoutParams().height);
    assertEquals(View.GONE, mozcView.getCandidateView().getVisibility());
    assertEquals(View.GONE, mozcView.getSymbolInputView().getVisibility());
    assertNull(mozcView.getCandidateView().getAnimation());
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
    ViewManager viewManager =
        new ViewManager(context, createMock(ViewEventListener.class), null, null);
    Window window = createMockBuilder(Window.class)
        .withConstructor(Context.class)
        .withArgs(context)
        .createMock();
    Resources mockResources = createNiceMock(MockResources.class);
    Context mockContext = createNiceMock(MockContext.class);

    for (TestData testData : testDataList) {
      resetAll();
      VisibilityProxy.setField(viewManager, "mozcView", null);

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
      VisibilityProxy.setField(viewManager, "mozcView", mozcView);

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
    ViewManager viewManager =
        new ViewManager(context, createNiceMock(ViewEventListener.class), null, null);
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
    ViewManager viewManager =
        new ViewManager(context, createNiceMock(ViewEventListener.class), null, null);
    MozcView mozcView = createViewMock(MozcView.class);
    VisibilityProxy.setField(viewManager, "mozcView", mozcView);

    for (EmojiProviderType emojiProviderType : EmojiProviderType.values()) {
      resetAll();
      mozcView.setEmojiProviderType(emojiProviderType);
      replayAll();

      viewManager.setEmojiProviderType(emojiProviderType);

      verifyAll();
      assertEquals(emojiProviderType,
                   VisibilityProxy.getField(viewManager, "emojiProviderType"));
    }
  }

  @SmallTest
  public void testSetNarrowMode() {
    Context context = getInstrumentation().getTargetContext();
    ViewManager viewManager =
        new ViewManager(context, createNiceMock(ViewEventListener.class), null, null);
    MozcView mozcView = viewManager.createMozcView(context);

    // viewManager to mozcView.
    viewManager.setNarrowMode(true);
    assertTrue(mozcView.isNarrowMode());
    viewManager.setNarrowMode(false);
    assertFalse(mozcView.isNarrowMode());
  }

  @SmallTest
  public void testSetPopupEnabled() {
    Context context = getInstrumentation().getTargetContext();
    ViewManager viewManager =
        new ViewManager(context, createNiceMock(ViewEventListener.class), null, null);
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
    ViewManager viewManager =
        new ViewManager(context, createNiceMock(ViewEventListener.class), null, null);
    MozcView mozcView = viewManager.createMozcView(context);

    viewManager.setFlickSensitivity(10);
    assertEquals(10, mozcView.getKeyboardView().getFlickSensitivity());
    viewManager.setFlickSensitivity(-10);
    assertEquals(-10, mozcView.getKeyboardView().getFlickSensitivity());
  }

  @SmallTest
  public void testSetSkinType() {
    Context context = getInstrumentation().getTargetContext();
    ViewManager viewManager =
        new ViewManager(context, createNiceMock(ViewEventListener.class), null, null);
    MozcView mozcView = viewManager.createMozcView(context);

    viewManager.setSkinType(SkinType.ORANGE_LIGHTGRAY);
    assertEquals(SkinType.ORANGE_LIGHTGRAY, mozcView.getSkinType());
    viewManager.setSkinType(SkinType.TEST);
    assertEquals(SkinType.TEST, mozcView.getSkinType());
  }
}
