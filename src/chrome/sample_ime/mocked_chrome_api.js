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
 * Defines a object if it is not defined.
 * @param {var_names} objectName object name you want to define. You pass
 *     'A.B.C' as an argument and window.A.B.C is defined.
 */
function defineObject(objectName) {
  var obj = window;
  var names = objectName.split('.');

  for (var i = 0; i < names.length; ++i) {
    var name = names[i];
    if (!obj[name]) {
      obj[name] = function() {};
    }
    obj = obj[name];
  }
}

// console.log is used for logging, but it doesn't implemented on test
// environment. So we inject a dummy function.
defineObject('console.log');

// Overrides chrome.input.ime for testing.
defineObject('chrome.input');

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
  clearComposition: function() {
    fail('clearComposition is not overrided.');
  },
  setComposition: function() {
    fail('setComposition is not overrided.');
  },
  setCandidateWindowProperties: function() {
    fail('setCandidateWindowProperties is not overrided.');
  },
  setCandidates: function() {
    fail('setCandidates is not overrided.');
  },
  setCursorPosition: function() {
    fail('setCursorPosition is not overrided.');
  },
  commitText: function() {
    fail('commitText is not overrided.');
  },
  setMenuItems: function() {
    fail('setMenuItems is not overrided.');
  },
  updateMenuItems: function() {
    fail('updateMenuItems is not overrided.');
  }
};
