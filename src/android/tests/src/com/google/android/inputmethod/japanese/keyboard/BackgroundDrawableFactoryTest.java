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

package org.mozc.android.inputmethod.japanese.keyboard;

import static org.easymock.EasyMock.capture;
import static org.easymock.EasyMock.geq;
import static org.easymock.EasyMock.gt;
import static org.easymock.EasyMock.isA;

import org.mozc.android.inputmethod.japanese.keyboard.BackgroundDrawableFactory.DrawableType;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;
import org.mozc.android.inputmethod.japanese.testing.MozcMatcher.DeepCopyPaintCapture;
import org.mozc.android.inputmethod.japanese.view.SkinType;

import android.graphics.Canvas;
import android.graphics.ComposeShader;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RadialGradient;
import android.graphics.RectF;
import android.graphics.Shader;
import android.graphics.drawable.Drawable;
import android.test.suitebuilder.annotation.SmallTest;

import org.easymock.Capture;

/**
 */
public class BackgroundDrawableFactoryTest extends InstrumentationTestCaseWithMock {
  private BackgroundDrawableFactory factory;

  @Override
  public void setUp() throws Exception {
    super.setUp();
    factory = new BackgroundDrawableFactory(1f);
  }

  @Override
  public void tearDown() throws Exception {
    factory = null;
    super.tearDown();
  }

  @SmallTest
  public void testFlickCenterDrawable() {
    Capture<RectF> ovalCapture = new Capture<RectF>();
    Capture<Paint> paintCapture = new DeepCopyPaintCapture();
    Canvas canvas = createMock(Canvas.class);
    canvas.drawCircle(gt(0f), gt(0f), gt(0f), capture(paintCapture));
    replayAll();

    Drawable drawable = factory.getDrawable(DrawableType.TWELVEKEYS_CENTER_FLICK);
    drawable.setBounds(0, 0, 100, 100);
    drawable.draw(canvas);

    verifyAll();
    Shader shader1 = paintCapture.getValue().getShader();
    assertTrue(shader1 instanceof RadialGradient);

    resetAll();
    ovalCapture.reset();
    paintCapture.reset();
    canvas.drawCircle(gt(0f), gt(0f), gt(0f), capture(paintCapture));
    replayAll();

    drawable.setBounds(0, 0, 200, 200);
    drawable.draw(canvas);

    verifyAll();
    Shader shader2 = paintCapture.getValue().getShader();
    assertTrue(shader2 instanceof RadialGradient);

    assertNotSame(shader1, shader2);
  }

  @SmallTest
  public void testFlickTriangleHighlightDrawable() {
    Canvas canvas = createMock(Canvas.class);
    DrawableType[] drawableTypeList = {
        DrawableType.TWELVEKEYS_LEFT_FLICK,
        DrawableType.TWELVEKEYS_UP_FLICK,
        DrawableType.TWELVEKEYS_RIGHT_FLICK,
        DrawableType.TWELVEKEYS_DOWN_FLICK,
    };

    for (DrawableType drawableType: drawableTypeList) {
      resetAll();
      Capture<Path> pathCapture = new Capture<Path>();
      Capture<Paint> paintCapture = new DeepCopyPaintCapture();
      canvas.drawPath(capture(pathCapture), capture(paintCapture));
      replayAll();

      Drawable drawable = factory.getDrawable(drawableType);
      drawable.setBounds(0, 0, 100, 100);
      drawable.draw(canvas);

      verifyAll();
      assertFalse(pathCapture.getValue().isEmpty());
      Shader shader1 = paintCapture.getValue().getShader();
      assertTrue(shader1 instanceof ComposeShader);

      resetAll();
      pathCapture.reset();
      paintCapture.reset();
      canvas.drawPath(capture(pathCapture), capture(paintCapture));
      replayAll();
      drawable.setBounds(0, 0, 200, 200);
      drawable.draw(canvas);

      verifyAll();
      assertFalse(pathCapture.getValue().isEmpty());
      Shader shader2 = paintCapture.getValue().getShader();
      assertTrue(shader2 instanceof ComposeShader);

      assertNotSame(shader1, shader2);
    }
  }

  @SmallTest
  public void testTwelveKeysReleasedDrawable() {
    Canvas canvas = createStrictMock(Canvas.class);
    DrawableType[] drawableTypeList = {
        DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND,
        DrawableType.TWELVEKEYS_FUNCTION_KEY_BACKGROUND,
    };

    for (DrawableType drawableType : drawableTypeList) {
      Capture<Paint> paintCapture = new DeepCopyPaintCapture();
      resetAll();
      canvas.drawRect(geq(0f), geq(0f), geq(0f), geq(0f), isA(Paint.class));  // Shadow.
      canvas.drawRect(geq(0f), geq(0f), geq(0f), geq(0f), capture(paintCapture));  // Base.
      canvas.drawRect(geq(0f), geq(0f), geq(0f), geq(0f), isA(Paint.class));  // Highlight
      canvas.drawRect(geq(0f), geq(0f), geq(0f), geq(0f), isA(Paint.class));  // Left side shade
      canvas.drawRect(geq(0f), geq(0f), geq(0f), geq(0f), isA(Paint.class));  // Right side shade
      canvas.drawRect(geq(0f), geq(0f), geq(0f), geq(0f), isA(Paint.class));  // Bottom shade.
      replayAll();

      Drawable drawable = factory.getDrawable(drawableType);
      drawable.setBounds(0, 0, 100, 100);
      drawable.setState(new int[] {});
      drawable.draw(canvas);

      verifyAll();
      Shader shader1 = paintCapture.getValue().getShader();
      assertTrue(shader1 instanceof LinearGradient);

      resetAll();
      paintCapture.reset();
      resetAll();
      canvas.drawRect(geq(0f), geq(0f), geq(0f), geq(0f), isA(Paint.class));  // Shadow.
      canvas.drawRect(geq(0f), geq(0f), geq(0f), geq(0f), capture(paintCapture));  // Base.
      canvas.drawRect(geq(0f), geq(0f), geq(0f), geq(0f), isA(Paint.class));  // Highlight
      canvas.drawRect(geq(0f), geq(0f), geq(0f), geq(0f), isA(Paint.class));  // Left side shade
      canvas.drawRect(geq(0f), geq(0f), geq(0f), geq(0f), isA(Paint.class));  // Right side shade
      canvas.drawRect(geq(0f), geq(0f), geq(0f), geq(0f), isA(Paint.class));  // Bottom shade.
      replayAll();
      drawable.setBounds(0, 0, 200, 200);
      drawable.draw(canvas);

      verifyAll();
      Shader shader2 = paintCapture.getValue().getShader();
      assertTrue(shader2 instanceof LinearGradient);

      assertNotSame(shader1, shader2);
    }
  }

  @SmallTest
  public void testTwelveKeysPressedDrawable() {
    Canvas canvas = createStrictMock(Canvas.class);
    DrawableType[] drawableTypeList = {
        DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND,
        DrawableType.TWELVEKEYS_FUNCTION_KEY_BACKGROUND,
    };

    for (DrawableType drawableType : drawableTypeList) {
      Capture<Paint> paintCapture = new DeepCopyPaintCapture();
      resetAll();
      canvas.drawRect(geq(0f), geq(0f), geq(0f), geq(0f), capture(paintCapture));
      replayAll();

      Drawable drawable = factory.getDrawable(drawableType);
      drawable.setBounds(0, 0, 100, 100);
      drawable.setState(new int[] { android.R.attr.state_pressed });
      drawable.draw(canvas);

      verifyAll();
      Shader shader1 = paintCapture.getValue().getShader();
      assertTrue(shader1 instanceof LinearGradient);

      resetAll();
      paintCapture.reset();
      resetAll();
      canvas.drawRect(geq(0f), geq(0f), geq(0f), geq(0f), capture(paintCapture));
      replayAll();
      drawable.setBounds(0, 0, 200, 200);
      drawable.draw(canvas);

      verifyAll();
      Shader shader2 = paintCapture.getValue().getShader();
      assertTrue(shader2 instanceof LinearGradient);

      assertNotSame(shader1, shader2);
    }
  }

  @SmallTest
  public void testQwertyDrawable() {
    Canvas canvas = createStrictMock(Canvas.class);
    DrawableType[] drawableTypeList = {
        DrawableType.QWERTY_REGULAR_KEY_BACKGROUND,
        DrawableType.QWERTY_FUNCTION_KEY_BACKGROUND,
        DrawableType.QWERTY_FUNCTION_KEY_LIGHT_ON_BACKGROUND,
        DrawableType.QWERTY_FUNCTION_KEY_LIGHT_OFF_BACKGROUND,
    };

    int[][] stateList = {
        new int[] {},
        new int[] { android.R.attr.state_pressed },
    };

    for (int[] state : stateList) {
      for (DrawableType drawableType : drawableTypeList) {
        Capture<Paint> paintCapture = new DeepCopyPaintCapture();
        resetAll();
        canvas.drawRoundRect(isA(RectF.class), gt(0f), gt(0f), isA(Paint.class));  // Shadow.
        canvas.drawRoundRect(isA(RectF.class), gt(0f), gt(0f), capture(paintCapture));  // Base.

        if (drawableType == DrawableType.QWERTY_FUNCTION_KEY_LIGHT_ON_BACKGROUND ||
            drawableType == DrawableType.QWERTY_FUNCTION_KEY_LIGHT_OFF_BACKGROUND) {
          // Light mark should be drawn.
          canvas.drawCircle(gt(0f), gt(0f), gt(0f), isA(Paint.class));  // Base.
          canvas.drawCircle(gt(0f), gt(0f), gt(0f), isA(Paint.class));  // Shade.
        }

        replayAll();

        Drawable drawable = factory.getDrawable(drawableType);
        drawable.setBounds(0, 0, 100, 100);
        drawable.setState(state);
        drawable.draw(canvas);

        verifyAll();
        Shader shader1 = paintCapture.getValue().getShader();
        assertTrue(shader1 instanceof LinearGradient);

        resetAll();
        paintCapture.reset();
        resetAll();
        canvas.drawRoundRect(isA(RectF.class), gt(0f), gt(0f), isA(Paint.class));  // Shadow.
        canvas.drawRoundRect(isA(RectF.class), gt(0f), gt(0f), capture(paintCapture));  // Base.

        if (drawableType == DrawableType.QWERTY_FUNCTION_KEY_LIGHT_ON_BACKGROUND ||
            drawableType == DrawableType.QWERTY_FUNCTION_KEY_LIGHT_OFF_BACKGROUND) {
          // Light mark should be drawn.
          canvas.drawCircle(gt(0f), gt(0f), gt(0f), isA(Paint.class));  // Base.
          canvas.drawCircle(gt(0f), gt(0f), gt(0f), isA(Paint.class));  // Shade.
        }
        replayAll();
        drawable.setBounds(0, 0, 200, 200);
        drawable.draw(canvas);

        verifyAll();
        Shader shader2 = paintCapture.getValue().getShader();
        assertTrue(shader2 instanceof LinearGradient);

        assertNotSame(shader1, shader2);
      }
    }
  }

  @SmallTest
  public void testSetSkinType() {
    Drawable drawable = factory.getDrawable(DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND);
    assertNotNull(drawable);

    // The same instance should be used.
    assertSame(drawable, factory.getDrawable(DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND));

    // If setSkinType is invoked, but actually skin is not changed,
    // the same instance should be used.
    factory.setSkinType(SkinType.ORANGE_LIGHTGRAY);
    assertSame(drawable, factory.getDrawable(DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND));

    // If setSkinType is invoked, and actually the skin is changed,
    // the difference instance should be created.
    factory.setSkinType(SkinType.TEST);
    assertNotSame(drawable, factory.getDrawable(DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND));
  }
}
