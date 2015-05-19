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
 * @fileoverview Sample IME for ChromeOS with IME Extension API.
 */

'use strict';

/**
 * Sample IME with IME extension API.
 * @constructor
 */
function SampleIme() {
  /**
   * Context information which is provided by Chrome.
   * @type {?Object}
   * @private
   */
  this.context_ = null;

  /**
   * Engine id which is specified on manifest.js.
   * @type {string}
   * @private
   */
  this.engineID_ = '';

  /**
   * Indicates IME suggests dummy candidates or not.
   * @type {boolean}
   * @private
   */
  this.useDummyCandidates_ = true;

  /**
   * Immutable conversion table. It is used to suggest special candidates.
   * @type {Object}
   * @private
   */
  this.conversionTable_ = {
    star: '\u2606',  // '☆'
    heart: '\u2661'  // '♡'
  };

  /**
   * Ignore key table not to handle some keys.
   * @type {Object}
   * @private
   */
  this.ignoreKeySet_ = {};

  var ignoreKeys = [
    {  // PrintScreen
      key: 'ChromeOSSwitchWindow',
      ctrlKey: true
    }, {  // PrintScreen
      key: 'ChromeOSSwitchWindow',
      ctrlKey: true,
      shiftKey: true
    }
  ];
  for (var i = 0; i < ignoreKeys.length; ++i) {
    this.ignoreKeySet_[this.stringifyKeyAndModifiers_(ignoreKeys[i])] = true;
  }

  // Some properties are initialized on {@code SampleIme.clear_}.
  this.clear_();
}

/**
 * Enum of IME state.
 * @enum {number}
 */
SampleIme.State = {
  /** IME doesn't have any input text. */
  PRECOMPOSITION: 0,
  /**
   * IME has a input text, but no candidate are expressly selected or input text
   * is not segmented.
   */
  COMPOSITION: 1,
  /**
   * IME has a input text, and one of the candidate is selected or input text is
   * segmentated.
   */
  CONVERSION: 2
};

/**
 * Segment information of a composition text.
 * @constructor
 */
SampleIme.Segment = function() {
  /**
   * Start position of the segment.
   * @type {number}
   */
  this.start = 0;

  /**
   * Candidates list.
   * @type {Array.<string>}
   */
  this.candidates = [];

  /**
   * Focused candidate index.
   * @type {number}
   */
  this.focusedIndex = 0;
};

/**
 * Clears properties of IME.
 * @private
 */
SampleIme.prototype.clear_ = function() {
  /**
   * Raw input text.
   * @type {string}
   * @private
   */
  this.inputText_ = '';

  /**
   * Commit text.
   * This is a volatile property, and will be cleared by
   * {@code SampleIme.updateCommitText_}.
   * @type {?string}
   * @private
   */
  this.commitText_ = null;

  /**
   * Segments information.
   * @type {Array.<SampleIme.Segment>}
   * @private
   */
  this.segments_ = [];

  /**
   * Cursor position.
   * @type {number}
   * @private
   */
  this.cursor_ = 0;

  /**
   * Focused segment index.
   * @type {number}
   * @private
   */
  this.focusedSegmentIndex_ = 0;

  /**
   * The state of the IME.
   * @type {SampleIme.State}
   * @private
   */
  this.state_ = SampleIme.State.PRECOMPOSITION;
};

/**
 * Stringify key event data.
 * @param {Object} keyData key event data.
 * @return {string} stringified key event data.
 * @private
 */
SampleIme.prototype.stringifyKeyAndModifiers_ = function(keyData) {
  var keys = [keyData.key];
  if (keyData.altKey) { keys.push('alt'); }
  if (keyData.ctrlKey) { keys.push('ctrl'); }
  if (keyData.shiftKey) { keys.push('shift'); }
  return keys.join(' ');
};

/**
 * Append a new empty segment on SampleIme.segments.
 * @private
 */
SampleIme.prototype.appendNewSegment_ = function() {
  var startPosition = this.inputText_.length;
  if (this.segments_.length === 0) {
    startPosition = 0;
  }

  var newSegment = new SampleIme.Segment();
  newSegment.start = startPosition;

  this.segments_.push(newSegment);
};

/**
 * Get input text on the segment.
 * @param {number=} opt_segmentIndex index of the segment you want to get
 *     a text. this.focusedSegmentIndex_ is used as a default value.
 * @return {string} input text of the segment.
 * @private
 */
SampleIme.prototype.getInputTextOnSegment_ = function(opt_segmentIndex) {
  if (this.state_ === SampleIme.State.PRECOMPOSITION) {
    return '';
  }

  var segmentIndex = (opt_segmentIndex == null) ?
      this.focusedSegmentIndex_ : opt_segmentIndex;
  if (segmentIndex < 0 || this.segments_.length <= segmentIndex) {
    return '';
  }

  var start = this.segments_[segmentIndex].start;
  var end = (segmentIndex + 1 === this.segments_.length) ?
      this.inputText_.length : this.segments_[segmentIndex + 1].start;
  var length = end - start;

  return this.inputText_.substr(start, length);
};

/**
 * Generates and sets candidates of the segment.
 * @param {number=} opt_segmentIndex index of the segment you want to get
 *     a text. this.focusedSegmentIndex_ is used as a default value.
 * @private
 */
SampleIme.prototype.generateCandidates_ = function(opt_segmentIndex) {
  if (this.state_ === SampleIme.State.PRECOMPOSITION) {
    return;
  }

  var segmentIndex = (opt_segmentIndex == null) ?
      this.focusedSegmentIndex_ : opt_segmentIndex;
  if (segmentIndex < 0 || this.segments_.length <= segmentIndex) {
    return;
  }

  var segment = this.segments_[segmentIndex];
  var text = this.getInputTextOnSegment_(segmentIndex);

  segment.focusedIndex = 0;

  if (text === '') {
    segment.candidates = [];
    return;
  }

  segment.candidates = [
    text.replace(/(\w)/g, function(match) {
      // Converts ASCII alphabet characters to its fullwidth versions.
      var offset = 65248;
      return String.fromCharCode(match.charCodeAt(0) + offset);
    }),
    text,
    text.toUpperCase(),
    text.substr(0, 1).toUpperCase() + text.substr(1).toLowerCase()
  ];

  if (text in this.conversionTable_) {
    segment.candidates.push(this.conversionTable_[text]);
  }

  if (this.useDummyCandidates_) {
    segment.candidates.push('DummyCandidate1');
    segment.candidates.push('DummyCandidate2');
  }
};

/**
 * Gets preedit text.
 * @return {string} preedit text.
 * @private
 */
SampleIme.prototype.getPreeditText_ = function() {
  if (this.state_ === SampleIme.State.PRECOMPOSITION) {
    return '';
  }

  var texts = [];
  for (var i = 0; i < this.segments_.length; ++i) {
    var segment = this.segments_[i];
    texts.push(segment.candidates[segment.focusedIndex]);
  }
  return texts.join('');
};

/**
 * Updates preedit text.
 * @private
 */
SampleIme.prototype.updatePreedit_ = function() {
  if (this.state_ === SampleIme.State.PRECOMPOSITION) {
    chrome.input.ime.clearComposition({
      contextID: parseInt(this.context_.contextID)
    });
    return;
  }

  var segmentsData = [];
  for (var i = 0; i < this.segments_.length; ++i) {
    var text = this.segments_[i].candidates[this.segments_[i].focusedIndex];
    var start = (i === 0) ? 0 : segmentsData[i - 1].end;
    var end = start + text.length;

    segmentsData.push({
      start: start,
      end: end,
      style: 'underline'
    });
  }
  if (this.state_ === SampleIme.State.CONVERSION) {
    segmentsData[this.focusedSegmentIndex_].style = 'doubleUnderline';
  }

  var cursorPos = 0;
  if (this.state_ === SampleIme.State.CONVERSION) {
    for (var i = 0; i < this.focusedSegmentIndex_; ++i) {
      var segment = this.segments_[i];
      cursorPos += segment.candidates[segment.focusedIndex].length;
    }
  } else {
    cursorPos = this.cursor_;
  }

  var composition = {
    contextID: this.context_.contextID,
    text: this.getPreeditText_(),
    segments: segmentsData,
    cursor: cursorPos
  };

  if (this.state_ === SampleIme.State.CONVERSION) {
    composition.selectionStart = segmentsData[this.focusedSegmentIndex_].start;
    composition.selectionEnd = segmentsData[this.focusedSegmentIndex_].end;
  }

  chrome.input.ime.setComposition(composition);
};

/**
 * Updates candidates.
 * @private
 */
SampleIme.prototype.updateCandidates_ = function() {
  if (this.state_ === SampleIme.State.PRECOMPOSITION) {
    chrome.input.ime.setCandidateWindowProperties({
      engineID: this.engineID_,
      properties: {
        visible: false,
        auxiliaryTextVisible: false
      }
    });
  } else {
    var segment = this.segments_[this.focusedSegmentIndex_];

    chrome.input.ime.setCandidates({
      contextID: this.context_.contextID,
      candidates: segment.candidates.map(function(value, index) {
        return {
          candidate: value,
          id: index
        };
      })
    });
    chrome.input.ime.setCursorPosition({
      contextID: this.context_.contextID,
      candidateID: segment.focusedIndex
    });
    chrome.input.ime.setCandidateWindowProperties({
      engineID: this.engineID_,
      properties: {
        visible: true,
        cursorVisible: true,
        vertical: true,
        pageSize: 5,
        auxiliaryTextVisible: true,
        auxiliaryText: 'Sample IME'
      }
    });
  }
};

/**
 * Updates commit text if {@code commitText_} isn't null.
 * This function clears {@code commitText_} since it is a volatile property.
 * @private
 */
SampleIme.prototype.updateCommitText_ = function() {
  if (this.commitText_ === null) {
    return;
  }

  chrome.input.ime.commitText({
    contextID: this.context_.contextID,
    text: this.commitText_
  });

  this.commitText_ = null;
};

/**
 * Updates output using IME Extension API.
 * @private
 */
SampleIme.prototype.update_ = function() {
  this.updatePreedit_();
  this.updateCandidates_();
  this.updateCommitText_();
};

/**
 * Commits a preedit text and clears a context.
 * @private
 */
SampleIme.prototype.commit_ = function() {
  if (this.state_ === SampleIme.State.PRECOMPOSITION) {
    return;
  }

  var commitText = this.getPreeditText_();
  this.clear_();
  this.commitText_ = commitText;
};

/**
 * Inserts characters into the cursor position.
 * @param {string} value text we want to insert into.
 * @private
 */
SampleIme.prototype.insertCharacters_ = function(value) {
  if (this.state_ === SampleIme.State.CONVERSION) {
    return;
  }

  if (this.state_ === SampleIme.State.PRECOMPOSITION) {
    this.appendNewSegment_();
    this.focusedSegmentIndex_ = 0;
  }

  var text = this.inputText_;
  this.inputText_ =
      text.substr(0, this.cursor_) + value + text.substr(this.cursor_);
  this.state_ = SampleIme.State.COMPOSITION;
  this.moveCursor_(this.cursor_ + value.length);

  this.generateCandidates_();
};

/**
 * Removes a character.
 * @param {number} index index of the character you want to remove.
 * @private
 */
SampleIme.prototype.removeCharacter_ = function(index) {
  if (this.state_ !== SampleIme.State.COMPOSITION) {
    return;
  }

  if (index < 0 || this.inputText_.length <= index) {
    return;
  }

  this.inputText_ =
      this.inputText_.substr(0, index) + this.inputText_.substr(index + 1);

  if (this.inputText_.length === 0) {
    this.clear_();
    return;
  }

  if (index < this.cursor_) {
    this.moveCursor_(this.cursor_ - 1);
  }

  this.generateCandidates_();
};

/**
 * Moves a cursor position.
 * @param {number} index Cursor position.
 * @private
 */
SampleIme.prototype.moveCursor_ = function(index) {
  if (this.state_ !== SampleIme.State.COMPOSITION) {
    return;
  }

  if (index < 0 || this.inputText_.length < index) {
    return;
  }

  this.cursor_ = index;
};

/**
 * Expands a focused segment.
 * @private
 */
SampleIme.prototype.expandSegment_ = function() {
  if (this.state_ === SampleIme.State.PRECOMPOSITION) {
    return;
  }

  this.state_ = SampleIme.State.CONVERSION;

  var index = this.focusedSegmentIndex_;
  var segments = this.segments_;
  if (index + 1 >= segments.length) {
    return;
  }

  if ((index + 2 === segments.length &&
       segments[index + 1].start + 1 === this.inputText_.length) ||
      (index + 2 < segments.length &&
       segments[index + 1].start + 1 === segments[index + 2].start)) {
    segments.splice(index + 1, 1);
  } else {
    ++segments[index + 1].start;
    this.generateCandidates_(index + 1);
  }

  this.generateCandidates_();
};

/**
 * Shrinks a focused segment.
 * @private
 */
SampleIme.prototype.shrinkSegment_ = function() {
  if (this.state_ === SampleIme.State.PRECOMPOSITION) {
    return;
  }

  this.state_ = SampleIme.State.CONVERSION;

  var segments = this.segments_;
  var index = this.focusedSegmentIndex_;

  if (index + 1 === segments.length) {
    if (this.inputText_.length - segments[index].start > 1) {
      this.appendNewSegment_();
      segments[index + 1].start = this.inputText_.length - 1;
      this.generateCandidates_();
      this.generateCandidates_(index + 1);
    }
  } else {
    if (segments[index + 1].start - segments[index].start > 1) {
      --segments[index + 1].start;
      this.generateCandidates_();
      this.generateCandidates_(index + 1);
    }
  }
};

/**
 * Resets a segmentation data of the preedit text.
 * @private
 */
SampleIme.prototype.resetSegments_ = function() {
  if (this.state_ !== SampleIme.State.CONVERSION) {
    return;
  }

  this.segments_ = [];
  this.appendNewSegment_();
  this.focusedSegmentIndex_ = 0;
  this.generateCandidates_();
  this.state_ = SampleIme.State.COMPOSITION;
};

/**
 * Selects a candidate.
 * @param {number} candidateID index of the candidate.
 * @private
 */
SampleIme.prototype.focusCandidate_ = function(candidateID) {
  if (this.state_ === SampleIme.State.PRECOMPOSITION) {
    return;
  }

  var segment = this.segments_[this.focusedSegmentIndex_];
  if (candidateID < 0 || segment.candidates.length <= candidateID) {
    return;
  }

  segment.focusedIndex = candidateID;
  this.state_ = SampleIme.State.CONVERSION;
};

/**
 * Focuses a segment.
 * @param {number} segmentID index of the segment.
 * @private
 */
SampleIme.prototype.focusSegment_ = function(segmentID) {
  if (this.state_ !== SampleIme.State.CONVERSION) {
    return;
  }

  if (segmentID < 0 || this.segments_.length <= segmentID) {
    return;
  }

  this.focusedSegmentIndex_ = segmentID;
};

/**
 * Handles a alphabet key.
 * @param {Object} keyData key event data.
 * @return {boolean} true if key event is consumed.
 * @private
 */
SampleIme.prototype.handleKey_ = function(keyData) {
  var keyValue = keyData.key;

  if (keyData.altKey || keyData.ctrlKey || keyData.shiftKey) {
    return false;
  }

  if (!keyValue.match(/^[a-z]$/i)) {
    return false;
  }

  if (this.state_ === SampleIme.State.CONVERSION) {
    this.commit_();
  }

  this.insertCharacters_(keyValue);

  this.update_();

  return true;
};

/**
 * Handles a non-alphabet key.
 * @param {Object} keyData key event data.
 * @return {boolean} true if key event is consumed.
 * @private
 */
SampleIme.prototype.handleSpecialKey_ = function(keyData) {
  if (this.state_ === SampleIme.State.PRECOMPOSITION) {
    return false;
  }

  var segment = this.segments_[this.focusedSegmentIndex_];

  if (!keyData.altKey && !keyData.ctrlKey && !keyData.shiftKey) {
    switch (keyData.key) {
    case 'Backspace':
      if (this.state_ === SampleIme.State.CONVERSION) {
        this.resetSegments_();
      } else if (this.cursor_ !== 0) {
        this.removeCharacter_(this.cursor_ - 1);
      }
      break;
    case 'Delete':
      if (this.state_ === SampleIme.State.CONVERSION) {
        this.resetSegments_();
      } else if (this.cursor_ !== this.inputText_.length) {
        this.removeCharacter_(this.cursor_);
      }
      break;
    case 'Up':
      var previous_index = segment.focusedIndex - 1;
      if (previous_index === -1) {
        previous_index = segment.candidates.length - 1;
      }
      this.focusCandidate_(previous_index);
      break;
    case 'Down':
    case ' ':
      var next_index = segment.focusedIndex + 1;
      if (next_index === segment.candidates.length) {
        next_index = 0;
      }
      this.focusCandidate_(next_index);
      break;
    case 'Left':
      if (this.state_ === SampleIme.State.CONVERSION) {
        if (this.focusedSegmentIndex_ !== 0) {
          this.focusSegment_(this.focusedSegmentIndex_ - 1);
        }
      } else {
        this.moveCursor_(this.cursor_ - 1);
      }
      break;
    case 'Right':
      if (this.state_ === SampleIme.State.CONVERSION) {
        if (this.focusedSegmentIndex_ + 1 !== this.segments_.length) {
          this.focusSegment_(this.focusedSegmentIndex_ + 1);
        }
      } else {
        this.moveCursor_(this.cursor_ + 1);
      }
      break;
    case 'Enter':
      this.commit_();
      break;
    default:
      return true;
    }
  } else if (!keyData.altKey && !keyData.ctrlKey && keyData.shiftKey) {
    switch (keyData.key) {
    case 'Left':
      if (this.state_ !== SampleIme.State.PRECOMPOSITION) {
        this.shrinkSegment_();
      }
      break;
    case 'Right':
      if (this.state_ !== SampleIme.State.PRECOMPOSITION) {
        this.expandSegment_();
      }
      break;
    default:
      return true;
    }
  } else {
    return true;
  }

  this.update_();
  return true;
};

/**
 * Sets up a menu on a uber tray.
 * @private
 */
SampleIme.prototype.setUpMenu_ = function() {
  chrome.input.ime.setMenuItems({
    engineID: this.engineID_,
    items: [
      {
        id: 'use_dummy_candidates',
        label: 'Disable dummy candidates',
        style: 'none'
      }
    ]
  });
};

/**
 * Callback method. It is called when IME is activated.
 * @param {string} engineID engine ID.
 */
SampleIme.prototype.onActivate = function(engineID) {
  console.log('onActivate [' + engineID + ']');
  this.engineID_ = engineID;
  this.clear_();
  this.setUpMenu_();
};

/**
 * Callback method. It is called when IME is deactivated.
 * @param {string} engineID engine ID.
 */
SampleIme.prototype.onDeactivated = function(engineID) {
  console.log('onDeactivated [' + engineID + ']');
  this.clear_();
  this.engineID_ = '';
};

/**
 * Callback method. It is called when a context acquires a focus.
 * @param {Object} context context information.
 */
SampleIme.prototype.onFocus = function(context) {
  console.log('onFocus');
  this.context_ = context;
  this.clear_();
};

/**
 * Callback method. It is called when a context lost a focus.
 * @param {number} contextID ID of the context.
 */
SampleIme.prototype.onBlur = function(contextID) {
  console.log('onBlur');
  this.clear_();
  this.context_ = null;
};

/**
 * Callback method. It is called when properties of the context is changed.
 * @param {Object} context context information.
 */
SampleIme.prototype.onInputContextUpdate = function(context) {
  console.log('onInputContextUpdate');
  this.context_ = context;
  this.update_();
};

/**
 * Callback method. It is called when IME catches a new key event.
 * @param {number} engineID ID of the engine.
 * @param {Object} keyData key event data.
 * @return {boolean} true if the key event is consumed.
 */
SampleIme.prototype.onKeyEvent = function(engineID, keyData) {
  console.log('onKeyEvent [' + engineID + '] key: ' + keyData.key);

  if (keyData.type !== 'keydown') {
    return false;
  }

  if (this.ignoreKeySet_[this.stringifyKeyAndModifiers_(keyData)]) {
    return false;
  }

  return this.handleKey_(keyData) || this.handleSpecialKey_(keyData);
};

/**
 * Callback method. It is called when candidates on candidate window is clicked.
 * @param {number} engineID ID of the engine.
 * @param {number} candidateID Index of the candidate.
 * @param {string} button Which mouse button was clicked.
 */
SampleIme.prototype.onCandidateClicked =
    function(engineID, candidateID, button) {
  console.log('onCandidateClicked [' + engineID + ']');
  if (button === 'left') {
    this.focusCandidate_(candidateID);
  }
};

/**
 * Callback method. It is called when menu item on uber tray is activated.
 * @param {number} engineID ID of the engine.
 * @param {string} name name of the menu item.
 */
SampleIme.prototype.onMenuItemActivated = function(engineID, name) {
  console.log('onMenuItemActivated [' + engineID + ']');

  if (name !== 'use_dummy_candidates') {
    return;
  }
  var menuItem = {
    id: 'use_dummy_candidates',
    style: 'none'
  };

  this.useDummyCandidates_ = !this.useDummyCandidates_;
  if (this.useDummyCandidates_) {
    menuItem.label = 'Disable dummy candidates';
  } else {
    menuItem.label = 'Enable dummy candidates';
  }

  chrome.input.ime.updateMenuItems({
    engineID: engineID,
    items: [menuItem]
  });
};

/**
 * Initializes IME.
 */
document.addEventListener('readystatechange', function() {
  if (document.readyState === 'complete') {
    console.log('Initializing Sample IME...');
    var sample_ime = new SampleIme;

    chrome.input.ime.onActivate.addListener(
      function(engineID) { sample_ime.onActivate(engineID); });
    chrome.input.ime.onDeactivated.addListener(
      function(engineID) { sample_ime.onDeactivated(engineID); });
    chrome.input.ime.onFocus.addListener(
      function(context) { sample_ime.onFocus(context); });
    chrome.input.ime.onBlur.addListener(
      function(contextID) { sample_ime.onBlur(contextID); });
    chrome.input.ime.onInputContextUpdate.addListener(
      function(context) { sample_ime.onInputContextUpdate(context); });
    chrome.input.ime.onKeyEvent.addListener(
      function(engineID, keyData) {
        return sample_ime.onKeyEvent(engineID, keyData);
      });
    chrome.input.ime.onCandidateClicked.addListener(
      function(engineID, candidateID, button) {
        sample_ime.onCandidateClicked(engineID, candidateID, button);
      });
    chrome.input.ime.onMenuItemActivated.addListener(
      function(engineID, name) {
        sample_ime.onMenuItemActivated(engineID, name);
      });
  }
});
