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

import org.mozc.android.inputmethod.japanese.keyboard.BackgroundDrawableFactory.DrawableType;
import org.mozc.android.inputmethod.japanese.keyboard.Key.Stick;
import org.mozc.android.inputmethod.japanese.keyboard.Keyboard.KeyboardSpecification;
import org.mozc.android.inputmethod.japanese.resources.R;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.MoreObjects;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;

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
  @VisibleForTesting static class KeyAttributes {

    @VisibleForTesting static class Builder {

      private int width;
      private int height;
      private int horizontalLayoutWeight;
      private int horizontalGap;
      private int verticalGap;
      private int defaultIconWidth;
      private int defaultIconHeight;
      private int defaultHorizontalPadding;
      private int defaultVerticalPadding;
      private DrawableType keyBackgroundDrawableType =
          DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND;
      private int edgeFlags;
      private boolean isRepeatable;
      private boolean isModifier;
      private Stick stick = Stick.EVEN;
      private List<KeyState> keyStateList = Collections.<KeyState>emptyList();

      Builder setWidth(int width) {
        this.width = width;
        return this;
      }
      Builder setHeight(int height) {
        this.height = height;
        return this;
      }
      Builder setHorizontalLayoutWeight(int horizontalLayoutWeight) {
        this.horizontalLayoutWeight = horizontalLayoutWeight;
        return this;
      }
      Builder setHorizontalGap(int horizontalGap) {
        this.horizontalGap = horizontalGap;
        return this;
      }
      Builder setVerticalGap(int verticalGap) {
        this.verticalGap = verticalGap;
        return this;
      }
      Builder setDefaultIconWidth(int defaultIconWidth) {
        this.defaultIconWidth = defaultIconWidth;
        return this;
      }
      Builder setDefaultIconHeight(int defaultIconHeight) {
        this.defaultIconHeight = defaultIconHeight;
        return this;
      }
      Builder setDefaultHorizontalPadding(int defaultHorizontalPadding) {
        this.defaultHorizontalPadding = defaultHorizontalPadding;
        return this;
      }
      Builder setDefaultVerticalPadding(int defaultVerticalPadding) {
        this.defaultVerticalPadding = defaultVerticalPadding;
        return this;
      }
      Builder setKeybackgroundDrawableType(DrawableType type) {
        this.keyBackgroundDrawableType = Preconditions.checkNotNull(type);
        return this;
      }
      Builder setEdgeFlags(int edgeFlags) {
        this.edgeFlags = edgeFlags;
        return this;
      }
      Builder setRepeatable(boolean isRepeatable) {
        this.isRepeatable = isRepeatable;
        return this;
      }
      Builder setModifier(boolean isModifier) {
        this.isModifier = isModifier;
        return this;
      }
      Builder setStick(Stick stick) {
        this.stick = Preconditions.checkNotNull(stick);
        return this;
      }
      Builder setKeyStateList(List<KeyState> keyStateList) {
        this.keyStateList = Preconditions.checkNotNull(keyStateList);
        return this;
      }
      KeyAttributes build() {
        return new KeyAttributes(this);
      }
    }

    final int width;
    final int height;
    final int horizontalLayoutWeight;
    final int horizontalGap;
    final int verticalGap;
    final int defaultIconWidth;
    final int defaultIconHeight;
    final int defaultHorizontalPadding;
    final int defaultVerticalPadding;
    final DrawableType keyBackgroundDrawableType;
    final int edgeFlags;
    final boolean isRepeatable;
    final boolean isModifier;
    final Stick stick;
    final List<KeyState> keyStateList;

    private KeyAttributes(Builder builder) {
      this.width = builder.width;
      this.height = builder.height;
      this.horizontalLayoutWeight = builder.horizontalLayoutWeight;
      this.horizontalGap = builder.horizontalGap;
      this.verticalGap = builder.verticalGap;
      this.defaultIconWidth = builder.defaultIconWidth;
      this.defaultIconHeight = builder.defaultIconHeight;
      this.defaultHorizontalPadding = builder.defaultHorizontalPadding;
      this.defaultVerticalPadding = builder.defaultVerticalPadding;
      this.keyBackgroundDrawableType =
          Preconditions.checkNotNull(builder.keyBackgroundDrawableType);
      this.edgeFlags = builder.edgeFlags;
      this.isRepeatable = builder.isRepeatable;
      this.isModifier = builder.isModifier;
      this.stick = builder.stick;
      this.keyStateList = builder.keyStateList;
    }

    static Builder newBuilder() {
      return new Builder();
    }

    Builder toBuilder() {
      return newBuilder()
          .setWidth(width)
          .setHeight(height)
          .setHorizontalLayoutWeight(horizontalLayoutWeight)
          .setHorizontalGap(horizontalGap)
          .setVerticalGap(verticalGap)
          .setDefaultHorizontalPadding(defaultHorizontalPadding)
          .setDefaultVerticalPadding(defaultVerticalPadding)
          .setDefaultIconWidth(defaultIconWidth)
          .setDefaultIconHeight(defaultIconHeight)
          .setKeybackgroundDrawableType(keyBackgroundDrawableType)
          .setEdgeFlags(edgeFlags)
          .setRepeatable(isRepeatable)
          .setModifier(isModifier)
          .setStick(stick)
          .setKeyStateList(keyStateList);
    }

    Key buildKey(int x, int y, int width) {
      return new Key(x, y, width, height, horizontalGap, edgeFlags,
          isRepeatable, isModifier, stick, keyBackgroundDrawableType, keyStateList);
    }
  }

  /** Attributes for the popup dimensions. */
  private static class PopUpAttributes {

    final int popUpHeight;
    final int popUpXOffset;
    final int popUpYOffset;
    final int popUpIconWidth;
    final int popUpIconHeight;

    PopUpAttributes(int popUpHeight, int popUpXOffset, int popUpYOffset,
                    int popUpIconWidth, int popUpIconHeight) {
      this.popUpHeight = popUpHeight;
      this.popUpXOffset = popUpXOffset;
      this.popUpYOffset = popUpYOffset;
      this.popUpIconWidth = popUpIconWidth;
      this.popUpIconHeight = popUpIconHeight;
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

  private static final int[] POPUP_ATTRIBUTES = {
    R.attr.popUpIcon,
    R.attr.popUpLongPressIcon,
    R.attr.popUpHeight,
    R.attr.popUpXOffset,
    R.attr.popUpYOffset,
    R.attr.popUpIconWidth,
    R.attr.popUpIconHeight,
  };
  static {
    Arrays.sort(POPUP_ATTRIBUTES);
  }
  private static final int POPUP_KEY_ICON_INDEX =
      Arrays.binarySearch(POPUP_ATTRIBUTES, R.attr.popUpIcon);
  private static final int POPUP_KEY_LONG_PRESS_ICON_INDEX =
      Arrays.binarySearch(POPUP_ATTRIBUTES, R.attr.popUpLongPressIcon);
  private static final int POPUP_KEY_HEIGHT_INDEX =
      Arrays.binarySearch(POPUP_ATTRIBUTES, R.attr.popUpHeight);
  private static final int POPUP_KEY_X_OFFSET_INDEX =
      Arrays.binarySearch(POPUP_ATTRIBUTES, R.attr.popUpXOffset);
  private static final int POPUP_KEY_Y_OFFSET_INDEX =
      Arrays.binarySearch(POPUP_ATTRIBUTES, R.attr.popUpYOffset);
  private static final int POPUP_KEY_ICON_WIDTH_INDEX =
      Arrays.binarySearch(POPUP_ATTRIBUTES, R.attr.popUpIconWidth);
  private static final int POPUP_KEY_ICON_HEIGHT_INDEX =
      Arrays.binarySearch(POPUP_ATTRIBUTES, R.attr.popUpIconHeight);

  /** Attributes for a {@code <Key>} element. */
  private static final int[] KEY_ATTRIBUTES = {
    R.attr.keyWidth,
    R.attr.keyHeight,
    R.attr.keyHorizontalLayoutWeight,
    R.attr.horizontalGap,
    R.attr.defaultIconWidth,
    R.attr.defaultIconHeight,
    R.attr.defaultHorizontalPadding,
    R.attr.defaultVerticalPadding,
    R.attr.keyBackground,
    R.attr.keyEdgeFlags,
    R.attr.isRepeatable,
    R.attr.isModifier,
  };
  static {
    Arrays.sort(KEY_ATTRIBUTES);
  }
  private static final int KEY_KEY_WIDTH_INDEX =
      Arrays.binarySearch(KEY_ATTRIBUTES, R.attr.keyWidth);
  private static final int KEY_KEY_HEIGHT_INDEX =
      Arrays.binarySearch(KEY_ATTRIBUTES, R.attr.keyHeight);
  private static final int KEY_KEY_HORIZONTAL_LAYOUT_WEIGHT_INDEX =
      Arrays.binarySearch(KEY_ATTRIBUTES, R.attr.keyHorizontalLayoutWeight);
  private static final int KEY_HORIZONTAL_GAP_INDEX =
      Arrays.binarySearch(KEY_ATTRIBUTES, R.attr.horizontalGap);
  private static final int KEY_DEFAULT_ICON_WIDTH_INDEX =
      Arrays.binarySearch(KEY_ATTRIBUTES, R.attr.defaultIconWidth);
  private static final int KEY_DEFAULT_ICON_HEIGHT_INDEX =
      Arrays.binarySearch(KEY_ATTRIBUTES, R.attr.defaultIconHeight);
  private static final int KEY_DEFAULT_HORIZONTAL_PADDING_INDEX =
      Arrays.binarySearch(KEY_ATTRIBUTES, R.attr.defaultHorizontalPadding);
  private static final int KEY_DEFAULT_VERTICAL_PADDING_INDEX =
      Arrays.binarySearch(KEY_ATTRIBUTES, R.attr.defaultVerticalPadding);
  private static final int KEY_KEY_BACKGROUND_INDEX =
      Arrays.binarySearch(KEY_ATTRIBUTES, R.attr.keyBackground);
  private static final int KEY_KEY_EDGE_FLAGS_INDEX =
      Arrays.binarySearch(KEY_ATTRIBUTES, R.attr.keyEdgeFlags);
  private static final int KEY_IS_REPEATABLE_INDEX =
      Arrays.binarySearch(KEY_ATTRIBUTES, R.attr.isRepeatable);
  private static final int KEY_IS_MODIFIER_INDEX =
      Arrays.binarySearch(KEY_ATTRIBUTES, R.attr.isModifier);

  /** Attributes for a {@code <Spacer>} element. */
  private static final int[] SPACER_ATTRIBUTES = {
    R.attr.keyWidth,
    R.attr.keyHeight,
    R.attr.keyHorizontalLayoutWeight,
    R.attr.keyEdgeFlags,
    R.attr.stick,
    R.attr.keyBackground,
  };
  static {
    Arrays.sort(SPACER_ATTRIBUTES);
  }
  private static final int SPACER_KEY_WIDTH_INDEX =
      Arrays.binarySearch(SPACER_ATTRIBUTES, R.attr.keyWidth);
  private static final int SPACER_KEY_HEIGHT_INDEX =
      Arrays.binarySearch(SPACER_ATTRIBUTES, R.attr.keyHeight);
  private static final int SPACER_KEY_HORIZONTAL_LAYOUT_WEIGHT_INDEX =
      Arrays.binarySearch(SPACER_ATTRIBUTES, R.attr.keyHorizontalLayoutWeight);
  private static final int SPACER_KEY_EDGE_FLAGS_INDEX =
      Arrays.binarySearch(SPACER_ATTRIBUTES, R.attr.keyEdgeFlags);
  private static final int SPACER_STICK_INDEX =
      Arrays.binarySearch(SPACER_ATTRIBUTES, R.attr.stick);
  private static final int SPACER_KEY_BACKGROUND_INDEX =
      Arrays.binarySearch(SPACER_ATTRIBUTES, R.attr.keyBackground);

  /** Attributes for a {@code <KeyState>} element. */
  private static final int[] KEY_STATE_ATTRIBUTES = {
    R.attr.contentDescription,
    R.attr.metaState,
    R.attr.nextMetaState,
    R.attr.nextRemovedMetaStates,
  };
  static {
    Arrays.sort(KEY_STATE_ATTRIBUTES);
  }
  private static final int KEY_STATE_CONTENT_DESCRIPTION_INDEX =
      Arrays.binarySearch(KEY_STATE_ATTRIBUTES, R.attr.contentDescription);
  private static final int KEY_STATE_META_STATE_INDEX =
      Arrays.binarySearch(KEY_STATE_ATTRIBUTES, R.attr.metaState);
  private static final int KEY_STATE_NEXT_META_STATE_INDEX =
      Arrays.binarySearch(KEY_STATE_ATTRIBUTES, R.attr.nextMetaState);
  private static final int KEY_STATE_NEXT_REMOVED_META_STATES_INDEX =
      Arrays.binarySearch(KEY_STATE_ATTRIBUTES, R.attr.nextRemovedMetaStates);

  /** Attributes for a {@code <KeyEntity>} element. */
  private static final int[] KEY_ENTITY_ATTRIBUTES = {
    R.attr.sourceId,
    R.attr.keyCode,
    R.attr.longPressKeyCode,
    R.attr.longPressTimeoutTrigger,
    R.attr.keyIcon,
    R.attr.keyCharacter,
    R.attr.flickHighlight,
    R.attr.horizontalPadding,
    R.attr.verticalPadding,
    R.attr.iconWidth,
    R.attr.iconHeight,
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
  private static final int KEY_ENTITY_LONG_PRESS_TIMEOUT_TRIGGER_INDEX =
      Arrays.binarySearch(KEY_ENTITY_ATTRIBUTES, R.attr.longPressTimeoutTrigger);
  private static final int KEY_ENTITY_KEY_ICON_INDEX =
      Arrays.binarySearch(KEY_ENTITY_ATTRIBUTES, R.attr.keyIcon);
  private static final int KEY_ENTITY_KEY_CHAR_INDEX =
      Arrays.binarySearch(KEY_ENTITY_ATTRIBUTES, R.attr.keyCharacter);
  private static final int KEY_ENTITY_FLICK_HIGHLIGHT_INDEX =
      Arrays.binarySearch(KEY_ENTITY_ATTRIBUTES, R.attr.flickHighlight);
  private static final int KEY_ENTITY_HORIZONTAL_PADDING_INDEX =
      Arrays.binarySearch(KEY_ENTITY_ATTRIBUTES, R.attr.horizontalPadding);
  private static final int KEY_ENTITY_VERTICAL_PADDING_INDEX =
      Arrays.binarySearch(KEY_ENTITY_ATTRIBUTES, R.attr.verticalPadding);
  private static final int KEY_ENTITY_ICON_WIDTH_INDEX =
      Arrays.binarySearch(KEY_ENTITY_ATTRIBUTES, R.attr.iconWidth);
  private static final int KEY_ENTITY_ICON_HEIGHT_INDEX =
      Arrays.binarySearch(KEY_ENTITY_ATTRIBUTES, R.attr.iconHeight);

  /**
   * Mapping table from enum value in xml to DrawableType by using the enum value as index.
   */
  private static final DrawableType[] KEY_BACKGROUND_DRAWABLE_TYPE_MAP = {
    DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND,
    DrawableType.TWELVEKEYS_FUNCTION_KEY_BACKGROUND,
    DrawableType.TWELVEKEYS_FUNCTION_KEY_BACKGROUND_WITH_THREEDOTS,
    DrawableType.QWERTY_REGULAR_KEY_BACKGROUND,
    DrawableType.QWERTY_FUNCTION_KEY_BACKGROUND,
    DrawableType.QWERTY_FUNCTION_KEY_BACKGROUND_WITH_THREEDOTS,
    DrawableType.QWERTY_FUNCTION_KEY_SPACE_WITH_THREEDOTS,
    DrawableType.KEYBOARD_SEPARATOR_TOP,
    DrawableType.KEYBOARD_SEPARATOR_CENTER,
    DrawableType.KEYBOARD_SEPARATOR_BOTTOM,
    DrawableType.TRNASPARENT,
  };

  /**
   * @return "sourceId" assigned to {@code value}.
   */
  private static int getSourceId(TypedValue value, @SuppressWarnings("unused") int defaultValue) {
    Preconditions.checkNotNull(value);
    Preconditions.checkArgument(
        value.type == TypedValue.TYPE_INT_DEC || value.type == TypedValue.TYPE_INT_HEX,
        "sourceId is mandatory for KeyEntity.");
    return value.data;
  }

  /**
   * @return the pixel offsets based on metrics and base
   */
  @VisibleForTesting static int getDimensionOrFraction(
      Optional<TypedValue> optionalValue, int base, int defaultValue, DisplayMetrics metrics) {
    if (!optionalValue.isPresent()) {
      return defaultValue;
    }
    TypedValue value = optionalValue.get();

    switch (value.type) {
      case TypedValue.TYPE_DIMENSION:
        return TypedValue.complexToDimensionPixelOffset(value.data, metrics);
      case TypedValue.TYPE_FRACTION:
        return Math.round(TypedValue.complexToFraction(value.data, base, base));
    }

    throw new IllegalArgumentException("The type dimension or fraction is required." +
                                       "  value = " + value.toString());
  }

  @VisibleForTesting static int getFraction(
      Optional<TypedValue> optionalValue, int base, int defaultValue) {
    if (!optionalValue.isPresent()) {
      return defaultValue;
    }
    TypedValue value = optionalValue.get();

    if (value.type == TypedValue.TYPE_FRACTION) {
      return Math.round(TypedValue.complexToFraction(value.data, base, base));
    }

    throw new IllegalArgumentException(
        "The type fraction is required.  value = " + value.toString());
  }

  /**
   * @return "codes" assigned to {@code value}
   */
  @VisibleForTesting static int getCode(Optional<TypedValue> optionalValue, int defaultValue) {
    if (!optionalValue.isPresent()) {
      return defaultValue;
    }
    TypedValue value = optionalValue.get();

    if (value.type == TypedValue.TYPE_INT_DEC ||
        value.type == TypedValue.TYPE_INT_HEX) {
      return value.data;
    }
    if (value.type == TypedValue.TYPE_STRING) {
      return Integer.parseInt(value.string.toString());
    }

    return defaultValue;
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
          "The start of document is expected, but actually not: "
              + parser.getPositionDescription());
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
          "Tag <" + expectedName + "> is expected, but found <" + actualName + ">: "
              + parser.getPositionDescription());
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
  private final KeyboardSpecification specification;

  public KeyboardParser(Resources resources,
                        int keyboardWidth, int keyboardHeight,
                        KeyboardSpecification specification) {
    this.resources = Preconditions.checkNotNull(resources);
    this.keyboardWidth = keyboardWidth;
    this.keyboardHeight = keyboardHeight;
    this.specification = Preconditions.checkNotNull(specification);
    this.xmlResourceParser = resources.getXml(specification.getXmlLayoutResourceId());
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
    Optional<String> contentDescription = Optional.absent();
    {
      TypedArray attributes = resources.obtainAttributes(parser, R.styleable.Keyboard);
      try {
        DisplayMetrics metrics = resources.getDisplayMetrics();
        // The default keyWidth is 10% of the display for width, and 50px for height.
        keyAttributes = parseKeyAttributes(
            attributes,
            KeyAttributes.newBuilder()
                .setWidth(keyboardWidth / 10)
                .setHeight(50)
                .setKeybackgroundDrawableType(DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND)
                .setDefaultIconWidth(keyboardWidth)
                .setDefaultIconHeight(keyboardHeight)
                .setDefaultHorizontalPadding(0)
                .setDefaultVerticalPadding(0)
                .build(),
            metrics,
            this.keyboardWidth,
            this.keyboardHeight,
            R.styleable.Keyboard_keyWidth,
            R.styleable.Keyboard_keyHeight,
            R.styleable.Keyboard_keyHorizontalLayoutWeight,
            R.styleable.Keyboard_horizontalGap,
            R.styleable.Keyboard_verticalGap,
            R.styleable.Keyboard_defaultIconWidth,
            R.styleable.Keyboard_defaultIconHeight,
            R.styleable.Keyboard_defaultHorizontalPadding,
            R.styleable.Keyboard_defaultVerticalPadding,
            R.styleable.Keyboard_keyBackground);
        popUpAttributes = parsePopUpAttributes(
            attributes,
            new PopUpAttributes(0, 0, 0, 0, 0),
            metrics,
            this.keyboardWidth,
            R.styleable.Keyboard_popUpHeight,
            R.styleable.Keyboard_popUpXOffset,
            R.styleable.Keyboard_popUpYOffset,
            R.styleable.Keyboard_popUpIconWidth,
            R.styleable.Keyboard_popUpIconHeight);
        flickThreshold = parseFlickThreshold(
            attributes, R.styleable.Keyboard_flickThreshold);
        contentDescription = Optional.fromNullable(
            attributes.getString(R.styleable.Keyboard_keyboardContentDescription));
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

    return new Keyboard(Preconditions.checkNotNull(contentDescription),
        Preconditions.checkNotNull(rowList), flickThreshold, specification);
  }

  @VisibleForTesting
  static List<Key> buildKeyList(List<KeyAttributes> keyAttributesList,
                                        int y, int rowWidth) {
    float remainingWidthByWeight = rowWidth;
    int remainingWeight = 0;
    for (KeyAttributes attributes : keyAttributesList) {
      remainingWidthByWeight -= attributes.width;
      remainingWeight += attributes.horizontalLayoutWeight;
    }

    List<Key> keyList = new ArrayList<Key>(keyAttributesList.size());
    float exactX = 0;
    for (KeyAttributes attributes : keyAttributesList) {
      int weight = attributes.horizontalLayoutWeight;
      Preconditions.checkState(remainingWeight >= weight);
      float widthByWeight = weight > 0 ? remainingWidthByWeight * weight / remainingWeight : 0;
      remainingWidthByWeight -= widthByWeight;
      remainingWeight -= weight;
      int x = Math.round(exactX);
      Key key =
          attributes.buildKey(x, y, Math.round(exactX + widthByWeight + attributes.width) - x);
      keyList.add(key);
      exactX += widthByWeight + attributes.width;
    }
    Preconditions.checkState(remainingWeight == 0);
    return keyList;
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
            Optional.fromNullable(attributes.peekValue(ROW_VERTICAL_GAP_INDEX)),
            keyboardHeight, defaultKeyAttributes.verticalGap, metrics);
        rowHeight = getDimensionOrFraction(
            Optional.fromNullable(attributes.peekValue(ROW_KEY_HEIGHT_INDEX)), keyboardHeight,
            defaultKeyAttributes.height, metrics);
        edgeFlags = attributes.getInt(ROW_ROW_EDGE_FLAGS_INDEX, 0);
      } finally {
        attributes.recycle();
      }
    }

    List<KeyAttributes> keyAttributesList = new ArrayList<KeyAttributes>(16);  // 16 is heuristic
    while (true) {
      parser.next();
      ignoreWhiteSpaceAndComment(parser);
      assertNotEndDocument(parser);
      if (parser.getEventType() == XmlResourceParser.END_TAG) {
        break;
      }
      if ("Key".equals(parser.getName())) {
        keyAttributesList.add(parseKey(edgeFlags, defaultKeyAttributes, popUpAttributes));
      } else if ("Spacer".equals(parser.getName())) {
        keyAttributesList.add(parseSpacer(edgeFlags, defaultKeyAttributes));
      }
    }

    assertEndTag(parser, "Row");
    List<Key> keyList = buildKeyList(keyAttributesList, y, keyboardWidth);
    return buildRow(keyList, rowHeight, verticalGap);
  }

  /**
   * Parses a {@code Key} element, and returns an instance.
   */
  private KeyAttributes parseKey(int edgeFlags, KeyAttributes defaultKeyAttributes,
                                 PopUpAttributes popUpAttributes)
      throws XmlPullParserException, IOException {
    XmlResourceParser parser = this.xmlResourceParser;
    assertStartTag(parser, "Key");

    KeyAttributes keyAttributes;
    boolean isRepeatable;
    boolean isModifier;
    DisplayMetrics metrics = resources.getDisplayMetrics();
    {
      TypedArray attributes = resources.obtainAttributes(parser, KEY_ATTRIBUTES);
      try {
        keyAttributes = parseKeyAttributes(
            attributes, defaultKeyAttributes, metrics, keyboardWidth, keyboardHeight,
            KEY_KEY_WIDTH_INDEX, KEY_KEY_HEIGHT_INDEX, KEY_KEY_HORIZONTAL_LAYOUT_WEIGHT_INDEX,
            KEY_HORIZONTAL_GAP_INDEX, -1,
            KEY_DEFAULT_ICON_WIDTH_INDEX, KEY_DEFAULT_ICON_HEIGHT_INDEX,
            KEY_DEFAULT_HORIZONTAL_PADDING_INDEX, KEY_DEFAULT_VERTICAL_PADDING_INDEX,
            KEY_KEY_BACKGROUND_INDEX);
        edgeFlags |= attributes.getInt(KEY_KEY_EDGE_FLAGS_INDEX, 0);
        isRepeatable = attributes.getBoolean(KEY_IS_REPEATABLE_INDEX, false);
        isModifier = attributes.getBoolean(KEY_IS_MODIFIER_INDEX, false);
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

      keyStateList.add(parseKeyState(keyAttributes, popUpAttributes, metrics));
    }

    // At the moment, we just accept keys which has default state.
    boolean hasDefault = false;
    boolean hasLongPressKeyCode = false;
    for (KeyState keyState : keyStateList) {
      if (keyState.getMetaStateSet().isEmpty()
          || keyState.getMetaStateSet().contains(KeyState.MetaState.FALLBACK)) {
        hasDefault = true;
        Optional<Flick> flick = keyState.getFlick(Flick.Direction.CENTER);
        Preconditions.checkState(flick.isPresent());
        if (flick.get().getKeyEntity().getLongPressKeyCode() != KeyEntity.INVALID_KEY_CODE) {
          hasLongPressKeyCode = true;
        }
        break;
      }
    }
    if (!hasDefault) {
      throw new IllegalArgumentException(
          "No default KeyState element is found: " + parser.getPositionDescription());
    }
    if (isRepeatable && hasLongPressKeyCode) {
      throw new IllegalArgumentException(
          "The key has both isRepeatable attribute and longPressKeyCode: "
            + parser.getPositionDescription());
    }

    assertEndTag(parser, "Key");
    return keyAttributes.toBuilder()
        .setEdgeFlags(edgeFlags)
        .setRepeatable(isRepeatable)
        .setModifier(isModifier)
        .setStick(Stick.EVEN)
        .setKeyStateList(keyStateList)
        .build();
  }

  /**
   * Parses a {@code Spacer} element, and returns an instance.
   */
  private KeyAttributes parseSpacer(int edgeFlags, KeyAttributes defaultKeyAttributes)
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
            SPACER_KEY_WIDTH_INDEX, SPACER_KEY_HEIGHT_INDEX,
            SPACER_KEY_HORIZONTAL_LAYOUT_WEIGHT_INDEX, -1, -1, -1, -1, -1, -1,
            SPACER_KEY_BACKGROUND_INDEX);
        edgeFlags |= attributes.getInt(SPACER_KEY_EDGE_FLAGS_INDEX, 0);
        stick = Stick.values()[attributes.getInt(SPACER_STICK_INDEX, 0)];
      } finally {
        attributes.recycle();
      }
    }

    parser.next();
    assertEndTag(parser, "Spacer");

    // Returns a dummy key object.
    return keyAttributes.toBuilder()
        .setRepeatable(false)
        .setModifier(false)
        .setEdgeFlags(edgeFlags)
        .setStick(stick)
        .build();
  }

  private KeyState parseKeyState(KeyAttributes defaultKeyAttributes,
                                 PopUpAttributes popUpAttributes, DisplayMetrics metrics)
      throws XmlPullParserException, IOException {
    XmlResourceParser parser = this.xmlResourceParser;
    assertStartTag(parser, "KeyState");

    String contentDescription;
    Set<KeyState.MetaState> metaStateSet;
    Set<KeyState.MetaState> nextAddMetaState;
    Set<KeyState.MetaState> nextRemoveMetaState;
    {
      TypedArray attributes = resources.obtainAttributes(parser, KEY_STATE_ATTRIBUTES);
      try {
        contentDescription = MoreObjects.firstNonNull(
            attributes.getText(KEY_STATE_CONTENT_DESCRIPTION_INDEX), "").toString();
        metaStateSet = parseMetaState(attributes, KEY_STATE_META_STATE_INDEX);
        nextAddMetaState = parseMetaState(attributes, KEY_STATE_NEXT_META_STATE_INDEX);
        nextRemoveMetaState = parseMetaState(attributes, KEY_STATE_NEXT_REMOVED_META_STATES_INDEX);
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
      flickList.add(parseFlick(defaultKeyAttributes, popUpAttributes, metrics));
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
    return new KeyState(contentDescription, metaStateSet, nextAddMetaState, nextRemoveMetaState,
                        flickList);
  }

  private Flick parseFlick(KeyAttributes defaultKeyAttributes,
                           PopUpAttributes popUpAttributes, DisplayMetrics metrics)
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
    KeyEntity entity = parseKeyEntity(defaultKeyAttributes, popUpAttributes, metrics);

    if (entity.getLongPressKeyCode() != KeyEntity.INVALID_KEY_CODE
        && direction != Flick.Direction.CENTER) {
      throw new IllegalArgumentException(
          "longPressKeyCode can be set to only a KenEntity for CENTER direction: "
              + parser.getPositionDescription());
    }

    parser.next();
    assertEndTag(parser, "Flick");

    return new Flick(direction, entity);
  }

  private KeyEntity parseKeyEntity(KeyAttributes defaultKeyAttributes,
                                   PopUpAttributes popUpAttributes, DisplayMetrics metrics)
      throws XmlPullParserException, IOException {
    XmlResourceParser parser = this.xmlResourceParser;
    assertStartTag(parser, "KeyEntity");

    int sourceId;
    int keyCode;
    int longPressKeyCode;
    boolean longPressTimeoutTrigger;
    int keyIconResourceId;
    Optional<String> keyCharacter = Optional.absent();
    boolean flickHighlight;
    int horizontalPadding;
    int verticalPadding;
    int iconWidth;
    int iconHeight;
    {
      TypedArray attributes = resources.obtainAttributes(parser, KEY_ENTITY_ATTRIBUTES);
      try {
        sourceId = getSourceId(attributes.peekValue(KEY_ENTITY_SOURCE_ID_INDEX), 0);
        if (!sourceIdSet.add(sourceId)) {
          //  Same sourceId is found.
          throw new IllegalArgumentException(
              "Duplicataed sourceId (" + sourceId + ") is found: "
              + xmlResourceParser.getPositionDescription());
        }
        keyCode = getCode(
            Optional.fromNullable(attributes.peekValue(KEY_ENTITY_KEY_CODE_INDEX)),
            KeyEntity.INVALID_KEY_CODE);
        longPressKeyCode = getCode(
            Optional.fromNullable(attributes.peekValue(KEY_ENTITY_LONG_PRESS_KEY_CODE_INDEX)),
            KeyEntity.INVALID_KEY_CODE);
        longPressTimeoutTrigger = attributes.getBoolean(KEY_ENTITY_LONG_PRESS_TIMEOUT_TRIGGER_INDEX,
                                                        true);
        keyIconResourceId = attributes.getResourceId(KEY_ENTITY_KEY_ICON_INDEX, 0);
        keyCharacter = Optional.fromNullable(attributes.getString(KEY_ENTITY_KEY_CHAR_INDEX));
        flickHighlight = attributes.getBoolean(KEY_ENTITY_FLICK_HIGHLIGHT_INDEX, false);

        horizontalPadding = getDimensionOrFraction(
                Optional.fromNullable(attributes.peekValue(KEY_ENTITY_HORIZONTAL_PADDING_INDEX)),
                keyboardWidth, defaultKeyAttributes.defaultHorizontalPadding, metrics);
        verticalPadding = getDimensionOrFraction(
                Optional.fromNullable(attributes.peekValue(KEY_ENTITY_VERTICAL_PADDING_INDEX)),
                keyboardHeight, defaultKeyAttributes.defaultVerticalPadding, metrics);
        iconWidth = getDimensionOrFraction(
            Optional.fromNullable(attributes.peekValue(KEY_ENTITY_ICON_WIDTH_INDEX)),
            keyboardWidth, defaultKeyAttributes.defaultIconWidth, metrics);
        iconHeight = getDimensionOrFraction(
            Optional.fromNullable(attributes.peekValue(KEY_ENTITY_ICON_HEIGHT_INDEX)),
            keyboardHeight, defaultKeyAttributes.defaultIconHeight, metrics);
      } finally {
        attributes.recycle();
      }
    }

    parser.next();
    ignoreWhiteSpaceAndComment(parser);

    Optional<PopUp> popUp = Optional.absent();
    if (parser.getEventType() == XmlResourceParser.START_TAG) {
      popUp = Optional.of(parsePopUp(popUpAttributes));
      parser.next();
      ignoreWhiteSpaceAndComment(parser);
    }

    assertEndTag(parser, "KeyEntity");

    return new KeyEntity(sourceId, keyCode, longPressKeyCode, longPressTimeoutTrigger,
        keyIconResourceId, keyCharacter, flickHighlight, popUp,
        horizontalPadding, verticalPadding, iconWidth, iconHeight);
  }

  private PopUp parsePopUp(PopUpAttributes defaultValue)
      throws XmlPullParserException, IOException {
    XmlResourceParser parser = this.xmlResourceParser;
    assertStartTag(parser, "PopUp");

    PopUpAttributes popUpAttributes;
    TypedArray attributes = resources.obtainAttributes(parser, POPUP_ATTRIBUTES);
    int popUpIconResourceId;
    int popUpLongPressIconResourceId;
    try {
      popUpAttributes = parsePopUpAttributes(attributes, defaultValue,
          resources.getDisplayMetrics(),
          keyboardWidth, POPUP_KEY_HEIGHT_INDEX,
          POPUP_KEY_X_OFFSET_INDEX, POPUP_KEY_Y_OFFSET_INDEX,
          POPUP_KEY_ICON_WIDTH_INDEX, POPUP_KEY_ICON_HEIGHT_INDEX);
      popUpIconResourceId = attributes.getResourceId(POPUP_KEY_ICON_INDEX, 0);
      popUpLongPressIconResourceId = attributes.getResourceId(POPUP_KEY_LONG_PRESS_ICON_INDEX, 0);
    } finally {
      attributes.recycle();
    }
    parser.next();
    assertEndTag(parser, "PopUp");

    return new PopUp(popUpIconResourceId,
                     popUpLongPressIconResourceId,
                     popUpAttributes.popUpHeight,
                     popUpAttributes.popUpXOffset,
                     popUpAttributes.popUpYOffset,
                     popUpAttributes.popUpIconWidth,
                     popUpAttributes.popUpIconHeight);
  }

  private float parseFlickThreshold(TypedArray attributes, int index) {
    float flickThreshold = attributes.getDimension(
        index, resources.getDimension(R.dimen.default_flick_threshold));
    Preconditions.checkArgument(
        flickThreshold > 0, "flickThreshold must be greater than 0.  value = " + flickThreshold);
    return flickThreshold;
  }

  private static KeyAttributes parseKeyAttributes(
      TypedArray attributes, KeyAttributes defaultValue, DisplayMetrics metrics,
      int keyboardWidth, int keyboardHeight,
      int keyWidthIndex, int keyHeightIndex, int keyHorizontalLayoutWeightIndex,
      int horizontalGapIndex, int verticalGapIndex,
      int defaultIconWidthIndex, int defaultIconHeightIndex,
      int defaultHorizontalPaddingIndex, int defaultVerticalPaddingIndex,
      int keyBackgroundIndex) {

    int keyWidth = (keyWidthIndex >= 0)
        ? getDimensionOrFraction(
              Optional.fromNullable(attributes.peekValue(keyWidthIndex)), keyboardWidth,
              defaultValue.width, metrics)
        : defaultValue.width;
    int keyHeight = (keyHeightIndex >= 0)
        ? getDimensionOrFraction(
              Optional.fromNullable(attributes.peekValue(keyHeightIndex)), keyboardHeight,
              defaultValue.height, metrics)
        : defaultValue.height;
    int keyHorizontalLayoutWeight = (keyHorizontalLayoutWeightIndex >= 0)
        ? attributes.getInt(keyHorizontalLayoutWeightIndex, defaultValue.horizontalLayoutWeight)
        : defaultValue.horizontalLayoutWeight;

    int horizontalGap = (horizontalGapIndex >= 0)
        ? getDimensionOrFraction(
              Optional.fromNullable(attributes.peekValue(horizontalGapIndex)),
              keyboardWidth, defaultValue.horizontalGap, metrics)
        : defaultValue.horizontalGap;
    int verticalGap = (verticalGapIndex >= 0)
        ? getDimensionOrFraction(
              Optional.fromNullable(attributes.peekValue(verticalGapIndex)),
              keyboardHeight, defaultValue.verticalGap, metrics)
        : defaultValue.verticalGap;
    int defaultIconWidth = (defaultIconWidthIndex >= 0)
        ? getDimensionOrFraction(
            Optional.fromNullable(attributes.peekValue(defaultIconWidthIndex)),
            keyboardWidth, defaultValue.defaultIconWidth, metrics)
        : defaultValue.defaultIconWidth;
    int defaultIconHeight = (defaultIconHeightIndex >= 0)
        ? getDimensionOrFraction(
            Optional.fromNullable(attributes.peekValue(defaultIconHeightIndex)),
            keyboardHeight, defaultValue.defaultIconHeight, metrics)
        : defaultValue.defaultIconHeight;
    int defaultHorizontalPadding = (defaultHorizontalPaddingIndex >= 0)
        ? getDimensionOrFraction(
            Optional.fromNullable(attributes.peekValue(defaultHorizontalPaddingIndex)),
            keyboardWidth, defaultValue.defaultHorizontalPadding, metrics)
        : defaultValue.defaultHorizontalPadding;
    int defaultVerticalPadding = (defaultVerticalPaddingIndex >= 0)
        ? getDimensionOrFraction(
            Optional.fromNullable(attributes.peekValue(defaultVerticalPaddingIndex)),
            keyboardWidth, defaultValue.defaultVerticalPadding, metrics)
        : defaultValue.defaultVerticalPadding;

    DrawableType keyBackgroundDrawableType = parseKeyBackgroundDrawableType(
        attributes, keyBackgroundIndex, defaultValue.keyBackgroundDrawableType);
    return KeyAttributes.newBuilder()
        .setWidth(keyWidth)
        .setHeight(keyHeight)
        .setHorizontalLayoutWeight(keyHorizontalLayoutWeight)
        .setHorizontalGap(horizontalGap)
        .setVerticalGap(verticalGap)
        .setDefaultHorizontalPadding(defaultHorizontalPadding)
        .setDefaultVerticalPadding(defaultVerticalPadding)
        .setDefaultIconWidth(defaultIconWidth)
        .setDefaultIconHeight(defaultIconHeight)
        .setKeybackgroundDrawableType(keyBackgroundDrawableType)
        .build();
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
      TypedArray attributes, PopUpAttributes defaultValue, DisplayMetrics metrics,
      int keyboardWidth, int popUpHeightIndex,
      int popUpXOffsetIndex, int popUpYOffsetIndex,
      int popUpIconWidthIndex, int popUpIconHeightIndex) {
    int popUpHeight = (popUpHeightIndex >= 0)
        ? getDimensionOrFraction(
            Optional.fromNullable(attributes.peekValue(popUpHeightIndex)), keyboardWidth,
            defaultValue.popUpHeight, metrics)
        : defaultValue.popUpHeight;
    int popUpXOffset = (popUpXOffsetIndex >= 0)
        ? getDimensionOrFraction(
            Optional.fromNullable(attributes.peekValue(popUpXOffsetIndex)), keyboardWidth,
            defaultValue.popUpXOffset, metrics)
        : defaultValue.popUpXOffset;
    int popUpYOffset = (popUpYOffsetIndex >= 0)
        ? getDimensionOrFraction(
            Optional.fromNullable(attributes.peekValue(popUpYOffsetIndex)), keyboardWidth,
            defaultValue.popUpYOffset, metrics)
        : defaultValue.popUpYOffset;
    int popUpIconWidth = (popUpIconWidthIndex >= 0)
        ? getDimensionOrFraction(
            Optional.fromNullable(attributes.peekValue(popUpIconWidthIndex)), keyboardWidth,
            defaultValue.popUpIconWidth, metrics)
        : defaultValue.popUpIconWidth;
    int popUpIconHeight = (popUpIconHeightIndex >= 0)
        ? getDimensionOrFraction(
            Optional.fromNullable(attributes.peekValue(popUpIconHeightIndex)), keyboardWidth,
            defaultValue.popUpIconHeight, metrics)
        : defaultValue.popUpIconHeight;
    return new PopUpAttributes(
        popUpHeight, popUpXOffset, popUpYOffset, popUpIconWidth, popUpIconHeight);
  }

  /**
   * Returns set of MetaState.
   *
   * <p>Empty set is returned if corresponding attribute is not found.
   * This is used for default KeyState.
   */
  private Set<KeyState.MetaState> parseMetaState(TypedArray attributes, int index) {
    int metaStateFlags = attributes.getInt(index, 0);
    if (metaStateFlags == 0) {
      return Collections.emptySet();
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

  private Flick.Direction parseFlickDirection(TypedArray attributes, int index) {
    return Flick.Direction.valueOf(attributes.getInt(index, Flick.Direction.CENTER.index));
  }

  private Row buildRow(List<Key> keyList, int height, int verticalGap) {
    return new Row(keyList, height, verticalGap);
  }
}
