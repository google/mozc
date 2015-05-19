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

import android.app.Dialog;
import android.app.Instrumentation;
import android.content.Context;
import android.view.View;

import org.easymock.EasyMockSupport;
import org.easymock.IMockBuilder;

/**
 * Utilities shared by the implementation of TestCase classes with mock.
 *
 * Unfortunately, there is no ways to do "mix-in" style implementation in Java, and we need to
 * inherit appropriate test class, such as InstrumentationTestCase/ActivityInstrumentationTestCase2
 * or so. Instead, we have shared implementation in this class, and invoke the methods from
 * inheriting classes as proxy.
 *
 */
class InstrumentationMockUtil {
  private Instrumentation instrumentation;
  private EasyMockSupport mockSupport;

  InstrumentationMockUtil(Instrumentation instrumentation, EasyMockSupport mockSupport) {
    this.instrumentation = instrumentation;
    this.mockSupport = mockSupport;
  }

  <T extends View> T createViewMock(Class<T> toMock) {
    return createViewMockBuilder(toMock).createMock();
  }

  <T extends View> IMockBuilder<T> createViewMockBuilder(Class<T> toMock) {
    return createMockBuilderWithContext(toMock);
  }

  <T extends Dialog> T createDialogMock(Class<T> toMock) {
    return createDialogMockBuilder(toMock).createMock();
  }

  <T extends Dialog> IMockBuilder<T> createDialogMockBuilder(Class<T> toMock) {
    return createMockBuilderWithContext(toMock);
  }

  private <T> IMockBuilder<T> createMockBuilderWithContext(Class<T> toMock) {
    return mockSupport.createMockBuilder(toMock)
        .withConstructor(Context.class)
        .withArgs(instrumentation.getTargetContext());
  }
}
