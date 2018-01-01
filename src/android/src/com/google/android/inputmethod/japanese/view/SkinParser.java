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

package org.mozc.android.inputmethod.japanese.view;

import org.mozc.android.inputmethod.japanese.resources.R;
import org.mozc.android.inputmethod.japanese.util.ParserUtil;
import com.google.common.base.Preconditions;
import com.google.common.collect.Maps;

import android.content.res.Resources;
import android.content.res.TypedArray;
import android.content.res.XmlResourceParser;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.util.Xml;

import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;

import java.io.IOException;
import java.lang.reflect.Field;
import java.util.Arrays;
import java.util.Collections;
import java.util.Map;

/**
 * Parses .xml file for skin and generates {@code Skin} instance.
 */
public class SkinParser {

  static class SkinParserException extends Exception {

    public SkinParserException(XmlPullParser parser, Throwable throwable) {
      super(composeMessage(parser, throwable.getLocalizedMessage()), throwable);
    }

    public SkinParserException(XmlPullParser parser, String detailMessage) {
      super(composeMessage(parser, detailMessage));
    }

    private static String composeMessage(XmlPullParser parser, String message) {
      return new StringBuffer(parser.getPositionDescription())
          .append(':').append(message).toString();
    }
  }

  private final XmlResourceParser parser;
  private final Resources resources;

  private static final int[] COLOR_ATTRIBUTES = {
    android.R.attr.name,
    android.R.attr.color,
  };
  static {
    Arrays.sort(COLOR_ATTRIBUTES);
  }
  private static final int COLOR_KEY_NAME_INDEX =
      Arrays.binarySearch(COLOR_ATTRIBUTES, android.R.attr.name);
  private static final int COLOR_KEY_COLOR_INDEX =
      Arrays.binarySearch(COLOR_ATTRIBUTES, android.R.attr.color);

  private static final int[] DRAWABLE_ATTRIBUTES = {
    android.R.attr.name,
  };
  private static final int DRAWABLE_KEY_NAME_INDEX =
      Arrays.binarySearch(DRAWABLE_ATTRIBUTES, android.R.attr.name);

  private static final int[] DIMENSION_ATTRIBUTES = {
    android.R.attr.name,
    R.attr.dimension,
  };
  static {
    Arrays.sort(DIMENSION_ATTRIBUTES);
  }
  private static final int DIMENSION_KEY_NAME_INDEX =
      Arrays.binarySearch(DIMENSION_ATTRIBUTES, android.R.attr.name);
  private static final int DIMENSION_KEY_DIMENSION_INDEX =
      Arrays.binarySearch(DIMENSION_ATTRIBUTES, R.attr.dimension);

  public SkinParser(Resources resources, XmlResourceParser parser) {
    this.resources = Preconditions.checkNotNull(resources);
    this.parser = Preconditions.checkNotNull(parser);
  }

  private static final Map<String, Field> fieldMap;
  static {
    Map<String, Field> tempMap = Maps.newHashMapWithExpectedSize(Skin.class.getFields().length);
    for (Field field : Skin.class.getFields()) {
      tempMap.put(field.getName(), field);
    }
    fieldMap = Collections.unmodifiableMap(tempMap);
  }

  public Skin parseSkin() throws SkinParserException {
    XmlResourceParser parser = this.parser;
    Skin skin = new Skin();
    AttributeSet attributeSet = Xml.asAttributeSet(parser);

    try {
      // Initial two events should be START_DOCUMENT and then START_TAG.
      parser.next();
      ParserUtil.assertStartDocument(parser);
      parser.nextTag();
      if (!"Skin".equals(parser.getName())) {
        throw new SkinParserException(parser,
            "<Skin> element is expected but met <" + parser.getName() + ">");
      }

      while (parser.nextTag() == XmlResourceParser.START_TAG) {
        String tagName = parser.getName();
        if ("Color".equals(tagName)) {
          TypedArray attributes = resources.obtainAttributes(parser, COLOR_ATTRIBUTES);
          try {
            String name = attributes.getString(COLOR_KEY_NAME_INDEX);
            if (name == null) {
              throw new SkinParserException(parser,
                                            "<Color> element's \"name\" attribute is mandatory.");
            }
            int color = attributes.getColor(COLOR_KEY_COLOR_INDEX, 0);
            Field field = fieldMap.get(name);
            if (field == null) {
              throw new SkinParserException(parser, name + " is undefined field.");
            }
            field.setInt(skin, color);
            if (parser.nextTag() != XmlResourceParser.END_TAG) {
              throw new SkinParserException(parser, "</Color> is expected but not found.");
            }
          } finally {
            attributes.recycle();
          }
          continue;
        }
        if ("Drawable".equals(tagName)) {
          TypedArray attributes = resources.obtainAttributes(parser, DRAWABLE_ATTRIBUTES);
          try {
            int originalDepth = parser.getDepth();
            String name = attributes.getString(DRAWABLE_KEY_NAME_INDEX);
            if (name == null) {
              throw new SkinParserException(parser,
                  "<Drawable> element's \"name\" attribute is mandatory.");
            }
            // Go forward to inner drawable tag.
            if (parser.nextTag() != XmlResourceParser.START_TAG) {
              throw new SkinParserException(parser, "Start tag for drawable is expected.");
            }
            Drawable drawable = Drawable.createFromXmlInner(resources, parser, attributeSet);
            if (drawable == null) {
              throw new SkinParserException(parser, "Invalid drawable.");
            }
            // Skip end tags for drawable and <Drawable>.
            // Unfortunatelly, Drawable.createFromXmlInner() leave the end tag of some drawables.
            // The behavior of the method depends on tye type of drawable and Android version.
            // See also: b/23951337 and b/24091330.
            while (parser.getDepth() > originalDepth) {
              if (parser.nextTag() != XmlResourceParser.END_TAG) {
                throw new SkinParserException(parser, "End tag is expected.");
              }
            }
            Field field = fieldMap.get(name);
            if (field == null) {
              throw new SkinParserException(parser, name + " is undefined field.");
            }
            field.set(skin, drawable);
          } finally {
            attributes.recycle();
          }
          continue;
        }
        if ("Dimension".equals(tagName)) {
          TypedArray attributes = resources.obtainAttributes(parser, DIMENSION_ATTRIBUTES);
          try {
            String name = attributes.getString(DIMENSION_KEY_NAME_INDEX);
            if (name == null) {
              throw new SkinParserException(parser,
                  "<Dimension> element's \"name\" attribute is mandatory.");
            }
            float dimension = attributes.getDimension(DIMENSION_KEY_DIMENSION_INDEX, 0);
            Field field = fieldMap.get(name);
            if (field == null) {
              throw new SkinParserException(parser, name + " is undefined field.");
            }
            field.setFloat(skin, dimension);
            if (parser.nextTag() != XmlResourceParser.END_TAG) {
              throw new SkinParserException(parser, "</Dimension> is expected but not found.");
            }
          } finally {
            attributes.recycle();
          }
          continue;
        }
        throw new SkinParserException(parser, "Unexpected <" + tagName + "> is found.");
      }
      parser.next();
      ParserUtil.assertEndDocument(parser);
    } catch (IllegalAccessException e) {
      throw new SkinParserException(parser, e);
    } catch (IllegalArgumentException e) {
      throw new SkinParserException(parser, e);
    } catch (XmlPullParserException e) {
      throw new SkinParserException(parser, e);
    } catch (IOException e) {
      throw new SkinParserException(parser, e);
    }
    return skin;
  }
}
