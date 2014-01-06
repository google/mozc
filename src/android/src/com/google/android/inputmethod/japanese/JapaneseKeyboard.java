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

import org.mozc.android.inputmethod.japanese.keyboard.Keyboard;
import org.mozc.android.inputmethod.japanese.keyboard.Row;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.CompositionMode;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Request.CrossingEdgeBehavior;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Request.SpaceOnAlphanumeric;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Request.SpecialRomanjiTable;
import org.mozc.android.inputmethod.japanese.resources.R;

import java.util.List;

/**
 */
public class JapaneseKeyboard extends Keyboard {
  /**
   * Each keyboard has its own specification.
   *
   * For example, some keyboards use a special Romanji table.
   */
  public static enum KeyboardSpecification {
    // 12 keys.
    TWELVE_KEY_TOGGLE_KANA(
        new KeyboardSpecificationName("TWELVE_KEY_TOGGLE_KANA", 0, 1, 0),
        R.xml.kbd_12keys_kana,
        CompositionMode.HIRAGANA,
        SpecialRomanjiTable.TWELVE_KEYS_TO_HIRAGANA,
        SpaceOnAlphanumeric.SPACE_OR_CONVERT_KEEPING_COMPOSITION,
        true,
        CrossingEdgeBehavior.DO_NOTHING),

    TWELVE_KEY_TOGGLE_ALPHABET(
        new KeyboardSpecificationName("TWELVE_KEY_TOGGLE_ALPHABET", 0, 1, 0),
        R.xml.kbd_12keys_abc,
        CompositionMode.HALF_ASCII,
        SpecialRomanjiTable.TWELVE_KEYS_TO_HALFWIDTHASCII,
        SpaceOnAlphanumeric.COMMIT,
        false,
        CrossingEdgeBehavior.DO_NOTHING),

    TWELVE_KEY_TOGGLE_NUMBER(
        new KeyboardSpecificationName("TWELVE_KEY_TOGGLE_NUMBER", 0, 1, 0),
        R.xml.kbd_12keys_123,
        CompositionMode.HALF_ASCII,
        SpecialRomanjiTable.TWELVE_KEYS_TO_NUMBER,
        SpaceOnAlphanumeric.COMMIT,
        false,
        CrossingEdgeBehavior.DO_NOTHING),

    TWELVE_KEY_TOGGLE_QWERTY_ALPHABET(
        new KeyboardSpecificationName("TWELVE_KEY_TOGGLE_QWERTY_ALPHABET", 0, 3, 0),
        R.xml.kbd_12keys_qwerty_abc,
        CompositionMode.HALF_ASCII,
        SpecialRomanjiTable.QWERTY_MOBILE_TO_HALFWIDTHASCII,
        SpaceOnAlphanumeric.COMMIT,
        false,
        CrossingEdgeBehavior.COMMIT_WITHOUT_CONSUMING),

    // Flick mode.
    TWELVE_KEY_FLICK_KANA(
        new KeyboardSpecificationName("TWELVE_KEY_FLICK_KANA", 0, 1, 2),
        R.xml.kbd_12keys_flick_kana,
        CompositionMode.HIRAGANA,
        SpecialRomanjiTable.FLICK_TO_HIRAGANA,
        SpaceOnAlphanumeric.SPACE_OR_CONVERT_KEEPING_COMPOSITION,
        true,
        CrossingEdgeBehavior.DO_NOTHING),

    TWELVE_KEY_FLICK_ALPHABET(
        new KeyboardSpecificationName("TWELVE_KEY_FLICK_ALPHABET", 0, 1, 0),
        R.xml.kbd_12keys_flick_abc,
        CompositionMode.HALF_ASCII,
        SpecialRomanjiTable.FLICK_TO_HALFWIDTHASCII,
        SpaceOnAlphanumeric.COMMIT,
        false,
        CrossingEdgeBehavior.COMMIT_WITHOUT_CONSUMING),

    TWELVE_KEY_FLICK_NUMBER(
        new KeyboardSpecificationName("TWELVE_KEY_FLICK_NUMBER", 0, 1, 0),
        R.xml.kbd_12keys_flick_123,
        CompositionMode.HALF_ASCII,
        SpecialRomanjiTable.FLICK_TO_NUMBER,
        SpaceOnAlphanumeric.COMMIT,
        false,
        CrossingEdgeBehavior.COMMIT_WITHOUT_CONSUMING),

    TWELVE_KEY_TOGGLE_FLICK_KANA(
        new KeyboardSpecificationName("TWELVE_KEY_TOGGLE_FLICK_KANA", 0, 1, 2),
        R.xml.kbd_12keys_flick_kana,
        CompositionMode.HIRAGANA,
        SpecialRomanjiTable.TOGGLE_FLICK_TO_HIRAGANA,
        SpaceOnAlphanumeric.SPACE_OR_CONVERT_KEEPING_COMPOSITION,
        true,
        CrossingEdgeBehavior.DO_NOTHING),

    TWELVE_KEY_TOGGLE_FLICK_ALPHABET(
        new KeyboardSpecificationName("TWELVE_KEY_TOGGLE_FLICK_ALPHABET", 0, 1, 0),
        R.xml.kbd_12keys_flick_abc,
        CompositionMode.HALF_ASCII,
        SpecialRomanjiTable.TOGGLE_FLICK_TO_HALFWIDTHASCII,
        SpaceOnAlphanumeric.COMMIT,
        false,
        CrossingEdgeBehavior.DO_NOTHING),

    TWELVE_KEY_TOGGLE_FLICK_NUMBER(
        new KeyboardSpecificationName("TWELVE_KEY_TOGGLE_FLICK_ALPHABET", 0, 1, 0),
        R.xml.kbd_12keys_flick_123,
        CompositionMode.HALF_ASCII,
        SpecialRomanjiTable.TOGGLE_FLICK_TO_NUMBER,
        SpaceOnAlphanumeric.COMMIT,
        false,
        CrossingEdgeBehavior.DO_NOTHING),

    // QWERTY keyboard.
    QWERTY_KANA(
        new KeyboardSpecificationName("QWERTY_KANA", 0, 3, 0),
        R.xml.kbd_qwerty_kana,
        CompositionMode.HIRAGANA,
        SpecialRomanjiTable.QWERTY_MOBILE_TO_HIRAGANA,
        SpaceOnAlphanumeric.SPACE_OR_CONVERT_KEEPING_COMPOSITION,
        false,
        CrossingEdgeBehavior.DO_NOTHING),

    QWERTY_KANA_NUMBER(
        new KeyboardSpecificationName("QWERTY_KANA_NUMBER", 0, 2, 0),
        R.xml.kbd_qwerty_kana_123,
        CompositionMode.HIRAGANA,
        SpecialRomanjiTable.QWERTY_MOBILE_TO_HIRAGANA_NUMBER,
        SpaceOnAlphanumeric.SPACE_OR_CONVERT_KEEPING_COMPOSITION,
        false,
        CrossingEdgeBehavior.DO_NOTHING),

    QWERTY_ALPHABET(
        new KeyboardSpecificationName("QWERTY_ALPHABET", 0, 3, 0),
        R.xml.kbd_qwerty_abc,
        CompositionMode.HALF_ASCII,
        SpecialRomanjiTable.QWERTY_MOBILE_TO_HALFWIDTHASCII,
        SpaceOnAlphanumeric.COMMIT,
        false,
        CrossingEdgeBehavior.COMMIT_WITHOUT_CONSUMING),

    QWERTY_ALPHABET_NUMBER(
        new KeyboardSpecificationName("QWERTY_ALPHABET_NUMBER", 0, 2, 0),
        R.xml.kbd_qwerty_abc_123,
        CompositionMode.HALF_ASCII,
        SpecialRomanjiTable.QWERTY_MOBILE_TO_HALFWIDTHASCII,
        SpaceOnAlphanumeric.COMMIT,
        false,
        CrossingEdgeBehavior.COMMIT_WITHOUT_CONSUMING),

    // Godan keyboard.
    GODAN_KANA(
        new KeyboardSpecificationName("GODAN_KANA", 0, 1, 0),
        R.xml.kbd_godan_kana,
        CompositionMode.HIRAGANA,
        SpecialRomanjiTable.GODAN_TO_HIRAGANA,
        SpaceOnAlphanumeric.SPACE_OR_CONVERT_KEEPING_COMPOSITION,
        true,
        CrossingEdgeBehavior.COMMIT_WITHOUT_CONSUMING),

    // HARDWARE QWERTY keyboard.
    HARDWARE_QWERTY_KANA(
        new KeyboardSpecificationName("HARDWARE_QWERTY_KANA", 0, 1, 0),
        0,
        CompositionMode.HIRAGANA,
        SpecialRomanjiTable.DEFAULT_TABLE,
        SpaceOnAlphanumeric.SPACE_OR_CONVERT_KEEPING_COMPOSITION,
        false,
        CrossingEdgeBehavior.DO_NOTHING),

    HARDWARE_QWERTY_ALPHABET(
        new KeyboardSpecificationName("HARDWARE_QWERTY_ALPHABET", 0, 1, 0),
        0,
        CompositionMode.HALF_ASCII,
        SpecialRomanjiTable.DEFAULT_TABLE,
        SpaceOnAlphanumeric.COMMIT,
        false,
        CrossingEdgeBehavior.DO_NOTHING),

    ;

    private final KeyboardSpecificationName specName;
    private final int resourceId;
    private final CompositionMode compositionMode;
    private final SpecialRomanjiTable specialRomanjiTable;
    private final SpaceOnAlphanumeric spaceOnAlphanumeric;
    private final boolean kanaModifierInsensitiveConversion;
    private final CrossingEdgeBehavior crossingEdgeBehavior;

    private KeyboardSpecification(
        KeyboardSpecificationName specName,
        int resourceId,
        CompositionMode compositionMode,
        SpecialRomanjiTable specialRomanjiTable,
        SpaceOnAlphanumeric spaceOnAlphanumeric,
        boolean kanaModifierInsensitiveConversion,
        CrossingEdgeBehavior crossingEdgeBehavior) {
      this.specName = specName;
      this.resourceId = resourceId;
      this.compositionMode = compositionMode;
      this.specialRomanjiTable = specialRomanjiTable;
      this.spaceOnAlphanumeric = spaceOnAlphanumeric;
      this.kanaModifierInsensitiveConversion = kanaModifierInsensitiveConversion;
      this.crossingEdgeBehavior = crossingEdgeBehavior;
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

    public KeyboardSpecificationName getSpecName() {
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
  }

  private final KeyboardSpecification specification;

  public JapaneseKeyboard(
      List<Row> rowList, float flickThreshold, KeyboardSpecification specification) {
    super(rowList, flickThreshold);
    if (specification == null) {
      throw new NullPointerException("specification is null.");
    }
    this.specification = specification;
  }

  public KeyboardSpecification getSpecification() {
    return specification;
  }
}
