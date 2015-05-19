// Copyright 2010-2013, Google Inc.
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

package org.mozc.android.inputmethod.japanese.session;

import android.content.Context;

/**
 * This class represents an interface to access libjnitestingbackdoor.so,
 * which has test methods.
 *
 */
class JNITestingBackdoor {
  /**
   * Initialize the shared object.
   *
   * This method is idempotent so calling this repeatedly is safe.
   *
   * @param testerContext a Context of tester's. It's not testee's because the .so is installed
   *        as shared object of tester project's artifact
   */
  static void initialize(Context testerContext) {
    System.load("/data/data/" + testerContext.getApplicationInfo().packageName
                + "/lib/libjnitestingbackdoor.so");
  }

  /**
   * Execute http request.
   *
   * @param method GET, HEAD or POST in UTF-8
   * @param url URL in UTF-8
   * @param content the content for POST method
   * @return the result of the request. Null if something goes wrong.
   */
  static synchronized native byte[] httpRequest(
      byte[] method, byte[] url, byte[] content);
}
