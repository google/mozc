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
import static org.easymock.EasyMock.anyBoolean;
import static org.easymock.EasyMock.anyObject;
import static org.easymock.EasyMock.capture;
import static org.easymock.EasyMock.eq;
import static org.easymock.EasyMock.expect;
import static org.easymock.EasyMock.expectLastCall;
import static org.easymock.EasyMock.isA;
import static org.easymock.EasyMock.same;

import org.mozc.android.inputmethod.japanese.FeedbackManager.FeedbackEvent;
import org.mozc.android.inputmethod.japanese.FeedbackManager.FeedbackListener;
import org.mozc.android.inputmethod.japanese.HardwareKeyboard.CompositionSwitchMode;
import org.mozc.android.inputmethod.japanese.JapaneseKeyboard.KeyboardSpecification;
import org.mozc.android.inputmethod.japanese.KeycodeConverter.KeyEventInterface;
import org.mozc.android.inputmethod.japanese.MozcService.SymbolHistoryStorageImpl;
import org.mozc.android.inputmethod.japanese.SymbolInputView.MajorCategory;
import org.mozc.android.inputmethod.japanese.ViewManager.LayoutAdjustment;
import org.mozc.android.inputmethod.japanese.emoji.EmojiProviderType;
import org.mozc.android.inputmethod.japanese.model.JapaneseSoftwareKeyboardModel;
import org.mozc.android.inputmethod.japanese.model.SelectionTracker;
import org.mozc.android.inputmethod.japanese.model.SymbolCandidateStorage.SymbolHistoryStorage;
import org.mozc.android.inputmethod.japanese.preference.ClientSidePreference;
import org.mozc.android.inputmethod.japanese.preference.ClientSidePreference.HardwareKeyMap;
import org.mozc.android.inputmethod.japanese.preference.ClientSidePreference.InputStyle;
import org.mozc.android.inputmethod.japanese.preference.ClientSidePreference.KeyboardLayout;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.CompositionMode;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.DeletionRange;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.GenericStorageEntry.StorageType;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.TouchEvent;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Output;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Preedit;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Preedit.Segment;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Preedit.Segment.Annotation;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Request;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Result;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Result.ResultType;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoConfig.Config;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoConfig.Config.SelectionShortcut;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoConfig.Config.SessionKeymap;
import org.mozc.android.inputmethod.japanese.session.SessionExecutor;
import org.mozc.android.inputmethod.japanese.session.SessionExecutor.EvaluationCallback;
import org.mozc.android.inputmethod.japanese.session.SessionHandlerFactory;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;
import org.mozc.android.inputmethod.japanese.testing.MozcPreferenceUtil;
import org.mozc.android.inputmethod.japanese.testing.Parameter;
import org.mozc.android.inputmethod.japanese.testing.VisibilityProxy;
import org.mozc.android.inputmethod.japanese.ui.MenuDialog.MenuDialogListener;
import org.mozc.android.inputmethod.japanese.view.SkinType;
import com.google.protobuf.ByteString;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.content.res.Configuration;
import android.media.AudioManager;
import android.os.Handler;
import android.os.SystemClock;
import android.test.suitebuilder.annotation.SmallTest;
import android.text.InputType;
import android.text.SpannableStringBuilder;
import android.util.Pair;
import android.view.KeyEvent;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

import org.easymock.Capture;

import java.lang.reflect.InvocationTargetException;
import java.util.Collections;
import java.util.List;

/**
 * Due to limitation of AndroidMock (in more precise EasyMock 2.4 used by AndroidMock),
 * It's difficult to write appropriate expectation based on IMocksControl.
 * So, this test contains bad hack;
 * - If the mock's state is before "replay", all arguments of method invocations are ignored,
 *   and default values (0, false or null) are returned.
 *   Thus, we abuse it as "a kind of nice mock", and invoke "reset" method before writing
 *   actual expectations.
 * TODO(hidehiko): Remove the hack, after switching.
 *
 */
public class MozcServiceTest extends InstrumentationTestCaseWithMock {
  private ViewManager createViewManagerMock(Context context, ViewEventListener viewEventListener) {
    return createMockBuilder(ViewManager.class)
        .withConstructor(Context.class, ViewEventListener.class,
                         SymbolHistoryStorage.class, MenuDialogListener.class)
        .withArgs(context, viewEventListener, null, null)
        .createMock();
  }

  // Consider using createInitializedService() instead of this method.
  private MozcService createService() {
    return initializeService(new MozcService());
  }

  private MozcService initializeService(MozcService service) {
    VisibilityProxy.setField(service, "sendSyncDataCommandHandler", new Handler());
    attachBaseContext(service);
    return service;
  }

  private void attachBaseContext(MozcService service) {
    try {
      VisibilityProxy.invokeByName(
          service, "attachBaseContext", getInstrumentation().getTargetContext());
    } catch (InvocationTargetException e) {
      fail(e.toString());
    }
  }

  private MozcService createInitializedService(SessionExecutor sessionExecutor) {
    MozcService service = createService();
    invokeOnCreateInternal(service, null, null, getDefaultDeviceConfiguration(), sessionExecutor);
    return service;
  }

  private MozcService initializeMozcService(MozcService service,
                                            SessionExecutor sessionExecutor) {
    invokeOnCreateInternal(initializeService(service), null, null,
                           getDefaultDeviceConfiguration(), sessionExecutor);
    return service;
  }

  private void clearSharedPreferences(SharedPreferences sharedPreferences, MozcService service) {
    OnSharedPreferenceChangeListener listener =
        VisibilityProxy.getField(service, "sharedPreferenceChangeListener");
    sharedPreferences.unregisterOnSharedPreferenceChangeListener(listener);
    Editor editor = sharedPreferences.edit();
    editor.clear();
    editor.commit();
  }

  private Configuration getDefaultDeviceConfiguration() {
    Configuration configuration = new Configuration();
    configuration.orientation = Configuration.ORIENTATION_PORTRAIT;
    // Note: Some other fields might be needed.
    // But currently only orientation field causes flaky test results.
    return configuration;
  }

  private static void invokeOnCreateInternal(MozcService service,
                                             ViewManager viewManager,
                                             SharedPreferences sharedPreferences,
                                             Configuration deviceConfiguration,
                                             SessionExecutor sessionExecutor) {
    ViewEventListener eventListener = service.new MozcEventListener();
    service.onCreateInternal(
        eventListener, viewManager, sharedPreferences, deviceConfiguration, sessionExecutor);
  }

  @SmallTest
  public void testOnCreate() {
    SharedPreferences preferences = MozcPreferenceUtil.getSharedPreferences(
        getInstrumentation().getContext(), "TEST_ON_CREATE");
    // Client-side conifg
    preferences.edit()
        .putBoolean("pref_haptic_feedback_key", true)
        .putBoolean("pref_other_anonimous_mode_key", true)
        .commit();

    SessionExecutor sessionExecutor = createStrictMock(SessionExecutor.class);
    sessionExecutor.reset(isA(SessionHandlerFactory.class), isA(Context.class));
    sessionExecutor.setLogging(anyBoolean());
    sessionExecutor.createSession();
    sessionExecutor.setImposedConfig(isA(Config.class));
    sessionExecutor.setConfig(ConfigUtil.toConfig(preferences));
    sessionExecutor.switchInputMode(isA(CompositionMode.class));
    expectLastCall().asStub();
    sessionExecutor.updateRequest(isA(Request.class), eq(Collections.<TouchEvent>emptyList()));
    expectLastCall().asStub();

    ViewManager viewManager = createMockBuilder(ViewManager.class)
        .withConstructor(Context.class, ViewEventListener.class,
                         SymbolHistoryStorage.class, MenuDialogListener.class)
        .withArgs(getInstrumentation().getTargetContext(),
                  createNiceMock(ViewEventListener.class),
                  null,
                  createNiceMock(MenuDialogListener.class))
        .addMockedMethod("onConfigurationChanged")
        .createMock();
    viewManager.onConfigurationChanged(anyObject(Configuration.class));

    replayAll();

    MozcService service = createService();

    // Both sessionExecutor and viewManager is not initialized.
    assertNull(VisibilityProxy.getField(service, "sessionExecutor"));
    assertNull(VisibilityProxy.getField(service, "viewManager"));

    // A window is not created.
    assertNull(service.getWindow());

    // Initialize the service.
    try {
      // Emulate non-dev channel situation.
      MozcUtil.setDevChannel(false);
      invokeOnCreateInternal(service, viewManager, preferences, getDefaultDeviceConfiguration(),
                             sessionExecutor);
    } finally {
      MozcUtil.setDevChannel(null);
    }

    verifyAll();

    // The following variables must have been initialized after the initialization of the service.
    assertNotNull(service.getWindow());
    assertSame(sessionExecutor, VisibilityProxy.getField(service, "sessionExecutor"));
    assertNotNull(VisibilityProxy.getField(service, "viewManager"));
    assertTrue(VisibilityProxy.<FeedbackManager>getField(service, "feedbackManager")
        .isHapticFeedbackEnabled());

    // SYNC_DATA should be sent periodically.
    assertTrue(
        VisibilityProxy.<Handler>getField(service, "sendSyncDataCommandHandler").hasMessages(0));

    // SharedPreferences should be cached.
    assertSame(preferences, VisibilityProxy.getField(service, "sharedPreferences"));
  }

  @SmallTest
  public void testOnCreateInputView() {
    MozcService service = createService();
    // A test which calls onCreateInputView before onCreate is not needed
    // because IMF ensures calling onCreate before onCreateInputView.
    SessionExecutor sessionExecutor = createNiceMock(SessionExecutor.class);
    replayAll();

    invokeOnCreateInternal(service, null, null, getDefaultDeviceConfiguration(), sessionExecutor);

    verifyAll();
    assertNotNull(service.onCreateInputView());
    // Created view is tested in CandidateViewTest.
  }

  @SmallTest
  public void testOnFinishInput() {
    SessionExecutor sessionExecutor = createMock(SessionExecutor.class);
    MozcService service = createInitializedService(sessionExecutor);

    resetAll();
    sessionExecutor.resetContext();

    ViewEventListener eventListener = createNiceMock(ViewEventListener.class);
    Context context = getInstrumentation().getTargetContext();
    ViewManager viewManager = createViewManagerMock(context, eventListener);
    VisibilityProxy.setField(service, "viewManager", viewManager);
    viewManager.reset();
    replayAll();

    // When onFinishInput is called, the inputHandler should receive CLEANUP command.
    service.onFinishInput();

    verifyAll();
  }

  @SmallTest
  public void testOnCompositionModeChange() {
    SessionExecutor sessionExecutor = createMock(SessionExecutor.class);
    MozcService service = createInitializedService(sessionExecutor);

    VisibilityProxy.setField(
        service,
        "currentKeyboardSpecification", KeyboardSpecification.TWELVE_KEY_TOGGLE_ALPHABET);
    resetAll();
    sessionExecutor.updateRequest(anyObject(ProtoCommands.Request.class),
                                  eq(Collections.<TouchEvent>emptyList()));
    // Exactly one message is issued to switch composition mode.
    // It should not render a result in order to preserve
    // the existent text. b/5181946
    sessionExecutor.switchInputMode(CompositionMode.HIRAGANA);
    replayAll();

    ViewEventListener listener = service.new MozcEventListener();
    listener.onKeyEvent(null, null, KeyboardSpecification.TWELVE_KEY_TOGGLE_KANA,
                        Collections.<TouchEvent>emptyList());

    verifyAll();
  }

  @SmallTest
  public void testOnKeyDown() {
    SessionExecutor sessionExecutor = createNiceMock(SessionExecutor.class);
    replayAll();
    MozcService service = initializeMozcService(
        new MozcService() {
          @Override
          public boolean isInputViewShown() {
            return true;
          }
        },
        sessionExecutor);

    KeyEvent[] keyEventsToBeConsumed = {
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_SPACE),
        // 'a'
        new KeyEvent(0, 0, 0, 0, 0, 0, 0, 0x1E),
    };

    for (KeyEvent keyEvent : keyEventsToBeConsumed) {
      assertTrue(service.onKeyDown(keyEvent.getKeyCode(), keyEvent));
    }

    KeyEvent[] keyEventsNotToBeConsumed = {
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_HOME),
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_MENU),
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_SEARCH),
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_VOLUME_DOWN),
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_VOLUME_MUTE),
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_VOLUME_UP),
    };

    for (KeyEvent keyEvent : keyEventsNotToBeConsumed) {
      assertFalse(service.onKeyDown(keyEvent.getKeyCode(), keyEvent));
    }

    verifyAll();
  }

  @SmallTest
  public void testOnKeyUp() {
    SessionExecutor sessionExecutor = createMockBuilder(SessionExecutor.class)
        .addMockedMethod("sendKeyEvent")
        .createMock();
    replayAll();
    MozcService service = initializeMozcService(
        new MozcService() {
          @Override
          public boolean isInputViewShown() {
            return true;
          }
        },
        sessionExecutor);

    KeyEvent[] keyEventsToBeConsumed = {
        new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_SPACE),
        // 'a'
        new KeyEvent(0, 0, 0, 0, 0, 0, 0, 0x1E),
    };

    for (KeyEvent keyEvent : keyEventsToBeConsumed) {
      assertTrue(service.onKeyUp(keyEvent.getKeyCode(), keyEvent));
    }

    KeyEvent[] keyEventsNotToBeConsumed = {
        new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_HOME),
        new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_MENU),
        new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_SEARCH),
        new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_VOLUME_DOWN),
        new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_VOLUME_MUTE),
        new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_VOLUME_UP),
    };

    for (KeyEvent keyEvent : keyEventsNotToBeConsumed) {
      assertFalse(service.onKeyUp(keyEvent.getKeyCode(), keyEvent));
    }

    verifyAll();

    KeyEvent[] keyEventsPassedToCallback = {
        new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_BACK),
        new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_SHIFT_LEFT),
        new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_SHIFT_RIGHT),
        new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_ALT_LEFT),
        new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_ALT_RIGHT),
        new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_CTRL_LEFT),
        new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_CTRL_RIGHT),
    };
    for (KeyEvent keyEvent : keyEventsPassedToCallback) {
      resetAll();
      sessionExecutor.sendKeyEvent(matchesKeyEvent(keyEvent), isA(EvaluationCallback.class));
      replayAll();
      assertTrue(service.onKeyUp(keyEvent.getKeyCode(), keyEvent));
      verifyAll();
    }
  }

  @SmallTest
  public void testOnKeyDownByHardwareKeyboard() {
    Context context = getInstrumentation().getTargetContext();

    MozcService service = createMockBuilder(MozcService.class)
        .addMockedMethods("isInputViewShown", "sendKeyWithKeyboardSpecification")
        .createMock();
    SessionExecutor sessionExecutor = createNiceMock(SessionExecutor.class);
    HardwareKeyboard hardwareKeyboard = createNiceMock(HardwareKeyboard.class);
    ViewEventListener eventListener = createNiceMock(ViewEventListener.class);
    ViewManager viewManager = createMockBuilder(ViewManager.class)
        .withConstructor(Context.class, ViewEventListener.class,
                         SymbolHistoryStorage.class, MenuDialogListener.class)
        .withArgs(context, eventListener, null, null)
        .createNiceMock();

    // Before test, we need to setup mock instances with some method calls.
    // Because the calls is out of this test, we do it by NiceMock and replayAll.
    replayAll();
    initializeMozcService(service, sessionExecutor);
    VisibilityProxy.setField(service, "hardwareKeyboard", hardwareKeyboard);
    SharedPreferences sharedPreferences = MozcPreferenceUtil.getSharedPreferences(
        context, "TEST_HARDWARE_KEY_DOWN");
    invokeOnCreateInternal(service, viewManager, sharedPreferences,
                           context.getResources().getConfiguration(), sessionExecutor);
    sharedPreferences.edit().remove(HardwareKeyboardSpecification.PREF_HARDWARE_KEYMAP_KEY)
        .commit();
    verifyAll();

    // Real test part.
    // In not narrow mode.
    resetAll();

    KeyEvent event = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_A);
    ProtoCommands.KeyEvent mozcKeyEvent =
        ProtoCommands.KeyEvent.newBuilder().setKeyCode('a').build();
    KeyboardSpecification keyboardSpecification = KeyboardSpecification.HARDWARE_QWERTY_KANA;

    assertNull(KeycodeConverter.getMozcSpecialKeyEvent(event));
    expect(service.isInputViewShown()).andStubReturn(true);

    ClientSidePreference clientSidePreference =
        VisibilityProxy.getField(service, "propagatedClientSidePreference");
    assertNotNull(clientSidePreference);
    assertNull(clientSidePreference.getHardwareKeyMap());
    hardwareKeyboard.setHardwareKeyMap(anyObject(HardwareKeyMap.class));

    expect(hardwareKeyboard.isKeyToConsume(event)).andStubReturn(true);
    expect(hardwareKeyboard.getCompositionMode()).andStubReturn(CompositionMode.HIRAGANA);

    expect(viewManager.isNarrowMode()).andReturn(false);
    expect(viewManager.hideSubInputView()).andReturn(true);
    viewManager.setNarrowMode(true);
    expect(hardwareKeyboard.setCompositionModeByKey(event)).andReturn(false);
    expect(hardwareKeyboard.getMozcKeyEvent(event)).andReturn(mozcKeyEvent);
    expect(hardwareKeyboard.getKeyEventInterface(event))
        .andReturn(KeycodeConverter.getKeyEventInterface(event));
    expect(hardwareKeyboard.getKeyboardSpecification()).andReturn(keyboardSpecification);
    expect(service.sendKeyWithKeyboardSpecification(
        same(mozcKeyEvent), matchesKeyEvent(event),
        same(keyboardSpecification),
        eq(getDefaultDeviceConfiguration()),
        eq(Collections.<TouchEvent>emptyList()))).andReturn(false);

    replayAll();

    service.onKeyDownInternal(0, event, getDefaultDeviceConfiguration());

    verifyAll();

    // In narrow mode.
    resetAll();

    expect(service.isInputViewShown()).andStubReturn(true);
    expect(hardwareKeyboard.isKeyToConsume(event)).andStubReturn(true);
    expect(hardwareKeyboard.getCompositionMode()).andStubReturn(CompositionMode.HIRAGANA);

    expect(viewManager.isNarrowMode()).andReturn(true);
    expect(hardwareKeyboard.setCompositionModeByKey(event)).andReturn(false);
    expect(hardwareKeyboard.getMozcKeyEvent(event)).andReturn(mozcKeyEvent);
    expect(hardwareKeyboard.getKeyEventInterface(event))
        .andReturn(KeycodeConverter.getKeyEventInterface(event));
    expect(hardwareKeyboard.getKeyboardSpecification()).andReturn(keyboardSpecification);
    expect(service.sendKeyWithKeyboardSpecification(
        same(mozcKeyEvent), matchesKeyEvent(event),
        same(keyboardSpecification),
        eq(getDefaultDeviceConfiguration()),
        eq(Collections.<TouchEvent>emptyList()))).andReturn(false);

    replayAll();

    service.onKeyDownInternal(0, event, getDefaultDeviceConfiguration());

    verifyAll();

    // Change CompositionMode.
    resetAll();

    expect(service.isInputViewShown()).andStubReturn(true);
    expect(hardwareKeyboard.isKeyToConsume(event)).andStubReturn(true);

    expect(viewManager.isNarrowMode()).andReturn(true);
    expect(hardwareKeyboard.getCompositionMode()).andReturn(CompositionMode.HIRAGANA);
    expect(hardwareKeyboard.setCompositionModeByKey(event)).andReturn(false);
    expect(hardwareKeyboard.getCompositionMode()).andReturn(CompositionMode.HALF_ASCII);
    viewManager.setHardwareKeyboardCompositionMode(CompositionMode.HALF_ASCII);
    expect(hardwareKeyboard.getMozcKeyEvent(event)).andReturn(mozcKeyEvent);
    expect(hardwareKeyboard.getKeyEventInterface(event))
        .andReturn(KeycodeConverter.getKeyEventInterface(event));
    expect(hardwareKeyboard.getKeyboardSpecification()).andReturn(keyboardSpecification);
    expect(service.sendKeyWithKeyboardSpecification(
        same(mozcKeyEvent), matchesKeyEvent(event),
        same(keyboardSpecification),
        eq(getDefaultDeviceConfiguration()),
        eq(Collections.<TouchEvent>emptyList()))).andReturn(false);

    replayAll();

    service.onKeyDownInternal(0, event, getDefaultDeviceConfiguration());

    verifyAll();
  }

  @SmallTest
  public void testSendKeyWithKeyboardSpecification_switchKeyboardSpecification() {
    SessionExecutor sessionExecutor = createMock(SessionExecutor.class);
    MozcService service = createInitializedService(sessionExecutor);

    EvaluationCallback evaluationCallback = VisibilityProxy.getField(service, "evaluationCallback");

    VisibilityProxy.setField(service, "currentKeyboardSpecification",
                             KeyboardSpecification.TWELVE_KEY_FLICK_KANA);

    resetAll();
    ProtoCommands.KeyEvent mozcKeyEvent =
        ProtoCommands.KeyEvent.newBuilder().setKeyCode('a').build();
    KeyEventInterface keyEvent = KeycodeConverter.getKeyEventInterface(KeyEvent.KEYCODE_A);
    KeyboardSpecification keyboardSpecification = KeyboardSpecification.TWELVE_KEY_FLICK_KANA;

    service.sendKeyWithKeyboardSpecification(
        mozcKeyEvent, keyEvent, keyboardSpecification, getDefaultDeviceConfiguration(),
        Collections.<TouchEvent>emptyList());

    replayAll();

    sessionExecutor.sendKey(
        mozcKeyEvent, keyEvent, Collections.<TouchEvent>emptyList(), evaluationCallback);

    verifyAll();

    resetAll();

    mozcKeyEvent = ProtoCommands.KeyEvent.newBuilder().setKeyCode('a').build();
    keyEvent = KeycodeConverter.getKeyEventInterface(KeyEvent.KEYCODE_A);
    keyboardSpecification = KeyboardSpecification.HARDWARE_QWERTY_KANA;

    service.sendKeyWithKeyboardSpecification(
        mozcKeyEvent, keyEvent, keyboardSpecification, getDefaultDeviceConfiguration(),
        Collections.<TouchEvent>emptyList());

    replayAll();

    sessionExecutor.updateRequest(
        MozcUtil.getRequestForKeyboard(keyboardSpecification.getKeyboardSpecificationName(),
                                       keyboardSpecification.getSpecialRomanjiTable(),
                                       keyboardSpecification.getSpaceOnAlphanumeric(),
                                       keyboardSpecification.isKanaModifierInsensitiveConversion(),
                                       keyboardSpecification.getCrossingEdgeBehavior(),
                                       getDefaultDeviceConfiguration()),
        Collections.<TouchEvent>emptyList());
    sessionExecutor.sendKey(
        ProtoCommands.KeyEvent.newBuilder(mozcKeyEvent)
            .setMode(keyboardSpecification.getCompositionMode())
            .build(),
        keyEvent, Collections.<TouchEvent>emptyList(), evaluationCallback);

    verifyAll();

    resetAll();

    mozcKeyEvent = ProtoCommands.KeyEvent.newBuilder().setKeyCode('a').build();
    keyEvent = KeycodeConverter.getKeyEventInterface(KeyEvent.KEYCODE_A);
    keyboardSpecification = KeyboardSpecification.HARDWARE_QWERTY_KANA;

    service.sendKeyWithKeyboardSpecification(
        mozcKeyEvent, keyEvent, keyboardSpecification, getDefaultDeviceConfiguration(),
        Collections.<TouchEvent>emptyList());

    replayAll();

    sessionExecutor.sendKey(
        mozcKeyEvent, keyEvent, Collections.<TouchEvent>emptyList(), evaluationCallback);

    verifyAll();

    resetAll();

    mozcKeyEvent = null;
    keyEvent = null;
    keyboardSpecification = KeyboardSpecification.TWELVE_KEY_FLICK_KANA;

    service.sendKeyWithKeyboardSpecification(
        mozcKeyEvent, keyEvent, keyboardSpecification, getDefaultDeviceConfiguration(),
        Collections.<TouchEvent>emptyList());

    replayAll();

    sessionExecutor.updateRequest(
        MozcUtil.getRequestForKeyboard(keyboardSpecification.getKeyboardSpecificationName(),
                                       keyboardSpecification.getSpecialRomanjiTable(),
                                       keyboardSpecification.getSpaceOnAlphanumeric(),
                                       keyboardSpecification.isKanaModifierInsensitiveConversion(),
                                       keyboardSpecification.getCrossingEdgeBehavior(),
                                       getDefaultDeviceConfiguration()),
        Collections.<TouchEvent>emptyList());
    sessionExecutor.switchInputMode(keyboardSpecification.getCompositionMode());

    verifyAll();
  }


  @SmallTest
  public void testHideWindow() {
    SessionExecutor sessionExecutor = createStrictMock(SessionExecutor.class);
    MozcService service = createInitializedService(sessionExecutor);
    InputConnection inputConnection = createMock(InputConnection.class);
    // restartInput() can be used to install our InputConnection safely.
    service.onCreateInputMethodInterface().restartInput(inputConnection, new EditorInfo());

    // When the IME is binded to certain view and its window has been shown by showWindow(),
    // "finishComposingText" is called from InputMethodService::hideWindow() so that
    // the preedit is submitted.
    // However "finishComposingText" is not called in this test case because
    // the inputConnection is not binded to a view.
    resetAll();
    sessionExecutor.removePendingEvaluations();
    sessionExecutor.resetContext();
    replayAll();

    service.onWindowHidden();

    verifyAll();
  }

  @SmallTest
  public void testRenderInputConnection_updatingPreedit() {
    SessionExecutor sessionExecutor = createNiceMock(SessionExecutor.class);
    MozcService service = createInitializedService(sessionExecutor);
    SelectionTracker selectionTracker = createMock(SelectionTracker.class);
    VisibilityProxy.setField(service, "selectionTracker", selectionTracker);
    InputConnection inputConnection = createStrictMock(InputConnection.class);
    service.onCreateInputMethodInterface().restartInput(inputConnection, new EditorInfo());

    Preedit preedit = Preedit.newBuilder()
        .addSegment(Segment.newBuilder()
            .setValue("0")
            .setAnnotation(Annotation.UNDERLINE)
            .setValueLength(1))
        .addSegment(Segment.newBuilder()
            .setValue("1")
            .setAnnotation(Annotation.UNDERLINE)
            .setValueLength(1))
        .addSegment(Segment.newBuilder()
            .setValue("2")
            .setAnnotation(Annotation.HIGHLIGHT)
            .setValueLength(1))
        .setCursor(3)
        .build();
    Output output = Output.newBuilder().setConsumed(true).setPreedit(preedit).build();

    resetAll();
    expect(inputConnection.beginBatchEdit()).andReturn(true);
    Capture<SpannableStringBuilder> spannableStringBuilderCapture =
        new Capture<SpannableStringBuilder>();
    // the cursor is at the tail
    expect(inputConnection.setComposingText(capture(spannableStringBuilderCapture), eq(1)))
        .andReturn(true);
    selectionTracker.onRender(null, null, preedit);
    expect(inputConnection.endBatchEdit()).andReturn(true);
    replayAll();

    service.renderInputConnection(output, KeycodeConverter.getKeyEventInterface('\0'));

    verifyAll();
    SpannableStringBuilder sb = spannableStringBuilderCapture.getValue();
    assertEquals("012", sb.toString());
    assertEquals(0, sb.getSpanStart(VisibilityProxy.getField(service, "SPAN_UNDERLINE")));
    assertEquals(3, sb.getSpanEnd(VisibilityProxy.getField(service, "SPAN_UNDERLINE")));
    assertEquals(2, sb.getSpanStart(VisibilityProxy.getField(service, "SPAN_CONVERT_HIGHLIGHT")));
    assertEquals(3, sb.getSpanEnd(VisibilityProxy.getField(service, "SPAN_CONVERT_HIGHLIGHT")));
  }

  @SmallTest
  public void testRenderInputConnection_updatingPreeditAtCursorMiddle() {
    // Updating preedit (cursor is at the middle)
    SessionExecutor sessionExecutor = createMock(SessionExecutor.class);
    MozcService service = createInitializedService(sessionExecutor);
    SelectionTracker selectionTracker = createMock(SelectionTracker.class);
    VisibilityProxy.setField(service, "selectionTracker", selectionTracker);
    InputConnection inputConnection = createStrictMock(InputConnection.class);
    service.onCreateInputMethodInterface().restartInput(inputConnection, new EditorInfo());

    Preedit preedit = Preedit.newBuilder()
        .addSegment(Segment.newBuilder()
            .setValue("0")
            .setAnnotation(Annotation.UNDERLINE)
            .setValueLength(1))
        .addSegment(Segment.newBuilder()
            .setValue("1")
            .setAnnotation(Annotation.UNDERLINE)
            .setValueLength(1))
        .addSegment(Segment.newBuilder()
            .setValue("2")
            .setAnnotation(Annotation.UNDERLINE)
            .setValueLength(1))
        .setCursor(2)
        .build();
    Output output = Output.newBuilder().setConsumed(true).setPreedit(preedit).build();

    resetAll();
    expect(inputConnection.beginBatchEdit()).andReturn(true);
    Capture<SpannableStringBuilder> spannableStringBuilderCapture =
        new Capture<SpannableStringBuilder>();
    expect(inputConnection.setComposingText(
        capture(spannableStringBuilderCapture), eq(MozcUtil.CURSOR_POSITION_TAIL)))
        .andReturn(true);
    expect(selectionTracker.getPreeditStartPosition()).andReturn(0);
    expect(inputConnection.setSelection(2, 2)).andReturn(true);
    selectionTracker.onRender(null, null, preedit);
    expect(inputConnection.endBatchEdit()).andReturn(true);
    replayAll();

    service.renderInputConnection(output, KeycodeConverter.getKeyEventInterface('\0'));

    verifyAll();
    SpannableStringBuilder sb = spannableStringBuilderCapture.getValue();
    assertEquals("012", sb.toString());
    assertEquals(0, sb.getSpanStart(VisibilityProxy.getField(service, "SPAN_UNDERLINE")));
    assertEquals(3, sb.getSpanEnd(VisibilityProxy.getField(service, "SPAN_UNDERLINE")));
    assertEquals(0, sb.getSpanStart(VisibilityProxy.getField(service, "SPAN_BEFORE_CURSOR")));
    assertEquals(2, sb.getSpanEnd(VisibilityProxy.getField(service, "SPAN_BEFORE_CURSOR")));
  }

  @SmallTest
  public void testRenderInputConnection_webView() {
    // Updating preedit (cursor is at the middle)
    SessionExecutor sessionExecutor = createNiceMock(SessionExecutor.class);
    SelectionTracker selectionTracker = createMock(SelectionTracker.class);
    MozcService service = createInitializedService(sessionExecutor);
    InputConnection inputConnection = createStrictMock(InputConnection.class);
    EditorInfo editorInfo = new EditorInfo();
    editorInfo.inputType = InputType.TYPE_TEXT_VARIATION_WEB_EDIT_TEXT;
    service.onCreateInputMethodInterface().restartInput(inputConnection, editorInfo);

    Preedit preedit = Preedit.newBuilder()
        .addSegment(Segment.newBuilder()
            .setValue("0")
            .setAnnotation(Annotation.UNDERLINE)
            .setValueLength(1))
        .addSegment(Segment.newBuilder()
            .setValue("1")
            .setAnnotation(Annotation.UNDERLINE)
            .setValueLength(1))
        .addSegment(Segment.newBuilder()
            .setValue("2")
            .setAnnotation(Annotation.UNDERLINE)
            .setValueLength(1))
        .setCursor(2)
        .build();

    resetAll();
    expect(inputConnection.beginBatchEdit()).andReturn(true);
    Capture<SpannableStringBuilder> spannableStringBuilderCapture =
        new Capture<SpannableStringBuilder>();
    expect(inputConnection.setComposingText(
        capture(spannableStringBuilderCapture), eq(MozcUtil.CURSOR_POSITION_TAIL)))
        .andReturn(true);
    expect(selectionTracker.getPreeditStartPosition()).andReturn(0);
    expect(inputConnection.setSelection(2, 2)).andReturn(true);
    selectionTracker.onRender(null, null, preedit);
    expect(inputConnection.endBatchEdit()).andReturn(true);
    replayAll();

    VisibilityProxy.setField(service, "selectionTracker", selectionTracker);

    Output output = Output.newBuilder().setConsumed(true).setPreedit(preedit).build();
    service.renderInputConnection(output, KeycodeConverter.getKeyEventInterface('\0'));

    verifyAll();
    SpannableStringBuilder sb = spannableStringBuilderCapture.getValue();
    assertEquals("012", sb.toString());
    assertEquals(0, sb.getSpanStart(VisibilityProxy.getField(service, "SPAN_UNDERLINE")));
    assertEquals(3, sb.getSpanEnd(VisibilityProxy.getField(service, "SPAN_UNDERLINE")));
    assertEquals(0, sb.getSpanStart(VisibilityProxy.getField(service, "SPAN_BEFORE_CURSOR")));
    assertEquals(2, sb.getSpanEnd(VisibilityProxy.getField(service, "SPAN_BEFORE_CURSOR")));
  }

  @SmallTest
  public void testRenderInputConnection_commit() {
    SessionExecutor sessionExecutor = createNiceMock(SessionExecutor.class);
    MozcService service = createInitializedService(sessionExecutor);
    SelectionTracker selectionTracker = createMock(SelectionTracker.class);
    VisibilityProxy.setField(service, "selectionTracker", selectionTracker);

    InputConnection inputConnection = createStrictMock(InputConnection.class);
    service.onCreateInputMethodInterface().restartInput(inputConnection, new EditorInfo());

    Preedit preedit = Preedit.newBuilder()
        .addSegment(Segment.newBuilder()
            .setValue("0")
            .setAnnotation(Annotation.UNDERLINE)
            .setValueLength(1))
        .addSegment(Segment.newBuilder()
            .setValue("1")
            .setAnnotation(Annotation.UNDERLINE)
            .setValueLength(1))
        .addSegment(Segment.newBuilder()
            .setValue("2")
            .setAnnotation(Annotation.HIGHLIGHT)
            .setValueLength(1))
        .setCursor(3)
        .build();
    Output output = Output.newBuilder()
        .setConsumed(true)
        .setResult(Result.newBuilder().setValue("commit").setType(ResultType.STRING))
        .setPreedit(preedit)
        .build();

    resetAll();
    expect(inputConnection.beginBatchEdit()).andReturn(true);
    expect(inputConnection.commitText("commit", MozcUtil.CURSOR_POSITION_TAIL)).andReturn(true);
    Capture<SpannableStringBuilder> spannableStringBuilderCapture =
        new Capture<SpannableStringBuilder>();
    expect(inputConnection.setComposingText(
        capture(spannableStringBuilderCapture), eq(MozcUtil.CURSOR_POSITION_TAIL)))
        .andReturn(true);
    selectionTracker.onRender(null, "commit", preedit);
    expect(inputConnection.endBatchEdit()).andReturn(true);
    replayAll();

    service.renderInputConnection(output, KeycodeConverter.getKeyEventInterface('\0'));

    verifyAll();
    SpannableStringBuilder sb = spannableStringBuilderCapture.getValue();
    assertEquals("012", sb.toString());
    assertEquals(0, sb.getSpanStart(VisibilityProxy.getField(service, "SPAN_UNDERLINE")));
    assertEquals(3, sb.getSpanEnd(VisibilityProxy.getField(service, "SPAN_UNDERLINE")));
    assertEquals(2, sb.getSpanStart(VisibilityProxy.getField(service, "SPAN_CONVERT_HIGHLIGHT")));
    assertEquals(3, sb.getSpanEnd(VisibilityProxy.getField(service, "SPAN_CONVERT_HIGHLIGHT")));
  }

  @SmallTest
  public void testRenderInputConnection_firefox() {
    SessionExecutor sessionExecutor = createNiceMock(SessionExecutor.class);
    MozcService service = createInitializedService(sessionExecutor);
    SelectionTracker selectionTracker = createMock(SelectionTracker.class);
    VisibilityProxy.setField(service, "selectionTracker", selectionTracker);

    InputConnection inputConnection = createStrictMock(InputConnection.class);
    {
      EditorInfo editorInfo = new EditorInfo();
      editorInfo.packageName = "org.mozilla.firefox";
      service.onCreateInputMethodInterface().restartInput(inputConnection, editorInfo);
    }

    Preedit preedit = Preedit.newBuilder()
        .addSegment(Segment.newBuilder()
            .setValue("0")
            .setAnnotation(Annotation.UNDERLINE)
            .setValueLength(1))
        .addSegment(Segment.newBuilder()
            .setValue("1")
            .setAnnotation(Annotation.UNDERLINE)
            .setValueLength(1))
        .addSegment(Segment.newBuilder()
            .setValue("2")
            .setAnnotation(Annotation.HIGHLIGHT)
            .setValueLength(1))
        .setCursor(3)
        .build();
    Output output = Output.newBuilder()
        .setConsumed(true)
        .setResult(Result.newBuilder().setValue("commit").setType(ResultType.STRING))
        .setPreedit(preedit)
        .build();

    resetAll();
    expect(inputConnection.beginBatchEdit()).andReturn(true);
    expect(inputConnection.commitText("commit", MozcUtil.CURSOR_POSITION_TAIL)).andReturn(true);
    Capture<SpannableStringBuilder> spannableStringBuilderCapture =
        new Capture<SpannableStringBuilder>();
    expect(inputConnection.setComposingText(
        capture(spannableStringBuilderCapture), eq(MozcUtil.CURSOR_POSITION_TAIL)))
        .andReturn(true);
    expect(selectionTracker.getPreeditStartPosition()).andReturn(0);
    expect(inputConnection.setSelection(9, 9)).andReturn(true);
    selectionTracker.onRender(null, "commit", preedit);
    expect(inputConnection.endBatchEdit()).andReturn(true);
    replayAll();

    service.renderInputConnection(output, KeycodeConverter.getKeyEventInterface('\0'));

    verifyAll();
    SpannableStringBuilder sb = spannableStringBuilderCapture.getValue();
    assertEquals("012", sb.toString());
    assertEquals(0, sb.getSpanStart(VisibilityProxy.getField(service, "SPAN_UNDERLINE")));
    assertEquals(3, sb.getSpanEnd(VisibilityProxy.getField(service, "SPAN_UNDERLINE")));
    assertEquals(2, sb.getSpanStart(VisibilityProxy.getField(service, "SPAN_CONVERT_HIGHLIGHT")));
    assertEquals(3, sb.getSpanEnd(VisibilityProxy.getField(service, "SPAN_CONVERT_HIGHLIGHT")));
  }

  @SmallTest
  public void testRenderInputConnection_directInput() {
    // If mozc service doesn't consume the KeyEvent, delegate it to the sendKeyEvent.
    MozcService service = createMockBuilder(MozcService.class)
        .addMockedMethods("sendKeyEvent")
        .createMock();
    SessionExecutor sessionExecutor = createNiceMock(SessionExecutor.class);
    initializeMozcService(service, sessionExecutor);
    InputConnection inputConnection = createStrictMock(InputConnection.class);
    service.onCreateInputMethodInterface().restartInput(inputConnection, new EditorInfo());

    KeyEventInterface keyEvent = KeycodeConverter.getKeyEventInterface(KeyEvent.KEYCODE_A);
    resetAll();
    service.sendKeyEvent(same(keyEvent));
    replayAll();

    service.renderInputConnection(Output.newBuilder().setConsumed(false).build(), keyEvent);

    verifyAll();
  }

  @SmallTest
  public void testSendKeyEvent() {
    MozcService service = createMockBuilder(MozcService.class)
        .addMockedMethods("requestHideSelf", "sendDownUpKeyEvents", "isInputViewShown",
                          "sendDefaultEditorAction", "getCurrentInputConnection")
        .createMock();
    ViewEventListener eventListener = createNiceMock(ViewEventListener.class);
    ViewManager viewManager =
        createViewManagerMock(getInstrumentation().getTargetContext(), eventListener);
    VisibilityProxy.setField(service, "viewManager", viewManager);

    // If the given KeyEvent is null, do nothing.
    resetAll();
    replayAll();

    service.sendKeyEvent(null);

    verifyAll();

    // If the keyEvent is other than back key or enter key,
    // just forward it to the connected application.
    resetAll();
    expect(service.getCurrentInputConnection()).andStubReturn(createMock(InputConnection.class));
    service.sendDownUpKeyEvents(KeyEvent.KEYCODE_A);
    replayAll();

    service.sendKeyEvent(KeycodeConverter.getKeyEventInterface(KeyEvent.KEYCODE_A));

    verifyAll();

    // For meta keys, captured once.
    {
      resetAll();
      InputConnection inputConnection = createMock(InputConnection.class);
      expect(service.getCurrentInputConnection()).andStubReturn(inputConnection);
      Capture<KeyEvent> captureKeyEvent = new Capture<KeyEvent>();
      expect(inputConnection.sendKeyEvent(capture(captureKeyEvent))).andReturn(true);
      KeyEvent keyEvent = new KeyEvent(
          SystemClock.uptimeMillis(),
          SystemClock.uptimeMillis(),
          KeyEvent.ACTION_UP,
          KeyEvent.KEYCODE_SHIFT_LEFT,
          3,
          KeyEvent.META_SHIFT_ON,
          5,
          0xff,
          KeyEvent.FLAG_LONG_PRESS);
      replayAll();
      service.sendKeyEvent(KeycodeConverter.getKeyEventInterface(keyEvent));
      verifyAll();
      KeyEvent captured = captureKeyEvent.getValue();
      assertEquals(keyEvent.getDownTime(), captured.getDownTime());
      // Sent KeyEvent should have event time delayed to original KeyEvent.
      assertTrue(keyEvent.getEventTime() <= captured.getEventTime());
      assertEquals(keyEvent.getAction(), captured.getAction());
      assertEquals(keyEvent.getKeyCode(), captured.getKeyCode());
      assertEquals(keyEvent.getRepeatCount(), captured.getRepeatCount());
      assertEquals(keyEvent.getMetaState(), captured.getMetaState());
      assertEquals(keyEvent.getDeviceId(), captured.getDeviceId());
      assertEquals(keyEvent.getScanCode(), captured.getScanCode());
      assertEquals(keyEvent.getFlags(), captured.getFlags());
    }

    // For other keys, captured twice.
    {
      resetAll();
      InputConnection inputConnection = createStrictMock(InputConnection.class);
      expect(service.getCurrentInputConnection()).andStubReturn(inputConnection);
      Capture<KeyEvent> captureKeyEventDown = new Capture<KeyEvent>();
      Capture<KeyEvent> captureKeyEventUp = new Capture<KeyEvent>();
      expect(inputConnection.sendKeyEvent(capture(captureKeyEventDown))).andReturn(true);
      expect(inputConnection.sendKeyEvent(capture(captureKeyEventUp))).andReturn(true);
      KeyEvent keyEvent = new KeyEvent(
          SystemClock.uptimeMillis(),
          SystemClock.uptimeMillis(),
          KeyEvent.ACTION_UP,
          KeyEvent.KEYCODE_A,
          3,
          KeyEvent.META_SHIFT_ON,
          5,
          0xff,
          KeyEvent.FLAG_LONG_PRESS);
      replayAll();
      service.sendKeyEvent(KeycodeConverter.getKeyEventInterface(keyEvent));
      verifyAll();
      KeyEvent capturedDown = captureKeyEventDown.getValue();
      assertEquals(keyEvent.getDownTime(), capturedDown.getDownTime());
      // Sent KeyEvent should have event time delayed to original KeyEvent.
      assertTrue(keyEvent.getEventTime() <= capturedDown.getEventTime());
      assertEquals(KeyEvent.ACTION_DOWN, capturedDown.getAction());
      assertEquals(keyEvent.getKeyCode(), capturedDown.getKeyCode());
      assertEquals(0, capturedDown.getRepeatCount());
      assertEquals(keyEvent.getMetaState(), capturedDown.getMetaState());
      assertEquals(keyEvent.getDeviceId(), capturedDown.getDeviceId());
      assertEquals(keyEvent.getScanCode(), capturedDown.getScanCode());
      assertEquals(keyEvent.getFlags(), capturedDown.getFlags());
      KeyEvent capturedUp = captureKeyEventUp.getValue();
      assertEquals(keyEvent.getDownTime(), capturedUp.getDownTime());
      // Sent KeyEvent should have event time delayed to original KeyEvent.
      assertTrue(keyEvent.getEventTime() <= capturedUp.getEventTime());
      assertEquals(KeyEvent.ACTION_UP, capturedUp.getAction());
      assertEquals(keyEvent.getKeyCode(), capturedUp.getKeyCode());
      assertEquals(0, capturedUp.getRepeatCount());
      assertEquals(keyEvent.getMetaState(), capturedUp.getMetaState());
      assertEquals(keyEvent.getDeviceId(), capturedUp.getDeviceId());
      assertEquals(keyEvent.getScanCode(), capturedUp.getScanCode());
      assertEquals(keyEvent.getFlags(), capturedUp.getFlags());
    }
  }

  @SmallTest
  public void testSendKeyEventBack() {
    MozcService service = createMockBuilder(MozcService.class)
        .addMockedMethods("requestHideSelf", "sendDownUpKeyEvents", "isInputViewShown",
                          "sendDefaultEditorAction")
        .createMock();
    ViewEventListener eventListener = createNiceMock(ViewEventListener.class);
    ViewManager viewManager =
        createViewManagerMock(getInstrumentation().getTargetContext(), eventListener);
    VisibilityProxy.setField(service, "viewManager", viewManager);

    KeyEventInterface[] keyEventList = {
        // Software key event.
        KeycodeConverter.getKeyEventInterface(KeyEvent.KEYCODE_BACK),
        // Hardware key event.
        KeycodeConverter.getKeyEventInterface(new KeyEvent(KeyEvent.ACTION_DOWN,
                                                           KeyEvent.KEYCODE_BACK)),
    };

    for (KeyEventInterface keyEvent : keyEventList) {
      // Test for back key.
      // If the input view is not shown, don't handle inside the mozc, and pass it through to the
      // connected application.
      resetAll();
      expect(service.isInputViewShown()).andReturn(false);
      service.sendDownUpKeyEvents(KeyEvent.KEYCODE_BACK);
      replayAll();

      service.sendKeyEvent(keyEvent);

      verifyAll();

      // If the subview (SymbolInputView or CursorView) is shown, close it and do not pass the event
      // to the client application.
      resetAll();
      expect(service.isInputViewShown()).andReturn(true);
      expect(viewManager.hideSubInputView()).andReturn(true);
      replayAll();

      service.sendKeyEvent(keyEvent);

      verifyAll();

      // If the main view (software keyboard) is shown, close it and do nto pass the event to the
      // client application.
      resetAll();
      expect(service.isInputViewShown()).andReturn(true);
      expect(viewManager.hideSubInputView()).andReturn(false);
      service.requestHideSelf(0);
      replayAll();

      service.sendKeyEvent(keyEvent);

      verifyAll();
    }
}

  @SmallTest
  public void testSendKeyEventEnter() {
    MozcService service = createMockBuilder(MozcService.class)
        .addMockedMethods("requestHideSelf", "sendDownUpKeyEvents", "isInputViewShown",
                          "sendDefaultEditorAction")
        .createMock();
    ViewEventListener eventListener = createNiceMock(ViewEventListener.class);
    ViewManager viewManager =
        createViewManagerMock(getInstrumentation().getTargetContext(), eventListener);
    VisibilityProxy.setField(service, "viewManager", viewManager);

    KeyEventInterface[] keyEventList = {
        // Software key event.
        KeycodeConverter.getKeyEventInterface(KeyEvent.KEYCODE_ENTER),
        // Hardware key event.
        KeycodeConverter.getKeyEventInterface(new KeyEvent(KeyEvent.ACTION_DOWN,
                                                           KeyEvent.KEYCODE_ENTER)),
    };

    for (KeyEventInterface keyEvent : keyEventList) {
      // Test for enter key.
      // If the input view is not shown, don't handle inside the mozc.
      resetAll();
      expect(service.isInputViewShown()).andReturn(false);
      service.sendDownUpKeyEvents(KeyEvent.KEYCODE_ENTER);
      replayAll();

      service.sendKeyEvent(keyEvent);

      verifyAll();

      // If the not-consumed key event is enter, fallback to default editor action.
      resetAll();
      expect(service.isInputViewShown()).andReturn(true);
      expect(service.sendDefaultEditorAction(true)).andReturn(true);
      replayAll();

      service.sendKeyEvent(keyEvent);

      verifyAll();
    }
  }

  @SmallTest
  public void testRenderInputConnection_directInputDeletionRange() {
    SessionExecutor sessionExecutor = createNiceMock(SessionExecutor.class);
    MozcService service = createInitializedService(sessionExecutor);
    SelectionTracker selectionTracker = createMock(SelectionTracker.class);
    VisibilityProxy.setField(service, "selectionTracker", selectionTracker);
    InputConnection inputConnection = createStrictMock(InputConnection.class);
    service.onCreateInputMethodInterface().restartInput(inputConnection, new EditorInfo());

    class TestData extends Parameter {
      final DeletionRange deletionRange;
      final Pair<Integer, Integer> expectedRange;

      TestData(DeletionRange deletionRange, Pair<Integer, Integer> expectedRange) {
        this.deletionRange = deletionRange;
        this.expectedRange = expectedRange;
      }
    }

    TestData [] testDataList = {
        new TestData(
            DeletionRange.newBuilder().setOffset(-3).setLength(3).build(), Pair.create(3, 0)),
        new TestData(
            DeletionRange.newBuilder().setOffset(0).setLength(3).build(), Pair.create(0, 3)),
        new TestData(
            DeletionRange.newBuilder().setOffset(-3).setLength(6).build(), Pair.create(3, 3)),
        new TestData(
            DeletionRange.newBuilder().setOffset(0).setLength(0).build(), Pair.create(0, 0)),
        new TestData(DeletionRange.newBuilder().setOffset(-3).setLength(2).build(), null),
        new TestData(DeletionRange.newBuilder().setOffset(1).setLength(2).build(), null),
    };

    for (TestData testData : testDataList) {
      resetAll();
      expect(inputConnection.beginBatchEdit()).andReturn(true);
      if (testData.expectedRange != null) {
        expect(inputConnection.deleteSurroundingText(
            testData.expectedRange.first, testData.expectedRange.second))
            .andReturn(true);
      }
      Capture<CharSequence> composingTextCapture = new Capture<CharSequence>();
      expect(inputConnection.setComposingText(capture(composingTextCapture), eq(0)))
          .andReturn(true);
      selectionTracker.onRender(testData.deletionRange, null, null);
      expect(inputConnection.endBatchEdit()).andReturn(true);
      replayAll();

      Output output = Output.newBuilder()
          .setConsumed(true)
          .setDeletionRange(testData.deletionRange)
          .build();
      service.renderInputConnection(output, null);

      verifyAll();
      assertEquals("", composingTextCapture.getValue().toString());
    }
  }

  @SmallTest
  public void testSwitchKeyboard() throws SecurityException, IllegalArgumentException {
    // Prepare the service.
    SessionExecutor sessionExecutor = createMock(SessionExecutor.class);
    MozcService service = createInitializedService(sessionExecutor);

    // Prepares the mock.
    resetAll();
    List<TouchEvent> touchEventList = Collections.singletonList(TouchEvent.getDefaultInstance());
    KeyboardSpecification newSpecification = KeyboardSpecification.QWERTY_KANA;

    sessionExecutor.updateRequest(
        MozcUtil.getRequestForKeyboard(newSpecification.getKeyboardSpecificationName(),
                                       newSpecification.getSpecialRomanjiTable(),
                                       newSpecification.getSpaceOnAlphanumeric(),
                                       newSpecification.isKanaModifierInsensitiveConversion(),
                                       newSpecification.getCrossingEdgeBehavior(),
                                       getDefaultDeviceConfiguration()),
        touchEventList);
    sessionExecutor.switchInputMode(newSpecification.getCompositionMode());

    replayAll();

    // Get the event listener and execute.
    ViewEventListener listener = service.new MozcEventListener();

    {
      // To stabilize the unittest, overwrite orientation and write back once after the invocation.
      Configuration backup = new Configuration(service.getResources().getConfiguration());
      try {
        service.getResources().getConfiguration().orientation = Configuration.ORIENTATION_PORTRAIT;
        listener.onKeyEvent(null, null, newSpecification, touchEventList);
      } finally {
        service.getResources().getConfiguration().updateFrom(backup);
        assertEquals(backup, service.getResources().getConfiguration());
      }
    }

    // Verify.
    assertEquals(
        newSpecification, VisibilityProxy.getField(service, "currentKeyboardSpecification"));
    verifyAll();
  }

  @SmallTest
  public void testPreferenceInitialization() {
    MozcService service = createService();
    SharedPreferences sharedPreferences = MozcPreferenceUtil.getSharedPreferences(
        getInstrumentation().getContext(), "TEST_INPUT_STYLE");
    sharedPreferences.edit()
        .putString("pref_portrait_keyboard_layout_key", KeyboardLayout.QWERTY.name())
        .commit();

    SessionExecutor sessionExecutor = createMock(SessionExecutor.class);
    sessionExecutor.reset(isA(SessionHandlerFactory.class), same(service));
    sessionExecutor.setLogging(anyBoolean());
    sessionExecutor.createSession();
    sessionExecutor.setImposedConfig(isA(Config.class));
    sessionExecutor.updateRequest(isA(Request.class), eq(Collections.<TouchEvent>emptyList()));
    expectLastCall().asStub();
    sessionExecutor.switchInputMode(isA(CompositionMode.class));
    expectLastCall().asStub();
    sessionExecutor.setConfig(isA(Config.class));
    replayAll();

    try {
      invokeOnCreateInternal(service, null, sharedPreferences, getDefaultDeviceConfiguration(),
                             sessionExecutor);

      verifyAll();

      // Make sure the UI is initialized expectedly.
      ViewManager viewManager = VisibilityProxy.getField(service, "viewManager");
      JapaneseSoftwareKeyboardModel model =
          VisibilityProxy.getField(viewManager, "japaneseSoftwareKeyboardModel");
      assertEquals(KeyboardLayout.QWERTY, model.getKeyboardLayout());
    } finally {
      clearSharedPreferences(sharedPreferences, service);
    }
  }

  @SmallTest
  public void testSymbolHistoryStorageImpl_getAllHistory() {
    SessionExecutor sessionExecutor = createMock(SessionExecutor.class);
    SymbolHistoryStorage storage = new SymbolHistoryStorageImpl(sessionExecutor);

    class TestData extends Parameter {
      final MajorCategory majorCategory;
      final StorageType expectedStorageType;
      final String expectedValue;

      TestData(MajorCategory majorCategory, StorageType expectedStorageType, String expectedValue) {
        this.majorCategory = majorCategory;
        this.expectedStorageType = expectedStorageType;
        this.expectedValue = expectedValue;
      }
    }

    TestData[] testDataList = {
        new TestData(MajorCategory.SYMBOL, StorageType.SYMBOL_HISTORY, "SYMBOL_HISTORY"),
        new TestData(MajorCategory.EMOTICON, StorageType.EMOTICON_HISTORY, "EMOTICON_HISTORY"),
        new TestData(MajorCategory.EMOJI, StorageType.EMOJI_HISTORY, "EMOJI_HISTORY"),
    };

    for (TestData testData : testDataList) {
      resetAll();
      expect(sessionExecutor.readAllFromStorage(testData.expectedStorageType))
          .andReturn(Collections.singletonList(ByteString.copyFromUtf8(testData.expectedValue)));
      replayAll();

      assertEquals(testData.toString(),
                   Collections.singletonList(testData.expectedValue),
                   storage.getAllHistory(testData.majorCategory));

      verifyAll();
    }
  }

  @SmallTest
  public void testSymbolHistoryStorageImpl_addHistory() {
    SessionExecutor sessionExecutor = createMock(SessionExecutor.class);
    SymbolHistoryStorage storage = new SymbolHistoryStorageImpl(sessionExecutor);

    class TestData extends Parameter {
      final MajorCategory majorCategory;
      final StorageType expectedStorageType;
      final String expectedValue;

      TestData(MajorCategory majorCategory, StorageType expectedStorageType, String expectedValue) {
        this.majorCategory = majorCategory;
        this.expectedStorageType = expectedStorageType;
        this.expectedValue = expectedValue;
      }
    }

    TestData[] testDataList = {
        new TestData(MajorCategory.SYMBOL, StorageType.SYMBOL_HISTORY, "SYMBOL_HISTORY"),
        new TestData(MajorCategory.EMOTICON, StorageType.EMOTICON_HISTORY, "EMOTICON_HISTORY"),
        new TestData(MajorCategory.EMOJI, StorageType.EMOJI_HISTORY, "EMOJI_HISTORY"),
    };

    for (TestData testData : testDataList) {
      resetAll();
      sessionExecutor.insertToStorage(
          testData.expectedStorageType,
          testData.expectedValue,
          Collections.singletonList(ByteString.copyFromUtf8(testData.expectedValue)));
      replayAll();

      storage.addHistory(testData.majorCategory, testData.expectedValue);

      verifyAll();
    }
  }

  @SmallTest
  public void testMozcEventListener_onConversionCandidateSelected() {
    SessionExecutor sessionExecutor = createMock(SessionExecutor.class);
    MozcService service = createInitializedService(sessionExecutor);

    // Set feedback listener and force to enable feedbacks.
    resetAll();
    FeedbackListener feedbackListener = createMock(FeedbackListener.class);
    FeedbackManager feedbackManager = new FeedbackManager(feedbackListener);
    feedbackManager.setHapticFeedbackEnabled(true);
    feedbackManager.setHapticFeedbackDuration(100);
    feedbackManager.setSoundFeedbackEnabled(true);
    feedbackManager.setSoundFeedbackVolume(0.5f);
    VisibilityProxy.setField(service, "feedbackManager", feedbackManager);

    // Expectation.
    feedbackListener.onSound(AudioManager.FX_KEY_CLICK, 0.5f);
    feedbackListener.onVibrate(100L);

    sessionExecutor.submitCandidate(eq(0), isA(EvaluationCallback.class));
    replayAll();

    // Invoke onConversionCandidateSelected.
    ViewManager viewManager = VisibilityProxy.getField(service, "viewManager");
    ViewEventListener viewEventListener = VisibilityProxy.getField(viewManager, "eventListener");
    viewEventListener.onConversionCandidateSelected(0);

    verifyAll();
  }

  @SmallTest
  public void testMozcEventListener_onSymbolCandidateSelected() {
    SessionExecutor sessionExecutor = createMock(SessionExecutor.class);
    MozcService service = createInitializedService(sessionExecutor);
    InputConnection inputConnection = createStrictMock(InputConnection.class);
    service.onCreateInputMethodInterface().restartInput(inputConnection, new EditorInfo());

    // Set feedback listener and force to enable feedbacks.
    resetAll();
    FeedbackListener feedbackListener = createMock(FeedbackListener.class);
    FeedbackManager feedbackManager = new FeedbackManager(feedbackListener);
    feedbackManager.setHapticFeedbackEnabled(true);
    feedbackManager.setHapticFeedbackDuration(100);
    feedbackManager.setSoundFeedbackEnabled(true);
    feedbackManager.setSoundFeedbackVolume(0.5f);
    VisibilityProxy.setField(service, "feedbackManager", feedbackManager);

    // Expectation.
    expect(inputConnection.beginBatchEdit()).andReturn(true);
    expect(inputConnection.commitText("(^_^)", MozcUtil.CURSOR_POSITION_TAIL))
        .andReturn(true);
    expect(inputConnection.endBatchEdit()).andReturn(true);

    feedbackListener.onSound(AudioManager.FX_KEY_CLICK, 0.5f);
    feedbackListener.onVibrate(100);

    SymbolHistoryStorage historyStorage = createMock(SymbolHistoryStorage.class);
    historyStorage.addHistory(MajorCategory.EMOTICON, "(^_^)");
    VisibilityProxy.setField(service, "symbolHistoryStorage", historyStorage);
    replayAll();

    // Invoke onConversionCandidateSelected.
    ViewManager viewManager = VisibilityProxy.getField(service, "viewManager");
    ViewEventListener viewEventListener = VisibilityProxy.getField(viewManager, "eventListener");
    viewEventListener.onSymbolCandidateSelected(MajorCategory.EMOTICON, "(^_^)");

    verifyAll();
  }

  @SmallTest
  public void testMozcEventListener_onClickHardwareKeyboardCompositionModeButton() {
    MozcService service = createService();
    ViewEventListener viewEventListener = service.new MozcEventListener();
    ViewManager viewManager =
        createViewManagerMock(getInstrumentation().getTargetContext(), viewEventListener);
    invokeOnCreateInternal(service, viewManager, null, getDefaultDeviceConfiguration(),
                           createNiceMock(SessionExecutor.class));
    HardwareKeyboard hardwareKeyboard = createMock(HardwareKeyboard.class);
    VisibilityProxy.setField(service, "hardwareKeyboard", hardwareKeyboard);

    resetAll();

    hardwareKeyboard.setCompositionMode(CompositionSwitchMode.TOGGLE);
    expect(hardwareKeyboard.getCompositionMode()).andReturn(CompositionMode.HALF_ASCII);
    viewManager.setHardwareKeyboardCompositionMode(CompositionMode.HALF_ASCII);
    expect(hardwareKeyboard.getKeyboardSpecification())
        .andReturn(KeyboardSpecification.HARDWARE_QWERTY_ALPHABET);

    replayAll();

    viewEventListener.onClickHardwareKeyboardCompositionModeButton();

    verifyAll();
  }

  @SmallTest
  public void testOnUndo() {
    SessionExecutor sessionExecutor = createMock(SessionExecutor.class);
    MozcService service = createInitializedService(sessionExecutor);

    // Get tested object.
    ViewManager viewManager = VisibilityProxy.getField(service, "viewManager");
    ViewEventListener listener = VisibilityProxy.getField(viewManager, "eventListener");

    resetAll();
    List<TouchEvent> touchEventList = Collections.singletonList(TouchEvent.getDefaultInstance());
    sessionExecutor.undoOrRewind(eq(touchEventList), isA(EvaluationCallback.class));
    replayAll();

    listener.onUndo(touchEventList);

    verifyAll();
  }

  @SmallTest
  public void testOnSubmitPreedit() {
    SessionExecutor sessionExecutor = createMock(SessionExecutor.class);
    MozcService service = createInitializedService(sessionExecutor);

    // Get tested object.
    ViewManager viewManager = VisibilityProxy.getField(service, "viewManager");
    ViewEventListener listener = VisibilityProxy.getField(viewManager, "eventListener");

    resetAll();
    sessionExecutor.submit(isA(EvaluationCallback.class));
    replayAll();

    // Call the method.
    listener.onSubmitPreedit();

    verifyAll();
  }

  @SmallTest
  public void testOnExpandSuggestion() {
    SessionExecutor sessionExecutor = createMock(SessionExecutor.class);
    MozcService service = createInitializedService(sessionExecutor);

    // Get tested object.
    ViewManager viewManager = VisibilityProxy.getField(service, "viewManager");
    ViewEventListener listener = VisibilityProxy.getField(viewManager, "eventListener");

    resetAll();
    sessionExecutor.expandSuggestion(isA(EvaluationCallback.class));
    replayAll();

    listener.onExpandSuggestion();

    verifyAll();
  }

  @SmallTest
  public void testPropagateClientSidePreference() throws InvocationTargetException {
    SessionExecutor sessionExecutor = createNiceMock(SessionExecutor.class);
    replayAll();
    MozcService service = createInitializedService(sessionExecutor);

    ViewManager viewManager = VisibilityProxy.getField(service, "viewManager");

    // Initialization (all fields are propagated).
    {
      ClientSidePreference clientSidePreference =
          new ClientSidePreference(
              true, 100, true, 100, false, KeyboardLayout.QWERTY, InputStyle.TOGGLE, false, false,
              0, EmojiProviderType.DOCOMO, HardwareKeyMap.JAPANESE109A, SkinType.ORANGE_LIGHTGRAY,
              LayoutAdjustment.FILL, 100);
      VisibilityProxy.invokeByName(service, "propagateClientSidePreference", clientSidePreference);

      // Check all the fields are propagated.
      FeedbackManager feedbackManager = VisibilityProxy.getField(service, "feedbackManager");
      assertTrue(feedbackManager.isHapticFeedbackEnabled());
      assertEquals(feedbackManager.getHapticFeedbackDuration(), 100L);
      assertTrue(feedbackManager.isSoundFeedbackEnabled());
      assertEquals(feedbackManager.getSoundFeedbackVolume(), 0.2f);
      assertFalse(VisibilityProxy.<Boolean>getField(viewManager, "popupEnabled"));
      JapaneseSoftwareKeyboardModel model =
          VisibilityProxy.getField(viewManager, "japaneseSoftwareKeyboardModel");
      assertEquals(KeyboardLayout.QWERTY, model.getKeyboardLayout());
      assertEquals(InputStyle.TOGGLE, model.getInputStyle());
      assertFalse(model.isQwertyLayoutForAlphabet());
      assertFalse(VisibilityProxy.<Boolean>getField(viewManager, "fullscreenMode"));
      assertFalse(service.onEvaluateFullscreenMode());
      assertEquals(EmojiProviderType.DOCOMO,
                   VisibilityProxy.getField(viewManager, "emojiProviderType"));
      assertEquals(HardwareKeyMap.JAPANESE109A,
                   HardwareKeyboard.class.cast(
                       VisibilityProxy.getField(service, "hardwareKeyboard")).getHardwareKeyMap());
      assertEquals(LayoutAdjustment.FILL,
                   VisibilityProxy.getField(viewManager, "layoutAdjustment"));
      assertEquals(100,
                   VisibilityProxy.getField(viewManager, "keyboardHeightRatio"));
    }

    // Test for partial update of each attribute.
    class TestData extends Parameter {
      final ClientSidePreference clientSidePreference;
      final boolean expectHapticFeedback;
      final long expectHapticFeedbackDuration;
      final boolean expectSoundFeedback;
      final float expectSoundFeedbackVolume;
      final boolean expectPopupFeedback;
      final KeyboardLayout expectKeyboardLayout;
      final InputStyle expectInputStyle;
      final boolean expectQwertyLayoutForAlphabet;
      final boolean expectFullscreenMode;
      final int expectFlickSensitivity;
      final EmojiProviderType expectEmojiProviderType;
      final HardwareKeyMap expectHardwareKeyMap;
      final SkinType expectSkinType;
      final LayoutAdjustment expectLayoutAdjustment;
      final int expectKeyboardHeight;

      TestData(ClientSidePreference clientSidePreference,
               boolean expectHapticFeedback, long expectHapticFeedbackDuration,
               boolean expectSoundFeedback, float expectSoundFeedbackVolume,
               boolean expectPopupFeedback,
               KeyboardLayout expectKeyboardLayout,
               InputStyle expectInputStyle,
               boolean expectQwertyLayoutForAlphabet,
               boolean expectFullscreenMode,
               int expectFlickSensitivity,
               EmojiProviderType expectEmojiProviderType,
               HardwareKeyMap expectHardwareKeyMap,
               SkinType expectSkinType,
               LayoutAdjustment expectLayoutAdjustment,
               int expectKeyboardHeight) {
        this.clientSidePreference = clientSidePreference;
        this.expectHapticFeedback = expectHapticFeedback;
        this.expectHapticFeedbackDuration = expectHapticFeedbackDuration;
        this.expectSoundFeedback = expectSoundFeedback;
        this.expectSoundFeedbackVolume = expectSoundFeedbackVolume;
        this.expectPopupFeedback = expectPopupFeedback;
        this.expectKeyboardLayout = expectKeyboardLayout;
        this.expectInputStyle = expectInputStyle;
        this.expectQwertyLayoutForAlphabet = expectQwertyLayoutForAlphabet;
        this.expectFullscreenMode = expectFullscreenMode;
        this.expectFlickSensitivity = expectFlickSensitivity;
        this.expectEmojiProviderType = expectEmojiProviderType;
        this.expectHardwareKeyMap = expectHardwareKeyMap;
        this.expectSkinType = expectSkinType;
        this.expectLayoutAdjustment = expectLayoutAdjustment;
        this.expectKeyboardHeight = expectKeyboardHeight;
      }
    }
    // Note: following test case should tested in this order, as former test cases should affect to
    //       latter ones.
    TestData[] testDataList = {
        new TestData(
            new ClientSidePreference(
                false, 10, true, 10, true, KeyboardLayout.QWERTY, InputStyle.TOGGLE, false, false,
                0, EmojiProviderType.DOCOMO, HardwareKeyMap.DEFAULT, SkinType.ORANGE_LIGHTGRAY,
                LayoutAdjustment.FILL, 100),
            false,
            10,
            true,
            0.02f,
            true,
            KeyboardLayout.QWERTY,
            InputStyle.TOGGLE,
            false,
            false,
            0,
            EmojiProviderType.DOCOMO,
            HardwareKeyMap.DEFAULT,
            SkinType.ORANGE_LIGHTGRAY,
            LayoutAdjustment.FILL,
            100),
        new TestData(
            new ClientSidePreference(
                false, 20, true, 20, false, KeyboardLayout.QWERTY, InputStyle.TOGGLE, false, false,
                -1, EmojiProviderType.DOCOMO, HardwareKeyMap.DEFAULT, SkinType.ORANGE_LIGHTGRAY,
                LayoutAdjustment.FILL, 100),
            false,
            20,
            true,
            0.04f,
            false,
            KeyboardLayout.QWERTY,
            InputStyle.TOGGLE,
            false,
            false,
            -1,
            EmojiProviderType.DOCOMO,
            HardwareKeyMap.DEFAULT,
            SkinType.ORANGE_LIGHTGRAY,
            LayoutAdjustment.FILL,
            100),
        new TestData(
            new ClientSidePreference(
                false, 30, false, 30, true, KeyboardLayout.QWERTY, InputStyle.TOGGLE, false, false,
                2, EmojiProviderType.KDDI, HardwareKeyMap.DEFAULT, SkinType.ORANGE_LIGHTGRAY,
                LayoutAdjustment.FILL, 100),
            false,
            30,
            false,
            0.06f,
            true,
            KeyboardLayout.QWERTY,
            InputStyle.TOGGLE,
            false,
            false,
            2,
            EmojiProviderType.KDDI,
            HardwareKeyMap.DEFAULT,
            SkinType.ORANGE_LIGHTGRAY,
            LayoutAdjustment.FILL,
            100),
        new TestData(
            new ClientSidePreference(
                false, 40, false, 40, true, KeyboardLayout.TWELVE_KEYS, InputStyle.TOGGLE,
                false, false, -3, EmojiProviderType.SOFTBANK, HardwareKeyMap.DEFAULT,
                SkinType.ORANGE_LIGHTGRAY, LayoutAdjustment.FILL, 100),
            false,
            40,
            false,
            0.08f,
            true,
            KeyboardLayout.TWELVE_KEYS,
            InputStyle.TOGGLE,
            false,
            false,
            -3,
            EmojiProviderType.SOFTBANK,
            HardwareKeyMap.DEFAULT,
            SkinType.ORANGE_LIGHTGRAY,
            LayoutAdjustment.FILL,
            100),
        new TestData(
            new ClientSidePreference(
                false, 50, false, 50, true, KeyboardLayout.TWELVE_KEYS, InputStyle.FLICK,
                false, false, 4, EmojiProviderType.DOCOMO, HardwareKeyMap.DEFAULT,
                SkinType.ORANGE_LIGHTGRAY, LayoutAdjustment.FILL, 100),
            false,
            50,
            false,
            0.1f,
            true,
            KeyboardLayout.TWELVE_KEYS,
            InputStyle.FLICK,
            false,
            false,
            4,
            EmojiProviderType.DOCOMO,
            HardwareKeyMap.DEFAULT,
            SkinType.ORANGE_LIGHTGRAY,
            LayoutAdjustment.FILL,
            100),
        new TestData(
            new ClientSidePreference(
                false, 60, false, 60, true, KeyboardLayout.TWELVE_KEYS, InputStyle.FLICK,
                true, false, -5, EmojiProviderType.KDDI, HardwareKeyMap.DEFAULT,
                SkinType.ORANGE_LIGHTGRAY, LayoutAdjustment.FILL, 100),
            false,
            60,
            false,
            0.12f,
            true,
            KeyboardLayout.TWELVE_KEYS,
            InputStyle.FLICK,
            true,
            false,
            -5,
            EmojiProviderType.KDDI,
            HardwareKeyMap.DEFAULT,
            SkinType.ORANGE_LIGHTGRAY,
            LayoutAdjustment.FILL,
            100),
        new TestData(
            new ClientSidePreference(
                false, 70, false, 70, true, KeyboardLayout.TWELVE_KEYS, InputStyle.FLICK,
                true, true, 6, EmojiProviderType.SOFTBANK, HardwareKeyMap.DEFAULT,
                SkinType.ORANGE_LIGHTGRAY, LayoutAdjustment.FILL, 100),
            false,
            70,
            false,
            0.14f,
            true,
            KeyboardLayout.TWELVE_KEYS,
            InputStyle.FLICK,
            true,
            true,
            6,
            EmojiProviderType.SOFTBANK,
            HardwareKeyMap.DEFAULT,
            SkinType.ORANGE_LIGHTGRAY,
            LayoutAdjustment.FILL, 100),
        new TestData(
            new ClientSidePreference(
                false, 80, false, 80, true, KeyboardLayout.TWELVE_KEYS, InputStyle.FLICK,
                true, true, -7, EmojiProviderType.SOFTBANK, HardwareKeyMap.JAPANESE109A,
                SkinType.ORANGE_LIGHTGRAY, LayoutAdjustment.FILL, 100),
            false,
            80,
            false,
            0.16f,
            true,
            KeyboardLayout.TWELVE_KEYS,
            InputStyle.FLICK,
            true,
            true,
            -7,
            EmojiProviderType.SOFTBANK,
            HardwareKeyMap.JAPANESE109A,
            SkinType.ORANGE_LIGHTGRAY,
            LayoutAdjustment.FILL,
            100),
        new TestData(
            new ClientSidePreference(
                false, 90, false, 90, true, KeyboardLayout.TWELVE_KEYS, InputStyle.FLICK,
                true, true, 8, EmojiProviderType.SOFTBANK, HardwareKeyMap.TWELVEKEY,
                SkinType.ORANGE_LIGHTGRAY, LayoutAdjustment.RIGHT, 100),
            false,
            90,
            false,
            0.18f,
            true,
            KeyboardLayout.TWELVE_KEYS,
            InputStyle.FLICK,
            true,
            true,
            8,
            EmojiProviderType.SOFTBANK,
            HardwareKeyMap.TWELVEKEY,
            SkinType.ORANGE_LIGHTGRAY,
            LayoutAdjustment.RIGHT,
            100),
        new TestData(
            new ClientSidePreference(
                false, 100, false, 100, true, KeyboardLayout.TWELVE_KEYS, InputStyle.FLICK,
                true, true, 8, EmojiProviderType.SOFTBANK, HardwareKeyMap.TWELVEKEY,
                SkinType.ORANGE_LIGHTGRAY, LayoutAdjustment.RIGHT, 120),
            false,
            100,
            false,
            0.2f,
            true,
            KeyboardLayout.TWELVE_KEYS,
            InputStyle.FLICK,
            true,
            true,
            8,
            EmojiProviderType.SOFTBANK,
            HardwareKeyMap.TWELVEKEY,
            SkinType.ORANGE_LIGHTGRAY,
            LayoutAdjustment.RIGHT,
            120),
    };

    for (TestData testData : testDataList) {
      VisibilityProxy.invokeByName(
          service, "propagateClientSidePreference", testData.clientSidePreference);
      FeedbackManager feedbackManager = VisibilityProxy.getField(service, "feedbackManager");

      // Make sure all configurations are actually applied.
      assertEquals(testData.toString(),
                   testData.expectHapticFeedback, feedbackManager.isHapticFeedbackEnabled());
      assertEquals(testData.toString(),
                   testData.expectHapticFeedbackDuration,
                   feedbackManager.getHapticFeedbackDuration());
      assertEquals(testData.toString(),
                   testData.expectSoundFeedback, feedbackManager.isSoundFeedbackEnabled());
      // Check with an epsilon.
      assertEquals(testData.toString(),
                   testData.expectSoundFeedbackVolume, feedbackManager.getSoundFeedbackVolume(),
                   0.0001f);
      assertEquals(testData.toString(),
                   testData.expectPopupFeedback,
                   VisibilityProxy.getField(viewManager, "popupEnabled"));
      JapaneseSoftwareKeyboardModel model =
          VisibilityProxy.getField(viewManager, "japaneseSoftwareKeyboardModel");
      assertEquals(testData.toString(), testData.expectKeyboardLayout, model.getKeyboardLayout());
      assertEquals(testData.toString(), testData.expectInputStyle, model.getInputStyle());
      assertEquals(testData.toString(),
                   testData.expectQwertyLayoutForAlphabet, model.isQwertyLayoutForAlphabet());
      assertEquals(testData.toString(),
                   testData.expectFullscreenMode,
                   VisibilityProxy.getField(viewManager, "fullscreenMode"));
      assertEquals(testData.toString(),
                   testData.expectFullscreenMode, service.onEvaluateFullscreenMode());
      assertEquals(testData.toString(),
                   testData.expectFlickSensitivity,
                   VisibilityProxy.getField(viewManager, "flickSensitivity"));
      assertEquals(testData.toString(),
                   testData.expectEmojiProviderType,
                   VisibilityProxy.getField(viewManager, "emojiProviderType"));
      assertEquals(testData.toString(),
                   testData.expectHardwareKeyMap,
                   HardwareKeyboard.class.cast(
                       VisibilityProxy.getField(service, "hardwareKeyboard")).getHardwareKeyMap());
      assertEquals(testData.toString(),
                   testData.expectSkinType,
                   VisibilityProxy.getField(viewManager, "skinType"));
      assertEquals(testData.toString(),
                   testData.expectLayoutAdjustment,
                   VisibilityProxy.getField(viewManager, "layoutAdjustment"));
      assertEquals(testData.toString(),
                   testData.expectKeyboardHeight,
                   VisibilityProxy.getField(viewManager, "keyboardHeightRatio"));
    }
  }

  /**
   * We cannot check whether a sound is played or not.
   * Just a smoke test.
   */
  @SmallTest
  public void testOnSound() {
    SessionExecutor sessionExecutor = createNiceMock(SessionExecutor.class);
    replayAll();
    MozcService service = createInitializedService(sessionExecutor);
    FeedbackListener listener = VisibilityProxy.getField(
        VisibilityProxy.getField(service, "feedbackManager"), "feedbackListener");
    listener.onSound(AudioManager.FX_KEYPRESS_STANDARD, 0.1f);
    listener.onSound(FeedbackEvent.NO_SOUND, 0.1f);
  }

  @SmallTest
  public void testOnConfigurationChanged() throws SecurityException, IllegalArgumentException {
    SessionExecutor sessionExecutor = createMock(SessionExecutor.class);
    SelectionTracker selectionTracker = createMock(SelectionTracker.class);
    ViewManager viewManager = createMockBuilder(ViewManager.class)
        .withConstructor(Context.class, ViewEventListener.class,
                         SymbolHistoryStorage.class, MenuDialogListener.class)
        .withArgs(getInstrumentation().getTargetContext(),
                  createNiceMock(ViewEventListener.class),
                  null,
                  null)
        .addMockedMethods("isNarrowMode", "hideSubInputView", "setNarrowMode",
                          "onConfigurationChanged")
        .createMock();
    MozcService service = createService();
    invokeOnCreateInternal(service, viewManager, null, getDefaultDeviceConfiguration(),
                           sessionExecutor);
    VisibilityProxy.setField(service, "selectionTracker", selectionTracker);
    VisibilityProxy.setField(service, "inputBound", true);

    Config expectedConfig = Config.newBuilder()
        .setSessionKeymap(SessionKeymap.MOBILE)
        .setSelectionShortcut(SelectionShortcut.NO_SHORTCUT)
        .setUseEmojiConversion(true)
        .build();

    Configuration deviceConfig = new Configuration();
    deviceConfig.keyboard = Configuration.KEYBOARD_NOKEYS;
    deviceConfig.keyboardHidden = Configuration.KEYBOARDHIDDEN_YES;
    deviceConfig.hardKeyboardHidden = Configuration.KEYBOARDHIDDEN_UNDEFINED;

    InputConnection inputConnection = createMock(InputConnection.class);
    // restartInput() can be used to install our InputConnection safely.
    service.onCreateInputMethodInterface().restartInput(inputConnection, new EditorInfo());

    resetAll();
    expect(inputConnection.finishComposingText()).andReturn(true);

    expect(selectionTracker.getLastSelectionStart()).andReturn(0);
    expect(selectionTracker.getLastSelectionEnd()).andReturn(0);
    selectionTracker.onConfigurationChanged(same(deviceConfig));

    sessionExecutor.resetContext();
    sessionExecutor.setImposedConfig(expectedConfig);
    sessionExecutor.switchInputMode(isA(CompositionMode.class));
    expectLastCall().asStub();
    sessionExecutor.updateRequest(isA(Request.class), eq(Collections.<TouchEvent>emptyList()));
    expectLastCall().asStub();

    viewManager.onConfigurationChanged(deviceConfig);

    replayAll();

    // Test on no-hardware-keyboard configuration.
    service.onConfigurationChangedInternal(deviceConfig);

    verifyAll();

    Handler configurationChangedHandler =
        VisibilityProxy.getField(service, "configurationChangedHandler");
    assertTrue(configurationChangedHandler.hasMessages(0));
    // Clean up the pending message.
    configurationChangedHandler.removeMessages(0);
  }

  @SmallTest
  public void testOnUpdateSelection_exactMatch() {
    // Prepare the service.
    SessionExecutor sessionExecutor = createMock(SessionExecutor.class);
    SelectionTracker selectionTracker = createMock(SelectionTracker.class);
    MozcService service = createInitializedService(sessionExecutor);
    VisibilityProxy.setField(service, "selectionTracker", selectionTracker);

    resetAll();
    // No events for SessionExecutor are expected.
    expect(selectionTracker.onUpdateSelection(0, 0, 1, 1, 0, 1))
        .andReturn(SelectionTracker.DO_NOTHING);
    replayAll();

    service.onUpdateSelection(0, 0, 1, 1, 0, 1);

    verifyAll();
  }

  @SmallTest
  public void testOnUpdateSelection_resetContext() {
    // Prepare the service.
    MozcService service = createMockBuilder(MozcService.class)
        .addMockedMethods("isInputViewShown")
        .createMock();
    SessionExecutor sessionExecutor = createMock(SessionExecutor.class);
    initializeMozcService(service, sessionExecutor);
    VisibilityProxy.setField(service, "inputBound", true);

    InputConnection inputConnection = createMock(InputConnection.class);
    service.onCreateInputMethodInterface().restartInput(inputConnection, new EditorInfo());

    SelectionTracker selectionTracker = createMock(SelectionTracker.class);
    VisibilityProxy.setField(service, "selectionTracker", selectionTracker);

    resetAll();
    expect(selectionTracker.onUpdateSelection(0, 0, 1, 1, 0, 1))
        .andReturn(SelectionTracker.RESET_CONTEXT);

    // If the keyboard view is shown, reset the composing text here.
    expect(service.isInputViewShown()).andReturn(true);
    expect(inputConnection.finishComposingText()).andReturn(true);

    // The session should be reset.
    sessionExecutor.resetContext();
    replayAll();

    service.onUpdateSelectionInternal(0, 0, 1, 1, 0, 1);

    verifyAll();
  }

  @SmallTest
  public void testOnUpdateSelection_resetContextInivisibleKeyboard() {
    // Prepare the service.
    MozcService service = createMockBuilder(MozcService.class)
        .addMockedMethods("isInputViewShown")
        .createMock();
    SessionExecutor sessionExecutor = createMock(SessionExecutor.class);
    initializeMozcService(service, sessionExecutor);

    InputConnection inputConnection = createMock(InputConnection.class);
    service.onCreateInputMethodInterface().restartInput(inputConnection, new EditorInfo());

    SelectionTracker selectionTracker = createMock(SelectionTracker.class);
    VisibilityProxy.setField(service, "selectionTracker", selectionTracker);

    resetAll();
    expect(selectionTracker.onUpdateSelection(0, 0, 1, 1, 0, 1))
        .andReturn(SelectionTracker.RESET_CONTEXT);

    // If the keyboard view isn't shown, do NOT reset the composing text.
    expect(service.isInputViewShown()).andReturn(false);

    // The session should be reset.
    sessionExecutor.resetContext();
    replayAll();

    service.onUpdateSelectionInternal(0, 0, 1, 1, 0, 1);

    verifyAll();
  }

  @SmallTest
  public void testOnUpdateSelection_moveCursor() {
    // Prepare the service.
    SessionExecutor sessionExecutor = createMock(SessionExecutor.class);
    SelectionTracker selectionTracker = createMock(SelectionTracker.class);
    MozcService service = createInitializedService(sessionExecutor);
    VisibilityProxy.setField(service, "selectionTracker", selectionTracker);

    resetAll();
    expect(selectionTracker.onUpdateSelection(0, 0, 1, 1, 0, 1))
        .andReturn(5);
    // Send moveCursor event as selectionTracker says.
    sessionExecutor.moveCursor(eq(5), isA(EvaluationCallback.class));
    replayAll();

    service.onUpdateSelection(0, 0, 1, 1, 0, 1);

    verifyAll();
  }

  @SmallTest
  public void testOnHardwareKeyboardCompositionModeChange() {
    MozcService service = initializeService(createMockBuilder(MozcService.class)
        .addMockedMethod("isInputViewShown").createMock());
    Context context = getInstrumentation().getTargetContext();
    ViewManager viewManager = createMockBuilder(ViewManager.class)
        .withConstructor(Context.class, ViewEventListener.class,
                         SymbolHistoryStorage.class, MenuDialogListener.class)
        .withArgs(context, service.new MozcEventListener(), null, null)
        .addMockedMethod("setHardwareKeyboardCompositionMode")
        .createMock();
    invokeOnCreateInternal(service, viewManager, null, getDefaultDeviceConfiguration(),
                           createNiceMock(SessionExecutor.class));
    MozcView mozcView = viewManager.createMozcView(context);
    HardwareKeyboard hardwareKeyboard = createMockBuilder(HardwareKeyboard.class)
        .addMockedMethods("setCompositionMode", "getCompositionMode").createMock();
    VisibilityProxy.setField(service, "hardwareKeyboard", hardwareKeyboard);

    resetAll();
    expect(service.isInputViewShown()).andStubReturn(true);

    hardwareKeyboard.setCompositionMode(CompositionSwitchMode.TOGGLE);
    expect(hardwareKeyboard.getCompositionMode()).andReturn(CompositionMode.HIRAGANA);
    viewManager.setHardwareKeyboardCompositionMode(CompositionMode.HIRAGANA);

    replayAll();

    mozcView.getHardwareCompositionButton().performClick();

    verifyAll();
  }

  @SmallTest
  public void testMaybeSetNarrowMode() {
    ViewManager viewManager = createViewManagerMock(
        getInstrumentation().getTargetContext(),
        createNiceMock(ViewEventListener.class));
    MozcService service = createService();
    invokeOnCreateInternal(service, viewManager, null, getDefaultDeviceConfiguration(),
                           createNiceMock(SessionExecutor.class));

    Configuration configuration = new Configuration();

    resetAll();

    expect(viewManager.isNarrowMode()).andReturn(false);
    expect(viewManager.hideSubInputView()).andReturn(true);
    viewManager.setNarrowMode(true);

    replayAll();

    configuration.hardKeyboardHidden = Configuration.HARDKEYBOARDHIDDEN_NO;
    service.maybeSetNarrowMode(configuration);

    verifyAll();

    resetAll();

    expect(viewManager.isNarrowMode()).andReturn(true);
    viewManager.setNarrowMode(false);

    replayAll();

    configuration.hardKeyboardHidden = Configuration.HARDKEYBOARDHIDDEN_YES;
    service.maybeSetNarrowMode(configuration);

    verifyAll();

    resetAll();

    replayAll();

    configuration.hardKeyboardHidden = Configuration.HARDKEYBOARDHIDDEN_UNDEFINED;
    service.maybeSetNarrowMode(configuration);

    verifyAll();
  }
}
