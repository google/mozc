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

package org.mozc.android.inputmethod.japanese.view;

import android.graphics.drawable.AnimationDrawable;
import android.graphics.drawable.Drawable;

/**
 * Drawable implementation to support Emoji character with animation.
 *
 */
public class EmojiAnimationDrawable extends AnimationDrawable {

  /**
   * Cumulative duration of each Drawable.
   */
  private long totalDuration = 0;

  @Override
  public void addFrame(Drawable frame, int duration) {
    super.addFrame(frame, duration);
    totalDuration += duration;
  }

  public long getTotalDuration() {
    return totalDuration;
  }

  public boolean selectDrawableByTime(long time) {
    return selectDrawable(getDrawableIndexByTime(time));
  }

  private int getDrawableIndexByTime(long time) {
    long remainingTime = time % totalDuration;
    int numberOfFrames = getNumberOfFrames();
    long cumulativeTime = 0;
    // We expect the number of frames is up to 10, so we just use linear search here.
    for (int i = 0; i < numberOfFrames; ++i) {
      cumulativeTime += getDuration(i);
      if (remainingTime < cumulativeTime) {
        return i;
      }
    }
    return 0;
  }

  public long getNextFrameTiming(long time) {
    long remainingTime = time % totalDuration;
    int numberOfFrames = getNumberOfFrames();
    long cumulativeTime = 0;
    // We expect the number of frames is up to 10, so we just use linear search here.
    for (int i = 0; i < numberOfFrames; ++i) {
      cumulativeTime += getDuration(i);
      if (remainingTime < cumulativeTime) {
        break;
      }
    }
    return (time - remainingTime) + cumulativeTime;
  }
}
