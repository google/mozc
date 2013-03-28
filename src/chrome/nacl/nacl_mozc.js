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

/**
 * @fileoverview This file contains NaclMozc class implementation.
 *
 * TODO(horo): Write tests of nacl_mozc.js.
 *
 */

'use strict';

/**
 * Namespace for this extension.
 */
var mozc = window.mozc || {};

/**
 * Special key mapping table.
 * @const
 * @type {!Object.<string, string>}
 * @private
 */
mozc.SPECIAL_KEY_MAP_ = {
  'Backspace': 'BACKSPACE',
  'Tab': 'TAB',
  'Enter': 'ENTER',
  'Esc': 'ESCAPE',
  ' ': 'SPACE',
  'PageUp': 'PAGE_UP',
  'PageDown': 'PAGE_DOWN',
  'End': 'END',
  'Home': 'HOME',
  'Left': 'LEFT',
  'Up': 'UP',
  'Right': 'RIGHT',
  'Down': 'DOWN',
  'Insert': 'INSERT',
  'Delete': 'DEL',
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
 * Kana key mapping table for JP keyboard.
 * @const
 * @type {!Object.<string, Array.<string>>}
 * @private
 */
mozc.KANA_MAP_JP_ = {
  '1': ['\u306C', '\u306C'],  // 'ぬ', 'ぬ'
  '!': ['\u306C', '\u306C'],  // 'ぬ', 'ぬ'
  '2': ['\u3075', '\u3075'],  // 'ふ', 'ふ'
  '"': ['\u3075', '\u3075'],  // 'ふ', 'ふ'
  '3': ['\u3042', '\u3041'],  // 'あ', 'ぁ'
  '#': ['\u3042', '\u3041'],  // 'あ', 'ぁ'
  '4': ['\u3046', '\u3045'],  // 'う', 'ぅ'
  '$': ['\u3046', '\u3045'],  // 'う', 'ぅ'
  '5': ['\u3048', '\u3047'],  // 'え', 'ぇ'
  '%': ['\u3048', '\u3047'],  // 'え', 'ぇ'
  '6': ['\u304A', '\u3049'],  // 'お', 'ぉ'
  '&': ['\u304A', '\u3049'],  // 'お', 'ぉ'
  '7': ['\u3084', '\u3083'],  // 'や', 'ゃ'
  '\'': ['\u3084', '\u3083'],  // 'や', 'ゃ'
  '8': ['\u3086', '\u3085'],  // 'ゆ', 'ゅ'
  '(': ['\u3086', '\u3085'],  // 'ゆ', 'ゅ'
  '9': ['\u3088', '\u3087'],  // 'よ', 'ょ'
  ')': ['\u3088', '\u3087'],  // 'よ', 'ょ'
  '0': ['\u308F', '\u3092'],  // 'わ', 'を'
  '-': ['\u307B', '\u307B'],  // 'ほ', 'ほ'
  '=': ['\u307B', '\u307B'],  // 'ほ', 'ほ'
  '^': ['\u3078', '\u3078'],  // 'へ', 'へ'
  '~': ['\u3078', '\u3078'],  // 'へ', 'へ'
  'q': ['\u305F', '\u305F'],  // 'た', 'た'
  'Q': ['\u305F', '\u305F'],  // 'た', 'た'
  'w': ['\u3066', '\u3066'],  // 'て', 'て'
  'W': ['\u3066', '\u3066'],  // 'て', 'て'
  'e': ['\u3044', '\u3043'],  // 'い', 'ぃ'
  'E': ['\u3044', '\u3043'],  // 'い', 'ぃ'
  'r': ['\u3059', '\u3059'],  // 'す', 'す'
  'R': ['\u3059', '\u3059'],  // 'す', 'す'
  't': ['\u304B', '\u304B'],  // 'か', 'か'
  'T': ['\u304B', '\u304B'],  // 'か', 'か'
  'y': ['\u3093', '\u3093'],  // 'ん', 'ん'
  'Y': ['\u3093', '\u3093'],  // 'ん', 'ん'
  'u': ['\u306A', '\u306A'],  // 'な', 'な'
  'U': ['\u306A', '\u306A'],  // 'な', 'な'
  'i': ['\u306B', '\u306B'],  // 'に', 'に'
  'I': ['\u306B', '\u306B'],  // 'に', 'に'
  'o': ['\u3089', '\u3089'],  // 'ら', 'ら'
  'O': ['\u3089', '\u3089'],  // 'ら', 'ら'
  'p': ['\u305B', '\u305B'],  // 'せ', 'せ'
  'P': ['\u305B', '\u305B'],  // 'せ', 'せ'
  '@': ['\u309B', '\u309B'],  // '゛', '゛'
  '`': ['\u309B', '\u309B'],  // '゛', '゛'
  '[': ['\u309C', '\u300C'],  // '゜', '「'
  '{': ['\u309C', '\u300C'],  // '゜', '「'
  'a': ['\u3061', '\u3061'],  // 'ち', 'ち'
  'A': ['\u3061', '\u3061'],  // 'ち', 'ち'
  's': ['\u3068', '\u3068'],  // 'と', 'と'
  'S': ['\u3068', '\u3068'],  // 'と', 'と'
  'd': ['\u3057', '\u3057'],  // 'し', 'し'
  'D': ['\u3057', '\u3057'],  // 'し', 'し'
  'f': ['\u306F', '\u306F'],  // 'は', 'は'
  'F': ['\u306F', '\u306F'],  // 'は', 'は'
  'g': ['\u304D', '\u304D'],  // 'き', 'き'
  'G': ['\u304D', '\u304D'],  // 'き', 'き'
  'h': ['\u304F', '\u304F'],  // 'く', 'く'
  'H': ['\u304F', '\u304F'],  // 'く', 'く'
  'j': ['\u307E', '\u307E'],  // 'ま', 'ま'
  'J': ['\u307E', '\u307E'],  // 'ま', 'ま'
  'k': ['\u306E', '\u306E'],  // 'の', 'の'
  'K': ['\u306E', '\u306E'],  // 'の', 'の'
  'l': ['\u308A', '\u308A'],  // 'り', 'り'
  'L': ['\u308A', '\u308A'],  // 'り', 'り'
  ';': ['\u308C', '\u308C'],  // 'れ', 'れ'
  '+': ['\u308C', '\u308C'],  // 'れ', 'れ'
  ':': ['\u3051', '\u3051'],  // 'け', 'け'
  '*': ['\u3051', '\u3051'],  // 'け', 'け'
  ']': ['\u3080', '\u300D'],  // 'む', '」'
  '}': ['\u3080', '\u300D'],  // 'む', '」'
  'z': ['\u3064', '\u3063'],  // 'つ', 'っ'
  'Z': ['\u3064', '\u3063'],  // 'つ', 'っ'
  'x': ['\u3055', '\u3055'],  // 'さ', 'さ'
  'X': ['\u3055', '\u3055'],  // 'さ', 'さ'
  'c': ['\u305D', '\u305D'],  // 'そ', 'そ'
  'C': ['\u305D', '\u305D'],  // 'そ', 'そ'
  'v': ['\u3072', '\u3072'],  // 'ひ', 'ひ'
  'V': ['\u3072', '\u3072'],  // 'ひ', 'ひ'
  'b': ['\u3053', '\u3053'],  // 'こ', 'こ'
  'B': ['\u3053', '\u3053'],  // 'こ', 'こ'
  'n': ['\u307F', '\u307F'],  // 'み', 'み'
  'N': ['\u307F', '\u307F'],  // 'み', 'み'
  'm': ['\u3082', '\u3082'],  // 'も', 'も'
  'M': ['\u3082', '\u3082'],  // 'も', 'も'
  ',': ['\u306D', '\u3001'],  // 'ね', '、'
  '<': ['\u306D', '\u3001'],  // 'ね', '、'
  '.': ['\u308B', '\u3002'],  // 'る', '。'
  '>': ['\u308B', '\u3002'],  // 'る', '。'
  '/': ['\u3081', '\u30FB'],  // 'め', '・'
  '?': ['\u3081', '\u30FB']  // 'め', '・'
};

/**
 * Kana key mapping table for US keyboard.
 * @const
 * @type {!Object.<string, Array.<string>>}
 * @private
 */
mozc.KANA_MAP_US_ = {
  '`': ['\u308D', '\u308D'],  // 'ろ', 'ろ'
  '~': ['\u308D', '\u308D'],  // 'ろ', 'ろ'
  '1': ['\u306C', '\u306C'],  // 'ぬ', 'ぬ'
  '!': ['\u306C', '\u306C'],  // 'ぬ', 'ぬ'
  '2': ['\u3075', '\u3075'],  // 'ふ', 'ふ'
  '@': ['\u3075', '\u3075'],  // 'ふ', 'ふ'
  '3': ['\u3042', '\u3041'],  // 'あ', 'ぁ'
  '#': ['\u3042', '\u3041'],  // 'あ', 'ぁ'
  '4': ['\u3046', '\u3045'],  // 'う', 'ぅ'
  '$': ['\u3046', '\u3045'],  // 'う', 'ぅ'
  '5': ['\u3048', '\u3047'],  // 'え', 'ぇ'
  '%': ['\u3048', '\u3047'],  // 'え', 'ぇ'
  '6': ['\u304A', '\u3049'],  // 'お', 'ぉ'
  '^': ['\u304A', '\u3049'],  // 'お', 'ぉ'
  '7': ['\u3084', '\u3083'],  // 'や', 'ゃ'
  '&': ['\u3084', '\u3083'],  // 'や', 'ゃ'
  '8': ['\u3086', '\u3085'],  // 'ゆ', 'ゅ'
  '*': ['\u3086', '\u3085'],  // 'ゆ', 'ゅ'
  '9': ['\u3088', '\u3087'],  // 'よ', 'ょ'
  '(': ['\u3088', '\u3087'],  // 'よ', 'ょ'
  '0': ['\u308F', '\u3092'],  // 'わ', 'を'
  ')': ['\u308F', '\u3092'],  // 'わ', 'を'
  '-': ['\u307B', '\u30FC'],  // 'ほ', 'ー'
  '_': ['\u307B', '\u30FC'],  // 'ほ', 'ー'
  '=': ['\u3078', '\u3078'],  // 'へ', 'へ'
  '+': ['\u3078', '\u3078'],  // 'へ', 'へ'
  'q': ['\u305F', '\u305F'],  // 'た', 'た'
  'Q': ['\u305F', '\u305F'],  // 'た', 'た'
  'w': ['\u3066', '\u3066'],  // 'て', 'て'
  'W': ['\u3066', '\u3066'],  // 'て', 'て'
  'e': ['\u3044', '\u3043'],  // 'い', 'ぃ'
  'E': ['\u3044', '\u3043'],  // 'い', 'ぃ'
  'r': ['\u3059', '\u3059'],  // 'す', 'す'
  'R': ['\u3059', '\u3059'],  // 'す', 'す'
  't': ['\u304B', '\u304B'],  // 'か', 'か'
  'T': ['\u304B', '\u304B'],  // 'か', 'か'
  'y': ['\u3093', '\u3093'],  // 'ん', 'ん'
  'Y': ['\u3093', '\u3093'],  // 'ん', 'ん'
  'u': ['\u306A', '\u306A'],  // 'な', 'な'
  'U': ['\u306A', '\u306A'],  // 'な', 'な'
  'i': ['\u306B', '\u306B'],  // 'に', 'に'
  'I': ['\u306B', '\u306B'],  // 'に', 'に'
  'o': ['\u3089', '\u3089'],  // 'ら', 'ら'
  'O': ['\u3089', '\u3089'],  // 'ら', 'ら'
  'p': ['\u305B', '\u305B'],  // 'せ', 'せ'
  'P': ['\u305B', '\u305B'],  // 'せ', 'せ'
  '[': ['\u309B', '\u309B'],  // '゛', '゛'
  '{': ['\u309B', '\u309B'],  // '゛', '゛'
  ']': ['\u309C', '\u300C'],  // '゜', '「'
  '}': ['\u309C', '\u300C'],  // '゜', '「'
  '\\': ['\u3080', '\u300D'],  // 'む', '」'
  '|': ['\u3080', '\u300D'],  // 'む', '」'
  'a': ['\u3061', '\u3061'],  // 'ち', 'ち'
  'A': ['\u3061', '\u3061'],  // 'ち', 'ち'
  's': ['\u3068', '\u3068'],  // 'と', 'と'
  'S': ['\u3068', '\u3068'],  // 'と', 'と'
  'd': ['\u3057', '\u3057'],  // 'し', 'し'
  'D': ['\u3057', '\u3057'],  // 'し', 'し'
  'f': ['\u306F', '\u306F'],  // 'は', 'は'
  'F': ['\u306F', '\u306F'],  // 'は', 'は'
  'g': ['\u304D', '\u304D'],  // 'き', 'き'
  'G': ['\u304D', '\u304D'],  // 'き', 'き'
  'h': ['\u304F', '\u304F'],  // 'く', 'く'
  'H': ['\u304F', '\u304F'],  // 'く', 'く'
  'j': ['\u307E', '\u307E'],  // 'ま', 'ま'
  'J': ['\u307E', '\u307E'],  // 'ま', 'ま'
  'k': ['\u306E', '\u306E'],  // 'の', 'の'
  'K': ['\u306E', '\u306E'],  // 'の', 'の'
  'l': ['\u308A', '\u308A'],  // 'り', 'り'
  'L': ['\u308A', '\u308A'],  // 'り', 'り'
  ';': ['\u308C', '\u308C'],  // 'れ', 'れ'
  ':': ['\u308C', '\u308C'],  // 'れ', 'れ'
  '\'': ['\u3051', '\u3051'],  // 'け', 'け'
  '\"': ['\u3051', '\u3051'],  // 'け', 'け'
  'z': ['\u3064', '\u3063'],  // 'つ', 'っ'
  'Z': ['\u3064', '\u3063'],  // 'つ', 'っ'
  'x': ['\u3055', '\u3055'],  // 'さ', 'さ'
  'X': ['\u3055', '\u3055'],  // 'さ', 'さ'
  'c': ['\u305D', '\u305D'],  // 'そ', 'そ'
  'C': ['\u305D', '\u305D'],  // 'そ', 'そ'
  'v': ['\u3072', '\u3072'],  // 'ひ', 'ひ'
  'V': ['\u3072', '\u3072'],  // 'ひ', 'ひ'
  'b': ['\u3053', '\u3053'],  // 'こ', 'こ'
  'B': ['\u3053', '\u3053'],  // 'こ', 'こ'
  'n': ['\u307F', '\u307F'],  // 'み', 'み'
  'N': ['\u307F', '\u307F'],  // 'み', 'み'
  'm': ['\u3082', '\u3082'],  // 'も', 'も'
  'M': ['\u3082', '\u3082'],  // 'も', 'も'
  ',': ['\u306D', '\u3001'],  // 'ね', '、'
  '<': ['\u306D', '\u3001'],  // 'ね', '、'
  '.': ['\u308B', '\u3002'],  // 'る', '。'
  '>': ['\u308B', '\u3002'],  // 'る', '。'
  '/': ['\u3081', '\u30FB'],  // 'め', '・'
  '?': ['\u3081', '\u30FB']  // 'め', '・'
};

/**
 * The candidate window size.
 * @type {number}
 * @private
 */
mozc.CANDIDATE_WINDOW_PAGE_SIZE_ = 9;

/**
 * Enum of preedit method.
 * This is same as mozc.commands.Output.PreeditMethod in command.proto.
 * @enum {string}
 */
mozc.PreeditMethod = {
  ROMAN: 'ROMAN',
  KANA: 'KANA'
};

/**
 * Enum of composition mode.
 * This is same as mozc.commands.CompositionMode in command.proto.
 * @enum {string}
 */
mozc.CompositionMode = {
  DIRECT: 'DIRECT',
  HIRAGANA: 'HIRAGANA',
  FULL_KATAKANA: 'FULL_KATAKANA',
  HALF_ASCII: 'HALF_ASCII',
  FULL_ASCII: 'FULL_ASCII',
  HALF_KATAKANA: 'HALF_KATAKANA'
};

/**
 * Enum of menu item IDs.
 * @enum {string}
 */
mozc.MenuItemId = {
  MENU_COMPOSITION_HIRAGANA: 'MENU_COMPOSITION_HIRAGANA',
  MENU_COMPOSITION_FULL_KATAKANA: 'MENU_COMPOSITION_FULL_KATAKANA',
  MENU_COMPOSITION_FULL_ASCII: 'MENU_COMPOSITION_FULL_ASCII',
  MENU_COMPOSITION_HALF_KATAKANA: 'MENU_COMPOSITION_HALF_KATAKANA',
  MENU_COMPOSITION_HALF_ASCII: 'MENU_COMPOSITION_HALF_ASCII',
  MENU_COMPOSITION_DIRECT: 'MENU_COMPOSITION_DIRECT'
};

/**
 * Composition menu table.
 * @const
 * @type {Array.<!Object.<string, string>>}
 * @private
 */
mozc.COMPOSITION_MENU_TABLE_ = [
  {
    menu: mozc.MenuItemId.MENU_COMPOSITION_HIRAGANA,
    mode: mozc.CompositionMode.HIRAGANA,
    label: chrome.i18n.getMessage('compositionModeHiragana')
  },
  {
    menu: mozc.MenuItemId.MENU_COMPOSITION_FULL_KATAKANA,
    mode: mozc.CompositionMode.FULL_KATAKANA,
    label: chrome.i18n.getMessage('compositionModeFullKatakana')
  },
  {
    menu: mozc.MenuItemId.MENU_COMPOSITION_FULL_ASCII,
    mode: mozc.CompositionMode.FULL_ASCII,
    label: chrome.i18n.getMessage('compositionModeFullAscii')
  },
  {
    menu: mozc.MenuItemId.MENU_COMPOSITION_HALF_KATAKANA,
    mode: mozc.CompositionMode.HALF_KATAKANA,
    label: chrome.i18n.getMessage('compositionModeHalfKatakana')
  },
  {
    menu: mozc.MenuItemId.MENU_COMPOSITION_HALF_ASCII,
    mode: mozc.CompositionMode.HALF_ASCII,
    label: chrome.i18n.getMessage('compositionModeHalfAscii')
  },
  {
    menu: mozc.MenuItemId.MENU_COMPOSITION_DIRECT,
    mode: mozc.CompositionMode.DIRECT,
    label: chrome.i18n.getMessage('compositionModeDirect')
  }
];


/**
 * NaclMozc with IME extension API.
 * @constructor
 * @param {!HTMLElement} naclModule DOM Element of NaCl module.
 */
mozc.NaclMozc = function(naclModule) {
  /**
   * Context information which is provided by Chrome.
   * @type {InputContext}
   * @private
   */
  this.context_ = null;

  /**
   * Session id of Mozc's session.
   * @type {number}
   * @private
   */
  this.sessionID_ = 0;

  /**
   * The list of candidates.
   * This data structure is same as the 2nd argument of
   * chrome.input.ime.setCandidates().
   * @type {!Array.<!Object>}
   * @private
   */
  this.candidates_ = [];

  /**
   * The candidate window properties.
   * @type {!Object.<string, *>}
   * @private
   */
  this.candidateWindowProperties_ = {};

  /**
   * Array of callback functions.
   * Callbacks are added in postMessage_ and removed in onModuleMessage_.
   * @type {!Array.<!Function|undefined>}
   * @private
   */
  this.naclMessageCallbacks_ = [];

  /**
   * Array of callback functions which will be called when NaCl Mozc will be
   *     initialized.
   * @type {!Array.<!Function|undefined>}
   * @private
   */
  this.initializationCallbacks_ = [];

  /**
   * Keyboard layout. 'us' and 'jp' are supported.
   * @type {string}
   * @private
   */
  this.keyboardLayout_ = 'us';

  /**
   * Preedit method.
   * @type {mozc.PreeditMethod}
   * @private
   */
  this.preeditMethod_ = mozc.PreeditMethod.ROMAN;

  /**
   * Composition mode.
   * @type {mozc.CompositionMode}
   * @private
   */
  this.compositionMode_ = mozc.CompositionMode.HIRAGANA;

  /**
   * Whether the NaCl module has been initialized or not.
   * @type {boolean}
   * @private
   */
  this.isNaclInitialized_ = false;

  /**
   * Whether the JavaScript side code is handling an event or not.
   * @type {boolean}
   * @private
   */
  this.isHandlingEvent_ = false;

  /**
   * Array of waiting event handlers.
   * @type {!Array.<!Function>}
   * @private
   */
  this.waitingEventHandlers_ = [];

  /**
   * Engine id which is passed from IME Extension API.
   * @type {string}
   * @private
   */
  this.engine_id_ = '';

  /**
   * DOM Element of NaCl module.
   * @type {!HTMLElement}
   * @private
   */
  this.naclModule_ = naclModule;
  this.naclModule_.addEventListener(
      'load', this.onModuleLoad_.bind(this), true);
  this.naclModule_.addEventListener(
      'message', this.onModuleMessage_.bind(this), true);
  this.naclModule_.addEventListener(
      'progress', this.onModuleProgress_.bind(this), true);
  this.naclModule_.addEventListener(
      'error', this.onModuleError_.bind(this), true);

  // Registers event handlers.
  chrome.input.ime.onActivate.addListener(
      this.wrapAsyncHandler_(this.onActivate_));
  chrome.input.ime.onDeactivated.addListener(
      this.wrapAsyncHandler_(this.onDeactivated_));
  chrome.input.ime.onFocus.addListener(
      this.wrapAsyncHandler_(this.onFocus_));
  chrome.input.ime.onBlur.addListener(
      this.wrapAsyncHandler_(this.onBlur_));
  chrome.input.ime.onInputContextUpdate.addListener(
      this.wrapAsyncHandler_(this.onInputContextUpdate_));
  chrome.input.ime.onKeyEvent.addListener(
      this.wrapAsyncHandler_(this.onKeyEventAsync_), ['async']);
  chrome.input.ime.onCandidateClicked.addListener(
      this.wrapAsyncHandler_(this.onCandidateClicked_));
  chrome.input.ime.onMenuItemActivated.addListener(
      this.wrapAsyncHandler_(this.onMenuItemActivated_));
};

/**
 * Calls the callback when NaCl Mozc is initialized
 * @param {!Function} callback Function to be called when NaCl Mozc is
 *     initialized.
 */
mozc.NaclMozc.prototype.callWhenInitialized = function(callback) {
  if (this.isNaclInitialized_) {
    callback();
    return;
  }
  this.initializationCallbacks_.push(callback);
};

/**
 * Sends RELOAD command to NaCl module.
 * @param {!function(Object)=} opt_callback Function to be called with results
 *     from NaCl module.
 */
mozc.NaclMozc.prototype.sendReload = function(opt_callback) {
  this.postMozcCommand_(
      {'input': {'type': 'RELOAD'}},
      opt_callback ?
          (function(callback, response) {
            callback(response);
          }).bind(this, opt_callback) :
          undefined);
};

/**
 * Sends GET_CONFIG command to NaCl module.
 * @param {!function(Object)=} opt_callback Function to be called with results
 *     from NaCl module.
 */
mozc.NaclMozc.prototype.getConfig = function(opt_callback) {
  this.postMozcCommand_(
      {'input': {'type': 'GET_CONFIG'}},
      opt_callback ?
          (function(callback, response) {
            callback(response);
          }).bind(this, opt_callback) :
          undefined);
};

/**
 * Sets config and sends SET_CONFIG command to NaCl module.
 * @param {!Object} config Config object to be set to.
 * @param {!function(Object)=} opt_callback Function to be called with results
 *     from NaCl module.
 */
mozc.NaclMozc.prototype.setConfig = function(config, opt_callback) {
  if (config['preedit_method']) {
    this.setPreeditMethod(config['preedit_method']);
  }
  this.postMozcCommand_(
      {'input': {'type': 'SET_CONFIG', 'config': config}},
      opt_callback ?
          (function(callback, response) {
            callback(response);
          }).bind(this, opt_callback) :
          undefined);
};

/**
 * Sets preedit method
 * @param {string} newMethod The new preedit method to be set to. 'KANA' and
 *     'ROMAN' are supported.
 */
mozc.NaclMozc.prototype.setPreeditMethod = function(newMethod) {
  for (var key in mozc.PreeditMethod) {
    if (newMethod == mozc.PreeditMethod[key]) {
      this.preeditMethod_ = mozc.PreeditMethod[key];
      return;
    }
  }
  console.error('Preedit method ' + newMethod + ' is not supported.');
};

/**
 * Sends SEND_USER_DICTIONARY_COMMAND command to NaCl module.
 * @param {!Object} command User dictionary command object to be sent.
 * @param {!function(Object)=} opt_callback Function to be called with results
 *     from NaCl module.
 */
mozc.NaclMozc.prototype.sendUserDictionaryCommand = function(command,
                                                             opt_callback) {
  this.postMozcCommand_(
    {
      'input': {
        'type': 'SEND_USER_DICTIONARY_COMMAND',
        'user_dictionary_command': command
      }
    },
    opt_callback ?
        (function(callback, response) {
          callback(response['output']['user_dictionary_command_status']);
        }).bind(this, opt_callback) :
        undefined);
};

/**
 * Wraps event handler to be able to managed by waitingEventHandlers_.
 * @param {!Function} handler Event handler.
 * @return {!Function} Wraped Event handler.
 * @private
 */
mozc.NaclMozc.prototype.wrapAsyncHandler_ = function(handler) {
  return (function(handler) {
    this.waitingEventHandlers_.push(
        Function.prototype.apply.bind(
            handler,
            this,
            Array.prototype.slice.call(arguments, 1)));
    this.executeWatingEventHandlers_();
  }).bind(this, handler);
};

/**
 * Exexutes the waiting event handlers.
 * This function is used to prevent the event handlers from being executed while
 * the NaCl module is being initialized or another event handler is being
 * executed or waiting for the callback from the NaCl module.
 * @private
 */
mozc.NaclMozc.prototype.executeWatingEventHandlers_ = function() {
  while (this.isNaclInitialized_ &&
         !this.isHandlingEvent_ &&
         !this.hasNaclMessageCallback_() &&
         this.waitingEventHandlers_.length != 0) {
    this.isHandlingEvent_ = true;
    this.waitingEventHandlers_.shift()();
    this.isHandlingEvent_ = false;
  }
};

/**
 * Returns true if naclMessageCallbacks_ has callback object.
 * @return {boolean} naclMessageCallbacks_ has callback object or not.
 * @private
 */
mozc.NaclMozc.prototype.hasNaclMessageCallback_ = function() {
  for (var callbackId in this.naclMessageCallbacks_) {
    return true;
  }
  return false;
};

/**
 * Posts a message to NaCl module.
 * @param {!Object} message Message object to be posted to NaCl module.
 * @param {!function(Object)=} opt_callback Function to be called with results
 *     from NaCl module.
 * @private
 */
mozc.NaclMozc.prototype.postMessage_ = function(message, opt_callback) {
  if (!this.isNaclInitialized_) {
    console.error('NaCl module is not initialized yet.');
    return;
  }
  var callbackId = 0;
  while (callbackId in this.naclMessageCallbacks_) {
    ++callbackId;
  }
  this.naclMessageCallbacks_[callbackId] = opt_callback;
  message['id'] = callbackId;
  this.naclModule_['postMessage'](JSON.stringify(message));
};

/**
 * Posts a message which wraps the Mozc command to NaCl module.
 * @param {!Object} command Command object to be posted to NaCl module.
 * @param {!function(Object)=} opt_callback Function to be called with results
 *     from NaCl module.
 * @private
 */
mozc.NaclMozc.prototype.postMozcCommand_ = function(command, opt_callback) {
  this.postMessage_({'cmd': command}, opt_callback);
};

/**
 * Posts an event which is spesific to NaCl Mozc such as 'SyncToFile'.
 * @param {!Object} event Event object to be posted to NaCl module.
 * @param {!function(Object)=} opt_callback Function to be called with results
 *     from NaCl module.
 * @private
 */
mozc.NaclMozc.prototype.postNaclMozcEvent_ = function(event, opt_callback) {
  this.postMessage_({'event': event}, opt_callback);
};

/**
 * Outputs the response command from NaCl module.
 * @param {Object} mozcCommand Response command object from NaCl module.
 * @private
 */
mozc.NaclMozc.prototype.outputResponse_ = function(mozcCommand) {
  if (!mozcCommand || !mozcCommand['output']) {
    return;
  }
  this.updatePreedit_(mozcCommand['output']['preedit'] || null);
  this.commitResult_(mozcCommand['output']['result'] || null);
  this.updateCandidates_(mozcCommand['output']['candidates'] || null);
  if (mozcCommand['output']['mode']) {
    var new_mode = mozcCommand['output']['mode'];
    if (this.compositionMode_ != new_mode) {
      this.compositionMode_ = new_mode;
      this.updateMenuItems_();
    }
  }
};

/**
 * Updates preedit composition.
 * @param {Object} mozcPreedit The preedit data passed from NaCl Mozc module.
 * @private
 */
mozc.NaclMozc.prototype.updatePreedit_ = function(mozcPreedit) {
  if (!mozcPreedit) {
    chrome.input.ime.setComposition({
        'contextID': this.context_.contextID,
        'text': '',
        'cursor': 0});
    return;
  }

  var preeditString = '';
  var preeditSegments = [];

  for (var i = 0; i < mozcPreedit['segment']['length']; ++i) {
    var segment = {
      'start': preeditString.length,
      'end': preeditString.length + mozcPreedit['segment'][i]['value']['length']
    };
    if (mozcPreedit['segment'][i]['annotation'] == 'UNDERLINE') {
      segment.style = 'underline';
    } else if (mozcPreedit['segment'][i]['annotation'] == 'HIGHLIGHT') {
      segment.style = 'doubleUnderline';
    }
    preeditSegments.push(segment);
    preeditString += mozcPreedit['segment'][i]['value'];
  }

  // We do not use a cursor position obtained from Mozc. It is because the
  // cursor position is used to locate the candidate window.
  var cursor = 0;
  if (mozcPreedit['highlighted_position'] != undefined) {
    cursor = mozcPreedit['highlighted_position'];
  } else if (mozcPreedit['cursor'] != undefined) {
    cursor = mozcPreedit['cursor'];
  }

  chrome.input.ime.setComposition({
      'contextID': this.context_.contextID,
      'text': preeditString,
      'cursor': cursor,
      'segments': preeditSegments});
};

/**
 * Updates the candidate window properties.
 * This method calls chrome.input.ime.setCandidateWindowProperties() with
 * the properties which have been changed.
 * @param {!Object.<string, *>} properties The new properties.
 * @private
 */
mozc.NaclMozc.prototype.updateCandidateWindowProperties_ =
    function(properties) {
  var diffProperties = {};
  var propertyNames = ['cursorVisible',
                       'vertical',
                       'pageSize',
                       'auxiliaryTextVisible',
                       'auxiliaryText',
                       'visible'];
  var changed = false;
  for (var i = 0; i < propertyNames.length; ++i) {
    var name = propertyNames[i];
    if (this.candidateWindowProperties_[name] != properties[name]) {
      diffProperties[name] = properties[name];
      this.candidateWindowProperties_[name] = properties[name];
      changed = true;
    }
  }
  if (changed) {
    chrome.input.ime.setCandidateWindowProperties({
        'engineID': this.engine_id_,
        'properties': diffProperties});
  }
};

/**
 * Checks two objects are the same objects.
 * TODO(horo): Write unit test.
 * @param {Object} object1 The first object to compare.
 * @param {Object} object2 The second object to compare.
 * @return {boolean} Whether object1 and object2 are the same objects.
 * @private
 */
mozc.NaclMozc.prototype.compareObjects_ = function(object1, object2) {
  for (var key in object1) {
    if (typeof(object1[key]) != typeof(object2[key])) {
      return false;
    }
    if (object1[key]) {
      if (typeof(object1[key]) == 'object') {
        if (!this.compareObjects_(object1[key], object2[key])) {
          return false;
        }
      } else if (typeof(object1[key]) == 'function') {
        if (object1[key].toString() != object2[key].toString()) {
          return false;
        }
      } else {
        if (object1[key] !== object2[key]) {
          return false;
        }
      }
    } else {
      if (object1[key] !== object2[key]) {
        return false;
      }
    }
  }
  for (var key in object2) {
    if (typeof(object1[key]) == 'undefined' &&
        typeof(object2[key]) != 'undefined') {
      return false;
    }
  }
  return true;
};

/**
 * Updates the candidate window.
 * @param {Object} mozcCandidates The candidates data passed from NaCl Mozc
 *     module.
 * @private
 */
mozc.NaclMozc.prototype.updateCandidates_ = function(mozcCandidates) {
  if (!mozcCandidates) {
    this.updateCandidateWindowProperties_(
        {'visible': false, 'auxiliaryTextVisible': false });
    return;
  }

  var focusedID = 0;
  var newCandidates = [];
  var candidatesIdMap = {};
  for (var i = 0; i < mozcCandidates['candidate']['length']; ++i) {
    var item = mozcCandidates['candidate'][i];
    if (item['index'] == mozcCandidates['focused_index']) {
      focusedID = item['id'];
    }
    var newCandidate = {
      'candidate': item['value'],
      'id': item['id']
    };
    if (item['annotation']) {
      newCandidate['annotation'] = item['annotation']['description'];
      newCandidate['label'] = item['annotation']['shortcut'];
    }
    newCandidates.push(newCandidate);
    candidatesIdMap[item['id']] = newCandidate;
  }
  if (mozcCandidates['usages']) {
    for (var i = 0; i < mozcCandidates['usages']['information']['length'];
          ++i) {
      var usage = mozcCandidates['usages']['information'][i];
      for (var j = 0; j < usage['candidate_id']['length']; ++j) {
        if (candidatesIdMap[usage['candidate_id'][j]]) {
          candidatesIdMap[usage['candidate_id'][j]]['usage'] = {
            'title': usage['title'],
            'body': usage['description']
          };
        }
      }
    }
  }
  if (!this.compareObjects_(this.candidates_, newCandidates)) {
    // Calls chrome.input.ime.setCandidates() if the candidates has changed.
    chrome.input.ime.setCandidates({
      'contextID': this.context_.contextID,
      'candidates': newCandidates});
    this.candidates_ = newCandidates;
  }

  chrome.input.ime.setCursorPosition({
    'contextID': this.context_.contextID,
    'candidateID': focusedID});

  var auxiliaryText = '';
  if (mozcCandidates['footer']) {
    if (mozcCandidates['footer']['label'] != undefined) {
      auxiliaryText = mozcCandidates['footer']['label'];
    } else if (mozcCandidates['footer']['sub_label'] != undefined) {
      auxiliaryText = mozcCandidates['footer']['sub_label'];
    }
    if (mozcCandidates['footer']['index_visible'] &&
        mozcCandidates['focused_index'] != undefined) {
      if (auxiliaryText.length != 0) {
        auxiliaryText += ' ';
      }
      auxiliaryText += (mozcCandidates['focused_index'] + 1) + '/' +
                       mozcCandidates['size'];
    }
  }

  var pageSize = mozc.CANDIDATE_WINDOW_PAGE_SIZE_;
  if (mozcCandidates['category'] == 'SUGGESTION') {
    pageSize = Math.min(pageSize,
                        mozcCandidates['candidate']['length']);
  }
  this.updateCandidateWindowProperties_({
    'visible': true,
    'cursorVisible': true,
    'vertical': true,
    'pageSize': pageSize,
    'auxiliaryTextVisible': (auxiliaryText.length != 0),
    'auxiliaryText': auxiliaryText});
};

/**
 * Commits result string.
 * @param {Object} mozcResult The result data passed from NaCl Mozc module.
 * @private
 */
mozc.NaclMozc.prototype.commitResult_ = function(mozcResult) {
  if (!mozcResult) {
    return;
  }
  chrome.input.ime.commitText({
      'contextID': this.context_.contextID,
      'text': mozcResult['value']});
};

/**
 * Callback method called when IME is activated.
 * @param {string} engineID ID of the engine.
 * @private
 */
mozc.NaclMozc.prototype.onActivate_ = function(engineID) {
  this.engine_id_ = engineID;
  // Sets keyboardLayout_.
  var appDetails = chrome.app.getDetails();
  if (appDetails) {
    var input_components = chrome.app.getDetails()['input_components'];
    for (var i = 0; i < input_components.length; ++i) {
      if (input_components[i].id == engineID) {
        this.keyboardLayout_ = input_components[i]['layouts'][0];
      }
    }
  }
  this.updateMenuItems_();
};

/**
 * Callback method called when IME is deactivated.
 * @param {string} engineID ID of the engine.
 * @private
 */
mozc.NaclMozc.prototype.onDeactivated_ = function(engineID) {
  this.postNaclMozcEvent_({'type': 'SyncToFile'});
};

/**
 * Callback method called when a context acquires a focus.
 * @param {!InputContext} context The context information.
 * @private
 */
mozc.NaclMozc.prototype.onFocus_ = function(context) {
  this.context_ = context;
  this.postMozcCommand_(
      {'input': {'type': 'CREATE_SESSION'}},
      (function(response) {
        this.sessionID_ = response['output']['id'];
      }).bind(this));
};

/**
 * Callback method called when a context lost a focus.
 * @param {number} contextID ID of the context.
 * @private
 */
mozc.NaclMozc.prototype.onBlur_ = function(contextID) {
  if (!this.sessionID_) {
    console.error('Session has not been created.');
    return;
  }
  this.postMozcCommand_(
      {'input': {'type': 'DELETE_SESSION', 'id': this.sessionID_}},
      this.outputResponse_.bind(this));
  this.sessionID_ = 0;
};

/**
 * Callback method called when properties of the context is changed.
 * @param {!InputContext} context context information.
 * @private
 */
mozc.NaclMozc.prototype.onInputContextUpdate_ = function(context) {
  // TODO(horo): Notify this event to Mozc since input field type may be changed
  //             to password field.
  this.context_ = context;
};

/**
 * Callback method called when IME catches a new key event.
 * @param {string} engineID ID of the engine.
 * @param {!ChromeKeyboardEvent} keyData key event data.
 * @return {boolean|undefined} True if the keystroke was handled, false if not.
 *     In asynchronous mode the return value can be undefined because it will be
 *     ignored.
 * @private
 */
mozc.NaclMozc.prototype.onKeyEventAsync_ = function(engineID, keyData) {
  if (!this.sessionID_) {
    console.error('Session has not been created.');
    chrome.input.ime.keyEventHandled(keyData.requestId, false);
    return;
  }
  // Currently we only handle keydown events.
  // TODO(horo): Handle keyup event of modifier keys.
  //             See unix/ibus/key_event_handler.cc
  // TODO(horo): Consider Kana input mode.
  if (keyData.type != 'keydown') {
    chrome.input.ime.keyEventHandled(keyData.requestId, false);
    return;
  }

  // Do not disturb taking screenshot.
  if (keyData.ctrlKey && keyData.key == 'ChromeOSSwitchWindow') {
    chrome.input.ime.keyEventHandled(keyData.requestId, false);
    return;
  }

  var keyEvent = {};
  if (keyData.key.length == 1) {
    var charCode = keyData.key.charCodeAt(0);
    if (33 <= charCode && charCode <= 126) {
      // [33, 126] means printable characters.
      keyEvent['key_code'] = charCode;

      if (this.preeditMethod_ == 'KANA') {
        var kana = (this.keyboardLayout_ == 'jp') ?
                    mozc.KANA_MAP_JP_[keyData.key] :
                    mozc.KANA_MAP_US_[keyData.key];
        if (keyData.code == 'IntlYen') {
          kana = ['\u30FC', '\u30FC'];  // 'ー', 'ー'
        } else if (keyData.code == 'IntlRo') {
          kana = ['\u308D', '\u308D'];  // 'ろ', 'ろ'
        }
        if (kana != undefined) {
          keyEvent['key_string'] = keyData.shiftKey ?
                                   kana[1] : kana[0];
        }
      }
    }
  }
  if (mozc.SPECIAL_KEY_MAP_[keyData.key] != undefined) {
    keyEvent['special_key'] = mozc.SPECIAL_KEY_MAP_[keyData.key];
  }
  if ((keyEvent['key_code'] == undefined) &&
      (keyEvent['special_key'] == undefined)) {
    chrome.input.ime.keyEventHandled(keyData.requestId, false);
    return;
  }

  var modifiersKey = [];
  if (keyData.altKey) {
    modifiersKey.push('ALT');
  }
  if (keyData.ctrlKey) {
    modifiersKey.push('CTRL');
  }
  if (keyData.shiftKey) {
    if (keyEvent['special_key'] != undefined) {
      modifiersKey.push('SHIFT');
    }
  }
  if (modifiersKey.length) {
    keyEvent['modifier_keys'] = modifiersKey;
  }

  // TODO(horo): mode switching key handling
  keyEvent['mode'] = this.compositionMode_;
  if (this.compositionMode_ == 'DIRECT') {
    chrome.input.ime.keyEventHandled(keyData.requestId, false);
    return;
  }

  this.postMozcCommand_(
      {
        'input': {
          'type': 'SEND_KEY',
          'id': this.sessionID_,
          'key': keyEvent
        }
      },
      (function(requestId, response) {
        this.outputResponse_(response);
        chrome.input.ime.keyEventHandled(
            requestId,
            !!response['output']['consumed']);
      }).bind(this, keyData.requestId));
};

/**
 * Callback method called when candidates on candidate window is clicked.
 * @param {string} engineID ID of the engine.
 * @param {number} candidateID Index of the candidate.
 * @param {string} button Which mouse button was clicked.
 * @private
 */
mozc.NaclMozc.prototype.onCandidateClicked_ = function(engineID,
                                                       candidateID,
                                                       button) {
  this.postMozcCommand_(
      {
        'input': {
          'type': 'SEND_COMMAND',
          'id': this.sessionID_,
          'command': {
            'type': 'SELECT_CANDIDATE',
            'id': candidateID
          }
        }
      },
      this.outputResponse_.bind(this));
};

/**
 * Callback method called when menu item on uber tray is activated.
 * @param {string} engineID ID of the engine.
 * @param {string} name name of the menu item.
 * @private
 */
mozc.NaclMozc.prototype.onMenuItemActivated_ = function(engineID, name) {
  for (var i = 0; i < mozc.COMPOSITION_MENU_TABLE_.length; ++i) {
    if (name == mozc.COMPOSITION_MENU_TABLE_[i].menu) {
      this.switchCompositionMode_(mozc.COMPOSITION_MENU_TABLE_[i].mode);
      this.updateMenuItems_();
      return;
    }
  }
  console.error('Menu item ' + name + ' is not supported.');
};

/**
 * Switches the composition mode.
 * @param {mozc.CompositionMode} newMode The new composition mode to be set.
 * @private
 */
mozc.NaclMozc.prototype.switchCompositionMode_ = function(newMode) {
  var lastMode = this.compositionMode_;
  if (lastMode != mozc.CompositionMode.DIRECT &&
      newMode == mozc.CompositionMode.DIRECT) {
    this.postMozcCommand_(
        {
          'input': {
            'type': 'SEND_KEY',
            'id': this.sessionID_,
            'key': {'special_key': 'OFF'}
          }
        });
  } else if (newMode != mozc.CompositionMode.DIRECT) {
    if (lastMode == mozc.CompositionMode.DIRECT) {
      this.postMozcCommand_(
          {
            'input': {
              'type': 'SEND_KEY',
              'id': this.sessionID_,
              'key': {'special_key': 'ON'}
            }
          },
          (function(newMode, response) {
            this.postMozcCommand_(
                {
                  'input': {
                    'type': 'SEND_COMMAND',
                    'id': this.sessionID_,
                    'command': {
                      'type': 'SWITCH_INPUT_MODE',
                      'composition_mode': newMode
                    }
                  }
                });
          }).bind(this, newMode));
    } else {
      this.postMozcCommand_(
          {
            'input': {
              'type': 'SEND_COMMAND',
              'id': this.sessionID_,
              'command': {
                'type': 'SWITCH_INPUT_MODE',
                'composition_mode': newMode
              }
            }
          });
    }
  }
  this.compositionMode_ = newMode;
};

/**
 * Updates the menu items
 * @private
 */
mozc.NaclMozc.prototype.updateMenuItems_ = function() {
  var menuItems = [];
  for (var i = 0; i < mozc.COMPOSITION_MENU_TABLE_.length; ++i) {
    menuItems.push({
      'id': mozc.COMPOSITION_MENU_TABLE_[i].menu,
      'label': mozc.COMPOSITION_MENU_TABLE_[i].label,
      'checked': this.compositionMode_ == mozc.COMPOSITION_MENU_TABLE_[i].mode,
      'enabled': true,
      'visible': true
    });
  }

  chrome.input.ime.setMenuItems({
    engineID: this.engine_id_,
    'items': menuItems
  });
};

/**
 * Callback method called when the NaCl module has loaded.
 * @private
 */
mozc.NaclMozc.prototype.onModuleLoad_ = function() {
  // TODO(horo): Implement this method.
};

/**
 * Handler of the messages from the NaCl module.
 * @param {Object} message message object from the NaCl module.
 * @private
 */
mozc.NaclMozc.prototype.onModuleMessage_ = function(message) {
  var mozcResponse = {};
  try {
    mozcResponse = /** @type {!Object} */ (JSON.parse(message.data));
  } catch (e) {
    console.error('handleMessage: Error: ' + e.message);
    return;
  }
  if (mozcResponse['event'] &&
      mozcResponse['event']['type'] == 'InitializeDone') {
    this.isNaclInitialized_ = true;
    if (mozcResponse['event']['config']['preedit_method']) {
      this.setPreeditMethod(mozcResponse['event']['config']['preedit_method']);
    }
    while (this.initializationCallbacks_.length > 0) {
      this.initializationCallbacks_.shift()();
    }
    this.executeWatingEventHandlers_();
    return;
  }

  var callback = this.naclMessageCallbacks_[mozcResponse['id']];
  delete this.naclMessageCallbacks_[mozcResponse['id']];
  if (callback) {
    if (mozcResponse['cmd']) {
      callback(mozcResponse['cmd']);
    } else if (mozcResponse['event']) {
      callback(mozcResponse['event']);
    }
  }
  this.executeWatingEventHandlers_();
};

/**
 * Progress event handler of the NaCl module.
 * @param {Object} event Progress event object from the NaCl module.
 * @private
 */
mozc.NaclMozc.prototype.onModuleProgress_ = function(event) {
  if (!event) {
    console.error('progress event error: event is null');
    return;
  }
  if (event.lengthComputable && event.total > 0) {
    console.log('Loading: ' + (event.loaded / event.total * 100) + '%');
  } else {
    console.log('Loading...');
  }
};

/**
 * Error event handler of the NaCl module.
 * @private
 */
mozc.NaclMozc.prototype.onModuleError_ = function() {
  console.error('onModuleError', this.naclModule_['lastError']);
};

/**
 * New option page.
 * @param {!HTMLDocument} domDocument Document object of the option page.
 * @private
 */
mozc.NaclMozc.prototype.newOptionPage_ = function(domDocument) {
  var optionPage = new mozc.OptionPage(this, domDocument);
  optionPage.initialize();
};
