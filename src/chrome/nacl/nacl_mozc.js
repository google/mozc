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
 * @fileoverview This file contains NaclMozc class implementation.
 *
 * NaclMozc class provides Japanese input method for Chrome OS. The main logic
 * of Japanese conversion is written in C++ and is executed in NaCl environment.
 * This class communicate with the NaCl module using JSON message and provides
 * Japanese input method using IME extension API (chrome.input.ime).
 *
 */

'use strict';

/**
 * Namespace for this extension.
 * @suppress {const|duplicate}
 */
var mozc = window.mozc || {};

/**
 * The candidate window size.
 * @private {number}
 */
mozc.CANDIDATE_WINDOW_PAGE_SIZE_ = 9;

/**
 * Enum of preedit method.
 * This is same as mozc.commands.Output.PreeditMethod in command.proto.
 * @enum {string}
 * @private
 */
mozc.PreeditMethod_ = {
  ROMAN: 'ROMAN',
  KANA: 'KANA'
};

/**
 * Enum of composition mode.
 * This is same as mozc.commands.CompositionMode in command.proto.
 * @enum {string}
 * @private
 */
mozc.CompositionMode_ = {
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
 * @private
 */
mozc.MenuItemId_ = {
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
 * @private {!Array.<{menu: mozc.MenuItemId_,
 *                    mode: mozc.CompositionMode_,
 *                    labelId: string}>}
 */
mozc.COMPOSITION_MENU_TABLE_ = [
  {
    menu: mozc.MenuItemId_.MENU_COMPOSITION_HIRAGANA,
    mode: mozc.CompositionMode_.HIRAGANA,
    labelId: 'compositionModeHiragana'
  },
  {
    menu: mozc.MenuItemId_.MENU_COMPOSITION_FULL_KATAKANA,
    mode: mozc.CompositionMode_.FULL_KATAKANA,
    labelId: 'compositionModeFullKatakana'
  },
  {
    menu: mozc.MenuItemId_.MENU_COMPOSITION_FULL_ASCII,
    mode: mozc.CompositionMode_.FULL_ASCII,
    labelId: 'compositionModeFullAscii'
  },
  {
    menu: mozc.MenuItemId_.MENU_COMPOSITION_HALF_KATAKANA,
    mode: mozc.CompositionMode_.HALF_KATAKANA,
    labelId: 'compositionModeHalfKatakana'
  },
  {
    menu: mozc.MenuItemId_.MENU_COMPOSITION_HALF_ASCII,
    mode: mozc.CompositionMode_.HALF_ASCII,
    labelId: 'compositionModeHalfAscii'
  },
  {
    menu: mozc.MenuItemId_.MENU_COMPOSITION_DIRECT,
    mode: mozc.CompositionMode_.DIRECT,
    labelId: 'compositionModeDirect'
  }
];


/**
 * NaclMozc with IME extension API.
 * @param {!HTMLElement} naclModule DOM Element of NaCl module.
 * @constructor
 * @struct
 * @const
 */
mozc.NaclMozc = function(naclModule) {
  /**
   * Context information which is provided by Chrome.
   * @private {InputContext}
   */
  this.context_ = null;

  /**
   * Session id of Mozc's session. This id is handled as uint64 in NaCl. But
   * JavaScript can't handle uint64. So we handle it as string in JavaScript.
   * @private {string}
   */
  this.sessionID_ = '';

  /**
   * The list of candidates.
   * This data structure is same as the 2nd argument of
   * chrome.input.ime.setCandidates().
   * @private {!Array.<{candidate: string,
   *                    id: number,
   *                    label: string,
   *                    annotation: string,
   *                    usage: {title: string, body: string}}>}
   */
  this.candidates_ = [];

  /**
   * The candidate window properties.
   * @private {!Object.<string, *>}
   */
  this.candidateWindowProperties_ = {};

  /**
   * Array of callback functions.
   * Callbacks are added in postMessage_ and removed in onModuleMessage_.
   * @private {!Array.<function(!mozc.Command)|function(!mozc.Event)|undefined>}
   */
  this.naclMessageCallbacks_ = [];

  /**
   * Array of callback functions which will be called when NaCl Mozc will be
   *     initialized.
   * @private {!Array.<function()|undefined>}
   */
  this.initializationCallbacks_ = [];

  /**
   * Keyboard layout. 'us' and 'jp' are supported.
   * @private {string}
   */
  this.keyboardLayout_ = 'us';

  /**
   * Preedit method.
   * @private {mozc.PreeditMethod_}
   */
  this.preeditMethod_ = mozc.PreeditMethod_.ROMAN;

  /**
   * Composition mode.
   * @private {mozc.CompositionMode_}
   */
  this.compositionMode_ = mozc.CompositionMode_.HIRAGANA;

  /**
   * Whether the NaCl module has been initialized or not.
   * @private {boolean}
   */
  this.isNaclInitialized_ = false;

  /**
   * Whether the JavaScript side code is handling an event or not.
   * @private {boolean}
   */
  this.isHandlingEvent_ = false;

  /**
   * Array of waiting event handlers.
   * @private {!Array.<function()>}
   */
  this.waitingEventHandlers_ = [];

  /**
   * Engine id which is passed from IME Extension API.
   * @private {string}
   */
  this.engine_id_ = '';

  /**
   * Key event translator.
   * @private {!mozc.KeyTranslator}
   */
  this.keyTranslator_ = new mozc.KeyTranslator();

  /**
   * callbackCommand_ stores the callback message which is recieved from NaCl
   * module. This callback will be cancelled when the user presses the
   * subsequent key. In the current implementation, if the subsequent key event
   * also makes callback, the second callback will be called in the timimg of
   * the first callback.
   * @private {!mozc.Callback}
   */
  this.callbackCommand_ = /** @type {!mozc.Callback} */ ({});

  /**
   * The surrounding information.
   * @private {Object.<{text: string, focus: number, anchor: number}>}
   */
  this.surroundingInfo_ = null;

  /**
   * DOM Element of NaCl module.
   * @private {!HTMLElement}
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
  // chrome.input.ime.onSurroundingTextChanged is available from ChromeOS 27.
  if (chrome.input.ime.onSurroundingTextChanged) {
    chrome.input.ime.onSurroundingTextChanged.addListener(
        this.wrapAsyncHandler_(this.onSurroundingTextChanged_));
  }
  // chrome.input.ime.onReset is available from ChromeOS 29.
  if (chrome.input.ime.onReset) {
    chrome.input.ime.onReset.addListener(this.wrapAsyncHandler_(this.onReset_));
  }
};

/**
 * Calls the callback when NaCl Mozc is initialized
 * @param {function()} callback Function to be called when NaCl Mozc is
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
 * Creates a mozc.Command object with the specified input type.
 * @param {string} type Input type of mozc.Command object.
 * @return {!mozc.Command} Created mozc.Command.
 * @private
 */
mozc.NaclMozc.prototype.createMozcCommand_ = function(type) {
  return /** @type {!mozc.Command} */ ({'input': {'type': type}, 'output': {}});
};

/**
 * Creates a mozc.Event object with the specified type.
 * @param {string} type Type of mozc.Event object.
 * @return {!mozc.Event} Created mozc.Event.
 * @private
 */
mozc.NaclMozc.prototype.createMozcEvent_ = function(type) {
  return /** @type {!mozc.Event} */ ({'type': type});
};

/**
 * Creates a mozc.SessionCommand object with the specified type.
 * @param {string} type Type of mozc.SessionCommand object.
 * @return {!mozc.SessionCommand} Created mozc.SessionCommand.
 * @private
 */
mozc.NaclMozc.prototype.createMozcSessionCommand_ = function(type) {
  return /** @type {!mozc.SessionCommand} */ ({'type': type});
};

/**
 * Sends RELOAD command to NaCl module.
 * @param {function(!mozc.Command)=} opt_callback Function to be called with
 *     results from NaCl module.
 */
mozc.NaclMozc.prototype.sendReload = function(opt_callback) {
  this.postMozcCommand_(this.createMozcCommand_('RELOAD'), opt_callback);
};

/**
 * Sends GET_CONFIG command to NaCl module.
 * @param {function(!mozc.Command)=} opt_callback Function to be called with
 *     results from NaCl module.
 */
mozc.NaclMozc.prototype.getConfig = function(opt_callback) {
  this.postMozcCommand_(this.createMozcCommand_('GET_CONFIG'), opt_callback);
};

/**
 * Sets config and sends SET_CONFIG command to NaCl module.
 * @param {!Object.<string,*>} config Config object to be set to.
 * @param {function(!mozc.Command)=} opt_callback Function to be called with
 *     results from NaCl module.
 */
mozc.NaclMozc.prototype.setConfig = function(config, opt_callback) {
  if (config['preedit_method']) {
    this.setPreeditMethod(
        /** @type {mozc.PreeditMethod_} */ (config['preedit_method']));
  }
  var mozcCommand = this.createMozcCommand_('SET_CONFIG');
  mozcCommand.input.config = config;
  this.postMozcCommand_(mozcCommand, opt_callback);
};

/**
 * Sends CLEAR_USER_HISTORY command to NaCl module.
 * @param {function(!mozc.Command)=} opt_callback Function to be called with
 *     results from NaCl module.
 */
mozc.NaclMozc.prototype.clearUserHistory = function(opt_callback) {
  this.postMozcCommand_(this.createMozcCommand_('CLEAR_USER_HISTORY'),
                        opt_callback);
};

/**
 * Sends CLEAR_USER_PREDICTION command to NaCl module.
 * @param {function(!mozc.Command)=} opt_callback Function to be called with
 *     results from NaCl module.
 */
mozc.NaclMozc.prototype.clearUserPrediction = function(opt_callback) {
  this.postMozcCommand_(this.createMozcCommand_('CLEAR_USER_PREDICTION'),
                        opt_callback);
};

/**
 * Sets preedit method
 * @param {mozc.PreeditMethod_} newMethod The new preedit method to be set to.
 */
mozc.NaclMozc.prototype.setPreeditMethod = function(newMethod) {
  for (var key in mozc.PreeditMethod_) {
    if (newMethod == mozc.PreeditMethod_[key]) {
      this.preeditMethod_ = mozc.PreeditMethod_[key];
      return;
    }
  }
  console.error('Preedit method ' + newMethod + ' is not supported.');
};

/**
 * Sends SEND_USER_DICTIONARY_COMMAND command to NaCl module.
 * @param {!mozc.UserDictionaryCommand} command User dictionary command object
 *    to be sent.
 * @param {function(!mozc.UserDictionaryCommandStatus)=} opt_callback Function
 *     to be called with results from NaCl module.
 */
mozc.NaclMozc.prototype.sendUserDictionaryCommand = function(command,
                                                             opt_callback) {
  var mozcCommand = this.createMozcCommand_('SEND_USER_DICTIONARY_COMMAND');
  mozcCommand.input.user_dictionary_command = command;
  this.postMozcCommand_(
      mozcCommand,
      opt_callback ?
          /**
          * @this {!mozc.NaclMozc}
          * @param {function(!mozc.UserDictionaryCommandStatus)} callback
          * @param {!mozc.Command} response
          */
          (function(callback, response) {
            if (response.output.user_dictionary_command_status == undefined) {
              console.log('sendUserDictionaryCommand error');
              return;
            }
            callback(response.output.user_dictionary_command_status);
          }).bind(this, opt_callback) :
          undefined);
};

/**
 * Gets the version information of NaCl Mozc module.
 * @param {function(!mozc.Event)} callback Function to be called with results
 *     from NaCl module.
 */
mozc.NaclMozc.prototype.getVersionInfo = function(callback) {
  this.postNaclMozcEvent_(this.createMozcEvent_('GetVersionInfo'), callback);
};

/**
 * Gets POS list from NaCl Mozc module.
 * @param {function(!mozc.Event)} callback Function to be called with results
 *     from NaCl module.
 */
mozc.NaclMozc.prototype.getPosList = function(callback) {
  this.postNaclMozcEvent_(this.createMozcEvent_('GetPosList'), callback);
};

/**
 * Check if all characters in the given string is a legitimate character
 * for reading.
 * @param {string} text The string to check.
 * @param {function(!mozc.Event)} callback Function to be called with results
 *     from NaCl module.
 */
mozc.NaclMozc.prototype.isValidReading = function(text, callback) {
  var event = this.createMozcEvent_('IsValidReading');
  event.data = text;
  this.postNaclMozcEvent_(event, callback);
};

/**
 * Sends callback command to NaCl module.
 * @param {function(!mozc.Command)=} opt_callback Function to be called with
 *     results from NaCl module.
 * @private
 */
mozc.NaclMozc.prototype.sendCallbackCommand_ = function(opt_callback) {
  if (!this.sessionID_) {
    console.error('Session has not been created.');
    return;
  }
  if (!this.callbackCommand_.session_command) {
    return;
  }
  var command = this.callbackCommand_.session_command;
  if (command.type == 'CONVERT_REVERSE' && this.surroundingInfo_) {
    if (this.surroundingInfo_.focus < this.surroundingInfo_.anchor) {
      command.text = this.surroundingInfo_.text.substring(
                         this.surroundingInfo_.focus,
                         this.surroundingInfo_.anchor);
    } else {
      command.text = this.surroundingInfo_.text.substring(
                         this.surroundingInfo_.anchor,
                         this.surroundingInfo_.focus);
    }
  }
  this.callbackCommand_ = /** @type {!mozc.Callback} */ ({});
  this.postMozcSessionCommand_(command, opt_callback);
};

/**
 * Wraps event handler to be able to managed by waitingEventHandlers_.
 * @param {!Function} handler Event handler.
 * @return {!Function} Wraped Event handler.
 * @private
 */
mozc.NaclMozc.prototype.wrapAsyncHandler_ = function(handler) {
  return (/**
          * @this {!mozc.NaclMozc}
          * @param {!Function} handler
          */
          (function(handler) {
            this.waitingEventHandlers_.push(
                Function.prototype.apply.bind(
                    handler,
                    this,
                    Array.prototype.slice.call(arguments, 1)));
            this.executeWatingEventHandlers_();
          })).bind(this, handler);
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
 * @param {!mozc.Message} message Message object to be posted to NaCl module.
 * @param {!(function(!mozc.Command)|function(!mozc.Event))=} opt_callback
 *     Function to be called with results from NaCl module.
 * @private
 */
mozc.NaclMozc.prototype.postMessage_ = function(message, opt_callback) {
  if (!this.isNaclInitialized_) {
    console.error('NaCl module is not initialized yet.');
    return;
  }
  var callbackId = 0;
  while (callbackId in this.naclMessageCallbacks_) {
    callbackId++;
  }
  this.naclMessageCallbacks_[callbackId] = opt_callback;
  message.id = callbackId;
  this.naclModule_['postMessage'](JSON.stringify(message));
};

/**
 * Posts a message which wraps the Mozc command to NaCl module.
 * @param {!mozc.Command} command Command object to be posted to NaCl module.
 * @param {function(!mozc.Command)=} opt_callback Function to be called with
 *     results from NaCl module.
 * @private
 */
mozc.NaclMozc.prototype.postMozcCommand_ = function(command, opt_callback) {
  this.postMessage_(
      /** @type {!mozc.Message} */ ({'cmd': command}),
      opt_callback);
};

/**
 * Posts an event which is spesific to NaCl Mozc such as 'SyncToFile'.
 * @param {!mozc.Event} event Event object to be posted to NaCl module.
 * @param {function(!mozc.Event)=} opt_callback Function to be called with
 *     results from NaCl module.
 * @private
 */
mozc.NaclMozc.prototype.postNaclMozcEvent_ = function(event, opt_callback) {
  this.postMessage_(
      /** @type {!mozc.Message} */ ({'event': event}),
      opt_callback);
};

/**
 * Posts a SessionCommand to NaCl module.
 * @param {!mozc.SessionCommand} sessionCommand SessionCommand object to be
 *    posted to NaCl module.
 * @param {function(!mozc.Command)=} opt_callback Function to be called with
 *     results from NaCl module.
 * @private
 */
mozc.NaclMozc.prototype.postMozcSessionCommand_ = function(sessionCommand,
                                                           opt_callback) {
  var mozcCommand = this.createMozcCommand_('SEND_COMMAND');
  mozcCommand.input.id = this.sessionID_;
  mozcCommand.input.command = sessionCommand;
  this.postMozcCommand_(mozcCommand, opt_callback);
};

/**
 * Processes the response command from NaCl module.
 * @param {!mozc.Command} mozcCommand Response command object from NaCl module.
 * @private
 */
mozc.NaclMozc.prototype.processResponse_ = function(mozcCommand) {
  if (!mozcCommand || !mozcCommand.output) {
    return;
  }
  if (mozcCommand.output.deletion_range) {
    // chrome.input.ime.deleteSurroundingText is available from ChromeOS 27.
    if (chrome.input.ime.deleteSurroundingText) {
      chrome.input.ime.deleteSurroundingText({
          'engineID': this.engine_id_,
          'contextID': this.context_.contextID,
          'offset': mozcCommand.output.deletion_range.offset,
          'length': mozcCommand.output.deletion_range.length
          },
          this.outputResponse_.bind(this, mozcCommand.output));
      return;
    }
  }
  this.outputResponse_(mozcCommand.output);
};

/**
 * Outputs the response command from NaCl module.
 * @param {!mozc.Output} output mozc.Output object from NaCl module.
 * @private
 */
mozc.NaclMozc.prototype.outputResponse_ = function(output) {
  this.updatePreedit_(output.preedit);
  this.commitResult_(output.result);
  this.updateCandidates_(output.candidates);
  if (output.mode) {
    var new_mode = output.mode;
    if (this.compositionMode_ != new_mode) {
      this.compositionMode_ = /** @type {mozc.CompositionMode_} */ (new_mode);
      this.updateMenuItems_();
    }
  }
  if (output.callback) {
    this.callbackCommand_ = output.callback;
    if (this.callbackCommand_.delay_millisec) {
      window.setTimeout(
          this.sendCallbackCommand_.bind(
              this,
              this.processResponse_.bind(this)),
          this.callbackCommand_.delay_millisec);
    } else {
      this.sendCallbackCommand_(this.processResponse_.bind(this));
    }
  }
};

/**
 * Updates preedit composition.
 * @param {mozc.Preedit=} opt_mozcPreedit The preedit data passed from NaCl
 *     Mozc
 * module.
 * @private
 */
mozc.NaclMozc.prototype.updatePreedit_ = function(opt_mozcPreedit) {
  if (!opt_mozcPreedit) {
    chrome.input.ime.setComposition({
      'contextID': this.context_.contextID,
      'text': '',
      'cursor': 0
    });
    return;
  }

  var preeditString = '';
  var preeditSegments = [];

  for (var i = 0; i < opt_mozcPreedit.segment.length; ++i) {
    var segment = {
      'start': preeditString.length,
      'end': preeditString.length + opt_mozcPreedit.segment[i].value_length
    };
    if (opt_mozcPreedit.segment[i].annotation == 'UNDERLINE') {
      segment.style = 'underline';
    } else if (opt_mozcPreedit.segment[i].annotation == 'HIGHLIGHT') {
      segment.style = 'doubleUnderline';
    }
    preeditSegments.push(segment);
    preeditString += opt_mozcPreedit.segment[i].value;
  }

  // We do not use a cursor position obtained from Mozc. It is because the
  // cursor position is used to locate the candidate window.
  var cursor = 0;
  if (opt_mozcPreedit.highlighted_position != undefined) {
    cursor = opt_mozcPreedit.highlighted_position;
  } else if (opt_mozcPreedit.cursor != undefined) {
    cursor = opt_mozcPreedit.cursor;
  }

  chrome.input.ime.setComposition({
    'contextID': this.context_.contextID,
    'text': preeditString,
    'cursor': cursor,
    'segments': preeditSegments
  });
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
                       'visible',
                       'windowPosition'];
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
      'properties': diffProperties
    });
  }
};

/**
 * Checks two objects are the same objects.
 * @param {!Object} object1 The first object to compare.
 * @param {!Object} object2 The second object to compare.
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
 * @param {mozc.Candidates=} opt_mozcCandidates The candidates data passed
 *     from NaCl Mozc module.
 * @private
 */
mozc.NaclMozc.prototype.updateCandidates_ = function(opt_mozcCandidates) {
  if (!opt_mozcCandidates) {
    this.updateCandidateWindowProperties_(
        {'visible': false, 'auxiliaryTextVisible': false});
    return;
  }

  var focusedID = null;
  var newCandidates = [];
  var candidatesIdMap = {};
  for (var i = 0; i < opt_mozcCandidates.candidate.length; ++i) {
    var item = opt_mozcCandidates.candidate[i];
    if (item.index == opt_mozcCandidates.focused_index) {
      focusedID = item.id;
    }
    var newCandidate = {
      'candidate': item.value,
      'id': item.id
    };
    if (item.annotation) {
      newCandidate.annotation = item.annotation.description;
      newCandidate.label = item.annotation.shortcut;
    }
    newCandidates.push(newCandidate);
    candidatesIdMap[item.id] = newCandidate;
  }
  if (opt_mozcCandidates.usages) {
    for (var i = 0; i < opt_mozcCandidates.usages.information.length;
          ++i) {
      var usage = opt_mozcCandidates.usages.information[i];
      for (var j = 0; j < usage.candidate_id.length; ++j) {
        if (candidatesIdMap[usage.candidate_id[j]]) {
          candidatesIdMap[usage.candidate_id[j]].usage = {
            'title': usage.title,
            'body': usage.description
          };
        }
      }
    }
  }
  if (!this.compareObjects_(this.candidates_, newCandidates)) {
    // Calls chrome.input.ime.setCandidates() if the candidates has changed.
    chrome.input.ime.setCandidates({
      'contextID': this.context_.contextID,
      'candidates': newCandidates
    });
    this.candidates_ = newCandidates;
  }

  // We have to call setCursorPosition even if there is no focused candidate.
  // This is because the last set cusor position will be used to scroll the page
  // of the candidates if we don't set.
  chrome.input.ime.setCursorPosition({
    'contextID': this.context_.contextID,
    'candidateID': (focusedID != null) ? focusedID : 0});

  var auxiliaryText = '';
  if (opt_mozcCandidates.footer) {
    if (opt_mozcCandidates.footer.label != undefined) {
      auxiliaryText = opt_mozcCandidates.footer.label;
    } else if (opt_mozcCandidates.footer.sub_label != undefined) {
      auxiliaryText = opt_mozcCandidates.footer.sub_label;
    }
    if (opt_mozcCandidates.footer.index_visible &&
        opt_mozcCandidates.focused_index != undefined) {
      if (auxiliaryText.length != 0) {
        auxiliaryText += ' ';
      }
      auxiliaryText += (opt_mozcCandidates.focused_index + 1) + '/' +
                       opt_mozcCandidates.size;
    }
  }

  var pageSize = mozc.CANDIDATE_WINDOW_PAGE_SIZE_;
  if (opt_mozcCandidates.category == 'SUGGESTION') {
    pageSize = Math.min(pageSize,
                        opt_mozcCandidates.candidate.length);
  }

  var windowPosition = (opt_mozcCandidates.category == 'SUGGESTION' ||
                        opt_mozcCandidates.category == 'PREDICTION') ?
                       'composition' : 'cursor';

  this.updateCandidateWindowProperties_({
    'visible': true,
    'cursorVisible': (focusedID != null),
    'vertical': true,
    'pageSize': pageSize,
    'auxiliaryTextVisible': (auxiliaryText.length != 0),
    'auxiliaryText': auxiliaryText,
    'windowPosition': windowPosition
  });
};

/**
 * Commits result string.
 * @param {mozc.Result=} opt_mozcResult The result data passed from NaCl
 *     Mozc module.
 * @private
 */
mozc.NaclMozc.prototype.commitResult_ = function(opt_mozcResult) {
  if (!opt_mozcResult) {
    return;
  }
  chrome.input.ime.commitText({
    'contextID': this.context_.contextID,
    'text': opt_mozcResult.value
  });
};

/**
 * Callback method called when IME is activated.
 * @param {string} engineID ID of the engine.
 * @private
 */
mozc.NaclMozc.prototype.onActivate_ = function(engineID) {
  this.engine_id_ = engineID;
  // Sets keyboardLayout_.
  var getManifest = chrome.runtime.getManifest;
  if (getManifest) {
    var input_components = getManifest()['input_components'];
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
  this.postNaclMozcEvent_(this.createMozcEvent_('SyncToFile'));
};

/**
 * Callback method called when a context acquires a focus.
 * @param {!InputContext} context The context information.
 * @private
 */
mozc.NaclMozc.prototype.onFocus_ = function(context) {
  this.context_ = context;
  var mozcCommand = this.createMozcCommand_('CREATE_SESSION');
  mozcCommand.input.capability =
      /** @type {!mozc.Capability} */
      ({text_deletion: 'DELETE_PRECEDING_TEXT'});
  mozcCommand.input.application_info =
      /** @type {!mozc.ApplicationInfo} */
      ({timezone_offset: -(new Date()).getTimezoneOffset() * 60});
  this.postMozcCommand_(
      mozcCommand,
      /**
      * @this {!mozc.NaclMozc}
      * @param {!mozc.Command} response
      */
      (function(response) {
        if (response.output.id == undefined) {
          console.error('CREATE_SESSION error');
          return;
        }
        this.sessionID_ = response.output.id;
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
  var mozcCommand = this.createMozcCommand_('DELETE_SESSION');
  mozcCommand.input.id = this.sessionID_;
  this.postMozcCommand_(mozcCommand, this.processResponse_.bind(this));
  this.sessionID_ = '';
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
 * @private
 */
mozc.NaclMozc.prototype.onKeyEventAsync_ = function(engineID, keyData) {
  if (!this.sessionID_) {
    console.error('Session has not been created.');
    chrome.input.ime.keyEventHandled(keyData.requestId, false);
    return;
  }

  var keyEvent =
      this.keyTranslator_.translateKeyEvent(keyData,
                                            this.preeditMethod_ == 'KANA',
                                            this.keyboardLayout_);
  if (this.compositionMode_ == 'DIRECT') {
    // TODO(horo): Support custom keymap table.
    if (keyEvent.special_key == 'HANKAKU' ||
        keyEvent.special_key == 'HENKAN') {
      chrome.input.ime.keyEventHandled(keyData.requestId, true);
      this.switchCompositionMode_(mozc.CompositionMode_.HIRAGANA);
      this.updateMenuItems_();
    } else {
      chrome.input.ime.keyEventHandled(keyData.requestId, false);
    }
    return;
  }

  if (keyEvent.key_code == undefined &&
      keyEvent.special_key == undefined &&
      keyEvent.modifier_keys == undefined) {
    chrome.input.ime.keyEventHandled(keyData.requestId, false);
    return;
  }

  // Cancels the callback request.
  this.callbackCommand_ = /** @type {!mozc.Callback} */ ({});

  keyEvent.mode = this.compositionMode_;

  var context = /** @type {!mozc.Context} */ ({});
  if (this.surroundingInfo_) {
    context.preceding_text =
        this.surroundingInfo_.text.substring(
            0,
            Math.min(this.surroundingInfo_.focus,
                     this.surroundingInfo_.anchor));
    context.following_text =
        this.surroundingInfo_.text.substring(
            Math.max(this.surroundingInfo_.focus,
                     this.surroundingInfo_.anchor));
  }

  var mozcCommand = this.createMozcCommand_('SEND_KEY');
  mozcCommand.input.id = this.sessionID_;
  mozcCommand.input.key = keyEvent;
  mozcCommand.input.context = context;
  this.postMozcCommand_(
      mozcCommand,
      /**
      * @this {!mozc.NaclMozc}
      * @param {string} requestId
      * @param {!mozc.Command} response
      */
      (function(requestId, response) {
        this.processResponse_(response);
        chrome.input.ime.keyEventHandled(
            requestId,
            !!response.output.consumed);
      }).bind(this, keyData.requestId));
  return;
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
  var sessionCommand = this.createMozcSessionCommand_('SELECT_CANDIDATE');
  sessionCommand.id = candidateID;
  this.postMozcSessionCommand_(sessionCommand,
                               this.processResponse_.bind(this));
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
 * Callback method called the editable string around caret is changed or when
 * the caret position is moved. The text length is limited to 100 characters for
 * each back and forth direction.
 * @param {string} engineID ID of the engine.
 * @param {!Object.<{text: string, focus: number, anchor: number}>} info The
 *     surrounding information.
 * @private
 */
mozc.NaclMozc.prototype.onSurroundingTextChanged_ = function(engineID, info) {
  this.surroundingInfo_ = info;
};

/**
 * Callback method called when Chrome terminates ongoing text input session.
 * @param {string} engineID ID of the engine.
 * @private
 */
mozc.NaclMozc.prototype.onReset_ = function(engineID) {
  if (!this.sessionID_) {
    console.error('Session has not been created.');
    return;
  }
  this.postMozcSessionCommand_(
      this.createMozcSessionCommand_('RESET_CONTEXT'),
      /**
      * @this {!mozc.NaclMozc}
      * @param {!mozc.Command} response
      */
      (function(response) {
        if (response.output.mode) {
          // We ignore the mode in the response, because we should still use
          // the current compositionMode_.
          response.output.mode = undefined;
        }
        this.processResponse_(response);
      }).bind(this));
};

/**
 * Switches the composition mode.
 * @param {mozc.CompositionMode_} newMode The new composition mode to be set.
 * @private
 */
mozc.NaclMozc.prototype.switchCompositionMode_ = function(newMode) {
  var lastMode = this.compositionMode_;
  if (lastMode != mozc.CompositionMode_.DIRECT &&
      newMode == mozc.CompositionMode_.DIRECT) {
    var mozcCommand = this.createMozcCommand_('SEND_KEY');
    mozcCommand.input.id = this.sessionID_;
    mozcCommand.input.key =
        /** @type {!mozc.KeyEvent} */ ({'special_key': 'OFF'});
    this.postMozcCommand_(mozcCommand);
  } else if (newMode != mozc.CompositionMode_.DIRECT) {
    if (lastMode == mozc.CompositionMode_.DIRECT) {
      var mozcCommand = this.createMozcCommand_('SEND_KEY');
      mozcCommand.input.id = this.sessionID_;
      mozcCommand.input.key =
          /** @type {!mozc.KeyEvent} */ ({'special_key': 'ON'});
      this.postMozcCommand_(
          mozcCommand,
          /**
          * @this {!mozc.NaclMozc}
          * @param {!mozc.CompositionMode_} newMode
          * @param {!mozc.Command} response
          */
          (function(newMode, response) {
            var sessionCommand =
                this.createMozcSessionCommand_('SWITCH_INPUT_MODE');
            sessionCommand.composition_mode = newMode;
            this.postMozcSessionCommand_(sessionCommand);
          }).bind(this, newMode));
    } else {
      var sessionCommand =
          this.createMozcSessionCommand_('SWITCH_INPUT_MODE');
      sessionCommand.composition_mode = newMode;
      this.postMozcSessionCommand_(sessionCommand);
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
      'label': chrome.i18n.getMessage(mozc.COMPOSITION_MENU_TABLE_[i].labelId),
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
};

/**
 * Handler of the messages from the NaCl module.
 * @param {Object} message message object from the NaCl module.
 * @private
 */
mozc.NaclMozc.prototype.onModuleMessage_ = function(message) {
  if (!message) {
    console.error('onModuleMessage_ error');
    return;
  }
  var mozcResponse = {};
  try {
    mozcResponse = /** @type {!mozc.Message} */ (JSON.parse(message.data));
  } catch (e) {
    console.error('handleMessage: Error: ' + e.message);
    return;
  }
  if (mozcResponse.event &&
      mozcResponse.event.type == 'InitializeDone') {
    this.isNaclInitialized_ = true;
    if (mozcResponse.event.config['preedit_method']) {
      this.setPreeditMethod(
        /** @type {mozc.PreeditMethod_} */
        (mozcResponse.event.config['preedit_method']));
    }
    while (this.initializationCallbacks_.length > 0) {
      this.initializationCallbacks_.shift()();
    }
    this.executeWatingEventHandlers_();
    return;
  }

  var callback = this.naclMessageCallbacks_[mozcResponse.id];
  delete this.naclMessageCallbacks_[mozcResponse.id];
  if (callback) {
    if (mozcResponse.cmd) {
      callback(mozcResponse.cmd);
    } else if (mozcResponse.event) {
      callback(mozcResponse.event);
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
 * @param {!Window} optionWindow Window object of the option page.
 * @return {!mozc.OptionPage} Option page object.
 */
mozc.NaclMozc.prototype.newOptionPage = function(optionWindow) {
  var optionPage = new mozc.OptionPage(this, optionWindow);
  optionPage.initialize();
  return optionPage;
};
