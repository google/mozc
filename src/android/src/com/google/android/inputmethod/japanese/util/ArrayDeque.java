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

package org.mozc.android.inputmethod.japanese.util;

import com.google.common.base.Joiner;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Iterator;
import java.util.List;
import java.util.NoSuchElementException;

/**
 * A self-implementation of a queue of update records with upper limitation.
 * We just implement subset of java.util.Deque interface, which we need,
 * so that we can replace it in future easily.
 * This class should be replaced by java.util.ArrayDeque, when we start to support only API level 9
 * or later.
 *
 * @param <T> type of elements
 *
 */
public class ArrayDeque<T> implements Iterable<T> {
  private int headIndex;
  private int tailIndex;
  private final T[] elements;

  public ArrayDeque(int numElements) {
    // Allocate one more element for a sentinel.
    elements = ArrayDeque.<T>createArray(numElements + 1);
    headIndex = 0;
    tailIndex = 0;
  }

  @SuppressWarnings("unchecked")
  private static <T> T[] createArray(int size) {
    return (T[]) new Object[size];
  }

  public void clear() {
    Arrays.fill(elements, null);
    headIndex = 0;
    tailIndex = 0;
  }

  public T peekFirst() {
    if (isEmpty()) {
      return null;
    }
    return elements[headIndex];
  }

  public T peekLast() {
    if (isEmpty()) {
      return null;
    }
    int capacity = elements.length;
    return elements[(tailIndex + capacity - 1) % capacity];
  }

  public boolean isEmpty() {
    return headIndex == tailIndex;
  }

  /**
   * This implementation is slightly different from java.util.ArrayDeque. If this container is
   * already full, this method just returns {@code false}, instead of extending the internal
   * buffer.
   */
  public boolean offerLast(T element) {
    int nextIndex = (tailIndex + 1) % elements.length;
    if (headIndex == nextIndex) {
      return false;
    }

    elements[tailIndex] = element;
    tailIndex = nextIndex;
    return true;
  }

  public T removeFirst() throws NoSuchElementException {
    if (isEmpty()) {
      throw new NoSuchElementException("ArrayDeque is empty.");
    }
    T result = elements[headIndex];
    elements[headIndex] = null;
    headIndex = (headIndex + 1) % elements.length;
    return result;
  }

  public int size() {
    return (tailIndex - headIndex + elements.length) % elements.length;
  }

  public boolean contains(Object element) {
    int size = size();
    for (int i = 0; i < size; ++i) {
      int index = (headIndex + i) % elements.length;
      if (eq(element, elements[index])) {
        return true;
      }
    }
    return false;
  }

  private static boolean eq(Object r1, Object r2) {
    if (r1 == null) {
      return r2 == null;
    }
    return r1.equals(r2);
  }

  @Override
  public String toString() {
    int size = size();
    if (size == 0) {
      return "[empty]";
    }
    List<T> resultList = new ArrayList<T>(size);
    for (int i = 0; i < size; ++i) {
      resultList.add(elements[(headIndex + i) % elements.length]);
    }
    return new StringBuilder()
        .append("[(")
        .append(Joiner.on(")(").join(resultList))
        .append(")]")
        .toString();
  }

  @Override
  public Iterator<T> iterator() {
    return new Iterator<T>() {
      private int index = 0;

      @Override
      public boolean hasNext() {
        return index < size();
      }

      @Override
      public T next() {
        return elements[(headIndex + index++) % elements.length];
      }

      @Override
      public void remove() {
        throw new UnsupportedOperationException();
      }};
  }
}
