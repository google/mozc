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
import org.mozc.android.inputmethod.japanese.JapaneseKeyboard.KeyboardSpecification;
import org.mozc.android.inputmethod.japanese.KeycodeConverter.KeyEventInterface;
import org.mozc.android.inputmethod.japanese.emoji.EmojiProviderType;
import org.mozc.android.inputmethod.japanese.emoji.EmojiUtil;
import org.mozc.android.inputmethod.japanese.keyboard.KeyEntity;
import org.mozc.android.inputmethod.japanese.keyboard.KeyEventHandler;
import org.mozc.android.inputmethod.japanese.keyboard.KeyboardActionListener;
import org.mozc.android.inputmethod.japanese.keyboard.ProbableKeyEventGuesser;
import org.mozc.android.inputmethod.japanese.model.JapaneseSoftwareKeyboardModel;
import org.mozc.android.inputmethod.japanese.model.JapaneseSoftwareKeyboardModel.KeyboardMode;
import org.mozc.android.inputmethod.japanese.model.SymbolCandidateStorage;
import org.mozc.android.inputmethod.japanese.model.SymbolCandidateStorage.SymbolHistoryStorage;
import org.mozc.android.inputmethod.japanese.preference.ClientSidePreference.InputStyle;
import org.mozc.android.inputmethod.japanese.preference.ClientSidePreference.KeyboardLayout;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Command;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.CompositionMode;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.TouchEvent;
import org.mozc.android.inputmethod.japanese.resources.R;
import org.mozc.android.inputmethod.japanese.ui.MenuDialog;
import org.mozc.android.inputmethod.japanese.ui.MenuDialog.MenuDialogListener;
import org.mozc.android.inputmethod.japanese.util.ImeSwitcherFactory.ImeSwitcher;
import org.mozc.android.inputmethod.japanese.view.SkinType;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Rect;
import android.inputmethodservice.InputMethodService;
import android.os.Build;
import android.os.IBinder;
import android.os.Looper;
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
    public void onConversionCandidateSelected(int candidateId) {
      // Restore the keyboard frame if hidden.
      if (mozcView != null) {
        mozcView.resetKeyboardFrameVisibility();
      }
      super.onConversionCandidateSelected(candidateId);
    }
  }

  /**
   * Converts S/W Keyboard's keycode to KeyEvent instance.
   * Exposed as protected for testing purpose.
   */
  protected void onKey(int primaryCode, List<? extends TouchEvent> touchEventList) {
    if (primaryCode == keycodeCapslock ||
        primaryCode == keycodeAlt) {
      // Ignore those key events because they are handled by KeyboardView,
      // but send touchEventList for logging usage stats.
      if (eventListener != null) {
        eventListener.onKeyEvent(null, null, null, touchEventList);
      }
      return;
    }

    // Keyboard switch event.
    if (primaryCode == keycodeChartypeToKana ||
        primaryCode == keycodeChartypeTo123 ||
        primaryCode == keycodeChartypeToAbc ||
        primaryCode == keycodeChartypeToKana123 ||
        primaryCode == keycodeChartypeToAbc123) {
      if (primaryCode == keycodeChartypeToKana) {
        japaneseSoftwareKeyboardModel.setKeyboardMode(KeyboardMode.KANA);
      } else if (primaryCode == keycodeChartypeToAbc) {
        japaneseSoftwareKeyboardModel.setKeyboardMode(KeyboardMode.ALPHABET);
      } else if (primaryCode == keycodeChartypeTo123 ||
                 primaryCode == keycodeChartypeToKana123) {
        japaneseSoftwareKeyboardModel.setKeyboardMode(KeyboardMode.KANA_NUMBER);
      } else if (primaryCode == keycodeChartypeToAbc123) {
        japaneseSoftwareKeyboardModel.setKeyboardMode(KeyboardMode.ALPHABET_NUMBER);
      }
      setJapaneseKeyboard(
          japaneseSoftwareKeyboardModel.getKeyboardSpecification(), touchEventList);
      return;
    }

    if (primaryCode == keycodeMenuDialog || primaryCode == keycodeImePickerDialog) {
      // We need to reset the keyboard, otherwise it would miss the ACTION_UP event.
      if (mozcView != null) {
        mozcView.resetKeyboardViewState();
      }
      if (eventListener != null) {
        eventListener.onShowMenuDialog(touchEventList);
      }
      if (primaryCode == keycodeMenuDialog) {
        showMenuDialog();
      } else if (primaryCode == keycodeImePickerDialog) {
        showImePickerDialog();
      }
      return;
    }

    if (primaryCode == keycodeSymbol) {
      if (eventListener != null) {
        eventListener.onSubmitPreedit();
      }
      if (mozcView != null) {
        mozcView.resetKeyboardViewState();
        mozcView.showSymbolInputView();
        if (eventListener != null) {
          eventListener.onShowSymbolInputView(touchEventList);
        }
      }
      return;
    }

    if (primaryCode == keycodeUndo) {
      if (eventListener != null) {
        eventListener.onUndo(touchEventList);
      }
      return;
    }

    ProtoCommands.KeyEvent mozcKeyEvent =
        primaryKeyCodeConverter.createMozcKeyEvent(primaryCode, touchEventList);
    if (eventListener != null) {
      eventListener.onKeyEvent(mozcKeyEvent,
                               primaryKeyCodeConverter.getPrimaryCodeKeyEvent(primaryCode),
                               japaneseSoftwareKeyboardModel.getKeyboardSpecification(),
                               touchEventList);
    }
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
    public void onKey(int primaryCode, List<? extends TouchEvent> touchEventList) {
      ViewManager.this.onKey(primaryCode, touchEventList);
    }

    @Override
    public void onPress(int primaryCode) {
      if (eventListener != null && primaryCode != KeyEntity.INVALID_KEY_CODE) {
        eventListener.onFireFeedbackEvent(FeedbackEvent.KEY_DOWN);
      }
    }

    @Override
    public void onRelease(int primaryCode) {
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

  // Called back by keyboards.
  private final KeyEventHandler keyEventHandler;

  // Model to represent the current software keyboard state.
  @VisibleForTesting final JapaneseSoftwareKeyboardModel japaneseSoftwareKeyboardModel =
      new JapaneseSoftwareKeyboardModel();

  // The factory of parsed keyboard data.
  private final JapaneseKeyboardFactory japaneseKeyboardFactory = new JapaneseKeyboardFactory();

  private final SymbolCandidateStorage symbolCandidateStorage;

  // Current fullscreen mode
  private boolean fullscreenMode = false;

  // Current narrow mode
  private boolean narrowMode = false;

  // Current popup enabled state.
  private boolean popupEnabled = true;

  // Current voice input allowed state.
  private boolean voiceInputAllowed = false;

  private int flickSensitivity = 0;

  private CompositionMode hardwareCompositionMode = CompositionMode.HIRAGANA;

  @VisibleForTesting EmojiProviderType emojiProviderType = EmojiProviderType.NONE;

  /** Current skin type. */
  private SkinType skinType = SkinType.ORANGE_LIGHTGRAY;

  private LayoutAdjustment layoutAdjustment = LayoutAdjustment.FILL;

  /** Percentage of keyboard height */
  private int keyboardHeightRatio = 100;

  // Keycodes defined in resource files.
  // Printable keys are not defined. Refer them using character literal.
  // These are "constant values" as a matter of practice,
  // but such name like "KEYCODE_LEFT" makes Lint unhappy
  // because they are not "static final".
  private final int keycodeChartypeToKana;
  private final int keycodeChartypeTo123;
  private final int keycodeChartypeToAbc;
  private final int keycodeChartypeToKana123;
  private final int keycodeChartypeToAbc123;
  private final int keycodeSymbol;
  private final int keycodeUndo;
  private final int keycodeCapslock;
  private final int keycodeAlt;
  private final int keycodeMenuDialog;
  private final int keycodeImePickerDialog;

  // Handles software keyboard event and sends it to the service.
  private final KeyboardActionAdapter keyboardActionListener;

  private final PrimaryKeyCodeConverter primaryKeyCodeConverter;

  public ViewManager(Context context, final ViewEventListener listener,
                     SymbolHistoryStorage symbolHistoryStorage, ImeSwitcher imeSwitcher,
                     MenuDialogListener menuDialogListener) {
    this(context, listener, symbolHistoryStorage, imeSwitcher, menuDialogListener,
         new ProbableKeyEventGuesser(context.getAssets()));
  }

  @VisibleForTesting
  ViewManager(Context context, ViewEventListener listener,
              SymbolHistoryStorage symbolHistoryStorage, ImeSwitcher imeSwitcher,
              @Nullable MenuDialogListener menuDialogListener, ProbableKeyEventGuesser guesser) {
    Preconditions.checkNotNull(context);
    Preconditions.checkNotNull(listener);
    Preconditions.checkNotNull(imeSwitcher);

    primaryKeyCodeConverter = new PrimaryKeyCodeConverter(context, guesser);

    // Prefetch keycodes from resource
    Resources res = context.getResources();
    keycodeChartypeToKana = res.getInteger(R.integer.key_chartype_to_kana);
    keycodeChartypeTo123 = res.getInteger(R.integer.key_chartype_to_123);
    keycodeChartypeToAbc = res.getInteger(R.integer.key_chartype_to_abc);
    keycodeChartypeToKana123 = res.getInteger(R.integer.key_chartype_to_kana_123);
    keycodeChartypeToAbc123 = res.getInteger(R.integer.key_chartype_to_abc_123);
    keycodeSymbol = res.getInteger(R.integer.key_symbol);
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
    // Because an issue about native bitmap memory management on older Android,
    // there is a potential OutOfMemoryError. To reduce such an error case,
    // we retry to inflate or to create drawable when OOM is found.
    // Here is the injecting point of the procedure.
    LayoutInflater inflater = LayoutInflater.from(context);
    inflater = inflater.cloneInContext(MozcUtil.getContextWithOutOfMemoryRetrial(context));
    mozcView = MozcUtil.inflateWithOutOfMemoryRetrial(
        MozcView.class, inflater, R.layout.mozc_view, Optional.<ViewGroup>absent(), false);
    // Suppress update of View's internal state
    // until all the updates done in this method are finished. Just in case.
    mozcView.setVisibility(View.GONE);
    mozcView.setKeyboardHeightRatio(keyboardHeightRatio);
    mozcView.setEventListener(
        eventListener,
        new OnClickListener() {
          @Override
          public void onClick(View v) {
            setNarrowMode(!narrowMode);
          }
        },
        // User pushes these buttons to move position in order to see hidden text in editing rather
        // than to change his/her favorite position. So we should not apply it to preferences.
        new OnClickListener() {
          @Override
          public void onClick(View v) {
            setLayoutAdjustment(v.getContext().getResources(), LayoutAdjustment.LEFT);
            mozcView.startLayoutAdjustmentAnimation();
          }
        },
        new OnClickListener() {
          @Override
          public void onClick(View v) {
            setLayoutAdjustment(v.getContext().getResources(), LayoutAdjustment.RIGHT);
            mozcView.startLayoutAdjustmentAnimation();
          }
        });

    mozcView.setKeyEventHandler(keyEventHandler);

    setJapaneseKeyboard(japaneseSoftwareKeyboardModel.getKeyboardSpecification(),
                        Collections.<TouchEvent>emptyList());
    mozcView.setFullscreenMode(fullscreenMode);
    mozcView.setLayoutAdjustmentAndNarrowMode(layoutAdjustment, narrowMode);
    // At the moment, it is necessary to set the storage to the view, *before* setting emoji
    // provider type.
    // TODO(hidehiko): Remove the restriction.
    mozcView.setSymbolCandidateStorage(symbolCandidateStorage);
    mozcView.setEmojiProviderType(emojiProviderType);
    mozcView.setHardwareCompositionButtonImage(hardwareCompositionMode);
    mozcView.setPopupEnabled(popupEnabled);
    mozcView.setFlickSensitivity(flickSensitivity);
    mozcView.setSkinType(skinType);

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

    boolean voiceInputEnabled = voiceInputAllowed && imeSwitcher.isVoiceImeAvailable();
    menuDialog = new MenuDialog(
        mozcView.getContext(), Optional.fromNullable(menuDialogListener), voiceInputEnabled);
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
    if (outCommand.getOutput().getAllCandidateWords().getCandidatesCount() == 0 &&
        !outCommand.getInput().getRequestSuggestion()) {
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
  public KeyboardSpecification getJapaneseKeyboardSpecification() {
    return japaneseSoftwareKeyboardModel.getKeyboardSpecification();
  }

  /**
   * Set {@code EditorInfo} instance to the current view.
   */
  @Override
  public void setEditorInfo(EditorInfo attribute) {
    mozcView.setEmojiEnabled(
        EmojiUtil.isUnicodeEmojiAvailable(Build.VERSION.SDK_INT),
        EmojiUtil.isCarrierEmojiAllowed(attribute));
    mozcView.setPasswordField(MozcUtil.isPasswordField(attribute));
    mozcView.setEditorInfo(attribute);
    voiceInputAllowed = MozcUtil.isVoiceInputAllowed(attribute);

    japaneseSoftwareKeyboardModel.setInputType(attribute.inputType);
    setJapaneseKeyboard(
        japaneseSoftwareKeyboardModel.getKeyboardSpecification(),
        Collections.<TouchEvent>emptyList());
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
    MozcView mozcView = this.mozcView;

    // Try to hide a sub view from front to back.
    if (mozcView.hideSymbolInputView()) {
      return true;
    }

    return false;
  }

  /**
   * Creates and sets a keyboard represented by the resource id to the input frame.
   *
   * Note that this method requires inputFrameView is not null, and its first child is
   * the JapaneseKeyboardView.
   * @param specification Keyboard specification for the next
   */
  private void setJapaneseKeyboard(
      KeyboardSpecification specification, List<? extends TouchEvent> touchEventList) {
    eventListener.onKeyEvent(null, null, specification, touchEventList);
    if (mozcView != null) {
      Rect size = mozcView.getKeyboardSize();
      JapaneseKeyboard japaneseKeyboard =
          japaneseKeyboardFactory.get(mozcView.getResources(), specification,
                                      size.width(), size.height());
      mozcView.setJapaneseKeyboard(japaneseKeyboard);
      primaryKeyCodeConverter.setJapaneseKeyboard(japaneseKeyboard);
    }
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
      japaneseKeyboardFactory.clear();
    }

    japaneseSoftwareKeyboardModel.setKeyboardLayout(keyboardLayout);
    setJapaneseKeyboard(
        japaneseSoftwareKeyboardModel.getKeyboardSpecification(),
        Collections.<TouchEvent>emptyList());
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
      japaneseKeyboardFactory.clear();
    }

    japaneseSoftwareKeyboardModel.setInputStyle(inputStyle);
    setJapaneseKeyboard(
        japaneseSoftwareKeyboardModel.getKeyboardSpecification(),
        Collections.<TouchEvent>emptyList());
  }

  @Override
  public void setQwertyLayoutForAlphabet(boolean qwertyLayoutForAlphabet) {
    if (japaneseSoftwareKeyboardModel.isQwertyLayoutForAlphabet() != qwertyLayoutForAlphabet) {
      // If changed, clear the keyboard cache.
      japaneseKeyboardFactory.clear();
    }

    japaneseSoftwareKeyboardModel.setQwertyLayoutForAlphabet(qwertyLayoutForAlphabet);
    setJapaneseKeyboard(
        japaneseSoftwareKeyboardModel.getKeyboardSpecification(),
        Collections.<TouchEvent>emptyList());
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
   * @param isNarrowMode Whether mozc view shows in narrow mode or normal.
   */
  @Override
  public void setNarrowMode(boolean isNarrowMode) {
    this.narrowMode = isNarrowMode;
    if (mozcView != null) {
      mozcView.setLayoutAdjustmentAndNarrowMode(layoutAdjustment, isNarrowMode);
    }
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
    // Surely we don't anthing when on narrow mode already.
    if (isNarrowMode()) {
      return;
    }
    // Do nothing for the input from software keyboard.
    if (keyEventInterface == null || keyEventInterface.getNativeEvent() == null) {
      return;
    }
    // Do nothing if the key event doesn't have printable character without modifier.
    if (!command.getInput().hasKey()
        || !command.getInput().getKey().hasKeyCode()
        || command.getInput().getKey().hasModifiers()) {
      return;
    }
    // Passed all the check. Transit to narrow mode.
    hideSubInputView();
    setNarrowMode(true);
  }

  @Override
  public boolean isNarrowMode() {
    return narrowMode;
  }

  @Override
  public void setPopupEnabled(boolean popupEnabled) {
    this.popupEnabled = popupEnabled;
    if (mozcView != null) {
      mozcView.setPopupEnabled(popupEnabled);
    }
  }

  @Override
  public void setHardwareKeyboardCompositionMode(CompositionMode compositionMode) {
    hardwareCompositionMode = compositionMode;
    if (mozcView != null) {
      mozcView.setHardwareCompositionButtonImage(compositionMode);
    }
  }

  @Override
  public void setSkinType(SkinType skinType) {
    this.skinType = skinType;
    if (mozcView != null) {
      mozcView.setSkinType(skinType);
    }
  }

  @Override
  public void setLayoutAdjustment(Resources resources, LayoutAdjustment layoutAdjustment) {
    this.layoutAdjustment = layoutAdjustment;

    if (mozcView != null) {
      mozcView.setLayoutAdjustmentAndNarrowMode(layoutAdjustment, narrowMode);
    }
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
  }

  @Override
  public boolean isKeyConsumedOnViewAsynchronously(KeyEvent event) {
    return false;
  }

  @Override
  public void consumeKeyOnViewSynchronously(KeyEvent event) {
    throw new IllegalArgumentException("ViewManager doesn't consume any key event.");
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

  @VisibleForTesting
  @Override
  public JapaneseSoftwareKeyboardModel getJapaneseSoftwareKeyboardModel() {
    return japaneseSoftwareKeyboardModel;
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
  public SkinType getSkinType() {
    return skinType;
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
}
