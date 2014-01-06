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
 * @fileoverview This file contains NaclMozc initializer.
 *
 */

'use strict';

/**
 * Registers NaCl module handler.
 */
document.addEventListener('readystatechange', function() {
  if (document.readyState == 'complete') {
    if (!chrome || !chrome.input || !chrome.input.ime) {
      console.error('chrome.input.ime APIs are not available');
      return;
    }
    var naclModule = document.createElement('embed');
    naclModule.id = 'nacl_mozc_object';
    naclModule.src = 'nacl_session_handler.nmf';
    naclModule.type = 'application/x-nacl';
    naclModule.className = 'naclModule';
    naclModule.width = 0;
    naclModule.height = 0;
    var naclMozc =
        new mozc.NaclMozc(/** @type {!HTMLEmbedElement} */ (naclModule));
    // Makes newOptionPage method accessable using
    // chrome.extension.getBackgroundPage()['newOptionPage'].
    window['newOptionPage'] = naclMozc.newOptionPage.bind(naclMozc);
    var body = document.getElementsByTagName('body').item(0);
    body.appendChild(naclModule);
  }
}, true);
