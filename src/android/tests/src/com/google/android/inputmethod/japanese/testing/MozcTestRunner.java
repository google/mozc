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
import org.mozc.android.inputmethod.japanese.MozcUtil;
import org.mozc.android.inputmethod.japanese.stresstest.StressTest;

import android.os.Build;
import android.os.Bundle;
import android.test.AndroidTestRunner;
import android.test.InstrumentationTestRunner;
import android.test.suitebuilder.TestSuiteBuilder;

import dalvik.system.PathClassLoader;

import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.lang.reflect.AnnotatedElement;
import java.lang.reflect.Method;

/**
 * This is basic Mozc TestRunner which runs test methods that don't have StressTest
 * annotation.
 *
 */
public class MozcTestRunner extends InstrumentationTestRunner {

  /**
   * This is test runner class which filter test cases.
   */
  public static class TestRunner extends AndroidTestRunner {
    private static Method toMethod(TestCase testCase) {
      try {
        return testCase.getClass().getMethod(testCase.getName());
      } catch (NoSuchMethodException e) {
        return null;
      }
    }

    private static boolean hasSupportedApiLevel(AnnotatedElement element) {
      ApiLevel apiLevelAnnotation = element.getAnnotation(ApiLevel.class);
      if (apiLevelAnnotation == null) {
        // There is no ApiLevel annotation, so all platform should support this.
        return true;
      }

      return Build.VERSION.SDK_INT >= apiLevelAnnotation.value();
    }

    protected boolean filterTestMethod(Method method) {
      return hasSupportedApiLevel(method);
    }

    public Test filterTest(Test test) {
      // If the test class's ApiLevel value is larger than the environment's API Level,
      // skip the test.
      // On Dalvik VM, the class verification has not done yet (it will be done lazily)
      // so ClassNotFound will not be thrown even if the class contains newer API
      // than the runtime environment.
      // This mechanism is highly dependent on VM's implementation (but doesn't violate the spec)
      // but to simplify the code we use this.
      if (!hasSupportedApiLevel(test.getClass())) {
        return null;
      }

      if (test instanceof TestSuite) {
        TestSuite testSuite = TestSuite.class.cast(test);
        TestSuite dstSuite = new TestSuite(testSuite.getName());
        for (int i = 0; i < testSuite.testCount(); i++) {
          Test testat = testSuite.testAt(i);
          Test filteredtest = filterTest(testat);
          if (filteredtest != null) {
            dstSuite.addTest(filteredtest);
          }
        }
        if (dstSuite.countTestCases() == 0) {
          return null;
        }

        return dstSuite;
      }

      if (test instanceof TestCase){
        Method method = toMethod(TestCase.class.cast(test));
        if (method == null || !filterTestMethod(method)) {
          return null;
        }

        return test;
      }

      return test;
    }

    @Override
    public void setTest(Test test) {
      super.setTest(filterTest(test));
    }
  }

  private static final String ROOT_TARGET_PACKAGE = "org.mozc.android.inputmethod.japanese";
  private static final String[] SUB_PACKAGE_NAME_LIST = {
    "emoji",
    "keyboard", "model", "mushroom", "nativecallback", "preference", "session",
    "stresstest", "testing", "ui", "userdictionary", "util", "view",
  };

  private static final String XML_FILE_NAME = "gtest-report.xml";

  private MozcTestListener xmlListener;

  @Override
  public TestSuite getAllTests() {
    // NOTE: due to the dalvik VM implementation, memory allocation during code verification
    // would be failed on Android 2.1 (fixed at Android 2.2).
    // The memory allocation is stick to ClassLoader. So, as a work around, we use different
    // ClassLoaders for each packages heuristically, as a HACK.
    // TODO(hidehiko): make package management automatically.
    TestSuite result = new TestSuite();

    ClassLoader parentLoader = getTargetContext().getClassLoader();
    String apkPath = getContext().getApplicationInfo().sourceDir;

    String name = getClass().getName();

    // Add tests in the root package.
    {
      TestSuiteBuilder builder =
          new TestSuiteBuilder(name, new PathClassLoader(apkPath, parentLoader));
      builder.includePackages(ROOT_TARGET_PACKAGE);
      for (String packageName : SUB_PACKAGE_NAME_LIST) {
        builder.excludePackages(ROOT_TARGET_PACKAGE + "." + packageName);
      }
      result.addTest(builder.build());
    }

    // Add tests in the sub packages.
    for (String packageName : SUB_PACKAGE_NAME_LIST) {
      TestSuiteBuilder builder =
          new TestSuiteBuilder(name, new PathClassLoader(apkPath, parentLoader));
      builder.includePackages(ROOT_TARGET_PACKAGE + "." + packageName);
      result.addTest(builder.build());
    }

    return result;
  }

  @Override
  protected AndroidTestRunner getAndroidTestRunner() {
    AndroidTestRunner runner = new TestRunner(){
      @Override
      public boolean filterTestMethod(Method method){
        return super.filterTestMethod(method) && method.getAnnotation(StressTest.class) == null;
      }
    };
    runner.addTestListener(xmlListener);
    return runner;
  }

  @Override
  public void onCreate(Bundle arguments) {
    try {
      // Note that unit test runs on "target context"'s process so we cannot write anything
      // in /data/data/org.mozc.android.inputmethod.japanese.tests directory.
      File reportingXmlFile = new File(getTargetContext().getApplicationInfo().dataDir,
                                       XML_FILE_NAME);
      MozcLog.i("reporting XML is at " + reportingXmlFile.getAbsolutePath());
      BufferedWriter writer = new BufferedWriter(new OutputStreamWriter(
          new FileOutputStream(reportingXmlFile), "UTF-8"));
      xmlListener = new MozcTestListener(writer, "gtest-report");
    } catch (IOException e) {
      MozcLog.e(e.getMessage(), e);
    }
    // Invoke super.onCreate here (not the head of this method)
    // because super.onCreate starts unit testing internally so
    // after returning from it what we do will affect nothing.
    super.onCreate(arguments);
  }

  @Override
  public void finish(int resultCode, Bundle results) {
    if (xmlListener != null) {
      try {
        // Close the listener to make the lister close XML document correctly.
        MozcUtil.close(xmlListener, true);
      } catch (IOException e) {
        MozcLog.e(e.getMessage(), e);
      }
    }
    super.finish(resultCode, results);
  }
}
