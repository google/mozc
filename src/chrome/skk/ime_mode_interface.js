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
 * @fileoverview This file contains IME mode interface.
 */

'use strict';

console.assert(skk, 'ime.js must be loaded prior to this module');

/**
 * Enumeration contains input mode class names.
 * @enum {string}
 */
skk.InputMode = {
  ASCII: 'Ascii',
  KANA: 'Kana',
  // modes for Kanji
  PREEDIT: 'Preedit',
  CONVERSION: 'Conversion'
};

/**
 * Namespace for input mode classes.
 */
skk.mode = {};

/**
 * Interface represents input mode.
 * @interface
 */
skk.mode.InputModeInterface = function() {};

/**
 * Resets current status of the input mode.
 */
skk.mode.InputModeInterface.prototype.reset = function() {};

/**
 * Called when the input mode was initialized and activated.
 */
skk.mode.InputModeInterface.prototype.enter = skk.abstractMethod;

/**
 * Called when the input mode was destroyed.
 */
skk.mode.InputModeInterface.prototype.leave = skk.abstractMethod;

/**
 * Called when the input mode was suspended temporarily.
 */
skk.mode.InputModeInterface.prototype.suspend = skk.abstractMethod;

/**
 * Called when the input mode was resumed from temporary suspend.
 */
skk.mode.InputModeInterface.prototype.resume = skk.abstractMethod;

/**
 * Processes key event before it sent to addKey() method.
 * This method is mainly used for mode switching.
 * @param {KeyboardEvent} keyEvent Key event sent from browser.
 * @return {?KeyboardEvent} Key event to be processed by {@code addKey}.
 *     If return value is null, the key event never sent to {@code addKey}.
 */
skk.mode.InputModeInterface.prototype.prepareForKey = skk.abstractMethod;

/**
 * Processes key event.
 * @param {KeyboardEvent} keyEvent Dispathed by onKeyEvent() handler.
 * @return {boolean} True if the given key handled by this method.
 *     Otherwise returns false to mean that system should handle it.
 */
skk.mode.InputModeInterface.prototype.addKey = skk.abstractMethod;
