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

import org.mozc.android.inputmethod.japanese.MozcUtil;

import android.test.InstrumentationTestCase;
import android.test.MoreAsserts;
import android.test.suitebuilder.annotation.SmallTest;

import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

/**
 */
public class ZipFileUtilTest extends InstrumentationTestCase {

  private static byte[] readAll(String apkPath, String fileName) throws IOException {
    ZipFile apkFile = new ZipFile(apkPath);
    ZipEntry zipEntry = apkFile.getEntry(fileName);
    InputStream zipInputStream = apkFile.getInputStream(zipEntry);
    boolean succeeded = false;
    try {
      byte[] expected = new byte[(int) zipEntry.getSize()];
      for (int readSize = 0, offset = 0;
           (readSize = zipInputStream.read(expected, offset, expected.length - offset)) > 0;
           offset += readSize) { /* do nothing */ }
      succeeded = true;
      return expected;
    } finally {
      MozcUtil.close(zipInputStream, !succeeded);
    }
  }

  @SmallTest
  public void testGetBuffer() throws IOException {
    String[] fileNameList = {
        // Files with ".imy" extension are not compressed at build time.
        "AndroidManifest.xml", "assets/uncompressed.txt.imy",
    };
    String apkPath = getInstrumentation().getContext().getApplicationInfo().sourceDir;
    for (String fileName : fileNameList) {
      ByteBuffer buffer = ZipFileUtil.getBuffer(new ZipFile(apkPath), fileName);
      buffer.position(0);
      byte[] actual = new byte[buffer.remaining()];
      buffer.get(actual);
      byte[] expected = readAll(apkPath, fileName);
      MoreAsserts.assertEquals(expected, actual);
    }
  }
}
