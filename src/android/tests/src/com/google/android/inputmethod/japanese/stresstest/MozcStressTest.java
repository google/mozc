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

package org.mozc.android.inputmethod.japanese.stresstest;

import org.mozc.android.inputmethod.japanese.JapaneseKeyboard.KeyboardSpecification;
import org.mozc.android.inputmethod.japanese.MozcService;
import org.mozc.android.inputmethod.japanese.MozcUtil;
import org.mozc.android.inputmethod.japanese.ViewManager;
import org.mozc.android.inputmethod.japanese.preference.ClientSidePreference.KeyboardLayout;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.TouchEvent;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Request;
import org.mozc.android.inputmethod.japanese.resources.R;
import org.mozc.android.inputmethod.japanese.session.SessionExecutor;
import org.mozc.android.inputmethod.japanese.testing.MemoryLogger;
import org.mozc.android.inputmethod.japanese.testing.VisibilityProxy;
import org.mozc.android.inputmethod.japanese.testing.mocking.MozcMockSupport;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.Context;
import android.graphics.Rect;
import android.test.InstrumentationTestCase;
import android.test.UiThreadTest;
import android.view.Display;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

import java.lang.reflect.InvocationTargetException;
import java.util.Collections;
import java.util.List;
import java.util.Random;

/**
 * This is Stress Test class which runs long time and check memory leaks.
 *
 */
public class MozcStressTest extends InstrumentationTestCase {
  class OnCreateRunner implements Runnable {
    @Override
    public void run(){
       service = new MozcService(){
         /*
          * In tests which is set up here, this method returns false. This blocks our
          * tests and we don't concern this method. So this is little hacky.
          * @see android.inputmethodservice.InputMethodService#isInputViewShown()
          */
        @Override
        public boolean isInputViewShown() { return true; }
      };
      try {
        VisibilityProxy.invokeByName(
            service, "attachBaseContext", activity.getApplicationContext());
      } catch (InvocationTargetException e) {
        fail(e.getCause().toString());
      } catch (Exception e) {
        fail(e.toString());
      }
      service.onCreate();
      inputView = service.onCreateInputView();
      activity.setContentView(inputView);
      InputConnection inputConnection = mockSupport.createNiceMock(InputConnection.class);
      mockSupport.replayAll();
      service.onCreateInputMethodInterface().restartInput(inputConnection, new EditorInfo());
      viewManager = ViewManager.class.cast(service.viewManager);
    }
  }

  private MozcMockSupport mockSupport;

  private Activity activity;
  private volatile MozcService service;
  private volatile View inputView;
  private volatile ViewManager viewManager;
  private Context context;

  @Override
  protected void setUp() throws Exception {
    Instrumentation instrumentation = getInstrumentation();
    mockSupport = new MozcMockSupport(instrumentation);

    activity = launchActivity(
        "org.mozc.android.inputmethod.japanese", Activity.class, null);
    instrumentation.runOnMainSync(new OnCreateRunner());
    context = instrumentation.getTargetContext();
  }

  @Override
  protected void tearDown() throws Exception {
    if (activity != null) {
      activity.finish();
    }
    service = null;
    inputView = null;
    viewManager = null;
    context = null;

    mockSupport = null;
  }

  static void tap(View view, float x, float y) {
    view.onTouchEvent(MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, x, y, 0));
    view.onTouchEvent(MotionEvent.obtain(0, 0, MotionEvent.ACTION_UP, x, y, 0));
  }

  private Rect getViewRect() {
    Context context = activity.getApplicationContext();
    WindowManager windowmanager = WindowManager.class.cast(
        context.getSystemService(Context.WINDOW_SERVICE));
    Display disp = windowmanager.getDefaultDisplay();
    return new Rect(0, 0, disp.getWidth(),
                    context.getResources().getDimensionPixelSize(R.dimen.ime_window_height));
  }

  private static void sleep(long milliseconds) {
    try {
      Thread.sleep(milliseconds);
    } catch (Exception e) {
      fail("error:" + e);
    }
  }

  @StressTest
  @UiThreadTest
  public void testRandomTouch() {
    MemoryLogger memoryLogger = new MemoryLogger("testRandomTouch", 100);

    viewManager.setKeyboardLayout(KeyboardLayout.TWELVE_KEYS);

    final int actions = 500;

    final Random random = new Random();
    Rect rect = getViewRect();

    memoryLogger.logMemory("start");
    for (int i = 0; i < actions; i++) {
      final int x = rect.left + random.nextInt(rect.width());
      final int y = rect.top + random.nextInt(rect.height());

      inputView.dispatchTouchEvent(MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, x, y, 0));
      inputView.dispatchTouchEvent(MotionEvent.obtain(0, 0, MotionEvent.ACTION_UP, x, y, 0));

      // updateRequest sends asynchronous message to server. So we need to wait a moment
      // for server processing.
      sleep(200);

      memoryLogger.logMemoryInterval();
    }
    memoryLogger.logMemory("start");
  }

  @StressTest
  @UiThreadTest
  public void testUpdateRequest() {
    MemoryLogger memoryLogger = new MemoryLogger("testUpdateRequest", 100);

    viewManager.setKeyboardLayout(KeyboardLayout.TWELVE_KEYS);

    final int actions = 500;

    SessionExecutor session = SessionExecutor.getInstance(null);
    KeyboardSpecification specification = viewManager.getJapaneseKeyboardSpecification();
    Request request = MozcUtil.getRequestForKeyboard(
        specification.getKeyboardSpecificationName(),
        specification.getSpecialRomanjiTable(),
        specification.getSpaceOnAlphanumeric(),
        specification.isKanaModifierInsensitiveConversion(),
        specification.getCrossingEdgeBehavior(),
        getInstrumentation().getTargetContext().getResources().getConfiguration());
    List<TouchEvent> touchEventList = Collections.emptyList();

    memoryLogger.logMemory("start");
    for (int i = 0; i < actions; i++) {
      session.updateRequest(request, touchEventList);
      // updateRequest sends asynchronous message to server. So we need to wait a moment
      // for server processing.
      sleep(100);

      memoryLogger.logMemoryInterval();
    }
    memoryLogger.logMemory("end");
  }

  @StressTest
  @UiThreadTest
  public void testOnCreateInputView() {
    MemoryLogger memoryLogger = new MemoryLogger("testOnCreateInputView", 100);

    viewManager.setKeyboardLayout(KeyboardLayout.TWELVE_KEYS);

    final int actions = 500;

    memoryLogger.logMemory("start");
    for (int i = 0; i < actions; i++) {
      viewManager.createMozcView(context);
      memoryLogger.logMemoryInterval();
    }
    memoryLogger.logMemory("end");
  }
}
