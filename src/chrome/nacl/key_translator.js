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

/**
 * @fileoverview This file contains key transrator implementation.
 *
 */

'use strict';

/**
 * Function key mapping table.
 * Keycode is defined on http://www.w3.org/TR/uievents/#keyboard-key-codes
 * @const
 * @type {!Object.<string, string>}
 * @private
 */
mozc.FUNCTION_KEY_MAP_ = {
  'HistoryBack': 'F1',
  'HistoryForward': 'F2',
  'BrowserRefresh': 'F3',
  'ChromeOSFullscreen': 'F4',
  'ChromeOSSwitchWindow': 'F5',
  'BrightnessDown': 'F6',
  'BrightnessUp': 'F7',
  'AudioVolumeMute': 'F8',
  'AudioVolumeDown': 'F9',
  'AudioVolumeUp': 'F10'
};

/**
 * Normal key code mapping table for JP keyboard.
 * Keycode is defined on http://www.w3.org/TR/uievents/#keyboard-key-codes
 * @const
 * @type {!Object.<string, !Array.<string>>}
 * @private
 */
mozc.KEY_CODE_MAP_JP_ = {
  'Digit1': ['1', '!', '\u306C', '\u306C'],  // 'ぬ', 'ぬ'
  'Digit2': ['2', '"', '\u3075', '\u3075'],  // 'ふ', 'ふ'
  'Digit3': ['3', '#', '\u3042', '\u3041'],  // 'あ', 'ぁ'
  'Digit4': ['4', '$', '\u3046', '\u3045'],  // 'う', 'ぅ'
  'Digit5': ['5', '%', '\u3048', '\u3047'],  // 'え', 'ぇ'
  'Digit6': ['6', '&', '\u304A', '\u3049'],  // 'お', 'ぉ'
  'Digit7': ['7', '\'', '\u3084', '\u3083'],  // 'や', 'ゃ'
  'Digit8': ['8', '(', '\u3086', '\u3085'],  // 'ゆ', 'ゅ'
  'Digit9': ['9', ')', '\u3088', '\u3087'],  // 'よ', 'ょ'
  'Digit0': ['0', '0', '\u308F', '\u3092'],  // 'わ', 'を'
  'Minus': ['-', '=', '\u307B', '\u307B'],  // 'ほ', 'ほ'
  'Equal': ['^', '~', '\u3078', '\u3078'],  // 'へ', 'へ'
  'IntlYen': ['\\', '|', '\u30FC', '\u30FC'],  // 'ー', 'ー'

  'KeyQ': ['q', 'Q', '\u305F', '\u305F'],  // 'た', 'た'
  'KeyW': ['w', 'W', '\u3066', '\u3066'],  // 'て', 'て'
  'KeyE': ['e', 'E', '\u3044', '\u3043'],  // 'い', 'ぃ'
  'KeyR': ['r', 'R', '\u3059', '\u3059'],  // 'す', 'す'
  'KeyT': ['t', 'T', '\u304B', '\u304B'],  // 'か', 'か'
  'KeyY': ['y', 'Y', '\u3093', '\u3093'],  // 'ん', 'ん'
  'KeyU': ['u', 'U', '\u306A', '\u306A'],  // 'な', 'な'
  'KeyI': ['i', 'I', '\u306B', '\u306B'],  // 'に', 'に'
  'KeyO': ['o', 'O', '\u3089', '\u3089'],  // 'ら', 'ら'
  'KeyP': ['p', 'P', '\u305B', '\u305B'],  // 'せ', 'せ'
  'BracketLeft': ['@', '`', '\u309B', '\u309B'],  // '゛', '゛'
  'BracketRight': ['[', '{', '\u309C', '\u300C'],  // '゜', '「'

  'KeyA': ['a', 'A', '\u3061', '\u3061'],  // 'ち', 'ち'
  'KeyS': ['s', 'S', '\u3068', '\u3068'],  // 'と', 'と'
  'KeyD': ['d', 'D', '\u3057', '\u3057'],  // 'し', 'し'
  'KeyF': ['f', 'F', '\u306F', '\u306F'],  // 'は', 'は'
  'KeyG': ['g', 'G', '\u304D', '\u304D'],  // 'き', 'き'
  'KeyH': ['h', 'H', '\u304F', '\u304F'],  // 'く', 'く'
  'KeyJ': ['j', 'J', '\u307E', '\u307E'],  // 'ま', 'ま'
  'KeyK': ['k', 'K', '\u306E', '\u306E'],  // 'の', 'の'
  'KeyL': ['l', 'L', '\u308A', '\u308A'],  // 'り', 'り'
  'Semicolon': [';', '+', '\u308C', '\u308C'],  // 'れ', 'れ'
  'Quote': [':', '*', '\u3051', '\u3051'],  // 'け', 'け'
  'Backslash': [']', '}', '\u3080', '\u300D'],  // 'む', '」'

  'KeyZ': ['z', 'Z', '\u3064', '\u3063'],  // 'つ', 'っ'
  'KeyX': ['x', 'X', '\u3055', '\u3055'],  // 'さ', 'さ'
  'KeyC': ['c', 'C', '\u305D', '\u305D'],  // 'そ', 'そ'
  'KeyV': ['v', 'V', '\u3072', '\u3072'],  // 'ひ', 'ひ'
  'KeyB': ['b', 'B', '\u3053', '\u3053'],  // 'こ', 'こ'
  'KeyN': ['n', 'N', '\u307F', '\u307F'],  // 'み', 'み'
  'KeyM': ['m', 'M', '\u3082', '\u3082'],  // 'も', 'も'
  'Comma': [',', '<', '\u306D', '\u3001'],  // 'ね', '、'
  'Period': ['.', '>', '\u308B', '\u3002'],  // 'る', '。'
  'Slash': ['/', '?', '\u3081', '\u30FB'],  // 'め', '・'
  'IntlRo': ['\\', '_', '\u308D', '\u308D']  // 'ろ', 'ろ'
};

/**
 * Normal key code mapping table for US keyboard.
 * Keycode is defined on http://www.w3.org/TR/uievents/#keyboard-key-codes
 * @const
 * @type {!Object.<string, !Array.<string>>}
 * @private
 */
mozc.KEY_CODE_MAP_US_ = {
  'Backquote': ['`', '~', '\u308D', '\u308D'],  // 'ろ', 'ろ'
  'Digit1': ['1', '!', '\u306C', '\u306C'],  // 'ぬ', 'ぬ'
  'Digit2': ['2', '@', '\u3075', '\u3075'],  // 'ふ', 'ふ'
  'Digit3': ['3', '#', '\u3042', '\u3041'],  // 'あ', 'ぁ'
  'Digit4': ['4', '$', '\u3046', '\u3045'],  // 'う', 'ぅ'
  'Digit5': ['5', '%', '\u3048', '\u3047'],  // 'え', 'ぇ'
  'Digit6': ['6', '^', '\u304A', '\u3049'],  // 'お', 'ぉ'
  'Digit7': ['7', '&', '\u3084', '\u3083'],  // 'や', 'ゃ'
  'Digit8': ['8', '*', '\u3086', '\u3085'],  // 'ゆ', 'ゅ'
  'Digit9': ['9', '(', '\u3088', '\u3087'],  // 'よ', 'ょ'
  'Digit0': ['0', ')', '\u308F', '\u3092'],  // 'わ', 'を'
  'Minus': ['-', '_', '\u307B', '\u30FC'],  // 'ほ', 'ー'
  'Equal': ['=', '+', '\u3078', '\u3078'],  // 'へ', 'へ'

  'KeyQ': ['q', 'Q', '\u305F', '\u305F'],  // 'た', 'た'
  'KeyW': ['w', 'W', '\u3066', '\u3066'],  // 'て', 'て'
  'KeyE': ['e', 'E', '\u3044', '\u3043'],  // 'い', 'ぃ'
  'KeyR': ['r', 'R', '\u3059', '\u3059'],  // 'す', 'す'
  'KeyT': ['t', 'T', '\u304B', '\u304B'],  // 'か', 'か'
  'KeyY': ['y', 'Y', '\u3093', '\u3093'],  // 'ん', 'ん'
  'KeyU': ['u', 'U', '\u306A', '\u306A'],  // 'な', 'な'
  'KeyI': ['i', 'I', '\u306B', '\u306B'],  // 'に', 'に'
  'KeyO': ['o', 'O', '\u3089', '\u3089'],  // 'ら', 'ら'
  'KeyP': ['p', 'P', '\u305B', '\u305B'],  // 'せ', 'せ'
  'BracketLeft': ['[', '{', '\u309B', '\u309B'],  // '゛', '゛'
  'BracketRight': [']', '}', '\u309C', '\u300C'],  // '゜', '「'
  'Backslash': ['\\', '|', '\u3080', '\u300D'],  // 'む', '」'

  'KeyA': ['a', 'A', '\u3061', '\u3061'],  // 'ち', 'ち'
  'KeyS': ['s', 'S', '\u3068', '\u3068'],  // 'と', 'と'
  'KeyD': ['d', 'D', '\u3057', '\u3057'],  // 'し', 'し'
  'KeyF': ['f', 'F', '\u306F', '\u306F'],  // 'は', 'は'
  'KeyG': ['g', 'G', '\u304D', '\u304D'],  // 'き', 'き'
  'KeyH': ['h', 'H', '\u304F', '\u304F'],  // 'く', 'く'
  'KeyJ': ['j', 'J', '\u307E', '\u307E'],  // 'ま', 'ま'
  'KeyK': ['k', 'K', '\u306E', '\u306E'],  // 'の', 'の'
  'KeyL': ['l', 'L', '\u308A', '\u308A'],  // 'り', 'り'
  'Semicolon': [';', ':', '\u308C', '\u308C'],  // 'れ', 'れ'
  'Quote': ['\'', '\"', '\u3051', '\u3051'],  // 'け', 'け'

  'KeyZ': ['z', 'Z', '\u3064', '\u3063'],  // 'つ', 'っ'
  'KeyX': ['x', 'X', '\u3055', '\u3055'],  // 'さ', 'さ'
  'KeyC': ['c', 'C', '\u305D', '\u305D'],  // 'そ', 'そ'
  'KeyV': ['v', 'V', '\u3072', '\u3072'],  // 'ひ', 'ひ'
  'KeyB': ['b', 'B', '\u3053', '\u3053'],  // 'こ', 'こ'
  'KeyN': ['n', 'N', '\u307F', '\u307F'],  // 'み', 'み'
  'KeyM': ['m', 'M', '\u3082', '\u3082'],  // 'も', 'も'
  'Comma': [',', '<', '\u306D', '\u3001'],  // 'ね', '、'
  'Period': ['.', '>', '\u308B', '\u3002'],  // 'る', '。'
  'Slash': ['/', '?', '\u3081', '\u30FB'],  // 'め', '・'
  // for UK-keybord
  'IntlBackslash': ['\\', '|', '', ''],
  'IntlHash': ['#', '~', '', '']
};

/**
 * Special key code mapping table for JP keyboard.
 * Keycode is defined on http://www.w3.org/TR/uievents/#keyboard-key-codes
 * @const
 * @type {!Object.<string, string>}
 * @private
 */
mozc.SPECIAL_KEY_CODE_MAP_JP_ = {
  'Escape': 'ESCAPE',
  'Backspace': 'BACKSPACE',
  'Tab': 'TAB',
  'Enter': 'ENTER',
  'NumpadMultiply': 'MULTIPLY',
  'Space': 'SPACE',
  'CapsLock': 'CAPS_LOCK',
  'KanaMode': 'KANA',
  'PageUp': 'PAGE_UP',
  'End': 'END',
  'Delete': 'DEL',
  'Home': 'HOME',
  'PageDown': 'PAGE_DOWN',
  'ArrowUp': 'UP',
  'ArrowLeft': 'LEFT',
  'ArrowRight': 'RIGHT',
  'ArrowDown': 'DOWN',
  'Numpad0': 'NUMPAD0',
  'Numpad1': 'NUMPAD1',
  'Numpad2': 'NUMPAD2',
  'Numpad3': 'NUMPAD3',
  'Numpad4': 'NUMPAD4',
  'Numpad5': 'NUMPAD5',
  'Numpad6': 'NUMPAD6',
  'Numpad7': 'NUMPAD7',
  'Numpad8': 'NUMPAD8',
  'Numpad9': 'NUMPAD9',
  'NumpadSubtract': 'SUBTRACT',
  'NumpadAdd': 'ADD',
  'NumpadDecimal': 'DECIMAL',
  'NumpadEnter': 'SEPARATOR',
  'NumpadDivide': 'DIVIDE'
};

/**
 * Special key code mapping table for US keyboard.
 * Keycode is defined on http://www.w3.org/TR/uievents/#keyboard-key-codes
 * @const
 * @type {!Object.<string, string>}
 * @private
 */
mozc.SPECIAL_KEY_CODE_MAP_US_ = {
  'Escape': 'ESCAPE',
  'Backspace': 'BACKSPACE',
  'Tab': 'TAB',
  'Enter': 'ENTER',
  'NumpadMultiply': 'MULTIPLY',
  'Space': 'SPACE',
  'CapsLock': 'CAPS_LOCK',
  'KanaMode': 'KANA',
  'PageUp': 'PAGE_UP',
  'End': 'END',
  'Delete': 'DEL',
  'Home': 'HOME',
  'PageDown': 'PAGE_DOWN',
  'ArrowUp': 'UP',
  'ArrowLeft': 'LEFT',
  'ArrowRight': 'RIGHT',
  'ArrowDown': 'DOWN',
  'Numpad0': 'NUMPAD0',
  'Numpad1': 'NUMPAD1',
  'Numpad2': 'NUMPAD2',
  'Numpad3': 'NUMPAD3',
  'Numpad4': 'NUMPAD4',
  'Numpad5': 'NUMPAD5',
  'Numpad6': 'NUMPAD6',
  'Numpad7': 'NUMPAD7',
  'Numpad8': 'NUMPAD8',
  'Numpad9': 'NUMPAD9',
  'NumpadSubtract': 'SUBTRACT',
  'NumpadAdd': 'ADD',
  'NumpadDecimal': 'DECIMAL',
  'NumpadEnter': 'SEPARATOR',
  'NumpadDivide': 'DIVIDE'
};

/**
 * The modifier key mask of Shift key.
 * @type {number}
 * @private
 */
mozc.SHIFT_KEY_MASK_ = 1;

/**
 * The modifier key mask of Control key.
 * @type {number}
 * @private
 */
mozc.CTRL_KEY_MASK_ = 1 << 1;

/**
 * The modifier key mask of Alternate key.
 * @type {number}
 * @private
 */
mozc.ALT_KEY_MASK_ = 1 << 2;

/**
 * Modifier key mask table.
 * @const
 * @type {!Object.<string, number>}
 * @private
 */
mozc.MODIFIER_KEY_CODE_MASK_MAP_ = {
  'AltLeft': mozc.ALT_KEY_MASK_,
  'AltRight': mozc.ALT_KEY_MASK_,
  'ControlLeft': mozc.CTRL_KEY_MASK_,
  'ControlRight': mozc.CTRL_KEY_MASK_,
  'ShiftLeft': mozc.SHIFT_KEY_MASK_,
  'ShiftRight': mozc.SHIFT_KEY_MASK_
};

/**
 * Key translator
 * @constructor
 */
mozc.KeyTranslator = function() {
  /**
   * Current modifier key status. It is used to handle the keyup event of
   * modifier keys.
   * @type {number}
   * @private
   */
  this.modifierKeyFlags_ = 0;
};

/**
 * Translates modifier key event and returns Mozc's KeyEvent command message.
 * @param {!ChromeKeyboardEvent} keyData key event data.
 * @return {!mozc.KeyEvent} Mozc's KeyEvent command message.
 * @private
 */
mozc.KeyTranslator.prototype.translateModifierKeyEvent_ = function(keyData) {
  var keyEvent = /** @type {!mozc.KeyEvent} */ ({});
  var currentModifierKey =
      ((keyData.shiftKey ? mozc.SHIFT_KEY_MASK_ : 0) +
       (keyData.ctrlKey ? mozc.CTRL_KEY_MASK_ : 0) +
       (keyData.altKey ? mozc.ALT_KEY_MASK_ : 0));
  // To get the current status, calculates the exclusive OR.
  currentModifierKey ^= mozc.MODIFIER_KEY_CODE_MASK_MAP_[keyData.code];
  if (~this.modifierKeyFlags_ & currentModifierKey) {
    // When new modifier key is pressed, overwrites modifierKeyFlags_.
    this.modifierKeyFlags_ = currentModifierKey;
  }
  if (!this.modifierKeyFlags_ || currentModifierKey) {
    // We only handle the last modifier key up event.
    return keyEvent;
  }
  keyEvent.modifier_keys = [];
  if (this.modifierKeyFlags_ & mozc.ALT_KEY_MASK_) {
    keyEvent.modifier_keys.push('ALT');
  }
  if (this.modifierKeyFlags_ & mozc.CTRL_KEY_MASK_) {
    keyEvent.modifier_keys.push('CTRL');
  }
  if (this.modifierKeyFlags_ & mozc.SHIFT_KEY_MASK_) {
    keyEvent.modifier_keys.push('SHIFT');
  }
  // Clears modifierKeyFlags_.
  this.modifierKeyFlags_ = 0;
  return keyEvent;
};

/**
 * Translates key down event and returns Mozc's KeyEvent command message.
 * @param {!ChromeKeyboardEvent} keyData key event data.
 * @param {boolean} isKanaMode Whether preedit method is KANA mode.
 * @param {string} keyboardLayout current keyboard layout.
 * @return {!mozc.KeyEvent} Mozc's KeyEvent command message.
 * @private
 */
mozc.KeyTranslator.prototype.translateKeyDownEvent_ = function(keyData,
                                                               isKanaMode,
                                                               keyboardLayout) {
  // Clears modifierKeyFlags_.
  this.modifierKeyFlags_ = 0;
  var specialKey = (keyboardLayout == 'jp') ?
                   mozc.SPECIAL_KEY_CODE_MAP_JP_[keyData.code] :
                   mozc.SPECIAL_KEY_CODE_MAP_US_[keyData.code];
  // keyData.code of function keys are not correctly set. So we use keyData.key.
  if (mozc.FUNCTION_KEY_MAP_[keyData.key]) {
    specialKey = mozc.FUNCTION_KEY_MAP_[keyData.key];
  }
  var keyCodeMap = (keyboardLayout == 'jp') ?
                   mozc.KEY_CODE_MAP_JP_[keyData.code] :
                   mozc.KEY_CODE_MAP_US_[keyData.code];

  var keyEvent = /** @type {!mozc.KeyEvent} */ ({});
  if (specialKey) {
    keyEvent.special_key = specialKey;
  }

  if (keyData.shiftKey && !keyData.altKey && !keyData.ctrlKey && keyCodeMap) {
    // If the modifier is only Shift key and keyEvent is normal key, don't put
    // any modifiers.
  } else {
    var modifiersKey = [];
    if (keyData.altKey) {
      modifiersKey.push('ALT');
    }
    if (keyData.ctrlKey) {
      modifiersKey.push('CTRL');
    }
    if (keyData.shiftKey) {
      modifiersKey.push('SHIFT');
    }
    if (modifiersKey.length) {
      keyEvent.modifier_keys = modifiersKey;
    }
  }

  if (keyCodeMap) {
    keyEvent.key_code =
        keyData.shiftKey && !keyData.altKey && !keyData.ctrlKey ?
        keyCodeMap[1].charCodeAt(0) :
        keyCodeMap[0].charCodeAt(0);
    if (isKanaMode) {
      keyEvent.key_string = keyData.shiftKey ? keyCodeMap[3] : keyCodeMap[2];
    }
  }
  return keyEvent;
};

/**
 * Translates key event and returns Mozc's KeyEvent command message.
 * @param {!ChromeKeyboardEvent} keyData key event data.
 * @param {boolean} isKanaMode Whether preedit method is KANA mode.
 * @param {string} keyboardLayout current keyboard layout.
 * @return {!mozc.KeyEvent} Mozc's KeyEvent command message.
 */
mozc.KeyTranslator.prototype.translateKeyEvent = function(keyData,
                                                          isKanaMode,
                                                          keyboardLayout) {
  // Do not disturb taking screenshot.
  if (keyData.ctrlKey && keyData.key == 'ChromeOSSwitchWindow') {
    return /** @type {!mozc.KeyEvent} */ ({});
  } else if (mozc.MODIFIER_KEY_CODE_MASK_MAP_[keyData.code]) {
    return this.translateModifierKeyEvent_(keyData);
  } else if (keyData.type == 'keydown') {
    return this.translateKeyDownEvent_(keyData, isKanaMode, keyboardLayout);
  }
  return /** @type {!mozc.KeyEvent} */ ({});
};
