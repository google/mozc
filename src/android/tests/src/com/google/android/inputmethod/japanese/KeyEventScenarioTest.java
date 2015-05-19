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

import org.mozc.android.inputmethod.japanese.DependencyFactory.Dependency;
import org.mozc.android.inputmethod.japanese.JapaneseKeyboard.KeyboardSpecification;
import org.mozc.android.inputmethod.japanese.KeycodeConverter.KeyEventInterface;
import org.mozc.android.inputmethod.japanese.hardwarekeyboard.HardwareKeyboard.CompositionSwitchMode;
import org.mozc.android.inputmethod.japanese.preference.ClientSidePreference.HardwareKeyMap;
import org.mozc.android.inputmethod.japanese.preference.ClientSidePreference.InputStyle;
import org.mozc.android.inputmethod.japanese.preference.ClientSidePreference.KeyboardLayout;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Command;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.CompositionMode;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.TouchEvent;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Output;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Preedit;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Preedit.Segment;
import org.mozc.android.inputmethod.japanese.util.KeyEventSourceAccessor;
import com.google.common.base.Optional;

import android.content.res.Configuration;
import android.test.InstrumentationTestCase;
import android.view.InputDevice;
import android.view.KeyCharacterMap;
import android.view.KeyEvent;

import java.util.Collections;

/**
 * The focus of this test is to check the event flow from h/w key event,
 * MozcService, (real) SessionExecutor to MozcService.
 * Key character conversion is not the focus.
 *
 * <p>To write a scenario easily this test introduces DSL like format.
 */
public class KeyEventScenarioTest extends InstrumentationTestCase {

  private StubMozcService service;

  @Override
  public void setUp() {
    service = null;
    DependencyFactory.setDependency(Optional.<Dependency>absent());
  }

  private void verifyNarrowMode(boolean expectation) {
    waitForExecution();
    assertEquals(expectation, service.viewManager.isNarrowMode());
  }

  private void verifyKeyboardSpecificationInView(KeyboardSpecification specification) {
    waitForExecution();
    assertEquals(specification, service.viewManager.getJapaneseKeyboardSpecification());
  }

  private void verifyKeyboardSpecificationInService(KeyboardSpecification specification) {
    waitForExecution();
    assertEquals(specification, service.currentKeyboardSpecification);
  }

  private void verifyCompositionText(String expectation) {
    waitForExecution();
    Output output = service.lastOutput;
    String preeditText;
    if (!output.hasPreedit()) {
      preeditText = "";
    } else {
      Preedit preedit = output.getPreedit();
      StringBuilder builder = new StringBuilder();
      for (Segment segment : preedit.getSegmentList()) {
        builder.append(segment.getValue());
      }
      preeditText = builder.toString();
    }
    assertEquals(expectation, preeditText);
  }

  private void verifySubmittedText(String expectation) {
    waitForExecution();
    Output output = service.lastOutput;
    String actual;
    if (!output.hasResult() || !output.getResult().hasValue()) {
      actual = "";
    } else {
      actual = output.getResult().getValue();
    }
    assertEquals(expectation, actual);
  }

  private void verifyCompositionMode(CompositionMode expected) {
    waitForExecution();
    Output output = service.lastOutput;
    if (!output.hasMode()) {
      throw new IllegalStateException("No composition mode.");
    }
    assertEquals(expected, output.getMode());
  }

  private void hardwareKeyEvent(int keyCode, int metaState, int scanCode, int source) {
    KeyEvent keyEvent = new KeyEvent(0, 0, 0, keyCode, 0, metaState,
                                     KeyCharacterMap.VIRTUAL_KEYBOARD, scanCode);
    KeyEventSourceAccessor.setSource(keyEvent, source);
    service.onKeyDownInternal(keyEvent.getKeyCode(), keyEvent,
                              getDefaultDeviceConfiguration());
    service.onKeyUp(keyEvent.getKeyCode(), keyEvent);
  }

  private void hardwareKeyEventSequence(String text) {
    KeyCharacterMap keyCharacterMap =
        KeyCharacterMap.load(KeyCharacterMap.VIRTUAL_KEYBOARD);
    KeyEvent[] events = keyCharacterMap.getEvents(text.toCharArray());
    for (KeyEvent event : events) {
      KeyEventSourceAccessor.setSource(event, InputDevice.SOURCE_KEYBOARD);
      switch (event.getAction()) {
        case KeyEvent.ACTION_DOWN:
          service.onKeyDownInternal(event.getKeyCode(), event,
                                    getDefaultDeviceConfiguration());
          break;
        case KeyEvent.ACTION_UP:
          service.onKeyUp(event.getKeyCode(), event);
          break;
        default:
          throw new IllegalArgumentException("Only ACTION_DOWN/UP are allowed.");
      }
    }
  }

  private void softwareKeyEvent(int mozcKeyCode) {
    service.viewManager.getKeyboardActionListener().onKey(mozcKeyCode,
                                                          Collections.<TouchEvent>emptyList());
  }

  private void setHardwareKeyMap(HardwareKeyMap keyMap) {
    service.hardwareKeyboard.setHardwareKeyMap(keyMap);
  }

  private void setKeyboardSpecification(KeyboardLayout layout, InputStyle inputStyle) {
    service.viewManager.setKeyboardLayout(layout);
    service.viewManager.setInputStyle(inputStyle);
  }

  private void changeHardwareKeyboardCompositionMode(CompositionSwitchMode mode) {
    service.getViewEventListener().onHardwareKeyboardCompositionModeChange(mode);
  }

  // Waits for all the execution of the session executor.
  // Expected to be called from verify* methods.
  private void waitForExecution() {
    service.sessionExecutor.waitForAllQueuesForEmpty();
  }

  private static Configuration getDefaultDeviceConfiguration() {
    Configuration configuration = new Configuration();
    configuration.orientation = Configuration.ORIENTATION_PORTRAIT;
    // Note: Some other fields might be needed.
    // But currently only orientation field causes flaky test results.
    return configuration;
  }

  class StubMozcService extends MozcService {
    public Output lastOutput = Output.getDefaultInstance();
    @Override
    public boolean isInputViewShown() {
      // Always shown.
      // Showing real view from test code is really hard so use this hack instead.
      return true;
    }
    @Override
    void renderInputConnection(Command command, KeyEventInterface keyEvent) {
      // Capture the Output.
      this.lastOutput = command.getOutput();
    }
    @Override
    public void onConfigurationChanged(Configuration unused) {
      // Use static config for stable testing.
      super.onConfigurationChanged(getDefaultDeviceConfiguration());
    }
    @Override
    Configuration getConfiguration() {
      // Use static config for stable testing.
      return getDefaultDeviceConfiguration();
    }
  }

  private void createService(Dependency dependency) {
    DependencyFactory.setDependency(Optional.of(dependency));
    service = new StubMozcService();
    service.attachBaseContext(getInstrumentation().getTargetContext());
    service.onCreate();
  }

  /**
   * Normal case for Japanese keyboard.
   */
  public void testJapaneseHardWareKeyEvent_NormalCase() {
    createService(DependencyFactory.TOUCH_FRAGMENT_PREF);
    setHardwareKeyMap(HardwareKeyMap.JAPANESE109A);
    setKeyboardSpecification(KeyboardLayout.TWELVE_KEYS, InputStyle.TOGGLE_FLICK);
    // Before typing, software keyboard is shown.
    verifyNarrowMode(false);
    // Keyboard specs are for software keyboard.
    verifyKeyboardSpecificationInView(KeyboardSpecification.TWELVE_KEY_TOGGLE_FLICK_KANA);
    verifyKeyboardSpecificationInService(KeyboardSpecification.TWELVE_KEY_TOGGLE_FLICK_KANA);
    // TODO(matsuzakit): Make HardwareKeyEventSequence work for Japanese keyboard.
    // If no scan code exists, current implementation doesn't work.
    hardwareKeyEvent(KeyEvent.KEYCODE_A, 0, 0x001e, InputDevice.SOURCE_KEYBOARD);
    // Once a character is typed from h/w keyboard, narrow mode should be shown.
    verifyNarrowMode(true);
    // View's spec (== s/w kb's spec) is kept.
    verifyKeyboardSpecificationInView(KeyboardSpecification.TWELVE_KEY_TOGGLE_FLICK_KANA);
    // Service's spec is updated.
    verifyKeyboardSpecificationInService(KeyboardSpecification.HARDWARE_QWERTY_KANA);
  }

  /**
   * The same scenario as testJapaneseHardWareKeyEvent_NormalCase for default keyboard.
   */
  public void testDefaultHardWareKeyEvent_NormalCase() {
    createService(DependencyFactory.TOUCH_FRAGMENT_PREF);
    // Note: KeyEvent generated by HardwareKeyEventSequence doesn't have
    // scancode so JAPANESE109 doesn't work.
    setHardwareKeyMap(HardwareKeyMap.DEFAULT);
    setKeyboardSpecification(KeyboardLayout.TWELVE_KEYS, InputStyle.TOGGLE_FLICK);
    verifyNarrowMode(false);
    verifyKeyboardSpecificationInView(KeyboardSpecification.TWELVE_KEY_TOGGLE_FLICK_KANA);
    verifyKeyboardSpecificationInService(KeyboardSpecification.TWELVE_KEY_TOGGLE_FLICK_KANA);
    hardwareKeyEventSequence("hello");
    verifyCompositionText("へっぉ");
    verifyNarrowMode(true);
    verifyKeyboardSpecificationInView(KeyboardSpecification.TWELVE_KEY_TOGGLE_FLICK_KANA);
    verifyKeyboardSpecificationInService(KeyboardSpecification.HARDWARE_QWERTY_KANA);
    hardwareKeyEvent(KeyEvent.KEYCODE_ENTER, 0, 0, InputDevice.SOURCE_KEYBOARD);
    verifyCompositionText("");
    verifySubmittedText("へっぉ");
  }

  /**
   * Similar scenario as testJapaneseHardWareKeyEvent_NormalCase for software keyboard.
   *
   * <p>Narrow mode is not shown.
   */
  public void testSoftwareKeyboard_NormalCase() {
    createService(DependencyFactory.TOUCH_FRAGMENT_PREF);
    setKeyboardSpecification(KeyboardLayout.TWELVE_KEYS, InputStyle.TOGGLE_FLICK);
    verifyNarrowMode(false);
    verifyKeyboardSpecificationInView(KeyboardSpecification.TWELVE_KEY_TOGGLE_FLICK_KANA);
    verifyKeyboardSpecificationInService(KeyboardSpecification.TWELVE_KEY_TOGGLE_FLICK_KANA);
    softwareKeyEvent('1');
    verifyCompositionText("あ");
    // No narrow mode.
    verifyNarrowMode(false);
    verifyKeyboardSpecificationInView(KeyboardSpecification.TWELVE_KEY_TOGGLE_FLICK_KANA);
    verifyKeyboardSpecificationInService(KeyboardSpecification.TWELVE_KEY_TOGGLE_FLICK_KANA);
    softwareKeyEvent('\n');
    verifyCompositionText("");
    verifySubmittedText("あ");
  }

  /**
   * If h/w keyboard sends some special keys, narrow mode is not shown.
   */
  public void testHardwareSpecialKeyEvent_ShouldNotShowNarrowMode() {
    createService(DependencyFactory.TOUCH_FRAGMENT_PREF);
    setHardwareKeyMap(HardwareKeyMap.DEFAULT);
    verifyNarrowMode(false);
    verifyKeyboardSpecificationInView(KeyboardSpecification.TWELVE_KEY_TOGGLE_FLICK_KANA);
    verifyKeyboardSpecificationInService(KeyboardSpecification.TWELVE_KEY_TOGGLE_FLICK_KANA);
    hardwareKeyEvent(KeyEvent.KEYCODE_ENTER, 0, 0, InputDevice.SOURCE_KEYBOARD);
    hardwareKeyEvent(KeyEvent.KEYCODE_DPAD_LEFT, 0, 0, InputDevice.SOURCE_KEYBOARD);
    hardwareKeyEvent(KeyEvent.KEYCODE_DPAD_RIGHT, 0, 0, InputDevice.SOURCE_KEYBOARD);
    hardwareKeyEvent(KeyEvent.KEYCODE_DPAD_LEFT, KeyEvent.META_SHIFT_ON, 0,
                     InputDevice.SOURCE_KEYBOARD);
    hardwareKeyEvent(KeyEvent.KEYCODE_DPAD_RIGHT, KeyEvent.META_SHIFT_ON, 0,
                     InputDevice.SOURCE_KEYBOARD);
    // Nothing is input.
    verifyCompositionText("");
    // No narrow mode.
    verifyNarrowMode(false);
  }

  /**
   * Normal case for h/w 12keys keyboard.
   */
  public void testHardwareTwelveKey_NormalCase() {
    createService(DependencyFactory.TOUCH_FRAGMENT_PREF);
    setHardwareKeyMap(HardwareKeyMap.TWELVEKEY);
    setKeyboardSpecification(KeyboardLayout.TWELVE_KEYS, InputStyle.TOGGLE_FLICK);
    verifyNarrowMode(false);
    verifyKeyboardSpecificationInView(KeyboardSpecification.TWELVE_KEY_TOGGLE_FLICK_KANA);
    verifyKeyboardSpecificationInService(KeyboardSpecification.TWELVE_KEY_TOGGLE_FLICK_KANA);
    hardwareKeyEvent(KeyEvent.KEYCODE_1, 0, 0, InputDevice.SOURCE_KEYBOARD);
    hardwareKeyEvent(KeyEvent.KEYCODE_2, 0, 0, InputDevice.SOURCE_KEYBOARD);
    hardwareKeyEvent(KeyEvent.KEYCODE_3, 0, 0, InputDevice.SOURCE_KEYBOARD);
    hardwareKeyEvent(KeyEvent.KEYCODE_4, 0, 0, InputDevice.SOURCE_KEYBOARD);
    hardwareKeyEvent(KeyEvent.KEYCODE_5, 0, 0, InputDevice.SOURCE_KEYBOARD);
    hardwareKeyEvent(KeyEvent.KEYCODE_6, 0, 0, InputDevice.SOURCE_KEYBOARD);
    hardwareKeyEvent(KeyEvent.KEYCODE_7, 0, 0, InputDevice.SOURCE_KEYBOARD);
    hardwareKeyEvent(KeyEvent.KEYCODE_8, 0, 0, InputDevice.SOURCE_KEYBOARD);
    hardwareKeyEvent(KeyEvent.KEYCODE_9, 0, 0, InputDevice.SOURCE_KEYBOARD);
    hardwareKeyEvent(KeyEvent.KEYCODE_0, 0, 0, InputDevice.SOURCE_KEYBOARD);
    hardwareKeyEvent(KeyEvent.KEYCODE_STAR, 0, 0, InputDevice.SOURCE_KEYBOARD);
    verifyCompositionText("あかさたなはまやらゎ");
    verifyNarrowMode(true);
    verifyKeyboardSpecificationInView(KeyboardSpecification.TWELVE_KEY_TOGGLE_FLICK_KANA);
    verifyKeyboardSpecificationInService(KeyboardSpecification.TWELVE_KEY_TOGGLE_KANA);
    hardwareKeyEvent(KeyEvent.KEYCODE_ENTER, 0, 0, InputDevice.SOURCE_KEYBOARD);
    verifyCompositionText("");
    verifySubmittedText("あかさたなはまやらゎ");
  }

  public void testHardwareTwelveKey_CompositionModeChange() {
    createService(DependencyFactory.TOUCH_FRAGMENT_PREF);
    setHardwareKeyMap(HardwareKeyMap.TWELVEKEY);
    hardwareKeyEvent(KeyEvent.KEYCODE_1, 0, 0, InputDevice.SOURCE_KEYBOARD);
    verifyCompositionText("あ");
    changeHardwareKeyboardCompositionMode(CompositionSwitchMode.TOGGLE);
    hardwareKeyEvent(KeyEvent.KEYCODE_1, 0, 0, InputDevice.SOURCE_KEYBOARD);
    verifyCompositionText("あ@");
    changeHardwareKeyboardCompositionMode(CompositionSwitchMode.TOGGLE);
    hardwareKeyEvent(KeyEvent.KEYCODE_1, 0, 0, InputDevice.SOURCE_KEYBOARD);
    verifyCompositionText("あ@あ");
  }

  public void testHardwareTwelveKey_SpecialKeysFor12Keys() {
    createService(DependencyFactory.TOUCH_FRAGMENT_PREF);
    setHardwareKeyMap(HardwareKeyMap.TWELVEKEY);

    hardwareKeyEvent(KeyEvent.KEYCODE_1, 0, 0, InputDevice.SOURCE_KEYBOARD);
    // KEYCODE_POUND(#) behaves as KEYCODE_ENTER.
    hardwareKeyEvent(KeyEvent.KEYCODE_POUND, 0, 0, InputDevice.SOURCE_KEYBOARD);
    verifyCompositionText("");
    verifySubmittedText("あ");

    hardwareKeyEvent(KeyEvent.KEYCODE_1, 0, 0, InputDevice.SOURCE_KEYBOARD);
    // KEYCODE_DPAD_CENTER(#) behaves as KEYCODE_ENTER.
    hardwareKeyEvent(KeyEvent.KEYCODE_DPAD_CENTER, 0, 0, InputDevice.SOURCE_KEYBOARD);
    verifyCompositionText("");
    verifySubmittedText("あ");
  }

  /**
   * Zen/Han key (scan-code 0x29) changes composition mode.
   */
  public void testJapaneseHardWareKeyEvent_ZenHanKey() {
    createService(DependencyFactory.TOUCH_FRAGMENT_PREF);
    setHardwareKeyMap(HardwareKeyMap.JAPANESE109A);
    hardwareKeyEvent(KeyEvent.KEYCODE_GRAVE, 0, 0x29, InputDevice.SOURCE_KEYBOARD);
    verifyCompositionMode(CompositionMode.HALF_ASCII);
    // No characters (including grave) should be shown.
    verifyCompositionText("");
  }
}
