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

import org.mozc.android.inputmethod.japanese.KeyboardSpecificationName;
import org.mozc.android.inputmethod.japanese.keyboard.Flick.Direction;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.CompositionMode;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Request.CrossingEdgeBehavior;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Request.SpaceOnAlphanumeric;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Request.SpecialRomanjiTable;
import org.mozc.android.inputmethod.japanese.resources.R;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;

import android.util.SparseIntArray;

import java.util.Collections;
import java.util.List;

/**
 * A simple model class of a keyboard.
 * A keyboard can contain a sequence of {@code Row}s.
 *
 */
public class Keyboard {

  /**
   * Each keyboard has its own specification.
   *
   * For example, some keyboards use a special Romanji table.
   */
  public static enum KeyboardSpecification {
    // 12 keys.
    TWELVE_KEY_TOGGLE_KANA(
        new KeyboardSpecificationName("TWELVE_KEY_TOGGLE_KANA", 0, 2, 0),
        R.xml.kbd_12keys_kana,
        false,
        CompositionMode.HIRAGANA,
        SpecialRomanjiTable.TWELVE_KEYS_TO_HIRAGANA,
        SpaceOnAlphanumeric.SPACE_OR_CONVERT_KEEPING_COMPOSITION,
        true,
        CrossingEdgeBehavior.DO_NOTHING),

    TWELVE_KEY_TOGGLE_ALPHABET(
        new KeyboardSpecificationName("TWELVE_KEY_TOGGLE_ALPHABET", 0, 2, 0),
        R.xml.kbd_12keys_abc,
        false,
        CompositionMode.HALF_ASCII,
        SpecialRomanjiTable.TWELVE_KEYS_TO_HALFWIDTHASCII,
        SpaceOnAlphanumeric.COMMIT,
        false,
        CrossingEdgeBehavior.DO_NOTHING),

    TWELVE_KEY_TOGGLE_QWERTY_ALPHABET(
        new KeyboardSpecificationName("TWELVE_KEY_TOGGLE_QWERTY_ALPHABET", 0, 5, 0),
        R.xml.kbd_qwerty_abc,
        false,
        CompositionMode.HALF_ASCII,
        SpecialRomanjiTable.QWERTY_MOBILE_TO_HALFWIDTHASCII,
        SpaceOnAlphanumeric.COMMIT,
        false,
        CrossingEdgeBehavior.COMMIT_WITHOUT_CONSUMING),

    // Flick mode.
    TWELVE_KEY_FLICK_KANA(
        new KeyboardSpecificationName("TWELVE_KEY_FLICK_KANA", 0, 2, 0),
        R.xml.kbd_12keys_flick_kana,
        false,
        CompositionMode.HIRAGANA,
        SpecialRomanjiTable.FLICK_TO_HIRAGANA,
        SpaceOnAlphanumeric.SPACE_OR_CONVERT_KEEPING_COMPOSITION,
        true,
        CrossingEdgeBehavior.DO_NOTHING),

    TWELVE_KEY_FLICK_ALPHABET(
        new KeyboardSpecificationName("TWELVE_KEY_FLICK_ALPHABET", 0, 2, 0),
        R.xml.kbd_12keys_flick_abc,
        false,
        CompositionMode.HALF_ASCII,
        SpecialRomanjiTable.FLICK_TO_HALFWIDTHASCII,
        SpaceOnAlphanumeric.COMMIT,
        false,
        CrossingEdgeBehavior.COMMIT_WITHOUT_CONSUMING),

    TWELVE_KEY_TOGGLE_FLICK_KANA(
        new KeyboardSpecificationName("TWELVE_KEY_TOGGLE_FLICK_KANA", 0, 2, 0),
        R.xml.kbd_12keys_flick_kana,
        false,
        CompositionMode.HIRAGANA,
        SpecialRomanjiTable.TOGGLE_FLICK_TO_HIRAGANA,
        SpaceOnAlphanumeric.SPACE_OR_CONVERT_KEEPING_COMPOSITION,
        true,
        CrossingEdgeBehavior.DO_NOTHING),

    TWELVE_KEY_TOGGLE_FLICK_ALPHABET(
        new KeyboardSpecificationName("TWELVE_KEY_TOGGLE_FLICK_ALPHABET", 0, 2, 0),
        R.xml.kbd_12keys_flick_abc,
        false,
        CompositionMode.HALF_ASCII,
        SpecialRomanjiTable.TOGGLE_FLICK_TO_HALFWIDTHASCII,
        SpaceOnAlphanumeric.COMMIT,
        false,
        CrossingEdgeBehavior.DO_NOTHING),

    // QWERTY keyboard.
    QWERTY_KANA(
        new KeyboardSpecificationName("QWERTY_KANA", 0, 4, 0),
        R.xml.kbd_qwerty_kana,
        false,
        CompositionMode.HIRAGANA,
        SpecialRomanjiTable.QWERTY_MOBILE_TO_HIRAGANA,
        SpaceOnAlphanumeric.SPACE_OR_CONVERT_KEEPING_COMPOSITION,
        false,
        CrossingEdgeBehavior.DO_NOTHING),

    QWERTY_ALPHABET(
        new KeyboardSpecificationName("QWERTY_ALPHABET", 0, 5, 0),
        R.xml.kbd_qwerty_abc,
        false,
        CompositionMode.HALF_ASCII,
        SpecialRomanjiTable.QWERTY_MOBILE_TO_HALFWIDTHASCII,
        SpaceOnAlphanumeric.COMMIT,
        false,
        CrossingEdgeBehavior.COMMIT_WITHOUT_CONSUMING),

    QWERTY_ALPHABET_NUMBER(
        new KeyboardSpecificationName("QWERTY_ALPHABET_NUMBER", 0, 3, 0),
        R.xml.kbd_qwerty_abc_123,
        false,
        CompositionMode.HALF_ASCII,
        SpecialRomanjiTable.QWERTY_MOBILE_TO_HALFWIDTHASCII,
        SpaceOnAlphanumeric.COMMIT,
        false,
        CrossingEdgeBehavior.COMMIT_WITHOUT_CONSUMING),

    // Godan keyboard.
    GODAN_KANA(
        new KeyboardSpecificationName("GODAN_KANA", 0, 2, 0),
        R.xml.kbd_godan_kana,
        false,
        CompositionMode.HIRAGANA,
        SpecialRomanjiTable.GODAN_TO_HIRAGANA,
        SpaceOnAlphanumeric.SPACE_OR_CONVERT_KEEPING_COMPOSITION,
        true,
        CrossingEdgeBehavior.COMMIT_WITHOUT_CONSUMING),

    NUMBER(
        new KeyboardSpecificationName("NUMBER", 0, 1, 0),
        R.xml.kbd_123,
        false,
        CompositionMode.HALF_ASCII,
        SpecialRomanjiTable.QWERTY_MOBILE_TO_HALFWIDTHASCII,
        SpaceOnAlphanumeric.COMMIT,
        false,
        CrossingEdgeBehavior.DO_NOTHING),

    // Number keyboard on symbol input view.
    SYMBOL_NUMBER(
        new KeyboardSpecificationName("TWELVE_KEY_SYMBOL_NUMBER", 0, 1, 0),
        R.xml.kbd_symbol_123,
        false,
        CompositionMode.HALF_ASCII,
        SpecialRomanjiTable.QWERTY_MOBILE_TO_HALFWIDTHASCII,
        SpaceOnAlphanumeric.COMMIT,
        false,
        CrossingEdgeBehavior.DO_NOTHING),

    // HARDWARE QWERTY keyboard.
    HARDWARE_QWERTY_KANA(
        new KeyboardSpecificationName("HARDWARE_QWERTY_KANA", 0, 1, 0),
        0,
        true,
        CompositionMode.HIRAGANA,
        SpecialRomanjiTable.DEFAULT_TABLE,
        SpaceOnAlphanumeric.SPACE_OR_CONVERT_KEEPING_COMPOSITION,
        false,
        CrossingEdgeBehavior.DO_NOTHING),

    HARDWARE_QWERTY_ALPHABET(
        new KeyboardSpecificationName("HARDWARE_QWERTY_ALPHABET", 0, 1, 0),
        0,
        true,
        CompositionMode.HALF_ASCII,
        SpecialRomanjiTable.DEFAULT_TABLE,
        SpaceOnAlphanumeric.COMMIT,
        false,
        CrossingEdgeBehavior.DO_NOTHING),

    ;

    private final KeyboardSpecificationName specName;
    private final int resourceId;
    private final boolean isHardwareKeyboard;
    private final CompositionMode compositionMode;
    private final SpecialRomanjiTable specialRomanjiTable;
    private final SpaceOnAlphanumeric spaceOnAlphanumeric;
    private final boolean kanaModifierInsensitiveConversion;
    private final CrossingEdgeBehavior crossingEdgeBehavior;

    private KeyboardSpecification(
        KeyboardSpecificationName specName,
        int resourceId,
        boolean isHardwareKeyboard,
        CompositionMode compositionMode,
        SpecialRomanjiTable specialRomanjiTable,
        SpaceOnAlphanumeric spaceOnAlphanumeric,
        boolean kanaModifierInsensitiveConversion,
        CrossingEdgeBehavior crossingEdgeBehavior) {
      this.specName = Preconditions.checkNotNull(specName);
      this.resourceId = resourceId;
      this.isHardwareKeyboard = isHardwareKeyboard;
      this.compositionMode = Preconditions.checkNotNull(compositionMode);
      this.specialRomanjiTable = Preconditions.checkNotNull(specialRomanjiTable);
      this.spaceOnAlphanumeric = Preconditions.checkNotNull(spaceOnAlphanumeric);
      this.kanaModifierInsensitiveConversion = kanaModifierInsensitiveConversion;
      this.crossingEdgeBehavior = Preconditions.checkNotNull(crossingEdgeBehavior);
    }

    public int getXmlLayoutResourceId() {
      return resourceId;
    }

    public CompositionMode getCompositionMode() {
      return compositionMode;
    }

    public KeyboardSpecificationName getKeyboardSpecificationName() {
      return specName;
    }

    public SpecialRomanjiTable getSpecialRomanjiTable() {
      return specialRomanjiTable;
    }

    public SpaceOnAlphanumeric getSpaceOnAlphanumeric() {
      return spaceOnAlphanumeric;
    }

    public boolean isKanaModifierInsensitiveConversion() {
      return kanaModifierInsensitiveConversion;
    }

    public CrossingEdgeBehavior getCrossingEdgeBehavior() {
      return crossingEdgeBehavior;
    }

    public boolean isHardwareKeyboard() {
      return isHardwareKeyboard;
    }
  }

  private final Optional<String> contentDescription;
  private final float flickThreshold;
  private final List<Row> rowList;

  public final int contentLeft;
  public final int contentRight;
  public final int contentTop;
  public final int contentBottom;
  protected final KeyboardSpecification specification;
  private Optional<SparseIntArray> sourceIdToKeyCode = Optional.absent();

  public Keyboard(Optional<String> contentDescription,
                  List<? extends Row> rowList, float flickThreshold,
                  KeyboardSpecification specification) {
    this.contentDescription = Preconditions.checkNotNull(contentDescription);
    this.flickThreshold = flickThreshold;
    this.rowList = Collections.unmodifiableList(rowList);

    int left = Integer.MAX_VALUE, right = Integer.MIN_VALUE,
        top = Integer.MAX_VALUE, bottom = Integer.MIN_VALUE;
    for (Row row : this.rowList) {
      for (Key key : row.getKeyList()) {
        left = Math.min(left, key.getX());
        right = Math.max(right, key.getX() + key.getWidth());
        top = Math.min(top, key.getY());
        bottom = Math.max(bottom, key.getY() + key.getHeight());
      }
    }
    this.contentLeft = left;
    this.contentRight = right;
    this.contentTop = top;
    this.contentBottom = bottom;
    this.specification = Preconditions.checkNotNull(specification);
  }

  public Optional<String> getContentDescription() {
    return contentDescription;
  }

  public float getFlickThreshold() {
    return flickThreshold;
  }

  public List<Row> getRowList() {
    return rowList;
  }

  public KeyboardSpecification getSpecification() {
    return specification;
  }

  /**
   * Returns keyCode from {@code souceId}.
   *
   * <p>If not found, {@code Integer.MIN_VALUE} is returned.
   */
  public int getKeyCode(int sourceId) {
    ensureSourceIdToKeyCode();
    return sourceIdToKeyCode.get().get(sourceId, Integer.MIN_VALUE);
  }

  private void ensureSourceIdToKeyCode() {
    if (sourceIdToKeyCode.isPresent()) {
      return;
    }
    SparseIntArray result = new SparseIntArray();
    for (Row row : getRowList()) {
      for (Key key : row.getKeyList()) {
        for (KeyState keyState : key.getKeyStates()) {
          for (Direction direction : Direction.values()) {
            Optional<Flick> flick = keyState.getFlick(direction);
            if (flick.isPresent()) {
              KeyEntity keyEntity = flick.get().getKeyEntity();
              result.put(keyEntity.getSourceId(), keyEntity.getKeyCode());
            }
          }
        }
      }
    }
    sourceIdToKeyCode = Optional.of(result);
  }
}
