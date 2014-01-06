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
 * @fileoverview This file contains Chrome specific API mocks.
 *
 */

'use strict';

// Defines 'console' which are not defined in the test environment.
var console = {};
console.log = function() {};
console.error = function() {};

// Defines 'chrome' which are not defined in the test environment.
var chrome = {};
chrome.input = {};
chrome.runtime = {};
chrome.i18n = {};
chrome.identity = {};

/**
 * Mocked HTMLEmbedElement for NaCl Module.
 */
var NaClModuleElement = function() {
};

NaClModuleElement.prototype.addEventListener = function() {
  fail('NaClModuleElement.prototype.addEventListener is not overridden.');
};

NaClModuleElement.prototype.postMessage = function() {
  fail('NaClModuleElement.prototype.postMessage is not overridden.');
};

/**
 * Mock IME extension API.
 */
chrome.input.ime = {
  onActivate: {
    addListener: function() {
      fail('onActivate.addListener is not overridden.');
    }
  },
  onDeactivated: {
    addListener: function() {
      fail('onDeactivated.addListener is not overridden.');
    }
  },
  onFocus: {
    addListener: function() {
      fail('onFocus.addListener is not overridden.');
    }
  },
  onBlur: {
    addListener: function() {
      fail('onBlur.addListener is not overridden.');
    }
  },
  onInputContextUpdate: {
    addListener: function() {
      fail('onInputContextUpdate.addListener is not overridden.');
    }
  },
  onKeyEvent: {
    addListener: function() {
      fail('onKeyEvent.addListener is not overridden.');
    }
  },
  onCandidateClicked: {
    addListener: function() {
      fail('onCandidateClicked.addListener is not overridden.');
    }
  },
  onMenuItemActivated: {
    addListener: function() {
      fail('onMenuItemActivated.addListener is not overridden.');
    }
  },
  onSurroundingTextChanged: {
    addListener: function() {
      fail('onSurroundingTextChanged.addListener is not overridden.');
    }
  },
  onReset: {
    addListener: function() {
      fail('onReset.addListener is not overridden.');
    }
  },
  clearComposition: function() {
    fail('clearComposition is not overridden.');
  },
  setComposition: function() {
    fail('setComposition is not overridden.');
  },
  setCandidateWindowProperties: function() {
    fail('setCandidateWindowProperties is not overridden.');
  },
  setCandidates: function() {
    fail('setCandidates is not overridden.');
  },
  setCursorPosition: function() {
    fail('setCursorPosition is not overridden.');
  },
  commitText: function() {
    fail('commitText is not overridden.');
  },
  setMenuItems: function() {
    fail('setMenuItems is not overridden.');
  },
  updateMenuItems: function() {
    fail('updateMenuItems is not overridden.');
  },
  keyEventHandled: function() {
    fail('keyEventHandled is not overridden.');
  }
};

/**
 * Mock IME extension API.
 */
chrome.runtime.getManifest = function() {
  fail('chrome.runtime.getManifest is not overridden.');
};
chrome.i18n.getMessage = function() {
  fail('chrome.i18n.getMessage is not overridden.');
};

