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

package org.mozc.android.inputmethod.japanese;

import static org.easymock.EasyMock.eq;

import org.mozc.android.inputmethod.japanese.JapaneseKeyboard.KeyboardSpecification;
import org.mozc.android.inputmethod.japanese.keyboard.KeyEventHandler;
import org.mozc.android.inputmethod.japanese.keyboard.KeyboardActionListener;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.TouchEvent;
import org.mozc.android.inputmethod.japanese.resources.R;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;
import org.mozc.android.inputmethod.japanese.testing.MainThreadRunner;
import org.mozc.android.inputmethod.japanese.testing.Parameter;

import android.app.Activity;
import android.content.res.Resources;
import android.content.res.Resources.NotFoundException;
import android.os.Looper;
import android.test.suitebuilder.annotation.SmallTest;
import android.util.DisplayMetrics;
import android.view.MotionEvent;

import org.easymock.EasyMock;
import org.xmlpull.v1.XmlPullParserException;

import java.io.IOException;
import java.util.List;

/**
 */
public class JapaneseKeyboardViewTest extends InstrumentationTestCaseWithMock {
  private static final int STEP_COUNT = 20;
  private static final int NUM_ROWS = 4;

  // TODO(hidehiko): Get rid of hard coded parameter (21.8) and split the dependency to the real
  //   resources from functional unittests (instead, we should have test cases for resources
  //   or regression tests independently).
  private static int getKeyWidth(Resources resources) {
    DisplayMetrics displayMetrics = resources.getDisplayMetrics();
    return (int) Math.round(displayMetrics.widthPixels * 21.8);
  }

  private static int getTwelveKeysLayoutKeyHeight(Resources resources) {
    return resources.getDimensionPixelOffset(R.dimen.input_frame_height) / 4;
  }

  private static void drag(MainThreadRunner mainThreadRunner, JapaneseKeyboardView view,
                           int fromX, int toX, int fromY, int toY) {
    long downTime = MozcUtil.getUptimeMillis();
    long eventTime = MozcUtil.getUptimeMillis();

    mainThreadRunner.onTouchEvent(
        view, MotionEvent.obtain(downTime, eventTime, MotionEvent.ACTION_DOWN, fromX, fromY, 0));

    for (int i = 1; i < STEP_COUNT; ++i) {
      int x = fromX + (toX - fromX) * i / STEP_COUNT;
      int y = fromY + (toY - fromY) * i / STEP_COUNT;

      eventTime = MozcUtil.getUptimeMillis();
      mainThreadRunner.onTouchEvent(
          view, MotionEvent.obtain(downTime, eventTime, MotionEvent.ACTION_MOVE, x, y, 0));
    }

    eventTime = MozcUtil.getUptimeMillis();
    mainThreadRunner.onTouchEvent(
        view, MotionEvent.obtain(downTime, eventTime, MotionEvent.ACTION_UP, toX, toY, 0));
  }

  private JapaneseKeyboard createJapaneseKeyboard(KeyboardSpecification spec,
                                                  int width, int height) {
    Resources resources = getInstrumentation().getTargetContext().getResources();
    JapaneseKeyboardParser parser = new JapaneseKeyboardParser(
        resources, resources.getXml(spec.getXmlLayoutResourceId()), spec, width, height);
    try {
      return parser.parseKeyboard();
    } catch (NotFoundException e) {
      fail(e.getMessage());
    } catch (XmlPullParserException e) {
      fail(e.getMessage());
    } catch (IOException e) {
      fail(e.getMessage());
    }

    throw new AssertionError("Should never reach here");
  }

  @SmallTest
  public void testFlick() {
    JapaneseKeyboardView view = new JapaneseKeyboardView(getInstrumentation().getTargetContext());
    Activity activity = launchActivity(
        getInstrumentation().getTargetContext().getPackageName(), Activity.class, null);
    try {
      MainThreadRunner mainThreadRunner = new MainThreadRunner(getInstrumentation());
      mainThreadRunner.setContentView(activity, view);
      Resources resources = getInstrumentation().getTargetContext().getResources();
      int keyWidth = getKeyWidth(resources);
      int keyHeight = getTwelveKeysLayoutKeyHeight(resources);
      int width = resources.getDisplayMetrics().widthPixels;
      int height = keyHeight * NUM_ROWS;
      mainThreadRunner.setJapaneseKeyboard(
          view, createJapaneseKeyboard(KeyboardSpecification.TWELVE_KEY_FLICK_KANA, width, height));
      mainThreadRunner.layout(view, 0, 0, width, height);

      // Move center to up.
      int fromX = view.getWidth() / 2;
      int fromY = view.getHeight() / 2 - keyHeight / 2;

     class TestData extends Parameter {
       final int toX;
       final int toY;
       final int expectedCode;
       TestData(int toX, int toY, int expectedCode) {
         this.toX = toX;
         this.toY = toY;
         this.expectedCode = expectedCode;
       }
     }
     TestData[] testDataList = {
         new TestData(fromX - keyWidth, fromY, 'j'),   // Left
         new TestData(fromX + keyWidth, fromY, 'l'),   // Right
         new TestData(fromX, fromY - keyWidth, 'k'),  // Up
         new TestData(fromX, fromY + keyWidth, '%'),  // Down
     };

     KeyboardActionListener keyboardActionListener = createMock(KeyboardActionListener.class);
     KeyEventHandler keyEventHandler =
         new KeyEventHandler(Looper.myLooper(), keyboardActionListener, 0, 0, 0);
     for (TestData testData : testDataList) {
       resetAll();
       keyboardActionListener.onPress('5');
       keyboardActionListener.onKey(
           eq(testData.expectedCode), EasyMock.<List<TouchEvent>>notNull());
       keyboardActionListener.onRelease('5');
       replayAll();

       view.setKeyEventHandler(keyEventHandler);
       drag(mainThreadRunner, view, fromX, testData.toX, fromY, testData.toY);
       view.setKeyEventHandler(null);

       verifyAll();
     }
    } finally {
      if (activity != null) {
        activity.finish();
      }
    }
  }
}
