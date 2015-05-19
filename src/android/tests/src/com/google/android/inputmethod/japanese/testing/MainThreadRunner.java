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

package org.mozc.android.inputmethod.japanese.testing;

import org.mozc.android.inputmethod.japanese.JapaneseKeyboard;
import org.mozc.android.inputmethod.japanese.JapaneseKeyboardView;

import android.app.Activity;
import android.app.Instrumentation;
import android.view.MotionEvent;
import android.view.View;

/**
 * Utility class to run some methods on the UI thread.
 * We should consider to use {@link android.test.UiThreadTest} annotation instead.
 *
 * TODO(hidehiko): Replace MainThreadRunner's methods by UiThreadTest, and deprecate this class.
 *
 */
public class MainThreadRunner {
  private static class OnTouchEventRunner implements Runnable {
    private final View view;
    private final MotionEvent event;

    // A field to keep the result of onTouchEvent.
    volatile boolean result;
    volatile Throwable throwable = null;

    OnTouchEventRunner(View view, MotionEvent event) {
      this.view = view;
      this.event = event;
    }

    @Override
    public void run() {
      try {
        result = view.onTouchEvent(event);
      } catch (Throwable e) {
        // Catch a throwable here (Exception or Error is insufficient).
        // If not, this thread (UI-thread. Non-test main thread) stops and
        // the test process itself also stops.
        result = false;
        throwable = e;
      }
    }
  }

  private final Instrumentation instrumentation;

  public MainThreadRunner(Instrumentation instrumentation) {
    if (instrumentation == null) {
      throw new NullPointerException("instrumentation is null.");
    }
    this.instrumentation = instrumentation;
  }

  public void runOnMainSync(Runnable runner) {
    instrumentation.runOnMainSync(runner);
  }

  public void setContentView(final Activity activity, final View view) {
    runOnMainSync(new Runnable() {
      @Override
      public void run() {
        activity.setContentView(view);
      }
    });
  }

  public void layout(final View view,
                     final int left, final int top, final int right, final int bottom) {
    runOnMainSync(new Runnable() {
      @Override
      public void run() {
        view.layout(left, top, right, bottom);
      }
    });
  }

  public boolean onTouchEvent(View view, MotionEvent event) {
    OnTouchEventRunner runner = new OnTouchEventRunner(view, event);
    runOnMainSync(runner);
    if (runner.throwable != null) {
      throw new AssertionError(runner.throwable);
    }
    return runner.result;
  }

  public void setJapaneseKeyboard(final JapaneseKeyboardView view,
                                  final JapaneseKeyboard keyboard) {
    runOnMainSync(new Runnable() {
      @Override
      public void run() {
        view.setJapaneseKeyboard(keyboard);
      }
    });
  }
}
