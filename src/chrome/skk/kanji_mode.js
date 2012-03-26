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
 * @fileoverview This file contains Kanji preedit/conversion modes.
 */

'use strict';

console.assert(
    skk.mode, 'ime_mode_interface.js must be loaded prior to this module');

/**
 * Initializes preedit mode.
 * @constructor
 * @implements {skk.mode.InputModeInterface}
 * @param {skk.IME} ime IME instance.
 */
skk.mode.Preedit = function(ime) {
  this.mark_ = '\u25bd';  // '▽'; default preedit mark
  this.ime_ = ime;
  this.preedit_ = '';
  this.composer_ = new skk.Composer(skk.Composer.map.hiragana);
  this.waitingDeterminant_ = false;
};

/**
 * Sets symbol character that will appear in head of preedit.
 * @param {string} mark Symbol character.
 */
skk.mode.Preedit.prototype.setPreeditMark = function(mark) {
  this.mark_ = mark;
};

/**
 * @inheritDoc
 */
skk.mode.Preedit.prototype.reset = function() {
  console.debug('skk.mode.Preedit.reset');
  this.ime_.leaveCurrentMode();
};

/**
 * @inheritDoc
 */
skk.mode.Preedit.prototype.enter = function() {
  console.debug('skk.mode.Preedit.enter');
};

/**
 * @inheritDoc
 */
skk.mode.Preedit.prototype.leave = function() {
  console.debug('skk.mode.Preedit.leave');
  this.preedit_ = '';
  this.composer_.reset();
  skk.util.showPreedit(this.ime_.context.contextID, this.preedit_);
};

/**
 * @inheritDoc
 */
skk.mode.Preedit.prototype.resume = function() {
  console.debug('skk.mode.Preedit.resume');
  skk.util.showPreedit(
      this.ime_.context.contextID,
      this.mark_ + this.preedit_ + this.composer_.undetermined);
};

/**
 * @inheritDoc
 */
skk.mode.Preedit.prototype.suspend = function() {
  console.debug('skk.mode.Preedit.suspend');
  skk.util.showPreedit(this.ime_.context.contextID, '');
};

/**
 * @return {boolean} Returns true if the mode has preedit text, otherwise false.
 */
skk.mode.Preedit.prototype.hasPreedit = function() {
  console.debug('skk.mode.Preedit.hasPreedit');
  return this.preedit_ !== '';
};

/**
 * Removes last input preedit character.
 * @return {?string} Chopped character if there is preedit string,
 *     otherwise null.
 */
skk.mode.Preedit.prototype.backspace = function() {
  console.debug('skk.mode.Preedit.backspace');
  if (this.composer_.hasUndetermined()) {
    return this.composer_.backspace();
  }

  if (!this.hasPreedit()) { return null; }

  var chopped = this.preedit_.substr(-1);
  this.preedit_ = this.preedit_.slice(0, -1);
  return chopped;
};

/**
 * Looks up conversion candidates.
 * @param {string} base No conjugation part of word. 1+ length Hiragana string.
 * @param {string} stem Stem of conjugation. 0+ length latin string.
 * @param {string} conjugated First character of conjugated suffix kana.
 * @param {boolean=} opt_predictive If true was set, lists predicted candidates
 *     instead of exactly matching ones.
 */
skk.mode.Preedit.prototype.lookupCandidate = function(base,
                                                      stem,
                                                      conjugated,
                                                      opt_predictive) {
  var self = this;
  var conjugations = skk.VOWELS.map(
      function(vowel) {
        var roman = stem + vowel;
        return skk.Composer.map.hiragana[roman];
      }).filter(function(kana) { return !!kana; });
  var conjugationPattern = new RegExp('(' + conjugations.join('|') + ')$');

  self.ime_.dict.lookup(
      base, stem,
      function(isError, candidates, predictions) {
        if (isError) {
          console.error('skk.mode.Preedit.lookupCandidate: Error occured');
          return;
        }

        // TODO(sekia): Implement predictive conversion
        self.ime_.enterNewMode(skk.InputMode.CONVERSION, function(mode) {
          if (stem !== '') {
            candidates = Array.uniq(candidates.filter(
                function(candidate) {
                  return !!candidate.match(conjugationPattern);
                }).map(
                    function(candidate) {
                      return candidate.slice(0, -1) + conjugated;
                    }));
          }
          if (candidates.length === 0) { candidates = [base + conjugated]; }
          mode.setWordBaseAndStem(base, stem);
          mode.setPreedit(base + conjugated);
          mode.setCandidates(candidates);
        });
      });
};

/**
 * @inheritDoc
 */
skk.mode.Preedit.prototype.prepareForKey = function(keyEvent) {
  if (keyEvent.key === 'j' && keyEvent.ctrlKey) {
    this.ime_.leaveCurrentMode();
    return null;
  }

  if (skk.util.isArrow(keyEvent.key)) { return null; }

  if (skk.util.isSpace(keyEvent.key) &&
      this.hasPreedit() && !this.composer_.hasUndetermined()) {
    this.lookupCandidate(this.preedit_, '', '');
    return null;
  }

  if (skk.util.isBackspace(keyEvent.key)) {
    var chopped = this.backspace();
    console.debug('Chopped: ', chopped);
    if (this.composer_.undetermined === '') {
      this.waitingDeterminant_ = false;
    }

    if (!!chopped) {
      skk.util.showPreedit(
          this.ime_.context.contextID,
          this.mark_ + this.preedit_ + this.composer_.undetermined);
    } else {  // There's no preedit text any more
      this.ime_.leaveCurrentMode();
    }
    return null;
  }

  if (skk.util.isEnter(keyEvent.key)) {
    chrome.input.ime.commitText(
        {
          contextID: this.ime_.context.contextID,
          text: this.preedit_ + this.composer_.undetermined
        });
    this.preedit_ = '';
    this.composer_.undetermined = '';
    skk.util.showPreedit(this.ime_.context.contextID, this.preedit_);
    return null;
  }

  return keyEvent;
};

/**
 * @inheritDoc
 */
skk.mode.Preedit.prototype.addKey = function(keyEvent) {
  console.debug('skk.mode.Preedit.addKey: ', keyEvent.key);

  if (!skk.util.isPrintable(keyEvent.key)) { return false; }

  if (skk.util.isUpperLetter(keyEvent.key) &&
      !skk.util.isVowel(keyEvent.key) && this.hasPreedit()) {
    this.waitingDeterminant_ = true;
  }

  keyEvent.key = keyEvent.key.toLowerCase();

  console.debug('skk.mode.Preedit.addKey: ',
                'Undetermined: ', this.composer_.undetermined);

  var base = this.preedit_;
  var stem = this.composer_.undetermined;
  var determAndUndeterm = this.composer_.addKey(keyEvent.key);
  var determined = determAndUndeterm[0] || '';
  var undetermined = determAndUndeterm[1] || '';

  if (this.waitingDeterminant_ && determined !== '') {
    this.lookupCandidate(base, stem, determined);
    return true;
  }

  this.preedit_ += determined;
  skk.util.showPreedit(
      this.ime_.context.contextID,
      this.mark_ + this.preedit_ +
          (this.waitingDeterminant_ ? '*' : '') + undetermined);

  return true;
};

/**
 * Initializes candidate select mode.
 * @constructor
 * @implements {skk.mode.InputModeInterface}
 * @param {skk.IME} ime IME instance.
 */
skk.mode.Conversion = function(ime) {
  this.mark_ = '\u25bc';  // '▼'; default selection mark
  this.ime_ = ime;
  this.base_ = null;
  this.stem_ = null;
  this.preedit_ = null;
  this.candidates_ = null;
  this.selectedIndex_ = 0;
};

/**
 * @param {string} base No conjugation part of word. 1+ length Hiragana string.
 * @param {string} stem Stem of conjugation. 0+ length latin string.
 */
skk.mode.Conversion.prototype.setWordBaseAndStem = function(base, stem) {
  this.base_ = base;
  this.stem_ = stem;
};

/**
 * Sets symbol character that will appear in head of preedit.
 * @param {string} mark Symbol character.
 */
skk.mode.Conversion.prototype.setPreeditMark = function(mark) {
  this.mark_ = mark;
};

/**
 * Sets preedit string to be shown.
 * @param {string} preedit Preedit text.
 */
skk.mode.Conversion.prototype.setPreedit = function(preedit) {
  this.preedit_ = preedit;
};

/**
 * Sets conversion candidates to be displayed.
 * @param {Array.<string>} candidates Candidates to be listed.
 */
skk.mode.Conversion.prototype.setCandidates = function(candidates) {
  this.candidates_ = candidates;
  chrome.input.ime.setCandidates(
      {
        contextID: this.ime_.context.contextID,
        candidates: this.candidates_.map(
            function(candidate, index) {
              return {
                candidate: candidate,
                id: index
              };
            })
      });
};

/**
 * @inheritDoc
 */
skk.mode.Conversion.prototype.reset = function() {
  console.debug('skk.mode.Conversion.reset');
  this.ime_.leaveCurrentMode();  // Conversion mode

  console.assert(this.ime_.getCurrentMode() instanceof skk.mode.Preedit,
                 'skk.mode.Conversion:reset: Internal error: ' +
                     'Current input mode must be skk.mode.Preedit');
  this.ime_.leaveCurrentMode();  // Preedit mode
};

/**
 * @inheritDoc
 */
skk.mode.Conversion.prototype.enter = function() {
  console.debug('skk.mode.Conversion.enter');
  skk.util.showPreedit(
      this.ime_.context.contextID, this.mark_ + this.preedit_);
  this.showCandidateWindow();
};

/**
 * @inheritDoc
 */
skk.mode.Conversion.prototype.leave = function() {
  console.debug('skk.mode.Conversion.leave');
  this.preedit_ = '';
  this.candidates_ = [];
  skk.util.showPreedit(this.ime_.context.contextID, this.preedit_);
  this.hideCandidateWindow();
};

/**
 * @inheritDoc
 */
skk.mode.Conversion.prototype.resume = function() {
  console.debug('skk.mode.Conversion.resume');
  skk.util.showPreedit(
      this.ime_.context.contextID, this.mark_ + this.preedit_);
  this.showCandidateWindow();
};

/**
 * @inheritDoc
 */
skk.mode.Conversion.prototype.suspend = function() {
  console.debug('skk.mode.Conversion.suspend');
  skk.util.showPreedit(this.ime_.context.contextID, '');
  this.hideCandidateWindow();
};

/**
 * Commits selected candidates and leaves kanji mode.
 * @param {integer} candidateId 0-origin candidate index.
 */
skk.mode.Conversion.prototype.commitCandidate = function(candidateId) {
  var selection = this.candidates_[candidateId];
  this.ime_.dict.addHistory(this.base_, this.stem_, selection);
  chrome.input.ime.commitText(
      {
        contextID: this.ime_.context.contextID,
        text: selection
      });
  skk.util.showPreedit(this.ime_.context.contextID, '');
  this.ime_.leaveCurrentMode();  // Leave from Conversion mode
  console.assert(this.ime_.getCurrentMode() instanceof skk.mode.Preedit,
                 'skk.mode.Conversion:commitCandidate: Internal error: ' +
                     'Current input mode must be skk.mode.Preedit');
  this.ime_.leaveCurrentMode();  // Leave from Preedit mode
};

/**
 * @inheritDoc
 */
skk.mode.Conversion.prototype.prepareForKey = function(keyEvent) {
  if ((keyEvent.key == 'g' && keyEvent.ctrlKey) ||
      skk.util.isBackspace(keyEvent.key) || skk.util.isEscape(keyEvent.key)) {
    // cancel
    this.ime_.leaveCurrentMode();
    return null;
  }

  if (skk.util.isSpace(keyEvent.key)) {
    this.selectedIndex_ += skk.NUM_CANDIDATES;
    // TODO(sekia): implement word registering
    this.selectedIndex_ %= this.candidates_.length;
    chrome.input.ime.setCursorPosition(
        {
          contextID: this.ime_.context.contextID,
          candidateID: this.selectedIndex_
        });
    return null;
  }

  if (keyEvent.key == 'x') {
    // previous page.
    this.selectedIndex_ -= skk.NUM_CANDIDATES;
    // TODO(sekia): implement word registering
    if (this.selectedIndex_ < 0) {
      // cancel to the previous mode.
      this.ime_.leaveCurrentMode();
      return null;
    }

    chrome.input.ime.setCursorPosition(
        {
          contextID: this.ime_.context.contextID,
          candidateID: this.selectedIndex_
        });
    return null;
  }

  // If the key is nothing special, it just commits the candidate and
  // handle as usual.  SKK does not have focus on the candidate window
  // and always commit the first one.
  var index = this.selectedIndex_ - (this.selectedIndex_ % skk.NUM_CANDIDATES);
  var isIndexKey = false;

  var indexMap = { a: 0, s: 1, d: 2, f: 3, j: 4, k: 5, l: 6 };
  if (keyEvent.key in indexMap) {
    // Do not handle the key as usual if the key is selecting a candidate.
    index += indexMap[keyEvent.key];
    isIndexKey = true;
  }

  this.commitCandidate(index);
  if (isIndexKey) {
    return null;
  } else {
    return keyEvent;
  }
};

/**
 * @inheritDoc
 */
skk.mode.Conversion.prototype.addKey = function(keyEvent) {
  console.error('skk.mode.Conversion.addKey: Must not reach here');
  return false;
};

/**
 * Displays candidate window.
 */
skk.mode.Conversion.prototype.showCandidateWindow = function() {
  chrome.input.ime.setCandidateWindowProperties(
      {
        engineID: this.ime_.engineID,
        properties: { visible: true }
      });
};

/**
 * Hides candidate window.
 */
skk.mode.Conversion.prototype.hideCandidateWindow = function() {
  chrome.input.ime.setCandidateWindowProperties(
      {
        engineID: this.ime_.engineID,
        properties: { visible: false }
      });
};
