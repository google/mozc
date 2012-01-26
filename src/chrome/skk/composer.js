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
 * @fileoverview This file contains skk.Composer class implementation.
 */

'use strict';

console.assert(skk, 'ime.js must be loaded prior to this module');

/**
 * skk.Composer accumlates alphabet sequence and converts them to hiragana,
 * katakana or other characters.
 * @constructor
 * @param {Object} kanaMap ASCII-(Hiragana|Katakana|...) mapping.
 */
skk.Composer = function(kanaMap) {
  this.undetermined = '';
  this.kanaMap_ = kanaMap;
};

/**
 * Holds Roman-OtherCharacter mapping objects.
 */
skk.Composer.map = {};

/**
 * Roman-Hiragana mapping.
 */
skk.Composer.map.hiragana = {
  a: '\u3042',
  i: '\u3044',
  u: '\u3046',
  e: '\u3048',
  o: '\u304a',
  ka: '\u304b',
  ki: '\u304d',
  ku: '\u304f',
  ke: '\u3051',
  ko: '\u3053',
  sa: '\u3055',
  si: '\u3057',
  su: '\u3059',
  se: '\u305b',
  so: '\u305d',
  ta: '\u305f',
  ti: '\u3061',
  tu: '\u3064',
  tsu: '\u3064',
  te: '\u3066',
  to: '\u3068',
  na: '\u306a',
  ni: '\u306b',
  nu: '\u306c',
  ne: '\u306d',
  no: '\u306e',
  ha: '\u306f',
  hi: '\u3072',
  hu: '\u3075',
  he: '\u3078',
  ho: '\u307b',
  ma: '\u307e',
  mi: '\u307f',
  mu: '\u3080',
  me: '\u3081',
  mo: '\u3082',
  ya: '\u3084',
  yu: '\u3086',
  ye: '\u3044\u3047',
  yo: '\u3088',
  ra: '\u3089',
  ri: '\u308a',
  ru: '\u308b',
  re: '\u308c',
  ro: '\u308d',
  wa: '\u308f',
  wi: '\u3046\u3043',
  we: '\u3046\u3047',
  wo: '\u3092',
  ga: '\u304c',
  gi: '\u304e',
  gu: '\u3050',
  ge: '\u3052',
  go: '\u3054',
  za: '\u3056',
  zi: '\u3058',
  zu: '\u305a',
  ze: '\u305c',
  zo: '\u305e',
  da: '\u3060',
  di: '\u3062',
  du: '\u3065',
  de: '\u3067',
  'do': '\u3069',  // 'do' is a reserved word
  ba: '\u3070',
  bi: '\u3073',
  bu: '\u3076',
  be: '\u3079',
  bo: '\u307c',
  pa: '\u3071',
  pi: '\u3074',
  pu: '\u3077',
  pe: '\u307a',
  po: '\u307d',
  ja: '\u3058\u3083',
  ji: '\u3058',
  ju: '\u3058\u3085',
  je: '\u3058\u3047',
  jo: '\u3058\u3087',
  fa: '\u3075\u3041',
  fi: '\u3075\u3043',
  fu: '\u3075',
  fe: '\u3075\u3047',
  fo: '\u3075\u3049',
  va: '\u3094\u3041',
  vi: '\u3094\u3043',
  vu: '\u3094',
  ve: '\u3094\u3047',
  vo: '\u3094\u3049',
  sha: '\u3057\u3083',
  shi: '\u3057',
  shu: '\u3057\u3085',
  she: '\u3057\u3047',
  sho: '\u3057\u3087',
  cha: '\u3061\u3083',
  chi: '\u3061',
  chu: '\u3061\u3085',
  che: '\u3061\u3047',
  cho: '\u3061\u3085',
  tha: '\u3066\u3041',
  thi: '\u3066\u3043',
  thu: '\u3066\u3085',
  the: '\u3066\u3047',
  tho: '\u3066\u3087',
  kya: '\u304d\u3083',
  kyi: '\u304d\u3043',
  kyu: '\u304d\u3085',
  kye: '\u304d\u3047',
  kyo: '\u304d\u3087',
  gya: '\u304e\u3083',
  gyi: '\u304e\u3043',
  gyu: '\u304e\u3085',
  gye: '\u304e\u3047',
  gyo: '\u304e\u3087',
  sya: '\u3057\u3083',
  syi: '\u3057\u3043',
  syu: '\u3057\u3085',
  sye: '\u3057\u3047',
  syo: '\u3057\u3087',
  jya: '\u3058\u3083',
  jyi: '\u3058\u3043',
  jyu: '\u3058\u3085',
  jye: '\u3058\u3047',
  jyo: '\u3058\u3087',
  zya: '\u3058\u3083',
  zyi: '\u3058\u3043',
  zyu: '\u3058\u3085',
  zye: '\u3058\u3047',
  zyo: '\u3058\u3087',
  tya: '\u3061\u3083',
  tyi: '\u3061\u3043',
  tyu: '\u3061\u3085',
  tye: '\u3061\u3047',
  tyo: '\u3061\u3087',
  nya: '\u306b\u3083',
  nyi: '\u306b\u3043',
  nyu: '\u306b\u3085',
  nye: '\u306b\u3047',
  nyo: '\u306b\u3087',
  hya: '\u3072\u3083',
  hyi: '\u3072\u3043',
  hyu: '\u3072\u3085',
  hye: '\u3072\u3047',
  hyo: '\u3072\u3087',
  mya: '\u307f\u3083',
  myi: '\u307f\u3043',
  myu: '\u307f\u3085',
  mye: '\u307f\u3047',
  myo: '\u307f\u3087',
  rya: '\u308a\u3083',
  ryi: '\u308a\u3043',
  ryu: '\u308a\u3085',
  rye: '\u308a\u3047',
  ryo: '\u308a\u3087',
  xa: '\u3041',
  xi: '\u3043',
  xu: '\u3045',
  xe: '\u3047',
  xo: '\u3049',
  xya: '\u3083',
  xyu: '\u3085',
  xyo: '\u3087',
  xwa: '\u308e',
  xwi: '\u3090',
  xwe: '\u3091',
  xtu: '\u3063',
  nn: '\u3093',
  "n'": '\u3093',
  '-': '\u30fc',
  ',': '\u3001',
  '.': '\u3002',
  ' ': ' ',
  "'": "'",
  '"': '"',
  '`': '`',
  ';': ';',
  ':': ':',
  '+': '+',
  '*': '*',
  '/': '/',
  '\\': '\\',
  '=': '=',
  '^': '^',
  '|': '|',
  '!': '!',
  '?': '?',
  '#': '#',
  '@': '@',
  '$': '$',
  '%': '%',
  '&': '&',
  '(': '(',
  ')': ')',
  '{': '{',
  '}': '}',
  '[': '\u300c',
  ']': '\u300d',
  '<': '<',
  '>': '>',
  'zh': '\u2190',
  'zj': '\u2193',
  'zk': '\u2191',
  'zl': '\u2192',
  'z ': '\u3000',
  'z-': '\u301c',
  'z,': '\u2025',
  'z.': '\u2026',
  'z[': '\u300e',
  'z]': '\u300f'
};

/**
 * Roman-Katakana mapping.
 */
skk.Composer.map.katakana = {
  a: '\u30a2',
  i: '\u30a4',
  u: '\u30a6',
  e: '\u30a8',
  o: '\u30aa',
  ka: '\u30ab',
  ki: '\u30ad',
  ku: '\u30af',
  ke: '\u30b1',
  ko: '\u30b3',
  sa: '\u30b5',
  si: '\u30b7',
  su: '\u30b9',
  se: '\u30bb',
  so: '\u30bd',
  ta: '\u30bf',
  ti: '\u30c1',
  tu: '\u30c4',
  tsu: '\u30c4',
  te: '\u30c6',
  to: '\u30c8',
  na: '\u30ca',
  ni: '\u30cb',
  nu: '\u30cc',
  ne: '\u30cd',
  no: '\u30ce',
  ha: '\u30cf',
  hi: '\u30d2',
  hu: '\u30d5',
  he: '\u30d8',
  ho: '\u30db',
  ma: '\u30de',
  mi: '\u30df',
  mu: '\u30e0',
  me: '\u30e1',
  mo: '\u30e2',
  ya: '\u30e4',
  yu: '\u30e6',
  ye: '\u30a4\u30a7',
  yo: '\u30e8',
  ra: '\u30e9',
  ri: '\u30ea',
  ru: '\u30eb',
  re: '\u30ec',
  ro: '\u30ed',
  wa: '\u30ef',
  wi: '\u30a6\u30a3',
  we: '\u30a6\u30a7',
  wo: '\u30f2',
  ga: '\u30ac',
  gi: '\u30ae',
  gu: '\u30b0',
  ge: '\u30b2',
  go: '\u30b4',
  za: '\u30b6',
  zi: '\u30b8',
  zu: '\u30ba',
  ze: '\u30bc',
  zo: '\u30be',
  da: '\u30c0',
  di: '\u30c2',
  du: '\u30c5',
  de: '\u30c7',
  'do': '\u30c9',  // 'do' is a reserved word
  ba: '\u30d0',
  bi: '\u30d3',
  bu: '\u30d6',
  be: '\u30d9',
  bo: '\u30dc',
  pa: '\u30d1',
  pi: '\u30d4',
  pu: '\u30d7',
  pe: '\u30da',
  po: '\u30dd',
  ja: '\u30b8\u30e3',
  ji: '\u30b8',
  ju: '\u30b8\u30e5',
  je: '\u30b8\u30a7',
  jo: '\u30b8\u30e7',
  fa: '\u30d5\u30a1',
  fi: '\u30d5\u30a3',
  fu: '\u30d5',
  fe: '\u30d5\u30a7',
  fo: '\u30d5\u30a9',
  va: '\u30f4\u30a1',
  vi: '\u30f4\u30a3',
  vu: '\u30f4',
  ve: '\u30f4\u30a7',
  vo: '\u30f4\u30a9',
  sha: '\u30b7\u30e3',
  shi: '\u30b7',
  shu: '\u30b7\u30e5',
  she: '\u30b7\u30a7',
  sho: '\u30b7\u30e7',
  cha: '\u30c1\u30e3',
  chi: '\u30c1',
  chu: '\u30c1\u30e5',
  che: '\u30c1\u30a7',
  cho: '\u30c1\u30e5',
  tha: '\u30c6\u30a1',
  thi: '\u30c6\u30a3',
  thu: '\u30c6\u30e5',
  the: '\u30c6\u30a7',
  tho: '\u30c6\u30e7',
  kya: '\u30ad\u30e3',
  kyi: '\u30ad\u30a3',
  kyu: '\u30ad\u30e5',
  kye: '\u30ad\u30a7',
  kyo: '\u30ad\u30e7',
  gya: '\u30ae\u30e3',
  gyi: '\u30ae\u30a3',
  gyu: '\u30ae\u30e5',
  gye: '\u30ae\u30a7',
  gyo: '\u30ae\u30e7',
  sya: '\u30b7\u30e3',
  syi: '\u30b7\u30a3',
  syu: '\u30b7\u30e5',
  sye: '\u30b7\u30a7',
  syo: '\u30b7\u30e7',
  jya: '\u30b8\u30e3',
  jyi: '\u30b8\u30a3',
  jyu: '\u30b8\u30e5',
  jye: '\u30b8\u30a7',
  jyo: '\u30b8\u30e7',
  zya: '\u30b8\u30e3',
  zyi: '\u30b8\u30a3',
  zyu: '\u30b8\u30e5',
  zye: '\u30b8\u30a7',
  zyo: '\u30b8\u30e7',
  tya: '\u30c1\u30e3',
  tyi: '\u30c1\u30a3',
  tyu: '\u30c1\u30e5',
  tye: '\u30c1\u30a7',
  tyo: '\u30c1\u30e7',
  nya: '\u30cb\u30e3',
  nyi: '\u30cb\u30a3',
  nyu: '\u30cb\u30e5',
  nye: '\u30cb\u30a7',
  nyo: '\u30cb\u30e7',
  hya: '\u30d2\u30e3',
  hyi: '\u30d2\u30a3',
  hyu: '\u30d2\u30e5',
  hye: '\u30d2\u30a7',
  hyo: '\u30d2\u30e7',
  mya: '\u30df\u30e3',
  myi: '\u30df\u30a3',
  myu: '\u30df\u30e5',
  mye: '\u30df\u30a7',
  myo: '\u30df\u30e7',
  rya: '\u30ea\u30e3',
  ryi: '\u30ea\u30a3',
  ryu: '\u30ea\u30e5',
  rye: '\u30ea\u30a7',
  ryo: '\u30ea\u30e7',
  xa: '\u30a1',
  xi: '\u30a3',
  xu: '\u30a5',
  xe: '\u30a7',
  xo: '\u30a9',
  xya: '\u30e3',
  xyu: '\u30e5',
  xyo: '\u30e7',
  xwa: '\u30ee',
  xwi: '\u30f0',
  xwe: '\u30f1',
  xtu: '\u30c3',
  nn: '\u30f3',
  "n'": '\u30f3',
  '-': '\u30fc',
  ',': '\u3001',
  '.': '\u3002',
  ' ': ' ',
  "'": "'",
  '"': '"',
  '`': '`',
  ';': ';',
  ':': ':',
  '+': '+',
  '*': '*',
  '/': '/',
  '\\': '\\',
  '=': '=',
  '^': '^',
  '|': '|',
  '!': '!',
  '?': '?',
  '#': '#',
  '@': '@',
  '$': '$',
  '%': '%',
  '&': '&',
  '(': '(',
  ')': ')',
  '{': '{',
  '}': '}',
  '[': '300c',
  ']': '300d',
  '<': '<',
  '>': '>',
  'zh': '\u2190',
  'zj': '\u2193',
  'zk': '\u2191',
  'zl': '\u2192',
  'z ': '\u3000',
  'z-': '\u301c',
  'z,': '\u2025',
  'z.': '\u2026',
  'z[': '\u300e',
  'z]': '\u300f'
};

/**
 * @param {string} c An input character.
 * @return {boolean} Whether the character is a japanese vowel.
 */
skk.Composer.prototype.isVowel = function(c) {
  return !!(c.match(/^[aeiou]$/));
};

/**
 * @param {string} c An input character.
 * @return {boolean} Whether the character is an english alphabet.
 */
skk.Composer.prototype.isAlpha = function(c) {
  return !!(c.match(/^[a-zA-Z]$/));
};

/**
 * @return {string} Last character of undetermined string.
 */
skk.Composer.prototype.lastUndeterminedChar = function() {
  if (this.undetermined.length == 0) { return ''; }
  return this.undetermined.substr(-1);
};

/**
 * Resets current composer status.
 */
skk.Composer.prototype.reset = function() {
  this.undetermined = '';
};

/**
 * Adds a character to undetermined string.
 * When there is a mapping entry correspond to the string, emmits it as
 * determined characters and bites off it from the string.
 *
 * Example:
 * # Consider comp.undetermined == 't'
 * determAndUndeterm = comp.addKey('t'); # ['っ', 't']
 * # Now comp.undetermined == 't'
 * determAndUndeterm = comp.addKey('a'); # ['た', '']
 * # Now comp.undetermined == ''
 *
 * @param {string} c An input character.
 * @return {Array.<string, string>} A pair of determined character and
 *     undetermined string.
 */
skk.Composer.prototype.addKey = function(c) {
  var concat = this.undetermined + c;
  if (concat in this.kanaMap_) {
    console.debug('composer.js: addKey: ',
                  concat, '->', this.kanaMap_[concat]);
    this.undetermined = '';
    return [this.kanaMap_[concat], this.undetermined];
  }

  // e.g. 'tt' -> ['っ', 't']
  if (this.lastUndeterminedChar() === c) {
    this.undetermined = c;
    return [this.kanaMap_['xtu'], c];
  }

  // e.g. 'nk' -> ['ん', 'k']
  if (this.lastUndeterminedChar() === 'n') {
    this.undetermined = c;
    return [this.kanaMap_['nn'], c];
  }

  var keys = Object.keys(this.kanaMap_);
  var noCand = Array.all(keys,
                         function(key) { return key.search(concat) !== 0; });
  while (noCand) {
    console.assert(concat !== '');
    concat = concat.substr(1);
    noCand = Array.all(keys,
                       function(key) { return key.search(concat) !== 0; });
  }
  if (!!this.kanaMap_[concat]) {
    return [this.kanaMap_[concat], ''];
  } else {
    this.undetermined = concat;
    return ['', this.undetermined];
  }
};

/**
 * @return {boolean} Whether composer has undetermined string or not.
 */
skk.Composer.prototype.hasUndetermined = function() {
  return this.undetermined !== '';
};

/**
 * Chops tailing character of undetermined string.
 * If composer didn't have undetermined string, returns null.
 * @return {?string} Chopped character.
 */
skk.Composer.prototype.backspace = function() {
  if (!this.hasUndetermined()) { return null; }
  var chopped = this.undetermined.substr(-1);
  this.undetermined = this.undetermined.slice(0, -1);
  return chopped;
};
