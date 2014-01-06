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

/**
 * A class corresponding to {@code &lt;Flick&gt;} element in xml resource files.
 * This class represents what should be happened when user flicks at a key.
 * 
 */
public class Flick {
  /**
   * A simple enum representing flicking direction.
   */
  public static enum Direction {
    CENTER(0), LEFT(1), RIGHT(2), UP(3), DOWN(4);

    // An index also defined in res/values/attr.xml.
    final int index;
    private Direction(int index) {
      this.index = index;
    }

    public static Direction valueOf(int index) {
      for (Direction direction : values()) {
        if (direction.index == index) {
          return direction;
        }
      }
      throw new IllegalArgumentException("Corresponding Direction is not found: " + index);
    }
  }

  private final Flick.Direction direction;
  private final KeyEntity keyEntity;
  public Flick(Direction direction, KeyEntity keyEntity) {
    if (direction == null) {
      throw new NullPointerException("direction shouldn't be null.");
    }
    if (keyEntity == null) {
      throw new NullPointerException("keyEntity shouldn't be null.");
    }
    this.direction = direction;
    this.keyEntity = keyEntity;
  }

  public Direction getDirection() {
    return direction;
  }

  public KeyEntity getKeyEntity() {
    return keyEntity;
  }
}
