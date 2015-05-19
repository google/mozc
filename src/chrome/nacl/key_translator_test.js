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
 * @fileoverview This file contains unit tests of nacl_mozc.js.
 *
 */

'use strict';

var keyTranslator = null;
var pressedKeys = null;
var isKanaMode = false;
var keyboardLayout = '';

function setUp() {
  keyTranslator = new mozc.KeyTranslator();
  pressedKeys = {};
  isKanaMode = false;
  keyboardLayout = 'us';
}

function tearDown() {
  keyTranslator = null;
  pressedKeys = null;
  isKanaMode = false;
  keyboardLayout = '';
}

function setKanaMode(newKanaMode) {
  isKanaMode = newKanaMode;
}

function setKeyboardLayout(newLayout) {
  keyboardLayout = newLayout;
}

function keyDown(code, key) {
  var result =
      keyTranslator.translateKeyEvent({
        type: 'keydown',
        altKey: (pressedKeys['AltLeft'] || pressedKeys['AltRight']),
        ctrlKey: (pressedKeys['ControlLeft'] || pressedKeys['ControlRight']),
        shiftKey: (pressedKeys['ShiftLeft'] || pressedKeys['ShiftRight']),
        code: code,
        key: key
      }, isKanaMode, keyboardLayout);
  pressedKeys[code] = true;
  return result;
}

function keyUp(code, key, isKana, layout) {
  var result =
      keyTranslator.translateKeyEvent({
        type: 'keyup',
        altKey: (pressedKeys['AltLeft'] || pressedKeys['AltRight']),
        ctrlKey: (pressedKeys['ControlLeft'] || pressedKeys['ControlRight']),
        shiftKey: (pressedKeys['ShiftLeft'] || pressedKeys['ShiftRight']),
        code: code,
        key: key
      }, isKanaMode, keyboardLayout);
  pressedKeys[code] = false;
  return result;
}

function testTranslateKey_Space() {
  assertObjectEquals({special_key: 'SPACE'}, keyDown('Space', ' '));
  assertObjectEquals({}, keyUp('Space', ' '));
}

function testTranslateKey_F1() {
  assertObjectEquals({special_key: 'F1'}, keyDown('', 'HistoryBack'));
  assertObjectEquals({}, keyUp('', 'HistoryBack'));
}

function testTranslateKey_CtrlLeft() {
  assertObjectEquals({}, keyDown('ControlLeft', 'Ctrl'));
  assertObjectEquals({modifier_keys: ['CTRL']}, keyUp('ControlLeft', 'Ctrl'));
}

function testTranslateKey_CtrlRight() {
  assertObjectEquals({}, keyDown('ControlRight', 'Ctrl'));
  assertObjectEquals({modifier_keys: ['CTRL']}, keyUp('ControlRight', 'Ctrl'));
}

function testTranslateKey_ShiftLeft() {
  assertObjectEquals({}, keyDown('ShiftLeft', 'Shift'));
  assertObjectEquals({modifier_keys: ['SHIFT']}, keyUp('ShiftLeft', 'Shift'));
}

function testTranslateKey_ShiftRight() {
  assertObjectEquals({}, keyDown('ShiftRight', 'Shift'));
  assertObjectEquals({modifier_keys: ['SHIFT']}, keyUp('ShiftRight', 'Shift'));
}

function testTranslateKey_Ctrl_Shift() {
  // Ctrl down, Shift down, Shift up, Ctrl up
  assertObjectEquals({}, keyDown('ControlLeft', 'Ctrl'));
  assertObjectEquals({}, keyDown('ShiftLeft', 'Shift'));
  assertObjectEquals({}, keyUp('ShiftLeft', 'Shift'));
  assertObjectEquals({modifier_keys: ['CTRL', 'SHIFT']},
                     keyUp('ControlLeft', 'Ctrl'));
  // Ctrl down, Shift down, Ctrl up, Shift up
  assertObjectEquals({}, keyDown('ControlLeft', 'Ctrl'));
  assertObjectEquals({}, keyDown('ShiftLeft', 'Shift'));
  assertObjectEquals({}, keyUp('ControlLeft', 'Ctrl'));
  assertObjectEquals({modifier_keys: ['CTRL', 'SHIFT']},
                     keyUp('ShiftLeft', 'Shift'));
}

function testTranslateKey_A() {
  assertObjectEquals({key_code: 97}, keyDown('KeyA', 'a'));
  assertObjectEquals({}, keyUp('KeyA', 'a'));
}

function testTranslateKey_Ctrl_Bs() {
  // Ctrl down, BS down, BS up, Ctrl up
  assertObjectEquals({}, keyDown('ControlLeft', 'Ctrl'));
  assertObjectEquals({special_key: 'BACKSPACE', modifier_keys: ['CTRL']},
                     keyDown('Backspace', 'Backspace'));
  assertObjectEquals({}, keyUp('Backspace', 'Backspace'));
  assertObjectEquals({}, keyUp('ControlLeft', 'Ctrl'));
}

function testTranslateKey_Ctrl_Shift_Bs() {
  // Ctrl down, Shift down, BS down, BS up,Shift up, Ctrl up
  assertObjectEquals({}, keyDown('ControlLeft', 'Ctrl'));
  assertObjectEquals({}, keyDown('ShiftLeft', 'Shift'));
  assertObjectEquals({
                       special_key: 'BACKSPACE',
                       modifier_keys: ['CTRL', 'SHIFT']},
                     keyDown('Backspace', 'Backspace'));
  assertObjectEquals({}, keyUp('Backspace', 'Backspace'));
  assertObjectEquals({}, keyUp('ShiftLeft', 'Shift'));
  // TODO(horo): This behavior is same as Mac Mozc. But it shoud be
  // assertObjectEquals({}, keyUp('ControlLeft', 'Ctrl'));
  assertObjectEquals({modifier_keys: ['CTRL']}, keyUp('ControlLeft', 'Ctrl'));
}

function testTranslateKey_Shift_A() {
  // Shift + 'a'
  assertObjectEquals({}, keyDown('ShiftLeft', 'Shift'));
  assertObjectEquals({key_code: 'A'.charCodeAt(0)}, keyDown('KeyA', 'a'));
  assertObjectEquals({}, keyUp('KeyA', 'a'));
  assertObjectEquals({}, keyUp('ShiftLeft', 'Shift'));
}

function testTranslateKey_Ctrl_A() {
  // Ctrl + 'a'
  assertObjectEquals({}, keyDown('ControlLeft', 'Control'));
  assertObjectEquals({key_code: 'a'.charCodeAt(0), modifier_keys: ['CTRL']},
                     keyDown('KeyA', 'a'));
  assertObjectEquals({}, keyUp('KeyA', 'a'));
  assertObjectEquals({}, keyUp('ControlLeft', 'Control'));
}

function testTranslateKey_Ctrl_Alt_A() {
  // Ctrl down, Alt down, a down, a up, Alt up, Ctrl up
  assertObjectEquals({}, keyDown('ControlLeft', 'Control'));
  assertObjectEquals({}, keyDown('AltLeft', 'Alt'));
  assertObjectEquals({
                       key_code: 'a'.charCodeAt(0),
                       modifier_keys: ['ALT', 'CTRL']
                     },
                     keyDown('KeyA', 'a'));
  assertObjectEquals({}, keyUp('KeyA', 'a'));
  assertObjectEquals({}, keyUp('AltLeft', 'Alt'));
  assertObjectEquals({modifier_keys: ['CTRL']},
                     keyUp('ControlLeft', 'Control'));
}

function testTranslateKey_Ctrl_Shift_A() {
  // Ctrl down, Shift down, a down, a up, Shift up, Ctrl up
  assertObjectEquals({}, keyDown('ControlLeft', 'Control'));
  assertObjectEquals({}, keyDown('ShiftLeft', 'Shift'));
  assertObjectEquals({
                       key_code: 'a'.charCodeAt(0),
                       modifier_keys: ['CTRL', 'SHIFT']
                     },
                     keyDown('KeyA', 'a'));
  assertObjectEquals({}, keyUp('KeyA', 'a'));
  assertObjectEquals({}, keyUp('ShiftLeft', 'Shift'));
  assertObjectEquals({modifier_keys: ['CTRL']},
                     keyUp('ControlLeft', 'Control'));
}

function testTakingScreenShotKey() {
  assertObjectEquals({special_key: 'F5'}, keyDown('', 'ChromeOSSwitchWindow'));

  assertObjectEquals({}, keyDown('AltLeft', 'Alt'));
  assertObjectEquals({special_key: 'F5', modifier_keys: ['ALT']},
                     keyDown('', 'ChromeOSSwitchWindow'));
  assertObjectEquals({}, keyDown('ShiftLeft', 'Shift'));
  assertObjectEquals({special_key: 'F5', modifier_keys: ['ALT', 'SHIFT']},
                     keyDown('', 'ChromeOSSwitchWindow'));
  assertObjectEquals({}, keyUp('AltLeft', 'Alt'));
  assertObjectEquals({special_key: 'F5', modifier_keys: ['SHIFT']},
                     keyDown('', 'ChromeOSSwitchWindow'));
  assertObjectEquals({}, keyUp('ShiftLeft', 'Shift'));


  assertObjectEquals({}, keyDown('ControlLeft', 'Control'));
  assertObjectEquals({}, keyDown('AltLeft', 'Alt'));
  assertObjectEquals({}, keyDown('', 'ChromeOSSwitchWindow'));
  assertObjectEquals({}, keyDown('ShiftLeft', 'Shift'));
  assertObjectEquals({}, keyDown('', 'ChromeOSSwitchWindow'));
  assertObjectEquals({}, keyUp('AltLeft', 'Alt'));
  assertObjectEquals({}, keyDown('', 'ChromeOSSwitchWindow'));
  assertObjectEquals({}, keyUp('ShiftLeft', 'Shift'));
  assertObjectEquals({modifier_keys: ['ALT', 'CTRL', 'SHIFT']},
                     keyUp('ControlLeft', 'Control'));
}

function testDigit6Key_US() {
  assertObjectEquals({key_code: '6'.charCodeAt(0)}, keyDown('Digit6', '6'));
  assertObjectEquals({}, keyDown('ShiftLeft', 'Shift'));
  assertObjectEquals({key_code: '^'.charCodeAt(0)}, keyDown('Digit6', '^'));
}

function testDigit6Key_JP() {
  setKeyboardLayout('jp');
  assertObjectEquals({key_code: '6'.charCodeAt(0)}, keyDown('Digit6', '6'));
  assertObjectEquals({}, keyDown('ShiftLeft', 'Shift'));
  assertObjectEquals({key_code: '&'.charCodeAt(0)}, keyDown('Digit6', '&'));
}

function testDigit6Key_US_KANA() {
  setKanaMode(true);
  assertObjectEquals({
                       key_code: '6'.charCodeAt(0),
                       key_string: '\u304A'  // 'お'
                     },
                     keyDown('Digit6', '6'));
  assertObjectEquals({}, keyDown('ShiftLeft', 'Shift'));
  assertObjectEquals({
                       key_code: '^'.charCodeAt(0),
                       key_string: '\u3049'  // 'ぉ'
                     },
                     keyDown('Digit6', '^'));
}

function testDigit6Key_JP_KANA() {
  setKanaMode(true);
  setKeyboardLayout('jp');
  assertObjectEquals({
                       key_code: '6'.charCodeAt(0),
                       key_string: '\u304A'  // 'お'
                     },
                     keyDown('Digit6', '6'));
  assertObjectEquals({}, keyDown('ShiftLeft', 'Shift'));
  assertObjectEquals({
                       key_code: '&'.charCodeAt(0),
                       key_string: '\u3049'  // 'ぉ'
                     },
                     keyDown('Digit6', '&'));
}

function testBackquoteKey_US() {
  assertObjectEquals({key_code: '`'.charCodeAt(0)}, keyDown('Backquote', '`'));
  assertObjectEquals({}, keyDown('ShiftLeft', 'Shift'));
  assertObjectEquals({key_code: '~'.charCodeAt(0)}, keyDown('Backquote', '~'));
}

function testBackquoteKey_JP() {
  setKeyboardLayout('jp');
  assertObjectEquals({}, keyDown('Backquote', ''));
  assertObjectEquals({}, keyDown('ShiftLeft', 'Shift'));
  assertObjectEquals({modifier_keys: ['SHIFT']}, keyDown('Backquote', ''));
}

function testBackquoteKey_US_KANA() {
  setKanaMode(true);
  assertObjectEquals({
                       key_code: '`'.charCodeAt(0),
                       key_string: '\u308D'  // 'ろ'
                     },
                     keyDown('Backquote', '`'));
  assertObjectEquals({}, keyDown('ShiftLeft', 'Shift'));
  assertObjectEquals({
                       key_code: '~'.charCodeAt(0),
                       key_string: '\u308D'  // 'ろ'
                     },
                     keyDown('Backquote', '~'));
}

function testBackquoteKey_JP_KANA() {
  setKanaMode(true);
  setKeyboardLayout('jp');
  assertObjectEquals({}, keyDown('Backquote', ''));
  assertObjectEquals({}, keyDown('ShiftLeft', 'Shift'));
  assertObjectEquals({modifier_keys: ['SHIFT']}, keyDown('Backquote', ''));
}
