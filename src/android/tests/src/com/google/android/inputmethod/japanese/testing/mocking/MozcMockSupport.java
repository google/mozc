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

package org.mozc.android.inputmethod.japanese.testing.mocking;

import android.app.Instrumentation;
import android.content.Context;

import org.easymock.EasyMockSupport;
import org.easymock.IMocksControl;
import org.easymock.internal.MocksControl.MockType;

import java.io.File;

/**
 * Mozc customized version of EasyMockSupport.
 *
 * There is a plan that EasyMock supports Android mocking based on dexmaker, as far as we know,
 * but it seems pending for a while. So, we have some minimum support focusing on Mozc.
 *
 * EasyMock has started to support EasyMockSupport in order to provide some convenient methods,
 * such as replayAll, resetAll and verifyAll, inside a test case since version 2.5.2.
 * The class also have a facet as a helper class to inject some customized mocking code,
 * which we can use for MechaMozc mocking code.
 *
 * An example usage of this class is as follows:
 * {@code
 * public class SomeTestClass extends InstrumentationTestCase {
 *   private EasyMockSupport mockSupport;
 *
 *   @Override
 *   protected void setUp() throws Exception {
 *     super.setUp();
 *     mockSupport = new MozcMockSupport(getInstrumentation());
 *   }
 *
 *   @Override
 *   protected void tearDown() throws Exception {
 *     mockSupport = null;
 *     super.tearDown();
 *   }
 *
 *   public void testSomething() {
 *     SomeClass mockInstance = mockSupport.createMock(SomeClass.class);
 *   }
 * }
 * }
 *
 *
 */
public class MozcMockSupport extends EasyMockSupport {
  private final File dexCache;

  public MozcMockSupport() {
    dexCache = null;
  }

  public MozcMockSupport(Instrumentation instrumentation) {
    this.dexCache = getDefaultDexCacheDirectory(instrumentation);
  }

  public static File getDefaultDexCacheDirectory(Instrumentation instrumentation) {
    return instrumentation.getTargetContext().getDir("dx", Context.MODE_PRIVATE);
  }

  @Override
  public IMocksControl createControl() {
    return createControl(MockType.DEFAULT);
  }

  @Override
  public IMocksControl createNiceControl() {
    return createControl(MockType.NICE);
  }

  @Override
  public IMocksControl createStrictControl() {
    return createControl(MockType.STRICT);
  }

  private IMocksControl createControl(MockType type) {
    IMocksControl control = new AndroidMocksControl(type, dexCache);
    controls.add(control);
    return control;
  }
}
