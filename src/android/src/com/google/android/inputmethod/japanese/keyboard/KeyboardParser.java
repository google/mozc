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

import org.mozc.android.inputmethod.japanese.keyboard.BackgroundDrawableFactory.DrawableType;
import org.mozc.android.inputmethod.japanese.keyboard.Key.Stick;
import org.mozc.android.inputmethod.japanese.resources.R;

import android.content.res.Resources;
import android.content.res.TypedArray;
import android.content.res.XmlResourceParser;
import android.util.DisplayMetrics;
import android.util.TypedValue;

import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.EnumSet;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 */
public class KeyboardParser {

  /** Attributes for the key dimensions. */
  private static class KeyAttributes {
    final int width;
    final int height;
    final int horizontalGap;
    final int verticalGap;
    DrawableType keyBackgroundDrawableType;

    KeyAttributes(int width, int height, int horizontalGap, int verticalGap,
                 DrawableType keyBackgroundDrawableType) {
      this.width = width;
      this.height = height;
      this.horizontalGap = horizontalGap;
      this.verticalGap = verticalGap;
      this.keyBackgroundDrawableType = keyBackgroundDrawableType;
    }
  }

  /** Attributes for the popup dimensions. */
  private static class PopUpAttributes {
    final int popUpWidth;
    final int popUpHeight;
    final int popUpXOffset;
    final int popUpYOffset;

    PopUpAttributes(int popUpWidth, int popUpHeight, int popUpXOffset, int popUpYOffset) {
      this.popUpWidth = popUpWidth;
      this.popUpHeight = popUpHeight;
      this.popUpXOffset = popUpXOffset;
      this.popUpYOffset = popUpYOffset;
    }
  }

  /*
   * Following codes are the list of attributes which a particular element can be support.
   * Note that, although it seems undocumented, the order of values in int[] attributes array
   * needs to be sorted. Considering the maintenance, we list attributes as we like,
   * sort in static block, and initialize the XXX_INDEX by binarySearch the value,
   * to keep the correct behavior regardless of editing the attr.xml file in future.
   */

  /** The attributes for a {@code <Row>} element. */
  private static final int[] ROW_ATTRIBUTES = {
    R.attr.verticalGap,
    R.attr.keyHeight,
    R.attr.rowEdgeFlags,
  };
  static {
    Arrays.sort(ROW_ATTRIBUTES);
  }
  private static final int ROW_VERTICAL_GAP_INDEX =
      Arrays.binarySearch(ROW_ATTRIBUTES, R.attr.verticalGap);
  private static final int ROW_KEY_HEIGHT_INDEX =
      Arrays.binarySearch(ROW_ATTRIBUTES, R.attr.keyHeight);
  private static final int ROW_ROW_EDGE_FLAGS_INDEX =
      Arrays.binarySearch(ROW_ATTRIBUTES, R.attr.rowEdgeFlags);

  /** Attributes for a {@code <Key>} element. */
  private static final int[] KEY_ATTRIBUTES = {
    R.attr.keyWidth,
    R.attr.keyHeight,
    R.attr.horizontalGap,
    R.attr.keyBackground,
    R.attr.keyEdgeFlags,
    R.attr.isRepeatable,
    R.attr.isModifier,
    R.attr.isSticky,
  };
  static {
    Arrays.sort(KEY_ATTRIBUTES);
  }
  private static final int KEY_KEY_WIDTH_INDEX =
      Arrays.binarySearch(KEY_ATTRIBUTES, R.attr.keyWidth);
  private static final int KEY_KEY_HEIGHT_INDEX =
      Arrays.binarySearch(KEY_ATTRIBUTES, R.attr.keyHeight);
  private static final int KEY_HORIZONTAL_GAP_INDEX =
      Arrays.binarySearch(KEY_ATTRIBUTES, R.attr.horizontalGap);
  private static final int KEY_KEY_BACKGROUND_INDEX =
      Arrays.binarySearch(KEY_ATTRIBUTES, R.attr.keyBackground);
  private static final int KEY_KEY_EDGE_FLAGS_INDEX =
      Arrays.binarySearch(KEY_ATTRIBUTES, R.attr.keyEdgeFlags);
  private static final int KEY_IS_REPEATABLE_INDEX =
      Arrays.binarySearch(KEY_ATTRIBUTES, R.attr.isRepeatable);
  private static final int KEY_IS_MODIFIER_INDEX =
      Arrays.binarySearch(KEY_ATTRIBUTES, R.attr.isModifier);
  private static final int KEY_IS_STICKY_INDEX =
      Arrays.binarySearch(KEY_ATTRIBUTES, R.attr.isSticky);

  /** Attributes for a {@code <Spacer>} element. */
  private static final int[] SPACER_ATTRIBUTES = {
    R.attr.keyHeight,
    R.attr.horizontalGap,
    R.attr.keyEdgeFlags,
    R.attr.stick,
  };
  static {
    Arrays.sort(SPACER_ATTRIBUTES);
  }
  private static final int SPACER_KEY_HEIGHT_INDEX =
      Arrays.binarySearch(SPACER_ATTRIBUTES, R.attr.keyHeight);
  private static final int SPACER_HORIZONTAL_GAP_INDEX =
      Arrays.binarySearch(SPACER_ATTRIBUTES, R.attr.horizontalGap);
  private static final int SPACER_KEY_EDGE_FLAGS_INDEX =
      Arrays.binarySearch(SPACER_ATTRIBUTES, R.attr.keyEdgeFlags);
  private static final int SPACER_STICK_INDEX =
      Arrays.binarySearch(SPACER_ATTRIBUTES, R.attr.stick);

  /** Attributes for a {@code <KeyState>} element. */
  private static final int[] KEY_STATE_ATTRIBUTES = {
    R.attr.keyBackground,
    R.attr.metaState,
    R.attr.nextMetaState,
  };
  static {
    Arrays.sort(KEY_STATE_ATTRIBUTES);
  }
  private static final int KEY_STATE_KEY_BACKGROUND_INDEX =
      Arrays.binarySearch(KEY_STATE_ATTRIBUTES, R.attr.keyBackground);
  private static final int KEY_STATE_META_STATE_INDEX =
      Arrays.binarySearch(KEY_STATE_ATTRIBUTES, R.attr.metaState);
  private static final int KEY_STATE_NEXT_META_STATE_INDEX =
      Arrays.binarySearch(KEY_STATE_ATTRIBUTES, R.attr.nextMetaState);

  /** Attributes for a {@code <KeyEntity>} element. */
  private static final int[] KEY_ENTITY_ATTRIBUTES = {
    R.attr.sourceId,
    R.attr.keyCode,
    R.attr.longPressKeyCode,
    R.attr.keyIcon,
    R.attr.keyCharacter,
    R.attr.flickHighlight,
  };
  static {
    Arrays.sort(KEY_ENTITY_ATTRIBUTES);
  }
  private static final int KEY_ENTITY_SOURCE_ID_INDEX =
      Arrays.binarySearch(KEY_ENTITY_ATTRIBUTES, R.attr.sourceId);
  private static final int KEY_ENTITY_KEY_CODE_INDEX =
      Arrays.binarySearch(KEY_ENTITY_ATTRIBUTES, R.attr.keyCode);
  private static final int KEY_ENTITY_LONG_PRESS_KEY_CODE_INDEX =
      Arrays.binarySearch(KEY_ENTITY_ATTRIBUTES, R.attr.longPressKeyCode);
  private static final int KEY_ENTITY_KEY_ICON_INDEX =
      Arrays.binarySearch(KEY_ENTITY_ATTRIBUTES, R.attr.keyIcon);
  private static final int KEY_ENTITY_KEY_CHAR_INDEX =
      Arrays.binarySearch(KEY_ENTITY_ATTRIBUTES, R.attr.keyCharacter);
  private static final int KEY_ENTITY_FLICK_HIGHLIGHT_INDEX =
      Arrays.binarySearch(KEY_ENTITY_ATTRIBUTES, R.attr.flickHighlight);

  /**
   * Mapping table from enum value in xml to DrawableType by using the enum value as index.
   */
  private static final DrawableType[] KEY_BACKGROUND_DRAWABLE_TYPE_MAP = {
    DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND,
    DrawableType.TWELVEKEYS_FUNCTION_KEY_BACKGROUND,
    DrawableType.QWERTY_REGULAR_KEY_BACKGROUND,
    DrawableType.QWERTY_FUNCTION_KEY_BACKGROUND,
    DrawableType.QWERTY_FUNCTION_KEY_LIGHT_ON_BACKGROUND,
    DrawableType.QWERTY_FUNCTION_KEY_LIGHT_OFF_BACKGROUND,
  };

  /**
   * @return "sourceId" assigned to {@code value}.
   */
  static int getSourceId(TypedValue value, int defaultValue) {
    if (value == null ||
        (value.type != TypedValue.TYPE_INT_DEC &&
         value.type != TypedValue.TYPE_INT_HEX)) {
      throw new IllegalArgumentException("sourceId is mandatory for KeyEntity.");
    }
    return value.data;
  }

  /**
   * @return the pixel offsets based on metrics and base
   */
  static int getDimensionOrFraction(
      TypedValue value, int base, int defaultValue, DisplayMetrics metrics) {
    if (value == null) {
      return defaultValue;
    }

    switch (value.type) {
      case TypedValue.TYPE_DIMENSION:
        return TypedValue.complexToDimensionPixelOffset(value.data, metrics);
      case TypedValue.TYPE_FRACTION:
        return Math.round(TypedValue.complexToFraction(value.data, base, base));
    }

    throw new IllegalArgumentException("The type dimension or fraction is required." +
                                       "  value = " + value.toString());
  }

  /**
   * @return "codes" assigned to {@code value}
   */
  static int getCode(TypedValue value, int defaultValue) {
    if (value == null) {
      return defaultValue;
    }

    if (value.type == TypedValue.TYPE_INT_DEC ||
        value.type == TypedValue.TYPE_INT_HEX) {
      return value.data;
    }
    if (value.type == TypedValue.TYPE_STRING) {
      return Integer.parseInt(value.string.toString());
    }

    return defaultValue;
  }

  /**
   * A simple wrapper of {@link CharSequence#toString()}, in order to avoid
   * {@code NullPointerException}.
   * @param sequence input character sequence
   * @return {@code sequence.toString()} if {@code sequence} is not {@code null}.
   *         Otherwise, {@code null}.
   */
  static String toStringOrNull(CharSequence sequence) {
    if (sequence == null) {
      return null;
    }
    return sequence.toString();
  }

  private static void ignoreWhiteSpaceAndComment(XmlPullParser parser)
      throws XmlPullParserException, IOException {
    int event = parser.getEventType();
    while (event == XmlPullParser.IGNORABLE_WHITESPACE || event == XmlPullParser.COMMENT) {
      event = parser.next();
    }
  }

  private static void assertStartDocument(XmlPullParser parser) throws XmlPullParserException {
    if (parser.getEventType() != XmlPullParser.START_DOCUMENT) {
      throw new IllegalArgumentException(
          "The start of document is expected, but actually not: " +
          parser.getPositionDescription());
    }
  }

  private static void assertEndDocument(XmlPullParser parser) throws XmlPullParserException {
    if (parser.getEventType() != XmlPullParser.END_DOCUMENT) {
      throw new IllegalArgumentException(
          "The end of document is expected, but actually not: " + parser.getPositionDescription());
    }
  }

  private static void assertNotEndDocument(XmlPullParser parser) throws XmlPullParserException {
    if (parser.getEventType() == XmlPullParser.END_DOCUMENT) {
      throw new IllegalArgumentException(
          "Unexpected end of document is found: " + parser.getPositionDescription());
    }
  }

  private static void assertTagName(XmlPullParser parser, String expectedName) {
    String actualName = parser.getName();
    if (!actualName.equals(expectedName)) {
      throw new IllegalArgumentException(
          "Tag <" + expectedName + "> is expected, but found <" + actualName + ">: " +
          parser.getPositionDescription());
    }
  }

  private static void assertStartTag(XmlPullParser parser, String expectedName)
      throws XmlPullParserException {
    if (parser.getEventType() != XmlPullParser.START_TAG) {
      throw new IllegalArgumentException(
          "Start tag <" + expectedName + "> is expected: " + parser.getPositionDescription());
    }
    assertTagName(parser, expectedName);
  }

  private static void assertEndTag(XmlPullParser parser, String expectedName)
      throws XmlPullParserException {
    if (parser.getEventType() != XmlPullParser.END_TAG) {
      throw new IllegalArgumentException(
          "End tag </" + expectedName + "> is expected: " + parser.getPositionDescription());
    }
    assertTagName(parser, expectedName);
  }

  private final Resources resources;
  private final XmlResourceParser xmlResourceParser;
  private final Set<Integer> sourceIdSet = new HashSet<Integer>();
  private final int keyboardWidth;
  private final int keyboardHeight;

  public KeyboardParser(Resources resources, XmlResourceParser xmlResourceParser,
                        int keyboardWidth, int keyboardHeight) {
    if (resources == null) {
      throw new NullPointerException("resources shouldn't be null.");
    }
    if (xmlResourceParser == null) {
      throw new NullPointerException("xmlResourceParser shouldn't be null.");
    }

    this.resources = resources;
    this.xmlResourceParser = xmlResourceParser;
    this.keyboardWidth = keyboardWidth;
    this.keyboardHeight = keyboardHeight;
  }

  /**
   * Parses a XML resource and returns a Keyboard instance.
   */
  public Keyboard parseKeyboard() throws XmlPullParserException, IOException {
    // TODO(hidehiko): Refactor by spliting this method into two layers,
    //                 one is parse Keyboard element, and another is parsing a full xml file.
    XmlResourceParser parser = this.xmlResourceParser;

    // Initial two events should be START_DOCUMENT and then START_TAG.
    parser.next();
    assertStartDocument(parser);

    // The root element should be "Keyboard".
    parser.next();
    assertStartTag(parser, "Keyboard");

    // To clean the code, we probably should make a class which holds attributes of each
    // element in this method and following parseXxx methods, e.g.;
    //
    // private Attributes parseAttributes(int[] styles) {
    //   TypedArray attributes = ...
    //   try {
    //     Type1 var1 = ...;  // Read from attributes.
    //     Type2 var2 = ...;
    //     return new Attribute(var1, var2, ...);
    //   } finally {
    //     attributes.recycle();
    //   }
    // }
    //
    // However, it turned out that the code consumed the time a lot. So, considering the
    // situation, we'll use the following style in parseXxx methods in this class:
    //
    //    Type1 var1;
    //    Type2 var2;
    //    {
    //      TypedArray attributes = ...;
    //      try {
    //         // initialization of var1, var2, ...
    //      } finally {
    //        attributes.recycle();
    //      }
    //    }
    //    // Here, we can use var1 and var2 as these are initialized.
    //
    // By this code, the consumed time of parseKeyboard method is reduced by about 40%.
    // So, it should be beneficial enough.
    KeyAttributes keyAttributes;
    PopUpAttributes popUpAttributes;
    float flickThreshold;
    {
      TypedArray attributes = resources.obtainAttributes(parser, R.styleable.Keyboard);
      try {
        DisplayMetrics metrics = resources.getDisplayMetrics();
        // The default keyWidth is 10% of the display for width, and 50px for height.
        keyAttributes = parseKeyAttributes(
            attributes,
            new KeyAttributes(keyboardWidth / 10, 50, 0, 0, null),
            metrics,
            this.keyboardWidth,
            this.keyboardHeight,
            R.styleable.Keyboard_keyWidth,
            R.styleable.Keyboard_keyHeight,
            R.styleable.Keyboard_horizontalGap,
            R.styleable.Keyboard_verticalGap,
            R.styleable.Keyboard_keyBackground);
        popUpAttributes = parsePopUpAttributes(
            attributes,
            metrics,
            this.keyboardWidth,
            R.styleable.Keyboard_popUpWidth,
            R.styleable.Keyboard_popUpHeight,
            R.styleable.Keyboard_popUpXOffset,
            R.styleable.Keyboard_popUpYOffset);
        flickThreshold = parseFlickThreshold(
            attributes, R.styleable.Keyboard_flickThreshold);
      } finally {
        attributes.recycle();
      }
    }

    List<Row> rowList = new ArrayList<Row>();
    int y = 0;
    while (true) {
      parser.next();
      ignoreWhiteSpaceAndComment(parser);
      assertNotEndDocument(parser);
      if (parser.getEventType() == XmlResourceParser.END_TAG) {
        break;
      }
      Row row = parseRow(y, keyAttributes, popUpAttributes);
      rowList.add(row);
      y += row.getHeight() + row.getVerticalGap();
    }
    assertEndTag(parser, "Keyboard");

    // Meke sure the end of document.
    parser.next();
    ignoreWhiteSpaceAndComment(parser);
    assertEndDocument(parser);

    return buildKeyboard(rowList, flickThreshold);
  }

  /**
   * Parses a {@code Row} element, and returns a {@code Row} instance
   * containing a list of {@code Key}s.
   */
  private Row parseRow(
      int y, KeyAttributes defaultKeyAttributes, PopUpAttributes popUpAttributes)
      throws XmlPullParserException, IOException {
    XmlResourceParser parser = this.xmlResourceParser;
    assertStartTag(parser, "Row");

    int verticalGap;
    int rowHeight;
    int edgeFlags;
    {
      DisplayMetrics metrics = resources.getDisplayMetrics();
      TypedArray attributes = resources.obtainAttributes(parser, ROW_ATTRIBUTES);
      try {
        verticalGap = getDimensionOrFraction(
            attributes.peekValue(ROW_VERTICAL_GAP_INDEX),
            keyboardHeight, defaultKeyAttributes.verticalGap, metrics);
        rowHeight = getDimensionOrFraction(
            attributes.peekValue(ROW_KEY_HEIGHT_INDEX), keyboardHeight,
            defaultKeyAttributes.height, metrics);
        edgeFlags = attributes.getInt(ROW_ROW_EDGE_FLAGS_INDEX, 0);
      } finally {
        attributes.recycle();
      }
    }

    List<Key> keyList = new ArrayList<Key>();
    int x = 0;
    while (true) {
      parser.next();
      ignoreWhiteSpaceAndComment(parser);
      assertNotEndDocument(parser);
      if (parser.getEventType() == XmlResourceParser.END_TAG) {
        break;
      }
      if ("Key".equals(parser.getName())) {
        Key key = parseKey(x, y, edgeFlags, defaultKeyAttributes, popUpAttributes);
        keyList.add(key);
        x += key.getWidth();
      } else if ("Spacer".equals(parser.getName())) {
        Key key = parseSpacer(x, y, edgeFlags, defaultKeyAttributes);
        keyList.add(key);
        x += key.getWidth();
      }
    }

    assertEndTag(parser, "Row");
    return buildRow(keyList, rowHeight, verticalGap);
  }

  /**
   * Parses a {@code Key} element, and returns an instance.
   */
  private Key parseKey(int x, int y, int edgeFlags,
                       KeyAttributes defaultKeyAttributes, PopUpAttributes popUpAttributes)
      throws XmlPullParserException, IOException {
    XmlResourceParser parser = this.xmlResourceParser;
    assertStartTag(parser, "Key");

    KeyAttributes keyAttributes;
    boolean isRepeatable;
    boolean isModifier;
    boolean isSticky;
    {
      TypedArray attributes = resources.obtainAttributes(parser, KEY_ATTRIBUTES);
      try {
        DisplayMetrics metrics = resources.getDisplayMetrics();
        keyAttributes = parseKeyAttributes(
            attributes, defaultKeyAttributes, metrics, keyboardWidth, keyboardHeight,
            KEY_KEY_WIDTH_INDEX, KEY_KEY_HEIGHT_INDEX, KEY_HORIZONTAL_GAP_INDEX, -1,
            KEY_KEY_BACKGROUND_INDEX);
        edgeFlags |= attributes.getInt(KEY_KEY_EDGE_FLAGS_INDEX, 0);
        isRepeatable = attributes.getBoolean(KEY_IS_REPEATABLE_INDEX, false);
        isModifier = attributes.getBoolean(KEY_IS_MODIFIER_INDEX, false);
        isSticky = attributes.getBoolean(KEY_IS_STICKY_INDEX, false);
      } finally {
        attributes.recycle();
      }
    }

    List<KeyState> keyStateList = new ArrayList<KeyState>();
    while (true) {
      parser.next();
      ignoreWhiteSpaceAndComment(parser);
      assertNotEndDocument(parser);
      if (parser.getEventType() == XmlResourceParser.END_TAG) {
        break;
      }

      keyStateList.add(
          parseKeyState(keyAttributes.keyBackgroundDrawableType, popUpAttributes));
    }

    // At the moment, we just accept keys which has unmodified state.
    boolean hasUnmodified = false;
    boolean hasLongPressKeyCode = false;
    for (KeyState keyState : keyStateList) {
      if (keyState.getMetaStateSet().contains(KeyState.MetaState.UNMODIFIED)) {
        hasUnmodified = true;
        if (keyState.getFlick(Flick.Direction.CENTER).getKeyEntity().getLongPressKeyCode() !=
            KeyEntity.INVALID_KEY_CODE) {
          hasLongPressKeyCode = true;
        }
        break;
      }
    }
    if (!hasUnmodified) {
      throw new IllegalArgumentException(
          "No unmodified KeyState element is found: " + parser.getPositionDescription());
    }

    if (isRepeatable && hasLongPressKeyCode) {
      throw new IllegalArgumentException(
          "The key has both isRepeatable attribute and longPressKeyCode: " +
          parser.getPositionDescription());
    }

    assertEndTag(parser, "Key");
    return new Key(
        x, y, keyAttributes.width, keyAttributes.height, keyAttributes.horizontalGap,
        edgeFlags, isRepeatable, isModifier, isSticky, Stick.EVEN, keyStateList);
  }

  /**
   * Parses a {@code Spacer} element, and returns an instance.
   */
  private Key parseSpacer(int x, int y, int edgeFlags, KeyAttributes defaultKeyAttributes)
      throws XmlPullParserException, IOException {
    XmlResourceParser parser = this.xmlResourceParser;
    DisplayMetrics metrics = resources.getDisplayMetrics();
    assertStartTag(parser, "Spacer");

    KeyAttributes keyAttributes;
    Stick stick;
    {
      TypedArray attributes = resources.obtainAttributes(parser, SPACER_ATTRIBUTES);
      try {
        keyAttributes = parseKeyAttributes(
            attributes, defaultKeyAttributes, metrics, keyboardWidth, keyboardHeight,
            -1, SPACER_KEY_HEIGHT_INDEX, SPACER_HORIZONTAL_GAP_INDEX, -1, -1);
        edgeFlags |= attributes.getInt(SPACER_KEY_EDGE_FLAGS_INDEX, 0);
        stick = Stick.values()[attributes.getInt(SPACER_STICK_INDEX, 0)];
      } finally {
        attributes.recycle();
      }
    }

    parser.next();
    assertEndTag(parser, "Spacer");

    // Returns a dummy key object.
    return new Key(
        x, y, keyAttributes.horizontalGap, keyAttributes.height,
        0, edgeFlags, false, false, false, stick, Collections.<KeyState>emptyList());
  }

  private KeyState parseKeyState(DrawableType defaultBackgroundDrawableType,
                                 PopUpAttributes popUpAttributes)
      throws XmlPullParserException, IOException {
    XmlResourceParser parser = this.xmlResourceParser;
    assertStartTag(parser, "KeyState");

    DrawableType backgroundDrawableType;
    Set<KeyState.MetaState> metaStateSet;
    KeyState.MetaState nextMetaState;
    {
      TypedArray attributes = resources.obtainAttributes(parser, KEY_STATE_ATTRIBUTES);
      try {
        backgroundDrawableType = parseKeyBackgroundDrawableType(
            attributes, KEY_STATE_KEY_BACKGROUND_INDEX, defaultBackgroundDrawableType);
        metaStateSet = parseMetaState(attributes, KEY_STATE_META_STATE_INDEX);
        if (metaStateSet.isEmpty()) {
          throw new AssertionError("MetaState should be found.");
        }
        nextMetaState = parseNextMetaState(attributes, KEY_STATE_NEXT_META_STATE_INDEX);
      } finally {
        attributes.recycle();
      }
    }

    List<Flick> flickList = new ArrayList<Flick>();
    while (true) {
      parser.next();
      ignoreWhiteSpaceAndComment(parser);
      assertNotEndDocument(parser);
      if (parser.getEventType() == XmlResourceParser.END_TAG) {
        break;
      }
      flickList.add(parseFlick(backgroundDrawableType, popUpAttributes));
    }

    // At the moment, we support only keys which has flick data to the CENTER direction.
    boolean isCenterFound = false;
    for (Flick flick : flickList) {
      if (flick.getDirection() == Flick.Direction.CENTER) {
        isCenterFound = true;
        break;
      }
    }
    if (!isCenterFound) {
      throw new IllegalArgumentException(
          "No CENTER flick element is found: " + parser.getPositionDescription());
    }

    assertEndTag(parser, "KeyState");
    return new KeyState(metaStateSet, nextMetaState, flickList);
  }

  private Flick parseFlick(DrawableType backgroundDrawableType,
                           PopUpAttributes popUpAttributes)
      throws XmlPullParserException, IOException {
    XmlResourceParser parser = this.xmlResourceParser;
    assertStartTag(parser, "Flick");

    Flick.Direction direction;
    {
      TypedArray attributes = resources.obtainAttributes(parser, R.styleable.Flick);
      try {
        direction = parseFlickDirection(attributes, R.styleable.Flick_direction);
      } finally {
        attributes.recycle();
      }
    }

    parser.next();
    KeyEntity entity = parseKeyEntity(backgroundDrawableType, popUpAttributes);

    if (entity.getLongPressKeyCode() != KeyEntity.INVALID_KEY_CODE &&
        direction != Flick.Direction.CENTER) {
      throw new IllegalArgumentException(
          "longPressKeyCode can be set to only a KenEntity for CENTER direction: " +
          parser.getPositionDescription());
    }

    parser.next();
    assertEndTag(parser, "Flick");

    return new Flick(direction, entity);
  }

  private KeyEntity parseKeyEntity(
      DrawableType backgroundDrawableType, PopUpAttributes popUpAttributes)
      throws XmlPullParserException, IOException {
    XmlResourceParser parser = this.xmlResourceParser;
    assertStartTag(parser, "KeyEntity");

    int sourceId;
    int keyCode;
    int longPressKeyCode;
    int keyIconResourceId;
    String keyCharacter;
    DrawableType keyBackgroundDrawableType;
    boolean flickHighlight;
    {
      TypedArray attributes = resources.obtainAttributes(parser, KEY_ENTITY_ATTRIBUTES);
      try {
        sourceId = getSourceId(attributes.peekValue(KEY_ENTITY_SOURCE_ID_INDEX), 0);
        if (!sourceIdSet.add(sourceId)) {
          //  Same sourceId is found.
          throw new IllegalArgumentException(
              "Duplicataed sourceId is found: " + xmlResourceParser.getPositionDescription());
        }
        keyCode = getCode(
            attributes.peekValue(KEY_ENTITY_KEY_CODE_INDEX), KeyEntity.INVALID_KEY_CODE);
        longPressKeyCode = getCode(attributes.peekValue(KEY_ENTITY_LONG_PRESS_KEY_CODE_INDEX),
                                   KeyEntity.INVALID_KEY_CODE);
        keyIconResourceId = attributes.getResourceId(KEY_ENTITY_KEY_ICON_INDEX, 0);
        keyCharacter = attributes.getString(KEY_ENTITY_KEY_CHAR_INDEX);
        flickHighlight = attributes.getBoolean(KEY_ENTITY_FLICK_HIGHLIGHT_INDEX, false);
      } finally {
        attributes.recycle();
      }
    }

    parser.next();
    ignoreWhiteSpaceAndComment(parser);

    PopUp popUp = null;
    if (parser.getEventType() == XmlResourceParser.START_TAG) {
      popUp = parsePopUp(popUpAttributes);
      parser.next();
      ignoreWhiteSpaceAndComment(parser);
    }

    assertEndTag(parser, "KeyEntity");

    return new KeyEntity(sourceId, keyCode, longPressKeyCode, keyIconResourceId, keyCharacter,
                         backgroundDrawableType, flickHighlight, popUp);
  }

  private PopUp parsePopUp(PopUpAttributes popUpAttributes)
      throws XmlPullParserException, IOException {
    XmlResourceParser parser = this.xmlResourceParser;
    assertStartTag(parser, "PopUp");

    int popUpIconResourceId;
    {
      TypedArray attributes = resources.obtainAttributes(parser, R.styleable.PopUp);
      try {
        popUpIconResourceId = attributes.getResourceId(R.styleable.PopUp_popUpIcon, 0);
      } finally {
        attributes.recycle();
      }
    }
    parser.next();
    assertEndTag(parser, "PopUp");

    return new PopUp(popUpIconResourceId,
                     popUpAttributes.popUpWidth,
                     popUpAttributes.popUpHeight,
                     popUpAttributes.popUpXOffset,
                     popUpAttributes.popUpYOffset);
  }

  private float parseFlickThreshold(TypedArray attributes, int index) {
    float flickThreshold = attributes.getDimension(
        index, resources.getDimension(R.dimen.default_flick_threshold));
    if (flickThreshold <= 0) {
      throw new IllegalArgumentException(
          "flickThreshold must be greater than 0.  value = " + flickThreshold);
    }
    return flickThreshold;
  }

  private static KeyAttributes parseKeyAttributes(
      TypedArray attributes, KeyAttributes defaultValue, DisplayMetrics metrics,
      int keyboardWidth, int keyboardHeight,
      int keyWidthIndex, int keyHeightIndex, int horizontalGapIndex, int verticalGapIndex,
      int keyBackgroundIndex) {
    int keyWidth = (keyWidthIndex >= 0)
        ? getDimensionOrFraction(
              attributes.peekValue(keyWidthIndex), keyboardWidth,
              defaultValue.width, metrics)
        : defaultValue.width;
    int keyHeight = (keyHeightIndex >= 0)
        ? getDimensionOrFraction(
              attributes.peekValue(keyHeightIndex), keyboardHeight,
              defaultValue.height, metrics)
        : defaultValue.height;

    int horizontalGap = (horizontalGapIndex >= 0)
        ? getDimensionOrFraction(
              attributes.peekValue(horizontalGapIndex),
              keyboardWidth, defaultValue.horizontalGap, metrics)
        : defaultValue.horizontalGap;
    int verticalGap = (verticalGapIndex >= 0)
        ? getDimensionOrFraction(
              attributes.peekValue(verticalGapIndex),
              keyboardHeight, defaultValue.verticalGap, metrics)
        : defaultValue.verticalGap;

    DrawableType keyBackgroundDrawableType = parseKeyBackgroundDrawableType(
        attributes, keyBackgroundIndex, defaultValue.keyBackgroundDrawableType);
    return new KeyAttributes(
        keyWidth, keyHeight, horizontalGap, verticalGap, keyBackgroundDrawableType);
  }

  private static DrawableType parseKeyBackgroundDrawableType(
      TypedArray attributes, int index, DrawableType defaultValue) {
    if (index < 0) {
      return defaultValue;
    }
    int value = attributes.getInt(index, -1);
    return (value < 0) ? defaultValue : KEY_BACKGROUND_DRAWABLE_TYPE_MAP[value];
  }

  private static PopUpAttributes parsePopUpAttributes(
      TypedArray attributes, DisplayMetrics metrics, int keyboardWidth,
      int popUpWidthIndex, int popUpHeightIndex, int popUpXOffsetIndex, int popUpYOffsetIndex) {
    int popUpWidth = getDimensionOrFraction(
        attributes.peekValue(popUpWidthIndex), keyboardWidth, 0, metrics);
    int popUpHeight = getDimensionOrFraction(
        attributes.peekValue(popUpHeightIndex), keyboardWidth, 0, metrics);
    int popUpXOffset = getDimensionOrFraction(
        attributes.peekValue(popUpXOffsetIndex), keyboardWidth, 0, metrics);
    int popUpYOffset = getDimensionOrFraction(
        attributes.peekValue(popUpYOffsetIndex), keyboardWidth, 0, metrics);
    return new PopUpAttributes(popUpWidth, popUpHeight, popUpXOffset, popUpYOffset);
  }

  private Set<KeyState.MetaState> parseMetaState(TypedArray attributes, int index) {
    int metaStateFlags = attributes.getInt(index, 0);
    if (metaStateFlags == 0) {
      // Specialize unmodified state.
      return EnumSet.of(KeyState.MetaState.UNMODIFIED);
    }

    Set<KeyState.MetaState> result = EnumSet.noneOf(KeyState.MetaState.class);
    for (int i = 0; i < Integer.SIZE; ++i) {
      int flag = metaStateFlags & (1 << i);
      if (flag != 0) {
        result.add(KeyState.MetaState.valueOf(flag));
      }
    }
    return result;
  }

  private KeyState.MetaState parseNextMetaState(TypedArray attributes, int index) {
    return KeyState.MetaState.valueOf(attributes.getInt(index, 0));
  }

  private Flick.Direction parseFlickDirection(TypedArray attributes, int index) {
    return Flick.Direction.valueOf(attributes.getInt(index, Flick.Direction.CENTER.index));
  }

  protected Keyboard buildKeyboard(List<Row> rowList, float flickThreshold) {
    return new Keyboard(rowList, flickThreshold);
  }

  protected Row buildRow(List<Key> keyList, int height, int verticalGap) {
    return new Row(keyList, height, verticalGap);
  }
}
