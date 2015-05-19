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

// TODO(hsumita): Introduces Closure library into mozc code tree if we release
// this file as an open source.

goog.require('goog.array');
goog.require('goog.testing.MockControl');
goog.require('goog.testing.mockmatchers.ArgumentMatcher');
goog.require('goog.testing.mockmatchers.IgnoreArgument');

/**
 * Finds an item in an array, returns the index, or -1 if not found.
 * @param {Array.<Object>} array Target array.
 * @param {Object} target Taret you want to get a index.
 * @return {number} -1 if array doesn't contain the item, or index of the item.
 */
function findInArray(array, target) {
  return goog.array.findIndex(array, function(elem) {
    return elem === target;
  });
}

/**
 * Overwrite the function by empty one.
 * @param {!Object} obj Target object.
 * @param {string} fullName The name of the function.
 * @return {function} An original function.
 */
function overwriteByEmptyFunction(obj, fullName) {
  var names = fullName.split('.');
  var original = obj[names[0]];

  for (var i = 0; i < names.length; ++i) {
    obj = obj[names[i]] = function() {};
  }

  return original;
}

var ENGINE_ID = '12345';
var CONTEXT = {
  contextID: 67890,
  type: 'text'
};

var mockControl = null;
var callbackMatcher = null;
var _ = null;

function setUp() {
  mockControl = new goog.testing.MockControl();
  callbackMatcher =
      new goog.testing.mockmatchers.ArgumentMatcher(function(obj) {
    return typeof(obj) == 'function';
  });
  _ = new goog.testing.mockmatchers.IgnoreArgument();
}

function tearDown() {
  mockControl.$tearDown();
  mockControl = null;
}

function getIme() {
  // Sets a dummy method, activates IME, and restores original one.
  var api = chrome.input.ime;

  if (!api.onActivate) {
    fail('api.onActivate is not defined!');
  }
  if (!chrome.input.ime.onActivate) {
    fail('chrome.input.ime.onActivate is not defined!');
  }
  if (!chrome.input.ime.onActivate.addListener) {
    fail('chrome.input.ime.onActivate.addListener is not defined!');
  }

  var originalOnActivateListener =
      overwriteByEmptyFunction(api, 'onActivate.addListener');
  var originalOnDeactivatedListener =
      overwriteByEmptyFunction(api, 'onDeactivated.addListener');
  var originalOnFocusListener =
      overwriteByEmptyFunction(api, 'onFocus.addListener');
  var originalOnBlurListener =
      overwriteByEmptyFunction(api, 'onBlur.addListener');
  var originalOnInputContextUpdateListener =
      overwriteByEmptyFunction(api, 'onInputContextUpdate.addListener');
  var originalOnKeyEventListener =
      overwriteByEmptyFunction(api, 'onKeyEvent.addListener');
  var originalOnCandidateClickedListener =
      overwriteByEmptyFunction(api, 'onCandidateClicked.addListener');
  var originalOnMenuItemActivatedListener =
      overwriteByEmptyFunction(api, 'onMenuItemActivated.addListener');
  var originalSetMenuItems = overwriteByEmptyFunction(api, 'setMenuItems');

  var ime = new sampleImeForImeExtensionApi.SampleIme;
  ime.onActivate(ENGINE_ID);

  api.onActivate = originalOnActivateListener;
  api.onDeactivated = originalOnDeactivatedListener;
  api.onBlur = originalOnBlurListener;
  api.onInputContextUpdate = originalOnInputContextUpdateListener;
  api.onKeyEvent = originalOnKeyEventListener;
  api.onCandidateClicked = originalOnCandidateClickedListener;
  api.onMenuItemActivated = originalOnMenuItemActivatedListener;
  api.setMenuItems = originalSetMenuItems;

  ime.onFocus(CONTEXT);

  return ime;
}

function testInsertCharacters() {
  var ime = getIme();

  assertEquals('', ime.inputText_);
  assertEquals(0, ime.segments_.length);
  assertEquals(0, ime.cursor_);
  assertEquals(sampleImeForImeExtensionApi.SampleIme.State.PRECOMPOSITION,
               ime.state_);

  ime.insertCharacters_('a');
  assertEquals('a', ime.inputText_);
  assertEquals(1, ime.segments_.length);
  assertEquals(1, ime.cursor_);
  assertEquals(0, ime.focusedSegmentIndex_);
  assertEquals(sampleImeForImeExtensionApi.SampleIme.State.COMPOSITION,
               ime.state_);

  ime.insertCharacters_('bc');
  assertEquals('abc', ime.inputText_);
  assertEquals(1, ime.segments_.length);
  assertEquals(3, ime.cursor_);
  assertEquals(0, ime.focusedSegmentIndex_);
  assertEquals(sampleImeForImeExtensionApi.SampleIme.State.COMPOSITION,
               ime.state_);

  ime.moveCursor_(1);
  ime.insertCharacters_('A');
  assertEquals('aAbc', ime.inputText_);
  assertEquals(1, ime.segments_.length);
  assertEquals(2, ime.cursor_);
  assertEquals(0, ime.focusedSegmentIndex_);
  assertEquals(sampleImeForImeExtensionApi.SampleIme.State.COMPOSITION,
               ime.state_);
}

function testRemoveCharacter() {
  var ime = getIme();

  ime.insertCharacters_('abcde');
  assertEquals('abcde', ime.inputText_);
  assertEquals(5, ime.cursor_);
  assertEquals(sampleImeForImeExtensionApi.SampleIme.State.COMPOSITION,
               ime.state_);

  ime.moveCursor_(2);

  ime.removeCharacter_(0);
  assertEquals('bcde', ime.inputText_);
  assertEquals(1, ime.cursor_);
  assertEquals(sampleImeForImeExtensionApi.SampleIme.State.COMPOSITION,
               ime.state_);

  ime.removeCharacter_(3);
  assertEquals('bcd', ime.inputText_);
  assertEquals(1, ime.cursor_);
  assertEquals(sampleImeForImeExtensionApi.SampleIme.State.COMPOSITION,
               ime.state_);

  ime.removeCharacter_(-1);
  assertEquals('bcd', ime.inputText_);
  assertEquals(1, ime.cursor_);
  assertEquals(sampleImeForImeExtensionApi.SampleIme.State.COMPOSITION,
               ime.state_);

  ime.removeCharacter_(4);
  assertEquals('bcd', ime.inputText_);
  assertEquals(1, ime.cursor_);
  assertEquals(sampleImeForImeExtensionApi.SampleIme.State.COMPOSITION,
               ime.state_);

  ime.removeCharacter_(1);
  assertEquals('bd', ime.inputText_);
  assertEquals(1, ime.cursor_);
  assertEquals(sampleImeForImeExtensionApi.SampleIme.State.COMPOSITION,
               ime.state_);

  ime.removeCharacter_(0);
  ime.removeCharacter_(0);
  assertEquals('', ime.inputText_);
  assertEquals(0, ime.cursor_);
  assertEquals(sampleImeForImeExtensionApi.SampleIme.State.PRECOMPOSITION,
               ime.state_);
}

function testMoveCursor() {
  var ime = getIme();

  // Precomposition mode.
  ime.moveCursor_(1);
  assertEquals(0, ime.cursor_);

  // Composition mode.
  ime.insertCharacters_('abcde');
  ime.moveCursor_(2);
  assertEquals(2, ime.cursor_);
  ime.moveCursor_(-1);
  assertEquals(2, ime.cursor_);
  ime.moveCursor_(100);
  assertEquals(2, ime.cursor_);

  // Conversion mode.
  ime.focusCandidate_(1);
  ime.moveCursor_(0);
  assertEquals(2, ime.cursor_);
}

function testExpandShrinkSegment() {
  var ime = getIme();

  ime.isUpdated_ = [];
  ime.generateCandidates_ = function(opt_segmentIndex) {
    var segmentIndex = (opt_segmentIndex == null) ?
        this.focusedSegmentIndex_ : opt_segmentIndex;
    this.isUpdated_[segmentIndex] = true;
  };
  ime.clearUpdatedSegmentList_ = function() {
    this.isUpdated_ = [];
  };
  ime.compareUpdatedSegmentIndexes_ = function(indexes) {
    for (var i = 0; i < indexes.length; ++i) {
      if (!this.isUpdated_[indexes[i]]) {
        return false;
      }
    }
    return true;
  };

  ime.compareSegmentBoundaries_ = function(indexes) {
    if (this.segments_.length !== indexes.length) {
      return false;
    }
    for (var i = 0; i < this.segments_.length; ++i) {
      if (this.segments_[i].start !== indexes[i]) {
        return false;
      }
    }
    return true;
  };

  ime.insertCharacters_('abcde');
  assertEquals(sampleImeForImeExtensionApi.SampleIme.State.COMPOSITION,
               ime.state_);

  // abcde -> abcde
  ime.clearUpdatedSegmentList_();
  ime.expandSegment_();
  assertTrue(ime.compareUpdatedSegmentIndexes_([]));
  assertTrue(ime.compareSegmentBoundaries_([0]));
  assertEquals(sampleImeForImeExtensionApi.SampleIme.State.CONVERSION,
               ime.state_);

  // abcde -> abcd|e
  ime.clearUpdatedSegmentList_();
  ime.shrinkSegment_();
  assertTrue(ime.compareUpdatedSegmentIndexes_([0, 1]));
  assertTrue(ime.compareSegmentBoundaries_([0, 4]));

  // abcd|e -> abcde|
  ime.clearUpdatedSegmentList_();
  ime.expandSegment_();
  assertTrue(ime.compareUpdatedSegmentIndexes_([0]));
  assertTrue(ime.compareSegmentBoundaries_([0]));

  // abcde| -> ab|cde
  ime.shrinkSegment_();
  ime.shrinkSegment_();
  ime.shrinkSegment_();

  // ab|cde -> a|bcde
  ime.clearUpdatedSegmentList_();
  ime.shrinkSegment_();
  assertTrue(ime.compareUpdatedSegmentIndexes_([0, 1]));
  assertTrue(ime.compareSegmentBoundaries_([0, 1]));

  // a|bcde -> a|bcde
  ime.clearUpdatedSegmentList_();
  ime.shrinkSegment_();
  assertTrue(ime.compareUpdatedSegmentIndexes_([]));
  assertTrue(ime.compareSegmentBoundaries_([0, 1]));

  // a|bcde -> ab|cde
  ime.clearUpdatedSegmentList_();
  ime.expandSegment_();
  assertTrue(ime.compareUpdatedSegmentIndexes_([0, 1]));
  assertTrue(ime.compareSegmentBoundaries_([0, 2]));

  ime.focusedSegmentIndex_ = 1;

  // ab|cde -> ab|cd|e
  ime.clearUpdatedSegmentList_();
  ime.shrinkSegment_();
  assertTrue(ime.compareUpdatedSegmentIndexes_([1, 2]));
  assertTrue(ime.compareSegmentBoundaries_([0, 2, 4]));

  // ab|cd|e -> ab|cde
  ime.clearUpdatedSegmentList_();
  ime.expandSegment_();
  assertTrue(ime.compareUpdatedSegmentIndexes_([1]));
  assertTrue(ime.compareSegmentBoundaries_([0, 2]));

  // ab|cde -> ab|c|de
  ime.shrinkSegment_();
  ime.shrinkSegment_();

  // ab|c|de -> ab|c|de
  ime.clearUpdatedSegmentList_();
  ime.shrinkSegment_();
  assertTrue(ime.compareUpdatedSegmentIndexes_([]));
  assertTrue(ime.compareSegmentBoundaries_([0, 2, 3]));

  // ab|c|de -> ab|cd|e
  ime.clearUpdatedSegmentList_();
  ime.expandSegment_();
  assertTrue(ime.compareUpdatedSegmentIndexes_([1, 2]));
  assertTrue(ime.compareSegmentBoundaries_([0, 2, 4]));

  // ab|cd|e -> abc|d|e
  ime.focusedSegmentIndex_ = 0;
  ime.expandSegment_();

  // abc|d|e -> abcd|e
  ime.clearUpdatedSegmentList_();
  ime.expandSegment_();
  assertTrue(ime.compareUpdatedSegmentIndexes_([0]));
  assertTrue(ime.compareSegmentBoundaries_([0, 4]));
}

function testResetSegments() {
  var ime = getIme();

  ime.resetSegments_();
  assertEquals(sampleImeForImeExtensionApi.SampleIme.State.PRECOMPOSITION,
               ime.state_);
  assertEquals(0, ime.segments_.length);

  ime.insertCharacters_('abc');
  ime.resetSegments_();
  assertEquals(sampleImeForImeExtensionApi.SampleIme.State.COMPOSITION,
               ime.state_);
  assertEquals(1, ime.segments_.length);

  ime.state_ = sampleImeForImeExtensionApi.SampleIme.State.CONVERSION;
  ime.resetSegments_();
  assertEquals(sampleImeForImeExtensionApi.SampleIme.State.COMPOSITION,
               ime.state_);
  assertEquals(1, ime.segments_.length);

  ime.shrinkSegment_();
  ime.resetSegments_();
  assertEquals(sampleImeForImeExtensionApi.SampleIme.State.COMPOSITION,
               ime.state_);
  assertEquals(1, ime.segments_.length);
}

function testCandidates() {
  var ime = getIme();

  ime.insertCharacters_('foostar');
  assertEquals(1, ime.segments_.length);
  assertNotEquals(0, ime.segments_[0].candidates.length);
  assertEquals('\uFF46\uFF4F\uFF4F\uFF53\uFF54\uFF41\uFF52',  // 'ｆｏｏｓｔａｒ'
               ime.segments_[0].candidates[0]);

  // 'foostar' -> 'foo|star'
  ime.shrinkSegment_();
  ime.shrinkSegment_();
  ime.shrinkSegment_();
  ime.shrinkSegment_();

  assertEquals(2, ime.segments_.length);
  assertNotEquals(0, ime.segments_[0].candidates.length);
  assertNotEquals(0, ime.segments_[1].candidates.length);
  assertEquals('\uFF46\uFF4F\uFF4F',  // 'ｆｏｏ'
               ime.segments_[0].candidates[0]);
  assertEquals('\uFF53\uFF54\uFF41\uFF52',  // 'ｓｔａｒ'
               ime.segments_[1].candidates[0]);
  assertNotEquals(-1, findInArray(ime.segments_[1].candidates,
                                  '\u2606'));  // '☆'

  // Dummy candidates.
  var dummyCandidate = 'DummyCandidate1';
  ime.useDummyCandidates_ = false;
  ime.generateCandidates_(0);
  assertEquals(-1, findInArray(ime.segments_[0].candidates, dummyCandidate));
  ime.useDummyCandidates_ = true;
  ime.generateCandidates_(0);
  assertNotEquals(-1, findInArray(ime.segments_[0].candidates, dummyCandidate));

  var segmentLength = ime.segments_.length;
  ime.getInputTextOnSegment_(-1);
  assertEquals(segmentLength, ime.segments_.length);
  ime.getInputTextOnSegment_(100);
  assertEquals(segmentLength, ime.segments_.length);
}

function testSelectCandidate() {
  var ime = getIme();

  ime.focusCandidate_(0);
  assertEquals(sampleImeForImeExtensionApi.SampleIme.State.PRECOMPOSITION,
               ime.state_);
  assertEquals(0, ime.segments_.length);

  ime.insertCharacters_('abcde');
  ime.focusCandidate_(0);
  assertEquals(sampleImeForImeExtensionApi.SampleIme.State.CONVERSION,
               ime.state_);
  assertEquals(0, ime.segments_[0].focusedIndex);

  ime.focusCandidate_(1);
  assertEquals(sampleImeForImeExtensionApi.SampleIme.State.CONVERSION,
               ime.state_);
  assertEquals(1, ime.segments_[0].focusedIndex);

  ime.shrinkSegment_();
  ime.focusSegment_(1);
  ime.focusCandidate_(2);
  assertEquals(2, ime.segments_[1].focusedIndex);

  ime.focusCandidate_(100);
  assertEquals(2, ime.segments_[1].focusedIndex);

  ime.focusCandidate_(-1);
  assertEquals(2, ime.segments_[1].focusedIndex);
}

function testGetPreeditText() {
  var ime = getIme();

  assertEquals('', ime.getPreeditText_());

  ime.insertCharacters_('foo');
  assertEquals('\uFF46\uFF4F\uFF4F', ime.getPreeditText_());  // 'ｆｏｏ'

  ime.insertCharacters_('star');
  // 'foostar' -> 'foo|star'
  ime.shrinkSegment_();
  ime.shrinkSegment_();
  ime.shrinkSegment_();
  ime.shrinkSegment_();
  ime.focusSegment_(1);
  var index = findInArray(ime.segments_[1].candidates, '\u2606');  // '☆'
  assertNotEquals(-1, index);
  ime.focusCandidate_(index);
  assertEquals('\uFF46\uFF4F\uFF4F\u2606', ime.getPreeditText_());  // 'ｆｏｏ☆'
}

function testUpdatePreedit() {
  var ime = getIme();

  // Precomposition mode.
  mockControl.createMethodMock(chrome.input.ime, 'clearComposition')({
    contextID: CONTEXT.contextID
  }, callbackMatcher).$times(1);
  mockControl.$replayAll();
  ime.updatePreedit_();
  mockControl.$verifyAll();
  assertEquals(sampleImeForImeExtensionApi.SampleIme.State.PRECOMPOSITION,
               ime.state_);

  // Composition mode.
  ime.insertCharacters_('abstarc');
  ime.moveCursor_(2);
  mockControl.createMethodMock(chrome.input.ime, 'setComposition')({
    contextID: CONTEXT.contextID,
    text: '\uFF41\uFF42\uFF53\uFF54\uFF41\uFF52\uFF43',  // 'ａｂｓｔａｒｃ'
    segments: [{
      start: 0,
      end: 7,
      style: 'underline'
    }],
    cursor: 2
  }, callbackMatcher).$times(1);
  mockControl.$replayAll();
  ime.updatePreedit_();
  mockControl.$verifyAll();
  assertEquals(sampleImeForImeExtensionApi.SampleIme.State.COMPOSITION,
               ime.state_);

  // 'abstarc' -> 'ab|star|c'
  ime.shrinkSegment_();
  ime.shrinkSegment_();
  ime.shrinkSegment_();
  ime.shrinkSegment_();
  ime.shrinkSegment_();
  ime.focusSegment_(1);
  ime.shrinkSegment_();

  // Conversion mode.
  ime.focusCandidate_(findInArray(ime.segments_[1].candidates,
                                  '\u2606'));  // '☆'
  assertEquals('\uFF41\uFF42\u2606\uFF43', ime.getPreeditText_());
  mockControl.createMethodMock(chrome.input.ime, 'setComposition')({
    contextID: CONTEXT.contextID,
    text: '\uFF41\uFF42\u2606\uFF43',  // 'ａｂ☆ｃ'
    selectionStart: 2,
    selectionEnd: 3,
    segments: [
      {
        start: 0,
        end: 2,
        style: 'underline'
      }, {
        start: 2,
        end: 3,
        style: 'doubleUnderline'
      }, {
        start: 3,
        end: 4,
        style: 'underline'
      }
    ],
    cursor: 2
  }, callbackMatcher).$times(1);
  mockControl.$replayAll();
  ime.updatePreedit_();
  mockControl.$verifyAll();
  assertEquals(sampleImeForImeExtensionApi.SampleIme.State.CONVERSION,
               ime.state_);

  // Backs to composition mode.
  ime.resetSegments_();
  mockControl.createMethodMock(chrome.input.ime, 'setComposition')({
    contextID: CONTEXT.contextID,
    text: '\uFF41\uFF42\uFF53\uFF54\uFF41\uFF52\uFF43',  // 'ａｂｓｔａｒｃ'
    segments: [{
      start: 0,
      end: 7,
      style: 'underline'
    }],
    cursor: 2
  }, callbackMatcher).$times(1);
  mockControl.$replayAll();
  ime.updatePreedit_();
  mockControl.$verifyAll();
  assertEquals(sampleImeForImeExtensionApi.SampleIme.State.COMPOSITION,
               ime.state_);
}

function testUpdateCandidates() {
  var ime = getIme();

  // Precomposition mode.
  mockControl.createMethodMock(
      chrome.input.ime, 'setCandidateWindowProperties')({
        engineID: ENGINE_ID,
        properties: {
          visible: false,
          auxiliaryTextVisible: false
        }
      }, callbackMatcher).$times(1);
  mockControl.$replayAll();
  ime.updateCandidates_();
  mockControl.$verifyAll();

  // Simpe verifier for chrome.input.ime.setCandidates.
  var simpleVerifier = function(expected, actual) {
    assertEquals(expected[0].contextID, actual[0].contextID);
    var candidates = actual[0].candidates;
    assertTrue(candidates.length > 0);
    assertEquals('1st candidate', candidates[0].annotation);
    for (var i = 0; i < candidates.length; ++i) {
      assertEquals(i, candidates[i].id);
      assertTrue(!!candidates[i].candidate);
      var label_index = i % sampleImeForImeExtensionApi.SampleIme.PAGE_SIZE_;
      var label = String.fromCharCode(0x2160 + label_index);  // 'Ⅰ' + index
      assertEquals(label, candidates[i].label);
    }
    assertTrue(callbackMatcher.matches(actual[1]));
    return true;
  };

  // Composition and Conversion mode.
  ime.insertCharacters_('abcde');
  mockControl.createMethodMock(chrome.input.ime, 'setCandidates')({
    contextID: CONTEXT.contextID
  }).$registerArgumentListVerifier('setCandidates', simpleVerifier).$times(1);
  mockControl.createMethodMock(chrome.input.ime, 'setCursorPosition')({
    contextID: CONTEXT.contextID,
    candidateID: ime.segments_[0].focusedIndex
  }, callbackMatcher).$times(1);
  mockControl.createMethodMock(
      chrome.input.ime, 'setCandidateWindowProperties')({
        engineID: ENGINE_ID,
        properties: {
          visible: true,
          cursorVisible: true,
          vertical: true,
          pageSize: 5,
          auxiliaryTextVisible: true,
          auxiliaryText: 'Sample IME'
        }
      }, callbackMatcher).$times(1);
  mockControl.$replayAll();
  ime.updateCandidates_();
  mockControl.$verifyAll();
}

function testUpdateCommitText() {
  var ime = getIme();

  ime.insertCharacters_('abcde');
  ime.shrinkSegment_();
  ime.focusCandidate_(findInArray(ime.segments_[0].candidates, 'Abcd'));
  ime.focusedSegmentIndex_ = 1;
  // 'ｅ'
  ime.focusCandidate_(findInArray(ime.segments_[1].candidates, '\uFF45'));

  var text = ime.getPreeditText_();
  assertEquals('Abcd' + '\uFF45', text);  // 'Abcd|ｅ'
  ime.commit_();
  assertEquals(text, ime.commitText_);

  mockControl.createMethodMock(chrome.input.ime, 'commitText')({
    contextID: CONTEXT.contextID,
    text: text
  }, callbackMatcher).$times(1);
  mockControl.$replayAll();
  ime.updateCommitText_();
  mockControl.$verifyAll();
  assertEquals(null, ime.commitText_);

  // Shouldn't call chrome.input.ime.commitText() since it doesn't have
  // commitText_.
  mockControl.createMethodMock(chrome.input.ime, 'commitText')().$times(0);
  mockControl.$replayAll();
  ime.updateCommitText_();
  mockControl.$verifyAll();
}

function testUpdate() {
  var ime = getIme();

  mockControl.createMethodMock(ime, 'updatePreedit_')().$times(1);
  mockControl.createMethodMock(ime, 'updateCandidates_')().$times(1);
  mockControl.createMethodMock(ime, 'updateCommitText_')().$times(1);
  mockControl.$replayAll();
  ime.update_();
  mockControl.$verifyAll();
}

function testHandleKey() {
  var ime = getIme();

  mockControl.createMethodMock(ime, 'update_')().$times(0);
  mockControl.$replayAll();
  assertFalse(ime.handleKey_({key: '0'}));
  assertFalse(ime.handleKey_({key: '!'}));
  assertFalse(ime.handleKey_({key: 'a', altKey: true}));
  mockControl.$verifyAll();
  assertEquals(sampleImeForImeExtensionApi.SampleIme.State.PRECOMPOSITION,
               ime.state_);

  mockControl.createMethodMock(ime, 'update_')().$times(1);
  mockControl.$replayAll();
  assertTrue(ime.handleKey_({key: 'a'}));
  mockControl.$verifyAll();
  assertEquals(sampleImeForImeExtensionApi.SampleIme.State.COMPOSITION,
               ime.state_);
  assertEquals('a', ime.getInputTextOnSegment_());
}

function testHandleSpecialKey() {
  var ime = getIme();

  // Precomposition mode.
  assertFalse(ime.handleSpecialKey_({key: 'Backspace'}));
  assertFalse(ime.handleSpecialKey_({key: 'Delete'}));
  assertFalse(ime.handleSpecialKey_({key: 'Up'}));
  assertFalse(ime.handleSpecialKey_({key: 'Left'}));
  assertFalse(ime.handleSpecialKey_({key: 'Enter'}));
  assertFalse(ime.handleSpecialKey_({key: 'Left', ctrlKey: true}));
  assertFalse(ime.handleSpecialKey_({key: 'DUMMY'}));

  var verify = function(key, isCalled) {
    var expectedTimes = isCalled ? 1 : 0;
    mockControl.createMethodMock(ime, 'update_')().$times(expectedTimes);
    mockControl.$replayAll();
    assertTrue(ime.handleSpecialKey_(key));
    mockControl.$verifyAll();
  };

  // Composition mode.
  ime.insertCharacters_('abcdefgh');
  verify({key: 'Backspace'}, true);
  assertEquals('abcdefg', ime.getInputTextOnSegment_());
  verify({key: 'Left'}, true);
  assertEquals(6, ime.cursor_);
  verify({key: 'Delete'}, true);
  assertEquals('abcdef', ime.getInputTextOnSegment_());
  verify({key: 'Left'}, true);
  verify({key: 'Left'}, true);
  assertEquals(4, ime.cursor_);
  verify({key: 'Right'}, true);
  assertEquals(5, ime.cursor_);
  verify({key: 'DUMMY'}, false);

  // Conversion mode.
  verify({key: 'Left', shiftKey: true}, true);
  verify({key: 'Left', shiftKey: true}, true);
  assertEquals('abcd', ime.getInputTextOnSegment_());
  verify({key: 'Right', shiftKey: true}, true);
  assertEquals('abcde', ime.getInputTextOnSegment_());
  verify({key: 'Right'}, true);
  assertEquals('f', ime.getInputTextOnSegment_());
  verify({key: 'Left'}, true);
  assertEquals('abcde', ime.getInputTextOnSegment_());
  verify({key: 'Down'}, true);
  assertEquals(1, ime.segments_[0].focusedIndex);
  verify({key: ' '}, true);
  assertEquals(2, ime.segments_[0].focusedIndex);
  verify({key: 'Up'}, true);
  assertEquals(1, ime.segments_[0].focusedIndex);
  verify({key: 'DUMMY'}, false);

  // Check cursor key on Conversion mode.
  var lastCandidateIndex = ime.segments_[0].candidates.length - 1;
  verify({key: 'Up'}, true);
  assertEquals(0, ime.segments_[0].focusedIndex);
  verify({key: 'Up'}, true);
  assertEquals(lastCandidateIndex, ime.segments_[0].focusedIndex);
  verify({key: 'Up'}, true);
  assertEquals(lastCandidateIndex - 1, ime.segments_[0].focusedIndex);
  verify({key: 'Down'}, true);
  assertEquals(lastCandidateIndex, ime.segments_[0].focusedIndex);
  verify({key: 'Down'}, true);
  assertEquals(0, ime.segments_[0].focusedIndex);
  verify({key: 'Right'}, true);
  assertEquals(1, ime.focusedSegmentIndex_);
  verify({key: 'Right'}, true);
  assertEquals(1, ime.focusedSegmentIndex_);
  verify({key: 'Left'}, true);
  assertEquals(0, ime.focusedSegmentIndex_);
  verify({key: 'Left'}, true);
  assertEquals(0, ime.focusedSegmentIndex_);

  verify({key: 'Backspace'}, true);
  assertEquals(sampleImeForImeExtensionApi.SampleIme.State.COMPOSITION,
               ime.state_);
  verify({key: 'Left', shiftKey: true}, true);
  assertEquals(sampleImeForImeExtensionApi.SampleIme.State.CONVERSION,
               ime.state_);
  verify({key: 'Delete'}, true);
  assertEquals(sampleImeForImeExtensionApi.SampleIme.State.COMPOSITION,
               ime.state_);
  verify({key: 'Enter'}, true);
  assertEquals(sampleImeForImeExtensionApi.SampleIme.State.PRECOMPOSITION,
               ime.state_);
}

function testIgnoreKey() {
  var ime = getIme();

  ime.insertCharacters_('a');

  var keyEvent = {
    type: 'keydown',
    key: 'ChromeOSSwitchWindow',
    altKey: false,
    ctrlKey: false,
    shiftKey: false
  };

  assertTrue(ime.onKeyEvent(ENGINE_ID, keyEvent));
  keyEvent.ctrlKey = true;
  assertFalse(ime.onKeyEvent(ENGINE_ID, keyEvent));
  keyEvent.shiftKey = true;
  assertFalse(ime.onKeyEvent(ENGINE_ID, keyEvent));
  keyEvent.altKey = true;
  assertTrue(ime.onKeyEvent(ENGINE_ID, keyEvent));
}

function testSetUpMenu() {
  var ime = getIme();

  // Simpe verifier for chrome.input.ime.setUpMenu.
  var simpleVerifier = function(expected, actual) {
    assertEquals(ENGINE_ID, actual[0].engineID);
    assertEquals(4, actual[0].items.length);
    assertTrue(callbackMatcher.matches(actual[1]));
    return true;
  };

  mockControl.createMethodMock(chrome.input.ime, 'setMenuItems')()
      .$registerArgumentListVerifier('setMenuItems', simpleVerifier).$times(1);

  mockControl.$replayAll();
  ime.setUpMenu_();
  mockControl.$verifyAll();
}

function testOnActivate() {
  var ime = getIme();

  var ANOTHER_ENGINE_ID = 24680;

  mockControl.createMethodMock(ime, 'setUpMenu_')().$times(1);
  mockControl.createMethodMock(ime, 'clear_')().$times(1);
  mockControl.$replayAll();
  ime.onActivate(ANOTHER_ENGINE_ID);
  mockControl.$verifyAll();
  assertEquals(ANOTHER_ENGINE_ID, ime.engineID_);
}

function testOnDeactivated() {
  var ime = getIme();

  mockControl.createMethodMock(ime, 'clear_')().$times(1);
  mockControl.$replayAll();
  ime.onDeactivated(ENGINE_ID);
  mockControl.$verifyAll();
  assertEquals('', ime.engineID_);
}

function testOnFocusBlurUpdate() {
  var ime = getIme();

  assertEquals(CONTEXT, ime.context_);

  mockControl.createMethodMock(ime, 'clear_')().$times(1);
  mockControl.$replayAll();
  ime.onBlur(CONTEXT.contextID);
  mockControl.$verifyAll();
  assertEquals(null, ime.context_);

  mockControl.createMethodMock(ime, 'clear_')().$times(1);
  mockControl.$replayAll();
  ime.onFocus(CONTEXT);
  mockControl.$verifyAll();
  assertEquals(CONTEXT, ime.context_);

  var ANOTHER_CONTEXT = {
    contextID: 54321,
    type: 'text'
  };
  mockControl.createMethodMock(ime, 'clear_')().$times(0);
  mockControl.createMethodMock(ime, 'update_')().$times(1);
  mockControl.$replayAll();
  ime.onInputContextUpdate(ANOTHER_CONTEXT);
  mockControl.$verifyAll();
  assertEquals(ANOTHER_CONTEXT, ime.context_);
}

function testOnKeyEvent() {
  var ime = getIme();

  var input = [
    [false, false],
    [false, true],
    [true, false],
    [true, true]
  ];

  for (var i = 0; i < input.length; ++i) {
    mockControl.createMethodMock(ime, 'handleKey_')(_).
        $returns(input[i][0]).$anyTimes();
    mockControl.createMethodMock(ime, 'handleSpecialKey_')(_).
        $returns(input[i][1]).$anyTimes();

    mockControl.$replayAll();
    assertFalse(ime.onKeyEvent(ENGINE_ID, {type: 'keyup'}));
    assertEquals(input[i][0] || input[i][1],
                 ime.onKeyEvent(ENGINE_ID, {type: 'keydown'}));
    mockControl.$verifyAll();
    mockControl.$resetAll();
  }
}

function testOnCandidateClicked() {
  var ime = getIme();

  mockControl.createMethodMock(ime, 'focusCandidate_')().$times(0);
  mockControl.$replayAll();
  ime.onCandidateClicked(ENGINE_ID, CLICKED_ID, 'right');
  ime.onCandidateClicked(ENGINE_ID, CLICKED_ID, 'middle');
  mockControl.$verifyAll();

  var CLICKED_ID = 3;
  mockControl.createMethodMock(ime, 'focusCandidate_')(CLICKED_ID).$times(1);
  mockControl.createMethodMock(ime, 'update_')().$times(1);
  mockControl.$replayAll();
  ime.onCandidateClicked(ENGINE_ID, CLICKED_ID, 'left');
  mockControl.$verifyAll();
}

function testOnMenuItemActivated() {
  var ime = getIme();

  mockControl.createMethodMock(chrome.input.ime, 'updateMenuItems')().$times(0);
  mockControl.$replayAll();
  ime.onMenuItemActivated(ENGINE_ID, '__dummy_menu_item_name__');
  mockControl.$verifyAll();

  // Simpe verifier for chrome.input.ime.updateMenuItems.
  var simpleVerifier = function(expected, actual) {
    assertEquals(ENGINE_ID, actual[0].engineID);
    assertEquals(4, actual[0].items.length);
    assertTrue(callbackMatcher.matches(actual[1]));
    for (var i = 0; i < expected[0].length; ++i) {
      var expectedItem = expected[0][i];
      var actualItem = actual[0].items[i];
      for (var key in expectedItem) {
        assertEquals(expectedItem[key], actualItem[key]);
      }
    }
    return true;
  };

  // Initial state of menu items
  var expectedItems = [
    {
      id: 'enable_dummy_candidates',
      label: 'Enable dummy candidates',
      style: 'radio',
      checked: true,
      enabled: true,
      visible: true
    }, {
      id: 'disable_dummy_candidates',
      label: 'Disable dummy candidates',
      style: 'radio',
      checked: false,
      enabled: true,
      visible: true
    }, {
      id: 'enable_radio_menu',
      label: 'Enable radio menu',
      style: 'check',
      checked: true
    }, {
      id: 'display_radio_menu',
      label: 'Display radio menu',
      style: 'check',
      checked: true
    }
  ];

  // Disables dummy candidate.
  expectedItems[0].checked = false;
  expectedItems[1].checked = true;
  mockControl.createMethodMock(chrome.input.ime, 'updateMenuItems')(
      expectedItems).$registerArgumentListVerifier(
          'updateMenuItems', simpleVerifier).$times(1);
  mockControl.$replayAll();
  ime.onMenuItemActivated(ENGINE_ID, 'disable_dummy_candidates');
  mockControl.$verifyAll();
  assertFalse(ime.useDummyCandidates_);

  // Enable dummy candidate.
  expectedItems[0].checked = true;
  expectedItems[1].checked = false;
  mockControl.createMethodMock(chrome.input.ime, 'updateMenuItems')(
      expectedItems).$registerArgumentListVerifier(
          'updateMenuItems', simpleVerifier).$times(1);
  mockControl.$replayAll();
  ime.onMenuItemActivated(ENGINE_ID, 'enable_dummy_candidates');
  mockControl.$verifyAll();
  assertTrue(ime.useDummyCandidates_);

  // Disables radio menu item.
  expectedItems[0].enabled = false;
  expectedItems[1].enabled = false;
  expectedItems[2].checked = false;
  mockControl.createMethodMock(chrome.input.ime, 'updateMenuItems')(
      expectedItems).$registerArgumentListVerifier(
          'updateMenuItems', simpleVerifier).$times(1);
  mockControl.$replayAll();
  ime.onMenuItemActivated(ENGINE_ID, 'enable_radio_menu');
  mockControl.$verifyAll();

  // Enables radio menu item.
  expectedItems[0].enabled = true;
  expectedItems[1].enabled = true;
  expectedItems[2].checked = true;
  mockControl.createMethodMock(chrome.input.ime, 'updateMenuItems')(
      expectedItems).$registerArgumentListVerifier(
          'updateMenuItems', simpleVerifier).$times(1);
  mockControl.$replayAll();
  ime.onMenuItemActivated(ENGINE_ID, 'enable_radio_menu');
  mockControl.$verifyAll();

  // Hides radio menu item.
  expectedItems[0].visible = false;
  expectedItems[1].visible = false;
  expectedItems[3].checked = false;
  mockControl.createMethodMock(chrome.input.ime, 'updateMenuItems')(
      expectedItems).$registerArgumentListVerifier(
          'updateMenuItems', simpleVerifier).$times(1);
  mockControl.$replayAll();
  ime.onMenuItemActivated(ENGINE_ID, 'display_radio_menu');
  mockControl.$verifyAll();

  // Shows radio menu item.
  expectedItems[0].visible = true;
  expectedItems[1].visible = true;
  expectedItems[3].checked = true;
  mockControl.createMethodMock(chrome.input.ime, 'updateMenuItems')(
      expectedItems).$registerArgumentListVerifier(
          'updateMenuItems', simpleVerifier).$times(1);
  mockControl.$replayAll();
  ime.onMenuItemActivated(ENGINE_ID, 'display_radio_menu');
  mockControl.$verifyAll();
}
