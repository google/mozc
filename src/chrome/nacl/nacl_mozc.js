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
 */

'use strict';

/**
 * Namespace for this extension.
 */
var mozc = window.mozc || {};

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
    labelId: 'compositionModeHiragana'
  },
  {
    menu: mozc.MenuItemId.MENU_COMPOSITION_FULL_KATAKANA,
    mode: mozc.CompositionMode.FULL_KATAKANA,
    labelId: 'compositionModeFullKatakana'
  },
  {
    menu: mozc.MenuItemId.MENU_COMPOSITION_FULL_ASCII,
    mode: mozc.CompositionMode.FULL_ASCII,
    labelId: 'compositionModeFullAscii'
  },
  {
    menu: mozc.MenuItemId.MENU_COMPOSITION_HALF_KATAKANA,
    mode: mozc.CompositionMode.HALF_KATAKANA,
    labelId: 'compositionModeHalfKatakana'
  },
  {
    menu: mozc.MenuItemId.MENU_COMPOSITION_HALF_ASCII,
    mode: mozc.CompositionMode.HALF_ASCII,
    labelId: 'compositionModeHalfAscii'
  },
  {
    menu: mozc.MenuItemId.MENU_COMPOSITION_DIRECT,
    mode: mozc.CompositionMode.DIRECT,
    labelId: 'compositionModeDirect'
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
   * Key event translator.
   * @type {Object}
   * @private
   */
  this.keyTranslator_ = new mozc.KeyTranslator();

  /**
   * callbackCommand_ stores the callback message which is recieved from NaCl
   * module. This callback will be cancelled when the user presses the
   * subsequent key. In the current implementation, if the subsequent key event
   * also makes callback, the second callback will be called in the timimg of
   * the first callback.
   * @type {!Object.<string, *>}
   * @private
   */
  this.callbackCommand_ = {};

  /**
   * The surrounding information.
   * @type {Object}
   * @private
   */
  this.surroundingInfo_ = null;

  /**
   * The build number of Chrome.
   * See: http://www.chromium.org/releases/version-numbers
   * @type {number}
   * @private
   */
  this.chromeBuildNumber_ = 0;
  var versionStrings =
      window.navigator.appVersion.match(/Chrome\/(\d+)\.(\d+)\.(\d+)\.(\d+)/);
  if (versionStrings) {
    this.chromeBuildNumber_ = parseInt(versionStrings[3], 10);
  }

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
  this.postMozcCommand_({'input': {'type': 'RELOAD'}}, opt_callback);
};

/**
 * Sends GET_CONFIG command to NaCl module.
 * @param {!function(Object)=} opt_callback Function to be called with results
 *     from NaCl module.
 */
mozc.NaclMozc.prototype.getConfig = function(opt_callback) {
  this.postMozcCommand_({'input': {'type': 'GET_CONFIG'}}, opt_callback);
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
  this.postMozcCommand_({'input': {'type': 'SET_CONFIG', 'config': config}},
                        opt_callback);
};

/**
 * Sends CLEAR_USER_HISTORY command to NaCl module.
 * @param {!function(Object)=} opt_callback Function to be called with results
 *     from NaCl module.
 */
mozc.NaclMozc.prototype.clearUserHistory = function(opt_callback) {
  this.postMozcCommand_({'input': {'type': 'CLEAR_USER_HISTORY'}},
                        opt_callback);
};

/**
 * Sends CLEAR_USER_PREDICTION command to NaCl module.
 * @param {!function(Object)=} opt_callback Function to be called with results
 *     from NaCl module.
 */
mozc.NaclMozc.prototype.clearUserPrediction = function(opt_callback) {
  this.postMozcCommand_({'input': {'type': 'CLEAR_USER_PREDICTION'}},
                        opt_callback);
};

/**
 * Sends START_CLOUD_SYNC command to NaCl module.
 * @param {!function(Object)=} opt_callback Function to be called with results
 *     from NaCl module.
 */
mozc.NaclMozc.prototype.startCloudSync = function(opt_callback) {
  this.postMozcCommand_({'input': {'type': 'START_CLOUD_SYNC'}},
                        opt_callback);
};

/**
 * Sends GET_CLOUD_SYNC_STATUS command to NaCl module.
 * @param {!function(Object)=} opt_callback Function to be called with results
 *     from NaCl module.
 */
mozc.NaclMozc.prototype.getCloudSyncStatus = function(opt_callback) {
  this.postMozcCommand_({'input': {'type': 'GET_CLOUD_SYNC_STATUS'}},
                        opt_callback);
};

/**
 * Sends ADD_AUTH_CODE command to NaCl module.
 * @param {!Object} authInfo Authorization information for Sync.
 * @param {!function(Object)=} opt_callback Function to be called with results
 *     from NaCl module.
 */
mozc.NaclMozc.prototype.addAuthCode = function(authInfo, opt_callback) {
  this.postMozcCommand_(
      {'input': {'type': 'ADD_AUTH_CODE', 'auth_code': authInfo}},
      opt_callback);
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
 * Gets the version information of NaCl Mozc module.
 * @param {!function(Object)} callback Function to be called with results
 *     from NaCl module.
 */
mozc.NaclMozc.prototype.getVersionInfo = function(callback) {
  this.postNaclMozcEvent_({'type': 'GetVersionInfo'}, callback);
};

/**
 * Sends callback command to NaCl module.
 * @param {!function(Object)=} opt_callback Function to be called with results
 *     from NaCl module.
 * @private
 */
mozc.NaclMozc.prototype.sendCallbackCommand_ = function(opt_callback) {
  if (!this.sessionID_) {
    console.error('Session has not been created.');
    return;
  }
  if (!this.callbackCommand_['session_command']) {
    return;
  }
  var command = this.callbackCommand_['session_command'];
  if (command['type'] == 'CONVERT_REVERSE' && this.surroundingInfo_) {
    if (this.surroundingInfo_['focus'] < this.surroundingInfo_['anchor']) {
      command['text'] = this.surroundingInfo_['text'].substring(
                            this.surroundingInfo_['focus'],
                            this.surroundingInfo_['anchor']);
    } else {
      command['text'] = this.surroundingInfo_['text'].substring(
                            this.surroundingInfo_['anchor'],
                            this.surroundingInfo_['focus']);
    }
  }
  this.callbackCommand_ = {};
  this.postMozcCommand_(
      {
        'input': {
          'type': 'SEND_COMMAND',
          'id': this.sessionID_,
          'command': command
        }
      },
      opt_callback);
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
  var outputFinction = (function(mozcCommand) {
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
    if (mozcCommand['output']['callback']) {
      this.callbackCommand_ = mozcCommand['output']['callback'];
      if (this.callbackCommand_['delay_millisec']) {
        setTimeout(
            this.sendCallbackCommand_.bind(
                this,
                this.outputResponse_.bind(this)),
            this.callbackCommand_['delay_millisec']);
      } else {
        this.sendCallbackCommand_(this.outputResponse_.bind(this));
      }
    }
  }).bind(this, mozcCommand);
  if (mozcCommand['output']['deletion_range']) {
    // chrome.input.ime.deleteSurroundingText is available from ChromeOS 27.
    if (chrome.input.ime.deleteSurroundingText) {
      chrome.input.ime.deleteSurroundingText({
          'engineID': this.engine_id_,
          'contextID': this.context_.contextID,
          'offset': mozcCommand['output']['deletion_range']['offset'],
          'length': mozcCommand['output']['deletion_range']['length']},
          outputFinction);
      return;
    }
  }
  outputFinction();
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
      'end': preeditString.length + mozcPreedit['segment'][i]['value_length']
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

  if (this.chromeBuildNumber_ >= 1492) {
    // Chrome before this patch, setCandidateWindowProperties does not support
    // windowPosition. https://chromiumcodereview.appspot.com/14054009/
    // TODO(horo): Remove this hack and add 'windowPosition' in the definition
    // line of propertyNames when Chrome 28 become stable.
    propertyNames.push('windowPosition');
  }

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

  var focusedID = null;
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

  if (this.chromeBuildNumber_ < 1490 && focusedID == null) {
    // https://src.chromium.org/viewvc/chrome?revision=196321&view=revision
    // Chrome before this patch, cursorVisible = false doesn't work correctly.
    // So we set candidateID = 0 and cursorVisible = true.
    // TODO(horo): Remove this hack when Chrome 28 become stable.
    focusedID = 0;
  }

  // If focusedID is null we don't need to call setCursorPosition. But Chrome
  // does not update candidates if we don't call setCursorPosition.
  // TODO(horo): Don't call setCursorPosition if focusedID is null after the bug
  // will be fixed. http://crbug.com/234868
  chrome.input.ime.setCursorPosition({
    'contextID': this.context_.contextID,
    'candidateID': (focusedID != null) ? focusedID : 0});

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

  var windowPosition = (mozcCandidates['category'] == 'SUGGESTION' ||
                        mozcCandidates['category'] == 'PREDICTION') ?
                       'composition' : 'cursor';

  this.updateCandidateWindowProperties_({
    'visible': true,
    'cursorVisible': (focusedID != null),
    'vertical': true,
    'pageSize': pageSize,
    'auxiliaryTextVisible': (auxiliaryText.length != 0),
    'auxiliaryText': auxiliaryText,
    'windowPosition': windowPosition});
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
      {
        'input': {
          'type': 'CREATE_SESSION',
          'capability': {
            'text_deletion': 'DELETE_PRECEDING_TEXT'
          }
        }
      },
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

  var keyEvent =
      this.keyTranslator_.translateKeyEvent(keyData,
                                            this.preeditMethod_ == 'KANA',
                                            this.keyboardLayout_);

  if (this.compositionMode_ == 'DIRECT') {
    // TODO(horo): Support custom keymap table.
    if (keyEvent['special_key'] == 'HANKAKU' ||
        keyEvent['special_key'] == 'HENKAN') {
      chrome.input.ime.keyEventHandled(keyData.requestId, true);
      this.switchCompositionMode_(mozc.CompositionMode.HIRAGANA);
      this.updateMenuItems_();
    } else {
      chrome.input.ime.keyEventHandled(keyData.requestId, false);
    }
    return;
  }

  if (keyEvent['key_code'] == undefined &&
      keyEvent['special_key'] == undefined &&
      keyEvent['modifier_keys'] == undefined) {
    chrome.input.ime.keyEventHandled(keyData.requestId, false);
    return;
  }

  // Cancels the callback request.
  this.callbackCommand_ = {};

  keyEvent['mode'] = this.compositionMode_;

  var context = {};
  if (this.surroundingInfo_) {
    context['preceding_text'] =
        this.surroundingInfo_['text'].substring(
            0,
            Math.min(this.surroundingInfo_['focus'],
                     this.surroundingInfo_['anchor']));
    context['following_text'] =
        this.surroundingInfo_['text'].substring(
            Math.max(this.surroundingInfo_['focus'],
                     this.surroundingInfo_['anchor']));
  }

  this.postMozcCommand_(
      {
        'input': {
          'type': 'SEND_KEY',
          'id': this.sessionID_,
          'key': keyEvent,
          'context': context
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
 * Callback method called the editable string around caret is changed or when
 * the caret position is moved. The text length is limited to 100 characters for
 * each back and forth direction.
 * @param {string} engineID ID of the engine.
 * @param {!Object} info The surrounding information.
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
  this.postMozcCommand_(
      {
        'input': {
          'type': 'SEND_COMMAND',
          'id': this.sessionID_,
          'command': {
            'type': 'RESET_CONTEXT'
          }
        }
      },
      this.outputResponse_.bind(this));
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
 * Calls chrome.identity.getAuthToken and returns the result to NaCl module.
 * This method is called from NaCl via NaclJsProxy.
 * @param {Object} args arguments which are used to call getAuthToken.
 * @private
 */
mozc.NaclMozc.prototype.jsCallGetAuthToken_ = function(args) {
  if (!chrome.identity ||
      !chrome.identity.getAuthToken) {
    // chrome.identity.getAuthToken is available from ChromeOS 29.
    // See: https://code.google.com/p/chromium/issues/detail?id=233250
    this.naclModule_['postMessage'](JSON.stringify({
      'jscall': 'GetAuthToken'
    }));
    return;
  }
  chrome.identity.getAuthToken(
      {interactive: !!args['interactive']},
      (function(token) {
        this.naclModule_['postMessage'](JSON.stringify({
          'jscall': 'GetAuthToken',
          'access_token': token
        }));
      }).bind(this));
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
  if (mozcResponse['jscall']) {
    if (mozcResponse['jscall'] == 'GetAuthToken') {
      this.jsCallGetAuthToken_(mozcResponse['args']);
      return;
    }
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
 * @param {!Object} consoleObject Console object of the option page.
 * @return {!mozc.OptionPage} Option page object.
 */
mozc.NaclMozc.prototype.newOptionPage = function(domDocument, consoleObject) {
  var optionPage = new mozc.OptionPage(this, domDocument, consoleObject);
  optionPage.initialize();
  return optionPage;
};
