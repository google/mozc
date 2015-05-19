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
 * @fileoverview This file contains skk.IME class implementation.
 */

'use strict';

/**
 * Root namespace.
 */
var skk = {};

/**
 * Array holding japanese vowels.
 * @const
 * @type Array.<string>
 */
skk.VOWELS = ['a', 'i', 'u', 'e', 'o'];

/**
 * Number of candidates listed at once.
 * @const
 * @type number
 */
skk.NUM_CANDIDATES = 7;

/**
 * Key of LocalStorage used for storing conversion history.
 * @const
 * @type string
 */
skk.HISTORY_STORAGE_KEY = 'conversion_history';

/**
 * Does nothing but reports error. This is useful as placeholder for interface.
 * @type function
 */
skk.abstractMethod = function() {
  console.error('skk.abstractMethod: Unimplemented abstract method');
};

/**
 * Main class. Receives IME events and delegates most of them to skk.mode.*.
 * @constructor
 * @param {Element} nacl DOM Element embedding NaCl module.
 */
skk.IME = function(nacl) {
  this.engineID = null;
  this.isActive_ = false;
  this.modeStack_ = [new skk.mode.Kana(this)];

  this.context = null;
  this.dict = new skk.Dict(nacl);
  this.registerEventHandlers();
};

/**
 * Returns enabled ime mode.
 * @return {skk.mode.InputModeInterface} Input mode object.
 */
skk.IME.prototype.getCurrentMode = function() {
  return this.modeStack_[this.modeStack_.length - 1];
};

/**
 * Switches current ime mode.
 * @param {skk.InputMode} newMode Input mode to switch.
 * @param {function(skk.InputModeInterface)=} opt_callback
 *     Callback function that takes new input mode object as an argument.
 *     This is called back before {@code enter} method is invoked.
 */
skk.IME.prototype.switchMode = function(newMode, opt_callback) {
  console.log('skk.IME.switchMode: ', newMode);
  this.leaveCurrentMode();
  this.enterNewMode(newMode, opt_callback);
};

/**
 * Suspends current ime mode, then initializes a new ime mode and enables it.
 * @param {skk.InputMode} newMode Class name to be newly initialized.
 * @param {function(skk.InputModeInterface)=} opt_callback
 *     Callback function that takes new input mode object as an argument.
 *     This is called back before {@code enter} method is invoked.
 */
skk.IME.prototype.enterNewMode = function(newMode, opt_callback) {
  console.log('skk.IME.enterNewMode: ', newMode);
  if (this.modeStack_.length !== 0) { this.getCurrentMode().suspend(); }
  var mode = new skk.mode[newMode](this);
  this.modeStack_.push(mode);
  if (!!opt_callback) { opt_callback(mode); }
  mode.enter();
};

/**
 * Disables current ime mode and resumes recently suspended mode.
 */
skk.IME.prototype.leaveCurrentMode = function() {
  this.modeStack_.pop().leave();
  if (this.modeStack_.length !== 0) { this.getCurrentMode().resume(); }
};

/**
 * Registers event handlers to the browser.
 */
skk.IME.prototype.registerEventHandlers = function() {
  var self = this;

  chrome.input.ime.onActivate.addListener(function(engineID) {
    console.log('ime.js: onActivate');
    self.isActive_ = true;
    self.engineID = engineID;
    chrome.input.ime.setCandidateWindowProperties(
        {
          engineID: self.engineID,
          properties: {
            visible: false,
            cursorVisible: false,
            pageSize: skk.NUM_CANDIDATES
          }
        });
  });

  chrome.input.ime.onDeactivated.addListener(function() {
    console.log('ime.js: onDeactivated');
    self.isActive_ = false;
    self.context = null;
    self.getCurrentMode().reset();
  });

  chrome.input.ime.onFocus.addListener(function(context) {
    console.log('ime.js: onFocus');
    console.debug('context id: ', context.contextID);
    self.context = context;
  });

  chrome.input.ime.onBlur.addListener(function(contextID) {
    console.log('ime.js: onBlur');
    if (!self.context) { return; }
    if (self.context.contextID !== contextID) { return; }
    self.getCurrentMode().reset();
    self.context = null;
  });

  chrome.input.ime.onKeyEvent.addListener(function(engine, keyEvent) {
    console.log('ime.js: onKeyEvent: ', keyEvent);
    if (keyEvent.type == 'keyup') { return true; }

    keyEvent = self.getCurrentMode().prepareForKey(keyEvent);
    if (!keyEvent) { return true; }

    var handled = self.getCurrentMode().addKey(keyEvent);
    console.debug('ime.js: onKeyEvent: handled = ', handled);
    return handled;
  });

  chrome.input.ime.onInputContextUpdate.addListener(function() {
    console.log('ime.js: onInputContextUpdate');
  });

  chrome.input.ime.onCandidateClicked.addListener(
      function(engineID, candidateID, button) {
        console.log('ime.js: onCandidateClicked');
        if (self.engineID !== engineID) { return; }
        console.assert(this.ime_.getCurrentMode() instanceof
                       skk.mode.Conversion,
                       'ime.js: onCandidateClicked: Internal error: ' +
                           'Current input mode must be skk.mode.Conversion');
        self.getCurrentMode().commitCandidate(candidateID);
      });

  chrome.input.ime.onMenuItemActivated.addListener(function() {
    console.log('ime.js: onMenuItemActivated');
  });
};

// Since IME constructor requires NaCl module,
// initialization must be deferred until the module's DOM Element is created.
document.addEventListener(
    'readystatechange',
    function() {
      if (document.readyState === 'complete') {
        var naclModule = document.getElementById('skk_dict');
        new skk.IME(naclModule);
      }
    });
