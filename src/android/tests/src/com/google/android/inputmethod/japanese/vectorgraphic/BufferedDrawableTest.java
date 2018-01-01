// Copyright 2010-2018, Google Inc.
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

package org.mozc.android.inputmethod.japanese.vectorgraphic;

import com.google.common.base.Preconditions;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Picture;
import android.graphics.drawable.PictureDrawable;
import android.test.suitebuilder.annotation.SmallTest;

import junit.framework.TestCase;

import java.util.Locale;
import java.util.Random;

/**
 * Tests for BufferedDrawable.
 */
public class BufferedDrawableTest extends TestCase {

  private Picture createPicture(Random random, int width, int height, int lines) {
    Picture picture = new Picture();
    Canvas canvas = picture.beginRecording(width, height);
    Paint paint = new Paint();
    paint.setStrokeWidth(3f);
    for (int i = 0; i < lines; ++i) {
      // NOTE: Translucent color makes noise so use opaque color.
      paint.setARGB(0xFF, random.nextInt(0xFF), random.nextInt(0xFF), random.nextInt(0xFF));
      canvas.drawLine(random.nextInt(width), random.nextInt(height),
                      random.nextInt(width), random.nextInt(height), paint);
    }
    picture.endRecording();
    return picture;
  }

  float calculateErrorRate(Bitmap lhs, Bitmap rhs) {
    Preconditions.checkArgument(lhs.getWidth() == rhs.getWidth());
    Preconditions.checkArgument(lhs.getHeight() == rhs.getHeight());
    int pixels = lhs.getWidth() * lhs.getHeight();
    int[] lhsPixels = new int[pixels];
    int[] rhsPixels = new int[pixels];
    lhs.getPixels(lhsPixels, 0, lhs.getWidth(), 0, 0, lhs.getWidth(), lhs.getHeight());
    rhs.getPixels(rhsPixels, 0, lhs.getWidth(), 0, 0, lhs.getWidth(), lhs.getHeight());

    int errors = 0;
    for (int i = 0; i < pixels; ++i) {
      if (lhsPixels[i] != rhsPixels[i]) {
        ++errors;
      }
    }
    return (float) errors / pixels;
  }

  private void drawAndCompare(String testName, Bitmap baseResult, Bitmap bufferedResult,
                              Canvas canvas, Random random, float acceptableErrorRate) {
    int lines = 1000;
    int canvasSize = baseResult.getHeight();
    Picture picture = createPicture(random, canvasSize, canvasSize, lines);
    {
      PictureDrawable baseDrawable = new PictureDrawable(picture);
      baseDrawable.setBounds(0, 0, canvasSize, canvasSize);
      canvas.setBitmap(baseResult);
      baseDrawable.draw(canvas);
    }
    {
      BufferedDrawable bufferedDrawable = new BufferedDrawable(new PictureDrawable(picture));
      bufferedDrawable.setBounds(0, 0, canvasSize, canvasSize);
      canvas.setBitmap(bufferedResult);
      bufferedDrawable.draw(canvas);
    }
    float errorRate = calculateErrorRate(baseResult, bufferedResult);
    if (errorRate > acceptableErrorRate) {
      BitmapSaver.saveAsPng(testName + "_base.png", baseResult);
      BitmapSaver.saveAsPng(testName + "_buffered.png", bufferedResult);
      fail(String.format(Locale.ENGLISH, "%s -- errorRate:%f", testName, errorRate));
    }
  }

  @SmallTest
  public void testIdentityMatrix() {
    int canvasSize = 512;
    int iteration = 30;
    Bitmap baseResult = Bitmap.createBitmap(canvasSize, canvasSize, Bitmap.Config.ARGB_8888);
    Bitmap bufferedResult = Bitmap.createBitmap(canvasSize, canvasSize, Bitmap.Config.ARGB_8888);
    Canvas canvas = new Canvas();
    try {
      for (int i = 0; i < iteration; ++i) {
        Random random = new Random(i);
        drawAndCompare("testNoMatrix" + i, baseResult, bufferedResult, canvas, random, 0);
      }
    } finally {
      baseResult.recycle();
      bufferedResult.recycle();
    }
  }

  @SmallTest
  public void testTranslation() {
    int canvasSize = 512;
    int iteration = 30;
    Bitmap baseResult = Bitmap.createBitmap(canvasSize, canvasSize, Bitmap.Config.ARGB_8888);
    Bitmap bufferedResult = Bitmap.createBitmap(canvasSize, canvasSize, Bitmap.Config.ARGB_8888);
    Canvas canvas = new Canvas();
    try {
      for (int i = 0; i < iteration; ++i) {
        Random random = new Random(i);
        canvas.setMatrix(null);
        canvas.translate((int) (random.nextGaussian() * canvasSize / 10),
                         (int) (random.nextGaussian() * canvasSize / 10));
        // Translation causes small noise.
        drawAndCompare("testTransformation" + i,
                       baseResult, bufferedResult, canvas, random, 0.02f);
      }
    } finally {
      baseResult.recycle();
      bufferedResult.recycle();
    }
  }

  @SmallTest
  public void testScalingTransformation() {
    int canvasSize = 512;
    int iteration = 30;
    Bitmap baseResult = Bitmap.createBitmap(canvasSize, canvasSize, Bitmap.Config.ARGB_8888);
    Bitmap bufferedResult = Bitmap.createBitmap(canvasSize, canvasSize, Bitmap.Config.ARGB_8888);
    Canvas canvas = new Canvas();
    try {
      for (int i = 0; i < iteration; ++i) {
        Random random = new Random(i);
        canvas.setMatrix(null);
        canvas.translate((int) (random.nextGaussian() * canvasSize / 10),
                         (int) (random.nextGaussian() * canvasSize / 10));
        canvas.scale(random.nextFloat() * 2f + 0.1f, random.nextFloat() * 2f + 0.1f);
        // Translation and scale cause small noise.
        drawAndCompare("testScalingTransformation" + i,
                       baseResult, bufferedResult, canvas, random, 0.02f);
      }
    } finally {
      baseResult.recycle();
      bufferedResult.recycle();
    }
  }

  @SmallTest
  public void testVariousCanvasSize() {
    int[] canvasWidth = {15, 16, 17, 255, 256, 257};
    int[] canvasHeight = {15, 16, 17, 255, 256, 257};
    Canvas canvas = new Canvas();
    for (int width : canvasWidth) {
      for (int height : canvasHeight) {
        Bitmap baseResult = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        Bitmap bufferedResult = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        try {
          drawAndCompare("testVariousCanvasSize(" + width + ", " + height + ")",
              baseResult, bufferedResult, canvas, new Random(0), 0.02f);
        } finally {
          baseResult.recycle();
          bufferedResult.recycle();
        }
      }
    }
  }
}
