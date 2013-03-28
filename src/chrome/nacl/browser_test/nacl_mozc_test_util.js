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
 * @fileoverview Test utilities for NaCl Mozc.
 *
 */

'use strict';

/**
 * Namespace
 */
var mozc_test_util = window.mozc_test_util || {};

/**
 * Refresh IME extension API mock.
 */
mozc_test_util.refreshMockApi = function() {
  var chrome = window.chrome || {};
  chrome.i18n = chrome.i18n || {};
  chrome.i18n.getMessage =
      chrome.i18n.getMessage ||
      function(messageName, opt_args) {
        // Dummy implementation.
        return messageName;
      };
  chrome.input = chrome.input || {};
  var dummyListener = function() {};
  dummyListener.addListener = function(callback) {};
  var dummyFunction = function() {};

  chrome.input.ime = {
    'onActivate': dummyListener,
    'onBlur': dummyListener,
    'onCandidateClicked': dummyListener,
    'onDeactivated': dummyListener,
    'onFocus': dummyListener,
    'onInputContextUpdate': dummyListener,
    'onKeyEvent': dummyListener,
    'onMenuItemActivated': dummyListener,
    'clarComposition': dummyFunction,
    'commitText': dummyFunction,
    'keyEventHandled': dummyFunction,
    'setCandidateWindowProperties': dummyFunction,
    'setCandidates': dummyFunction,
    'setComposition': dummyFunction,
    'setCursorPosition': dummyFunction,
    'setMenuItems': dummyFunction,
    'updateMenuItems': dummyFunction
  };
};

/**
 * Mozc test executer.
 * @constructor
 * @param {!Object} status Tester object.
 * @param {function(function(function(), integer, function()=), !mozc.NaclMozc)}
 *     testRegisterer Function to register test cases. It takes a registerer
 *     function and a mozc object.
 */
mozc_test_util.Executer = function(status, testRegisterer) {
  mozc_test_util.refreshMockApi();

  /**
   * Tester object.
   * @type {!Object}
   * @private
   */
  this.status_ = status;

  /**
   * Test cases.
   * @type {!Array.<!Object>}
   * @private
   */
  this.testCases_ = [];

  var mozcId = 'test_nacl_mozc_' + ++mozc_test_util.Executer.nextIdNumber_;
  var naclModule = createNaClEmbed({
    id: mozcId,
    src: 'nacl_session_handler.nmf'
  });

  var executer = this;
  naclModule.addEventListener('load', function() {
    if (naclModule.readyState == 4) {
      testRegisterer(executer.register_.bind(executer),
                     new window.mozc.NaclMozc(naclModule));
      executer.start_(naclModule);
    }
  }, true);

  // Loads NaCl module.
  var body = document.getElementsByTagName('body').item(0);
  body.appendChild(naclModule);
};

/**
 * Next ID number to create a unique ID for NaCl module.
 * @type {number}
 * @private
 */
mozc_test_util.Executer.nextIdNumber_ = 0;

/**
 * Registers a test case.
 * @param {function()} runner Test body.
 * @param {integer} callbackNum Expected callback number from NaCl.
 * @param {function()=} opt_afterCallback Function which is called after
 *     callback.
 * @private
 */
mozc_test_util.Executer.prototype.register_ = function(
    runner, callbackNum, opt_afterCallback) {
  if (!callbackNum && opt_afterCallback != undefined) {
    this.status_.fail('Invalid arguments. ' +
                      'opt_afterCallback should be undefined.');
    return;
  }

  this.testCases_.push({
    'runner': runner,
    'callbackNum': callbackNum,
    'afterCallback': opt_afterCallback || null
  });
};

/**
 * Starts test cases after Nacl Mozc is loaded.
 * @param {!HTMLElement} naclModule NaCl module object.
 * @private
 */
mozc_test_util.Executer.prototype.start_ = function(naclModule) {
  var executer = this;

  var messageListener = function(message) {
    try {
      var mozcResponse = JSON.parse(message.data);
      if (mozcResponse['event']['type'] != 'InitializeDone') {
        return;
      }
    } catch (err) {
      this.status_.fail(err.message);
      return;
    }
    naclModule.removeEventListener(messageListener);

    // Triggers tests.
    var messageHandler = executer.onModuleMessage_.bind(executer);
    naclModule.addEventListener('message', messageHandler, true);
    executer.triggerNextTest_();
  };

  naclModule.addEventListener('message', messageListener, true);
};

/**
 * Executes a test case. If the test case doesn't require callback from NaCl,
 * this method triggeres next test case.
 * @private
 */
mozc_test_util.Executer.prototype.triggerNextTest_ = function() {
  if (this.testCases_.length == 0) {
    return;
  }

  var testCase = this.testCases_[0];
  if (testCase.runner) {
    testCase.runner();
    delete testCase.runner;
  }

  if (testCase.callbackNum <= 0) {
    this.testCases_.shift();
    this.triggerNextTest_();
  }
};

/**
 * Handles callback from NaCl and triggeres next test case.
 * @private
 */
mozc_test_util.Executer.prototype.onModuleMessage_ = function() {
  if (this.testCases_.length == 0) {
    return;
  }

  var testCase = this.testCases_[0];
  if (testCase.afterCallback) {
    testCase.afterCallback();
    delete testCase.afterCallback;
  }

  --testCase.callbackNum;
  if (testCase.callbackNum <= 0) {
     this.testCases_.shift();
    this.triggerNextTest_();
  }
};

mozc_test_util.refreshMockApi();
