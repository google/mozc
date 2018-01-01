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

import org.mozc.android.inputmethod.japanese.FeedbackManager.FeedbackEvent;
import org.mozc.android.inputmethod.japanese.KeycodeConverter.KeyEventInterface;
import org.mozc.android.inputmethod.japanese.emoji.EmojiProviderType;
import org.mozc.android.inputmethod.japanese.emoji.EmojiUtil;
import org.mozc.android.inputmethod.japanese.hardwarekeyboard.HardwareKeyboard;
import org.mozc.android.inputmethod.japanese.hardwarekeyboard.HardwareKeyboard.CompositionSwitchMode;
import org.mozc.android.inputmethod.japanese.keyboard.KeyEntity;
import org.mozc.android.inputmethod.japanese.keyboard.KeyEventHandler;
import org.mozc.android.inputmethod.japanese.keyboard.Keyboard;
import org.mozc.android.inputmethod.japanese.keyboard.Keyboard.KeyboardSpecification;
import org.mozc.android.inputmethod.japanese.keyboard.KeyboardActionListener;
import org.mozc.android.inputmethod.japanese.keyboard.KeyboardFactory;
import org.mozc.android.inputmethod.japanese.keyboard.ProbableKeyEventGuesser;
import org.mozc.android.inputmethod.japanese.model.JapaneseSoftwareKeyboardModel;
import org.mozc.android.inputmethod.japanese.model.JapaneseSoftwareKeyboardModel.KeyboardMode;
import org.mozc.android.inputmethod.japanese.model.SymbolCandidateStorage;
import org.mozc.android.inputmethod.japanese.model.SymbolCandidateStorage.SymbolHistoryStorage;
import org.mozc.android.inputmethod.japanese.model.SymbolMajorCategory;
import org.mozc.android.inputmethod.japanese.preference.ClientSidePreference.HardwareKeyMap;
import org.mozc.android.inputmethod.japanese.preference.ClientSidePreference.InputStyle;
import org.mozc.android.inputmethod.japanese.preference.ClientSidePreference.KeyboardLayout;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Command;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.CompositionMode;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.TouchEvent;
import org.mozc.android.inputmethod.japanese.resources.R;
import org.mozc.android.inputmethod.japanese.ui.MenuDialog;
import org.mozc.android.inputmethod.japanese.ui.MenuDialog.MenuDialogListener;
import org.mozc.android.inputmethod.japanese.util.CursorAnchorInfoWrapper;
import org.mozc.android.inputmethod.japanese.util.ImeSwitcherFactory.ImeSwitcher;
import org.mozc.android.inputmethod.japanese.view.Skin;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Rect;
import android.inputmethodservice.InputMethodService;
import android.os.Build;
import android.os.IBinder;
import android.os.Looper;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.Window;
import android.view.inputmethod.EditorInfo;

import java.util.Collections;
import java.util.List;

import javax.annotation.Nullable;

/**
 * Manages Input, Candidate and Extracted views.
 *
 */
public class ViewManager implements ViewManagerInterface {

  /**
   * An small wrapper to inject keyboard view resizing when a user selects a candidate.
   */
  class ViewManagerEventListener extends ViewEventDelegator {

    ViewManagerEventListener(ViewEventListener delegated) {
      super(delegated);
    }

    @Override
    public void onConversionCandidateSelected(int candidateId, Optional<Integer> rowIndex) {
      // Restore the keyboard frame if hidden.
      if (mozcView != null) {
        mozcView.resetKeyboardFrameVisibility();
      }
      super.onConversionCandidateSelected(candidateId, rowIndex);
    }
  }

  /**
   * Converts S/W Keyboard's keycode to KeyEvent instance.
   */
  void onKey(int primaryCode, List<TouchEvent> touchEventList) {
    if (primaryCode == keycodeCapslock || primaryCode == keycodeAlt) {
      // Ignore those key events because they are handled by KeyboardView,
      // but send touchEventList for logging usage stats.
      eventListener.onKeyEvent(null, null, null, touchEventList);
      return;
    }

    // Keyboard switch event.
    if (primaryCode == keycodeChartypeToKana
        || primaryCode == keycodeChartypeToAbc
        || primaryCode == keycodeChartypeToAbc123) {
      if (primaryCode == keycodeChartypeToKana) {
        japaneseSoftwareKeyboardModel.setKeyboardMode(KeyboardMode.KANA);
      } else if (primaryCode == keycodeChartypeToAbc) {
        japaneseSoftwareKeyboardModel.setKeyboardMode(KeyboardMode.ALPHABET);
      } else if (primaryCode == keycodeChartypeToAbc123) {
        japaneseSoftwareKeyboardModel.setKeyboardMode(KeyboardMode.ALPHABET_NUMBER);
      }
      propagateSoftwareKeyboardChange(touchEventList);
      return;
    }

    if (primaryCode == keycodeGlobe) {
      imeSwitcher.switchToNextInputMethod(false);
      return;
    }

    if (primaryCode == keycodeMenuDialog || primaryCode == keycodeImePickerDialog) {
      // We need to reset the keyboard, otherwise it would miss the ACTION_UP event.
      if (mozcView != null) {
        mozcView.resetKeyboardViewState();
      }
      eventListener.onShowMenuDialog(touchEventList);
      if (primaryCode == keycodeMenuDialog) {
        showMenuDialog();
      } else if (primaryCode == keycodeImePickerDialog) {
        showImePickerDialog();
      }
      return;
    }

    if (primaryCode == keycodeSymbol) {
      if (mozcView != null) {
        mozcView.showSymbolInputView(Optional.<SymbolMajorCategory>absent());
      }
      return;
    }

    if (primaryCode == keycodeSymbolEmoji) {
      if (mozcView != null) {
        mozcView.showSymbolInputView(Optional.of(SymbolMajorCategory.EMOJI));
      }
      return;
    }

    if (primaryCode == keycodeUndo) {
      eventListener.onUndo(touchEventList);
      return;
    }

    Optional<ProtoCommands.KeyEvent> mozcKeyEvent =
        primaryKeyCodeConverter.createMozcKeyEvent(primaryCode, touchEventList);
    eventListener.onKeyEvent(mozcKeyEvent.orNull(),
                             primaryKeyCodeConverter.getPrimaryCodeKeyEvent(primaryCode),
                             getActiveSoftwareKeyboardModel().getKeyboardSpecification(),
                             touchEventList);
  }

  /**
   * A simple KeyboardActionListener implementation which just delegates onKey event to
   * ViewManager's onKey method.
   */
  class KeyboardActionAdapter implements KeyboardActionListener {
    @Override
    public void onCancel() {
    }

    @Override
    public void onKey(int primaryCode, List<TouchEvent> touchEventList) {
      ViewManager.this.onKey(primaryCode, touchEventList);
    }

    @Override
    public void onPress(int primaryCode) {
      if (primaryCode != KeyEntity.INVALID_KEY_CODE) {
        eventListener.onFireFeedbackEvent(FeedbackEvent.KEY_DOWN);
      }
    }

    @Override
    public void onRelease(int primaryCode) {
    }
  }

  @VisibleForTesting class ViewLayerEventHandler {
    private static final int NEXUS_KEYBOARD_VENDOR_ID = 0x0D62;
    private static final int NEXUS_KEYBOARD_PRODUCT_ID = 0x160B;
    private boolean isEmojiKeyDownAvailable = false;
    private boolean isEmojiInvoking = false;
    private int pressedKeyNum = 0;
    @VisibleForTesting boolean disableDeviceCheck = false;

    @SuppressLint("NewApi")
    private boolean hasPhysicalEmojiKey(KeyEvent event) {
      InputDevice device = InputDevice.getDevice(event.getDeviceId());
      return disableDeviceCheck
          || (Build.VERSION.SDK_INT >= 19
              && device != null
              && device.getVendorId() == NEXUS_KEYBOARD_VENDOR_ID
              && device.getProductId() == NEXUS_KEYBOARD_PRODUCT_ID);
    }

    private boolean isEmojiKey(KeyEvent event) {
      if (!hasPhysicalEmojiKey(event)) {
        return false;
      }
      if (event.getKeyCode() != KeyEvent.KEYCODE_ALT_LEFT
          && event.getKeyCode() != KeyEvent.KEYCODE_ALT_RIGHT) {
        return false;
      }
      if (event.getAction() == KeyEvent.ACTION_UP) {
        return event.hasNoModifiers();
      } else {
        return event.hasModifiers(KeyEvent.META_ALT_ON);
      }
    }

    public boolean evaluateKeyEvent(KeyEvent event) {
      Preconditions.checkNotNull(event);
      if (event.getAction() == KeyEvent.ACTION_DOWN) {
        ++pressedKeyNum;
      } else if (event.getAction() == KeyEvent.ACTION_UP) {
        pressedKeyNum = Math.max(0, pressedKeyNum - 1);
      } else {
        return false;
      }

      if (isEmojiKey(event)) {
        if (event.getAction() == KeyEvent.ACTION_DOWN) {
          isEmojiKeyDownAvailable = true;
          isEmojiInvoking = false;
        } else if (isEmojiKeyDownAvailable && pressedKeyNum == 0) {
          isEmojiKeyDownAvailable = false;
          isEmojiInvoking = true;
        }
      } else {
        isEmojiKeyDownAvailable = false;
        isEmojiInvoking = false;
      }
      return isEmojiInvoking;
    }

    public void invoke() {
      if (!isEmojiInvoking) {
        return;
      }
      isEmojiInvoking = false;
      if (mozcView != null) {
        if (isSymbolInputViewVisible) {
          hideSubInputView();
          if (!narrowMode) {
            setNarrowMode(true);
          }
        } else {
          isSymbolInputViewShownByEmojiKey = true;
          if (narrowMode) {
            // Turned narrow mode off on all devices for Emoji palette.
            setNarrowModeWithoutVersionCheck(false);
          }
          mozcView.showSymbolInputView(Optional.of(SymbolMajorCategory.EMOJI));
        }
      }
    }

    public void reset() {
      isEmojiKeyDownAvailable = false;
      isEmojiInvoking = false;
      pressedKeyNum = 0;
    }
  }

  // Registered by the user (typically MozcService)
  @VisibleForTesting final ViewEventListener eventListener;

  // The view of the MechaMozc.
  @VisibleForTesting MozcView mozcView;

  // Menu dialog and its listener.
  private final MenuDialogListener menuDialogListener;
  @VisibleForTesting MenuDialog menuDialog = null;

  // IME switcher instance to detect that voice input is available or not.
  private final ImeSwitcher imeSwitcher;

  /** Key event handler to handle events on Mozc server. */
  private final KeyEventHandler keyEventHandler;

  /** Key event handler to handle events on view layer. */
  @VisibleForTesting final ViewLayerEventHandler viewLayerKeyEventHandler =
      new ViewLayerEventHandler();

  /**
   * Model to represent the current software keyboard state.
   * All the setter methods don't affect symbolNumberSoftwareKeyboardModel but
   * japaneseSoftwareKeyboardModel.
   */
  private final JapaneseSoftwareKeyboardModel japaneseSoftwareKeyboardModel =
      new JapaneseSoftwareKeyboardModel();
  /**
   * Model to represent the number software keyboard state.
   * Its keyboard mode is set in the constructor to KeyboardMode.SYMBOL_NUMBER and will never be
   * changed.
   */
  private final JapaneseSoftwareKeyboardModel symbolNumberSoftwareKeyboardModel =
      new JapaneseSoftwareKeyboardModel();

  @VisibleForTesting final HardwareKeyboard hardwareKeyboard;

  /** True if symbol input view is visible. */
  private boolean isSymbolInputViewVisible;

  /** True if symbol input view is shown by the Emoji key on physical keyboard. */
  private boolean isSymbolInputViewShownByEmojiKey;

  /** The factory of parsed keyboard data. */
  private final KeyboardFactory keyboardFactory = new KeyboardFactory();

  private final SymbolCandidateStorage symbolCandidateStorage;

  /** Current fullscreen mode */
  private boolean fullscreenMode = false;

  /** Current narrow mode */
  private boolean narrowMode = false;

  /** Current narrow mode */
  private boolean narrowModeByConfiguration = false;

  /** Current popup enabled state. */
  private boolean popupEnabled = true;

  /** Current Globe button enabled state. */
  private boolean globeButtonEnabled = false;

  /** True if CursorAnchorInfo is enabled. */
  private boolean cursorAnchroInfoEnabled = false;

  /** True if hardware keyboard exists. */
  private boolean hardwareKeyboardExist = false;

  /**
   * True if voice input is eligible.
   * <p>
   * This conditions is calculated based on following conditions.
   * <ul>
   * <li>VoiceIME's status: If VoiceIME is not available, this flag becomes false.
   * <li>EditorInfo: If current editor does not want to use voice input, this flag becomes false.
   *   <ul>
   *   <li>Voice input might be explicitly forbidden by the editor.
   *   <li>Voice input should be useless for the number input editors.
   *   <li>Voice input should be useless for password field.
   *   <ul>
   * </ul>
   */
  private boolean isVoiceInputEligible = false;

  private boolean isVoiceInputEnabledByPreference = true;

  private int flickSensitivity = 0;

  @VisibleForTesting EmojiProviderType emojiProviderType = EmojiProviderType.NONE;

  /** Current skin type. */
  private Skin skin = Skin.getFallbackInstance();

  private LayoutAdjustment layoutAdjustment = LayoutAdjustment.FILL;

  /** Percentage of keyboard height */
  private int keyboardHeightRatio = 100;

  // Keycodes defined in resource files.
  // Printable keys are not defined. Refer them using character literal.
  // These are "constant values" as a matter of practice,
  // but such name like "KEYCODE_LEFT" makes Lint unhappy
  // because they are not "static final".
  private final int keycodeChartypeToKana;
  private final int keycodeChartypeToAbc;
  private final int keycodeChartypeToAbc123;
  private final int keycodeGlobe;
  private final int keycodeSymbol;
  private final int keycodeSymbolEmoji;
  private final int keycodeUndo;
  private final int keycodeCapslock;
  private final int keycodeAlt;
  private final int keycodeMenuDialog;
  private final int keycodeImePickerDialog;

  /** Handles software keyboard event and sends it to the service. */
  private final KeyboardActionAdapter keyboardActionListener;

  private final PrimaryKeyCodeConverter primaryKeyCodeConverter;

  public ViewManager(Context context, ViewEventListener listener,
                     SymbolHistoryStorage symbolHistoryStorage, ImeSwitcher imeSwitcher,
                     MenuDialogListener menuDialogListener) {
    this(context, listener, symbolHistoryStorage, imeSwitcher, menuDialogListener,
         new ProbableKeyEventGuesser(context.getAssets()), new HardwareKeyboard());
  }

  @VisibleForTesting
  ViewManager(Context context, ViewEventListener listener,
              SymbolHistoryStorage symbolHistoryStorage, ImeSwitcher imeSwitcher,
              @Nullable MenuDialogListener menuDialogListener, ProbableKeyEventGuesser guesser,
              HardwareKeyboard hardwareKeyboard) {
    Preconditions.checkNotNull(context);
    Preconditions.checkNotNull(listener);
    Preconditions.checkNotNull(imeSwitcher);
    Preconditions.checkNotNull(hardwareKeyboard);

    primaryKeyCodeConverter = new PrimaryKeyCodeConverter(context, guesser);

    symbolNumberSoftwareKeyboardModel.setKeyboardMode(KeyboardMode.SYMBOL_NUMBER);

    // Prefetch keycodes from resource
    Resources res = context.getResources();
    keycodeChartypeToKana = res.getInteger(R.integer.key_chartype_to_kana);
    keycodeChartypeToAbc = res.getInteger(R.integer.key_chartype_to_abc);
    keycodeChartypeToAbc123 = res.getInteger(R.integer.key_chartype_to_abc_123);
    keycodeGlobe = res.getInteger(R.integer.key_globe);
    keycodeSymbol = res.getInteger(R.integer.key_symbol);
    keycodeSymbolEmoji = res.getInteger(R.integer.key_symbol_emoji);
    keycodeUndo = res.getInteger(R.integer.key_undo);
    keycodeCapslock = res.getInteger(R.integer.key_capslock);
    keycodeAlt = res.getInteger(R.integer.key_alt);
    keycodeMenuDialog = res.getInteger(R.integer.key_menu_dialog);
    keycodeImePickerDialog = res.getInteger(R.integer.key_ime_picker_dialog);

    // Inject some logics into the listener.
    eventListener = new ViewManagerEventListener(listener);
    keyboardActionListener = new KeyboardActionAdapter();
    // Prepare callback object.
    keyEventHandler = new KeyEventHandler(
        Looper.getMainLooper(),
        keyboardActionListener,
        res.getInteger(R.integer.config_repeat_key_delay),
        res.getInteger(R.integer.config_repeat_key_interval),
        res.getInteger(R.integer.config_long_press_key_delay));

    this.imeSwitcher = imeSwitcher;
    this.menuDialogListener = menuDialogListener;
    this.symbolCandidateStorage = new SymbolCandidateStorage(symbolHistoryStorage);
    this.hardwareKeyboard = hardwareKeyboard;
  }

  /**
   * Creates new input view.
   *
   * "Input view" is a software keyboard in almost all cases.
   *
   * Previously created input view is not accessed any more after calling this method.
   *
   * @param context
   * @return newly created view.
   */
  @Override
  public MozcView createMozcView(Context context) {
    mozcView = MozcView.class.cast(LayoutInflater.from(context).inflate(R.layout.mozc_view, null));
    // Suppress update of View's internal state
    // until all the updates done in this method are finished. Just in case.
    mozcView.setVisibility(View.GONE);
    mozcView.setKeyboardHeightRatio(keyboardHeightRatio);
    mozcView.setCursorAnchorInfoEnabled(cursorAnchroInfoEnabled);
    OnClickListener widenButtonClickListener = new OnClickListener() {
      @Override
      public void onClick(View v) {
        eventListener.onFireFeedbackEvent(FeedbackEvent.NARROW_FRAME_WIDEN_BUTTON_DOWN);
        setNarrowMode(!narrowMode);
      }
    };
    OnClickListener leftAdjustButtonClickListener = new OnClickListener() {
      @Override
      public void onClick(View v) {
        eventListener.onUpdateKeyboardLayoutAdjustment(LayoutAdjustment.LEFT);
      }
    };
    OnClickListener rightAdjustButtonClickListener = new OnClickListener() {
      @Override
      public void onClick(View v) {
        eventListener.onUpdateKeyboardLayoutAdjustment(LayoutAdjustment.RIGHT);
      }
    };

    OnClickListener microphoneButtonClickListener = new OnClickListener() {
      @Override
      public void onClick(View v) {
        eventListener.onFireFeedbackEvent(FeedbackEvent.MICROPHONE_BUTTON_DOWN);
        imeSwitcher.switchToVoiceIme("ja-jp");
      }
    };
    mozcView.setEventListener(
        eventListener,
        widenButtonClickListener,
        // User pushes these buttons to move position in order to see hidden text in editing rather
        // than to change his/her favorite position. So we should not apply it to preferences.
        leftAdjustButtonClickListener,
        rightAdjustButtonClickListener,
        microphoneButtonClickListener);

    mozcView.setKeyEventHandler(keyEventHandler);

    propagateSoftwareKeyboardChange(Collections.<TouchEvent>emptyList());
    mozcView.setFullscreenMode(fullscreenMode);
    mozcView.setLayoutAdjustmentAndNarrowMode(layoutAdjustment, narrowMode);
    // At the moment, it is necessary to set the storage to the view, *before* setting emoji
    // provider type.
    // TODO(hidehiko): Remove the restriction.
    mozcView.setSymbolCandidateStorage(symbolCandidateStorage);
    mozcView.setEmojiProviderType(emojiProviderType);
    mozcView.setPopupEnabled(popupEnabled);
    mozcView.setFlickSensitivity(flickSensitivity);
    mozcView.setSkin(skin);

    // Clear the menu dialog.
    menuDialog = null;

    reset();

    mozcView.setVisibility(View.VISIBLE);
    return mozcView;
  }

  private void showMenuDialog() {
    if (mozcView == null) {
      MozcLog.w("mozcView is not initialized.");
      return;
    }

    menuDialog = new MenuDialog(mozcView.getContext(), Optional.fromNullable(menuDialogListener));
    IBinder windowToken = mozcView.getWindowToken();
    if (windowToken == null) {
      MozcLog.w("Unknown window token");
    } else {
      menuDialog.setWindowToken(windowToken);
    }
    menuDialog.show();
  }

  private void showImePickerDialog() {
    if (mozcView == null) {
      MozcLog.w("mozcView is not initialized.");
      return;
    }
    if (!MozcUtil.requestShowInputMethodPicker(mozcView.getContext())) {
      MozcLog.e("Failed to send message to launch the input method picker dialog.");
    }
  }

  private void maybeDismissMenuDialog() {
    MenuDialog menuDialog = this.menuDialog;
    if (menuDialog != null) {
      menuDialog.dismiss();
    }
  }

  /**
   * Renders views which this instance own based on Command.Output.
   *
   * Note that showing/hiding views is Service's responsibility.
   */
  @Override
  public void render(Command outCommand) {
    if (outCommand == null) {
      return;
    }
    if (mozcView == null) {
      return;
    }
    if (outCommand.getOutput().getAllCandidateWords().getCandidatesCount() == 0
        && !outCommand.getInput().getRequestSuggestion()) {
      // The server doesn't return the suggestion result, because there is following
      // key sequence, which will trigger the suggest and the new suggestion will overwrite
      // the current suggest. In order to avoid chattering the candidate window,
      // we skip the following rendering.
      return;
    }

    mozcView.setCommand(outCommand);
    if (outCommand.getOutput().getAllCandidateWords().getCandidatesCount() == 0) {
      // If the candidate is empty (i.e. the CandidateView will go to GONE),
      // reset the keyboard so that a user can type keyboard.
      mozcView.resetKeyboardFrameVisibility();
    }
  }

  /**
   * @return the current keyboard specification.
   */
  @Override
  public KeyboardSpecification getKeyboardSpecification() {
    return getActiveSoftwareKeyboardModel().getKeyboardSpecification();
  }

  /** Set {@code EditorInfo} instance to the current view. */
  @Override
  public void setEditorInfo(EditorInfo attribute) {
    if (mozcView != null) {
      mozcView.setEmojiEnabled(
          EmojiUtil.isUnicodeEmojiAvailable(Build.VERSION.SDK_INT),
          EmojiUtil.isCarrierEmojiAllowed(attribute));
      mozcView.setPasswordField(MozcUtil.isPasswordField(attribute.inputType));
      mozcView.setEditorInfo(attribute);
    }
    isVoiceInputEligible = MozcUtil.isVoiceInputPreferred(attribute);

    japaneseSoftwareKeyboardModel.setInputType(attribute.inputType);
    // TODO(hsumita): Set input type on Hardware keyboard, too. Otherwise, Hiragana input can be
    //                enabled unexpectedly. (e.g. Number text field.)
    propagateSoftwareKeyboardChange(Collections.<TouchEvent>emptyList());
  }

  private boolean shouldVoiceImeBeEnabled() {
    // Disable voice IME if hardware keyboard exists to avoid a framework bug.
    return isVoiceInputEligible && isVoiceInputEnabledByPreference && !hardwareKeyboardExist
        && imeSwitcher.isVoiceImeAvailable();
  }

  @Override
  public void setTextForActionButton(CharSequence text) {
    // TODO(mozc-team): Implement action button handling.
  }

  @Override
  public boolean hideSubInputView() {
    if (mozcView == null) {
      return false;
    }

    // Try to hide a sub view from front to back.
    if (mozcView.hideSymbolInputView()) {
      return true;
    }

    return false;
  }

  /**
   * Creates and sets a keyboard represented by the resource id to the input frame.
   * <p>
   * Note that this method requires inputFrameView is not null, and its first child is
   * the JapaneseKeyboardView.
   */
  private void updateKeyboardView() {
    if (mozcView == null) {
      return;
    }
    Rect size = mozcView.getKeyboardSize();
    Keyboard keyboard = keyboardFactory.get(
        mozcView.getResources(), japaneseSoftwareKeyboardModel.getKeyboardSpecification(),
        size.width(), size.height());
    mozcView.setKeyboard(keyboard);
    primaryKeyCodeConverter.setKeyboard(keyboard);
  }

  /**
   * Propagates the change of S/W keyboard to the view layer and the H/W keyboard configuration.
   */
  private void propagateSoftwareKeyboardChange(List<TouchEvent> touchEventList) {
    KeyboardSpecification specification = japaneseSoftwareKeyboardModel.getKeyboardSpecification();

    // TODO(team): The purpose of the following call of onKeyEvent() is to tell the change of
    // software keyboard specification to Mozc server through the event listener registered by
    // MozcService. Obviously, calling onKeyEvent() for this purpose is abuse and should be fixed.
    eventListener.onKeyEvent(null, null, specification, touchEventList);

    // Update H/W keyboard specification to keep a consistency with S/W keyboard.
    hardwareKeyboard.setCompositionMode(
        specification.getCompositionMode() == CompositionMode.HIRAGANA
        ? CompositionSwitchMode.KANA : CompositionSwitchMode.ALPHABET);

    updateKeyboardView();
  }

  private void propagateHardwareKeyboardChange() {
    propagateHardwareKeyboardChangeAndSendKey(null);
  }

  /**
   * Propagates the change of S/W keyboard to the view layer and the H/W keyboard configuration, and
   * the send key event to Mozc server.
   */
  private void propagateHardwareKeyboardChangeAndSendKey(@Nullable KeyEvent event) {
    KeyboardSpecification specification = hardwareKeyboard.getKeyboardSpecification();

    if (event == null) {
      eventListener.onKeyEvent(null, null, specification, Collections.<TouchEvent>emptyList());
    } else {
      eventListener.onKeyEvent(
          hardwareKeyboard.getMozcKeyEvent(event), hardwareKeyboard.getKeyEventInterface(event),
          specification, Collections.<TouchEvent>emptyList());
    }

    // Update S/W keyboard specification to keep a consistency with H/W keyboard.
    japaneseSoftwareKeyboardModel.setKeyboardMode(
        specification.getCompositionMode() == CompositionMode.HIRAGANA
        ? KeyboardMode.KANA : KeyboardMode.ALPHABET);

    updateKeyboardView();
  }

  /**
   * Set this keyboard layout to the specified one.
   * @param keyboardLayout New keyboard layout.
   * @throws NullPointerException If <code>keyboardLayout</code> is <code>null</code>.
   */
  @Override
  public void setKeyboardLayout(KeyboardLayout keyboardLayout) {
    Preconditions.checkNotNull(keyboardLayout);

    if (japaneseSoftwareKeyboardModel.getKeyboardLayout() != keyboardLayout) {
      // If changed, clear the keyboard cache.
      keyboardFactory.clear();
    }

    japaneseSoftwareKeyboardModel.setKeyboardLayout(keyboardLayout);
    propagateSoftwareKeyboardChange(Collections.<TouchEvent>emptyList());
  }

  /**
   * Set the input style.
   * @param inputStyle new input style.
   * @throws NullPointerException If <code>inputStyle</code> is <code>null</code>.
   * TODO(hidehiko): Refactor out following keyboard switching logic into another class.
   */
  @Override
  public void setInputStyle(InputStyle inputStyle) {
    Preconditions.checkNotNull(inputStyle);

    if (japaneseSoftwareKeyboardModel.getInputStyle() != inputStyle) {
      // If changed, clear the keyboard cache.
      keyboardFactory.clear();
    }

    japaneseSoftwareKeyboardModel.setInputStyle(inputStyle);
    propagateSoftwareKeyboardChange(Collections.<TouchEvent>emptyList());
  }

  @Override
  public void setQwertyLayoutForAlphabet(boolean qwertyLayoutForAlphabet) {
    if (japaneseSoftwareKeyboardModel.isQwertyLayoutForAlphabet() != qwertyLayoutForAlphabet) {
      // If changed, clear the keyboard cache.
      keyboardFactory.clear();
    }

    japaneseSoftwareKeyboardModel.setQwertyLayoutForAlphabet(qwertyLayoutForAlphabet);
    propagateSoftwareKeyboardChange(Collections.<TouchEvent>emptyList());
  }

  @Override
  public void setFullscreenMode(boolean fullscreenMode) {
    this.fullscreenMode = fullscreenMode;
    if (mozcView != null) {
      mozcView.setFullscreenMode(fullscreenMode);
    }
  }

  @Override
  public boolean isFullscreenMode() {
    return fullscreenMode;
  }

  @Override
  public void setFlickSensitivity(int flickSensitivity) {
    this.flickSensitivity = flickSensitivity;
    if (mozcView != null) {
      mozcView.setFlickSensitivity(flickSensitivity);
    }
  }

  @Override
  public void setEmojiProviderType(EmojiProviderType emojiProviderType) {
    Preconditions.checkNotNull(emojiProviderType);

    this.emojiProviderType = emojiProviderType;
    if (mozcView != null) {
      mozcView.setEmojiProviderType(emojiProviderType);
    }
  }

  /**
   * Updates whether Globe button should be enabled or not based on
   * {@code InputMethodManager#shouldOfferSwitchingToNextInputMethod(IBinder)}
   */
  @Override
  public void updateGlobeButtonEnabled() {
    this.globeButtonEnabled = imeSwitcher.shouldOfferSwitchingToNextInputMethod();
    if (mozcView != null) {
      mozcView.setGlobeButtonEnabled(globeButtonEnabled);
    }
  }

  /**
   * Updates whether Microphone button should be enabled or not based on
   * availability of voice input method.
   */
  @Override
  public void updateMicrophoneButtonEnabled() {
    if (mozcView != null) {
      mozcView.setMicrophoneButtonEnabled(shouldVoiceImeBeEnabled());
    }
  }

  /**
   * Sets narrow mode.
   * <p>
   * The behavior of this method depends on API level.
   * We decided to respect the configuration on API 21 or later, so this method ignores the argument
   * in such case. If you really want to bypass the version check, please use
   * {@link #setNarrowModeWithoutVersionCheck(boolean)} instead.
   */
  private void setNarrowMode(boolean isNarrowMode) {
    if (Build.VERSION.SDK_INT >= 21) {
      // Always respects configuration on Lollipop or later.
      setNarrowModeWithoutVersionCheck(narrowModeByConfiguration);
    } else {
      setNarrowModeWithoutVersionCheck(isNarrowMode);
    }
  }

  private void setNarrowModeWithoutVersionCheck(boolean newNarrowMode) {
    if (narrowMode == newNarrowMode) {
      return;
    }
    narrowMode = newNarrowMode;
    if (newNarrowMode) {
      hideSubInputView();
    }
    if (mozcView != null) {
      mozcView.setLayoutAdjustmentAndNarrowMode(layoutAdjustment, newNarrowMode);
    }
    updateMicrophoneButtonEnabled();
    eventListener.onNarrowModeChanged(newNarrowMode);
  }

  /**
   * Returns true if we should transit to narrow mode,
   *  based on returned {@code Command} and {@code KeyEventInterface} from the server.
   *
   * <p>If all of the following conditions are satisfied, narrow mode is shown.
   * <ul>
   * <li>The key event is from h/w keyboard.
   * <li>The key event has printable character without modifier.
   * </ul>
   */
  @Override
  public void maybeTransitToNarrowMode(Command command, KeyEventInterface keyEventInterface) {
    Preconditions.checkNotNull(command);
    // Surely we don't anything when on narrow mode already.
    if (narrowMode) {
      return;
    }
    // Do nothing for the input from software keyboard.
    if (keyEventInterface == null || !keyEventInterface.getNativeEvent().isPresent()) {
      return;
    }
    // Do nothing if input doesn't have a key. (e.g. pure modifier key)
    if (!command.getInput().hasKey()) {
      return;
    }

    // Passed all the check. Transit to narrow mode.
    setNarrowMode(true);
  }

  @Override
  public boolean isNarrowMode() {
    return narrowMode;
  }

  @Override
  public boolean isFloatingCandidateMode() {
    return mozcView != null && mozcView.isFloatingCandidateMode();
  }

  @Override
  public void setPopupEnabled(boolean popupEnabled) {
    this.popupEnabled = popupEnabled;
    if (mozcView != null) {
      mozcView.setPopupEnabled(popupEnabled);
    }
  }

  @Override
  public void switchHardwareKeyboardCompositionMode(CompositionSwitchMode mode) {
    Preconditions.checkNotNull(mode);

    CompositionMode oldMode = hardwareKeyboard.getCompositionMode();
    hardwareKeyboard.setCompositionMode(mode);
    CompositionMode newMode = hardwareKeyboard.getCompositionMode();
    if (oldMode != newMode) {
      propagateHardwareKeyboardChange();
    }
  }

  @Override
  public void setHardwareKeyMap(HardwareKeyMap hardwareKeyMap) {
    hardwareKeyboard.setHardwareKeyMap(Preconditions.checkNotNull(hardwareKeyMap));
  }

  @Override
  public void setSkin(Skin skin) {
    this.skin = Preconditions.checkNotNull(skin);
    if (mozcView != null) {
      mozcView.setSkin(skin);
    }
  }

  @Override
  public void setMicrophoneButtonEnabledByPreference(boolean microphoneButtonEnabled) {
    this.isVoiceInputEnabledByPreference = microphoneButtonEnabled;
    updateMicrophoneButtonEnabled();
  }

  /**
   * Set layout adjustment and show animation if required.
   * <p>
   * Note that this method does *NOT* update SharedPreference.
   * If you want to update it, use ViewEventListener#onUpdateKeyboardLayoutAdjustment(),
   * which updates SharedPreference and indirectly calls this method.
   */
  @Override
  public void setLayoutAdjustment(LayoutAdjustment layoutAdjustment) {
    Preconditions.checkNotNull(layoutAdjustment);
    if (mozcView != null) {
      mozcView.setLayoutAdjustmentAndNarrowMode(layoutAdjustment, narrowMode);
      if (this.layoutAdjustment != layoutAdjustment) {
        mozcView.startLayoutAdjustmentAnimation();
      }
    }
    this.layoutAdjustment = layoutAdjustment;
  }

  @Override
  public void setKeyboardHeightRatio(int keyboardHeightRatio) {
    this.keyboardHeightRatio = keyboardHeightRatio;
    if (mozcView != null) {
      mozcView.setKeyboardHeightRatio(keyboardHeightRatio);
    }
  }

  /**
   * Reset the status of the current input view.
   *
   * This method must be called when the IME is turned on.
   * Note that this method can be called before {@link #createMozcView(Context)}
   * so null-check is mandatory.
   */
  @Override
  public void reset() {
    if (mozcView != null) {
      mozcView.reset();
    }

    viewLayerKeyEventHandler.reset();

    // Reset menu dialog.
    maybeDismissMenuDialog();
  }

  @Override
  public void computeInsets(Context context, InputMethodService.Insets outInsets, Window window) {
    // The IME's area is prioritized than app's.
    // - contentTopInsets
    //   - This is the top part of the UI that is the main content.
    //   - This affects window layout.
    //       So if this value is changed, resizing the application behind happens.
    //   - This value is relative to the top edge of the input method window.
    // - visibleTopInsets
    //   - This is the top part of the UI that is visibly covering the application behind it.
    //       Changing this value will not cause resizing the application.
    //   - This is *not* to clip IME's drawing area.
    //   - This value is relative to the top edge of the input method window.
    // Thus it seems that we have to guarantee contentTopInsets <= visibleTopInsets.
    // If contentTopInsets < visibleTopInsets, the app's UI is drawn on IME's area
    // but almost all (or completely all?) application does not draw anything
    // on such "outside" area from the app's window.
    // Conclusion is that we should guarantee contentTopInsets == visibleTopInsets.
    //
    // On Honeycomb or later version, we cannot take touch events outside of IME window.
    // As its workaround, we cover the almost screen by transparent view, and we need to consider
    // the gap between the top of user visible IME window, and the transparent view's top here.
    // Note that touch events for the transparent view will be ignored by IME and automatically
    // sent to the application if it is not for the IME.

    View contentView = window.findViewById(Window.ID_ANDROID_CONTENT);
    int contentViewWidth = contentView.getWidth();
    int contentViewHeight = contentView.getHeight();

    if (mozcView == null) {
      outInsets.touchableInsets = InputMethodService.Insets.TOUCHABLE_INSETS_CONTENT;
      outInsets.contentTopInsets = contentViewHeight
          - context.getResources().getDimensionPixelSize(R.dimen.input_frame_height);
      outInsets.visibleTopInsets = outInsets.contentTopInsets;
      return;
    }

    mozcView.setInsets(contentViewWidth, contentViewHeight, outInsets);
  }

  @Override
  public void onConfigurationChanged(Configuration newConfig) {
    primaryKeyCodeConverter.setConfiguration(newConfig);
    hardwareKeyboardExist = newConfig.keyboard != Configuration.KEYBOARD_NOKEYS;
    if (newConfig.hardKeyboardHidden != Configuration.HARDKEYBOARDHIDDEN_UNDEFINED){
      narrowModeByConfiguration =
          newConfig.hardKeyboardHidden == Configuration.HARDKEYBOARDHIDDEN_NO;
      setNarrowMode(narrowModeByConfiguration);
    }
  }

  @Override
  public boolean isKeyConsumedOnViewAsynchronously(KeyEvent event) {
    return viewLayerKeyEventHandler.evaluateKeyEvent(Preconditions.checkNotNull(event));
  }

  @Override
  public void consumeKeyOnViewSynchronously(KeyEvent event) {
    viewLayerKeyEventHandler.invoke();
  }

  @Override
  public void onHardwareKeyEvent(KeyEvent event) {
    // Maybe update the composition mode based on the event.
    // For example, zen/han key toggles the composition mode (hiragana <--> alphabet).
    CompositionMode compositionMode = hardwareKeyboard.getCompositionMode();
    hardwareKeyboard.setCompositionModeByKey(event);
    CompositionMode currentCompositionMode = hardwareKeyboard.getCompositionMode();
    if (compositionMode != currentCompositionMode) {
      propagateHardwareKeyboardChangeAndSendKey(event);
    } else {
      eventListener.onKeyEvent(
          hardwareKeyboard.getMozcKeyEvent(event), hardwareKeyboard.getKeyEventInterface(event),
          hardwareKeyboard.getKeyboardSpecification(), Collections.<TouchEvent>emptyList());
    }
  }

  @Override
  public boolean isGenericMotionToConsume(MotionEvent event) {
    return false;
  }

  @Override
  public boolean consumeGenericMotion(MotionEvent event) {
    return false;
  }

  @VisibleForTesting
  @Override
  public ViewEventListener getEventListener() {
    return eventListener;
  }

  /**
   * Returns active (shown) JapaneseSoftwareKeyboardModel.
   * If symbol picker is shown, symbol-number keyboard's is returned.
   */
  @VisibleForTesting
  @Override
  public JapaneseSoftwareKeyboardModel getActiveSoftwareKeyboardModel() {
    if (isSymbolInputViewVisible) {
      return symbolNumberSoftwareKeyboardModel;
    } else {
      return japaneseSoftwareKeyboardModel;
    }
  }

  @VisibleForTesting
  @Override
  public boolean isPopupEnabled() {
    return popupEnabled;
  }

  @VisibleForTesting
  @Override
  public int getFlickSensitivity() {
    return flickSensitivity;
  }

  @VisibleForTesting
  @Override
  public EmojiProviderType getEmojiProviderType() {
    return emojiProviderType;
  }

  @VisibleForTesting
  @Override
  public Skin getSkin() {
    return skin;
  }

  @VisibleForTesting
  @Override
  public boolean isMicrophoneButtonEnabledByPreference() {
    return isVoiceInputEnabledByPreference;
  }

  @VisibleForTesting
  @Override
  public LayoutAdjustment getLayoutAdjustment() {
    return layoutAdjustment;
  }

  @VisibleForTesting
  @Override
  public int getKeyboardHeightRatio() {
    return keyboardHeightRatio;
  }

  @VisibleForTesting
  @Override
  public HardwareKeyMap getHardwareKeyMap() {
    return hardwareKeyboard.getHardwareKeyMap();
  }

  @Override
  public void trimMemory() {
    if (mozcView != null) {
      mozcView.trimMemory();
    }
  }

  @Override
  public KeyboardActionListener getKeyboardActionListener() {
    return keyboardActionListener;
  }

  @Override
  public void onStartInputView(EditorInfo editorInfo) {
    if (mozcView != null) {
      mozcView.onStartInputView(editorInfo);
    }
  }

  @Override
  public void setCursorAnchorInfo(CursorAnchorInfoWrapper cursorAnchorInfo) {
    if (mozcView != null) {
      mozcView.setCursorAnchorInfo(cursorAnchorInfo);
    }
  }

  @Override
  public void setCursorAnchorInfoEnabled(boolean enabled) {
    this.cursorAnchroInfoEnabled = enabled;
    if (mozcView != null) {
      mozcView.setCursorAnchorInfoEnabled(enabled);
    }
  }

  @Override
  public void onShowSymbolInputView() {
    isSymbolInputViewVisible = true;
    mozcView.resetKeyboardViewState();
  }

  @Override
  public void onCloseSymbolInputView() {
    if (isSymbolInputViewShownByEmojiKey) {
      setNarrowMode(true);
    }
    isSymbolInputViewVisible = false;
    isSymbolInputViewShownByEmojiKey = false;
  }
}
