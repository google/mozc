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
 * @fileoverview This file provides some utility functions.
 * It also adds some static methods to JavaScript core object.
 */
'use strict';

console.assert(skk, 'ime.js must be loaded prior to this module');

if (!Object.inject) {
  /**
   * Merges {@code props} into {@code obj}.
   * If {@code obj} already had a same named property, it will be overwritten.
   * @param {Object} obj Object to be injected.
   * @param {Object} props Properties to inject.
   */
  Object.inject = function(obj, props) {
    for (var key in props) { obj[key] = props[key]; }
  };
}

if (!Object.keys) {
  /**
   * Returns all property names of {@code obj} as an array.
   * @param {Object} obj Object to be inspected.
   * @return {Array} Keys of obj.
   */
  Object.keys = function(obj) {
    var keys = [];
    for (var key in obj) { keys.push(key); }
    return keys;
  };
}

if (!Array.find) {
  /**
   * Returns first element in {@code ary} that satisfies predicate function.
   * @param {Array} ary Array to be checked.
   * @param {function(*):boolean} predicate Predicate function.
   *     Takes 1 argument and returns a boolean.
   * @return {*} First element satisfies predicate in {@code ary}, or null
   *     if it's not found.
   */
  Array.find = function(ary, predicate) {
    for (var i = 0; i < ary.length; i++) {
      if (predicate(ary[i])) { return ary[i]; }
    }
    return null;
  };
}

if (!Array.all) {
  /**
   * @param {Array} ary Array to be checked.
   * @param {function(*):boolean} predicate Predicate function.
   *     Takes 1 argument and returns a boolean.
   * @return {boolean} Returns true if all the element in {@code ary} satisfies
   *     {@code predicate}.
   */
  Array.all = function(ary, predicate) {
    for (var i = 0; i < ary.length; i++) {
      if (!predicate(ary[i])) { return false; }
    }
    return true;
  };
}

if (!Array.uniq) {
  /**
   * Removes duplicate elements from given array.
   * Note that this function only checks equality of elements by their string
   * expression (i.e. return value of {@code toString}).
   * @param {Array} ary Array.
   * @return {Array} Array that has no duplicate elements.
   */
  Array.uniq = function(ary) {
    var seenElements = {};
    var result = [];
    for (var i = 0; i < ary.length; i++) {
      var element = ary[i];
      if (seenElements[element.toString()]) { continue; }
      result.push(element);
      seenElements[element.toString()] = true;
    }
    return result;
  };
}

/**
 * Namespace for utility functions.
 */
skk.util = {
  /**
   * @param {string} key Value of {@code KeyboardEvent.key}.
   * @return {bloolean} Return true if {@code key} is a printable character,
   *     otherwise false.
   */
  isPrintable: function(key) {
    return !!key.match(
        /^[ \w,.\"\'`;:+\-*\/\\=^|!?#@$%&(){}\[\]<>]$/);
  },

  /**
   * @param {string} key Value of {@code KeyboardEvent.key}.
   * @return {boolean} Return true if {@code key} is an upper letter,
   *     otherwise false.
   */
  isUpperLetter: function(key) { return !!key.match(/^[A-Z]$/); },

  /**
   * @param {string} key Value of {@code KeyboardEvent.key}.
   * @return {boolean} Return true if {@code key} is a vowel character,
   *     otherwise false.
   */
  isVowel: function(key) { return !!key.match(/^[aiueo]$/); },

  /**
   * @param {string} key Value of {@code KeyboardEvent.key}.
   * @return {bloolean} Return true if {@code key} is white space,
   *     otherwise false.
   */
  isSpace: function(key) { return key === ' '; },

  /**
   * @param {string} key Value of {@code KeyboardEvent.key}.
   * @return {bloolean} Return true if {@code key} is enter, otherwise false.
   */
  isEnter: function(key) { return key === 'Enter'; },

  /**
   * @param {string} key Value of {@code KeyboardEvent.key}.
   * @return {bloolean} Return true if {@code key} is arrow , otherwise false.
   */
  isArrow: function(key) {
    return !!key.match(/^(?:Left|Up|Down|Right)$/);
  },

  /**
   * @param {string} key Value of {@code KeyboardEvent.key}.
   * @return {bloolean} Return true if {@code key} is backspace, otherwise
   *    false.
   */
  isBackspace: function(key) { return key === 'Backspace'; },

  /**
   * @param {string} key Value of {@code KeyboardEvent.key}.
   * @return {bloolean} Return true if {@code key} is escape, otherwise
   *    false.
   */
  isEscape: function(key) { return key === 'Esc'; },

  /**
   * Displays preedit text.
   * @param {number} contextId Context ID.
   * @param {string} preedit String to be shown.
   */
  showPreedit: function(contextId, preedit) {
    console.debug('skk.util.showPreedit');
    chrome.input.ime.setComposition(
        {
          contextID: contextId,
          text: preedit,
          cursor: preedit.length
        });
  }
};
