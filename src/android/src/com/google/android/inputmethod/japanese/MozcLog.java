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

package org.mozc.android.inputmethod.japanese;

import android.util.Log;

/**
 * Logger. Same as android.util.Log.
 *
 * Note that each methods decides whether it actually writes a log or not based on the result of
 * {@link Log#isLoggable(String, int)} so you can omit following idiom.
 * {@code
 * if (MozcLog.isLoggable(Log.INFO)) {
 *   MozcLog.i("foobar");
 * }
 * }
 * But for such a log entry which is very frequently created and is very heavy to create,
 * you can use (and are recommended to use) the above idiom.
 *
 * As described in Log's JavaDoc, the default threshold is INFO.
 * You can change the threshold by following command (on host side).
 * {@code adb shell setprop log.tag.Mozc VERVOSE}
 *
 * Q : "setprop" is tedious. Why do we have to depend on this mechanism?
 * A : By this way we can get detailed log even if we run release version.
 *     This is important when we are measuring the performance.
 *
 * Log level criteria:
 * VERVOSE - This level can be very heavy.
 * DEBUG - Detailed information
 *         but cannot be very heavy because this will be used to measure performance.
 * INFO - Usually the log will be show.
 * WARNING - For non-fatal error.
 * ERROR - For fatal (will exit abnormally) error.
 *
 */
public class MozcLog {
  private MozcLog() {
  }

  public static boolean isLoggable(int logLevel) {
    return Log.isLoggable(MozcUtil.LOGTAG, logLevel);
  }

  public static void v(String msg) {
    if (isLoggable(Log.VERBOSE)) {
      Log.v(MozcUtil.LOGTAG, msg);
    }
  }

  public static void v(String msg, Throwable e) {
    if (isLoggable(Log.VERBOSE)) {
      Log.v(MozcUtil.LOGTAG, msg, e);
    }
  }

  public static void d(String msg) {
    if (isLoggable(Log.DEBUG)) {
      Log.d(MozcUtil.LOGTAG, msg);
    }
  }

  public static void d(String msg, Throwable e) {
    if (isLoggable(Log.DEBUG)) {
      Log.d(MozcUtil.LOGTAG, msg, e);
    }
  }

  public static void i(String msg) {
    if (isLoggable(Log.INFO)) {
      Log.i(MozcUtil.LOGTAG, msg);
    }
  }

  public static void i(String msg, Throwable e) {
    if (isLoggable(Log.INFO)) {
      Log.i(MozcUtil.LOGTAG, msg, e);
    }
  }

  public static void w(String msg) {
    if (isLoggable(Log.WARN)) {
      Log.w(MozcUtil.LOGTAG, msg);
    }
  }

  public static void w(String msg, Throwable e) {
    if (isLoggable(Log.WARN)) {
      Log.w(MozcUtil.LOGTAG, msg, e);
    }
  }

  public static void e(String msg) {
    if (isLoggable(Log.ERROR)) {
      Log.e(MozcUtil.LOGTAG, msg);
    }
  }

  public static void e(String msg, Throwable e) {
    if (isLoggable(Log.ERROR)) {
      Log.e(MozcUtil.LOGTAG, msg, e);
    }
  }
}
