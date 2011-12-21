// Copyright 2010-2011, Google Inc.
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
 * @fileoverview This file contains skk.History class implementation.
 */

'use strict';

console.assert(skk, 'ime.js must be loaded prior to this module');

/**
 * ASCII record separator character.
 * @const
 */
skk.RECORD_SEPARATOR = String.fromCharCode('0x1E');

/**
 * Max number of history entries per word.
 * @const
 */
skk.MAX_HISTORY_PER_WORD = 8;

/**
 * Max number of kinds of words.
 * @const
 */
skk.MAX_WORDS = 5000;

/**
 * skk.History records recently selected conversion candidates.
 * @constructor
 */
skk.History = function() {
  /**
   * Dictionary holding recently used conversion candidates per word.
   * @private
   * @type Object
   */
  this.histories_ = {};
  /**
   * Holds registered words in LRU order. When the size of the array exceeds
   * {@code skk.MAX_WORDS}, History having the key equivalent to the head of
   * this array is removed.
   * @private
   * @type Array.<string>
   */
  this.registeredKeys_ = [];
};

/**
 * @param {string} base No conjugation part of word. 1+ length Hiragana string.
 * @param {string} stem Stem of conjugation. 0+ length latin string.
 * @return {string} Key string for lookup.
 * @private
 */
skk.History.prototype.getKeyFor_ = function(base, stem) {
  return base + skk.RECORD_SEPARATOR + stem;
};

/**
 * Serializes conversion history as a JSON string.
 * @return {string} JSON string which is able to be restored as object status by
 *     {@code deserialize}.
 * @see skk.History#deserialize
 */
skk.History.prototype.serialize = function() {
  return JSON.stringify({ histories: this.histories_,
                          registeredWords: this.registeredWords_ });
};

/**
 * Restores object status from serialized string.
 * @param {string} serialized JSON string returned from {@code deserialize}.
 * @see skk.History#serialize
 */
skk.History.prototype.deserialize = function(serialized) {
  var parsed = JSON.parse(serialized);
  this.histories_ = parsed.histories;
  this.registeredWords_ = parsed.registeredWords;
};

/**
 * Looks up recently used conversion candidates.
 * @param {string} base No conjugation part of word. 1+ length Hiragana string.
 * @param {string} stem Stem of conjugation. 0+ length latin string.
 * @return {Array.<string>} Conversion candidates. Ordered in LIFO.
 * @see skk.History#addCandidate
 */
skk.History.prototype.lookup = function(base, stem) {
  var candidates = this.histories_[this.getKeyFor_(base, stem)];
  if (!candidates) { return []; }
  return candidates.slice(0);  // Returns a copy
};

/**
 * Adds a candidate. If the candidate is already in the history, it is moved
 * to the top of candidate list.
 * @param {string} base No conjugation part of word. 1+ length Hiragana string.
 * @param {string} stem Stem of conjugation. 0+ length latin string.
 * @param {string} converted Converted string to be added to history.
 */
skk.History.prototype.addCandidate = function(base, stem, converted) {
  var key = this.getKeyFor_(base, stem);
  var candidates = this.histories_[key];
  if (!candidates) {
    candidates = [];
    this.histories_[key] = candidates;
    this.registeredKeys_.push(key);
  } else {
    for (var i = 0; i < this.registeredKeys_.length; ++i) {
      if (this.registeredKeys_[i] === key) {
        this.registeredKeys_.splice(i, 1);
        this.registeredKeys_.push(key);
        break;
      }
    }
  }
  for (var i = 0; i < candidates.length; ++i) {
    if (candidates[i] == converted) {
      candidates.splice(i, 1);
      break;
    }
  }
  candidates.unshift(converted);
  while (candidates.length > skk.MAX_HISTORY_PER_WORD) { candidates.pop(); }
  while (this.registeredKeys_.length > skk.MAX_WORDS) {
    var oldKey = this.registeredKeys_.shift();
    delete this.histories_[oldKey];
  }
};
