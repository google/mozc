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

import org.mozc.android.inputmethod.japanese.MozcLog;

import android.util.Xml;

import junit.framework.AssertionFailedError;
import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestListener;

import org.xmlpull.v1.XmlSerializer;

import java.io.Closeable;
import java.io.IOException;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.io.Writer;

/**
 * A TestListerner to write gtest compatible test report XML file.
 *
 * /data/data/{testee application's ID}/gtest-report.xml will be created.
 *
 * Sample output:
 * <pre>
 * <?xml version='1.0' encoding='UTF-8' standalone='yes' ?>
 * <testsuites name="gtest-report">
 *   <testsuite name="org.mozc.android.inputmethod.japanese.nativecallback.HttpClientTest">
 *     <testcase name="testCreateRequest" time="0.019" />
 *     <testcase name="testRequest" time="2.178">
 *       <error type="java.lang.NullPointerException">
 *         java.lang.NullPointerException
 *         at org.mozc.android.inputmethod.japanese.nativecallback.HttpClientTest.testRequest(HttpClientTest.java:91)
 *         at java.lang.reflect.Method.invokeNative(Native Method)
 *         at java.lang.reflect.Method.invoke(Method.java:521)
 *       </error>
 *     </testcase>
 *   </testsuite>
 * </testsuites>
 * </pre>
 *
 * Note that the result lack some attributes (e.g. 'errors' of testsuite).
 * This restriction comes from the spec of serializer.
 *
 */
public class MozcTestListener implements TestListener, Closeable {
  private XmlSerializer serializer;
  private TestCase currentTestCase;
  private long testCaseStartTime;
  private boolean isTimeAppended;
  private static final String NS_DEFAULT = "";
  private static final String TAG_TESTSUITES = "testsuites";
  private static final String TAG_TESTSUITE = "testsuite";
  private static final String TAG_TESTCASE = "testcase";
  private static final String TAG_ERROR = "error";
  private static final String TAG_FAILURE = "failure";
  private static final String ATTR_MESSAGE = "message";
  private static final String ATTR_NAME = "name";
  private static final String ATTR_TIME = "time";
  private static final String ATTR_TYPE = "type";

  /**
   * Note : {@link #close()} must be called from the caller side.
   *
   * The method will close the XML document by close tags.
   */
  public MozcTestListener(Writer writer, String suitesName) {
    serializer = Xml.newSerializer();
    try {
      serializer.setOutput(writer);
      serializer.startDocument("UTF-8", true);
      serializer.startTag(NS_DEFAULT, TAG_TESTSUITES);
      serializer.attribute(NS_DEFAULT, ATTR_NAME, suitesName);
    } catch (IOException e) {
      MozcLog.e(e.getMessage(), e);
    }
  }

  @Override
  public void addError(Test test, Throwable t) {
    addProblem(NS_DEFAULT, TAG_ERROR, t);
  }

  @Override
  public void addFailure(Test test, AssertionFailedError t) {
    addProblem(NS_DEFAULT, TAG_FAILURE, t);
  }

  // Reaches here because the test fails.
  // Note that endTest method will be also invoke soon.
  private void addProblem(String namespace, String tag, Throwable throwable) {
    try {
      maybeAddTimeAttribute();
      serializer.startTag(namespace, tag);
      if (throwable.getMessage() != null) {
        serializer.attribute(namespace, ATTR_MESSAGE, throwable.getMessage());
      }
      serializer.attribute(namespace, ATTR_TYPE, throwable.getClass().getName());
      StringWriter stringWriter = new StringWriter();
      throwable.printStackTrace(new PrintWriter(stringWriter));
      serializer.text(stringWriter.toString());
      serializer.endTag(namespace, tag);
    } catch (IOException e) {
      MozcLog.e(e.getMessage(), e);
    }
  }

  @Override
  public void endTest(Test test) {
    try {
      if (test != currentTestCase) {
        MozcLog.e("Unexpected test " + test + "; Expected one is " + currentTestCase);
      } else if (test instanceof TestCase){
        maybeAddTimeAttribute();
        serializer.endTag(NS_DEFAULT, TAG_TESTCASE);
      } else {
        MozcLog.e("Unextected type; " + test.getClass().getCanonicalName());
      }
    } catch (IOException e) {
      MozcLog.e(e.getMessage(), e);
    }
  }

  private void maybeAddTimeAttribute() throws IOException {
    // If current element is <testcase>, add time attribute.
    if (TAG_TESTCASE.equals(serializer.getName()) && !isTimeAppended) {
      serializer.attribute(NS_DEFAULT, ATTR_TIME,
          Double.toString((System.currentTimeMillis() - testCaseStartTime) / 1000.0));
      isTimeAppended = true;
    }
  }

  @Override
  public void startTest(Test test) {
    try {
      if (test instanceof TestCase){
        TestCase testCase = TestCase.class.cast(test);
        if (currentTestCase == null || testCase.getClass() != currentTestCase.getClass()) {
          // If this is the first test or different test case class is passed,
          // close last <testsuite> and start new <testsuite>.
          if (currentTestCase != null) {
            // <testsuite> has been started already so close it.
            serializer.endTag(NS_DEFAULT, TAG_TESTSUITE);
          }
          serializer.startTag(NS_DEFAULT, TAG_TESTSUITE);
          serializer.attribute(NS_DEFAULT, ATTR_NAME, testCase.getClass().getName());
        }
        serializer.startTag(NS_DEFAULT, TAG_TESTCASE);
        serializer.attribute(NS_DEFAULT, ATTR_NAME, testCase.getName());
        currentTestCase = testCase;
        testCaseStartTime = System.currentTimeMillis();
        isTimeAppended = false;
      } else {
        MozcLog.e("Unextected type; " + test.getClass().getCanonicalName());
      }
    } catch (IOException e) {
      MozcLog.e(e.getMessage(), e);
    }
  }

  @Override
  public void close() throws IOException {
    // Close all opened tags.
    while (serializer.getDepth() > 0) {
      serializer.endTag(serializer.getNamespace(), serializer.getName());
    }
    serializer.endDocument();
  }
}
