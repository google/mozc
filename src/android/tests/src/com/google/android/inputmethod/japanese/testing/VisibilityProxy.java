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

package org.mozc.android.inputmethod.japanese.testing;

import junit.framework.Assert;

import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

/**
 * Utility methods to access private fields/methods.
 *
 */
public class VisibilityProxy {
  // Disallow instantiation.
  private VisibilityProxy() {
  }

  /**
   * Gets the (private) field.
   * @param <T> the type of the field
   * @param target the instance of which field is gotten
   * @param fieldName the name of the field
   * @return the field value
   */
  public static <T> T getField(Object target, String fieldName) {
    return VisibilityProxy.<T>getFieldInternal(target.getClass(), target, fieldName);
  }

  /**
   * Gets the (private) static field.
   * @param targetClass the class of which field is gotten
   * @param fieldName the name of the field
   * @return the field value
   */
  public static <T> T getStaticField(Class<?> targetClass, String fieldName) {
    return VisibilityProxy.<T>getFieldInternal(targetClass, null, fieldName);
  }

  private static <T> T getFieldInternal(Class<?> clazz, Object target, String fieldName) {
    Field field = getFieldReflection(clazz, fieldName);
    try {
      @SuppressWarnings("unchecked")
      T result = (T) field.get(target);
      return result;
    } catch (Exception e) {
      Assert.fail(e.toString());
    }

    throw new AssertionError("Should never reach here.");
  }

  /**
   * Sets the (private) field.
   * @param target the instance of which field is set
   * @param fieldName the name of the field
   * @param value the value to be set
   */
  public static void setField(Object target, String fieldName, Object value) {
    setFieldInternal(target.getClass(), target, fieldName, value);
  }

  /**
   * Sets the (private) static field.
   * @param targetClass the class of which field is set
   * @param fieldName the name of the field
   * @param value the value to be set
   */
  public static void setStaticField(Class<?> targetClass, String fieldName, Object value) {
    setFieldInternal(targetClass, null, fieldName, value);
  }


  private static void setFieldInternal(
      Class<?> clazz, Object target, String fieldName, Object value) {
    Field field = getFieldReflection(clazz, fieldName);
    try {
      field.set(target, value);
    } catch (Exception e) {
      Assert.fail(e.toString());
    }
  }

  /** Returns a {@code Field} instance or fail if not found. */
  private static <T> Field getFieldReflection(Class<T> originalClass, String fieldName) {
    for (Class<? super T> clazz = originalClass; clazz != null; clazz = clazz.getSuperclass()) {
      try {
        Field field = clazz.getDeclaredField(fieldName);
        field.setAccessible(true);
        return field;
      } catch (NoSuchFieldException e) {
        // Ignore this exception, and try to find the field from the superclass.
      }
    }
    Assert.fail(originalClass.getCanonicalName() + "#" + fieldName + " is not found.");
    throw new AssertionError("Should never reach here.");
  }

  /**
   * Invokes the (private) method.
   *
   * This method does not work for overloaded method.
   * @param target the instance of which method is invoked
   * @param methodName the name of the method
   * @param args the arguments passed to the method
   * @throws InvocationTargetException the exception thrown by the invoked method
   */
  public static <T> T invokeByName(Object target, String methodName, Object... args)
      throws InvocationTargetException {
    return VisibilityProxy.<T>invokeByNameInternal(target.getClass(), target, methodName, args);
  }

  /**
   * Invokes the (private) static method.
   *
   * This method does not work for overloaded method.
   * @param clazz the {@code Class} instance which the method belongs to
   * @param methodName the name of the method
   * @param args the arguments passed to the method
   * @throws InvocationTargetException the exception thrown by the invoked method
   */
  public static <T> T invokeStaticByName(Class<?> clazz, String methodName, Object... args)
      throws InvocationTargetException {
    return VisibilityProxy.<T>invokeByNameInternal(clazz, null, methodName, args);
  }

  private static <T> T invokeByNameInternal(
      Class<?> originalClass, Object target, String methodName, Object... args)
      throws InvocationTargetException {
    Method method = getMethodReflection(originalClass, methodName);
    try {
      @SuppressWarnings("unchecked")
      T result = (T) method.invoke(target, args);
      return result;
    } catch (InvocationTargetException e) {
      // If the target method throws an exception, re-throw it.
      // The caller may expect this method to throw an exception.
      throw e;
    } catch (Exception e) {
      Assert.fail(e.toString());
    }

    throw new AssertionError("Should never reach here.");
  }

  /** Returns a {@code Method} instance or fail if not found. */
  private static <T> Method getMethodReflection(Class<T> originalClass, String methodName) {
    for (Class<? super T> clazz = originalClass; clazz != null; clazz = clazz.getSuperclass()) {
      for (Method method : clazz.getDeclaredMethods()) {
        if (method.getName().equals(methodName)) {
          // Found the method by name. Note that this doesn't work with overloaded methods.
          method.setAccessible(true);
          return method;
        }
      }
    }

    Assert.fail(originalClass.getCanonicalName() + "#" + methodName + " is not found.");
    throw new AssertionError("Should never reach here.");
  }
}
