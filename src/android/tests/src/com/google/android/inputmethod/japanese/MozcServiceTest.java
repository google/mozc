// Copyright 2010-2014, Google Inc.
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

import org.mozc.android.inputmethod.japanese.DependencyFactory.Dependency;
import org.mozc.android.inputmethod.japanese.FeedbackManager.FeedbackEvent;
import org.mozc.android.inputmethod.japanese.FeedbackManager.FeedbackListener;
import org.mozc.android.inputmethod.japanese.HardwareKeyboard.CompositionSwitchMode;
import org.mozc.android.inputmethod.japanese.JapaneseKeyboard.KeyboardSpecification;
import org.mozc.android.inputmethod.japanese.KeycodeConverter.KeyEventInterface;
import org.mozc.android.inputmethod.japanese.MozcService.SymbolHistoryStorageImpl;
import org.mozc.android.inputmethod.japanese.SymbolInputView.MajorCategory;
import org.mozc.android.inputmethod.japanese.ViewManagerInterface.LayoutAdjustment;
import org.mozc.android.inputmethod.japanese.emoji.EmojiProviderType;
import org.mozc.android.inputmethod.japanese.model.JapaneseSoftwareKeyboardModel;
import org.mozc.android.inputmethod.japanese.model.SelectionTracker;
import org.mozc.android.inputmethod.japanese.model.SymbolCandidateStorage.SymbolHistoryStorage;
import org.mozc.android.inputmethod.japanese.preference.ClientSidePreference;
import org.mozc.android.inputmethod.japanese.preference.ClientSidePreference.HardwareKeyMap;
import org.mozc.android.inputmethod.japanese.preference.ClientSidePreference.InputStyle;
import org.mozc.android.inputmethod.japanese.preference.ClientSidePreference.KeyboardLayout;
import org.mozc.android.inputmethod.japanese.preference.PreferenceUtil;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.CompositionMode;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Context.InputFieldType;
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
import org.mozc.android.inputmethod.japanese.testing.ApiLevel;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;
import org.mozc.android.inputmethod.japanese.testing.Parameter;
import org.mozc.android.inputmethod.japanese.testing.VisibilityProxy;
import org.mozc.android.inputmethod.japanese.ui.MenuDialog.MenuDialogListener;
import org.mozc.android.inputmethod.japanese.util.ImeSwitcherFactory.ImeSwitcher;
import org.mozc.android.inputmethod.japanese.view.SkinType;
import com.google.common.base.Optional;
import com.google.protobuf.ByteString;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.content.res.Configuration;
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

import org.easymock.Capture;

import java.lang.reflect.InvocationTargetException;
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
public class MozcServiceTest extends InstrumentationTestCaseWithMock {

  /**
   * Store registered listeners to unregister them on {@code clearSharedPreferences}.
   */
  private static final List<OnSharedPreferenceChangeListener> sharedPreferenceChangeListeners =
      new ArrayList<OnSharedPreferenceChangeListener>(1);

  private static class AlwaysShownMozcService extends MozcService {

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
    service.sendSyncDataCommandHandler = new Handler();
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

  private static void invokeOnCreateInternal(MozcService service,
                                             ViewManager viewManager,
                                             SharedPreferences sharedPreferences,
                                             Configuration deviceConfiguration,
                                             SessionExecutor sessionExecutor) {
    ViewEventListener eventListener = service.new MozcEventListener();
    service.onCreateInternal(
        eventListener, viewManager, sharedPreferences, deviceConfiguration, sessionExecutor);
    if (sharedPreferences != null) {
      sharedPreferenceChangeListeners.add(service.sharedPreferenceChangeListener);
    }
  }

  @SmallTest
  // UiThreadTest annotation to handle callback of shared preference.
  @UiThreadTest
  public void testOnCreate() {
    SharedPreferences preferences = getSharedPreferences();
    // Client-side conifg
    preferences.edit()
        .putBoolean("pref_haptic_feedback_key", true)
        .putBoolean("pref_other_anonimous_mode_key", true)
        .commit();

    SessionExecutor sessionExecutor = createStrictMock(SessionExecutor.class);
    sessionExecutor.reset(isA(SessionHandlerFactory.class), isA(Context.class));
    sessionExecutor.setLogging(anyBoolean());
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
    MozcService service = createService();
    try {
      invokeOnCreateInternal(service, null, null, getDefaultDeviceConfiguration(),
                             createNiceMock(SessionExecutor.class));
      assertSame(ViewManager.class, service.viewManager.getClass());

    } finally {
    }
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
  public void testKeyboardInitialized() {
    MozcService service = createService();
    SessionExecutor sessionExecutor = createNiceMock(SessionExecutor.class);
    KeyboardSpecification defaultSpecification = service.currentKeyboardSpecification;
    sessionExecutor.updateRequest(
        MozcUtil.getRequestForKeyboard(defaultSpecification.getKeyboardSpecificationName(),
                                       defaultSpecification.getSpecialRomanjiTable(),
                                       defaultSpecification.getSpaceOnAlphanumeric(),
                                       defaultSpecification.isKanaModifierInsensitiveConversion(),
                                       defaultSpecification.getCrossingEdgeBehavior(),
                                       getDefaultDeviceConfiguration()),
        Collections.<TouchEvent>emptyList());

    replayAll();
    invokeOnCreateInternal(service, null, null, getDefaultDeviceConfiguration(), sessionExecutor);
    verifyAll();
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
    MozcService service = createInitializedService(sessionExecutor);
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
    MozcService service = createInitializedService(sessionExecutor);
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
    MozcService service = createInitializedService(sessionExecutor);

    service.currentKeyboardSpecification = KeyboardSpecification.TWELVE_KEY_TOGGLE_ALPHABET;
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
  public void testSendKeyEventBackToApplication() {
    SessionExecutor sessionExecutor = createMock(SessionExecutor.class);
    MozcService service = createInitializedService(sessionExecutor);

    resetAll();
    sessionExecutor.sendKeyEvent(isA(KeyEventInterface.class), isA(EvaluationCallback.class));
    replayAll();
    ViewEventListener listener = service.new MozcEventListener();
    listener.onKeyEvent(null, KeycodeConverter.getKeyEventInterface(KeyEvent.KEYCODE_BACK), null,
                        Collections.<TouchEvent>emptyList());

    verifyAll();
  }

  @SmallTest
  public void testOnKeyDown() {
    SessionExecutor sessionExecutor = createNiceMock(SessionExecutor.class);
    replayAll();
    MozcService service = initializeMozcService(new AlwaysShownMozcService(), sessionExecutor);

    KeyEvent[] keyEventsToBeConsumed = {
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_SPACE),
        // 'a'
        new KeyEvent(0L, 0L, KeyEvent.ACTION_DOWN, 0, 0, 0, 0, 0x1E),
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
    MozcService service = initializeMozcService(new AlwaysShownMozcService(), sessionExecutor);

    KeyEvent[] keyEventsToBeConsumed = {
        new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_SPACE),
        // 'a'
        new KeyEvent(0L, 0L, KeyEvent.ACTION_UP, 0, 0, 0, 0, 0x1E),
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
  // UiThreadTest annotation to handle callback of shared preference.
  @UiThreadTest
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
    service.hardwareKeyboard = hardwareKeyboard;
    SharedPreferences sharedPreferences = getSharedPreferences();
    invokeOnCreateInternal(service, viewManager, sharedPreferences,
                           context.getResources().getConfiguration(), sessionExecutor);
    sharedPreferences.edit().remove(PreferenceUtil.PREF_HARDWARE_KEYMAP).commit();
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

    ClientSidePreference clientSidePreference = service.propagatedClientSidePreference;
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


}
