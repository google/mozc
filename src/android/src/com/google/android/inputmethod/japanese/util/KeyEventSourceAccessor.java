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

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.os.Build;
import android.view.InputDevice;
import android.view.KeyEvent;

/**
 * Accessor for {@code KeyEvent#getSource()} for all OS version.
 *
 * <p>If API Level < 9, SOURCE_KEYBOARD is returned as fallback.
 */
public class KeyEventSourceAccessor {

  private interface SourceAccessor {
    void setSource(KeyEvent keyEvent, int source);
    int getSource(KeyEvent keyEvent);
  }

  @SuppressLint("InlinedApi")
  static class FallbackSourceAccessor implements SourceAccessor {
    @Override
    public void setSource(KeyEvent keyEvent, int source) {
      // Do nothing
    }

    @Override
    public int getSource(KeyEvent keyEvent) {
      return InputDevice.SOURCE_KEYBOARD;
    }
  }

  @TargetApi(9)
  static class ConcreteSourceAccessor implements SourceAccessor {
    @Override
    public void setSource(KeyEvent keyEvent, int source) {
      keyEvent.setSource(source);
    }

    @Override
    public int getSource(KeyEvent keyEvent) {
      return keyEvent.getSource();
    }
  }

  private static final SourceAccessor sourceAccessor;

  static {
    SourceAccessor accessor;
    Class<? extends SourceAccessor> clazz = Build.VERSION.SDK_INT >= 9
        ? ConcreteSourceAccessor.class
        : FallbackSourceAccessor.class;
    try {
      accessor = clazz.newInstance();
    } catch (InstantiationException e) {
      accessor = new FallbackSourceAccessor();
    } catch (IllegalAccessException e) {
      accessor = new FallbackSourceAccessor();
    }
    sourceAccessor = accessor;
  }

  public static void setSource(KeyEvent keyEvent, int source) {
    sourceAccessor.setSource(keyEvent, source);
  }

  public static int getSource(KeyEvent keyEvent) {
    return sourceAccessor.getSource(keyEvent);
  }
}
