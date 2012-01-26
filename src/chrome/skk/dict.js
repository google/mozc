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
 * @fileoverview This file contains skk.Dict class implementation.
 */

'use strict';

console.assert(skk, 'ime.js must be loaded prior to this module');

/**
 * skk.Dict provides features to communicate with dictionary running as
 * Native Client module. Both Request/Response message are JSON formatted
 * string.
 * @constructor
 * @param {Element} nacl DOM Element embedding NaCl module.
 */
skk.Dict = function(nacl) {
  console.assert(nacl, 'Dict: NativeClient module required');
  this.nacl_ = nacl;
  this.nacl_.addEventListener('message', this.handleMessage.bind(this));
  this.id_ = 0;
  this.history_ = new skk.History();
  this.callbacks_ = {};
  console.log('Dict: Dictionary loaded', this);
};

/***
 * Max message id number.
 */
skk.Dict.kMaxId = 65536;

/**
 * Enum that holds message type descriptor.
 */
skk.Dict.RequestMethod = {
  LOOKUP: 'LOOKUP'
};

/**
 * Enum that holds response status codes.
 */
skk.Dict.ResponseStatus = {
  OK: 'OK',
  ERROR: 'ERROR'
};

/**
 * Generates a unique identifier.
 * @return {number} Identifier.
 */
skk.Dict.prototype.getUniqueId = function() {
  function nextId(self) {
    return self.id_ = ++self.id_ % skk.Dict.kMaxId;
  }
  var idCand = nextId(this);
  while (idCand in this.callbacks_) { idCand = nextId(this); }
  return idCand;
};

/**
 * Looks up dictionary entry.
 * @param {string} base No conjugation part of word. 1+ length hiragana string.
 * @param {string} stem Stem of conjugation. 0+ length latin string.
 * @param {function(boolean, Array.<string>, Array.<string>)} callback
 *     Callback function that takes 3 arguments:
 *     (is_error, array_of_candidates, array_of_predictions)
 *     It will be called back when response message returned from NaCl module.
 *
 * Example:
 * # Looks up conversion candidates for 'ことのは(kotonoha; words)':
 * dict.lookup(
 *   '\u3053\u3068\u306e\u306f',  # 'ことのは'
 *   '',                          # Nouns don't conjugate
 *   function(err, cands, preds) {
 *     if (err) { return; }
 *     cands.forEach(function (cand) { # Log all candidates
 *       console.log('Conversion candidate: ', cand); });
 *   });
 * # ...and for 'はしる(hashiru; running)':
 * dict.lookup(
 *   '\u306f\u3057', # 'はし'
 *   'r',
 *   function(err, cands, preds) { ... });
 */
skk.Dict.prototype.lookup = function(base, stem, callback) {
  var message = {
    id: this.getUniqueId(),
    method: skk.Dict.RequestMethod.LOOKUP,
    base: base,
    stem: stem
  };
  var self = this;
  this.callbacks_[message.id] = (
      function(callback) {
        return function(id, status, body) {
          // Unpacks response message and pass it to user registered callback.
          var isError = status === skk.Dict.ResponseStatus.ERROR;
          var candidates = isError ? [] : body.candidates;
          var predictions = isError ? [] : body.predictions;
          var recentlyUsed = self.history_.lookup(base, stem);
          candidates = Array.uniq(recentlyUsed.concat(candidates));
          callback(isError, candidates, predictions);
        };
      })(callback);
  var json = JSON.stringify(message);
  console.debug('Dict.lookup: Post message: ', json);
  this.nacl_.postMessage(json);
};

/**
 * Adds a conversion result to history.
 * @param {string} base No conjugation part of word. 1+ length Hiragana string.
 * @param {string} stem  Stem of conjugation. 0+ length latin string.
 * @param {string} converted Conversion result string.
 */
skk.Dict.prototype.addHistory = function(base, stem, converted) {
  this.history_.addCandidate(base, stem, converted);
};

/**
 * Serializes conversion history as a JSON string.
 * @return {string} JSON string which is able to be restored as object status by
 *     {@code deserialize}.
 * @see skk.History#serialize
 */
skk.Dict.prototype.serializeHistory = function() {
  return this.history_.serialize();
};

/**
 * Restores object status from serialized string.
 * @param {string} serialized JSON string returned from {@code deserialize}.
 * @see skk.History#deserialize
 */
skk.Dict.prototype.deserializeHistory = function(serialized) {
  this.history_.deserialize(serialized);
};

/**
 * Event handler.
 * Receives a message from NaCl module and dispatches it to registered callback.
 * @param {Object} event NaCl event object.
 */
skk.Dict.prototype.handleMessage = function(event) {
  console.debug('Dict.handleMessage: Received message: ', event.data);

  var res = {};
  try {
    res = JSON.parse(event.data);
  } catch (e) {
    console.error('Dict.handleMessage: Error: ' + e.message);
  }

  if (res.id) {
    var callback = this.callbacks_[res.id];
    delete this.callbacks_[res.id];
    callback(res.id, res.status, res.body);
  }
};
