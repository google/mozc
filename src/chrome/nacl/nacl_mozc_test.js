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

goog.require('goog.testing.MockControl');
goog.require('goog.testing.mockmatchers.IgnoreArgument');
goog.require('goog.testing.mockmatchers.SaveArgument');

var mockControl = null;
var _ = null;
var isFunction_ = goog.testing.mockmatchers.isFunction;
var mockNaclModule = null;
var naclEventHandlers = null;
var imeEventHandlers = null;
var imeMethodMocks = null;
var naclMozc = null;

function setUp() {
  mockControl = new goog.testing.MockControl();
  _ = new goog.testing.mockmatchers.IgnoreArgument();
  mockNaclModule = mockControl.createStrictMock(NaClModuleElement);
  naclEventHandlers = mockNaClModuleEvents(mockNaclModule);
  imeEventHandlers = mockImeEvents(mockControl);
  imeMethodMocks = mockImeMethods(mockControl);
  mockControl.createMethodMock(chrome.i18n, 'getMessage')(_)
             .$does(function(messageName) {return messageName + '_i18n';})
             .$anyTimes();
  mockControl.createMethodMock(chrome.runtime, 'getManifest')()
             .$returns({
    'input_components': [{
      'name': 'Google Japanese Input for US keyboad',
      'type': 'ime',
      'id': 'nacl_mozc_us',
      'description': 'Japanese input method.',
      'language': 'ja',
      'layouts': ['us']
    },{
      'name': 'Google Japanese Input for JP keyboad',
      'type': 'ime',
      'id': 'nacl_mozc_jp',
      'description': 'Japanese input method.',
      'language': 'ja',
      'layouts': ['jp']
    }]
  }).$anyTimes();
}

function tearDown() {
  mockControl.$tearDown();
  mockControl = null;
  mockNaclModule = null;
  _ = null;
  naclEventHandlers = null;
  imeEventHandlers = null;
  imeMethodMocks = null;
  naclMozc = null;
}

function mockNaClModuleEvents(module) {
  var handlers = {};
  module.addEventListener(_, _, _)
        .$does(function(type, listener, useCapture) {
            handlers[type] = listener;
        })
        .$anyTimes();
  return handlers;
}

function mockImeEvents(control) {
  var EVENT_NAMES = ['onActivate',
                     'onDeactivated',
                     'onFocus',
                     'onBlur',
                     'onInputContextUpdate',
                     'onKeyEvent',
                     'onCandidateClicked',
                     'onMenuItemActivated',
                     'onSurroundingTextChanged',
                     'onReset'];
  var handlers = {};
  for (var i = 0; i < EVENT_NAMES.length; i++) {
    var mockAddListener =
        control.createMethodMock(chrome.input.ime[EVENT_NAMES[i]],
                                 'addListener');
    var handlerSaver = (function(eventName, handler) {
                            handlers[eventName] = handler;
                        }).bind(this, EVENT_NAMES[i]);
    mockAddListener(_).$does(handlerSaver).$anyTimes();
    // chrome.input.ime.onKeyEvent.addListener accepts 2 arguments.
    mockAddListener(_, _).$does(handlerSaver).$anyTimes();
  }
  return handlers;
}

function mockImeMethods(control) {
  var METHOD_NAMES = ['setComposition',
                      'clearComposition',
                      'commitText',
                      'setCandidateWindowProperties',
                      'setCandidates',
                      'setCursorPosition',
                      'setMenuItems',
                      'updateMenuItems',
                      'keyEventHandled'];
  var mockMethods = {};
  for (var i = 0; i < METHOD_NAMES.length; i++) {
    mockMethods[METHOD_NAMES[i]] =
        mockControl.createMethodMock(chrome.input.ime, METHOD_NAMES[i]);
  }
  return mockMethods;
}

function expectedMenuItems(mode) {
  return {
    'engineID': 'nacl_mozc_jp',
    'items': [
      {
        'id': 'MENU_COMPOSITION_HIRAGANA',
        'label': 'compositionModeHiragana_i18n',
        'checked': (mode == 'HIRAGANA'),
        'enabled': true,
        'visible': true
      },
      {
        'id': 'MENU_COMPOSITION_FULL_KATAKANA',
        'label': 'compositionModeFullKatakana_i18n',
        'checked': (mode == 'FULL_KATAKANA'),
        'enabled': true,
        'visible': true
      },
      {
        'id': 'MENU_COMPOSITION_FULL_ASCII',
        'label': 'compositionModeFullAscii_i18n',
        'checked': (mode == 'FULL_ASCII'),
        'enabled': true,
        'visible': true
      },
      {
        'id': 'MENU_COMPOSITION_HALF_KATAKANA',
        'label': 'compositionModeHalfKatakana_i18n',
        'checked': (mode == 'HALF_KATAKANA'),
        'enabled': true,
        'visible': true
      },
      {
        'id': 'MENU_COMPOSITION_HALF_ASCII',
        'label': 'compositionModeHalfAscii_i18n',
        'checked': (mode == 'HALF_ASCII'),
        'enabled': true,
        'visible': true
      },
      {
        'id': 'MENU_COMPOSITION_DIRECT',
        'label': 'compositionModeDirect_i18n',
        'checked': (mode == 'DIRECT'),
        'enabled': true,
        'visible': true
      }
    ]
  };
}

function testSimpleScenario() {
  // Result of onActivate.
  imeMethodMocks['setMenuItems'](expectedMenuItems('HIRAGANA')).$times(1);

  // Result of onFocus.
  mockNaclModule.postMessage(JSON.stringify({
    'cmd': {
      'input': {
        'type': 'CREATE_SESSION',
        'capability': {
          'text_deletion': 'DELETE_PRECEDING_TEXT'
        },
        'application_info': {
          'timezone_offset': -(new Date()).getTimezoneOffset() * 60
        }
      },
      'output': {}
    },
    'id': 0
  })).$times(1);

  // Result of the first onKeyEvent.
  mockNaclModule.postMessage(JSON.stringify({
    'cmd': {
      'input': {
        'type': 'SEND_KEY',
        'id': '12345',
        'key': {'key_code': 97, 'mode': 'HIRAGANA'},
        'context': {}
      },
      'output': {}
    },
    'id': 0
  })).$times(1);
  imeMethodMocks['setCandidates']({
    'contextID': 1,
    'candidates': [{
      'candidate': 'あ',
      'id': 0,
      'annotation': 'Realtime Top',
      'label': undefined
    }]
  }).$times(1);
  imeMethodMocks['setCursorPosition']({
    'contextID': 1,
    'candidateID': 0
  }).$times(1);
  imeMethodMocks['setComposition']({
    'contextID': 1,
    'text': 'あ',
    'cursor': 1,
    'segments': [{
      'start': 0,
      'end': 1,
      'style': 'underline'
    }]
  }).$times(1);
  imeMethodMocks['setCandidateWindowProperties']({
    'engineID': 'nacl_mozc_jp',
    'properties': {
      'visible': true,
      'cursorVisible': false,
      'vertical': true,
      'pageSize': 1,
      'auxiliaryTextVisible': true,
      'auxiliaryText': 'Google',
      'windowPosition': 'composition'
    }
  }).$times(1);
  imeMethodMocks['keyEventHandled'](100, true).$times(1);

  // Result of the second onKeyEvent.
  mockNaclModule.postMessage(JSON.stringify({
    'cmd': {
      'input': {
        'type': 'SEND_KEY',
        'id': '12345',
        'key': {'special_key': 'ENTER', 'mode': 'HIRAGANA'},
        'context': {}
      },
      'output': {}
    },
    'id': 0
  })).$times(1);
  imeMethodMocks['setComposition']({
    contextID: 1, text: '', cursor: 0
  }).$times(1);
  imeMethodMocks['commitText']({contextID: 1, text: 'あ'}).$times(1);
  imeMethodMocks['setCandidateWindowProperties']({
    'engineID': 'nacl_mozc_jp',
    'properties': {
      'visible': false,
      'cursorVisible': undefined,
      'vertical': undefined,
      'pageSize': undefined,
      'auxiliaryTextVisible': false,
      'auxiliaryText': undefined,
      'windowPosition': undefined
    }
  }).$times(1);
  imeMethodMocks['keyEventHandled'](101, true).$times(1);

  mockControl.$replayAll();

  naclMozc = new mozc.NaclMozc(mockNaclModule);
  assertFalse(naclMozc.isNaclInitialized_);
  naclEventHandlers['message']({
    'data': JSON.stringify({
      'event': {
        'type': 'InitializeDone',
        'config': {
          'preedit_method': 'ROMAN'
        }
      }
    })
  });
  assertTrue(naclMozc.isNaclInitialized_);
  imeEventHandlers['onActivate']('nacl_mozc_jp');
  imeEventHandlers['onFocus']({'contextID': 1});
  naclEventHandlers['message']({
    'data': JSON.stringify({
      'cmd': {
        'input': {
          'capability': {'text_deletion': 'DELETE_PRECEDING_TEXT'},
          'type': 'CREATE_SESSION'
        },
        'output': {
          'id': '12345'
        }
      },
      'id': 0})
  });
  assertEquals(naclMozc.sessionID_, '12345');
  imeEventHandlers['onKeyEvent']('nacl_mozc_jp', {
    'altKey': false,
    'code': 'KeyA',
    'ctrlKey': false,
    'key': 'a',
    'requestId': 100,
    'shiftKey': false,
    'type': 'keydown'
  });
  naclEventHandlers['message']({
    'data': JSON.stringify({
      'cmd': {
        'input': {
          'type': 'SEND_KEY',
          'id': '12345',
          'key': {'key_code': 97, 'mode': 'HIRAGANA'},
          'context': {}
        },
        'output': {
          'candidates': {
            'candidate': [{
              'annotation': {'description': 'Realtime Top'},
              'id': 0,
              'index': 0,
              'value': 'あ'
            }],
            'category': 'SUGGESTION',
            'display_type': 'MAIN',
            'footer': {'sub_label': 'Google'},
            'position': 0,
            'size': 1
          },
          'consumed': true,
          'id': '12345',
          'mode': 'HIRAGANA',
          'preedit': {
            'cursor': 1,
            'segment': [{
              'annotation': 'UNDERLINE',
              'key': 'あ',
              'value': 'あ',
              'value_length': 1
            }]
          },
          'status': {
            'activated': true,
            'comeback_mode': 'HIRAGANA',
            'mode': 'HIRAGANA'
          }
        }
      },
      'id': 0})
  });
  imeEventHandlers['onKeyEvent']('nacl_mozc_jp', {
    'altKey': false,
    'code': 'Enter',
    'ctrlKey': false,
    'key': 'Enter',
    'requestId': 101,
    'shiftKey': false,
    'type': 'keydown'
  });
  naclEventHandlers['message']({
    'data': JSON.stringify({
      'cmd': {
        'input': {
          'type': 'SEND_KEY',
          'id': '12345',
          'key': {'special_key': 'ENTER', 'mode': 'HIRAGANA'},
          'context': {}
        },
        'output': {
          'consumed': true,
          'id': '12345',
          'mode': 'HIRAGANA',
          'result': {
            'key': 'あ',
            'type': 'STRING',
            'value': 'あ'
          },
          'status': {
            'activated': true,
            'comeback_mode': 'HIRAGANA',
            'mode': 'HIRAGANA'
          }
        }
      },
      'id': 0})
  });
  mockControl.$verifyAll();
}

function testSurroundingText() {
  // Result of onActivate.
  imeMethodMocks['setMenuItems'](expectedMenuItems('HIRAGANA')).$times(1);

  // Result of onFocus.
  mockNaclModule.postMessage(JSON.stringify({
    'cmd': {
      'input': {
        'type': 'CREATE_SESSION',
        'capability': {
          'text_deletion': 'DELETE_PRECEDING_TEXT'
        },
        'application_info': {
          'timezone_offset': -(new Date()).getTimezoneOffset() * 60
        }
      },
      'output': {}
    },
    'id': 0
  })).$times(1);

  // Result of the first onKeyEvent.
  mockNaclModule.postMessage(JSON.stringify({
    'cmd': {
      'input': {
        'type': 'SEND_KEY',
        'id': '12345',
        'key': {'key_code': 97, 'mode': 'HIRAGANA'},
        'context': {
          'preceding_text': 'ab',
          'following_text': 'fg'
        }
      },
      'output': {}
    },
    'id': 0
  })).$times(1);
  imeMethodMocks['setComposition']({
    'contextID': 1,
    'text': '',
    'cursor': 0
  }).$times(1);
  imeMethodMocks['setCandidateWindowProperties']({
    'engineID': 'nacl_mozc_jp',
    'properties': {
      'visible': false,
      'auxiliaryTextVisible': false
    }
  }).$times(1);
  imeMethodMocks['keyEventHandled'](100, false).$times(1);

  // Result of the second onKeyEvent.
  mockNaclModule.postMessage(JSON.stringify({
    'cmd': {
      'input': {
        'type': 'SEND_KEY',
        'id': '12345',
        'key': {'key_code': 97, 'mode': 'HIRAGANA'},
        'context': {
          'preceding_text': 'A',
          'following_text': 'G'
        }
      },
      'output': {}
    },
    'id': 0
  })).$times(1);
  imeMethodMocks['setComposition']({
    'contextID': 1,
    'text': '',
    'cursor': 0
  }).$times(1);
  imeMethodMocks['keyEventHandled'](101, false).$times(1);

  mockControl.$replayAll();

  naclMozc = new mozc.NaclMozc(mockNaclModule);
  assertFalse(naclMozc.isNaclInitialized_);
  naclEventHandlers['message']({
    'data': JSON.stringify({
      'event': {
        'type': 'InitializeDone',
        'config': {
          'preedit_method': 'ROMAN'
        }
      }
    })
  });
  assertTrue(naclMozc.isNaclInitialized_);
  imeEventHandlers['onActivate']('nacl_mozc_jp');
  imeEventHandlers['onFocus']({'contextID': 1});
  naclEventHandlers['message']({
    'data': JSON.stringify({
      'cmd': {
        'input': {
          'capability': {'text_deletion': 'DELETE_PRECEDING_TEXT'},
          'type': 'CREATE_SESSION'
        },
        'output': {
          'id': '12345'
        }
      },
      'id': 0})
  });
  assertEquals(naclMozc.sessionID_, '12345');
  imeEventHandlers['onSurroundingTextChanged']('nacl_mozc_jp', {
    'text': 'abcdefg',
    'focus': 2,
    'anchor': 5
  });
  imeEventHandlers['onKeyEvent']('nacl_mozc_jp', {
    'altKey': false,
    'code': 'KeyA',
    'ctrlKey': false,
    'key': 'a',
    'requestId': 100,
    'shiftKey': false,
    'type': 'keydown'
  });
  naclEventHandlers['message']({
    'data': JSON.stringify({
      'cmd': {
        'input': {
          'type': 'SEND_KEY',
          'id': '12345',
          'key': {'key_code': 97, 'mode': 'HIRAGANA'},
          'context': {
            'preceding_text': 'ab',
            'following_text': 'fg'
          }
        },
        'output': {
          'consumed': false,
          'id': '12345'
        }
      },
      'id': 0})
  });

  imeEventHandlers['onSurroundingTextChanged']('nacl_mozc_jp', {
    'text': 'ABCDEFG',
    'focus': 6,
    'anchor': 1
  });
  imeEventHandlers['onKeyEvent']('nacl_mozc_jp', {
    'altKey': false,
    'code': 'KeyA',
    'ctrlKey': false,
    'key': 'a',
    'requestId': 101,
    'shiftKey': false,
    'type': 'keydown'
  });
  naclEventHandlers['message']({
    'data': JSON.stringify({
      'cmd': {
        'input': {
          'type': 'SEND_KEY',
          'id': '12345',
          'key': {'key_code': 97, 'mode': 'HIRAGANA'},
          'context': {
            'preceding_text': 'A',
            'following_text': 'G'
          }
        },
        'output': {
          'consumed': false,
          'id': '12345'
        }
      },
      'id': 0})
  });
  mockControl.$verifyAll();
}

function testOnReset() {
  // Result of onActivate.
  imeMethodMocks['setMenuItems'](expectedMenuItems('HIRAGANA')).$times(1);

  // Result of onFocus.
  mockNaclModule.postMessage(JSON.stringify({
    'cmd': {
      'input': {
        'type': 'CREATE_SESSION',
        'capability': {'text_deletion': 'DELETE_PRECEDING_TEXT'},
        'application_info': {
          'timezone_offset': -(new Date()).getTimezoneOffset() * 60
        }
      },
      'output': {}
    },
    'id': 0
  })).$times(1);

  // Result of the onReset.
  mockNaclModule.postMessage(JSON.stringify({
    'cmd': {
      'input': {
        'type': 'SEND_COMMAND',
        'id': '12345',
        'command': {'type': 'RESET_CONTEXT'}
      },
      'output': {}
    },
    'id': 0
  })).$times(1);
  imeMethodMocks['setComposition']({
    'contextID': 1,
    'text': '',
    'cursor': 0
  }).$times(1);
  imeMethodMocks['setCandidateWindowProperties']({
    'engineID': 'nacl_mozc_jp',
    'properties': {
      'visible': false,
      'auxiliaryTextVisible': false
    }
  }).$times(1);
  mockControl.$replayAll();

  naclMozc = new mozc.NaclMozc(mockNaclModule);
  assertFalse(naclMozc.isNaclInitialized_);
  naclEventHandlers['message']({
    'data': JSON.stringify({
      'event': {
        'type': 'InitializeDone',
        'config': {
          'preedit_method': 'ROMAN'
        }
      }
    })
  });
  assertTrue(naclMozc.isNaclInitialized_);
  imeEventHandlers['onActivate']('nacl_mozc_jp');
  imeEventHandlers['onFocus']({'contextID': 1});
  naclEventHandlers['message']({
    'data': JSON.stringify({
      'cmd': {
        'input': {
          'capability': {'text_deletion': 'DELETE_PRECEDING_TEXT'},
          'type': 'CREATE_SESSION'
        },
        'output': {
          'id': '12345'
        }
      },
      'id': 0})
  });
  assertEquals(naclMozc.sessionID_, '12345');
  imeEventHandlers['onReset']('nacl_mozc_jp');
  naclEventHandlers['message']({
    'data': JSON.stringify({
      'cmd': {
        'input': {
          'type': 'SEND_COMMAND',
          'id': '12345',
          'command': {'type': 'RESET_CONTEXT'}
        },
        'output': {
          'mode': 'FULL_KATAKANA'
        }
      },
      'id': 0})
  });
  assertEquals(naclMozc.compositionMode_, 'HIRAGANA');
  mockControl.$verifyAll();
}
