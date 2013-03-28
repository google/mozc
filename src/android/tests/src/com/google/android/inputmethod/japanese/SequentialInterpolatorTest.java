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

import android.test.suitebuilder.annotation.SmallTest;
import android.view.animation.AccelerateInterpolator;
import android.view.animation.Interpolator;

import junit.framework.TestCase;

/**
 */
public class SequentialInterpolatorTest extends TestCase {
  @SmallTest
  public void testBuildEmpty() {
    try {
      SequentialInterpolator.newBuilder().build();
      fail("SequentialInterpolator.Builder must contain one or more interpolators.");
    } catch (IllegalArgumentException e) {
      // Expected.
    }
  }

  @SmallTest
  public void testBuildNegativeDuration() {
    try {
      SequentialInterpolator.newBuilder().add(new AccelerateInterpolator(), -1f, 1f).build();
      fail("The duration must be 0 or positive value.");
    } catch (IllegalArgumentException e) {
      // Expected.
    }
  }

  @SmallTest
  public void testBuildInvalidTarget() {
    try {
      SequentialInterpolator
          .newBuilder()
          .add(new AccelerateInterpolator(), 1f, 0f)
          .build();
      fail("The toValue of the last interpolator must be 1f.");
    } catch (IllegalArgumentException e) {
      // Expected.
    }
    try {
      SequentialInterpolator
          .newBuilder()
          .add(new AccelerateInterpolator(), 1f, 1f)
          .add(new AccelerateInterpolator(), -1f, 0f)
          .build();
      fail("The toValue of the last interpolator must be 1f.");
    } catch (IllegalArgumentException e) {
      // Expected.
    }
    // OK
    SequentialInterpolator
        .newBuilder()
        .add(new AccelerateInterpolator(), 1f, 0f)
        .add(new AccelerateInterpolator(), 1f, 1f)
        .build();
  }

  @SmallTest
  public void testGetInterpolationIdentical() {
    Interpolator original = new AccelerateInterpolator();
    Interpolator sequential = SequentialInterpolator.newBuilder().add(original, 1f, 1f).build();
    for (float i = 0f; i < 1f; i += 0.01f) {
      assertEquals(original.getInterpolation(i), sequential.getInterpolation(i));
    }
    assertEquals(original.getInterpolation(1f), sequential.getInterpolation(1f));
  }

  @SmallTest
  public void testGetInterpolationSequential() {
    Interpolator sequential = SequentialInterpolator
        .newBuilder()
        .add(new AccelerateInterpolator(), 1f, -1f)
        .add(new AccelerateInterpolator(), 1f, -2f)
        .add(new AccelerateInterpolator(), 1f, -3f)
        .add(new AccelerateInterpolator(), 1f, -4f)
        .add(new AccelerateInterpolator(), 1f, 1f)
        .build();
    assertEquals(0f, sequential.getInterpolation(0f * 1f / 5f));
    assertEquals(-1f, sequential.getInterpolation(1f * 1f / 5f));
    assertEquals(-2f, sequential.getInterpolation(2f * 1f / 5f));
    assertEquals(-3f, sequential.getInterpolation(3f * 1f / 5f));
    assertEquals(-4f, sequential.getInterpolation(4f * 1f / 5f));
    assertEquals(1f, sequential.getInterpolation(5f * 1f / 5f));
  }
}
