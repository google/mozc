// Copyright 2010-2018, Google Inc.
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

package org.mozc.android.inputmethod.japanese.ui;

import static org.easymock.EasyMock.eq;
import static org.easymock.EasyMock.expect;
import static org.easymock.EasyMock.notNull;
import static org.easymock.EasyMock.same;

import org.mozc.android.inputmethod.japanese.resources.R;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;
import org.mozc.android.inputmethod.japanese.ui.MenuDialog.MenuDialogListener;
import org.mozc.android.inputmethod.japanese.ui.MenuDialog.MenuDialogListenerHandler;
import com.google.common.base.Optional;

import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.test.mock.MockContext;
import android.test.mock.MockPackageManager;
import android.test.suitebuilder.annotation.SmallTest;

import java.util.Collections;
import java.util.List;

/**
 * Test for MenuDialog.
 *
 */
public class MenuDialogTest extends InstrumentationTestCaseWithMock{
  @Override
  protected void setUp() throws Exception {
    super.setUp();
  }

  @Override
  protected void tearDown() throws Exception {
    super.tearDown();
  }

  @SmallTest
  public void testMenuDialogListenerHandler() {
    Context context = createMock(MockContext.class);
    MenuDialogListener listener = createMock(MenuDialogListener.class);
    DialogInterface dialog = createMock(DialogInterface.class);
    int[] indexToIdTable = new int[] {
        R.string.menu_item_input_method,
        R.string.menu_item_preferences,
        R.string.menu_item_mushroom,
    };
    MenuDialogListenerHandler handler =
        new MenuDialogListenerHandler(context, indexToIdTable, Optional.of(listener));

    // Test for onShow.
    resetAll();
    listener.onShow(same(context));
    replayAll();

    handler.onShow(dialog);

    verifyAll();

    // Test for onDismiss.
    resetAll();
    listener.onDismiss(same(context));
    replayAll();

    handler.onDismiss(dialog);

    verifyAll();

    // Test for onShowInputMethodPickerSelected.
    resetAll();
    listener.onShowInputMethodPickerSelected(same(context));
    replayAll();

    handler.onClick(dialog, 0);

    verifyAll();

    // Test for onLaunchPreferenceActivitySelected.
    resetAll();
    listener.onLaunchPreferenceActivitySelected(same(context));
    replayAll();

    handler.onClick(dialog, 1);

    verifyAll();

    // Test for onShowMushroomSelectionDialogSelected.
    resetAll();
    listener.onShowMushroomSelectionDialogSelected(same(context));
    replayAll();

    handler.onClick(dialog, 2);

    verifyAll();
  }

  @SmallTest
  public void testEnableAndDisableMenu() {
    Context context = createMock(MockContext.class);

    // If no mushroom-aware application is installed, Mushroom should be disabled.
    resetAll();
    PackageManager packageManager = createMock(MockPackageManager.class);
    expect(packageManager.queryIntentActivities(notNull(Intent.class), eq(0)))
        .andStubReturn(Collections.<ResolveInfo>emptyList());
    expect(context.getPackageManager()).andStubReturn(packageManager);
    replayAll();
    List<Integer> result = MenuDialog.getEnabledMenuIds(context);
    assertEquals(2, result.size());
    assertEquals(R.string.menu_item_input_method, result.get(0).intValue());
    assertEquals(R.string.menu_item_preferences, result.get(1).intValue());
    verifyAll();

    // If a mushroom-aware application is installed, Mushroom should be enabled.
    resetAll();
    expect(packageManager.queryIntentActivities(notNull(Intent.class), eq(0)))
        .andStubReturn(Collections.singletonList(new ResolveInfo()));
    expect(context.getPackageManager()).andStubReturn(packageManager);
    replayAll();
    result = MenuDialog.getEnabledMenuIds(context);
    assertEquals(3, result.size());
    assertEquals(R.string.menu_item_input_method, result.get(0).intValue());
    assertEquals(R.string.menu_item_preferences, result.get(1).intValue());
    assertEquals(R.string.menu_item_mushroom, result.get(2).intValue());
    verifyAll();
  }
}
