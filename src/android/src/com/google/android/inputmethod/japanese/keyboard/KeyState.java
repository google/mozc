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

package org.mozc.android.inputmethod.japanese.keyboard;

import java.util.Collection;
import java.util.EnumMap;
import java.util.EnumSet;
import java.util.Set;

/**
 * This is a model class of a key's state, corresponding to a {@code &lt;KeyState&gt;} element
 * in a xml resource file.
 *
 * Each key can have multiple state based on meta keys' condition. This class represents such
 * various state of each key.
 *
 * Each key state can have multiple {@code Flick} instances. See also the class for the details.
 *
 */
public class KeyState {
  public enum MetaState {
    UNMODIFIED(0, false), SHIFT(1, true), CAPS_LOCK(2, false), ALT(4, false);

    private final int bitFlag;

    /**
     * If this flag is set to {@code true}, the meta state of the keyboard should be
     * automatically set back to {@code UNMODIFIED} when a user type a key under this state.
     */
    final boolean isOneTimeMetaState;

    private MetaState(int bitFlag, boolean isOneTimeMetaState) {
      this.bitFlag = bitFlag;
      this.isOneTimeMetaState = isOneTimeMetaState;
    }

    public static MetaState valueOf(int bitFlag) {
      for (MetaState metaState : values()) {
        if (metaState.bitFlag == bitFlag) {
          return metaState;
        }
      }
      throw new IllegalArgumentException("Corresponding MetaState is not found: " + bitFlag);
    }
  }

  private final EnumSet<MetaState> metaStateSet;
  private final MetaState nextMetaState;
  private final EnumMap<Flick.Direction, Flick> flickMap;

  public KeyState(Set<MetaState> metaStateSet, MetaState nextMetaState,
                  Collection<? extends Flick> flickCollection) {
    this.metaStateSet = EnumSet.copyOf(metaStateSet);
    this.nextMetaState = nextMetaState;
    this.flickMap = new EnumMap<Flick.Direction, Flick>(Flick.Direction.class);
    for (Flick flick : flickCollection) {
      if (this.flickMap.put(flick.getDirection(), flick) != null) {
        throw new IllegalArgumentException(
            "Duplicate flick direction is found: " + flick.getDirection());
      }
    }
  }

  public Set<MetaState> getMetaStateSet() {
    return metaStateSet;
  }

  public MetaState getNextMetaState() {
    return nextMetaState;
  }

  public Flick getFlick(Flick.Direction direction) {
    return flickMap.get(direction);
  }
}
