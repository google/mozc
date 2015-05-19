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

package org.mozc.android.inputmethod.japanese.mushroom;

import static org.easymock.EasyMock.capture;
import static org.easymock.EasyMock.eq;
import static org.easymock.EasyMock.expect;

import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;

import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;

import org.easymock.Capture;

import java.util.Collections;
import java.util.List;

/**
 */
public class MushroomUtilTest extends InstrumentationTestCaseWithMock {
  public void testGetMushroomApplicationList() {
    PackageManager manager = createMock(PackageManager.class);
    Capture<Intent> intentCapture = new Capture<Intent>();

    List<ResolveInfo> result = Collections.emptyList();
    expect(manager.queryIntentActivities(capture(intentCapture), eq(0)))
        .andReturn(result);

    replayAll();

    assertSame(result, MushroomUtil.getMushroomApplicationList(manager));

    verifyAll();
    Intent intent = intentCapture.getValue();
    assertNotNull(intent);
    assertEquals(MushroomUtil.ACTION, intent.getAction());
    assertEquals(Collections.singleton(MushroomUtil.CATEGORY),
                 intent.getCategories());
  }

  public void testCreateMushroomSelectionActivityLaunchingIntent() {
    Intent intent = MushroomUtil.createMushroomSelectionActivityLaunchingIntent(
        getInstrumentation().getTargetContext(), 10, "abc");

    assertNull(intent.getAction());
    assertEquals(Intent.FLAG_ACTIVITY_NEW_TASK, intent.getFlags());
    assertEquals(10, MushroomUtil.getFieldId(intent));
    assertEquals("abc", MushroomUtil.getReplaceKey(intent));
  }

  public void testCreateMushroomLaunchingIntent() {
    Intent intent = MushroomUtil.createMushroomLaunchingIntent(
        "mock.package.name", "mock.package.name.MockClassName", "abc");
    assertEquals(MushroomUtil.ACTION, intent.getAction());
    assertEquals(Collections.singleton(MushroomUtil.CATEGORY), intent.getCategories());
    assertEquals("abc", MushroomUtil.getReplaceKey(intent));
    assertFalse(intent.hasExtra(MushroomUtil.FIELD_ID));
    assertEquals("mock.package.name", intent.getComponent().getPackageName());
    assertEquals("mock.package.name.MockClassName", intent.getComponent().getClassName());
  }

  public void testSendReplaceKey() {
    Intent originalIntent = new Intent();
    originalIntent.putExtra(MushroomUtil.FIELD_ID, 10);
    Intent resultIntent = new Intent();
    resultIntent.putExtra(MushroomUtil.KEY, "abc");

    MushroomResultProxy resultProxy = MushroomResultProxy.getInstance();
    try {
      resultProxy.clear();
      MushroomUtil.sendReplaceKey(originalIntent, resultIntent);
      assertEquals("abc", resultProxy.getReplaceKey(10));
    } finally {
      resultProxy.clear();
    }
  }
}
