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

import org.easymock.internal.IProxyFactory;
import org.easymock.internal.MocksControl;

import java.io.File;
import java.lang.reflect.Method;

/**
 */
class AndroidMocksControl extends MocksControl {
  private final File dexCache;

  public AndroidMocksControl(MockType type, File dexCache) {
    super(type);
    this.dexCache = dexCache;
  }

  @Override
  protected <T> IProxyFactory<T> createProxyFactory(Class<T> toMock) {
    if (!toMock.isInterface()) {
      if (dexCache == null) {
        throw new IllegalStateException(
            "To use class mocking on Android, you need to set dexCache");
      }
      // Inject dex-version of the proxy factory.
      return new AndroidProxyFactory<T>(dexCache);
    }
    return super.createProxyFactory(toMock);
  }

  @Override
  public <T> T createMock(final String name, final Class<T> toMock, final Method... mockedMethods) {
    if (!toMock.isInterface()) {
      // Intercept creation of concrete classes' mocks, to set mockedMethods to the instance
      // for partial mocks in dexMaker customized way:
      // - Create a normal mock by invoking super class's method (which eventually invokes
      //   AndroidProxyFactory#create via createProxyFactory overwritten above.)
      // - Set target mock methods via AndroidProxyFactory#getHandler.
      T mock = createMock(name, toMock);
      if (mockedMethods.length > 0) {
        AndroidProxyFactory.getHandler(mock).setMockedMethods(mockedMethods);
      }
      return mock;
    }
    return super.createMock(name, toMock, mockedMethods);
  }

  @Override
  public <T> T createMock(final Class<T> toMock, final Method... mockedMethods) {
    if (!toMock.isInterface()) {
      // Intercept creation of concreate classes' partial mocks. See above method for details.
      T mock = createMock(toMock);
      if (mockedMethods.length > 0) {
        AndroidProxyFactory.getHandler(mock).setMockedMethods(mockedMethods);
      }
      return mock;
    }
    return super.createMock(toMock, mockedMethods);
  }
}
