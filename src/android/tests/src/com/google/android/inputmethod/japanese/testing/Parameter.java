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

package org.mozc.android.inputmethod.japanese.testing;

import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

/**
 * Parameter data as testing utility of parameterized test.
 *
 */
public abstract class Parameter {

  /**
   * Comparator to compare fields by their names.
   */
  private static class FieldNameComparator implements Comparator<Field> {
    @Override
    public int compare(Field lhs, Field rhs) {
      return lhs.getName().compareTo(rhs.getName());
    }
  }

  private static final Comparator<? super Field> FIELD_NAME_COMPARATOR =
      new FieldNameComparator();

  @Override
  public String toString() {
    List<Field> allFields = getAllFields();
    if (allFields.isEmpty()) {
      return "{}";
    }

    Collections.sort(allFields, FIELD_NAME_COMPARATOR);

    StringBuilder builder = new StringBuilder();
    builder.append("{ ");
    for (Field field : allFields) {
      builder.append(field.getName())
             .append("=")
             .append(getFieldString(field))
             .append("; ");
    }
    builder.append("}");

    return builder.toString();
 }

  private List<Field> getAllFields() {
    List<Field> result = new ArrayList<Field>();
    for (Class<?> cls = getClass(); cls != Parameter.class; cls = cls.getSuperclass()) {
      result.addAll(Arrays.asList(cls.getDeclaredFields()));
    }
    return result;
  }

  private static String toString(Object obj) {
    if (obj == null) {
      return "(null)";
    }

    if (obj instanceof Object[]) {
      return Arrays.toString(Object[].class.cast(obj));
    }
    if (obj instanceof long[]) {
      return Arrays.toString(long[].class.cast(obj));
    }
    if (obj instanceof int[]) {
      return Arrays.toString(int[].class.cast(obj));
    }
    if (obj instanceof char[]) {
      return Arrays.toString(char[].class.cast(obj));
    }
    if (obj instanceof boolean[]) {
      return Arrays.toString(boolean[].class.cast(obj));
    }
    if (obj instanceof byte[]) {
      return Arrays.toString(byte[].class.cast(obj));
    }
    if (obj instanceof float[]) {
      return Arrays.toString(float[].class.cast(obj));
    }
    if (obj instanceof short[]) {
      return Arrays.toString(short[].class.cast(obj));
    }
    if (obj instanceof double[]) {
      return Arrays.toString(double[].class.cast(obj));
    }

    return obj.toString();
  }

  private String getFieldString(Field field) {
    try {
      field.setAccessible(true);
      return toString(field.get(this));
    } catch (IllegalArgumentException e) {
      e.printStackTrace();
    } catch (IllegalAccessException e) {
      e.printStackTrace();
    }
    return "***FAILED_TO_ACCESS***";
  }
}
