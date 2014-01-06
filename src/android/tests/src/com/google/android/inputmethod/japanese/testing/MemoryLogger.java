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

import org.mozc.android.inputmethod.japanese.MozcLog;
import com.google.common.base.Preconditions;

import android.os.Debug;
import android.os.Debug.MemoryInfo;

/**
 * Logs memory usage.
 *
 * <p>Typical usage;
 * {@code
 * <pre>
 * MemoryLogger memoryLogger = new MemoryLogger("testMyStress", 100);
 * memoryLogger.logMemory("start");
 * for (int i = 0; i < 100000; ++i) {
 *   // Do something interesting
 *   memoryLogger.logMemoryInterval();
 * }
 * memoryLogger.logMemory("end");
 * </pre>
 * }
 */
public class MemoryLogger {

  private static void logMemoryInternal(String tag) {
    System.gc();
    Thread.yield();
    Runtime runtime = Runtime.getRuntime();
    MemoryInfo memoryInfo = new MemoryInfo();
    Debug.getMemoryInfo(memoryInfo);
    MozcLog.i(
        String.format(
            "%s: global:%d gcount:%d nativeHeap:%d javaHeap:%d private:%d pss:%d shared:%d",
            tag,
            Debug.getGlobalAllocSize(),
            Debug.getGlobalAllocCount(),
            Debug.getNativeHeapAllocatedSize(),
            runtime.totalMemory() - runtime.freeMemory(),
            memoryInfo.getTotalPrivateDirty(),
            memoryInfo.getTotalPss(),
            memoryInfo.getTotalSharedDirty()));
  }

  private final String componentName;
  private final int interval;
  private int count = 0;

  /**
   * @param componentName a tag printed at the head of the log entries. e.g. "testMyStress"
   * @param interval @see {@link #logMemoryInterval()}
   */
  public MemoryLogger(String componentName, int interval) {
    Preconditions.checkArgument(interval > 0);
    this.componentName = Preconditions.checkNotNull(componentName);
    this.interval = interval;
  }

  /**
   * Logs memory usage in following format.
   *
   * <p>{@code <p>{componentName}({tag}): xxxxxxxx}
   * @param tag a tag for readability. e.g. "start"
   */
  public void logMemory(String tag) {
    logMemoryInternal(String.format("%s(%s)", componentName, Preconditions.checkNotNull(tag)));
  }

  /**
   * Logs memory usage in following format.
   *
   * <p>{@code <p>{componentName}({count}): xxxxxxxx}
   *
   * <p>{@code count} is internal counter which is incremented when this method is invoked.
   * If {@code count} is *not* multiple number of {@code interval} given in the constructor,
   * logging is omitted for performance and avoiding verbosity.
   */
  public void logMemoryInterval() {
    ++count;
    if (count % interval == 0) {
      logMemoryInternal(String.format("%s(%d)", componentName, count));
    }
  }
}
