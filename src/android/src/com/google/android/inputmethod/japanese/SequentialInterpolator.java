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

import android.view.animation.Interpolator;

import java.util.ArrayList;
import java.util.List;

/**
 * This interpolator uses registered interpolators sequentially.
 *
 * If you want an interpolator which has complex interpolation, this class help you.
 *
 */
class SequentialInterpolator implements Interpolator {
  static class Builder {
    private List<Interpolator> interpolators;
    private List<Float> durations;
    private List<Float> toValues;
    private float totalDuration;
    private Builder() {
      final int EXPECTED_SIZE = 4;
      interpolators = new ArrayList<Interpolator>(EXPECTED_SIZE);
      durations = new ArrayList<Float>(EXPECTED_SIZE);
      totalDuration = 0f;
      toValues = new ArrayList<Float>(EXPECTED_SIZE);
    }

    /**
     * Adds internal interpolator.
     * @param interpolator the interpolator to be added.
     * @param duration the duration ratio which <code>interpolator</code> occupies.
     *     Must be 0 or positive value.
     * @param toValue the target value to which <code>interpolator</code> targes.
     *     The last interpolator's <code>toValue</code> must be 1f.
     * @return the builder.
     */
    Builder add(Interpolator interpolator, float duration, float toValue) {
      if (interpolator == null) {
        throw new NullPointerException("interpolator must not be null");
      }
      if (duration < 0f) {
        throw new IllegalArgumentException("duration must be >= 0");
      }
      interpolators.add(interpolator);
      durations.add(duration);
      toValues.add(toValue);
      totalDuration += duration;
      return this;
    }
    /**
     * @return Built interpolator.
     */
    SequentialInterpolator build() {
      if (interpolators.isEmpty()) {
        throw new IllegalArgumentException();
      }
      if (toValues.get(toValues.size() - 1) != 1f) {
        throw new IllegalArgumentException("The last toValue must be 1.0f");
      }
      return new SequentialInterpolator(interpolators, durations, totalDuration, toValues);
    }
  }

  private final Interpolator[] interpolators;
  private final Float[] durations;
  private final Float[] targets;
  private final float totalDuration;

  private SequentialInterpolator(List<Interpolator> interpolators, List<Float> durations,
      float totalDuration, List<Float> targets) {
    this.interpolators = interpolators.toArray(new Interpolator[interpolators.size()]);
    this.durations = durations.toArray(new Float[durations.size()]);
    this.totalDuration = totalDuration;
    this.targets = targets.toArray(new Float[targets.size()]);
  }

  @Override
  public float getInterpolation(float input) {
    Interpolator interpolator = null;
    final float targetDuration = totalDuration * input;
    float durationSum = 0;
    float from = 0f;
    float to = 0f;
    float modifiedInput = 0f;
    for (int i = 0; i < interpolators.length; ++i) {
      float oldDurationSum = durationSum;
      float duration = durations[i];
      durationSum += duration;
      to = targets[i];
      if (targetDuration <= durationSum) {
        interpolator = interpolators[i];
        modifiedInput = (targetDuration - oldDurationSum) / duration;
        break;
      }
      from = to;
    }
    return from + (to - from) * interpolator.getInterpolation(modifiedInput);
  }

  /**
   * @return the builder to build the interpolator.
   */
  static SequentialInterpolator.Builder newBuilder() {
    return new Builder();
  }
}
