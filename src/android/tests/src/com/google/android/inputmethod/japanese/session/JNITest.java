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

package org.mozc.android.inputmethod.japanese.session;

import org.mozc.android.inputmethod.japanese.MozcUtil;
import org.mozc.android.inputmethod.japanese.stresstest.StressTest;
import org.mozc.android.inputmethod.japanese.testing.HttpTestServer;
import org.mozc.android.inputmethod.japanese.testing.MemoryLogger;
import com.google.common.base.Strings;

import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.LargeTest;
import android.test.suitebuilder.annotation.SmallTest;

import java.io.IOException;
import java.io.UnsupportedEncodingException;

/**
 */
public class JNITest extends InstrumentationTestCase {
  @Override
  protected void setUp() throws Exception {
    super.setUp();
    JNITestingBackdoor.initialize(getInstrumentation().getContext());
  }

  private static byte[] toBytes(String string) throws UnsupportedEncodingException {
    if (string != null) {
      return string.getBytes("UTF-8");
    } else {
      return null;
    }
  }

  private static String toString(byte[] bytes) throws UnsupportedEncodingException {
    if (bytes != null) {
      return new String(bytes, "UTF-8");
    } else {
      return null;
    }
  }

  @SmallTest
  public void testHttpClientIntegration() throws IOException {
    HttpTestServer testServer = HttpTestServer.startServer();
    int port = testServer.getPort();
    try {
      String urlPrefix = "http://localhost:" + port;

      // Test data preparation must be after the initialization of the server
      // because port number is needed to generate URL.
      class Parameter extends org.mozc.android.inputmethod.japanese.testing.Parameter {
        final String method;
        final String url;
        final String content;
        final String expectedResult;
        public Parameter(String method, String url, String content, String expectedResult) {
          this.method = method;
          this.url = url;
          this.content = content;
          this.expectedResult = expectedResult;
        }
      }

      Parameter[] testDataList = {
          new Parameter("GET", urlPrefix, null, "GET\n/"),
          new Parameter("GET", urlPrefix + "/endpoint?key=value", null, "GET\n/endpoint?key=value"),
          new Parameter("GET", "invalid://url", null, null),
          new Parameter("HEAD", urlPrefix, null, HttpTestServer.COMMON_HEADER + "\n\n"),
          new Parameter("HEAD", urlPrefix + "/endpoint?key=value", null,
                        HttpTestServer.COMMON_HEADER + "\n\n"),
          new Parameter("HEAD", "invalid://url", null, null),
          new Parameter("POST", urlPrefix, "content", "content"),
          new Parameter("POST", "invalid://url", null, null),
      };
      for (Parameter testData : testDataList) {
        byte[] resultBytes =
            JNITestingBackdoor.httpRequest(toBytes(testData.method),
                                           toBytes(testData.url),
                                           toBytes(testData.content));
        assertEquals(testData.toString(), testData.expectedResult, toString(resultBytes));
      }
    } finally {
      if (testServer != null) {
        MozcUtil.close(testServer, true);
      }
    }
  }

  @StressTest
  @LargeTest
  public void testRepeatedHttpRequest() throws IOException {
    MemoryLogger memoryLogger = new MemoryLogger("testRepeatedHttpRequest", 100);
    HttpTestServer testServer = HttpTestServer.startServer();
    // To detect object leak, we have to repeat 1024 or more times.
    int repeatCount = 1200;
    int port = testServer.getPort();
    String urlPrefix = "http://localhost:" + port;
    String content = Strings.repeat("a", 100 * 1000);  // 100K characters
    memoryLogger.logMemory("start");
    try {
      for (int i = 0; i < repeatCount; ++i) {
        JNITestingBackdoor.httpRequest(toBytes("POST"),
                                       toBytes(urlPrefix),
                                       toBytes(content));
        memoryLogger.logMemoryInterval();
      }
      memoryLogger.logMemory("end");
    } finally {
      if (testServer != null) {
        MozcUtil.close(testServer, true);
      }
    }
  }
}
