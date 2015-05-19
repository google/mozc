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

import org.mozc.android.inputmethod.japanese.FeedbackManager.FeedbackEvent;
import org.mozc.android.inputmethod.japanese.FeedbackManager.FeedbackListener;
import org.mozc.android.inputmethod.japanese.HardwareKeyboard.CompositionSwitchMode;
import org.mozc.android.inputmethod.japanese.JapaneseKeyboard.KeyboardSpecification;
import org.mozc.android.inputmethod.japanese.KeycodeConverter.KeyEventInterface;
import org.mozc.android.inputmethod.japanese.SymbolInputView.MajorCategory;
import org.mozc.android.inputmethod.japanese.emoji.EmojiProviderType;
import org.mozc.android.inputmethod.japanese.model.SelectionTracker;
import org.mozc.android.inputmethod.japanese.model.SymbolCandidateStorage.SymbolHistoryStorage;
import org.mozc.android.inputmethod.japanese.mushroom.MushroomResultProxy;
import org.mozc.android.inputmethod.japanese.preference.ClientSidePreference;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates;
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
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.SessionCommand;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoConfig.Config;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoConfig.Config.SelectionShortcut;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoConfig.Config.SessionKeymap;
import org.mozc.android.inputmethod.japanese.resources.R;
import org.mozc.android.inputmethod.japanese.session.SessionExecutor;
import org.mozc.android.inputmethod.japanese.session.SessionHandlerFactory;
import org.mozc.android.inputmethod.japanese.util.ImeSwitcherFactory;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;
import com.google.protobuf.ByteString;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.content.res.Configuration;
import android.graphics.Color;
import android.inputmethodservice.InputMethodService;
import android.media.AudioManager;
import android.os.Build;
import android.os.Handler;
import android.os.Message;
import android.os.Vibrator;
import android.preference.PreferenceManager;
import android.text.InputType;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.style.BackgroundColorSpan;
import android.text.style.CharacterStyle;
import android.text.style.ForegroundColorSpan;
import android.text.style.UnderlineSpan;
import android.util.Log;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputBinding;
import android.view.inputmethod.InputConnection;

import java.util.ArrayList;
import java.util.Collections;
import java.util.EnumMap;
import java.util.List;
import java.util.Map;

import javax.annotation.Nullable;


/**
 * The input method service.
 *
 */
public class MozcService extends InputMethodService {

  /**
   * InputMethod implementation for MozcService.
   * This injects the composing text tracking feature by wrapping InputConnection.
   */
  public class MozcInputMethod extends InputMethodService.InputMethodImpl {
    @Override
    public void bindInput(InputBinding binding) {
      binding = new InputBinding(
          ComposingTextTrackingInputConnection.newInstance(binding.getConnection()),
          binding.getConnectionToken(),
          binding.getUid(),
          binding.getPid());
      super.bindInput(binding);
    }

    @Override
    public void startInput(InputConnection inputConnection, EditorInfo attribute) {
      super.startInput(
          ComposingTextTrackingInputConnection.newInstance(inputConnection), attribute);
    }

    @Override
    public void restartInput(InputConnection inputConnection, EditorInfo attribute) {
      super.restartInput(
          ComposingTextTrackingInputConnection.newInstance(inputConnection), attribute);
    }
  }

  private static class RealFeedbackListener implements FeedbackListener {
    private final Vibrator vibrator;
    private final AudioManager audioManager;

    private RealFeedbackListener(Vibrator vibrator, AudioManager audioManager) {
      if (vibrator == null) {
        MozcLog.w("vibrator must be non-null. Vibration is disabled.");
      }
      this.vibrator = vibrator;
      if (audioManager == null) {
        MozcLog.w("audioManager must be non-null. Sound feedback is disabled.");
      }
      this.audioManager = audioManager;
    }

    @Override
    public void onVibrate(long duration) {
      if (duration < 0) {
        MozcLog.w("duration must be >= 0 but " + duration);
        return;
      }
      if (vibrator != null) {
        vibrator.vibrate(duration);
      }
    }

    @Override
    public void onSound(int soundEffectType, float volume) {
      if (audioManager != null && soundEffectType != FeedbackManager.FeedbackEvent.NO_SOUND) {
        audioManager.playSoundEffect(soundEffectType, volume);
      }
    }
  }

  /** Adapter implementation of the symbol history manipulation. */
  static class SymbolHistoryStorageImpl implements SymbolHistoryStorage {
    static final Map<MajorCategory, StorageType> STORAGE_TYPE_MAP;
    static {
      Map<MajorCategory, StorageType> map =
          new EnumMap<MajorCategory, StorageType>(MajorCategory.class);
      map.put(MajorCategory.SYMBOL, StorageType.SYMBOL_HISTORY);
      map.put(MajorCategory.EMOTICON, StorageType.EMOTICON_HISTORY);
      map.put(MajorCategory.EMOJI, StorageType.EMOJI_HISTORY);
      STORAGE_TYPE_MAP = Collections.unmodifiableMap(map);
    }

    private final SessionExecutor sessionExecutor;

    SymbolHistoryStorageImpl(SessionExecutor sessionExecutor) {
      this.sessionExecutor = sessionExecutor;
    }

    @Override
    public List<String> getAllHistory(MajorCategory majorCategory) {
      List<ByteString> historyList =
          sessionExecutor.readAllFromStorage(STORAGE_TYPE_MAP.get(majorCategory));
      List<String> result = new ArrayList<String>(historyList.size());
      for (ByteString value : historyList) {
        result.add(MozcUtil.utf8CStyleByteStringToString(value));
      }
      return result;
    }

    @Override
    public void addHistory(MajorCategory majorCategory, String value) {
      sessionExecutor.insertToStorage(
          STORAGE_TYPE_MAP.get(majorCategory),
          value,
          Collections.singletonList(ByteString.copyFromUtf8(value)));
    }
  }

  // Called back from ViewManager
  // Package private for testing.
  class MozcEventListener implements ViewEventListener {
    @Override
    public void onConversionCandidateSelected(int candidateId) {
      sessionExecutor.submitCandidate(candidateId, renderResultCallback);
      feedbackManager.fireFeedback(FeedbackEvent.CANDIDATE_SELECTED);
    }

    @Override
    public void onSymbolCandidateSelected(MajorCategory majorCategory, String candidate) {
      // Directly commit the text.
      commitText(candidate);

      symbolHistoryStorage.addHistory(majorCategory, candidate);
      feedbackManager.fireFeedback(FeedbackEvent.CANDIDATE_SELECTED);
    }

    private void commitText(String text) {
      InputConnection inputConnection = getCurrentInputConnection();
      if (inputConnection == null) {
        return;
      }
      inputConnection.beginBatchEdit();
      try {
        inputConnection.commitText(text, MozcUtil.CURSOR_POSITION_TAIL);
      } finally {
        inputConnection.endBatchEdit();
      }
    }

    @Override
    public void onKeyEvent(
        ProtoCommands.KeyEvent mozcKeyEvent, KeyEventInterface keyEvent,
        KeyboardSpecification keyboardSpecification, List<? extends TouchEvent> touchEventList) {
      if (mozcKeyEvent == null && keyboardSpecification == null) {
        // We don't send a key event to Mozc native layer since {@code mozcKeyEvent} is null, and we
        // don't need to update the keyboard specification since {@code keyboardSpecification} is
        // also null.
        if (keyEvent == null) {
          // Send a usage information to Mozc native layer.
          sessionExecutor.usageStatsEvent(touchEventList);
        } else {
          // Send a key event (which is generated by Mozc in the usual case) to application.
          Preconditions.checkArgument(touchEventList.isEmpty());
          sessionExecutor.sendKeyEvent(keyEvent, sendKeyToApplicationCallback);
        }
        return;
      }

      sendKeyWithKeyboardSpecification(mozcKeyEvent, keyEvent,
                                       keyboardSpecification, getResources().getConfiguration(),
                                       touchEventList);
    }

    @Override
    public void onUndo(List<? extends TouchEvent> touchEventList) {
      sessionExecutor.undoOrRewind(touchEventList, renderResultCallback);
    }

    @Override
    public void onFireFeedbackEvent(FeedbackEvent event) {
      feedbackManager.fireFeedback(event);
    }

    @Override
    public void onSubmitPreedit() {
      sessionExecutor.submit(renderResultCallback);
    }

    @Override
    public void onExpandSuggestion() {
      sessionExecutor.expandSuggestion(renderResultCallback);
    }

    @Override
    public void onShowMenuDialog(List<? extends TouchEvent> touchEventList) {
      sessionExecutor.usageStatsEvent(touchEventList);
    }

    @Override
    public void onShowSymbolInputView(List<? extends TouchEvent> touchEventList) {
      // Send request with (only) keyboard name for logging usage stats.
      sessionExecutor.updateRequest(
          MozcUtil.getRequestForKeyboard(
              SymbolInputView.SPEC_NAME, null, null, null, null, getResources().getConfiguration()),
          touchEventList);
    }

    @Override
    public void onCloseSymbolInputView() {
      KeyboardSpecification specification = viewManager.getJapaneseKeyboardSpecification();
      sessionExecutor.updateRequest(
          MozcUtil.getRequestForKeyboard(
              specification.getKeyboardSpecificationName(),
              specification.getSpecialRomanjiTable(),
              specification.getSpaceOnAlphanumeric(),
              specification.isKanaModifierInsensitiveConversion(),
              specification.getCrossingEdgeBehavior(),
              getResources().getConfiguration()),
          Collections.<TouchEvent>emptyList());
    }

    @Override
    public void onHardwareKeyboardCompositionModeChange(CompositionSwitchMode mode) {
      CompositionMode oldMode = hardwareKeyboard.getCompositionMode();
      hardwareKeyboard.setCompositionMode(mode);
      CompositionMode newMode = hardwareKeyboard.getCompositionMode();
      if (oldMode != newMode) {
        viewManager.setHardwareKeyboardCompositionMode(newMode);
        sendKeyWithKeyboardSpecification(
            null, null,
            hardwareKeyboard.getKeyboardSpecification(),
            getResources().getConfiguration(),
            Collections.<TouchEvent>emptyList());
      }
    }

    @Override
    public void onActionKey() {
      // false means that the key is for Action and not ENTER.
      sendDefaultEditorAction(false);
    }
  }

  /**
   * Callback to render the result received from Mozc server.
   */
  private class RenderResultCallback implements SessionExecutor.EvaluationCallback {

    @Override
    public void onCompleted(Command command, @Nullable KeyEventInterface triggeringKeyEvent) {
      Preconditions.checkNotNull(command);
      Output output = command.getOutput();
      Input input = command.getInput();
      if (input.getCommand().getType() != SessionCommand.CommandType.EXPAND_SUGGESTION) {
        // For expanding suggestions, we don't need to update our rendering result.
        renderInputConnection(output, triggeringKeyEvent);
      }
      viewManager.render(command);
    }
  }

  /**
   * Callback to send key event to a application.
   */
  private class SendKeyToApplicationCallback implements SessionExecutor.EvaluationCallback {

    @Override
    public void onCompleted(@Nullable Command command,
                            @Nullable KeyEventInterface triggeringKeyEvent) {
      Preconditions.checkArgument(command == null);
      sendKeyEvent(triggeringKeyEvent);
    }
  }

  /**
   * Callback to send key event to view layer.
   */
  private class SendKeyToViewCallback implements SessionExecutor.EvaluationCallback {

    @Override
    public void onCompleted(@Nullable Command command, KeyEventInterface triggeringKeyEvent) {
      Preconditions.checkArgument(command == null);
      Preconditions.checkNotNull(triggeringKeyEvent);
      viewManager.consumeKeyOnViewSynchronously(triggeringKeyEvent.getNativeEvent());
    }
  }

  /**
   * Callback to invoke onUpdateSelectionInternal with delay for onConfigurationChanged.
   * See onConfigurationChanged for the details.
   */
  private class ConfigurationChangeCallback implements Handler.Callback {
    @Override
    public boolean handleMessage(Message msg) {
      int selectionStart = msg.arg1;
      int selectionEnd = msg.arg2;
      onUpdateSelectionInternal(selectionStart, selectionEnd, selectionStart, selectionEnd, -1, -1);
      return true;
    }
  }

  /**
   * We need to send SYNC_DATA command periodically. This class handles it.
   */
  private class SendSyncDataCommandHandler extends Handler {
    /**
     * The current period of sending SYNC_DATA is 15 mins (as same as desktop version).
     */
    static final int SYNC_DATA_COMMAND_PERIOD = 15 * 60 * 1000;

    @Override
    public void handleMessage(Message msg) {
      if (sessionExecutor != null) {
        sessionExecutor.syncData();
      }
      sendEmptyMessageDelayed(0, SYNC_DATA_COMMAND_PERIOD);
    }
  }

  /**
   * To trim memory, a message is handled to invoke trimMemory method
   * 10 seconds after hiding window.
   *
   * This class handles callback operation.
   * Posting and removing messages should be done in appropriate point.
   */
  private class MemoryTrimmingHandler extends Handler {

    /**
     * "what" value of message. Always use this.
     */
    static final int WHAT = 0;

    /**
     * Duration after hiding window in milliseconds.
     */
    static final int DURATION_MS = 10 * 1000;

    @Override
    public void handleMessage(Message msg) {
      trimMemory();
      // Other messages in the queue are removed as they will do the same thing
      // and will affect nothing.
      removeMessages(WHAT);
    }
  }


  // Keys for tweak preferences.
  private static final String PREF_TWEAK_PREFIX = "pref_tweak_";
  private static final String PREF_TWEAK_LOGGING_PROTOCOL_BUFFERS =
      "pref_tweak_logging_protocol_buffers";

  // Foreground color of characters which also have background color span.
  // Without this span default color (specified by the app) is used so
  // if default color is similar to specified background color the characters are hard to be read.
  @VisibleForTesting static final CharacterStyle SPAN_FOREGROUND_COLOR =
      new ForegroundColorSpan(Color.DKGRAY);

  // Focused segment's attribute.
  @VisibleForTesting static final CharacterStyle SPAN_CONVERT_HIGHLIGHT =
      new BackgroundColorSpan(0xFF88FFFF);

  // Cursor position.
  // Note that InputConnection seems not to be able to show cursor. This is a workaround.
  @VisibleForTesting static final CharacterStyle SPAN_BEFORE_CURSOR =
      new BackgroundColorSpan(0xFFFFAAFF);

  private static final CharacterStyle SPAN_PARTIAL_SUGGESTION_COLOR =
      new BackgroundColorSpan(0xFFFFE0E0);

  // Underline.
  @VisibleForTesting static final CharacterStyle SPAN_UNDERLINE = new UnderlineSpan();

  // Mozc's session. All session related task should be done via this instance.
  @VisibleForTesting SessionExecutor sessionExecutor;
  @VisibleForTesting final RenderResultCallback renderResultCallback = new RenderResultCallback();
  private final SendKeyToApplicationCallback sendKeyToApplicationCallback =
      new SendKeyToApplicationCallback();
  private final SendKeyToViewCallback sendKeyToViewCallback = new SendKeyToViewCallback();

  // A manager for all views and feedbacks.
  @VisibleForTesting
  public ViewManagerInterface viewManager;
  @VisibleForTesting FeedbackManager feedbackManager;
  @VisibleForTesting SymbolHistoryStorage symbolHistoryStorage;

  @VisibleForTesting SharedPreferences sharedPreferences;

  // A handler for onSharedPreferenceChanged().
  // Note: the handler is needed to be held by the service not to be GC'ed.
  @VisibleForTesting final OnSharedPreferenceChangeListener sharedPreferenceChangeListener =
      new SharedPreferenceChangeAdapter();

  // Preference information which are propagated. Null if not propagated yet.
  @VisibleForTesting ClientSidePreference propagatedClientSidePreference = null;

  // Track the selection.
  @VisibleForTesting SelectionTracker selectionTracker = new SelectionTracker();

  // A receiver to accept a notification via intents.
  @VisibleForTesting Handler configurationChangedHandler =
      new Handler(new ConfigurationChangeCallback());

  // Handler to process SYNC_DATA command for storing history data.
  @VisibleForTesting Handler sendSyncDataCommandHandler = new SendSyncDataCommandHandler();

  // Handler to process SYNC_DATA command for storing history data.
  private final Handler memoryTrimmingHandler = new MemoryTrimmingHandler();

  // This is a cache of MozcUtil.isDebug. It will be set in onCreateInternal and
  // will never be changed.
  private boolean isDebugBuild;

  // Current KeyboardSpecification, which is determined by the last key event.
  // Note that this might be different from what a user sees.
  // For example when a user is in narrow mode (this field is for H/W keyboard)
  // and (s)he taps widen button to see S/W keyboard,
  // (s)he will see S/W keyboard but this field keep to point H/W keyboard because
  // widen button is not a key event.
  // This behavior is error-prone and might be a kind of bug. At least the name doesn't represent
  // the behavior.
  // TODO(matsuzakit): Clarify the usage of this field (change the behavior to keep the latest
  //   state or change the name to represent current behavior).
  @VisibleForTesting KeyboardSpecification currentKeyboardSpecification =
      KeyboardSpecification.TWELVE_KEY_TOGGLE_KANA;

  // Non-final for testing
  // TODO(matsuzakit): Setting this in onCreateInternal might be more consistent.
  @VisibleForTesting HardwareKeyboard hardwareKeyboard = new HardwareKeyboard();

  // Current HardKeyboardHidden configuration value.
  // This is updated only when onConfigurationChanged is called and
  // Configuration.HARDKEYBOARDHIDDEN_* differs to this.
  private int currentHardKeyboardHidden = Configuration.HARDKEYBOARDHIDDEN_UNDEFINED;

  @VisibleForTesting boolean inputBound = false;

  private ApplicationCompatibility applicationCompatibility =
      ApplicationCompatibility.getDefaultInstance();

  @Override
  public void onBindInput() {
    super.onBindInput();
    inputBound = true;
  }

  @Override
  public void onUnbindInput() {
    inputBound = false;
    super.onUnbindInput();
  }

  @Override
  public MozcInputMethod onCreateInputMethodInterface() {
    return new MozcInputMethod();
  }

  @Override
  public void onCreate() {
    // Note: super.onCreate() is invoked in onCreateInternal. So, do not call it, here.
    MozcLog.d("start MozcService#onCreate " + System.nanoTime());

    // TODO(hidehiko): Restructure around initialization code in order to make tests stable.
    // Callback object mainly used by views.
    MozcEventListener eventListener = new MozcEventListener();
    SharedPreferences sharedPreferences = PreferenceManager.getDefaultSharedPreferences(this);
    SessionExecutor sessionExecutor =
        SessionExecutor.getInstanceInitializedIfNecessary(
            new SessionHandlerFactory(sharedPreferences), this);
    onCreateInternal(eventListener, null, sharedPreferences, getResources().getConfiguration(),
                     sessionExecutor);

    MozcLog.d("end MozcService#onCreate " + System.nanoTime());
  }

  @Override
  public void onDestroy() {
    feedbackManager.release();
    if (sessionExecutor != null) {
      sessionExecutor.syncData();
    }
    super.onDestroy();
  }

  @VisibleForTesting
  void onCreateInternal(ViewEventListener eventListener, ViewManagerInterface viewManager,
                        SharedPreferences sharedPreferences, Configuration deviceConfiguration,
                        SessionExecutor sessionExecutor) {
    super.onCreate();

    Context context = getApplicationContext();
    isDebugBuild = MozcUtil.isDebug(context);

    // TODO(hidehiko): Split following methods by functionalities to improve test coverage and
    //   test stableness.
    this.sessionExecutor = sessionExecutor;
    this.symbolHistoryStorage = new SymbolHistoryStorageImpl(sessionExecutor);
    prepareOnce(eventListener, symbolHistoryStorage, viewManager, sharedPreferences);
    prepareEveryTime(sharedPreferences, deviceConfiguration);

    if (propagatedClientSidePreference == null ||
        propagatedClientSidePreference.getHardwareKeyMap() == null) {
      HardwareKeyboardSpecification.maybeSetDetectedHardwareKeyMap(
          sharedPreferences, deviceConfiguration, false);
    }

    // Start sending SYNC_DATA message to mozc server periodically.
    sendSyncDataCommandHandler.sendEmptyMessageDelayed(
        0, SendSyncDataCommandHandler.SYNC_DATA_COMMAND_PERIOD);
    this.sharedPreferences = sharedPreferences;
  }

  /**
   * Prepares something which should be done every time when the session is newly created.
   */
  private void prepareEveryTime(
      SharedPreferences sharedPreferences, Configuration deviceConfiguration) {
    boolean isLogging = sharedPreferences != null &&
            sharedPreferences.getBoolean(PREF_TWEAK_LOGGING_PROTOCOL_BUFFERS, false);
    // Force to initialize here.
    sessionExecutor.reset(new SessionHandlerFactory(sharedPreferences), this);
    sessionExecutor.setLogging(isLogging);

    updateImposedConfig(getResources().getConfiguration());
    viewManager.onConfigurationChanged(getResources().getConfiguration());
    // Make sure that the server and the client have the same keyboard specification.
    // User preference's keyboard will be set after this step.
    changeKeyboardSpecificationAndSendKey(
        null, null, currentKeyboardSpecification, deviceConfiguration,
        Collections.<TouchEvent>emptyList());
    if (sharedPreferences != null) {
      propagateClientSidePreference(
          new ClientSidePreference(sharedPreferences, deviceConfiguration));
      // TODO(hidehiko): here we just set the config based on preferences. When we start
      //   to support sync on Android, we need to revisit the config related design.
      sessionExecutor.setConfig(ConfigUtil.toConfig(sharedPreferences));
    }

    maybeSetNarrowMode(deviceConfiguration);
  }

  /**
   * Prepares something which should be done only once.
   */
  private void prepareOnce(ViewEventListener eventListener,
      SymbolHistoryStorage symbolHistoryStorage,
      ViewManagerInterface viewManager,
      SharedPreferences sharedPreferences) {
    Context context = getApplicationContext();
    boolean omitWelcomeActivity = false;
    Optional<Intent> forwardIntent =
        ApplicationInitializerFactory.createInstance(this).initialize(
            omitWelcomeActivity,
            MozcUtil.isDevChannel(context),
            DependencyFactory.getDependency(getApplicationContext()).isWelcomeActivityPreferrable(),
            MozcUtil.getAbiIndependentVersionCode(context));
    if (forwardIntent.isPresent()) {
      startActivity(forwardIntent.get());
    }

    // Create a ViewManager.
    if (viewManager == null) {
      viewManager = DependencyFactory.getDependency(
          getApplicationContext()).createViewManager(
              getApplicationContext(),
              eventListener,
              symbolHistoryStorage,
              ImeSwitcherFactory.getImeSwitcher(this),
              new MozcMenuDialogListenerImpl(this));
    }

    // Setup FeedbackManager.
    feedbackManager = new FeedbackManager(new RealFeedbackListener(
        Vibrator.class.cast(getSystemService(Context.VIBRATOR_SERVICE)),
        AudioManager.class.cast(getSystemService(Context.AUDIO_SERVICE))));

    this.viewManager = viewManager;

    // Set a callback for preference changing.
    if (sharedPreferences != null) {
      sharedPreferences.registerOnSharedPreferenceChangeListener(sharedPreferenceChangeListener);
    }
  }

  @Override
  public boolean onEvaluateInputViewShown() {
    // TODO(matsuzakit): Implement me
    return true;
  }

  @Override
  public View onCreateInputView() {
    MozcLog.d("start MozcService#onCreateInputView " + System.nanoTime());
    View inputView = viewManager.createMozcView(this);
    MozcLog.d("end MozcService#onCreateInputView " + System.nanoTime());
    return inputView;
  }

  void resetContext() {
    if (sessionExecutor != null) {
      sessionExecutor.resetContext();
    }
    if (viewManager != null) {
      viewManager.reset();
    }
  }

  @Override
  public void onFinishInput() {
    // Omit rendering because the input view will soon disappear.
    resetContext();
    selectionTracker.onFinishInput();
    applicationCompatibility = ApplicationCompatibility.getDefaultInstance();
    super.onFinishInput();
  }

  @Override
  public void onStartInput(EditorInfo attribute, boolean restarting) {
    super.onStartInput(attribute, restarting);

    applicationCompatibility = ApplicationCompatibility.getInstance(attribute);

    // Update full screen mode, because the application may be changed.
    viewManager.setFullscreenMode(
        applicationCompatibility.isFullScreenModeSupported() &&
        propagatedClientSidePreference != null &&
        propagatedClientSidePreference.isFullscreenMode());

    // Some applications, e.g. gmail or maps, send onStartInput with restarting = true, when a user
    // rotates a device. In such cases, we don't want to update caret positions, nor reset
    // the context basically. However, some other applications, such as one with a webview widget
    // like a browser, send onStartInput with restarting = true, too. Unfortunately,
    // there seems no way to figure out which one causes this invocation.
    // So, as a point of compromise, we reset the context everytime here. Also, we'll send
    // finishComposingText as well, in case the new attached field has already had composing text
    // (we hit such a situation on webview, too).
    // See also onConfigurationChanged for caret position handling on gmail-like applications'
    // device rotation events.
    resetContext();
    InputConnection connection = getCurrentInputConnection();
    if (connection != null) {
      connection.finishComposingText();
      maybeCommitMushroomResult(attribute, connection);
    }

    // Send the connected field's attributes to the mozc server.
    sessionExecutor.switchInputFieldType(getInputFieldType(attribute));
    sessionExecutor.updateRequest(
        MozcUtil.createEmojiRequest(
            Build.VERSION.SDK_INT,
            (propagatedClientSidePreference != null && MozcUtil.isEmojiAllowed(attribute))
                ? propagatedClientSidePreference.getEmojiProviderType() : EmojiProviderType.NONE),
        Collections.<TouchEvent>emptyList());
    selectionTracker.onStartInput(
        attribute.initialSelStart, attribute.initialSelEnd, isWebEditText(attribute));
  }

  /**
   * Hook to support mushroom protocol. If there is pending Mushroom result for the connecting
   * field, commit it. Then, (regardless of whether there exists pending result,) clears
   * all remaining pending result.
   */
  static void maybeCommitMushroomResult(EditorInfo attribute, InputConnection connection) {
    if (connection == null) {
      return;
    }

    MushroomResultProxy resultProxy = MushroomResultProxy.getInstance();
    String result;
    synchronized (resultProxy) {
      // We need to obtain the result and then clear the all remaining result atomically.
      result = resultProxy.getReplaceKey(attribute.fieldId);
      resultProxy.clear();
    }
    if (result != null) {
      // Found the pending mushroom application result to the connecting field. Commit it.
      connection.commitText(result, MozcUtil.CURSOR_POSITION_TAIL);
    }
  }

  /**
   * @return true if connected view is WebEditText (or the application pretends it)
   */
  boolean isWebEditText(EditorInfo editorInfo) {
    if (editorInfo == null) {
      return false;
    }

    if (applicationCompatibility.isPretendingWebEditText()) {
      return true;
    }

    // TODO(hidehiko): Refine the heuristic to check isWebEditText related stuff.
    MozcLog.d("inputType: " + editorInfo.inputType);
    int variation = editorInfo.inputType & InputType.TYPE_MASK_VARIATION;
    return variation == InputType.TYPE_TEXT_VARIATION_WEB_EDIT_TEXT;
  }

  @Override
  public void onStartInputView(EditorInfo attribute, boolean restarting) {
    viewManager.setTextForActionButton(getTextForImeAction(attribute.imeOptions));
    viewManager.setEditorInfo(attribute);
  }

  static InputFieldType getInputFieldType(EditorInfo attribute) {
    int inputType = attribute.inputType;
    int inputClass = inputType & InputType.TYPE_MASK_CLASS;
    int inputVariation = inputType & InputType.TYPE_MASK_VARIATION;
    if (inputClass == InputType.TYPE_CLASS_TEXT &&
        (inputVariation == InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD ||
           inputVariation == InputType.TYPE_TEXT_VARIATION_PASSWORD ||
           inputVariation == InputType.TYPE_TEXT_VARIATION_WEB_PASSWORD)) {
      return InputFieldType.PASSWORD;
    }
    if (inputClass == InputType.TYPE_CLASS_PHONE) {
      return InputFieldType.TEL;
    }
    if (inputClass == InputType.TYPE_CLASS_NUMBER) {
      return InputFieldType.NUMBER;
    }
    return InputFieldType.NORMAL;
  }

  @Override
  public void onComputeInsets(InputMethodService.Insets outInsets) {
    viewManager.computeInsets(getApplicationContext(), outInsets, getWindow().getWindow());
  }

  /**
   * KeyDown event handler.
   *
   * This method is called only by the android framework e.g HOME,BACK or H/W keyboard input.
   */
  @Override
  public boolean onKeyDown(int keyCode, KeyEvent event) {
    return onKeyDownInternal(keyCode, event, getResources().getConfiguration());
  }

  /**
   * GenericMotionEvent handler.
   *
   * This method is called only by the android framework e.g H/W mouse, touch pad input, etc.
   */
  @TargetApi(17)
  @Override
  public boolean onGenericMotionEvent(MotionEvent event) {
    if (!isInputViewShown()) {
      return super.onGenericMotionEvent(event);
    }
    if (viewManager.isGenericMotionToConsume(event)) {
      return viewManager.consumeGenericMotion(event);
    }
    return super.onGenericMotionEvent(event);
  }

  boolean onKeyDownInternal(int keyCode, KeyEvent event, Configuration configuration) {
    if (MozcLog.isLoggable(Log.DEBUG)) {
      MozcLog.d(
          String.format(
              "onKeyDown keyCode:0x%x, metaState:0x%x, scanCode:0x%x, uniCode:0x%x, deviceId:%d",
              event.getKeyCode(), event.getMetaState(), event.getScanCode(),
              event.getUnicodeChar(), event.getDeviceId()));
    }

    if (isInputViewShown()) {
      if (KeycodeConverter.isSystemKey(event)) {
        // Special handle for back key. We need to post it to the server and maybeProcessBackKey
        // should handle it later. The posting the event is done in onKeyUp, so we just consume the
        // down key event here.
        if (keyCode == KeyEvent.KEYCODE_BACK) {
          return true;
        }
      } else {
        if (viewManager.isKeyConsumedOnViewAsynchronously(event)) {
          sessionExecutor.sendKeyEvent(KeycodeConverter.getKeyEventInterface(event),
                                       sendKeyToViewCallback);
          return true;
        }

        // TODO(yoichio): Refactor KeycodeConverter.
        ProtoCommands.KeyEvent mozcSpecialKeyEvent = KeycodeConverter.getMozcSpecialKeyEvent(event);
        // When event is special like BACK.
        if (mozcSpecialKeyEvent != null) {
          sendKeyWithKeyboardSpecification(
              mozcSpecialKeyEvent, KeycodeConverter.getKeyEventInterface(event),
              currentKeyboardSpecification,
              getResources().getConfiguration(),
              Collections.<TouchEvent>emptyList());
          return true;
        }

        if (propagatedClientSidePreference == null ||
            propagatedClientSidePreference.getHardwareKeyMap() == null) {
          HardwareKeyboardSpecification.maybeSetDetectedHardwareKeyMap(
              sharedPreferences, getResources().getConfiguration(), true);
        }

        if (hardwareKeyboard.isKeyToConsume(event)) {
          if (!viewManager.isNarrowMode()) {
            viewManager.hideSubInputView();
            viewManager.setNarrowMode(true);
          }

          CompositionMode compositionMode = hardwareKeyboard.getCompositionMode();
          hardwareKeyboard.setCompositionModeByKey(event);
          CompositionMode currentCompositionMode = hardwareKeyboard.getCompositionMode();
          if (currentCompositionMode != compositionMode) {
            viewManager.setHardwareKeyboardCompositionMode(currentCompositionMode);
          }
          sendKeyWithKeyboardSpecification(
              hardwareKeyboard.getMozcKeyEvent(event), hardwareKeyboard.getKeyEventInterface(event),
              hardwareKeyboard.getKeyboardSpecification(), configuration,
              Collections.<TouchEvent>emptyList());
          return true;
        }
      }
    }

    // If event does not consume the mozc event,
    // delegate to the super class.
    return super.onKeyDown(keyCode, event);
  }

  @Override
  public boolean onKeyUp(int keyCode, KeyEvent event) {
    if (isInputViewShown()) {
      if (KeycodeConverter.isSystemKey(event)) {
        // The back key should be processed as same as the meta keys.
        // See also comments described below.
        if (event.getKeyCode() == KeyEvent.KEYCODE_BACK) {
          sessionExecutor.sendKeyEvent(
              KeycodeConverter.getKeyEventInterface(event), sendKeyToApplicationCallback);
          return true;
        }
      } else {
        if (viewManager.isKeyConsumedOnViewAsynchronously(event)) {
          sessionExecutor.sendKeyEvent(KeycodeConverter.getKeyEventInterface(event),
                                       sendKeyToViewCallback);
          return true;
        }

        // The IME is active and the server can handle the event so consume the event.
        // Currently the server does not consume UP event for not meta keys.
        // For meta keys, to guarantee that this up event is sent to InputConnection after down key,
        // this is sent to evaluation handler.
        if (KeycodeConverter.isMetaKey(event)) {
          sessionExecutor.sendKeyEvent(KeycodeConverter.getKeyEventInterface(event),
                                       sendKeyToApplicationCallback);
          return true;
        }

        if (KeycodeConverter.getMozcSpecialKeyEvent(event) != null
            || hardwareKeyboard.isKeyToConsume(event)) {
          return true;
        }
      }
    }

    // If the IME is turned off or the event should not be sent to the server,
    // delegate to the super class.
    // Note that delegation should be done only when needed.
    // For example hardware keyboard's enter key should not be delegated
    // because its DOWN event is sent to the sever.
    // If delegated, enter key event is sent to the application twice (DOWN and UP).
    return super.onKeyUp(keyCode, event);
  }

  /**
   * Sends mozcKeyEvent and/or Request to mozc server.
   *
   * This skips to send request if the given keyboard specification is same as before.
   */
  boolean sendKeyWithKeyboardSpecification(
      ProtoCommands.KeyEvent mozcKeyEvent, KeyEventInterface event,
      KeyboardSpecification keyboardSpecification, Configuration configuration,
      List<? extends TouchEvent> touchEventList) {
    if (currentKeyboardSpecification == keyboardSpecification) {
      if (mozcKeyEvent != null) {
        // Send mozcKeyEvent as usual.
        sessionExecutor.sendKey(mozcKeyEvent, event, touchEventList, renderResultCallback);
        return true;
      }
    } else {
      changeKeyboardSpecificationAndSendKey(
          mozcKeyEvent, event, keyboardSpecification, configuration, touchEventList);
      updateStatusIcon();
      return true;
    }
    // Not consumed.
    return false;
  }

  /**
   * Sends Request for changing keybaord setting to mozc server and sends key.
   */
  private void changeKeyboardSpecificationAndSendKey(
      ProtoCommands.KeyEvent mozcKeyEvent, KeyEventInterface event,
      KeyboardSpecification keyboardSpecification, Configuration configuration,
      List<? extends TouchEvent> touchEventList) {
    // Send Request to change composition table.
    sessionExecutor.updateRequest(
        MozcUtil.getRequestForKeyboard(
            keyboardSpecification.getKeyboardSpecificationName(),
            keyboardSpecification.getSpecialRomanjiTable(),
            keyboardSpecification.getSpaceOnAlphanumeric(),
            keyboardSpecification.isKanaModifierInsensitiveConversion(),
            keyboardSpecification.getCrossingEdgeBehavior(),
            configuration),
        touchEventList);
    if (mozcKeyEvent == null) {
      // Change composition mode.
      sessionExecutor.switchInputMode(keyboardSpecification.getCompositionMode());
    } else {
      // Send key with composition mode change.
      sessionExecutor.sendKey(
          ProtoCommands.KeyEvent.newBuilder(mozcKeyEvent)
          .setMode(keyboardSpecification.getCompositionMode()).build(),
          event, touchEventList, renderResultCallback);
    }
    currentKeyboardSpecification = keyboardSpecification;
  }

  /**
   * Shows/Hides status icon according to the input view status.
   */
  private void updateStatusIcon() {
    if (isInputViewShown()) {
      showStatusIcon();
    } else {
      hideStatusIcon();
    }
  }

  /**
   * Shows the status icon basing on the current keyboard spec.
   */
  void showStatusIcon() {
    switch (currentKeyboardSpecification.getCompositionMode()) {
      case HIRAGANA:
        showStatusIcon(R.drawable.status_icon_hiragana);
        break;
      default:
        showStatusIcon(R.drawable.status_icon_alphabet);
        break;
    }
  }

  @Override
  public boolean onEvaluateFullscreenMode() {
    return viewManager.isFullscreenMode();
  }

  @Override
  public void onWindowShown() {
    showStatusIcon();
    // Remove memory trimming message.
    memoryTrimmingHandler.removeMessages(MemoryTrimmingHandler.WHAT);
    // Ensure keyboard's request.
    // The session might be deleted by trimMemory caused by onWindowHidden.
    // Note that this logic must be placed *after* removing the messages in memoryTrimmingHandler.
    // Otherwise the session might be unexpectedly deleted and newly re-created one will be used
    // without appropriate request which is sent below.
    changeKeyboardSpecificationAndSendKey(
        null, null, currentKeyboardSpecification, getResources().getConfiguration(),
        Collections.<TouchEvent>emptyList());
  }

  @Override
  public void onWindowHidden() {
    // "Hiding IME's window" is very similar to "Turning off IME" for PC.
    // Thus
    // - Commiting composing text.
    // - Removing all pending messages.
    // - Resetting Mozc server
    // are needed.
    sessionExecutor.removePendingEvaluations();

    resetContext();
    selectionTracker.onWindowHidden();
    viewManager.reset();
    hideStatusIcon();
    // MemoryTrimmingHandler.DURATION_MS from now, memory trimming will be done.
    // If the window is shown before MemoryTrimmingHandler.DURATION_MS,
    // the message posted here will be removed.
    memoryTrimmingHandler.removeMessages(MemoryTrimmingHandler.WHAT);
    memoryTrimmingHandler.sendEmptyMessageDelayed(MemoryTrimmingHandler.WHAT,
                                                  MemoryTrimmingHandler.DURATION_MS);
    super.onWindowHidden();
  }

  /**
   * Updates InputConnection.
   *
   * @param output Output message. Rendering is based on this parameter.
   * @param keyEvent Trigger event for this calling. When direct input is
   *        needed, this event is sent to InputConnection.
   */
  void renderInputConnection(Output output, KeyEventInterface keyEvent) {
    InputConnection inputConnection = getCurrentInputConnection();
    if (inputConnection == null) {
      return;
    }

    if (!output.hasConsumed() || !output.getConsumed()) {
      maybeCommitText(output, inputConnection);
      sendKeyEvent(keyEvent);
      return;
    }

    // Here the key is consumed by the Mozc server.
    inputConnection.beginBatchEdit();
    try {
      maybeDeleteSurroundingText(output, inputConnection);
      maybeCommitText(output, inputConnection);
      setComposingText(output, inputConnection);
      maybeSetSelection(output, inputConnection);
      selectionTracker.onRender(
          output.hasDeletionRange() ? output.getDeletionRange() : null,
          output.hasResult() ? output.getResult().getValue() : null,
          output.hasPreedit() ? output.getPreedit() : null);
    } finally {
      inputConnection.endBatchEdit();
    }
  }

  static KeyEvent createKeyEvent(KeyEvent original, long eventTime, int action, int repeatCount) {
    return new KeyEvent(
        original.getDownTime(), MozcUtil.getUptimeMillis(), action, original.getKeyCode(),
        repeatCount, original.getMetaState(), original.getDeviceId(), original.getScanCode(),
        original.getFlags());
  }

  /**
   * Sends the {@code KeyEvent}, which is not consumed by the mozc server.
   */
  void sendKeyEvent(KeyEventInterface keyEvent) {
    if (keyEvent == null) {
      return;
    }

    int keyCode = keyEvent.getKeyCode();
    // Some keys have a potential to be consumed from mozc client.
    if (maybeProcessBackKey(keyCode) || maybeProcessActionKey(keyCode)) {
      // The key event is consumed.
      return;
    }

    // Following code is to fallback to target activity.
    KeyEvent nativeKeyEvent = keyEvent.getNativeEvent();
    InputConnection inputConnection = getCurrentInputConnection();

    if (nativeKeyEvent != null && inputConnection != null) {
      // Meta keys are from this.onKeyDown/Up so fallback each time.
      if (KeycodeConverter.isMetaKey(nativeKeyEvent)) {
        inputConnection.sendKeyEvent(createKeyEvent(
            nativeKeyEvent, MozcUtil.getUptimeMillis(),
            nativeKeyEvent.getAction(), nativeKeyEvent.getRepeatCount()));
        return;
      }

      // Other keys are from this.onKeyDown so create dummy Down/Up events.
      inputConnection.sendKeyEvent(createKeyEvent(
          nativeKeyEvent, MozcUtil.getUptimeMillis(), KeyEvent.ACTION_DOWN, 0));

      inputConnection.sendKeyEvent(createKeyEvent(
          nativeKeyEvent, MozcUtil.getUptimeMillis(), KeyEvent.ACTION_UP, 0));
      return;
    }

    // Otherwise, just delegates the key event to the connected application.
    sendDownUpKeyEvents(keyCode);
  }

  /**
   * @return true if the key event is consumed
   */
  private boolean maybeProcessBackKey(int keyCode) {
    if (keyCode != KeyEvent.KEYCODE_BACK || !isInputViewShown()) {
      return false;
    }

    // Special handling for back key event, to close the software keyboard or its subview.
    // First, try to hide the subview, such as the symbol input view or the cursor view.
    // If neither is shown, hideSubInputView would fail, then hide the whole software keyboard.
    if (!viewManager.hideSubInputView()) {
      requestHideSelf(0);
    }
    return true;
  }

  private boolean maybeProcessActionKey(int keyCode) {
    // Handle the event iff the enter is pressed.
    if (keyCode != KeyEvent.KEYCODE_ENTER || !isInputViewShown()) {
      return false;
    }

    // Fall back to EditorAction. Note that the keyCode is ENTER here, so set the fromEnterKey
    // argument true.
    return sendDefaultEditorAction(true);
  }

  static void maybeDeleteSurroundingText(Output output, InputConnection inputConnection) {
    if (!output.hasDeletionRange()) {
      return;
    }

    DeletionRange range = output.getDeletionRange();
    int leftRange = -range.getOffset();
    int rightRange = range.getLength() - leftRange;
    if (leftRange < 0 || rightRange < 0) {
      // If the range does not include the current position, do nothing
      // because Android's API does not expect such situation.
      MozcLog.w("Deletion range has unsupported parameters: " + range.toString());
      return;
    }

    if (!inputConnection.deleteSurroundingText(leftRange, rightRange)) {
      MozcLog.e("Failed to delete surrounding text.");
    }
  }

  static void maybeCommitText(Output output, InputConnection inputConnection) {
    if (!output.hasResult()) {
      return;
    }

    String outputText = output.getResult().getValue();
    if (outputText.equals("")) {
      // Do nothing for an empty result string.
      return;
    }

    int position = MozcUtil.CURSOR_POSITION_TAIL;
    if (output.getResult().hasCursorOffset()) {
      if (output.getResult().getCursorOffset() ==
          -outputText.codePointCount(0, outputText.length())) {
        position = MozcUtil.CURSOR_POSITION_HEAD;
      } else {
        MozcLog.e("Unsupported position: " + output.getResult().toString());
      }
    }

    if (!inputConnection.commitText(outputText, position)) {
      MozcLog.e("Failed to commit text.");
    }
  }

  void setComposingText(Output output, InputConnection inputConnection) {
    if (!output.hasPreedit()) {
      // Reset composing text.
      if (!inputConnection.setComposingText("", 0)) {
        MozcLog.e("Failed to set composing text.");
      }
      return;
    }

    // Builds preedit expression.
    Preedit preedit = output.getPreedit();

    SpannableStringBuilder builder = new SpannableStringBuilder();
    for (Segment segment : preedit.getSegmentList()) {
      builder.append(segment.getValue());
      if (segment.hasAnnotation() && segment.getAnnotation() == Annotation.HIGHLIGHT) {
        // Highlight for the focused conversion part.
        int end = builder.length();
        int begin = end - segment.getValue().length();
        builder.setSpan(SPAN_CONVERT_HIGHLIGHT, begin, end, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        builder.setSpan(SPAN_FOREGROUND_COLOR, begin, end, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
      }
    }

    // Set underline for all the preedit text.
    builder.setSpan(SPAN_UNDERLINE, 0, builder.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);

    // Draw cursor if in composition mode.
    int cursor = preedit.getCursor();
    if (!(output.hasAllCandidateWords() &&
          output.getAllCandidateWords().hasCategory() &&
          output.getAllCandidateWords().getCategory() == ProtoCandidates.Category.CONVERSION)) {
      // We cannot show system cursor inside preedit here.
      // Instead we change text style before the preedit's cursor.
      if (cursor != builder.length()) {
        // This condition is workaround not to show unexpected background color for EditText.
        builder.setSpan(SPAN_PARTIAL_SUGGESTION_COLOR, cursor, builder.length(),
            Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        builder.setSpan(SPAN_FOREGROUND_COLOR, cursor, builder.length(),
            Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
      }
      if (cursor > 0) {
        builder.setSpan(SPAN_BEFORE_CURSOR, 0, cursor, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        builder.setSpan(SPAN_FOREGROUND_COLOR, 0, cursor, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
      }
    }

    // System cursor will be moved to the tail of preedit.
    // It triggers onUpdateSelection again.
    int cursorPosition = cursor > 0 ? MozcUtil.CURSOR_POSITION_TAIL : 0;
    if (!inputConnection.setComposingText(builder, cursorPosition)) {
      MozcLog.e("Failed to set composing text.");
    }
  }

  void maybeSetSelection(Output output, InputConnection inputConnection) {
    if (!output.hasPreedit()) {
      return;
    }

    Preedit preedit = output.getPreedit();
    int cursor = preedit.getCursor();
    if (cursor == 0 || cursor == getPreeditLength(preedit)) {
      // The cursor is at the beginning/ending of the preedit. So we don't anything about the
      // caret setting.
      return;
    }

    int caretPosition = selectionTracker.getPreeditStartPosition();
    if (output.hasDeletionRange()) {
      caretPosition += output.getDeletionRange().getOffset();
    }
    if (output.hasResult()) {
      caretPosition += output.getResult().getValue().length();
    }
    if (output.hasPreedit()) {
      caretPosition += output.getPreedit().getCursor();
    }

    if (!inputConnection.setSelection(caretPosition, caretPosition)) {
      MozcLog.e("Failed to set selection.");
    }
  }

  private static int getPreeditLength(Preedit preedit) {
    int result = 0;
    for (int i = 0; i < preedit.getSegmentCount(); ++i) {
      result += preedit.getSegment(i).getValueLength();
    }
    return result;
  }

  /**
   * Propagates the preferences which affect client-side.
   *
   * If the previous parameter (this.clientSidePreference) is null,
   * all the fields in the latest parameter are propagated.
   * If not, only differences are propagated.
   *
   * After the execution, {@code this.propagatedClientSidePreference} is updated.
   *
   * @param newPreference the ClientSidePreference to be propagated
   */
  @VisibleForTesting void propagateClientSidePreference(ClientSidePreference newPreference) {
    // TODO(matsuzakit): Receive a Config to reflect the current device configuration.
    if (newPreference == null) {
      MozcLog.e("newPreference must be non-null. No update is performed.");
      return;
    }
    ClientSidePreference oldPreference = propagatedClientSidePreference;
    if (oldPreference == null ||
        oldPreference.isHapticFeedbackEnabled() != newPreference.isHapticFeedbackEnabled()) {
      feedbackManager.setHapticFeedbackEnabled(newPreference.isHapticFeedbackEnabled());
    }
    if (oldPreference == null ||
        oldPreference.getHapticFeedbackDuration() != newPreference.getHapticFeedbackDuration()) {
      feedbackManager.setHapticFeedbackDuration(newPreference.getHapticFeedbackDuration());
    }
    if (oldPreference == null ||
        oldPreference.isSoundFeedbackEnabled() != newPreference.isSoundFeedbackEnabled()) {
      feedbackManager.setSoundFeedbackEnabled(newPreference.isSoundFeedbackEnabled());
    }
    if (oldPreference == null ||
        oldPreference.getSoundFeedbackVolume() != newPreference.getSoundFeedbackVolume()) {
      // The default value is 0.1f. In order to set the 50 to the default value, divide the
      // preference value by 500f heuristically.
      feedbackManager.setSoundFeedbackVolume(newPreference.getSoundFeedbackVolume() / 500f);
    }
    if (oldPreference == null ||
        oldPreference.isPopupFeedbackEnabled() != newPreference.isPopupFeedbackEnabled()) {
      viewManager.setPopupEnabled(newPreference.isPopupFeedbackEnabled());
    }
    if (oldPreference == null ||
        oldPreference.getKeyboardLayout() != newPreference.getKeyboardLayout()) {
      viewManager.setKeyboardLayout(newPreference.getKeyboardLayout());
    }
    if (oldPreference == null ||
        oldPreference.getInputStyle() != newPreference.getInputStyle()) {
      viewManager.setInputStyle(newPreference.getInputStyle());
    }
    if (oldPreference == null ||
        oldPreference.isQwertyLayoutForAlphabet() != newPreference.isQwertyLayoutForAlphabet()) {
      viewManager.setQwertyLayoutForAlphabet(newPreference.isQwertyLayoutForAlphabet());
    }
    if (oldPreference == null ||
        oldPreference.isFullscreenMode() != newPreference.isFullscreenMode()) {
      viewManager.setFullscreenMode(
          applicationCompatibility.isFullScreenModeSupported() &&
          newPreference.isFullscreenMode());
    }
    if (oldPreference == null ||
        oldPreference.getFlickSensitivity() != newPreference.getFlickSensitivity()) {
      viewManager.setFlickSensitivity(newPreference.getFlickSensitivity());
    }
    if (oldPreference == null ||
        oldPreference.getEmojiProviderType() != newPreference.getEmojiProviderType()) {
      viewManager.setEmojiProviderType(newPreference.getEmojiProviderType());
    }
    if (oldPreference == null ||
        oldPreference.getHardwareKeyMap() != newPreference.getHardwareKeyMap()) {
      hardwareKeyboard.setHardwareKeyMap(newPreference.getHardwareKeyMap());
    }
    if (oldPreference == null ||
        oldPreference.getSkinType() != newPreference.getSkinType()) {
      viewManager.setSkinType(newPreference.getSkinType());
    }
    if (oldPreference == null ||
        oldPreference.getLayoutAdjustment() != newPreference.getLayoutAdjustment()) {
      viewManager.setLayoutAdjustment(getResources(), newPreference.getLayoutAdjustment());
    }
    if (oldPreference == null ||
        oldPreference.getKeyboardHeightRatio() != newPreference.getKeyboardHeightRatio()) {
      viewManager.setKeyboardHeightRatio(newPreference.getKeyboardHeightRatio());
    }

    propagatedClientSidePreference  = newPreference;
  }

  /**
   * Sends imposed config to the Mozc server.
   *
   * Some config items should be mobile ones.
   * For example, "selection shortcut" should be disabled on software keyboard
   * regardless of stored config.
   * Imposed config should be based on device configuration
   * but currently we ignore device config because we currently do not support
   * hardware keyboard.
   *
   * @param deviceConfig the current device configuration
   */
  private void updateImposedConfig(Configuration deviceConfig) {
    // TODO(matsuzakit): deviceConfig should be used to set following config items.
    sessionExecutor.setImposedConfig(Config.newBuilder()
        .setSessionKeymap(SessionKeymap.MOBILE)
        .setSelectionShortcut(SelectionShortcut.NO_SHORTCUT)
        .setUseEmojiConversion(true)
        .build());
  }

  /**
   * A call-back to catch all the change on any preferences.
   */
  private class SharedPreferenceChangeAdapter implements OnSharedPreferenceChangeListener {
    @Override
    public void onSharedPreferenceChanged(SharedPreferences sharedPreferences, String key) {
      if (isDebugBuild) {
        MozcLog.d("onSharedPreferenceChanged : " + key);
      }
      if (key.startsWith(PREF_TWEAK_PREFIX)) {
        // If the key belongs to PREF_TWEAK group, re-create SessionHandler and view.
        prepareEveryTime(sharedPreferences, getResources().getConfiguration());
        setInputView(onCreateInputView());
        return;
      }
      propagateClientSidePreference(
          new ClientSidePreference(sharedPreferences, getResources().getConfiguration()));
      sessionExecutor.setConfig(ConfigUtil.toConfig(sharedPreferences));
    }
  }

  void maybeSetNarrowMode(Configuration configuration) {
    // If given hardKeyboardHidden is equal to current one, skip updating narrow mode.
    // In other words, only hardKeyboardHidden flag changes narrow mode automatically.
    // This behavior is beneficial for a user who want to change narrow/full mode manually
    // because this method keeps current narrow mode unless hardwarekeyboard connection is changed.
    if (viewManager != null && configuration.hardKeyboardHidden != currentHardKeyboardHidden) {
      currentHardKeyboardHidden = configuration.hardKeyboardHidden;
      switch(currentHardKeyboardHidden) {
        case Configuration.HARDKEYBOARDHIDDEN_NO:
          if (!viewManager.isNarrowMode()) {
            viewManager.hideSubInputView();
            viewManager.setNarrowMode(true);
          }
          break;
        case Configuration.HARDKEYBOARDHIDDEN_YES:
          if (viewManager.isNarrowMode()) {
            viewManager.setNarrowMode(false);
          }
          break;
        case Configuration.HARDKEYBOARDHIDDEN_UNDEFINED:
          break;
      }
    }
  }

  void onConfigurationChangedInternal(Configuration newConfig) {
    InputConnection inputConnection = getCurrentInputConnection();
    if (inputConnection != null) {
      if (inputBound) {
        inputConnection.finishComposingText();
      }
      int selectionStart = selectionTracker.getLastSelectionStart();
      int selectionEnd = selectionTracker.getLastSelectionEnd();
      if (selectionStart >= 0 && selectionEnd >= 0) {
        // We need to keep the last caret position, but it will be soon overwritten in
        // onStartInput. Theoretically, we should prohibit the overwriting, but unfortunately
        // there is no good way to figure out whether the invocation of onStartInput is caused by
        // configuration change, or not. Thus, instead, we'll make an event to invoke
        // onUpdateSelectionInternal with an expected position after the onStartInput invocation,
        // so that it will again overwrite the caret position.
        // Note that, if a user rotates the device with holding preedit text, it will be committed
        // by finishComposingText above, and onUpdateSelection will be invoked from the framework.
        // Invoke onUpdateSelectionInternal twice with same arguments should be safe in this
        // situation.
        configurationChangedHandler.sendMessage(
            configurationChangedHandler.obtainMessage(0, selectionStart, selectionEnd));
      }
    }
    resetContext();
    selectionTracker.onConfigurationChanged(newConfig);

    // NOTE : This method is not called at the time when the service is started.
    // Based on newConfig, imposed config and client side prefereces should be sent
    // because they change based on device config.
    updateImposedConfig(newConfig);
    propagateClientSidePreference(
        new ClientSidePreference(PreferenceManager.getDefaultSharedPreferences(this), newConfig));
    maybeSetNarrowMode(newConfig);
    viewManager.onConfigurationChanged(newConfig);
  }

  @Override
  public void onConfigurationChanged(Configuration newConfig) {
    onConfigurationChangedInternal(newConfig);
    // super.onConfigurationChanged must be called after propagateClientSidePreference
    // to use updated MobileConfiguration.
    super.onConfigurationChanged(newConfig);
  }

  void onUpdateSelectionInternal(int oldSelStart, int oldSelEnd,
                                 int newSelStart, int newSelEnd,
                                 int candidatesStart, int candidatesEnd) {
    MozcLog.d("start MozcService#onUpdateSelectionInternal " + System.nanoTime());
    if (isDebugBuild) {
      MozcLog.d("selection updated: [" + oldSelStart + ":" + oldSelEnd + "] "
                    + "to: [" + newSelStart + ":" + newSelEnd + "] "
                    + "candidates: [" + candidatesStart + ":" + candidatesEnd + "]");
    }

    int updateStatus = selectionTracker.onUpdateSelection(
        oldSelStart, oldSelEnd, newSelStart, newSelEnd, candidatesStart, candidatesEnd);
    if (isDebugBuild) {
      MozcLog.d(selectionTracker.toString());
    }
    switch (updateStatus) {
      case SelectionTracker.DO_NOTHING:
        // Do nothing.
        break;
      case SelectionTracker.RESET_CONTEXT: {
        sessionExecutor.resetContext();

        // Commit the current composing text (preedit text), in case we hit an unknown state.
        // Keeping the composing text sometimes makes it impossible for users to input characters,
        // because it can cause consecutive mis-understanding of caret positions.
        // We do this iff the keyboard is shown, because some other application may edit
        // composition string, such as Google Translate.
        if (isInputViewShown() && inputBound) {
          InputConnection inputConnection = getCurrentInputConnection();
          if (inputConnection != null) {
            inputConnection.finishComposingText();
          }
        }

        // Rendering default Command causes hiding candidate window,
        // and re-showing the keyboard view.
        viewManager.render(Command.getDefaultInstance());
        break;
      }
      default:
        // Otherwise, the updateStatus is the position of the cursor to be moved.
        if (updateStatus < 0) {
          throw new AssertionError("Unknown update status: " + updateStatus);
        }
        sessionExecutor.moveCursor(updateStatus, renderResultCallback);
        break;
    }

    MozcLog.d("end MozcService#onUpdateSelectionInternal " + System.nanoTime());
  }

  @Override
  public void onUpdateSelection(int oldSelStart, int oldSelEnd,
                                int newSelStart, int newSelEnd,
                                int candidatesStart, int candidatesEnd) {
    onUpdateSelectionInternal(
        oldSelStart, oldSelEnd, newSelStart, newSelEnd, candidatesStart, candidatesEnd);
    super.onUpdateSelection(
        oldSelStart, oldSelEnd, newSelStart, newSelEnd, candidatesStart, candidatesEnd);
  }

  private void trimMemory() {
    // We must guarantee the contract of MemoryManageable#trimMemory.
    if (!isInputViewShown()) {
      MozcLog.d("Trimming memory");
      sessionExecutor.deleteSession();
      viewManager.trimMemory();
    }
  }
}
