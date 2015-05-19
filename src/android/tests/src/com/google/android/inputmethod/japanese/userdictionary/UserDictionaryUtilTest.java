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

package org.mozc.android.inputmethod.japanese.userdictionary;

import org.mozc.android.inputmethod.japanese.testing.Parameter;

import android.net.Uri;

import junit.framework.TestCase;

import java.io.File;
import java.util.Arrays;

/**
 */
public class UserDictionaryUtilTest extends TestCase {
  public void testGenerateDictionaryNameByUri() {
    class TestData extends Parameter {
      final String expectedName;
      final String filePath;
      final String[] dictionaryNameList;

      TestData(String expectedName, String filePath, String[] dictionaryNameList) {
        this.expectedName = expectedName;
        this.filePath = filePath;
        this.dictionaryNameList = dictionaryNameList;
      }
    }

    TestData[] testDataList = {
        // Simplest case.
        new TestData("abc", "abc", new String[] {}),

        // With directory name.
        new TestData("abc", "directory/abc", new String[] {}),

        // With extension.
        new TestData("abc", "abc.txt", new String[] {}),

        // With both directory and extension.
        new TestData("abc", "directory/abc.txt", new String[] {}),

        // File name containing two periods.
        new TestData("abc.def", "directory/abc.def.txt", new String[] {}),

        // File name beginning with '.'.
        new TestData(".abc", "directory/.abc", new String[] {}),

        // The case where the (first) generated name is already in the dictionary name list.
        new TestData("abc (1)", "dictionary/abc.txt", new String[] { "abc" }),

        // The case where the first and second generated name are already
        // in the dictionary name list.
        new TestData("abc (2)", "dictionary/abc.txt", new String[] { "abc", "abc (1)" }),
    };

    for (TestData testData : testDataList) {
      assertEquals(testData.toString(),
                   testData.expectedName,
                   UserDictionaryUtil.generateDictionaryNameByUri(
                       Uri.fromFile(new File(testData.filePath)),
                       Arrays.asList(testData.dictionaryNameList)));
    }
  }
}
