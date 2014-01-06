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

import org.mozc.android.inputmethod.japanese.testing.mocking.MozcMockSupport;

import android.app.Activity;
import android.app.Dialog;
import android.app.Instrumentation;
import android.test.ActivityInstrumentationTestCase2;
import android.view.View;

import org.easymock.EasyMockSupport;
import org.easymock.IMockBuilder;
import org.easymock.IMocksControl;

/**
 * Base class of Test classes based on {@link ActivityInstrumentationTestCase2} with EasyMock.
 *
 * Android's test frame work is based on JUnit 3, not JUnit 4, so we cannot use annotation
 * base test classes. We can reduce boiler plate codes around EasyMock by this class
 * which has EasyMockSupport and exposes its methods as composition pattern.
 *
 */
public class ActivityInstrumentationTestCase2WithMock<T extends Activity>
    extends ActivityInstrumentationTestCase2<T> {
  private EasyMockSupport mockSupport;
  private InstrumentationMockUtil mockUtil;

  @SuppressWarnings("deprecation")
  public ActivityInstrumentationTestCase2WithMock(Class<T> activityClass) {
    // Note: We call deprecated constructor intentionally here,
    // because our min target is API level 7.
    // The first parameter of the constructor of the super class is ignored on 2.2 or later,
    // but used on 2.1. Probably, the right way to get the package name is by using PackageManager.
    // However, here, it is impossible to do it. Thus, we hard-code the package name here.
    // Eventually we should switch to the new framework and the following code should be cleaned
    // at that time.
    super("org.mozc.android.inputmethod.japanese", activityClass);
  }

  @Override
  protected void setUp() throws Exception {
    super.setUp();
    Instrumentation instrumentation = getInstrumentation();
    mockSupport = new MozcMockSupport(instrumentation);
    mockUtil = new InstrumentationMockUtil(instrumentation, mockSupport);
  }

  @Override
  protected void tearDown() throws Exception {
    mockSupport = null;
    mockUtil = null;
    super.tearDown();
  }

  public IMocksControl createControl() {
    return mockSupport.createControl();
  }

  public <T> T createMock(Class<T> toMock) {
    return mockSupport.createMock(toMock);
  }

  public <T> T createMock(String name, Class<T> toMock) {
    return mockSupport.createMock(name, toMock);
  }

  public <T> IMockBuilder<T> createMockBuilder(Class<T> toMock) {
    return mockSupport.createMockBuilder(toMock);
  }

  public IMocksControl createNiceControl() {
    return mockSupport.createNiceControl();
  }

  public <T> T createNiceMock(Class<T> toMock) {
    return mockSupport.createNiceMock(toMock);
  }

  public <T> T createNiceMock(String name, Class<T> toMock) {
    return mockSupport.createNiceMock(name, toMock);
  }

  public IMocksControl createStrictControl() {
    return mockSupport.createStrictControl();
  }

  public <T> T createStrictMock(Class<T> toMock) {
    return mockSupport.createStrictMock(toMock);
  }

  public <T> T createStrictMock(String name, Class<T> toMock) {
    return mockSupport.createStrictMock(toMock);
  }

  public void replayAll() {
    mockSupport.replayAll();
  }

  public void resetAll() {
    mockSupport.resetAll();
  }

  public void resetAllToDefault() {
    mockSupport.resetAllToDefault();
  }

  public void resetAllToNice() {
    mockSupport.resetAllToNice();
  }

  public void resetAllToStrict() {
    mockSupport.resetAllToStrict();
  }

  public void verifyAll() {
    mockSupport.verifyAll();
  }

  public <T extends View> T createViewMock(Class<T> toMock) {
    return mockUtil.createViewMock(toMock);
  }

  public <T extends View> IMockBuilder<T> createViewMockBuilder(Class<T> toMock) {
    return mockUtil.createViewMockBuilder(toMock);
  }

  public <T extends Dialog> T createDialogMock(Class<T> toMock) {
    return mockUtil.createDialogMock(toMock);
  }

  public <T extends Dialog> IMockBuilder<T> createDialogMockBuilder(Class<T> toMock) {
    return mockUtil.createDialogMockBuilder(toMock);
  }
}
