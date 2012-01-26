// Copyright 2010-2012, Google Inc.
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
 * @fileoverview This file contains Hiragana/Katakana input mode class, which
 * is used from ime.js.
 */

'use strict';

console.assert(
    skk.mode, 'ime_mode_interface.js must be loaded prior to this module');

/**
 * Initializes Hiragana/Katakana direct input mode.
 * @constructor
 * @implements {skk.mode.InputModeInterface}
 * @param {skk.IME} ime IME instance.
 */
skk.mode.Kana = function(ime) {
  this.ime_ = ime;
  this.currentMapIndex_ = 0;
  this.composer_ = new skk.Composer(skk.mode.Kana.maps[this.currentMapIndex_]);
  console.debug('skk.mode.Kana: constructed');
};

/**
 * Array holding Roman-Kana mapping. This is used for kana type toggling.
 * @see skk.mode.Kana#toggleKanaType
 */
skk.mode.Kana.maps = [skk.Composer.map.hiragana, skk.Composer.map.katakana];

/**
 * Toggles input kana type (Hiragana to Katakana, or Katakana to Hiragana).
 */
skk.mode.Kana.prototype.toggleKanaType = function() {
  console.debug('skk.mode.Kana.toggleKanaType');
  this.currentMapIndex_++;
  this.currentMapIndex_ %= skk.mode.Kana.maps.length;
  var map = skk.mode.Kana.maps[this.currentMapIndex_];
  this.reset();
  this.composer_ = new skk.Composer(map);
};

/**
 * @inheritDoc
 */
skk.mode.Kana.prototype.reset = function() {
  console.debug('skk.mode.Kana.reset');
  this.composer_.reset();
  skk.util.showPreedit(
      this.ime_.context.contextID, this.composer_.undetermined);
};

/**
 * @inheritDoc
 */
skk.mode.Kana.prototype.enter = function() {
  console.debug('skk.mode.Kana.enter');
};

/**
 * @inheritDoc
 */
skk.mode.Kana.prototype.leave = function() {
  console.debug('skk.mode.Kana.leave');
  this.composer_.reset();
  skk.util.showPreedit(
      this.ime_.context.contextID, this.composer_.undetermined);
};

/**
 * @inheritDoc
 */
skk.mode.Kana.prototype.resume = function() {
  console.debug('skk.mode.Kana.resume');
  skk.util.showPreedit(
      this.ime_.context.contextID, this.composer_.undetermined);
};

/**
 * @inheritDoc
 */
skk.mode.Kana.prototype.suspend = function() {
  console.debug('skk.mode.Kana.suspend');
  skk.util.showPreedit(this.ime_.context.contextID, '');
};

/**
 * @inheritDoc
 */
skk.mode.Kana.prototype.prepareForKey = function(keyEvent) {
  console.debug('skk.mode.Kana.prepareForKey: ', keyEvent.key);

  if (skk.util.isArrow(keyEvent.key) && this.composer_.hasUndetermined()) {
    return null;
  }

  if (keyEvent.key === 'q') {
    this.toggleKanaType();
    return null;
  }

  if (keyEvent.key === 'l') {
    this.ime_.switchMode(skk.InputMode.ASCII);
    return null;
  }

  if (skk.util.isUpperLetter(keyEvent.key)) {
    this.ime_.enterNewMode(skk.InputMode.PREEDIT);
    return keyEvent;
  }

  if (skk.util.isBackspace(keyEvent.key) && this.composer_.hasUndetermined()) {
    this.composer_.backspace();
    skk.util.showPreedit(
        this.ime_.context.contextID, this.composer_.undetermined);
    return null;
  }

  return keyEvent;
};

/**
 * @inheritDoc
 */
skk.mode.Kana.prototype.addKey = function(keyEvent) {
  console.debug('skk.mode.Kana.addKey: ', keyEvent.key);
  console.debug('skk.mode.Kana.addKey: ',
                'undetermined = ', this.composer_.undetermined);

  if (!skk.util.isPrintable(keyEvent.key)) { return false; }
  if (keyEvent.ctrlKey || keyEvent.altKey) { return false; }

  var determAndUndeterm = this.composer_.addKey(keyEvent.key);
  var determined = determAndUndeterm[0];
  var undetermined = determAndUndeterm[1];
  chrome.experimental.input.commitText(
      {
        contextID: this.ime_.context.contextID,
        text: determined
      });
  skk.util.showPreedit(this.ime_.context.contextID, undetermined);
  return true;
};
