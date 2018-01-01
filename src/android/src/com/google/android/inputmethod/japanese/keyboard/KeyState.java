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

package org.mozc.android.inputmethod.japanese.keyboard;

import com.google.common.base.MoreObjects;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;
import com.google.common.collect.Sets;

import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.EnumMap;
import java.util.EnumSet;
import java.util.Map;
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
    SHIFT(1, true),
    CAPS_LOCK(2, false),
    ALT(4, false),

    // Actions
    // c.f, http://developer.android.com/reference/android/view/inputmethod/EditorInfo.html
    // TODO(matsuzakit): Implement me.
    ACTION_DONE(8, false),
    ACTION_GO(16, false),
    ACTION_NEXT(32, false),
    ACTION_NONE(64, false),
    ACTION_PREVIOUS(128, false),
    ACTION_SEARCH(256, false),
    ACTION_SEND(512, false),

    // Text variation
    // c.f., http://developer.android.com/reference/android/text/InputType.html
    // Raw InputType uses 30 bits field so directly representing it into a flag is impossible.
    // Therefore only some important ones are listed up here.
    // TYPE_TEXT_VARIATION_URI
    VARIATION_URI(1024, false),
    // TYPE_TEXT_VARIATION_WEB_EMAIL_ADDRESS, TYPE_TEXT_VARIATION_EMAIL_ADDRESS
    VARIATION_EMAIL_ADDRESS(2048, false),

    // Set if Globe button should be offered.
    // c.f., SubtypeImeSwitcher#shouldOfferSwitchingToNextInputMethod()
    GLOBE(4096, false),
    // !GLOBE
    NO_GLOBE(8192, false),

    // Set if there is composition string.
    COMPOSING(16384, false),
    // Set if the KeyboardView is handling at least one touch event.
    HANDLING_TOUCH_EVENT(32768, false),

    // DO NOT USE. Theoretically this is package private entry.
    // "fallback" flag is useful when defining .xml file with logical-OR operator
    // (e.g. "fallback|composing").
    // This entry is used just for it.
    FALLBACK(1073741824, false),
    ;

    private final int bitFlag;

    /**
     * If this flag is set to {@code true}, the flag should be removed from the meta states
     * when a user type a key under this state.
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

    // MetaStates for character type.
    public static final Set<MetaState> CHAR_TYPE_EXCLUSIVE_GROUP =
        Sets.immutableEnumSet(MetaState.SHIFT, MetaState.CAPS_LOCK, MetaState.ALT);
    // MetaStates for actions.
    public static final Set<MetaState> ACTION_EXCLUSIVE_GROUP =
        Sets.immutableEnumSet(EnumSet.range(MetaState.ACTION_DONE, MetaState.ACTION_SEND));
    // MetaStates for text variations.
    public static final Set<MetaState> VARIATION_EXCLUSIVE_GROUP =
        Sets.immutableEnumSet(MetaState.VARIATION_URI, MetaState.VARIATION_EMAIL_ADDRESS);
    // MetaStates for Globe icon.
    public static final Set<MetaState> GLOBE_EXCLUSIVE_OR_GROUP =
        Sets.immutableEnumSet(MetaState.GLOBE, MetaState.NO_GLOBE);
    @SuppressWarnings("unchecked")
    private static final Collection<Set<MetaState>> EXCLUSIVE_GROUP =
        Arrays.<Set<MetaState>>asList(CHAR_TYPE_EXCLUSIVE_GROUP,
                                      ACTION_EXCLUSIVE_GROUP,
                                      VARIATION_EXCLUSIVE_GROUP,
                                      GLOBE_EXCLUSIVE_OR_GROUP);
    private static final Collection<Set<MetaState>> OR_GROUP =
        Collections.singleton(GLOBE_EXCLUSIVE_OR_GROUP);

    /**
     * Checks if {@code testee} is valid set.
     * <p>
     * Note that this check might be a little bit heavy.
     * Do not call from chokepoint.
     */
    public static boolean isValidSet(Set<MetaState> testee) {
      Preconditions.checkNotNull(testee);

      // Set#retainAll can make the implementation simpler, but it requires instantiation of
      // (Enum)Set for each iteration.
      for (Set<MetaState> exclusiveGroup : EXCLUSIVE_GROUP) {
        int count = 0;
        for (MetaState metaState : exclusiveGroup) {
          if (testee.contains(metaState)) {
            ++count;
            if (count >= 2) {
              return false;
            }
          }
        }
      }
      for (Set<MetaState> orGroup : OR_GROUP) {
        if (orGroup.isEmpty()) {
          return false;
        }
      }
      return true;
    }
  }

  private final String contentDescription;
  private final Set<MetaState> metaState;
  private final Set<MetaState> nextAddMetaStates;
  private final Set<MetaState> nextRemoveMetaStates;
  private final EnumMap<Flick.Direction, Flick> flickMap;

  public KeyState(String contentDescription,
                  Set<MetaState> metaStates,
                  Set<MetaState> nextAddMetaStates,
                  Set<MetaState> nextRemoveMetaStates,
                  Collection<? extends Flick> flickCollection) {
    this.contentDescription = Preconditions.checkNotNull(contentDescription);
    Preconditions.checkNotNull(metaStates);
    this.metaState = Sets.newEnumSet(metaStates, MetaState.class);
    this.nextAddMetaStates = Preconditions.checkNotNull(nextAddMetaStates);
    this.nextRemoveMetaStates = Preconditions.checkNotNull(nextRemoveMetaStates);
    this.flickMap = new EnumMap<Flick.Direction, Flick>(Flick.Direction.class);
    for (Flick flick : Preconditions.checkNotNull(flickCollection)) {
      Preconditions.checkArgument(
          this.flickMap.put(flick.getDirection(), flick) == null,
          "Duplicate flick direction is found: " + flick.getDirection());
    }
  }

  public String getContentDescription() {
    return contentDescription;
  }

  public Set<MetaState> getMetaStateSet() {
    return metaState;
  }

  /**
   * Gets next MetaState.
   * <p>
   * First, flags in {@code nextRemoveMetaState} are removed from {@code originalMetaState}.
   * Then, flags in {@code nextAddMetaState} are added into {@code originalMetaState}.
   * <p>
   * The result is "valid" in the light of {@code MetaState#isValidSet(Set)}.
   */
  public Set<MetaState> getNextMetaStates(Set<MetaState> originalMetaStates) {
    return Sets.union(Sets.difference(Preconditions.checkNotNull(originalMetaStates),
                                      nextRemoveMetaStates), nextAddMetaStates).immutableCopy();
  }

  public Optional<Flick> getFlick(Flick.Direction direction) {
    return Optional.fromNullable(flickMap.get(direction));
  }

  @Override
  public String toString() {
    MoreObjects.ToStringHelper helper = MoreObjects.toStringHelper(this);
    helper.add("metaStates", metaState.toString());
    for (Map.Entry<Flick.Direction, Flick> entry : flickMap.entrySet()) {
      helper.add("flickMap(" + entry.getKey().toString() + ")", entry.getValue().toString());
    }
    return helper.toString();
  }
}
