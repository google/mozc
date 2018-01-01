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

package org.mozc.android.inputmethod.japanese.util;

import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;

import java.io.IOException;

/**
 * Utility for XML parser.
 */
public class ParserUtil {

  public static void ignoreWhiteSpaceAndComment(XmlPullParser parser)
      throws XmlPullParserException, IOException {
    int event = parser.getEventType();
    while (event == XmlPullParser.IGNORABLE_WHITESPACE || event == XmlPullParser.COMMENT) {
      event = parser.next();
    }
  }

  public static void assertStartDocument(XmlPullParser parser) throws XmlPullParserException {
    if (parser.getEventType() != XmlPullParser.START_DOCUMENT) {
      throw new IllegalArgumentException(
          "The start of document is expected, but actually not: "
          + parser.getPositionDescription());
    }
  }

  public static void assertEndDocument(XmlPullParser parser) throws XmlPullParserException {
    if (parser.getEventType() != XmlPullParser.END_DOCUMENT) {
      throw new IllegalArgumentException(
          "The end of document is expected, but actually not: " + parser.getPositionDescription());
    }
  }

  public static void assertNotEndDocument(XmlPullParser parser) throws XmlPullParserException {
    if (parser.getEventType() == XmlPullParser.END_DOCUMENT) {
      throw new IllegalArgumentException(
          "Unexpected end of document is found: " + parser.getPositionDescription());
    }
  }

  public static void assertTagName(XmlPullParser parser, String expectedName) {
    String actualName = parser.getName();
    if (!actualName.equals(expectedName)) {
      throw new IllegalArgumentException(
          "Tag <" + expectedName + "> is expected, but found <" + actualName + ">: "
          + parser.getPositionDescription());
    }
  }

  public static void assertStartTag(XmlPullParser parser, String expectedName)
      throws XmlPullParserException {
    if (parser.getEventType() != XmlPullParser.START_TAG) {
      throw new IllegalArgumentException(
          "Start tag <" + expectedName + "> is expected: " + parser.getPositionDescription());
    }
    assertTagName(parser, expectedName);
  }

  public static void assertEndTag(XmlPullParser parser, String expectedName)
      throws XmlPullParserException {
    if (parser.getEventType() != XmlPullParser.END_TAG) {
      throw new IllegalArgumentException(
          "End tag </" + expectedName + "> is expected: " + parser.getPositionDescription());
    }
    assertTagName(parser, expectedName);
  }
}
