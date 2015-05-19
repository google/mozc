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

package org.mozc.android.inputmethod.japanese.util;

import android.test.suitebuilder.annotation.SmallTest;

import junit.framework.TestCase;

import java.util.NoSuchElementException;

/**
 * The small set of unit tests for ArrayDeque.
 * Note that offerLast is tested via whole test cases.
 */
public class ArrayDequeTest extends TestCase {
  @SmallTest
  public void testClear() {
    ArrayDeque<Integer> deque = new ArrayDeque<Integer>(10);
    assertEquals(0, deque.size());
    deque.clear();
    assertEquals(0, deque.size());

    for (int i = 0; i < 5; ++i) {
      deque.offerLast(0);
    }
    assertEquals(5, deque.size());
    deque.clear();
    assertEquals(0, deque.size());
  }

  @SmallTest
  public void testPeek() {
    ArrayDeque<Integer> deque = new ArrayDeque<Integer>(10);
    assertNull(deque.peekFirst());

    for (int i = 0; i < 5; ++i) {
      deque.offerLast(i);
    }

    // Peek doesn't remove any elements.
    assertEquals(Integer.valueOf(0), deque.peekFirst());
    assertEquals(Integer.valueOf(0), deque.peekFirst());
    assertEquals(Integer.valueOf(0), deque.peekFirst());

    assertEquals(Integer.valueOf(4), deque.peekLast());
    assertEquals(Integer.valueOf(4), deque.peekLast());
    assertEquals(Integer.valueOf(4), deque.peekLast());

    assertEquals(5, deque.size());
  }

  @SmallTest
  public void testSize() {
    ArrayDeque<Integer> deque = new ArrayDeque<Integer>(10);
    assertEquals(0, deque.size());
    assertTrue(deque.isEmpty());

    deque.offerLast(0);
    assertEquals(1, deque.size());
    assertFalse(deque.isEmpty());

    deque.offerLast(1);
    assertEquals(2, deque.size());
    assertFalse(deque.isEmpty());

    deque.removeFirst();
    assertEquals(1, deque.size());
    assertFalse(deque.isEmpty());

    deque.removeFirst();
    assertEquals(0, deque.size());
    assertTrue(deque.isEmpty());
  }

  @SmallTest
  public void testRemove() {
    ArrayDeque<Integer> deque = new ArrayDeque<Integer>(10);
    try {
      deque.removeFirst();
      fail("NoSuchElementException is expected.");
    } catch (NoSuchElementException e) {
      // pass.
    }

    for (int i = 0; i < 5; ++i) {
      deque.offerLast(i);
    }

    assertEquals(5, deque.size());
    assertEquals(Integer.valueOf(0), deque.removeFirst());
    assertEquals(4, deque.size());
    assertEquals(Integer.valueOf(1), deque.removeFirst());
    assertEquals(3, deque.size());
    assertEquals(Integer.valueOf(2), deque.removeFirst());
    assertEquals(2, deque.size());
    assertEquals(Integer.valueOf(3), deque.removeFirst());
    assertEquals(1, deque.size());
    assertEquals(Integer.valueOf(4), deque.removeFirst());
    assertEquals(0, deque.size());
  }

  @SmallTest
  public void testContains() {
    ArrayDeque<Integer> deque = new ArrayDeque<Integer>(10);

    for (int i = 0; i < 5; ++i) {
      deque.offerLast(i);
    }

    assertTrue(deque.contains(0));
    assertFalse(deque.contains(-1));
    assertTrue(deque.contains(4));
    assertFalse(deque.contains(5));
    assertFalse(deque.contains(Integer.MAX_VALUE));
    assertFalse(deque.contains(Integer.MIN_VALUE));
  }
}
