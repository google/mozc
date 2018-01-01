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
import static org.mozc.android.inputmethod.japanese.testing.MozcMatcher.sameOptional;
import static org.easymock.EasyMock.anyBoolean;
import static org.easymock.EasyMock.anyObject;
import static org.easymock.EasyMock.capture;
import static org.easymock.EasyMock.eq;
import static org.easymock.EasyMock.expect;
import static org.easymock.EasyMock.expectLastCall;
import static org.easymock.EasyMock.isA;
import static org.easymock.EasyMock.same;

import org.mozc.android.inputmethod.japanese.DependencyFactory.Dependency;
import org.mozc.android.inputmethod.japanese.FeedbackManager.FeedbackEvent;
import org.mozc.android.inputmethod.japanese.FeedbackManager.FeedbackListener;
import org.mozc.android.inputmethod.japanese.KeycodeConverter.KeyEventInterface;
import org.mozc.android.inputmethod.japanese.MozcBaseService.SymbolHistoryStorageImpl;
import org.mozc.android.inputmethod.japanese.ViewManagerInterface.LayoutAdjustment;
import org.mozc.android.inputmethod.japanese.emoji.EmojiProviderType;
import org.mozc.android.inputmethod.japanese.hardwarekeyboard.HardwareKeyboard.CompositionSwitchMode;
import org.mozc.android.inputmethod.japanese.keyboard.Keyboard.KeyboardSpecification;
import org.mozc.android.inputmethod.japanese.model.JapaneseSoftwareKeyboardModel;
import org.mozc.android.inputmethod.japanese.model.SelectionTracker;
import org.mozc.android.inputmethod.japanese.model.SymbolCandidateStorage.SymbolHistoryStorage;
import org.mozc.android.inputmethod.japanese.model.SymbolMajorCategory;
import org.mozc.android.inputmethod.japanese.preference.ClientSidePreference;
import org.mozc.android.inputmethod.japanese.preference.ClientSidePreference.HardwareKeyMap;
import org.mozc.android.inputmethod.japanese.preference.ClientSidePreference.InputStyle;
import org.mozc.android.inputmethod.japanese.preference.ClientSidePreference.KeyboardLayout;
import org.mozc.android.inputmethod.japanese.preference.PreferenceUtil;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateList;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.Category;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Command;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.CompositionMode;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Context.InputFieldType;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.DeletionRange;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.GenericStorageEntry.StorageType;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.TouchEvent;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Output;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Preedit;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Preedit.Segment;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Preedit.Segment.Annotation;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Request;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Result;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Result.ResultType;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.SessionCommand;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoConfig.Config;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoConfig.Config.SelectionShortcut;
import org.mozc.android.inputmethod.japanese.session.SessionExecutor;
import org.mozc.android.inputmethod.japanese.session.SessionExecutor.EvaluationCallback;
import org.mozc.android.inputmethod.japanese.session.SessionHandlerFactory;
import org.mozc.android.inputmethod.japanese.testing.ApiLevel;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;
import org.mozc.android.inputmethod.japanese.testing.Parameter;
import org.mozc.android.inputmethod.japanese.ui.MenuDialog.MenuDialogListener;
import org.mozc.android.inputmethod.japanese.util.ImeSwitcherFactory.ImeSwitcher;
import org.mozc.android.inputmethod.japanese.view.Skin;
import org.mozc.android.inputmethod.japanese.view.SkinType;
import com.google.common.base.Optional;
import com.google.protobuf.ByteString;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.media.AudioManager;
import android.os.Build;
import android.os.Handler;
import android.preference.PreferenceManager;
import android.test.UiThreadTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.text.InputType;
import android.text.SpannableStringBuilder;
import android.util.Pair;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputConnectionWrapper;

import org.easymock.Capture;

import java.util.ArrayList;
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
public class MozcBaseServiceTest extends InstrumentationTestCaseWithMock {

  /**
   * Store registered listeners to unregister them on {@code clearSharedPreferences}.
   */
  private static final List<OnSharedPreferenceChangeListener> sharedPreferenceChangeListeners =
      new ArrayList<OnSharedPreferenceChangeListener>(1);

  private static class AlwaysShownMozcService extends MozcBaseService {

    @Override
    public boolean isInputViewShown() {
      return true;
    }
  }

  @Override
  protected void setUp() throws Exception {
    super.setUp();
    DependencyFactory.setDependency(Optional.<Dependency>absent());
    clearSharedPreferences();
  }

  @Override
  protected void tearDown() throws Exception {
    clearSharedPreferences();
    DependencyFactory.setDependency(Optional.<Dependency>absent());
    super.tearDown();
  }

  private ViewManager createViewManagerMock(Context context, ViewEventListener viewEventListener) {
    return createMockBuilder(ViewManager.class)
        .withConstructor(Context.class, ViewEventListener.class, SymbolHistoryStorage.class,
                         ImeSwitcher.class, MenuDialogListener.class)
        .withArgs(context, viewEventListener, createNiceMock(SymbolHistoryStorage.class),
                  createNiceMock(ImeSwitcher.class), createNiceMock(MenuDialogListener.class))
        .createMock();
  }

  // Consider using createInitializedService() instead of this method.
  private MozcBaseService createService() {
    return initializeService(new MozcBaseService());
  }

  private MozcBaseService initializeService(MozcBaseService service) {
    service.sendSyncDataCommandHandler = new Handler();
    service.attachBaseContext(getInstrumentation().getTargetContext());
    return service;
  }

  private MozcBaseService createInitializedService(SessionExecutor sessionExecutor) {
    MozcBaseService service = createService();
    invokeOnCreateInternal(
        service, null, getSharedPreferences(), getDefaultDeviceConfiguration(), sessionExecutor);
    return service;
  }

  private MozcBaseService initializeMozcService(MozcBaseService service,
                                                SessionExecutor sessionExecutor) {
    invokeOnCreateInternal(initializeService(service), null, getSharedPreferences(),
                           getDefaultDeviceConfiguration(), sessionExecutor);
    return service;
  }

  private SharedPreferences getSharedPreferences() {
    return PreferenceManager.getDefaultSharedPreferences(getInstrumentation().getContext());
  }

  private void clearSharedPreferences() {
    SharedPreferences sharedPreferences = getSharedPreferences();

    // Clear listeners registered by {@code invokeOnCreateInternal}.
    for (OnSharedPreferenceChangeListener listener : sharedPreferenceChangeListeners) {
      sharedPreferences.unregisterOnSharedPreferenceChangeListener(listener);
    }
    sharedPreferenceChangeListeners.clear();

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

  private static void invokeOnCreateInternal(MozcBaseService service,
                                             ViewManager viewManager,
                                             SharedPreferences sharedPreferences,
                                             Configuration deviceConfiguration,
                                             SessionExecutor sessionExecutor) {
    ViewEventListener eventListener = service.new MozcEventListener();
    service.onCreateInternal(
        eventListener, viewManager, sharedPreferences, deviceConfiguration, sessionExecutor);
    sharedPreferenceChangeListeners.add(service.sharedPreferenceChangeListener);
  }

  @SmallTest
  // UiThreadTest annotation to handle callback of shared preference.
  @UiThreadTest
  public void testOnCreate() {
    SharedPreferences preferences = getSharedPreferences();
    // Client-side config
    preferences.edit()
        .putBoolean("pref_haptic_feedback_key", true)
        .putBoolean("pref_other_anonimous_mode_key", true)
        .commit();

    SessionExecutor sessionExecutor = createStrictMock(SessionExecutor.class);
    sessionExecutor.reset(isA(SessionHandlerFactory.class), isA(Context.class));
    sessionExecutor.setLogging(anyBoolean());
    sessionExecutor.setImposedConfig(isA(Config.class));
    sessionExecutor.setConfig(ConfigUtil.toConfig(preferences));
    sessionExecutor.switchInputMode(same(Optional.<KeyEventInterface>absent()),
                                    isA(CompositionMode.class),
                                    anyObject(EvaluationCallback.class));
    expectLastCall().asStub();
    sessionExecutor.updateRequest(isA(Request.class), eq(Collections.<TouchEvent>emptyList()));
    expectLastCall().asStub();
    sessionExecutor.preferenceUsageStatsEvent(
        anyObject(SharedPreferences.class), anyObject(Resources.class));

    ViewManager viewManager = createMockBuilder(ViewManager.class)
        .withConstructor(Context.class, ViewEventListener.class,
                         SymbolHistoryStorage.class, ImeSwitcher.class, MenuDialogListener.class)
        .withArgs(getInstrumentation().getTargetContext(),
                  createNiceMock(ViewEventListener.class),
                  createNiceMock(SymbolHistoryStorage.class),
                  createNiceMock(ImeSwitcher.class),
                  createNiceMock(MenuDialogListener.class))
        .addMockedMethod("onConfigurationChanged")
        .createMock();
    viewManager.onConfigurationChanged(anyObject(Configuration.class));

    replayAll();

    MozcBaseService service = createService();

    // Both sessionExecutor and viewManager is not initialized.
    assertNull(service.sessionExecutor);
    assertNull(service.viewManager);

    // A window is not created.
    assertNull(service.getWindow());

    // Initialize the service.
    try {
      // Emulate non-dev channel situation.
      MozcUtil.setDevChannel(Optional.of(false));
      invokeOnCreateInternal(service, viewManager, preferences, getDefaultDeviceConfiguration(),
                             sessionExecutor);
    } finally {
      MozcUtil.setDevChannel(Optional.<Boolean>absent());
    }

    verifyAll();

    // The following variables must have been initialized after the initialization of the service.
    assertNotNull(service.getWindow());
    assertSame(sessionExecutor, service.sessionExecutor);
    assertNotNull(service.viewManager);
    assertTrue(service.feedbackManager.isHapticFeedbackEnabled());

    // SYNC_DATA should be sent periodically.
    assertTrue(service.sendSyncDataCommandHandler.hasMessages(0));

    // SharedPreferences should be cached.
    assertSame(preferences, service.sharedPreferences);
  }

  @SmallTest
  public void testOnCreate_viewManager() {
    MozcBaseService service = createService();
    try {
      invokeOnCreateInternal(service, null, getSharedPreferences(), getDefaultDeviceConfiguration(),
                             createNiceMock(SessionExecutor.class));
      assertSame(ViewManager.class, service.viewManager.getClass());

    } finally {
    }
  }

  @SmallTest
  public void testOnCreateInputView() {
    MozcBaseService service = createService();
    // A test which calls onCreateInputView before onCreate is not needed
    // because IMF ensures calling onCreate before onCreateInputView.
    SessionExecutor sessionExecutor = createNiceMock(SessionExecutor.class);
    replayAll();

    invokeOnCreateInternal(
        service, null, getSharedPreferences(), getDefaultDeviceConfiguration(), sessionExecutor);

    verifyAll();
    assertNotNull(service.onCreateInputView());
    // Created view is tested in CandidateViewTest.
  }

  @SmallTest
  public void testKeyboardInitialized() {
    MozcBaseService service = createService();
    SessionExecutor sessionExecutor = createNiceMock(SessionExecutor.class);
    KeyboardSpecification defaultSpecification = service.currentKeyboardSpecification;
    sessionExecutor.updateRequest(
        MozcUtil.getRequestBuilder(
            service.getResources(), defaultSpecification, getDefaultDeviceConfiguration()).build(),
        Collections.<TouchEvent>emptyList());

    replayAll();
    invokeOnCreateInternal(
        service, null, getSharedPreferences(), getDefaultDeviceConfiguration(), sessionExecutor);
    verifyAll();
  }

  @SmallTest
  public void testOnFinishInput() {
    SessionExecutor sessionExecutor = createMock(SessionExecutor.class);
    MozcBaseService service = createInitializedService(sessionExecutor);

    resetAll();
    sessionExecutor.resetContext();

    ViewEventListener eventListener = createNiceMock(ViewEventListener.class);
    Context context = getInstrumentation().getTargetContext();
    ViewManager viewManager = createViewManagerMock(context, eventListener);
    service.viewManager = viewManager;
    viewManager.reset();
    replayAll();

    // When onFinishInput is called, the inputHandler should receive CLEANUP command.
    service.onFinishInput();

    verifyAll();
  }

  @SmallTest
  public void testOnStartInput_notRestarting() {
    SessionExecutor sessionExecutor = createMock(SessionExecutor.class);
    SelectionTracker selectionTracker = createMock(SelectionTracker.class);
    MozcBaseService service = createInitializedService(sessionExecutor);
    Capture<Request> requestCapture = new Capture<Request>();

    resetAll();
    sessionExecutor.resetContext();
    // Exactly one message is issued to switch input field type by onStartInput().
    // It should not render a result in order to preserve
    // the existent text. b/4422455
    sessionExecutor.switchInputFieldType(InputFieldType.NORMAL);

    // Setting for Emoji attributes.
    sessionExecutor.updateRequest(
        capture(requestCapture), eq(Collections.<TouchEvent>emptyList()));

    selectionTracker.onStartInput(1, 2, false);
    replayAll();

    service.selectionTracker = selectionTracker;

    EditorInfo editorInfo = new EditorInfo();
    editorInfo.initialSelStart = 1;
    editorInfo.initialSelEnd = 2;
    service.onStartInput(editorInfo, false);

    verifyAll();
    assertTrue(requestCapture.getValue().hasEmojiRewriterCapability());
    assertTrue(requestCapture.getValue().hasAvailableEmojiCarrier());
  }

  @SmallTest
  public void testOnStartInput_restarting() {
    SessionExecutor sessionExecutor = createMock(SessionExecutor.class);
    SelectionTracker selectionTracker = createMock(SelectionTracker.class);
    MozcBaseService service = createInitializedService(sessionExecutor);
    service.selectionTracker = selectionTracker;
    Capture<Request> requestCapture = new Capture<Request>();

    resetAll();
    sessionExecutor.resetContext();
    // Exactly one message is issued to switch input field type by onStartInput().
    // It should not render a result in order to preserve
    // the existent text. b/4422455
    sessionExecutor.switchInputFieldType(InputFieldType.NORMAL);

    // Setting for Emoji attributes.
    sessionExecutor.updateRequest(
        capture(requestCapture), eq(Collections.<TouchEvent>emptyList()));

    selectionTracker.onStartInput(1, 2, false);
    replayAll();

    EditorInfo editorInfo = new EditorInfo();
    editorInfo.initialSelStart = 1;
    editorInfo.initialSelEnd = 2;
    service.onStartInput(editorInfo, true);

    verifyAll();
    assertTrue(requestCapture.getValue().hasEmojiRewriterCapability());
    assertTrue(requestCapture.getValue().hasAvailableEmojiCarrier());
  }

  @SmallTest
  public void testOnCompositionModeChange() {
    SessionExecutor sessionExecutor = createMock(SessionExecutor.class);
    MozcBaseService service = createInitializedService(sessionExecutor);

    service.currentKeyboardSpecification = KeyboardSpecification.TWELVE_KEY_TOGGLE_ALPHABET;
    resetAll();
    sessionExecutor.updateRequest(anyObject(ProtoCommands.Request.class),
                                  eq(Collections.<TouchEvent>emptyList()));
    // Exactly one message is issued to switch composition mode.
    // It should not render a result in order to preserve
    // the existent text. b/5181946
    sessionExecutor.switchInputMode(same(Optional.<KeyEventInterface>absent()),
                                    eq(CompositionMode.HIRAGANA),
                                    anyObject(EvaluationCallback.class));
    replayAll();

    ViewEventListener listener = service.new MozcEventListener();
    listener.onKeyEvent(null, null, KeyboardSpecification.TWELVE_KEY_TOGGLE_KANA,
                        Collections.<TouchEvent>emptyList());

    verifyAll();
  }

  @SmallTest
  public void testSendKeyEventBackToApplication() {
    SessionExecutor sessionExecutor = createMock(SessionExecutor.class);
    MozcBaseService service = createInitializedService(sessionExecutor);

    resetAll();
    sessionExecutor.sendKeyEvent(isA(KeyEventInterface.class), isA(EvaluationCallback.class));
    replayAll();
    ViewEventListener listener = service.new MozcEventListener();
    listener.onKeyEvent(null, KeycodeConverter.getKeyEventInterface(KeyEvent.KEYCODE_BACK), null,
                        Collections.<TouchEvent>emptyList());

    verifyAll();
  }

  @SmallTest
  public void testOnKeyUp() {
    SessionExecutor sessionExecutor = createMockBuilder(SessionExecutor.class)
        .addMockedMethod("sendKeyEvent")
        .createMock();
    replayAll();
    MozcBaseService service = initializeMozcService(new AlwaysShownMozcService(), sessionExecutor);

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
  // UiThreadTest annotation to handle callback of shared preference.
  @UiThreadTest
  public void testOnKeyDownByHardwareKeyboard() {
    Context context = getInstrumentation().getTargetContext();

    MozcBaseService service = createMockBuilder(MozcBaseService.class)
        .addMockedMethods("isInputViewShown", "sendKeyWithKeyboardSpecification")
        .createMock();
    SessionExecutor sessionExecutor = createNiceMock(SessionExecutor.class);
    ViewEventListener eventListener = createNiceMock(ViewEventListener.class);
    ViewManager viewManager = createMockBuilder(ViewManager.class)
        .withConstructor(Context.class, ViewEventListener.class,
                         SymbolHistoryStorage.class, ImeSwitcher.class, MenuDialogListener.class)
        .withArgs(context, eventListener, createNiceMock(SymbolHistoryStorage.class),
                  createNiceMock(ImeSwitcher.class), createNiceMock(MenuDialogListener.class))
        .addMockedMethods("onHardwareKeyEvent")
        .createNiceMock();
    service.sendKeyWithKeyboardSpecification(
        same(ProtoCommands.KeyEvent.class.cast(null)), same(KeyEventInterface.class.cast(null)),
        anyObject(KeyboardSpecification.class), anyObject(Configuration.class),
        eq(Collections.<TouchEvent>emptyList()));
    expectLastCall().asStub();

    // Before test, we need to setup mock instances with some method calls.
    // Because the calls is out of this test, we do it by NiceMock and replayAll.
    replayAll();
    initializeMozcService(service, sessionExecutor);
    SharedPreferences sharedPreferences = getSharedPreferences();
    invokeOnCreateInternal(service, viewManager, sharedPreferences,
                           context.getResources().getConfiguration(), sessionExecutor);
    sharedPreferences.edit().remove(PreferenceUtil.PREF_HARDWARE_KEYMAP).commit();
    verifyAll();

    // Real test part.
    resetAll();

    KeyEvent event = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_A);
    expect(service.isInputViewShown()).andStubReturn(true);
    ClientSidePreference clientSidePreference = service.propagatedClientSidePreference;
    assertNotNull(clientSidePreference);
    assertEquals(HardwareKeyMap.DEFAULT, clientSidePreference.getHardwareKeyMap());
    viewManager.onHardwareKeyEvent(event);

    replayAll();

    assertTrue(service.onKeyDownInternal(0, event, getDefaultDeviceConfiguration()));

    verifyAll();
  }



  @SmallTest
  public void testSendKeyWithKeyboardSpecification_switchKeyboardSpecification() {
    SessionExecutor sessionExecutor = createMock(SessionExecutor.class);
    MozcBaseService service = createInitializedService(sessionExecutor);

    EvaluationCallback renderResultCallback = service.renderResultCallback;

    service.currentKeyboardSpecification = KeyboardSpecification.TWELVE_KEY_FLICK_KANA;

    // Send "a" without keyboard specification change. [TWELVE_KEY_FLICK_KANA]
    resetAll();

    ProtoCommands.KeyEvent mozcKeyEvent =
        ProtoCommands.KeyEvent.newBuilder().setKeyCode('a').build();
    KeyEventInterface keyEvent = KeycodeConverter.getKeyEventInterface(KeyEvent.KEYCODE_A);
    KeyboardSpecification keyboardSpecification = KeyboardSpecification.TWELVE_KEY_FLICK_KANA;

    sessionExecutor.sendKey(
        mozcKeyEvent, keyEvent, Collections.<TouchEvent>emptyList(), renderResultCallback);

    replayAll();

    service.sendKeyWithKeyboardSpecification(
        mozcKeyEvent, keyEvent, keyboardSpecification, getDefaultDeviceConfiguration(),
        Collections.<TouchEvent>emptyList());

    verifyAll();

    // Send "a" with keyboard specification change. [HARDWARE_QWERTY_KANA]
    // This is transition from software to hardware keyboard, so submit() is called.
    resetAll();

    mozcKeyEvent = ProtoCommands.KeyEvent.newBuilder().setKeyCode('a').build();
    keyEvent = KeycodeConverter.getKeyEventInterface(KeyEvent.KEYCODE_A);
    keyboardSpecification = KeyboardSpecification.HARDWARE_QWERTY_KANA;

    sessionExecutor.submit(same(service.renderResultCallback));
    sessionExecutor.updateRequest(
        MozcUtil.getRequestBuilder(
            service.getResources(), keyboardSpecification, getDefaultDeviceConfiguration()).build(),
        Collections.<TouchEvent>emptyList());
    sessionExecutor.sendKey(
        ProtoCommands.KeyEvent.newBuilder(mozcKeyEvent)
            .setMode(keyboardSpecification.getCompositionMode())
            .build(),
        keyEvent, Collections.<TouchEvent>emptyList(), renderResultCallback);

    replayAll();

    service.sendKeyWithKeyboardSpecification(
        mozcKeyEvent, keyEvent, keyboardSpecification, getDefaultDeviceConfiguration(),
        Collections.<TouchEvent>emptyList());

    verifyAll();

    // Send "a" without keyboard specification change. [HARDWARE_QWERTY_KANA]
    resetAll();

    mozcKeyEvent = ProtoCommands.KeyEvent.newBuilder().setKeyCode('a').build();
    keyEvent = KeycodeConverter.getKeyEventInterface(KeyEvent.KEYCODE_A);
    keyboardSpecification = KeyboardSpecification.HARDWARE_QWERTY_KANA;

    sessionExecutor.sendKey(
        mozcKeyEvent, keyEvent, Collections.<TouchEvent>emptyList(), renderResultCallback);

    replayAll();

    service.sendKeyWithKeyboardSpecification(
        mozcKeyEvent, keyEvent, keyboardSpecification, getDefaultDeviceConfiguration(),
        Collections.<TouchEvent>emptyList());

    verifyAll();

    // Change keyboard specification. [TWELVE_KEY_FLICK_KANA]
    resetAll();

    mozcKeyEvent = null;
    keyEvent = null;
    keyboardSpecification = KeyboardSpecification.TWELVE_KEY_FLICK_KANA;

    sessionExecutor.updateRequest(
        MozcUtil.getRequestBuilder(
            service.getResources(), keyboardSpecification, getDefaultDeviceConfiguration()).build(),
        Collections.<TouchEvent>emptyList());
    sessionExecutor.switchInputMode(same(Optional.<KeyEventInterface>absent()),
                                    eq(keyboardSpecification.getCompositionMode()),
                                    same(renderResultCallback));

    replayAll();

    service.sendKeyWithKeyboardSpecification(
        mozcKeyEvent, keyEvent, keyboardSpecification, getDefaultDeviceConfiguration(),
        Collections.<TouchEvent>emptyList());

    verifyAll();
  }

  @SmallTest
  public void testMetaKeyHandling_b13238551() {
    SessionExecutor sessionExecutor = createMock(SessionExecutor.class);
    MozcBaseService service = createInitializedService(sessionExecutor);
    service.currentKeyboardSpecification = KeyboardSpecification.TWELVE_KEY_FLICK_KANA;
    ProtoCommands.KeyEvent mozcKeyEvent = null;
    KeyEventInterface keyEvent = KeycodeConverter.getKeyEventInterface(KeyEvent.KEYCODE_SHIFT_LEFT);

    // Send shift key only without keyboard specification change.
    resetAll();

    KeyboardSpecification keyboardSpecification = KeyboardSpecification.TWELVE_KEY_FLICK_KANA;
    sessionExecutor.sendKeyEvent(keyEvent, service.sendKeyToApplicationCallback);

    replayAll();

    service.sendKeyWithKeyboardSpecification(
        mozcKeyEvent, keyEvent, keyboardSpecification, getDefaultDeviceConfiguration(),
        Collections.<TouchEvent>emptyList());

    verifyAll();

    // Send shift key only with keyboard specification change.
    resetAll();

    keyboardSpecification = KeyboardSpecification.HARDWARE_QWERTY_KANA;
    sessionExecutor.submit(same(service.renderResultCallback));
    sessionExecutor.updateRequest(
        MozcUtil.getRequestBuilder(
            service.getResources(), keyboardSpecification, getDefaultDeviceConfiguration()).build(),
        Collections.<TouchEvent>emptyList());
    sessionExecutor.switchInputMode(
        sameOptional(keyEvent), isA(CompositionMode.class), same(service.renderResultCallback));

    replayAll();

    service.sendKeyWithKeyboardSpecification(
        mozcKeyEvent, keyEvent, keyboardSpecification, getDefaultDeviceConfiguration(),
        Collections.<TouchEvent>emptyList());

    verifyAll();

    // Handle meta key on renderInputConnection invoked by renderResultCallback.
    resetAll();

    service.sendKeyEvent(keyEvent);

    replayAll();

    service.renderInputConnection(
        Command.newBuilder()
            .setInput(Input.newBuilder()
                .setType(Input.CommandType.SEND_COMMAND)
                .setCommand(SessionCommand.newBuilder()
                    .setType(SessionCommand.CommandType.SWITCH_INPUT_MODE)))
            .setOutput(Output.newBuilder()
                .setConsumed(true))
            .build(),
        keyEvent);

    verifyAll();
  }

  @SmallTest
  public void testShowWindow() {
    final InputConnection inputConnection = createMock(InputConnection.class);
    expect(inputConnection.beginBatchEdit()).andStubReturn(true);
    expect(inputConnection.endBatchEdit()).andStubReturn(true);
    // Expect that setComposingText() is not called.
    // If it is called, when the IME is shown existing selection range is cleared unexpectedly.
    // Unfortunately EasyMock#times() methods doesn't accept parameter 0 (meaning that
    // mocked method shouldn't be called) so such indirect approach is employed.
    replayAll();

    MozcBaseService service = new MozcBaseService() {
      @Override
      public InputConnection getCurrentInputConnection() {
        return inputConnection;
      }
    };
    service.attachBaseContext(getInstrumentation().getTargetContext());

    DependencyFactory.setDependency(Optional.of(DependencyFactory.TOUCH_FRAGMENT_PREF));
    service.onCreate();
    service.onWindowShown();
    service.sessionExecutor.waitForAllQueuesForEmpty();
    verifyAll();
  }

  @SmallTest
  public void testHideWindow() {
    SessionExecutor sessionExecutor = createStrictMock(SessionExecutor.class);
    MozcBaseService service = createInitializedService(sessionExecutor);
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
    MozcBaseService service = createInitializedService(sessionExecutor);
    SelectionTracker selectionTracker = createMock(SelectionTracker.class);
    service.selectionTracker = selectionTracker;
    InputConnection inputConnection = createStrictMock(InputConnection.class);
    service.onCreateInputMethodInterface().restartInput(inputConnection, new EditorInfo());

    Preedit preedit = Preedit.newBuilder()
        .addSegment(Segment.newBuilder()
            .setValue("\uD83D\uDC31")  // a character meaning a cat
            .setAnnotation(Annotation.HIGHLIGHT)
            .setValueLength(1))
        .addSegment(Segment.newBuilder()
            .setValue("1")
            .setAnnotation(Annotation.UNDERLINE)
            .setValueLength(1))
        .addSegment(Segment.newBuilder()
            .setValue("2")
            .setAnnotation(Annotation.UNDERLINE)
            .setValueLength(1))
        .setCursor(1)
        .build();
    Command command = Command.newBuilder()
        .setInput(Input.newBuilder().setType(Input.CommandType.SEND_KEY))
        .setOutput(Output.newBuilder()
            .setConsumed(true).setPreedit(preedit)
            .setAllCandidateWords(CandidateList.newBuilder().setCategory(Category.SUGGESTION)))
        .build();

    resetAll();
    expect(inputConnection.beginBatchEdit()).andReturn(true);
    Capture<SpannableStringBuilder> spannableStringBuilderCapture =
        new Capture<SpannableStringBuilder>();
    // the cursor is at the tail
    expect(inputConnection.setComposingText(capture(spannableStringBuilderCapture), eq(1)))
        .andReturn(true);
    expect(selectionTracker.getPreeditStartPosition()).andReturn(0);
    expect(inputConnection.setSelection(1, 1)).andReturn(true);
    selectionTracker.onRender(null, null, preedit);
    expect(inputConnection.endBatchEdit()).andReturn(true);
    replayAll();

    service.renderInputConnection(command, KeycodeConverter.getKeyEventInterface('\0'));

    verifyAll();
    SpannableStringBuilder sb = spannableStringBuilderCapture.getValue();
    assertEquals("\uD83D\uDC3112", sb.toString());
    assertEquals(0, sb.getSpanStart(MozcBaseService.SPAN_UNDERLINE));
    assertEquals(4, sb.getSpanEnd(MozcBaseService.SPAN_UNDERLINE));
    assertEquals(2, sb.getSpanStart(MozcBaseService.SPAN_PARTIAL_SUGGESTION_COLOR));
    assertEquals(4, sb.getSpanEnd(MozcBaseService.SPAN_PARTIAL_SUGGESTION_COLOR));
    assertEquals(0, sb.getSpanStart(MozcBaseService.SPAN_BEFORE_CURSOR));
    assertEquals(2, sb.getSpanEnd(MozcBaseService.SPAN_BEFORE_CURSOR));
  }

  @SmallTest
  public void testRenderInputConnection_updatingPreeditAtCursorMiddle() {
    // Updating preedit (cursor is at the middle)
    SessionExecutor sessionExecutor = createMock(SessionExecutor.class);
    MozcBaseService service = createInitializedService(sessionExecutor);
    SelectionTracker selectionTracker = createMock(SelectionTracker.class);
    service.selectionTracker = selectionTracker;
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
    Command command = Command.newBuilder()
        .setInput(Input.newBuilder().setType(Input.CommandType.SEND_KEY))
        .setOutput(Output.newBuilder().setConsumed(true).setPreedit(preedit))
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

    service.renderInputConnection(command, KeycodeConverter.getKeyEventInterface('\0'));

    verifyAll();
    SpannableStringBuilder sb = spannableStringBuilderCapture.getValue();
    assertEquals("012", sb.toString());
    assertEquals(0, sb.getSpanStart(MozcBaseService.SPAN_UNDERLINE));
    assertEquals(3, sb.getSpanEnd(MozcBaseService.SPAN_UNDERLINE));
    assertEquals(0, sb.getSpanStart(MozcBaseService.SPAN_BEFORE_CURSOR));
    assertEquals(2, sb.getSpanEnd(MozcBaseService.SPAN_BEFORE_CURSOR));
  }

  @SmallTest
  public void testRenderInputConnection_webView() {
    // Updating preedit (cursor is at the middle)
    SessionExecutor sessionExecutor = createNiceMock(SessionExecutor.class);
    SelectionTracker selectionTracker = createMock(SelectionTracker.class);
    MozcBaseService service = createInitializedService(sessionExecutor);
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

    service.selectionTracker = selectionTracker;

    Command command = Command.newBuilder()
        .setInput(Input.newBuilder().setType(Input.CommandType.SEND_KEY))
        .setOutput(Output.newBuilder().setConsumed(true).setPreedit(preedit))
        .build();
    service.renderInputConnection(command, KeycodeConverter.getKeyEventInterface('\0'));

    verifyAll();
    SpannableStringBuilder sb = spannableStringBuilderCapture.getValue();
    assertEquals("012", sb.toString());
    assertEquals(0, sb.getSpanStart(MozcBaseService.SPAN_UNDERLINE));
    assertEquals(3, sb.getSpanEnd(MozcBaseService.SPAN_UNDERLINE));
    assertEquals(0, sb.getSpanStart(MozcBaseService.SPAN_BEFORE_CURSOR));
    assertEquals(2, sb.getSpanEnd(MozcBaseService.SPAN_BEFORE_CURSOR));
  }

  @SmallTest
  public void testRenderInputConnection_commit() {
    SessionExecutor sessionExecutor = createNiceMock(SessionExecutor.class);
    MozcBaseService service = createInitializedService(sessionExecutor);
    SelectionTracker selectionTracker = createMock(SelectionTracker.class);
    service.selectionTracker = selectionTracker;

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
    Command command = Command.newBuilder()
        .setInput(Input.newBuilder().setType(Input.CommandType.SEND_KEY))
        .setOutput(Output.newBuilder()
            .setConsumed(true)
            .setResult(Result.newBuilder().setValue("commit").setType(ResultType.STRING))
            .setPreedit(preedit)
            .setAllCandidateWords(CandidateList.newBuilder().setCategory(Category.CONVERSION)))
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

    service.renderInputConnection(command, KeycodeConverter.getKeyEventInterface('\0'));

    verifyAll();
    SpannableStringBuilder sb = spannableStringBuilderCapture.getValue();
    assertEquals("012", sb.toString());
    assertEquals(0, sb.getSpanStart(MozcBaseService.SPAN_UNDERLINE));
    assertEquals(3, sb.getSpanEnd(MozcBaseService.SPAN_UNDERLINE));
    assertEquals(2, sb.getSpanStart(MozcBaseService.SPAN_CONVERT_HIGHLIGHT));
    assertEquals(3, sb.getSpanEnd(MozcBaseService.SPAN_CONVERT_HIGHLIGHT));
  }

  @SmallTest
  public void testRenderInputConnection_directInput() {
    // If mozc service doesn't consume the KeyEvent, delegate it to the sendKeyEvent.
    MozcBaseService service = createMockBuilder(MozcBaseService.class)
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

    Command command = Command.newBuilder()
        .setInput(Input.newBuilder().setType(Input.CommandType.SEND_KEY))
        .setOutput(Output.newBuilder().setConsumed(false))
        .build();

    service.renderInputConnection(command, keyEvent);

    verifyAll();
  }

  /**
   * If the output is for SWITCH_INPUT_MODE, composing text of InputConnection
   * must not be updated by setComposingText.
   */
  @SmallTest
  public void testRenderInputConnection_switchInputMode() {
    MozcBaseService service = createInitializedService(createNiceMock(SessionExecutor.class));
    InputConnection inputConnection =
        new InputConnectionWrapper(createNiceMock(InputConnection.class), false) {
      @Override
      public boolean setComposingText(CharSequence text, int newCursorPosition) {
        fail("setComposingText shouldn't be called for SWITCH_INPUT_MODE");
        return true;
      }
    };
    replayAll();

    service.onCreateInputMethodInterface().restartInput(inputConnection, new EditorInfo());

    Command command = Command.newBuilder()
        .setInput(Input.newBuilder()
            .setType(Input.CommandType.SEND_COMMAND)
            .setCommand(SessionCommand.newBuilder()
                .setType(SessionCommand.CommandType.SWITCH_INPUT_MODE)))
        .setOutput(Output.newBuilder().setConsumed(true))
        .build();

    service.renderInputConnection(command, null);
  }

  @SmallTest
  public void testSendKeyEvent() {
    MozcBaseService service = createMockBuilder(MozcBaseService.class)
        .addMockedMethods("requestHideSelf", "sendDownUpKeyEvents", "isInputViewShown",
                          "sendDefaultEditorAction", "getCurrentInputConnection")
        .createMock();
    ViewEventListener eventListener = createNiceMock(ViewEventListener.class);
    ViewManager viewManager =
        createViewManagerMock(getInstrumentation().getTargetContext(), eventListener);
    service.viewManager = viewManager;

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
          MozcUtil.getUptimeMillis(),
          MozcUtil.getUptimeMillis(),
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
          MozcUtil.getUptimeMillis(),
          MozcUtil.getUptimeMillis(),
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
    MozcBaseService service = createMockBuilder(MozcBaseService.class)
        .addMockedMethods("requestHideSelf", "sendDownUpKeyEvents", "isInputViewShown",
                          "sendDefaultEditorAction")
        .createMock();
    ViewEventListener eventListener = createNiceMock(ViewEventListener.class);
    ViewManager viewManager =
        createViewManagerMock(getInstrumentation().getTargetContext(), eventListener);
    service.viewManager = viewManager;

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

      // If SymbolInputView is shown, close it and do not pass the event
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
    MozcBaseService service = createMockBuilder(MozcBaseService.class)
        .addMockedMethods("requestHideSelf", "sendDownUpKeyEvents", "isInputViewShown",
                          "sendDefaultEditorAction", "getCurrentInputEditorInfo")
        .createMock();
    ViewEventListener eventListener = createNiceMock(ViewEventListener.class);
    ViewManager viewManager =
        createViewManagerMock(getInstrumentation().getTargetContext(), eventListener);
    service.viewManager = viewManager;

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
      expect(service.getCurrentInputEditorInfo()).andStubReturn(new EditorInfo());
      replayAll();

      service.sendKeyEvent(keyEvent);

      verifyAll();
    }
  }

  @SmallTest
  public void testRenderInputConnection_directInputDeletionRange() {
    SessionExecutor sessionExecutor = createNiceMock(SessionExecutor.class);
    MozcBaseService service = createInitializedService(sessionExecutor);
    SelectionTracker selectionTracker = createMock(SelectionTracker.class);
    service.selectionTracker = selectionTracker;
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

      Command command = Command.newBuilder()
          .setInput(Input.newBuilder().setType(Input.CommandType.SEND_KEY))
          .setOutput(Output.newBuilder()
              .setConsumed(true)
              .setDeletionRange(testData.deletionRange))
          .build();
      service.renderInputConnection(command, null);

      verifyAll();
      assertEquals("", composingTextCapture.getValue().toString());
    }
  }

  @SmallTest
  public void testSwitchKeyboard() throws SecurityException, IllegalArgumentException {
    // Prepare the service.
    SessionExecutor sessionExecutor = createMock(SessionExecutor.class);
    MozcBaseService service = createInitializedService(sessionExecutor);

    // Prepares the mock.
    resetAll();
    List<TouchEvent> touchEventList = Collections.singletonList(TouchEvent.getDefaultInstance());
    KeyboardSpecification newSpecification = KeyboardSpecification.QWERTY_KANA;

    sessionExecutor.updateRequest(
        MozcUtil.getRequestBuilder(
            service.getResources(), newSpecification, getDefaultDeviceConfiguration()).build(),
        touchEventList);
    sessionExecutor.switchInputMode(same(Optional.<KeyEventInterface>absent()),
                                    eq(newSpecification.getCompositionMode()),
                                    anyObject(EvaluationCallback.class));

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
    assertEquals(newSpecification, service.currentKeyboardSpecification);
    verifyAll();
  }

  @SmallTest
  // UiThreadTest annotation to handle callback of shared preference.
  @UiThreadTest
  public void testPreferenceInitialization() {
    MozcBaseService service = createService();
    SharedPreferences sharedPreferences = getSharedPreferences();
    sharedPreferences.edit()
        .putString("pref_portrait_keyboard_layout_key", KeyboardLayout.QWERTY.name())
        .commit();

    SessionExecutor sessionExecutor = createMock(SessionExecutor.class);
    sessionExecutor.reset(isA(SessionHandlerFactory.class), same(service));
    sessionExecutor.setLogging(anyBoolean());
    sessionExecutor.setImposedConfig(isA(Config.class));
    expectLastCall().asStub();
    sessionExecutor.updateRequest(isA(Request.class), eq(Collections.<TouchEvent>emptyList()));
    expectLastCall().asStub();
    sessionExecutor.switchInputMode(same(Optional.<KeyEventInterface>absent()),
                                    isA(CompositionMode.class),
                                    anyObject(EvaluationCallback.class));
    expectLastCall().asStub();
    sessionExecutor.setConfig(isA(Config.class));
    sessionExecutor.preferenceUsageStatsEvent(
        sharedPreferences, getInstrumentation().getTargetContext().getResources());
    replayAll();

    invokeOnCreateInternal(service, null, sharedPreferences, getDefaultDeviceConfiguration(),
                           sessionExecutor);


    verifyAll();

    // Make sure the UI is initialized expectedly.
    assertEquals(KeyboardLayout.QWERTY,
        service.viewManager.getActiveSoftwareKeyboardModel().getKeyboardLayout());
  }

  @SmallTest
  public void testSymbolHistoryStorageImpl_getAllHistory() {
    SessionExecutor sessionExecutor = createMock(SessionExecutor.class);
    SymbolHistoryStorage storage = new SymbolHistoryStorageImpl(sessionExecutor);

    class TestData extends Parameter {
      final SymbolMajorCategory majorCategory;
      final StorageType expectedStorageType;
      final String expectedValue;

      TestData(SymbolMajorCategory majorCategory, StorageType expectedStorageType,
          String expectedValue) {
        this.majorCategory = majorCategory;
        this.expectedStorageType = expectedStorageType;
        this.expectedValue = expectedValue;
      }
    }

    TestData[] testDataList = {
        new TestData(SymbolMajorCategory.SYMBOL, StorageType.SYMBOL_HISTORY, "SYMBOL_HISTORY"),
        new TestData(SymbolMajorCategory.EMOTICON, StorageType.EMOTICON_HISTORY,
                     "EMOTICON_HISTORY"),
        new TestData(SymbolMajorCategory.EMOJI, StorageType.EMOJI_HISTORY, "EMOJI_HISTORY"),
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
      final SymbolMajorCategory majorCategory;
      final StorageType expectedStorageType;
      final String expectedValue;

      TestData(SymbolMajorCategory majorCategory, StorageType expectedStorageType,
               String expectedValue) {
        this.majorCategory = majorCategory;
        this.expectedStorageType = expectedStorageType;
        this.expectedValue = expectedValue;
      }
    }

    TestData[] testDataList = {
        new TestData(SymbolMajorCategory.SYMBOL, StorageType.SYMBOL_HISTORY, "SYMBOL_HISTORY"),
        new TestData(SymbolMajorCategory.EMOTICON, StorageType.EMOTICON_HISTORY,
                     "EMOTICON_HISTORY"),
        new TestData(SymbolMajorCategory.EMOJI, StorageType.EMOJI_HISTORY, "EMOJI_HISTORY"),
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
    MozcBaseService service = createInitializedService(sessionExecutor);

    // Set feedback listener and force to enable feedbacks.
    resetAll();
    FeedbackListener feedbackListener = createMock(FeedbackListener.class);
    FeedbackManager feedbackManager = new FeedbackManager(feedbackListener);
    feedbackManager.setHapticFeedbackEnabled(true);
    feedbackManager.setHapticFeedbackDuration(100);
    feedbackManager.setSoundFeedbackEnabled(true);
    feedbackManager.setSoundFeedbackVolume(0.5f);
    service.feedbackManager = feedbackManager;

    // Expectation.
    feedbackListener.onSound(AudioManager.FX_KEY_CLICK, 0.5f);
    feedbackListener.onVibrate(100L);

    sessionExecutor.submitCandidate(eq(0), eq(Optional.<Integer>absent()),
                                    isA(EvaluationCallback.class));
    replayAll();

    // Invoke onConversionCandidateSelected.
    service.viewManager.getEventListener()
        .onConversionCandidateSelected(0, Optional.<Integer>absent());

    verifyAll();
  }

  @SmallTest
  public void testMozcEventListener_onSymbolCandidateSelected() {
    SessionExecutor sessionExecutor = createMock(SessionExecutor.class);
    MozcBaseService service = createInitializedService(sessionExecutor);
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
    service.feedbackManager = feedbackManager;

    // Expectation.
    expect(inputConnection.beginBatchEdit()).andReturn(true);
    expect(inputConnection.commitText("(^_^)", MozcUtil.CURSOR_POSITION_TAIL))
        .andReturn(true);
    expect(inputConnection.endBatchEdit()).andReturn(true);

    feedbackListener.onSound(AudioManager.FX_KEY_CLICK, 0.5f);
    feedbackListener.onVibrate(100);

    SymbolHistoryStorage historyStorage = createMock(SymbolHistoryStorage.class);
    historyStorage.addHistory(SymbolMajorCategory.EMOTICON, "(^_^)");
    service.symbolHistoryStorage = historyStorage;
    replayAll();

    // Invoke onConversionCandidateSelected.
    service.viewManager.getEventListener().onSymbolCandidateSelected(
        SymbolMajorCategory.EMOTICON, "(^_^)", true);

    verifyAll();
  }

  @SuppressWarnings("unchecked")
  @SmallTest
  public void testMozcEventListener_onNarrowModeChanged() {
    SessionExecutor sessionExecutorMock = createMock(SessionExecutor.class);
    MozcBaseService service = initializeMozcService(new MozcBaseService(), sessionExecutorMock);
    ViewManagerInterface viewManagerMock = createMock(ViewManagerInterface.class);
    service.viewManager = viewManagerMock;
    ViewEventListener viewEventListener = service.new MozcEventListener();

    Capture<Config> configCapture = new Capture<Config>();

    // On transition from narrow mode to non-narrow mode, submit() should be called.
    resetAll();
    expect(viewManagerMock.isFloatingCandidateMode()).andStubReturn(false);
    sessionExecutorMock.submit(same(service.renderResultCallback));
    sessionExecutorMock.setImposedConfig(capture(configCapture));
    replayAll();
    viewEventListener.onNarrowModeChanged(false);
    verifyAll();
    assertEquals(SelectionShortcut.NO_SHORTCUT,
                 configCapture.getValue().getSelectionShortcut());

    // On the opposite transition, submit() should not be called.
    resetAll();
    expect(viewManagerMock.isFloatingCandidateMode()).andStubReturn(true);
    sessionExecutorMock.setImposedConfig(capture(configCapture));
    replayAll();
    viewEventListener.onNarrowModeChanged(true);
    verifyAll();
    assertEquals(SelectionShortcut.SHORTCUT_123456789,
                 configCapture.getValue().getSelectionShortcut());

    // Don't use shortcut keys if floating candidate window is disabled.
    resetAll();
    expect(viewManagerMock.isFloatingCandidateMode()).andStubReturn(false);
    sessionExecutorMock.setImposedConfig(capture(configCapture));
    replayAll();
    viewEventListener.onNarrowModeChanged(true);
    verifyAll();
    assertEquals(SelectionShortcut.NO_SHORTCUT,
                 configCapture.getValue().getSelectionShortcut());
  }

  @SmallTest
  public void testOnUndo() {
    SessionExecutor sessionExecutor = createMock(SessionExecutor.class);
    MozcBaseService service = createInitializedService(sessionExecutor);

    resetAll();
    List<TouchEvent> touchEventList = Collections.singletonList(TouchEvent.getDefaultInstance());
    sessionExecutor.undoOrRewind(eq(touchEventList), isA(EvaluationCallback.class));
    replayAll();

    service.viewManager.getEventListener().onUndo(touchEventList);

    verifyAll();
  }

  @SmallTest
  public void testOnSubmitPreedit() {
    SessionExecutor sessionExecutor = createMock(SessionExecutor.class);
    MozcBaseService service = createInitializedService(sessionExecutor);

    resetAll();
    sessionExecutor.submit(isA(EvaluationCallback.class));
    replayAll();

    // Call the method.
    service.viewManager.getEventListener().onSubmitPreedit();

    verifyAll();
  }

  @SmallTest
  public void testOnExpandSuggestion() {
    SessionExecutor sessionExecutor = createMock(SessionExecutor.class);
    MozcBaseService service = createInitializedService(sessionExecutor);

    resetAll();
    sessionExecutor.expandSuggestion(isA(EvaluationCallback.class));
    replayAll();

    service.viewManager.getEventListener().onExpandSuggestion();

    verifyAll();
  }

  @SmallTest
  public void testPropagateClientSidePreference_touchUI() {
    Resources resoureces = getInstrumentation().getTargetContext().getResources();
    SessionExecutor sessionExecutor = createNiceMock(SessionExecutor.class);
    replayAll();
    MozcBaseService service = createInitializedService(sessionExecutor);

    ViewManagerInterface viewManager = service.viewManager;

    // Initialization (all fields are propagated).
    {
      ClientSidePreference clientSidePreference =
          new ClientSidePreference(
              true, 100, true, 100, false, KeyboardLayout.QWERTY, InputStyle.TOGGLE, false, false,
              0, EmojiProviderType.DOCOMO, HardwareKeyMap.JAPANESE109A, SkinType.ORANGE_LIGHTGRAY,
              true, LayoutAdjustment.FILL, 100);
      service.propagateClientSidePreference(clientSidePreference);

      // Check all the fields are propagated.
      FeedbackManager feedbackManager = service.feedbackManager;
      assertTrue(feedbackManager.isHapticFeedbackEnabled());
      assertEquals(feedbackManager.getHapticFeedbackDuration(), 100L);
      assertTrue(feedbackManager.isSoundFeedbackEnabled());
      assertEquals(feedbackManager.getSoundFeedbackVolume(), 0.8f);
      assertFalse(viewManager.isPopupEnabled());
      JapaneseSoftwareKeyboardModel model = viewManager.getActiveSoftwareKeyboardModel();
      assertEquals(KeyboardLayout.QWERTY, model.getKeyboardLayout());
      assertEquals(InputStyle.TOGGLE, model.getInputStyle());
      assertFalse(model.isQwertyLayoutForAlphabet());
      assertFalse(viewManager.isFullscreenMode());
      assertFalse(service.onEvaluateFullscreenMode());
      assertEquals(EmojiProviderType.DOCOMO, viewManager.getEmojiProviderType());
      assertEquals(HardwareKeyMap.JAPANESE109A, service.viewManager.getHardwareKeyMap());
      assertEquals(LayoutAdjustment.FILL, viewManager.getLayoutAdjustment());
      assertEquals(100, viewManager.getKeyboardHeightRatio());
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
      final Skin expectSkin;
      final boolean expectMicrophoneButtonEnabledByPreference;
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
               Skin expectSkin,
               boolean expectMicrophoneButtonEnabledByPreference,
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
        this.expectSkin = expectSkin;
        this.expectMicrophoneButtonEnabledByPreference = expectMicrophoneButtonEnabledByPreference;
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
                true, LayoutAdjustment.FILL, 100),
            false,
            10,
            true,
            0.08f,
            true,
            KeyboardLayout.QWERTY,
            InputStyle.TOGGLE,
            false,
            false,
            0,
            EmojiProviderType.DOCOMO,
            HardwareKeyMap.DEFAULT,
            SkinType.ORANGE_LIGHTGRAY.getSkin(resoureces),
            true,
            LayoutAdjustment.FILL,
            100),
        new TestData(
            new ClientSidePreference(
                false, 20, true, 20, false, KeyboardLayout.QWERTY, InputStyle.TOGGLE, false, false,
                -1, EmojiProviderType.DOCOMO, HardwareKeyMap.DEFAULT, SkinType.ORANGE_LIGHTGRAY,
                false, LayoutAdjustment.FILL, 100),
            false,
            20,
            true,
            0.16f,
            false,
            KeyboardLayout.QWERTY,
            InputStyle.TOGGLE,
            false,
            false,
            -1,
            EmojiProviderType.DOCOMO,
            HardwareKeyMap.DEFAULT,
            SkinType.ORANGE_LIGHTGRAY.getSkin(resoureces),
            false,
            LayoutAdjustment.FILL,
            100),
        new TestData(
            new ClientSidePreference(
                false, 30, false, 30, true, KeyboardLayout.QWERTY, InputStyle.TOGGLE, false, false,
                2, EmojiProviderType.KDDI, HardwareKeyMap.DEFAULT, SkinType.ORANGE_LIGHTGRAY,
                true, LayoutAdjustment.FILL, 100),
            false,
            30,
            false,
            0.24f,
            true,
            KeyboardLayout.QWERTY,
            InputStyle.TOGGLE,
            false,
            false,
            2,
            EmojiProviderType.KDDI,
            HardwareKeyMap.DEFAULT,
            SkinType.ORANGE_LIGHTGRAY.getSkin(resoureces),
            true,
            LayoutAdjustment.FILL,
            100),
        new TestData(
            new ClientSidePreference(
                false, 40, false, 40, true, KeyboardLayout.TWELVE_KEYS, InputStyle.TOGGLE,
                false, false, -3, EmojiProviderType.SOFTBANK, HardwareKeyMap.DEFAULT,
                SkinType.ORANGE_LIGHTGRAY, true, LayoutAdjustment.FILL, 100),
            false,
            40,
            false,
            0.32f,
            true,
            KeyboardLayout.TWELVE_KEYS,
            InputStyle.TOGGLE,
            false,
            false,
            -3,
            EmojiProviderType.SOFTBANK,
            HardwareKeyMap.DEFAULT,
            SkinType.ORANGE_LIGHTGRAY.getSkin(resoureces),
            true,
            LayoutAdjustment.FILL,
            100),
        new TestData(
            new ClientSidePreference(
                false, 50, false, 50, true, KeyboardLayout.TWELVE_KEYS, InputStyle.FLICK,
                false, false, 4, EmojiProviderType.DOCOMO, HardwareKeyMap.DEFAULT,
                SkinType.ORANGE_LIGHTGRAY, false, LayoutAdjustment.FILL, 100),
            false,
            50,
            false,
            0.4f,
            true,
            KeyboardLayout.TWELVE_KEYS,
            InputStyle.FLICK,
            false,
            false,
            4,
            EmojiProviderType.DOCOMO,
            HardwareKeyMap.DEFAULT,
            SkinType.ORANGE_LIGHTGRAY.getSkin(resoureces),
            false,
            LayoutAdjustment.FILL,
            100),
        new TestData(
            new ClientSidePreference(
                false, 60, false, 60, true, KeyboardLayout.TWELVE_KEYS, InputStyle.FLICK,
                true, false, -5, EmojiProviderType.KDDI, HardwareKeyMap.DEFAULT,
                SkinType.ORANGE_LIGHTGRAY, true, LayoutAdjustment.FILL, 100),
            false,
            60,
            false,
            0.48f,
            true,
            KeyboardLayout.TWELVE_KEYS,
            InputStyle.FLICK,
            true,
            false,
            -5,
            EmojiProviderType.KDDI,
            HardwareKeyMap.DEFAULT,
            SkinType.ORANGE_LIGHTGRAY.getSkin(resoureces),
            true,
            LayoutAdjustment.FILL,
            100),
        new TestData(
            new ClientSidePreference(
                false, 70, false, 70, true, KeyboardLayout.TWELVE_KEYS, InputStyle.FLICK,
                true, true, 6, EmojiProviderType.SOFTBANK, HardwareKeyMap.DEFAULT,
                SkinType.ORANGE_LIGHTGRAY, false, LayoutAdjustment.FILL, 100),
            false,
            70,
            false,
            0.56f,
            true,
            KeyboardLayout.TWELVE_KEYS,
            InputStyle.FLICK,
            true,
            true,
            6,
            EmojiProviderType.SOFTBANK,
            HardwareKeyMap.DEFAULT,
            SkinType.ORANGE_LIGHTGRAY.getSkin(resoureces),
            false,
            LayoutAdjustment.FILL, 100),
        new TestData(
            new ClientSidePreference(
                false, 80, false, 80, true, KeyboardLayout.TWELVE_KEYS, InputStyle.FLICK,
                true, true, -7, EmojiProviderType.SOFTBANK, HardwareKeyMap.JAPANESE109A,
                SkinType.ORANGE_LIGHTGRAY, false, LayoutAdjustment.FILL, 100),
            false,
            80,
            false,
            0.64f,
            true,
            KeyboardLayout.TWELVE_KEYS,
            InputStyle.FLICK,
            true,
            true,
            -7,
            EmojiProviderType.SOFTBANK,
            HardwareKeyMap.JAPANESE109A,
            SkinType.ORANGE_LIGHTGRAY.getSkin(resoureces),
            false,
            LayoutAdjustment.FILL,
            100),
        new TestData(
            new ClientSidePreference(
                false, 90, false, 90, true, KeyboardLayout.TWELVE_KEYS, InputStyle.FLICK,
                true, true, 8, EmojiProviderType.SOFTBANK, HardwareKeyMap.DEFAULT,
                SkinType.ORANGE_LIGHTGRAY, true, LayoutAdjustment.RIGHT, 100),
            false,
            90,
            false,
            0.72f,
            true,
            KeyboardLayout.TWELVE_KEYS,
            InputStyle.FLICK,
            true,
            true,
            8,
            EmojiProviderType.SOFTBANK,
            HardwareKeyMap.DEFAULT,
            SkinType.ORANGE_LIGHTGRAY.getSkin(resoureces),
            true,
            LayoutAdjustment.RIGHT,
            100),
        new TestData(
            new ClientSidePreference(
                false, 100, false, 100, true, KeyboardLayout.TWELVE_KEYS, InputStyle.FLICK,
                true, true, 8, EmojiProviderType.SOFTBANK, HardwareKeyMap.DEFAULT,
                SkinType.ORANGE_LIGHTGRAY, true, LayoutAdjustment.RIGHT, 120),
            false,
            100,
            false,
            0.8f,
            true,
            KeyboardLayout.TWELVE_KEYS,
            InputStyle.FLICK,
            true,
            true,
            8,
            EmojiProviderType.SOFTBANK,
            HardwareKeyMap.DEFAULT,
            SkinType.ORANGE_LIGHTGRAY.getSkin(resoureces),
            true,
            LayoutAdjustment.RIGHT,
            120),
    };

    for (TestData testData : testDataList) {
      service.propagateClientSidePreference(testData.clientSidePreference);
      FeedbackManager feedbackManager = service.feedbackManager;

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
                   viewManager.isPopupEnabled());
      JapaneseSoftwareKeyboardModel model = viewManager.getActiveSoftwareKeyboardModel();
      assertEquals(testData.toString(), testData.expectKeyboardLayout, model.getKeyboardLayout());
      assertEquals(testData.toString(), testData.expectInputStyle, model.getInputStyle());
      assertEquals(testData.toString(),
                   testData.expectQwertyLayoutForAlphabet, model.isQwertyLayoutForAlphabet());
      assertEquals(testData.toString(),
                   testData.expectFullscreenMode,
                   viewManager.isFullscreenMode());
      assertEquals(testData.toString(),
                   testData.expectFullscreenMode, service.onEvaluateFullscreenMode());
      assertEquals(testData.toString(),
                   testData.expectFlickSensitivity,
                   viewManager.getFlickSensitivity());
      assertEquals(testData.toString(),
                   testData.expectEmojiProviderType,
                   viewManager.getEmojiProviderType());
      assertEquals(testData.toString(),
                   testData.expectHardwareKeyMap, service.viewManager.getHardwareKeyMap());
      assertEquals(testData.toString(),
                   testData.expectSkin,
                   viewManager.getSkin());
      assertEquals(testData.toString(),
                   testData.expectMicrophoneButtonEnabledByPreference,
                   viewManager.isMicrophoneButtonEnabledByPreference());
      assertEquals(testData.toString(),
                   testData.expectLayoutAdjustment,
                   viewManager.getLayoutAdjustment());
      assertEquals(testData.toString(),
                   testData.expectKeyboardHeight,
                   viewManager.getKeyboardHeightRatio());
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
    MozcBaseService service = createInitializedService(sessionExecutor);
    FeedbackListener listener = service.feedbackManager.feedbackListener;
    listener.onSound(AudioManager.FX_KEYPRESS_STANDARD, 0.1f);
    listener.onSound(FeedbackEvent.NO_SOUND, 0.1f);
  }

  @SmallTest
  public void testOnConfigurationChanged() throws SecurityException, IllegalArgumentException {
    SessionExecutor sessionExecutor = createMock(SessionExecutor.class);
    SelectionTracker selectionTracker = createMock(SelectionTracker.class);
    ViewManager viewManager = createMockBuilder(ViewManager.class)
        .withConstructor(Context.class, ViewEventListener.class,
                         SymbolHistoryStorage.class, ImeSwitcher.class, MenuDialogListener.class)
        .withArgs(getInstrumentation().getTargetContext(),
                  createNiceMock(ViewEventListener.class),
                  createNiceMock(SymbolHistoryStorage.class),
                  createNiceMock(ImeSwitcher.class),
                  createNiceMock(MenuDialogListener.class))
        .addMockedMethods("onConfigurationChanged")
        .createMock();
    MozcBaseService service = createService();
    invokeOnCreateInternal(
        service, viewManager, getSharedPreferences(), getDefaultDeviceConfiguration(),
        sessionExecutor);
    service.selectionTracker = selectionTracker;
    service.inputBound = true;

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
    selectionTracker.onConfigurationChanged();

    sessionExecutor.resetContext();
    sessionExecutor.switchInputMode(same(Optional.<KeyEventInterface>absent()),
                                    isA(CompositionMode.class),
                                    anyObject(EvaluationCallback.class));
    expectLastCall().asStub();
    sessionExecutor.updateRequest(isA(Request.class), eq(Collections.<TouchEvent>emptyList()));
    expectLastCall().asStub();

    viewManager.onConfigurationChanged(deviceConfig);

    replayAll();

    // Test on no-hardware-keyboard configuration.
    service.onConfigurationChangedInternal(deviceConfig);

    verifyAll();

    Handler configurationChangedHandler = service.configurationChangedHandler;
    assertTrue(configurationChangedHandler.hasMessages(0));
    // Clean up the pending message.
    configurationChangedHandler.removeMessages(0);
  }

  @SmallTest
  public void testOnUpdateSelection_exactMatch() {
    // Prepare the service.
    SessionExecutor sessionExecutor = createMock(SessionExecutor.class);
    SelectionTracker selectionTracker = createMock(SelectionTracker.class);
    MozcBaseService service = createInitializedService(sessionExecutor);
    service.selectionTracker = selectionTracker;

    resetAll();
    // No events for SessionExecutor are expected.
    expect(selectionTracker.onUpdateSelection(0, 0, 1, 1, 0, 1, false))
        .andReturn(SelectionTracker.DO_NOTHING);
    replayAll();

    service.onUpdateSelection(0, 0, 1, 1, 0, 1);

    verifyAll();
  }

  @SmallTest
  public void testOnUpdateSelection_resetContext() {
    // Prepare the service.
    MozcBaseService service = createMockBuilder(MozcBaseService.class)
        .addMockedMethods("isInputViewShown")
        .createMock();
    SessionExecutor sessionExecutor = createMock(SessionExecutor.class);
    initializeMozcService(service, sessionExecutor);
    service.inputBound = true;

    InputConnection inputConnection = createMock(InputConnection.class);
    service.onCreateInputMethodInterface().restartInput(inputConnection, new EditorInfo());

    SelectionTracker selectionTracker = createMock(SelectionTracker.class);
    service.selectionTracker = selectionTracker;

    resetAll();
    expect(selectionTracker.onUpdateSelection(0, 0, 1, 1, 0, 1, false))
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
    MozcBaseService service = createMockBuilder(MozcBaseService.class)
        .addMockedMethods("isInputViewShown")
        .createMock();
    SessionExecutor sessionExecutor = createMock(SessionExecutor.class);
    initializeMozcService(service, sessionExecutor);

    InputConnection inputConnection = createMock(InputConnection.class);
    service.onCreateInputMethodInterface().restartInput(inputConnection, new EditorInfo());

    SelectionTracker selectionTracker = createMock(SelectionTracker.class);
    service.selectionTracker = selectionTracker;

    resetAll();
    expect(selectionTracker.onUpdateSelection(0, 0, 1, 1, 0, 1, false))
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
    MozcBaseService service = createInitializedService(sessionExecutor);
    service.selectionTracker = selectionTracker;

    resetAll();
    expect(selectionTracker.onUpdateSelection(0, 0, 1, 1, 0, 1, false))
        .andReturn(5);
    // Send moveCursor event as selectionTracker says.
    sessionExecutor.moveCursor(eq(5), isA(EvaluationCallback.class));
    replayAll();

    service.onUpdateSelection(0, 0, 1, 1, 0, 1);

    verifyAll();
  }

  @SmallTest
  public void testGetInputFieldType() {
    class TestData extends Parameter {
      final int inputType;
      final InputFieldType expectedMode;

      TestData (int inputType, InputFieldType expectedMode) {
        this.inputType = inputType;
        this.expectedMode = expectedMode;
      }
    }
    TestData[] testDataList = {
        new TestData((InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD),
                     InputFieldType.PASSWORD),
        new TestData((InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI
                      | InputType.TYPE_TEXT_FLAG_AUTO_COMPLETE),
                     InputFieldType.NORMAL),
        new TestData((InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD
                      | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS),
                     InputFieldType.PASSWORD),
        new TestData((InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_SHORT_MESSAGE
                      | InputType.TYPE_TEXT_FLAG_AUTO_CORRECT),
                     InputFieldType.NORMAL),
        new TestData((InputType.TYPE_CLASS_NUMBER | InputType.TYPE_NUMBER_FLAG_SIGNED),
                      InputFieldType.NUMBER),
        new TestData(InputType.TYPE_CLASS_PHONE,
                     InputFieldType.TEL),
        new TestData(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_WEB_PASSWORD,
                     InputFieldType.PASSWORD),
    };

    for (TestData testData : testDataList) {
      EditorInfo editorInfo = new EditorInfo();
      editorInfo.inputType = testData.inputType;
      assertEquals(testData.expectedMode, MozcBaseService.getInputFieldType(editorInfo));
    }
  }

  @SmallTest
  public void testOnHardwareKeyboardCompositionModeChange() {
    MozcBaseService service = initializeService(createMockBuilder(MozcBaseService.class)
        .addMockedMethod("isInputViewShown").createMock());
    Context context = getInstrumentation().getTargetContext();
    ViewManager viewManager = createMockBuilder(ViewManager.class)
        .withConstructor(Context.class, ViewEventListener.class,
                         SymbolHistoryStorage.class, ImeSwitcher.class, MenuDialogListener.class)
        .withArgs(context, service.new MozcEventListener(),
                  createNiceMock(SymbolHistoryStorage.class),
                  createNiceMock(ImeSwitcher.class),
                  createNiceMock(MenuDialogListener.class))
        .addMockedMethod("switchHardwareKeyboardCompositionMode")
        .createMock();
    invokeOnCreateInternal(
        service, viewManager, getSharedPreferences(), getDefaultDeviceConfiguration(),
        createNiceMock(SessionExecutor.class));
    MozcView mozcView = viewManager.createMozcView(context);

    resetAll();

    expect(service.isInputViewShown()).andStubReturn(true);
    viewManager.switchHardwareKeyboardCompositionMode(CompositionSwitchMode.TOGGLE);

    replayAll();

    mozcView.findViewById(R.id.hardware_composition_button).performClick();

    verifyAll();
  }

  @SmallTest
  @ApiLevel(17)
  @TargetApi(17)
  public void testHandleGenericMotionEvent() {
    MozcBaseService service = createMockBuilder(MozcBaseService.class)
        .addMockedMethods("isInputViewShown")
        .createMock();
    ViewEventListener eventListener = createNiceMock(ViewEventListener.class);
    ViewManager viewManager =
        createViewManagerMock(getInstrumentation().getTargetContext(), eventListener);
    service.viewManager = viewManager;

    MotionEvent motionEvent = MotionEvent.obtain(0, 0, 0, 0, 0, 0);
    try {
      expect(service.isInputViewShown()).andReturn(true);
      expect(service.viewManager.isGenericMotionToConsume(motionEvent)).andReturn(true);
      expect(service.viewManager.consumeGenericMotion(motionEvent)).andReturn(true);
      replayAll();

      assertTrue(service.onGenericMotionEvent(motionEvent));

      // Do not handle the event
      resetAll();
      expect(service.isInputViewShown()).andReturn(false);
      replayAll();

      assertFalse(service.onGenericMotionEvent(motionEvent));
    } finally {
      motionEvent.recycle();
    }
  }



}
